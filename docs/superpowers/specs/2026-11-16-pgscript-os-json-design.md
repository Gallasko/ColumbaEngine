# PgScript `os` and `json` modules

2026-11-16. Project 2 of six towards the writing tool for columbaengine.org (project 1, site
analytics, is live; see "Where this fits").

## Why

The writing tool will be written in PgScript, run by the command-line tool
(`PgCompilerBootstrap` / `PgCompiler`), as the engine's other tools are
(`/tools/component_generator.pg`, `/tools/merge_tokens.pg`). It must run git, search the engine,
call an LLM API and the site's analytics over HTTPS (through `curl`), read the API key from the
environment, date articles, list folders, and read and write JSON (API replies, the article
records). PgScript has none of this today: scripts get `math`, `algorithm`, `string`, `file`
(`readFile`, `writeFile`, `fileExists`), `ecs` and a few globals, plus `getArg`/`getArgCount` in
the command-line tool. The author also wants this as a useful case for PgScript.

## Goals

- `import "module" as name`: a native module's functions bound to a table `name`, so
  `import "os" as os` gives `os.call(...)`. A plain `import "module"` stays flat, as every import
  is today (the author's rule: "like Python, `import "os"` is flat like any import and the `as`
  binds it to a table").
- An `os` native module: `call`, the environment, time, folders and exit, registered in the
  command-line tool only.
- A `json` native module, `parse` and `stringify`, registered everywhere.
- All three tested in `test_compiler`.

## Non-goals

- No change to a plain import: every existing script keeps working as it is, and nothing is
  rewritten to the `as` form in this project.
- `as` on a file import (`import "tools/lib" as lib`) is not part of this: it stays a compile
  error, with a message saying `as` takes native modules only for now.

- No HTTP client in the engine: HTTPS goes through `os.call` and `curl`. A libcurl module may come
  later if `curl` proves limiting.
- No sockets or HTTP server (project 5).
- No `os.call` on Windows yet, and none in the web build.
- No time limit inside `os.call`; network calls use `curl --max-time`.
- No streaming of a command's output while it runs: `os.call` returns when the command ends.

## Anti-goals

- `os` reachable from a game, a mod or the web build: a game script must never run a command.
  It is therefore never registered by `EntitySystem::setupVm`, so `import "os"` there fails
  ("not found as file or native module").
- A precompiled or cached chunk that imported `as` losing its table when it is loaded again.
- A string from data (an article title, an API body) being interpreted by a shell when the
  caller used the list form.
- A deadlock between writing a command's input and reading its output.
- A hand-edited JSON file losing its key order after a read and a write.

## Approaches considered

- **How scripts reach a module.** Asked of the author, who chose: plain imports flat, `as` binds a
  table. Weighed against: ready-made global tables `os`/`json` with no import (no language change,
  but two styles of module), and every import namespaced (about 250 scripts rewritten, and the
  analyses and fused paths that know natives by bare name, such as `charAt` and `len`, taught
  dotted calls in the same project).

- **`os.call`: `posix_spawn` with three pipes and a `poll` loop** (chosen). `popen` cannot give
  input and read output together, nor keep stderr apart; a large input and a large output must
  be written and read at the same time or both sides block. `fork`/`exec` does the same with more
  ways to go wrong (signal-safety after `fork` in a threaded process).
- **`json` over nlohmann's `ordered_json`** (chosen): `/src/Engine/Helpers/json.hpp` is already in
  the engine (used by `/src/Engine/Versioning/manifest.cpp`, `/src/Engine/UI/theme.h`, the
  Aseprite loader); `ordered_json` keeps keys in file order. Moving `arena::Json` from the
  `voxel-arena` branch (`/examples/VoxelArena/Core/json.cpp`) would bring that branch's code onto
  `Chronicle` for features not asked for (line numbers per value, comments). A parser written in
  PgScript would rewrite and debug what `json.hpp` already does. (`/tools/merge_tokens.pg` holds a
  PgScript JSON scanner, spans only, no values: not a library.)

## Design

### `import "module" as name`

- The parser already reads `import "x" as name` (`Parser::importStatement`,
  `/src/Engine/Interpreter/parser.cpp`, the `as` name in `ImportStatement::importName`); the AST
  compiler (`AstCompiler::visitStatement(ImportStatement*)`) ignores it today. It now builds a
  table holding each of the module's functions (and exported variables) under its own name, and
  binds it to the global `name`. No flat names are defined by an `as` import.
- `name` must be an identifier; anything else is a compile error. A module that does not exist is
  the same error as today.
- `as` with a file import is a compile error ("`as` takes a native module only for now").
- Both forms of one module in one script are allowed (`import "string"` and
  `import "string" as str`): the flat names and the table are independent.
- The import is recorded in the chunk with its name (`Chunk::importedModules` and what
  `/src/Engine/Compiler/chunk_serializer.cpp` writes), so a chunk loaded from the module cache or a
  `.pgc` file binds the table again (`vm_core.cpp`, `module_cache.cpp`, `cparser.cpp` replay
  `importedModules` today). The `.pgc` format changes: a `.pgc` written before is refused or
  read by its version, never misread (Chronicle's precompiled rules are rebuilt once, see
  `make ChronicleRules`).
- The table survives what the flat names survive (a VM that resets its globals re-binds it the way
  it re-defines `registeredNativeFunctions`).
- The Pratt front-end (`cparser.cpp`) gives a clear compile error for `as` instead of ignoring it;
  the AST front-end, the default, is the one that supports it.
- Calls through the table (`os.call(...)`) are ordinary field calls: the analyses that know
  natives by bare name do not see them, which is fine for `os` and `json`.

### `os` (command-line tool only)

Registered as a native module by the command-line tool, next to `getArg`
(`/examples/PgCompiler/application.cpp`, both places it calls `ecs.setupVm`), never by
`EntitySystem::setupVm`. Scripts write `import "os" as os` and call `os.call(...)`, or a plain
`import "os"` and call `call(...)`. Linux and macOS.

- `os.call(command)` / `os.call(command, options)`
  - `command` a string: run as `/bin/sh -c <command>` (pipes, globs, `&&` work).
  - `command` a list of strings: run directly (`posix_spawnp`, searched in `PATH`), the first
    element the program, nothing interpreted. An empty list or a non-string element is a script
    error.
  - `options`, a table, every key optional: `input` (a string written to the command's stdin,
    then stdin closed; without it stdin is empty, closed at once), `cwd` (a string, the folder the
    command runs in). An unknown key is a script error (a typo must not pass silently).
  - Returns a table `{code, out, err}`: `code` the exit code (an int); `out` and `err` everything
    the command wrote, as strings.
  - A command that fails is not a script error: it returns its code. A command killed by a
    signal returns `128 + signal`, as shells report it. A program that cannot be started (not
    found, not executable, a `cwd` that does not exist) returns code 127 with the reason in `err`.
    Only wrong arguments to `os.call` itself are script errors.
  - The command inherits the script's environment.
  - On Windows: a script error "os.call is not available on Windows yet".
- `os.env(name)`: the variable's value as a string, `none` when it is not set.
- `os.time()`: milliseconds since 1970-01-01 UTC, an int.
- `os.formatTime(ms, format)`: the time `ms` formatted in UTC with `strftime` conversions
  (`"%Y-%m-%d"` gives `2026-11-16`).
- `os.listDir(path)`: the names in the folder (no `.` and `..`), sorted by byte order, as a list. A
  missing path or a path that is not a folder is a script error.
- `os.isDir(path)`: true when the path is a folder (false when it does not exist).
- `os.makeDir(path)`: creates the folder and any missing parents; nothing when it already exists;
  a script error when it cannot (a file in the way, no permission).
- `os.exit(code)`: ends the program at once with that exit code.

Everything else a tool needs (walking a tree, `curl` calls, git) is written in PgScript on top.

### `json` (everywhere)

Registered as a native module by `EntitySystem::setupVm`, so games and mods have it too: it reads
and writes text only. `import "json" as json` gives `json.parse(...)`.

- `json.parse(text)`:
  - object → table (keys in file order), array → list, string → string, `true`/`false` → bool,
    `null` → `none`;
  - a number written without `.` or an exponent → an int when it fits PgScript's ints (±2^46),
    else a double; any other number (`2.0`, `1e3`) → a double, so a value saved as a double comes
    back as one and `/` keeps dividing it as a double (my call);
  - text that is not JSON → a script error giving the line and column.
- `json.stringify(value)` compact; `json.stringify(value, indent)` pretty with `indent` spaces:
  - list → array; table → object, string keys in the table's order, integer keys written as
    strings (`"5"`) before them, as the table holds them;
  - string, int, double, bool → themselves; `none` → `null`; a whole double is written with its
    `.0` (`2.0`), so it reads back as a double;
  - bytes that are not valid UTF-8 in a string are written as U+FFFD instead of failing (my call:
    command output goes into API bodies, and one bad byte in a git log must not stop the tool);
  - a double that is not finite, a function, a class instance that is not a plain table, or a
    table or list containing itself → a script error.
- `json.parse(json.stringify(v))` gives back an equal value for anything made of tables, lists,
  strings, numbers, bools and `none` (integer keys come back as string keys).

### Tests (`test_compiler`)

- `as`: `import "string" as s` then `s.strlen("abc")`; no flat `strlen` after it; both forms in one
  script; `as` with a missing module, with a file import, with a non-identifier (errors); a script
  importing `as` run again from the module cache and from a `.pgc` written and read back (the table
  is there); the Pratt front-end's error.

- `json`: each type both ways; key order kept through parse and stringify; nested values;
  integers past ±2^46 as doubles; `none` ↔ `null`; the indent form; errors (bad text with line and
  column, a self-containing table, a function).
- `os.call` with commands found on any Linux: `echo` (out), `sh -c 'echo x >&2; exit 3'` (err and
  code), `cat` with `input` (input reaches the command), an input and an output of several MB
  together (no deadlock), the list form with an argument holding `'`, `$` and spaces (passed
  untouched), a missing program (127), `cwd`, an unknown option key (error).
- `os.env`, `os.time`, `os.formatTime` on a fixed time, `os.listDir`/`os.isDir`/`os.makeDir` in a
  temporary folder.
- The tests register `os` on their VM themselves, as the command-line tool does.
- Known engine bug to keep out of the tests and the tool: reading a field straight off a table a
  call has just built (`os.call(cmd).out`) can read freed memory. Put the result in a `var` first.

## Left to the builder

- File names and layout (for example `/src/Engine/Helpers/osmodule.h/.cpp` and `jsonmodule.h/.cpp`
  beside `stringmodule.h`), and how they join the CMake targets the command-line tool and the
  tests link.
- Reference counting when building tables and lists from C++ (follow `createTable`,
  `ObjInstance::setField`, `setInt` and `StringModule`'s list building).
- How the `poll` loop reads and writes (buffer sizes, `EINTR`), and the error texts.
- Whether the macOS build is actually compiled and tried (Linux is the one that must work).

## Where this fits

The six projects: (1) site analytics, live; (2) this; (3) the writing tool's core as commands
(ideas, plans, drafts in several voices, code from the engine, stitching, publishing, Reddit/X
versions, the records); (4) SVG visuals in PgScript; (5) a local web page served by PgScript;
(6) live PgScript code boxes in articles.
