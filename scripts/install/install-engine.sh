#!/bin/bash

# ColumbaEngine Installation Script
# Downloads, builds, and installs ColumbaEngine with dependencies
# Creates a basic test application

set -e  # Exit on any error

# Configuration
ColumbaEngine_VERSION="${ColumbaEngine_VERSION:-main}"  # Can be set via environment variable
ColumbaEngine_REPO="${ColumbaEngine_REPO:-https://github.com/Gallasko/ColumbaEngine.git}"  # Update this!
INSTALL_PREFIX="${INSTALL_PREFIX:-/usr/local}"
INSTALL_DIR="${INSTALL_DIR:-$HOME/ColumbaEngine-install}"
BUILD_JOBS="${BUILD_JOBS:-$(nproc)}"
BUILD_TYPE="${BUILD_TYPE:-Release}"  # Default to Release build

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Logging functions
log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Check if running as root for system installation
check_permissions() {
    if [[ "$INSTALL_PREFIX" == "/usr/local" && $EUID -ne 0 ]]; then
        log_warning "Installing to $INSTALL_PREFIX requires sudo privileges"
        log_info "You may be prompted for your password during installation"
    fi
}

# Detect OS and install dependencies
install_dependencies() {
    log_info "Installing system dependencies..."

    if [[ -f /etc/os-release ]]; then
        . /etc/os-release
        OS=$ID
    else
        log_error "Cannot detect OS type"
        exit 1
    fi

    case $OS in
        ubuntu|debian)
            sudo apt update
            sudo apt install -y \
                build-essential \
                cmake \
                git \
                libgl1-mesa-dev \
                libglu1-mesa-dev \
                libx11-dev \
                libxext-dev \
                libasound2-dev \
                libpulse-dev \
                libudev-dev \
                pkg-config
            ;;
        fedora|rhel|centos)
            if command -v dnf &> /dev/null; then
                PKG_MGR="dnf"
            else
                PKG_MGR="yum"
            fi

            sudo $PKG_MGR install -y \
                gcc-c++ \
                cmake \
                git \
                mesa-libGL-devel \
                mesa-libGLU-devel \
                libX11-devel \
                libXext-devel \
                alsa-lib-devel \
                pulseaudio-libs-devel \
                systemd-devel \
                pkgconfig
            ;;
        arch)
            sudo pacman -S --needed --noconfirm \
                base-devel \
                cmake \
                git \
                mesa \
                glu \
                libx11 \
                libxext \
                alsa-lib \
                libpulse \
                systemd \
                pkgconf
            ;;
        *)
            log_warning "Unsupported OS: $OS"
            log_info "Please install the following manually:"
            log_info "- Build tools (gcc, g++, make)"
            log_info "- CMake 3.18+"
            log_info "- Git"
            log_info "- OpenGL development libraries"
            log_info "- X11 development libraries"
            log_info "- Audio development libraries (ALSA, PulseAudio)"
            read -p "Press Enter to continue if dependencies are installed..."
            ;;
    esac
}

# Download ColumbaEngine source
download_ColumbaEngine() {
    log_info "Downloading ColumbaEngine version: $ColumbaEngine_VERSION"
    log_info "Working directory: $INSTALL_DIR"

    if [[ -d "$INSTALL_DIR" ]]; then
        log_warning "Directory $INSTALL_DIR already exists"
        read -p "Remove it and continue? (y/N): " -n 1 -r
        echo
        if [[ $REPLY =~ ^[Yy]$ ]]; then
            rm -rf "$INSTALL_DIR"
        else
            log_error "Installation cancelled"
            exit 1
        fi
    fi

    # Create directory with explicit permissions
    mkdir -p "$INSTALL_DIR"
    if [[ ! -w "$INSTALL_DIR" ]]; then
        log_error "Cannot write to directory: $INSTALL_DIR"
        log_info "Please ensure you have write permissions to this location"
        exit 1
    fi

    cd "$INSTALL_DIR"

    git clone --recursive "$ColumbaEngine_REPO" ColumbaEngine
    cd ColumbaEngine

    if [[ "$ColumbaEngine_VERSION" != "main" ]]; then
        git checkout "$ColumbaEngine_VERSION"
    fi

    log_success "ColumbaEngine source downloaded to: $INSTALL_DIR/ColumbaEngine"
}

# Build ColumbaEngine
build_ColumbaEngine() {
    log_info "Building ColumbaEngine in $BUILD_TYPE mode..."

    cd "$INSTALL_DIR/ColumbaEngine"
    mkdir -p build
    cd build

    # Use Unix Makefiles instead of Ninja to avoid dependency issues
    cmake .. \
        -G "Unix Makefiles" \
        -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
        -DCMAKE_INSTALL_PREFIX="$INSTALL_PREFIX" \
        -DBUILD_EXAMPLES=OFF \
        -DBUILD_STATIC_LIB=ON

    make -j"$BUILD_JOBS"

    log_success "ColumbaEngine built successfully in $BUILD_TYPE mode"
}

# Install ColumbaEngine
install_ColumbaEngine() {
    log_info "Installing ColumbaEngine to $INSTALL_PREFIX..."

    cd "$INSTALL_DIR/ColumbaEngine/build"

    if [[ "$INSTALL_PREFIX" == "/usr/local" ]]; then
        sudo make install
    else
        make install
    fi

    # Update library cache if installing to system directories
    if [[ "$INSTALL_PREFIX" == "/usr/local" ]]; then
        sudo ldconfig
    fi

    # Verify installation
    log_info "Verifying installation..."

    # Check main library
    if [[ -f "$INSTALL_PREFIX/lib/libColumbaEngine.a" ]]; then
        log_success "✓ Main library installed: $INSTALL_PREFIX/lib/libColumbaEngine.a"
    else
        log_error "✗ Main library missing: $INSTALL_PREFIX/lib/libColumbaEngine.a"
    fi

    # Check headers
    if [[ -d "$INSTALL_PREFIX/include/ColumbaEngine" ]]; then
        log_success "✓ Headers installed: $INSTALL_PREFIX/include/ColumbaEngine/"
    else
        log_error "✗ Headers missing: $INSTALL_PREFIX/include/ColumbaEngine/"
    fi

    # Check CMake config
    if [[ -f "$INSTALL_PREFIX/lib/cmake/ColumbaEngine/ColumbaEngineConfig.cmake" ]]; then
        log_success "✓ CMake config installed: $INSTALL_PREFIX/lib/cmake/ColumbaEngine/"
    else
        log_error "✗ CMake config missing: $INSTALL_PREFIX/lib/cmake/ColumbaEngine/"
        log_info "Checking what CMake files exist..."
        find "$INSTALL_PREFIX" -name "*ColumbaEngine*" -path "*/cmake/*" 2>/dev/null || log_info "No CMake files found"
    fi

    # List some dependency libraries
    log_info "Checking dependency libraries..."
    ls -la "$INSTALL_PREFIX/lib/"lib{SDL2,glew,freetype}* 2>/dev/null | head -5 || log_info "Some dependency libraries may be missing"

    log_success "ColumbaEngine installed successfully"
}

# Create test application
create_test_app() {
    log_info "Creating test application..."

    local test_dir="$INSTALL_DIR/test-app"
    mkdir -p "$test_dir/src"
    cd "$test_dir"

    # Create main.cpp
    cat > src/main.cpp << 'EOF'
#include <iostream>

#include "application.h"

int main(int argc, char *argv[])
{
    // Decouple C++ and C stream for faster runtime
    std::ios_base::sync_with_stdio(false);

    GameApp app("ColumbaEngine Test App");

    return app.exec();
}
EOF

    # Create application.h
    cat > src/application.h << 'EOF'
#ifndef APPLICATION_H
#define APPLICATION_H

#include "engine.h"

class GameApp
{
public:
    GameApp(const std::string& appName);
    ~GameApp();

    int exec();

private:
    pg::Engine engine;
};

#endif
EOF

    # Create application.cpp
    cat > src/application.cpp << 'EOF'
#include "application.h"
#include "boxbouncersystem.h"

using namespace pg;

namespace {
    static const char *const DOM = "App";
}

GameApp::GameApp(const std::string &appName) : engine(appName)
{
    engine.setSetupFunction([this](EntitySystem& ecs, Window& window)
    {
        auto config = engine.getConfig();
        ecs.createSystem<BoxBouncerSystem>(config.width, config.height);
    });
}

GameApp::~GameApp()
{
}

int GameApp::exec()
{
    return engine.exec();
}
EOF

    # Create boxbouncersystem.h
    cat > src/boxbouncersystem.h << 'EOF'
// BoxBouncerSystem.h
#pragma once

#include <random>

#include "Systems/basicsystems.h"
#include "2D/simple2dobject.h"

using namespace pg;

struct BouncingBox
{
    float velocityX;
    float velocityY;
    float speed;

    BouncingBox(float vx = 0.0f, float vy = 0.0f, float spd = 100.0f)
        : velocityX(vx), velocityY(vy), speed(spd) {}
};

class BoxBouncerSystem : public System<InitSys, Listener<TickEvent>>
{
private:
    float screenWidth;
    float screenHeight;
    std::mt19937 rng;
    std::uniform_real_distribution<float> colorDist;

    float deltaTime = 0.0f;

    EntityRef ent;

public:
    BoxBouncerSystem(float width = 820.0f, float height = 640.0f)
        : screenWidth(width), screenHeight(height), rng(std::random_device{}()), colorDist(0.0f, 255.0f) {}

    // Name of the system so it is easier to debug the taskflow
    virtual std::string getSystemName() const override { return "Box Bouncer System"; }

    void init() override
    {
        // Create a 2D square
        auto shape = makeSimple2DShape(ecsRef, Shape2D::Square,
            screenWidth / 2, screenHeight / 2,
            {255.0f, 100.0f, 100.0f, 255.0f}); // Red square

        auto pos = shape.get<PositionComponent>();

        // Set initial size
        pos->width = 120.0f;
        pos->height = 70.0f;

        // Add bouncing behavior component
        auto bouncer = shape.attachGeneric<BouncingBox>();
        bouncer->velocityX = 180.0f;  // pixels per second
        bouncer->velocityY = 180.0f;  // pixels per second
        bouncer->speed = 150.0f;

        ent = shape.entity;
    }

    virtual void onEvent(const TickEvent& event) override
    {
        deltaTime += event.tick / 1000.0f;
    }

    void execute() override
    {
        if (deltaTime == 0.0f)
            return;

        auto pos = ent->get<PositionComponent>();
        auto shape2D = ent->get<Simple2DObject>();
        auto bouncer = ent->get<BouncingBox>();

        if (not pos or not shape2D or not bouncer)
            return;

        // Update position
        auto x = pos->x + bouncer->velocityX * deltaTime;
        auto y = pos->y + bouncer->velocityY * deltaTime;

        // Check boundaries and bounce
        float width = pos->width;
        float height = pos->height;

        // Left/Right boundaries
        if (x <= 0 or x + width >= screenWidth)
        {
            bouncer->velocityX = -bouncer->velocityX;
            changeColor(shape2D);

            // Keep within bounds
            if (x <= 0)
                x = 0;
            else
                x = screenWidth - width;
        }

        // Top/Bottom boundaries
        if (y <= 0 or y + height >= screenHeight)
        {
            bouncer->velocityY = -bouncer->velocityY;
            changeColor(shape2D);

            // Keep within bounds
            if (y <= 0)
                y = 0;
            else
                y = screenHeight - height;
        }

        pos->setX(x);
        pos->setY(y);

        deltaTime = 0.0f;
    }

    void setScreenSize(float width, float height)
    {
        screenWidth = width;
        screenHeight = height;
    }

private:
    void changeColor(Simple2DObject* simple2D)
    {
        auto r = colorDist(rng), g = colorDist(rng), b = colorDist(rng);

        // Change to random color when bouncing
        simple2D->setColors({r, g, b, 255.0f});
    }
};
EOF

    # Create CMakeLists.txt
    cat > CMakeLists.txt << 'EOF'
cmake_minimum_required(VERSION 3.18)
project(ColumbaEngineTestApp VERSION 1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Try to find ColumbaEngine with multiple fallback strategies
find_package(ColumbaEngine QUIET)

if(NOT ColumbaEngine_FOUND)
    # Try with common installation paths
    set(CMAKE_PREFIX_PATH
        /usr/local/lib/cmake/ColumbaEngine
        /usr/lib/cmake/ColumbaEngine
        ~/.local/lib/cmake/ColumbaEngine
        ${CMAKE_PREFIX_PATH}
    )
    find_package(ColumbaEngine QUIET)
endif()

if(NOT ColumbaEngine_FOUND)
    # Try setting ColumbaEngine_DIR directly
    if(EXISTS "/usr/local/lib/cmake/ColumbaEngine/ColumbaEngineConfig.cmake")
        set(ColumbaEngine_DIR "/usr/local/lib/cmake/ColumbaEngine")
        find_package(ColumbaEngine REQUIRED)
    elseif(EXISTS "$ENV{HOME}/.local/lib/cmake/ColumbaEngine/ColumbaEngineConfig.cmake")
        set(ColumbaEngine_DIR "$ENV{HOME}/.local/lib/cmake/ColumbaEngine")
        find_package(ColumbaEngine REQUIRED)
    else()
        message(FATAL_ERROR "ColumbaEngine not found. Please ensure it's installed or set ColumbaEngine_DIR manually.")
    endif()
endif()

# Create executable
add_executable(ColumbaEngineTestApp
    src/main.cpp
    src/application.cpp
)

# Link with ColumbaEngine
target_link_libraries(ColumbaEngineTestApp PRIVATE ColumbaEngine::ColumbaEngine)

# Print debug info
message(STATUS "ColumbaEngine found: ${ColumbaEngine_FOUND}")
message(STATUS "ColumbaEngine include dir: ${ColumbaEngine_INCLUDE_DIR}")
message(STATUS "ColumbaEngine library: ${ColumbaEngine_LIBRARY}")
EOF

    # Create enhanced build script with debug/release options
    cat > build.sh << 'EOF'
#!/bin/bash

# ColumbaEngine Test App Build Script
# Usage: ./build.sh [debug|release] [clean]

set -e  # Exit on any error

# Default build type
BUILD_TYPE="Release"
CLEAN_BUILD=false

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Function to print colored output
print_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Parse command line arguments
for arg in "$@"; do
    case $arg in
        debug|Debug|DEBUG)
            BUILD_TYPE="Debug"
            print_info "Building in Debug mode"
            ;;
        release|Release|RELEASE)
            BUILD_TYPE="Release"
            print_info "Building in Release mode"
            ;;
        clean|Clean|CLEAN)
            CLEAN_BUILD=true
            print_info "Clean build requested"
            ;;
        help|--help|-h)
            echo "Usage: $0 [debug|release] [clean]"
            echo ""
            echo "Options:"
            echo "  debug    - Build in Debug mode"
            echo "  release  - Build in Release mode (default)"
            echo "  clean    - Clean build directory before building"
            echo "  help     - Show this help message"
            echo ""
            echo "Examples:"
            echo "  $0              # Release build"
            echo "  $0 debug        # Debug build"
            echo "  $0 release clean # Clean release build"
            exit 0
            ;;
        *)
            print_warning "Unknown argument: $arg (ignored)"
            ;;
    esac
done

print_info "Building ColumbaEngine Test App in $BUILD_TYPE mode..."

# Create build directory if it doesn't exist
mkdir -p build

# Clean build if requested
if [ "$CLEAN_BUILD" = true ]; then
    print_info "Cleaning build directory..."
    rm -rf build/*
fi

# Navigate to build directory
cd build

# Configure with CMake
print_info "Configuring CMake..."
cmake -DCMAKE_BUILD_TYPE=$BUILD_TYPE ..

# Build the project
print_info "Building project..."
make -j$(nproc)

# Check if build was successful
if [ $? -eq 0 ]; then
    print_success "Build completed successfully!"
    print_info "Executable: $(pwd)/ColumbaEngineTestApp"

    # Show build info
    echo ""
    echo "Build Information:"
    echo "  Build Type: $BUILD_TYPE"
    echo "  Build Directory: $(pwd)"

    # Offer to run the application
    echo ""
    read -p "Do you want to run the application? [y/N]: " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        print_info "Running application..."
        ./ColumbaEngineTestApp
    fi
else
    print_error "Build failed!"
    exit 1
fi
EOF
    chmod +x build.sh

    # Create README
    cat > README.md << 'EOF'
# ColumbaEngine Test Application

This is a basic test application created by the ColumbaEngine installation script.
It demonstrates a simple bouncing box that changes color when it hits the screen edges.

## Building

```bash
# Release build (default)
./build.sh

# Debug build
./build.sh debug

# Clean build
./build.sh clean

# Clean debug build
./build.sh debug clean
```

## Running

```bash
cd build
./ColumbaEngineTestApp
```

The application will create a window displaying a bouncing colored box. Press ESC or close the window to exit.

## Project Structure

- `src/main.cpp` - Entry point
- `src/application.h/cpp` - Main application class using pg::Engine
- `src/boxbouncersystem.h` - Box bouncer system demonstrating ECS usage
- `CMakeLists.txt` - CMake configuration
- `build.sh` - Build script with debug/release options

## What it demonstrates

This example shows:
- Using the `pg::Engine` class for easy setup
- Creating a custom system (`BoxBouncerSystem`) with ECS
- Using `InitSys` for initialization
- Listening to `TickEvent` for game loop updates
- Creating 2D shapes with `makeSimple2DShape`
- Attaching custom components (`BouncingBox`)
- Simple physics and collision detection with screen bounds

## Modifying

You can modify the application by editing the files in `src/`. The CMakeLists.txt is set up to automatically find and link ColumbaEngine.

Try experimenting with:
- Different initial velocities in `BoxBouncerSystem::init()`
- Multiple bouncing boxes
- Different shapes (Triangle, Circle)
- Adding gravity or acceleration
EOF

    log_success "Test application created in: $test_dir"
}

# Build and test the application
test_application() {
    log_info "Building test application..."

    cd "$INSTALL_DIR/test-app"
    ./build.sh 2>&1 | tee build.log

    # Check if build succeeded
    if [[ -f "build/ColumbaEngineTestApp" ]]; then
        log_success "Test application built successfully!"
        log_info "You can run it with: cd $INSTALL_DIR/test-app/build && ./ColumbaEngineTestApp"
    else
        log_warning "Test application build failed. Debugging..."

        # Check if ColumbaEngine config exists
        log_info "Checking ColumbaEngine installation..."
        if [[ -f "$INSTALL_PREFIX/lib/cmake/ColumbaEngine/ColumbaEngineConfig.cmake" ]]; then
            log_info "✓ ColumbaEngine CMake config found at: $INSTALL_PREFIX/lib/cmake/ColumbaEngine/"
        else
            log_error "✗ ColumbaEngine CMake config not found!"
            log_info "Looking for ColumbaEngine files..."
            find "$INSTALL_PREFIX" -name "*ColumbaEngine*" -type f 2>/dev/null | head -10 || true
        fi

        # Try building with explicit path
        log_info "Attempting build with explicit ColumbaEngine path..."
        cd build
        if cmake .. -DColumbaEngine_DIR="$INSTALL_PREFIX/lib/cmake/ColumbaEngine"; then
            log_info "CMake succeeded with explicit path"
            if make; then
                log_success "Build succeeded with explicit path!"
                return 0
            fi
        fi

        log_warning "Test application build failed, but ColumbaEngine installation completed"
        log_info "You may need to set ColumbaEngine_DIR manually when building projects"
        return 1
    fi
}

# Clean up function
cleanup() {
    if [[ -n "$1" ]]; then
        log_error "Installation failed: $1"
        exit 1
    fi
}

# Main installation function
main() {
    log_info "ColumbaEngine Installation Script"
    log_info "============================="
    log_info "Version: $ColumbaEngine_VERSION"
    log_info "Build type: $BUILD_TYPE"
    log_info "Install prefix: $INSTALL_PREFIX"
    log_info "Working directory: $INSTALL_DIR"
    echo

    # Setup error handling
    trap 'cleanup "Interrupted"' INT TERM

    # Check permissions
    check_permissions

    # Install dependencies
    install_dependencies

    # Download source
    download_ColumbaEngine

    # Build engine
    build_ColumbaEngine

    # Install engine
    install_ColumbaEngine

    # Create test app
    create_test_app

    # Test the installation
    test_application

    echo
    log_success "ColumbaEngine installation completed successfully!"
    echo
    log_info "Installation summary:"
    log_info "- ColumbaEngine built in: $BUILD_TYPE mode"
    log_info "- ColumbaEngine installed to: $INSTALL_PREFIX"
    log_info "- Test application: $INSTALL_DIR/test-app"
    log_info "- Source code: $INSTALL_DIR/ColumbaEngine"
    echo
    log_info "To run the test application:"
    log_info "  cd $INSTALL_DIR/test-app/build"
    log_info "  ./ColumbaEngineTestApp"
    echo
    log_info "To create new projects, use find_package(ColumbaEngine REQUIRED) in CMake"
}

# Show usage information
usage() {
    echo "Usage: $0 [options]"
    echo
    echo "Options:"
    echo "  -h, --help           Show this help message"
    echo "  -v, --version VER    Specify ColumbaEngine version/tag (default: main)"
    echo "  -r, --repo URL       Specify repository URL"
    echo "  -p, --prefix PATH    Install prefix (default: /usr/local)"
    echo "  -d, --dir PATH       Working directory (default: ~/ColumbaEngine-install)"
    echo "  -j, --jobs N         Number of build jobs (default: nproc)"
    echo "  --debug              Build ColumbaEngine in Debug mode"
    echo "  --release            Build ColumbaEngine in Release mode (default)"
    echo
    echo "Environment variables:"
    echo "  ColumbaEngine_VERSION     Engine version to install"
    echo "  ColumbaEngine_REPO        Repository URL"
    echo "  INSTALL_PREFIX       Installation prefix"
    echo "  BUILD_TYPE           Build type (Debug or Release)"
    echo
    echo "Examples:"
    echo "  $0                                    # Install latest release build"
    echo "  $0 --debug                           # Install latest debug build"
    echo "  $0 -v v1.0.0 --release              # Install specific version in release mode"
    echo "  $0 -p ~/.local --debug               # Install debug build to user directory"
    echo "  $0 -r https://github.com/user/repo   # Use different repository"
}

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -h|--help)
            usage
            exit 0
            ;;
        -v|--version)
            ColumbaEngine_VERSION="$2"
            shift 2
            ;;
        -r|--repo)
            ColumbaEngine_REPO="$2"
            shift 2
            ;;
        -p|--prefix)
            INSTALL_PREFIX="$2"
            shift 2
            ;;
        -d|--dir)
            INSTALL_DIR="$2"
            shift 2
            ;;
        -j|--jobs)
            BUILD_JOBS="$2"
            shift 2
            ;;
        --debug)
            BUILD_TYPE="Debug"
            shift
            ;;
        --release)
            BUILD_TYPE="Release"
            shift
            ;;
        *)
            log_error "Unknown option: $1"
            usage
            exit 1
            ;;
    esac
done

# Run main installation
main