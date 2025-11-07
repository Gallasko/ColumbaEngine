# ColumbaEngine

[![CI](https://github.com/Gallasko/PgEngine/actions/workflows/main.yml/badge.svg?branch=main)](https://github.com/Gallasko/PgEngine/actions/workflows/main.yml) [![Documentation Status](https://readthedocs.org/projects/columbaengine/badge/?version=latest)](https://columbaengine.readthedocs.io/en/latest/?badge=latest) [![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)

## About

ColumbaEngine is a **completely free and open source** game engine built with modern C++ and a pure Entity Component System (ECS) architecture. Whether you're building 2D games, prototyping ideas, or learning game engine development, ColumbaEngine provides a solid foundation without any licensing fees or royalties.

### Why ColumbaEngine?

- **100% Free and Open Source** - No hidden costs, no royalties, no licensing fees. Ever.
- **Pure ECS Architecture** - Built from the ground up with data-oriented design for maximum performance
- **Cross-Platform** - Deploy your games to Linux, Windows, and the web (with more platforms coming)
- **Transparent Development** - All development happens in the open with community input
- **No Vendor Lock-in** - Your project is yours. Use any tools, modify the engine, distribute freely
- **Modern C++17** - Clean, maintainable codebase using modern C++ best practices
- **Built-in Editor** - Visual scene creation without external dependencies
- **Lightweight** - Pay only for what you use - unused systems have zero overhead

## Quick Start

### One-Line Installation (Linux/macOS)

```bash
curl -sSL https://raw.githubusercontent.com/Gallasko/ColumbaEngine/main/scripts/install/install-engine.sh | bash
```

Or clone and run locally:

```bash
git clone https://github.com/Gallasko/ColumbaEngine.git
cd ColumbaEngine/scripts/install
./install-engine.sh
```

### Manual Installation

<details>
<summary>Click for detailed manual installation steps</summary>

#### Prerequisites
- C++ compiler with C++17 support (GCC, Clang, or MSVC)
- CMake 3.18+
- Git

#### Dependencies
The installation script will handle these automatically, but for manual installation:
- SDL2 (`libsdl2-dev`)
- SDL2-TTF (`libsdl2-ttf-dev`)
- OpenGL (`libgl1-mesa-dev` on Linux)

#### Build Steps

**Linux/macOS:**
```bash
# Install dependencies (Ubuntu/Debian example)
sudo apt update
sudo apt install cmake libgl1-mesa-dev libsdl2-dev libsdl2-ttf-dev

# Clone and build
git clone --recursive https://github.com/Gallasko/ColumbaEngine.git
cd ColumbaEngine
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . -j$(nproc)
```

**Windows (MinGW):**
```bash
# Ensure MinGW-64 and CMake are installed and in PATH
git clone --recursive https://github.com/Gallasko/ColumbaEngine.git
cd ColumbaEngine
mkdir build && cd build
cmake -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release ..
cmake --build . -j
```

**WebAssembly (Emscripten):**
```bash
# Install Emscripten first: https://emscripten.org/docs/getting_started/downloads.html
cd ColumbaEngine
mkdir build-web && cd build-web
emcmake cmake ..
cmake --build . -j
emrun ./ColumbaEngine.html
```

</details>

## Documentation

Full documentation is available at: **[columbaengine.readthedocs.io](https://columbaengine.readthedocs.io)**

- [Getting Started Guide](https://columbaengine.readthedocs.io/en/latest/getting_started.html)
- [API Reference](https://columbaengine.readthedocs.io/en/latest/api/api.html)
- [Tutorials & Examples](https://columbaengine.readthedocs.io/en/latest/tutorials.html)
- [Contributing Guide](CONTRIBUTING.md)

## Features

### Core Features
- **Pure ECS Architecture** - Data-oriented design with efficient component management
- **Complete 2D Rendering** - Production-ready 2D graphics pipeline
- **3D Support** - Coming soon
- **Built-in Scene Editor** - Visual scene creation and management
- **Custom Scripting (PgScript)** - Flexible scripting language for game logic
- **Pay-for-what-you-use** - No performance overhead for unused systems
- **Event System** - Efficient inter-system communication
- **Taskflow Integration** - Parallel system execution with profiling support

### Platform Support
- Linux (Ubuntu, Fedora, Arch)
- Windows (MinGW-64)
- WebAssembly (Emscripten)
- macOS (Community contributions welcome)

### Who is it for?

ColumbaEngine is perfect for:
- **Indie developers** looking for a lightweight, performant engine
- **Students** learning game engine architecture and ECS patterns
- **Hobbyists** who want full control over their game engine
- **C++ developers** seeking a modern, well-structured codebase
- **Anyone** who believes in open source and wants to contribute to a growing community

## Examples & Games

- **Full Tetris Clone** - Complete implementation showcasing engine capabilities
- **More Examples** - Available at [pigeoncodeur.itch.io](https://pigeoncodeur.itch.io/)

## Development

### Installation Options

The installation script supports various options:

```bash
# Install specific version
./install-engine.sh --version v1.0.0

# Custom installation directory (no sudo required)
./install-engine.sh --prefix ~/.local

# Specify number of build jobs
./install-engine.sh --jobs 4

# All options
./install-engine.sh --version main --prefix ~/.local --jobs 8
```

### Verify Installation

After installation, verify everything works:

```bash
# Run validation script
./scripts/install/validate-installation.sh

# Or test manually
cd ~/ColumbaEngine-install/test-app
./build.sh
./ColumbaEngineTestApp
```

### Profiling

Enable Taskflow profiling to analyze system scheduling:

```bash
# Linux/macOS
export TF_ENABLE_PROFILER=profile.json
./ColumbaEngine

# Windows PowerShell
$env:TF_ENABLE_PROFILER="profile.json"
.\ColumbaEngine.exe
```

## Contributing

We're actively seeking contributors to help build a thriving community around ColumbaEngine!

### Current Priorities
1. **Documentation** - Help us improve tutorials and API documentation
2. **Platform Support** - Test and improve cross-platform compatibility
3. **System Stabilization** - Help stabilize and optimize existing systems
4. **Editor Enhancement** - Contribute to the built-in scene editor
5. **Examples & Tutorials** - Create sample projects and learning resources

### How to Contribute
1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Follow our [coding style guide](codingstyle.md)
4. Commit your changes (`git commit -m 'Add amazing feature'`)
5. Push to the branch (`git push origin feature/amazing-feature`)
6. Open a Pull Request

See [CONTRIBUTING.md](CONTRIBUTING.md) for detailed guidelines.

## Project Status

**Stage:** Early Development - Expect breaking changes (with migration guides)

This project is under active development. While the 2D pipeline is production-ready, APIs may change as we refine the engine architecture. We provide migration guides for breaking changes.

## Troubleshooting

### Common Issues

<details>
<summary>Installation script fails</summary>

- Ensure you have sudo privileges for system installation
- Try installing to user directory: `./install-engine.sh --prefix ~/.local`
- Check [detailed installation guide](scripts/install/INSTALLATION.md)
</details>

<details>
<summary>Build errors</summary>

- Verify all dependencies are installed
- Check CMake version: `cmake --version` (requires 3.18+)
- Review build logs in the build directory
</details>

<details>
<summary>Performance issues</summary>

- Build in Release mode: `cmake -DCMAKE_BUILD_TYPE=Release ..`
- Enable profiling to identify bottlenecks (see Profiling section)
</details>

For more help, please:
- Check the [documentation](https://columbaengine.readthedocs.io)
- Open an [issue](https://github.com/Gallasko/ColumbaEngine/issues)
- Join our community discussions

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Acknowledgments

- SDL2 team for the excellent multimedia library
- Taskflow team for the parallel execution framework
- All contributors and community members

---

**Ready to build something amazing?** Get started with ColumbaEngine today!

```bash
# Start your journey
curl -sSL https://raw.githubusercontent.com/Gallasko/ColumbaEngine/main/scripts/install/install-engine.sh | bash
```

Star this repository to support the project!