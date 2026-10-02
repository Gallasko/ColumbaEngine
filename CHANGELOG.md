# Changelog

All notable changes to ColumbaEngine are documented here. The project is in early development (pre-1.0): minor versions may contain breaking changes, and migration notes are included when they do.

## [0.9.0] - Unreleased

First tagged release. Everything below describes the state of the engine at the point of tagging rather than a delta.

### Fixed
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

### Changed
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
