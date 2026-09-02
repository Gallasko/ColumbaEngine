# ColumbaEngine Quickstart Template

The recommended starting point for a new ColumbaEngine game. It shows a bouncing box that changes color when it hits the screen edges — delete `boxbouncersystem.h` and replace it with your own systems once it runs.

## Prerequisites

ColumbaEngine must be installed (via the [install script](../../scripts/install/install-engine.sh) or `make install` from the engine repo). The CMakeLists here links it with `find_package(ColumbaEngine)`.

## Starting a new project

```bash
cp -r templates/quickstart ~/my-game
cd ~/my-game
./build.sh
cd build && ./ColumbaEngineTestApp
```

Rename the project by editing `project(...)` and the `add_executable`/`target_link_libraries` target name in `CMakeLists.txt`.

## Building

```bash
./build.sh            # Release build (default)
./build.sh debug      # Debug build
./build.sh clean      # Clean build
```

## Project Structure

- `src/main.cpp` - Entry point (a dozen lines; `pg::Engine` hides the SDL/platform boilerplate)
- `src/application.h/cpp` - Main application class using `pg::Engine`
- `src/boxbouncersystem.h` - Example ECS system, replace with your own
- `CMakeLists.txt` - Finds and links the installed engine
- `build.sh` - Build script with debug/release options

## What it demonstrates

- Using the `pg::Engine` class for setup
- Creating a custom system with `InitSys` initialization
- Listening to `TickEvent` for game-loop updates
- Creating 2D shapes with `makeSimple2DShape`
- Attaching custom components (`BouncingBox`)
- Bouncing off screen bounds

## Next steps

- Follow the Breakout tutorial in the engine docs to grow this into a real game
- Try multiple boxes, different shapes (Triangle, Circle), gravity, or input handling
