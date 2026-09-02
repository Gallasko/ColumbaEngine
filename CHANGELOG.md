# Changelog

All notable changes to ColumbaEngine are documented here. The project is in early development (pre-1.0): minor versions may contain breaking changes, and migration notes are included when they do.

## [0.9.0] - Unreleased

First tagged release. Everything below describes the state of the engine at the point of tagging rather than a delta.

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
