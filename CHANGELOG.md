# Changelog

All notable changes to ColumbaEngine are documented here. The project is in early development (pre-1.0): minor versions may contain breaking changes, and migration notes are included when they do.

## [0.9.0] - Unreleased

First tagged release. Everything below describes the state of the engine at the point of tagging rather than a delta.

### Fixed
- Chronicle: a short, wide window showed the compact page whatever the life. The three columns asked for
  a window 840 high, a figure that still counted the parts panel under the log, where it no longer is. The
  least height is the right column's (664), and the left column, which grows with what he holds and has
  learned, is measured once the page has settled: the compact page comes only when the window is too
  short for it, and the three columns come back when the window grows or a new life begins.
- Chronicle: the ending's veil covered 1320 x 1020 whatever the window's size (anchors on both sides
  do not stretch a shape); the scene sizes it to the window, on every resize, and it dims the page
  (`ink` at 40%) instead of washing it out.
- Chronicle: a tile, a heading, a running row or its progress rule made while the game runs showed
  for a frame at the window's top left, before its anchors placed it. They are made off the stage
  (`Core/offstage.h`), as the tooltip gloss already was.
- PgScript: `import` of a module that has a compiled `.pgc` beside it no longer breaks when the module
  defines functions. The imported bytecode was run through the optimization passes a second time (a crash in
  `PoppingJumpPass`) and the functions it carried were never decoded (a crash at the first call). It is now
  taken as compiled, its function tree decoded, and the native modules it was compiled against are loaded.
- Table copies (`serializeToTable`): an empty string field is kept instead of being dropped, an empty
  vector element or map value no longer stores an uninitialized value, nested tables inside vectors and
  maps no longer leak a reference, and a malformed number reads as 0 instead of throwing.
- `deserializeTo` and `deserializeComponentFromTable` skip the table fields that have no serialized form
  (functions) instead of emitting an untyped attribute, and write doubles with full precision.
- `deserializeEntityFromTable` with `createNew = false` reuses the entity named by `__entityId`; the id was
  read from the raw value and never matched.
- Prefab trees: a node keeps the z its factory gave it. The wrapping container was left at z 0 and its main
  entity follows the container's z, so after the first frame every node of a built tree sat on one depth and
  shapes of the same material drew in an arbitrary order (in Chronicle, the panel grounds vanished behind
  the page).
- Prefab trees: `flow` on a node without a kind (a bare container) now chains its children from the
  container; it was silently ignored. When two nodes of one scope take the same name the first keeps it
  (a sibling used to replace the earlier one, and a child could replace its parent node's own name), with
  the same error log as before. A reserved name (`main`, `parent`) on a child of a layout node is now
  reported, and anchors on the children of a layout node are reported once instead of being dropped
  silently.
- Chronicle: the running activity's progress rule in the activity list stayed empty for the whole term;
  only the copy under "At work now" was fed. Both rows now fill month by month.

### Removed
- The tree-walking interpreter: `PgInterpreter`, `Interpreter`, `InterpreterSystem`, `Environment`,
  `Valuable`, `Function`, `SysModule`, `Resolver` and the system functions, with their sources under
  `src/Engine/Interpreter/` (only the lexer, the tokens, the AST nodes, the parser and `visitor.h` remain).
  Every script goes through the bytecode VM.
- The interpreter modules `core`, `time`, `scene`, `audio`, `2Dshapes`, `2Dtexture`, `uitext`, `position`,
  the interpreter `ui`, `input`, `ecs`, `log`, `renderer` and `theme` headers, `EntitySystem::createInterpreterSystem`
  and the `InterpreterSystem` overloads of `ComponentRegistry::addEventListener` / `removeEventListener`.
  Migration: write a `NativeModule` and register it with `VM::addNativeModule` or
  `EntitySystem::registerCustomVmModule`; systems written in script go through `StandardSystem`.

### Changed
- Chronicle: the three columns are the page down to a window of 680 x 566 (it was 1136 x 664). They come
  in three widths and three heights, the roomiest the window holds: the two fixed columns narrow from
  300 / 360 to 200 / 280, the margins from 16 to 6 and the gaps from 12 to 4. The scene writes a step into
  the page before it builds it (`shapeColumns`); the compact page is only what is left under that, or for a
  left column the window is too short for.
- Chronicle: plain names. The panels are Inventory, Stats and Log (they were "What he holds", "What he has
  learned" and "What has happened"), the compact page's tabs Stats, Inventory, Timeline and Log, and its
  choice Activities. The row at work shows its name first: no tally in a column of the three.
- Chronicle: play sessions are sent to the analytics proxy from a browser (`Core/analytics.h`, after
  GameDevJs2026's): `chronicle.session_start`, `chronicle.session_end` every time the page is hidden or
  closed, with the time the page was in view and a one-line digest of where the life stood
  (`LifeSave::digest`), and `chronicle.life_end`. The session id is random and new for every page load.
  Nothing is sent from a native build or from a page served from localhost.
- Web builds without threads: `-DWEB_THREADS=OFF` (a build tree of its own) builds the engine with
  `PG_NO_THREADS`. The ecs has no thread and no taskflow executor: `EntitySystem::start()` only marks it as
  running and the frame callback runs the systems through `executeOnce()`, a few passes a frame, on a serial
  scheduler (`ECS/serialtaskflow.h`) that keeps the order the task graph asks for. The window is made without
  the init thread, and the save folder is mounted from IndexedDB (written back on its own) instead of OPFS.
  Such a build starts on a page that is not cross-origin isolated, a frame on another site included. Saves
  do not pass between a build with threads and one without.
- Web builds: `Window::swapBuffer()` no longer calls `glGetError` every frame in a release build. In a
  browser that call waits for the GPU to finish the frame, and the message it would log is compiled out there.
- Web builds: a fresh build tree finds the generated components again. The committed seed is copied under
  `generated/Components`, the layout the engine's includes expect (`-DPREGENERATED_COMPONENTS_DIR` is no
  longer needed for that).
- Chronicle: `web/index.html` is now `web/index.html.in`: the page is written when the game is packed
  (`web/packpage.cmake`). It is told whether its build needs threads, and only then asks for a cross-origin
  isolated page, and it asks for `Chronicle.js`, `.wasm` and `.data` under a stamp made from the three, so
  that a browser which kept one file of the build before never pairs it with a new one (a `LinkError` on
  hosts that cache `.js` longer than `.wasm`).
- Chronicle: a life is saved on its own, after every month, every choice, every thing done at once and every
  new life, with no line in the log (`LifeScene::autoSave`; never with `--no-save`). `S` still writes it and
  says so. A life that has just ended is not written: loaded again, its last month ends it again.
- Chronicle on the web: with no save a player begins a new life at 7 (`LaunchOptions::freshWithoutSave`)
  instead of the mockup's life, and `make ChronicleWeb` packs `chronicle-web.zip` (`web/index.html` and the
  three built files) ready to upload.
- Chronicle has a web target: `make Chronicle` in an Emscripten build tree gives `Chronicle.html`. Its pack
  holds only what the game reads (shaders, boot scripts, its fonts, icons, pages and `rules/*.pg`), taken
  from the source tree (`CHRONICLE_WEB_FILES` in `CMakeLists.txt`), and the target relinks when one of those
  files changes. See `examples/Chronicle/README.md`.
- Web builds: the `res/`, `scripts/` and `shader/` preloads are no longer in the global linker flags. Each
  web example adds them through `WEB_DEFAULT_PRELOAD`, so a target can pack its own files instead.
- Boot scripts run on the bytecode VM: `Window` owns a `VM` (`window.vm`, set up with `EntitySystem::setupVm`)
  instead of a `PgInterpreter`, and `res/logManager.pg`, `res/setupRenderer.pg`, `res/sysRegister.pg` and the
  editor's `res/sysThema.pg` go through it. The `log`, `renderer` and `theme` modules are ported to
  `NativeModule` (`Systems/lognativemodule.h`, `Renderer/renderernativemodule.h`,
  `Systems/themenativemodule.h`). Migration: `window.interpreter` is gone; use
  `window.vm->addNativeModule(...)` and `window.vm->interpretFromFile(...)`. `registerRenderSystems` takes
  a `VM*`.
- Scene enter and leave scripts, and any `ExecuteFileScriptEvent` / `ExecuteCodeScriptEvent`, run on the VM
  through the new `ScriptRunnerSystem` (`Systems/scriptrunner.h`, registered with the core systems). The
  events carry a `ScriptFunctions` map of native functions instead of `CustomSysFunctions`, and
  `getCurrentScene()` returns a table.
- `Visitor` lives in `Interpreter/visitor.h` and its `visit` methods, like `Expression::accept`, return
  nothing.
- The AST front-end (`ScriptFrontEnd::Ast`) is the default for `VM` and `EntitySystem`; select the Pratt
  compiler with `setVMFrontEnd(ScriptFrontEnd::Pratt)` / `VM::setFrontEnd`.
- PgScript: `none` is a value and a literal for the absence of a value. It is falsy, equal only to itself
  (`none == 0` and `none == false` are false), printed as `none`, and `typeOf(none)` is `"none"`. `none` is now
  a reserved word. A function that ends without a `return`, or runs a bare `return`, returns `none` (it was
  `0`), and `not none` is `true`. A `var` without an initializer still reads as `0`. From C++:
  `makeNoneValue()` and `IS_NONE`.
- PgScript: reading something that is not there gives `none`, so a lookup no longer needs a `contain` guard.
  `t.missing` and `v[i]` or `s[i]` past either end were runtime errors, `t["missing"]` gave `false`, an unknown
  property of a component proxy gave `-1`, and the holes of a sparse vector literal were `0`. Writing past the
  end of a vector is still an error. A field set to `none` stays a field (`contain` is true, for-in visits
  it). Migration: a test like `t["k"] == false` on a missing key is now false; compare with `none` or use
  `not t["k"]`. From C++: `ObjInstance::getField` returns none for a missing field (it was the int `0`).
- PgScript natives that have nothing to return give `none` instead of `0`: `print`, `sendEvent` and
  `randomSeed`.
- VM globals: `VM::findGlobal(name)` replaces `findDefinedGlobal` and `findGlobalCell`. It returns the `Value`
  itself, which is `IS_UNDEFINED` when the global was never declared or not assigned yet; `VM::GlobalCell` is
  gone and `globalCells` is a `std::vector<Value>`. Migration: `cell->value` becomes the returned value, and
  the `nullptr` / `defined` checks become `IS_UNDEFINED(value)`.
- Script bridge: `Compiler/ecsserialization.h` is split by concern into `Compiler/componentproxy.h`
  (proxies and their metadata), `Compiler/componentattach.h` (`attachComp` handlers) and
  `Compiler/tableserialization.h` (table copies); it still includes all three. A component proxy now holds
  its component and metadata pointers directly (`ObjInstance::proxyTarget` / `proxyMeta`) and the property
  opcodes forward to the component without a field lookup, a metamethod call or a name string. See
  `docs/COMPONENT_PROXY_SYSTEM.md`. Migration: `ComponentProxyRegistry::getMetadata()` is replaced by
  `findMetadata()` (returns a pointer, null when missing), `ComponentProxyMetadata::properties` is a vector
  in declaration order with `addProperty()` / `findProperty()`, the accessors of `PropertyMetadata` are
  function pointers, and `ComponentProxy::createProxy()` takes the metadata pointer. Generated components
  pick all of this up on the next build; refresh the committed seed with `SnapshotComponents`.
- Script bridge: the table-with-setters fallback is removed (`ComponentSerializerRegistry`,
  `REGISTER_COMPONENT_SERIALIZER`, `REGISTER_COMPONENT_PROXY`, the `REGISTER_*_SETTER` macros and the
  generated `serialize<Name>WithSetters` functions). Components are written from scripts by assignment
  (`pos.x = 10`), which calls the component setter; `pos.setX(10)` is not available. The
  `customSerializationSetter` key of a `.pgcomp` field is no longer read. A write to a read-only proxy
  property now stops the script with a runtime error.
- Compile time: the component registry stores its per-component and per-event callbacks through one
  function-pointer type per signature instead of one lambda type each, `ECS/entitysystem.h` no longer
  includes `Renderer/rendercall.h`, and `Compiler/vm.h` no longer includes the lexer or the AST pass
  headers (`VM::astPassManager` is now a `std::unique_ptr`). `ECS/entitysystem.h`, `ECS/commanddispatcher.h`
  and `Renderer/renderer.h` no longer include `Memory/concurrentqueue.h` (the queues live in the `.cpp`
  files), the six shape render systems compile their `GenericRenderSystem` base once, and the
  `stringToX` enum maps of generated components, `position.h`, `gamefacts.h` and `achievement.h` are
  declared `extern` and built once. Migration: include `Renderer/rendercall.h`, `Interpreter/lexer.h`,
  `Compiler/ast/ast_pass.h`, `Memory/concurrentqueue.h` or the standard headers you use (`<array>`,
  `<limits>`, ...) directly instead of relying on these headers to bring them in.
- Achievements: `Achievement`, `AchievementReward` and `AchievementSys` live in `Systems/achievement.h`,
  promoted from the GameOff example. An achievement is a list of `FactChecker` on the world facts and a list
  of rewards (a `StandardEvent`, or `AddFact` / `IncreaseFact` / `RemoveFact`); unlocking sends the
  `achievementUnlocked` event and the `<name>_unlocked` fact. New: `AchievementSys::clear()`, and an
  achievement added with `addNewAchivement` that is already complete now sends its rewards and events like
  any other. Migration: include `Systems/achievement.h` instead of the example's `achievement.h`. The
  Chronicle example uses it for its deeds, next to tasks with limited uses, tasks that change with
  repetition and holdings that produce or deplete each month (`examples/Chronicle/rules/`).
- Chronicle: a life whose Vitality reaches 0 is lost and a new one begins. Vitality
  stands under the clock's age, turns red when a month takes from it, and the first such month stops the
  running months; what was taken mends, one every two months, up to the most it can be (`vitmax`). The ledger rows have a tooltip (what is held, its limit, what it brings or uses), holdings can
  have a limit (`statLimits` in `rules/lib.pg`) that locks what would fill them, and an activity row shows
  how often it was done and what is left of a limited one. The door meters left the Life page.
- Chronicle: the 7-30 alpha balance of `res/chronicle/chronicle-balance`. Fifty activities on three
  exclusive paths (the Keep, the Collegium, the Hidden Hand) replace the mockup's table, with the
  Warrior, Mage and Thief skills (Arms, Discipline, Lore, Arcana, Stealth, Guile, Renown), reagents and
  favors. An activity's costs are taken when it begins; work with meals provided eats no ration and costs
  no Vitality; an activity opens at its `fromAge` and must end by its `finishBy`; parts stop at 30 and
  skills at 10; a life with no Vitality left ends before the term pays. An activity is listed once he is
  of age for it and holds most of what it asks (75% on average, costs aside), so a child is not shown
  every locked door at once. A fresh life starts with 12
  rations. The Guild's letter and its wage are gone.
  Migration: a save from before holds the old stats (`swd`, `ride`, `haggle`, `letter`); start a new
  life.
- Chronicle: confirming an activity starts the months, one every 2 seconds (`--month-ms N` to change
  it), and they stop when its term ends. The running row's progress rule fills through each month in
  the month's time (`ProgressRule::setGlide`, `ActivityRow::setGlide`). `SPACE` pauses and resumes
  work under way, keeping what the month had run; at nothing it does nothing, and a *Pass a month*
  button in "At work now" passes one. The head of "At work now" reads
  `RUNNING`, `PAUSED · SPACE` or `IDLE` (`Panel::setAsideColor`), and a month that stops the work
  because it took from Vitality says so in the log, with the key that goes on.
- Chronicle: a life no longer stops at 30. The last milestone ends his prime: every activity of it
  closes, an *Old age* group (tales at the inn, a garden, teaching) and the market's rations stay,
  and the most his Vitality can be falls by one every two months (`aging` in `rules/resources.pg`)
  until the life ends, with its own line in the next life's log. `resources.pg` takes `age`.
  Migration: `Rules::month` takes the age first (`month(age, character, board, out)`).
- Chronicle: a life that ends is no longer replaced at once. Its ending comes up over the page
  (`res/chronicle/ui/ending.yaml`): who he was, how it ended, what he became, the work he went
  back to most, what he left, the deeds told of him and his figures, written by the new
  `rules/epitaph.pg`. Nothing passes until *Begin a new life* (or `N`).
- Chronicle: nothing is cut or runs out of its row any more. A log line wraps instead of ending in
  an ellipsis, and its row grows; a figure too long to share the line goes under the text. An
  activity's gains wrap in the row, which grows by the lines they take. In a log figure an amount
  holds to its stat by a no-break space.
- Chronicle: the great step of each age is listed ahead of its time, whatever he has of what it
  asks: the three ways into a path from 13, a path's proving from 16, its mastery from 21. The row
  stands locked with what it asks, the age first. An activity sets it with `showAge` and `showFrom`
  (`rules/activitytable.pg`).
- Chronicle: an activity says when it closes. From two years before he can no longer begin it, its
  row carries a line saying when (`CLOSES 5/11`: month/year of the world's date), in red for the last six
  (`ActivityRow::setUntil`, `until` and `urgent` from the rules), so no row leaves the list
  unannounced.
- Chronicle: tooltips read in colour and in sections. A gloss row may carry a tone (`gain`, `loss`,
  `time`, `muted`) that colours its value, or be a section heading (`GlossRow::tone`, `heading`);
  an activity's gloss is laid out as what it brings, takes and asks, a requirement green when he
  has it and red when he is short, and a holding's rows come toned from `rules/resources.pg`.
- Chronicle: the rows of the choice are compact (`ActivityRowSpec::compact`, `compact: true` on an
  `ActivityList`): a name, the time it takes and when it closes, on a ground that says its kind
  (work that takes months, a thing done at once, what he cannot do yet). The gains, the count, the
  requirements and the progress rule left the row for its gloss, which gains a head (the count
  beside the name, `GlossSpec::aside`, the group under it) and a hair rule between its sections.
  The progress rule stays in "At work now".
- Chronicle: the choice is a grid. With a tile width (`ActivityListSpec::tileWidth`, `tileWidth:`
  in a file) a list lays its rows as tiles, as many to a line as fit: three at 1320, more as the
  window widens. A tile is the name over the time and the closing; the tiles of a line are level,
  and another width lays them again. The last month an activity may be begun reads
  `CLOSES THIS MONTH`.
- Chronicle: on the Life page his Strength, Dexterity and Intelligence stand in "What he has
  learned", over his skills, and the panel of parts under the log is gone: the log takes the
  column to the bottom. The market's group comes right under Work. An activity's gloss names a
  gain or a cost by the stat's full name (`Strength  +1`, from the new `name` of `gains` and
  `costs`), and no longer says the age it is done by nor how to begin it.
- Chronicle: in the Life screen's glosses a value follows its label instead of standing in a
  column at the right (`GlossSpec::inlineValues`), and a section's heading is set in the figures'
  weight and full ink. An activity's gloss shows the number of times it was done alone beside the
  name (`3`, `0/2` for one that can be done twice), and no footnote on a locked one.
- Chronicle: the Life page is tighter. Its head is two lines (the title in the heading face, the
  year with what he is beside it), the columns start at 70 instead of 178, stand 12 apart and 16
  from the window's edges, and the left one is 300 wide. The choice has no heading: the tabs stand
  at its head (`Tabs::setWidth`). A tile done at once leaves its time line empty instead of
  writing `NOW`. The page turns compact under 1284 x 840.
- Chronicle: the world has a calendar. It begins at Year 0 with the first life and runs on from
  life to life (`LifeSave::world`, the `world` input of the rules): a death does not rewind it. The
  Life page shows it at the top right (the year, then the month and its season) in place of his
  age, and an activity that closes says the date it closes on. The clock says his age to the
  month (`9 YEARS 10 MONTHS OLD`, `LifeClock::setUnit`) and no longer the next milestone.
- Chronicle: what a work leaves him besides a figure ("Mara's pupil", "Known to the Watch") has
  its row in the ledger, under TIES (`ties` in `rules/resources.pg`). A tie, a standing or a title
  (`title: true`) shows its name alone, with no `1` beside it, and its gloss states no count and
  no limit. A holding with a limit says both on one row of its gloss (`3/60`); one without says
  no limit at all. The work he is at keeps its
  tile when it could no longer be begun, and a running tile carries no edge: the edge is the
  selection's. The running row's caption is `MONTH 3 OF 6` alone. Only the Life tab is shown
  until the other pages exist.
- Facts: `WorldFacts`, `FactChecker` and the `AddFact` / `IncreaseFact` / `RemoveFact` events live in
  `Systems/gamefacts.h`, promoted from the GameOff example together with fact metadata and the event
  serializers. It replaces `Systems/factsystem.h`. Migration: include `Systems/gamefacts.h`; a save written
  under the old `factMap` key still loads, new saves use `worldFacts` and `factMetadata`. The Chronicle
  example is fed through `WorldFactsUpdate` instead of `GameDataView`.
- Theming: `ThemeSystem` + `ThemeComponent` replace `ThemeManager`. A theme file (`docs/THEMING.md`) holds
  colour tokens with per-theme values and aliases, scales, text styles with font files, and elements; an entity
  is painted by attaching `ThemeComponent{"element.key"}` and `setTheme` repaints everything. The Chronicle
  `Tokens`/`TextStyles`/`PaintSystem` are folded into it. Migration: `getSystem<ThemeManager>()` becomes
  `getSystem<ThemeSystem>()`, the flat `key.r/.g/.b/.a` properties become colour tokens in a theme file
  (`res/editor/theme.json` for the editor), and the script module keeps `setCurrentTheme`/`getCurrentTheme`.
- Prefab trees: `theme` is a node keyword like `kind` and `name`; the builder attaches a `ThemeComponent`
  with that element key to the node's leaf.
- Prefab trees: `buildTree` returns the root `EntityRef`, which always carries a `Prefab` (a layout root is
  wrapped like every other node). `PrefabBuildResult`, `buildNode`, `BuildContext` and typed handles are
  gone; a factory is `FactoryResult(EntitySystem*, const NodeSpec&)` returning entity, slot and
  childDefaults (`leafFactory` adapts a props-in, entity-out builder). A kit piece lives on its entity as a
  component (`getEntity("fed")->get<RequirementList>()`), its setters are also helpers on its `Prefab`
  (`callHelper("setItem", size_t{0}, 18, 18)`), `Prefab::findEntity` searches a subtree by name, and a
  prefab's helpers are dropped with it.
- Layouts: a layout follows what the position solver moves or resizes, not only setter calls. A layout
  placed by an anchor places its children again when it moves, and a child sized by a constraint (a prefab
  container following its main entity) makes its layout re-stack. `LayoutSystem` listens to
  `PositionSettledEvent` for it.
- Prefabs: a prefab leaves a child that a layout stacks to that layout, which decides what is in view and
  what clips it. An unclipped prefab no longer strips the `ClippedTo` a scrolling layout gave its rows.
- Clipping: a `ClippedTo` that arrives, changes or leaves marks its entity as changed, so it is drawn with
  the new clip on the next settle even when it does not move.

### Engine
- Pure-ECS core (sparse sets, groups, system traits) with Taskflow-based parallel scheduling
- Complete 2D pipeline: sprites, texture atlases, tilemaps, Aseprite import, 2D animation, tweens
- UI toolkit: TTF text, buttons, text input, list views, progress bars, anchor/sizer layout, theming
- 2D collision: spatial-hash broad phase, AABB narrow phase, raycasts (collision detection by design; dynamics stay in game logic)
- PgScript scripting language: bytecode VM with optimization passes, NaN-boxed values, ECS bindings, hot reload (native)
- Audio (music + SFX channels via SDL2_mixer), input (keyboard/mouse/gamepad), scenes, binary serialization and save system
- Component code generation from `.pgcomp` schemas, self-hosted in PgScript

### Platforms
- Linux (Ubuntu, Fedora, Arch), Windows (MinGW-64), WebAssembly (Emscripten, first-class)

### Tooling & distribution
- One-line installer that builds, installs, and scaffolds a starter app (`scripts/install/`)
- CMake package (`find_package(ColumbaEngine)`), CPack TGZ/DEB/RPM packaging
- `BUILD_EDITOR` option for the early-preview scene editor (native builds, ON by default)

### Known limitations
- Scene editor is an early preview
- Networking (TCP/UDP client-server) works but is not battle-tested
- Particle system is disabled pending rework; 3D rendering is on the roadmap
- Native Windows MSVC builds are not yet supported (use MinGW-64)
