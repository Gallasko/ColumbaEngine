# PgScript `os`, `json` and `import ... as` Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** `import "module" as name` binds a native module to a table; a `json` module everywhere and an `os` module (`call`, env, time, folders, exit) in the command-line tool.

**Architecture:** The AST compiler, on an `import "m" as n` of a native module, asks the VM to build a table of the module's functions and bind it to the global `n`, and records the import in the chunk as the string `"m as n"`, so every place that replays a chunk's imports (bytecode file, cached bytecode, module cache, a `.pgc` import in the Pratt parser) binds it again through one VM call. `json` wraps nlohmann's `ordered_json` (`/src/Engine/Helpers/json.hpp`); `os.call` uses `posix_spawnp` with three pipes and a `poll` loop.

**Tech Stack:** C++17, the PgScript VM (`/src/Engine/Compiler`), nlohmann json 3.x (in tree), POSIX (`posix_spawn`, `poll`), GoogleTest (`test_compiler`).

**Spec:** `/docs/superpowers/specs/2026-11-16-pgscript-os-json-design.md`

## Global Constraints

- Branch `Chronicle`, in `/home/corse/Documents/PgEngine`; parallel sessions share this tree: commit only this plan's files, never `git stash`, never rewrite a file another change touches.
- Builds in `/release` (`make -j4 <target>`, never a bare `-j`), only if the author allows building in this session; otherwise hand them the command.
- A plain `import "m"` is unchanged: flat names, as today. `import "m" as n` binds a table `n` and defines no flat names.
- `as` takes a native module only; with a file import it is a compile error saying "`as` takes a native module only for now".
- `os` is registered only by the command-line tool (`/examples/PgCompiler/application.cpp`), never by `EntitySystem::setupVm`. `json` is registered by `EntitySystem::setupVm`.
- `os.call` returns `{code, out, err}`; a failing command is not a script error; a program that cannot start → code 127 and the reason in `err`; killed by a signal → `128 + signal`.
- `json.parse`: a number without `.` or exponent → int when within ±2^46 (70368744177664), else double; others → double. `json.stringify`: a whole double keeps `.0`; invalid UTF-8 → U+FFFD.
- Tests put a call's result in a `var` before reading a field (`var r = os.call(...)`, then `r.out`): reading a field straight off a table a call has just built can read freed memory.

## Review Focus

1. A command that never reads its input (`true` given 5 MB): the host must not die of SIGPIPE; `os.call` returns code 0 (Task 3).
2. Many calls in a row (300 `os.call`s): no file descriptor leaks, the 300th works like the first (Task 3).
3. JSON nested 100 000 levels deep: a script error, not a crash of the host (Task 2).
4. A VM that resets its globals between scripts (as Chronicle runs rules) gets the `as` table again on the next script's import (Task 1).
5. `os.call` output that is not UTF-8 passed to `json.stringify`: U+FFFD in place, no error (Task 2 for stringify, Task 3 for the bytes reaching a string unchanged).

---

### Task 1: `import "module" as name`

**Files:**
- Modify: `/src/Engine/Compiler/vm.h` (beside `loadNativeModule`, ~line 1091)
- Modify: `/src/Engine/Compiler/ast/ast_compiler.cpp` (`visitStatement(ImportStatement*)`, ~line 1272)
- Modify: `/src/Engine/Compiler/vm_core.cpp:536`, `:586`; `/src/Engine/Compiler/cparser.cpp:1405-1450` (the import statement) and `:1553` (replay); `/src/Engine/Compiler/module_cache.cpp:142`
- Create: `/test/pgcompiler/unit/test_import_as.cc`; Modify: `/CMakeLists.txt` (test_compiler sources, after `test/pgcompiler/unit/test_shape_budget.cc`)

**Interfaces:**
- Produces, in `VM`:
  - `bool hasNativeModule(const std::string& module) const`
  - `bool loadNativeModuleAs(const std::string& module, const std::string& alias)` — a table (`createTable`) with one field per exported function (`createNativeFunction`) and per exported variable (`elementToValue`), runs the module's `init`, binds it with `defineGlobal(alias, table)` replacing any value the global had; false when the module is unknown.
  - `static std::string importRecord(const std::string& module, const std::string& alias)` — `module` when `alias` is empty, else `module + " as " + alias`.
  - `bool replayImport(const std::string& record)` — splits a record made by `importRecord` and calls `loadNativeModule` or `loadNativeModuleAs`.
- Every replay site (`vm_core.cpp` both, `cparser.cpp:1553`, `module_cache.cpp:142`) calls `replayImport(record)` instead of `loadNativeModule(name)`; the AST compiler pushes `importRecord(module, alias)` into `importedModules`.

Ruling recorded in the plan: the record is a string in the existing `importedModules` list, so the `.pgc` format does not change. An old `.pgc` (plain names) still loads; `BYTECODE_VERSION` stays 1; Chronicle's precompiled rules need no rebuild for this (the spec allowed a format change; this avoids it).

In the AST compiler: when `stmt->importName` is set, the alias is `stmt->importName->getName()`; it must match `^[A-Za-z_][A-Za-z0-9_]*$` (a string literal `"s"` arrives with its quotes and fails), else compile error "`as` needs a name: import \"m\" as name". If `vm->hasNativeModule(module)`: `loadNativeModuleAs`, record. Else, if the module is a file the compiler would import (`parseImportFile` would find it; check the same path it builds, without importing it): compile error "`as` takes a native module only for now". Else the existing not-found error. The `isNamed` branch stays as it is. In `cparser.cpp`'s `importStatement`: a `TOK_AS` after the module name is a compile error "`import ... as` needs the AST front-end".

- [ ] **Step 1: Write the failing tests** in `/test/pgcompiler/unit/test_import_as.cc` (fixture as `test_module_cache.cc`: an `EntitySystem` with `setVMOptimizationLevel(O3)`, `setVMFrontEnd(Ast)`, a `VM` given `ecs.setupVm(vm)`, a temp dir; results read from `vm.testOutput` or a global `out`)

```cpp
TEST_F(ImportAs, BindsATableOfTheModule)          // import "string" as s / print(s.strlen("abc"))  -> "3"
TEST_F(ImportAs, DefinesNoFlatNames)              // import "string" as s / print(strlen("a"))      -> RUNTIME_ERROR
TEST_F(ImportAs, BothFormsTogether)               // import "string" / import "string" as s / print(strlen("ab") + s.strlen("abc")) -> "5"
TEST_F(ImportAs, MissingModuleIsACompileError)    // import "nosuch" as n -> COMPILE_ERROR
TEST_F(ImportAs, FileImportIsACompileError)       // write lib.pg; import "lib" as l -> COMPILE_ERROR
TEST_F(ImportAs, NameMustBeAnIdentifier)          // import "string" as "s" -> COMPILE_ERROR
TEST_F(ImportAs, PrattFrontEndRefusesIt)          // setVMFrontEnd(Pratt): import "string" as s -> COMPILE_ERROR
TEST_F(ImportAs, ModuleCacheBindsItAgain)         // lib.pg: import "string" as s / fun f() { return s.strlen("ab") }
                                                  // main.pg: import "lib" / print(f()); run main twice (2nd from the cache) -> "2" both times
TEST_F(ImportAs, SerializedChunkBindsItAgain)     // compile, ChunkSerializer::serialize, deserialize into a second VM (as
                                                  // test_none_value.cc:300-320), replay its imports, run -> s.strlen works
TEST_F(ImportAs, OldPlainRecordsStillLoadFlat)    // a chunk whose importedModules is {"string"} -> strlen defined flat
TEST_F(ImportAs, ResetVmBindsItOnTheNextImport)   // same VM: run script A (import "string" as s), reset globals as a second
                                                  // interpretFromText does, run script B with the same import -> works
TEST(ImportRecord, RoundTrips)                    // importRecord("os","")=="os"; importRecord("os","o")=="os as o"
```

- [ ] **Step 2: Run** `cd /home/corse/Documents/PgEngine/release && make -j4 test_compiler && ./test_compiler --gtest_filter='ImportAs*:ImportRecord*'` — Expected: build error (no `importRecord`), then the tests fail once it compiles.
- [ ] **Step 3: Implement** the VM functions, the AST compiler branch, the replay sites, the Pratt error.
- [ ] **Step 4: Run** the same command — Expected: all pass. Then `./test_compiler` whole — Expected: no new failures (compare with a run before Task 1, saved to the workspace).
- [ ] **Step 5: Commit** `git add` the listed files; `git commit -m "PgScript: import \"module\" as name binds a native module to a table"`.

### Task 2: the `json` module

**Files:**
- Create: `/src/Engine/Helpers/jsonmodule.h`, `/src/Engine/Helpers/jsonmodule.cpp`
- Modify: `/CMakeLists.txt` (add `src/Engine/Helpers/jsonmodule.cpp` to both engine source lists, next to `src/Engine/Helpers/demangle.cpp` at ~line 259 and ~355; the test file to test_compiler)
- Modify: `/src/Engine/ECS/entitysystem.cpp:903` (`vm.addNativeModule("json", JsonModule{});` after `file`)
- Create: `/test/pgcompiler/unit/test_json_module.cc`

**Interfaces:**
- Produces: `class JsonModule : public NativeModule` exporting `parse(text)` and `stringify(value)` / `stringify(value, indent)`. Scripts: `import "json" as json` then `json.parse(...)`.
- `MAX_DEPTH = 512` (nesting deeper than that in `parse` → script error "json: nested deeper than 512"; the same limit stops `stringify` on deep or self-containing values).

Conversion rules are the spec's; key order: `nlohmann::ordered_json`. Errors are thrown as `std::runtime_error` with a message starting "json.parse:" / "json.stringify:" (the VM turns a throw from a native into a script error, as `FileModule` relies on). Parse errors give "line L, column C" (count them from nlohmann's byte position in the text). Stringify: `dump(indent or -1, ' ', false, nlohmann::json::error_handler_t::replace)`; a double whole and finite written with `.0` (nlohmann does). A table's integer keys come first as strings, then string keys in the table's order (use `ObjInstance`'s int keys and `fieldCount`/`fieldName`/`fieldValue`).

- [ ] **Step 1: Write the failing tests** (`/test/pgcompiler/unit/test_json_module.cc`, scripts run as in Task 1, results printed)

```cpp
TEST_F(JsonModule, ParsesEachType)          // {"a":1,"b":2.5,"c":"x","d":true,"e":null,"f":[1,2]} -> typeOf each: int double string bool none array
TEST_F(JsonModule, WholeDoubleStaysDouble)  // json.parse("2.0") typeOf "double"; json.parse("1e3") "double"; json.parse("7") "int"
TEST_F(JsonModule, BigIntegerIsDouble)      // json.parse("70368744177665") typeOf "double"; "70368744177664" -> "int"
TEST_F(JsonModule, KeyOrderKept)            // json.stringify(json.parse("{\"z\":1,\"a\":2,\"m\":3}")) == "{\"z\":1,\"a\":2,\"m\":3}"
TEST_F(JsonModule, StringifiesValues)       // json.stringify([1, 2.0, "x", true, none]) == "[1,2.0,\"x\",true,null]"
TEST_F(JsonModule, IntKeysAsStringsFirst)   // var t = {b: 1}; t[5] = 2 -> "{\"5\":2,\"b\":1}"
TEST_F(JsonModule, Indent)                  // json.stringify({a: [1]}, 2) == "{\n  \"a\": [\n    1\n  ]\n}"
TEST_F(JsonModule, RoundTripNested)         // parse(stringify(v)) prints the same as v for a nested table/list
TEST_F(JsonModule, BadTextGivesLineAndColumn) // json.parse("{\n  \"a\": }") -> RUNTIME_ERROR, error text contains "line 2"
TEST_F(JsonModule, SelfContainingIsAnError) // var t = {}; t.me = t; json.stringify(t) -> RUNTIME_ERROR
TEST_F(JsonModule, FunctionIsAnError)       // fun f() {} json.stringify({f: f}) -> RUNTIME_ERROR
TEST_F(JsonModule, DeepNestingIsAnErrorNotACrash) // text of 100000 "[" then 100000 "]" (built in C++, defined as a global) -> RUNTIME_ERROR   (Review Focus 3)
TEST_F(JsonModule, InvalidUtf8IsReplaced)   // a string holding the byte 0xFF (built in C++ and defined as a global) -> stringify contains "�" or the UTF-8 of U+FFFD   (Review Focus 5)
TEST_F(JsonModule, AvailableInSetupVm)      // import "json" as json works with only ecs.setupVm
```

- [ ] **Step 2: Run** `cd release && make -j4 test_compiler && ./test_compiler --gtest_filter='JsonModule*'` — Expected: fails (module missing).
- [ ] **Step 3: Implement** `JsonModule`.
- [ ] **Step 4: Run** the same — Expected: all pass; `./test_compiler` whole — no new failures.
- [ ] **Step 5: Commit** `git commit -m "PgScript: a json module (parse, stringify)"`.

### Task 3: `os.call`

**Files:**
- Create: `/src/Engine/Helpers/osmodule.h`, `/src/Engine/Helpers/osmodule.cpp`
- Modify: `/CMakeLists.txt` (add `src/Engine/Helpers/osmodule.cpp` to both engine source lists; the test file to test_compiler)
- Modify: `/examples/PgCompiler/application.cpp` (after each `ecs.setupVm(*vm);`, lines 175 and 291: `vm->addNativeModule("os", OsModule{});`)
- Create: `/test/pgcompiler/unit/test_os_module.cc`

**Interfaces:**
- Produces: `class OsModule : public NativeModule`; this task exports `call(command)` / `call(command, options)`. Task 4 adds the rest to the same class.
- Behaviour is the spec's. Argument errors (`std::runtime_error`, "os.call: ..."): `command` neither a string nor a non-empty list of strings; `options` not a table; an `options` key other than `input` and `cwd`; `input` or `cwd` not a string.
- `#ifdef _WIN32`: `call` throws "os.call is not available on Windows yet".

How: `posix_spawn_file_actions` duplicates three pipes onto 0/1/2 and, with `cwd`, changes folder with `posix_spawn_file_actions_addchdir_np` (glibc 2.29+, this machine has 2.35; macOS 10.15+). A `cwd` that is not an existing folder is checked before spawning (→ 127). A string runs `/bin/sh -c <command>`, a list `posix_spawnp(argv[0], argv)`; the parent's pipe ends are `O_CLOEXEC`; the parent writes `input` and reads stdout and stderr in one `poll` loop until both reach EOF, then `waitpid`. SIGPIPE: block it in the calling thread around the loop (`pthread_sigmask`), treat `EPIPE` on stdin as "input not wanted" (close it, keep reading), and drain a pending SIGPIPE with `sigtimedwait` before unblocking. A spawn failure (`posix_spawnp` returns `ENOENT`, `EACCES`, a bad `cwd`) → code 127 and `err` "os.call: cannot start <program>: <strerror>".

- [ ] **Step 1: Write the failing tests** (`/test/pgcompiler/unit/test_os_module.cc`; the fixture registers `OsModule` on the VM after `ecs.setupVm(vm)`, as the command-line tool does; scripts `import "os" as os`; result in a `var` before reading fields)

```cpp
TEST_F(OsCall, StringRunsThroughTheShell)   // var r = os.call("echo hi | tr a-z A-Z") -> r.code 0, r.out "HI\n", r.err ""
TEST_F(OsCall, ErrAndCode)                  // os.call("echo x >&2; exit 3") -> code 3, err "x\n"
TEST_F(OsCall, ListRunsDirectly)            // os.call(["printf", "%s", "a'b $HOME c"]) -> out "a'b $HOME c"
TEST_F(OsCall, InputReachesTheCommand)      // os.call(["cat"], {input: "abc"}) -> out "abc"
TEST_F(OsCall, BigInputAndOutputTogether)   // 8 MB input to ["cat"] -> out has the same length (no deadlock); also the same through
                                            // "cat; cat >&2"-style both streams: os.call("cat | tee /dev/stderr", {input: 4 MB})
TEST_F(OsCall, MissingProgramIs127)         // os.call(["no-such-program-xyz"]) -> code 127, err contains "no-such-program-xyz"
TEST_F(OsCall, Cwd)                         // os.call(["pwd"], {cwd: <temp dir>}) -> out is the temp dir + "\n"; a missing cwd -> 127
TEST_F(OsCall, KilledBySignal)              // os.call("kill -9 $$") -> code 137
TEST_F(OsCall, UnknownOptionIsAnError)      // os.call(["true"], {inputs: "x"}) -> RUNTIME_ERROR
TEST_F(OsCall, BadCommandIsAnError)         // os.call([]) and os.call(5) and os.call([1]) -> RUNTIME_ERROR
TEST_F(OsCall, InputNotReadNoSigpipe)       // os.call(["true"], {input: 5 MB}) -> code 0; the test process is still running   (Review Focus 1)
TEST_F(OsCall, NoDescriptorLeak)            // 300 calls of ["true"] in a loop, then os.call(["echo", "ok"]).out == "ok\n"; count of /proc/self/fd
                                            // entries before and after equal   (Review Focus 2)
TEST_F(OsCall, BytesComeBackUnchanged)      // os.call(["printf", "\\377"]) -> out is the single byte 0xFF (checked from C++)   (Review Focus 5)
TEST_F(OsCall, NotInSetupVm)                // a VM given only ecs.setupVm: import "os" -> COMPILE_ERROR
```

- [ ] **Step 2: Run** `cd release && make -j4 test_compiler && ./test_compiler --gtest_filter='OsCall*'` — Expected: fails (module missing).
- [ ] **Step 3: Implement** `OsModule::call`, the registration in `application.cpp`.
- [ ] **Step 4: Run** the same — Expected: all pass; `./test_compiler` whole — no new failures; `make -j4 PgCompilerBootstrap` builds, and `release/PgCompilerBootstrap` on a script `import "os" as os` / `var r = os.call(["echo", "cli"])` / `print(r.out)` prints `cli`.
- [ ] **Step 5: Commit** `git commit -m "PgScript: an os module for the command-line tool, os.call"`.

### Task 4: the rest of `os`, and the docs

**Files:**
- Modify: `/src/Engine/Helpers/osmodule.h`, `/src/Engine/Helpers/osmodule.cpp`
- Modify: `/test/pgcompiler/unit/test_os_module.cc`
- Modify: `/docs/compiler/QUICK_START.md` (imports: the `as` form; the `json` and `os` modules, one example each, `os` command-line only)

**Interfaces:**
- Consumes: `OsModule` (Task 3).
- Produces: `env(name)`, `time()`, `formatTime(ms, format)`, `listDir(path)`, `isDir(path)`, `makeDir(path)`, `exit(code)`, behaviour as the spec. `formatTime` uses `gmtime_r` + `strftime` with a 256-byte buffer (a longer result → script error). `listDir` via `std::filesystem::directory_iterator`, names sorted with `std::sort` on the strings. `makeDir` via `std::filesystem::create_directories`. `exit` flushes `std::cout` then `std::exit(code)`.

- [ ] **Step 1: Write the failing tests**

```cpp
TEST_F(OsModule, Env)              // setenv("PG_OS_TEST", "v", 1) in C++; os.env("PG_OS_TEST") == "v"; os.env("PG_OS_UNSET_XYZ") typeOf "none"
TEST_F(OsModule, Time)             // os.time() within 5000 ms of the C++ clock's ms since epoch; typeOf "int"
TEST_F(OsModule, FormatTime)       // os.formatTime(1794830998386, "%Y-%m-%d %H:%M:%S") == "2026-11-16 12:09:58"
TEST_F(OsModule, ListDirSorted)    // temp dir with b, a, .hidden, sub/ -> [".hidden", "a", "b", "sub"]; a missing path -> RUNTIME_ERROR
TEST_F(OsModule, IsDir)            // dir true, file false, missing false
TEST_F(OsModule, MakeDirWithParents) // os.makeDir(tmp + "/x/y/z") then os.isDir true; again: no error; over a file -> RUNTIME_ERROR
TEST(OsModuleExit, ExitsWithTheCode) // EXPECT_EXIT(run "import \"os\" as os / os.exit(7)", ::testing::ExitedWithCode(7), "")
```

- [ ] **Step 2: Run** `cd release && make -j4 test_compiler && ./test_compiler --gtest_filter='OsModule*'` — Expected: fails.
- [ ] **Step 3: Implement** the functions; write the doc section.
- [ ] **Step 4: Run** the same, then `./test_compiler` whole — Expected: all pass, no new failures.
- [ ] **Step 5: Commit** `git commit -m "PgScript: os env, time, folders and exit; docs"`.
