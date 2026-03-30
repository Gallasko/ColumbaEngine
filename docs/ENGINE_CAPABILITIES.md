# PgEngine (ColumbaEngine) — Engine Capabilities

**PgEngine** (also known as ColumbaEngine) is a free, open-source 2D game engine written in modern C++17. It is designed around a **pure Entity Component System (ECS)** architecture with a zero-overhead philosophy: systems you don't use cost nothing at runtime. It targets indie developers, students, and hobbyists who want complete control over their game development pipeline.

---

## Architecture Overview

The engine is built on three foundational pillars:

1. **Pure ECS Core** — Entities are containers of components. Systems execute logic over groups of entities. There is no inheritance-based game object hierarchy.
2. **Event System** — Systems communicate through typed C++ events and flexible dynamic `StandardEvent`s. Events can be listened to synchronously or queued for batched processing.
3. **Parallel Execution** — Systems run via **Taskflow 3.6.0**, allowing multi-threaded, dependency-aware parallel execution graphs.

---

## ECS System

**Entities** are identified by unique IDs. **Components** are plain data containers with optional lifecycle hooks (`onCreation`, `onDeletion`, `onCopy`). **Systems** declare their component dependencies through a template trait system:

```cpp
struct MySystem : public System<
    Own<ComponentA>,           // Owns and iterates these components
    Ref<ComponentB>,           // References without ownership
    Listener<MyEvent>,         // Listens to C++ typed events
    QueuedListener<MyEvent>,   // Deferred batched event processing
    InitSys,                   // Has an init() lifecycle method
    SaveSys,                   // Has save/load serialization support
    DeltaTime                  // Receives per-frame delta time
> { ... };
```

Systems can also be created dynamically at runtime via `StandardSystem` — a data-driven system with named properties and script-driven behavior, requiring no C++ compilation.

**Component Groups** allow systems to efficiently iterate over entities that match a specific combination of components.

---

## 2D Rendering

The engine has a production-ready 2D rendering pipeline built on **OpenGL** (via GLEW) and **SDL2**:

- **Sprite rendering** with full 2D transformations (position, rotation, scale)
- **Texture atlasing** for memory-efficient sprite sheets
- **Frame-based animation** using Aseprite JSON+PNG format or custom keypoint animators (`Texture2DAnimationComponent`)
- **Camera system** (`Camera2D`) with viewport management and culling
- **Multi-layer rendering** (Pre-Render, Render, Post-Process stages)
- **Opacity/blend modes**: Opaque, Normal, Additive, Subtractive
- **Scissor testing** for UI clipping regions
- **TrueType font rendering** via Freetype 2.13.3
- **Tile map loading** (`TileLoader`)

Supported image formats: PNG, JPG (via stb_image).

---

## Collision & Physics

- **Broad-phase**: Spatial hash grid (`PagePos`) for fast candidate filtering
- **Narrow-phase**: AABB (Axis-Aligned Bounding Box) rectangle collision
- **Ray casting**: Ray-AABB intersection for projectiles and line-of-sight queries
- **Surface normals**: Computed on collision for response
- Physics simulation (velocity, friction, drag) is intentionally lightweight and is left to game logic — typically implemented in PgScript for flexibility

---

## Audio

Powered by **SDL2_mixer 2.6.3**:

- Background music playback with loop control
- Multi-channel sound effects
- Master, music, and SFX volume controls independently
- Events: `StartAudio`, `StopAudio`, `PauseAudio`, `ResumeAudio`, `PlaySoundEffect`, `SetMasterVolume`, `SetMusicVolume`, `SetSoundEffectsVolume`

---

## Input Handling

Full input via **SDL2**:

- **Keyboard**: Full scancode set, press/grab/release state tracking
- **Mouse**: Button press/release, position, delta movement
- **Gamepad**: SDL2 game controller API, buttons and analog axes

Input state is available through the `Input` manager and can be attached to entities via `InputComponent`.

---

## UI System

A complete widget toolkit:

- `TTFText` — TrueType text rendering
- `Button` — Interactive buttons with hover/press states
- `TextInput` — Keyboard-driven text entry fields
- `ListView` — Scrollable item lists
- `ProgressBar` — Visual progress indicators
- `Focusable` — Focus management across inputs
- `Scrollable` — Scrolling container behavior
- `ThemeManager` — Consistent styling across UI elements
- Layout via `Sizer` and named anchors for responsive positioning
- Narrative/sentence text support for dialogue systems

---

## Scene System

- **Scene** — Abstract base class with `init`, `startUp`, `onLeave`, `execute` lifecycle
- **SceneFile** — Serializable scene data: entity lists, subscenes, and scripts triggered on enter/leave
- **Subscenes** — Nested scene hierarchies
- **SceneElementSystem** — State machine-driven scene loading and unloading
- Full serialization: scenes can be saved and restored from disk

---

## Scripting: PgScript

A custom scripting language with full **bytecode compilation** and a **register-based virtual machine**:

- Dynamically typed (similar to Lua)
- First-class functions and closures
- Tables (associative arrays)
- Import system for modular scripts
- `//` and `/* */` comment syntax
- **NanBox** value representation: 64-bit packed values for efficiency

**Compilation pipeline**: Source → Lexer → Parser → AST → Bytecode → VM

**Native modules callable from scripts:**

| Module | Capabilities |
|---|---|
| `TextureModule` | Create and manipulate 2D sprites |
| `PositionModule` | Read/write entity transforms |
| `UIModule` | Create text, buttons, and UI widgets |
| `AudioModule` | Play music and sound effects |
| `InputModule` | Query keyboard, mouse, gamepad state |
| `FileModule` | File I/O operations |
| `MathModule` | Trigonometry, random numbers |
| `StringModule` | String manipulation |
| `AlgorithmModule` | Utility algorithms |
| `TimeModule` | Delta time access |

Scripts integrate with the ECS through `sysData` (inter-system shared state), the event system, and `ComponentRegistry` for dynamic component access.

---

## Networking

Client-server multiplayer via **SDL2_net 2.2.0**:

- TCP (reliable) and UDP (fast/unreliable) transport
- Abstracted backend layer (pluggable transports)
- Connection state tracking per client (`ClientInfo`)
- Heartbeat/ping for connection monitoring
- RTT (Round Trip Time) calculation
- Fragmented packet reassembly
- `SendDataToServer` event for data dispatch
- Status: functional but early-stage; not yet battle-tested for production multiplayer

---

## Save / Load System

- Full binary serialization via the `Archive` format
- Entity and component persistence
- Scene snapshots
- Version compatibility support
- Systems opt in via the `SaveSys` trait

---

## Prefab System

- Prefabs define reusable entity templates
- Instantiated at runtime with components pre-attached
- Supports copying and cloning with component `onCopy` hooks

---

## Memory & Performance

- **MemoryPool**: Object pooling for allocation efficiency
- **ConcurrentQueue / BlockingConcurrentQueue**: Thread-safe queues
- **ThreadPool**: Managed worker threads
- **RWLock**: Read-write locking for shared data
- **ParallelFor**: Parallel iteration helpers
- Taskflow-based system execution graph with dependency resolution
- Cache-friendly ECS layout (data-oriented design)

---

## Profiling

- Built-in `Profiler` system measuring per-system execution time
- Taskflow execution graph export to `.dot` for visualization
- JSON export for external analysis
- Enabled with `-DPG_PROFILE` build flag

---

## Built-in Editor

A visual scene editor is included (`src/Editor/`):

- GUI-driven scene and entity creation
- Component inspector for editing properties
- Entity management and hierarchy view
- Property editing for components

---

## Cross-Platform Support

| Platform | Status |
|---|---|
| Linux (gcc/clang) | Primary / Fully supported |
| Windows (MinGW) | Supported |
| WebAssembly (Emscripten) | Supported with full feature set |
| macOS | Community contributions |

---

## Build System

- **CMake** with C++17
- All dependencies vendored in `import/` (no system installs required beyond OpenGL)
- Build flags: `-DPG_PROFILE`, `-DAUTO_CONVERT_EVENT`, `-DBUILD_EXAMPLES`, `-DUSE_GDB`
- Parallel builds: `cmake --build . -j$(nproc)`

**Vendored dependencies:** SDL2 2.28.5, SDL2_mixer 2.6.3, SDL2_net 2.2.0, Freetype 2.13.3, GLM (header-only), Taskflow 3.6.0, Google Test 1.14.0, GLEW, TinyGLTF, nlohmann/json, stb_image

---

## Examples Included

| Example | Demonstrates |
|---|---|
| Asteroid | Full arcade shooter: physics, bullets, collision, PgScript |
| TetrisClone | Scene system, UI, animation, key mapping |
| SimpleBoxBouncer | Minimal starter template |
| InvadersBreaker | Game loops, enemy spawning, collision |
| RenderingTest | 2D rendering showcase |
| SimpleClientServer | TCP/UDP networking |
| StandardSys | Dynamic StandardSystem API |
| PgCompiler / PgInterpreter | PgScript compilation and execution |
| PixelJam / GameOff / GMTK2025 | Complete game jam entries |
| EmptyAppTemplate | Clean project starting point |

---

## Key Design Decisions

- **Zero overhead for unused systems** — systems not registered have no runtime cost
- **Script-first for rapid iteration** — PgScript allows modifying gameplay without recompiling C++
- **Event-driven communication** — systems never call each other directly; they emit and listen to events
- **MIT License** — completely free, no royalties, no restrictions

---

## Codebase Scale

- ~174 engine header files across 18 subsystems
- ~41,000 lines of engine code
- 15+ example projects