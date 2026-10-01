# Changelog

All notable changes to ColumbaEngine are documented here. The project is in early development (pre-1.0): minor versions may contain breaking changes, and migration notes are included when they do.

## [0.9.0] - Unreleased

First tagged release. Everything below describes the state of the engine at the point of tagging rather than a delta.

### Changed
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
