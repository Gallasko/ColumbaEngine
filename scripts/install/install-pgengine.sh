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
#include <stdio.h>
#include <stdint.h>
#include <assert.h>

#include "application.h"
#include "logger.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <SDL2/SDL.h>
#include <SDL_opengles2.h>
#include <GLES2/gl2.h>
#else
#ifdef __linux__
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#elif _WIN32
#include <SDL.h>
#include <SDL_opengl.h>
#endif
#include <GL/gl.h>
#endif

int main(int argc, char *argv[])
{
    // Decouple C++ and C stream for faster runtime
    std::ios_base::sync_with_stdio(false);

#ifdef __EMSCRIPTEN__
    printf("Starting program...\n");
#endif

    GameApp app("ColumbaEngine Test App");

    return app.exec();
}
EOF

    # Create application.h
    cat > src/application.h << 'EOF'
#ifndef APPLICATION_H
#define APPLICATION_H

#include "window.h"

class GameApp
{
public:
    GameApp(const std::string& appName);
    ~GameApp();

    int exec();

private:
    std::string appName;
};

#endif
EOF

    # Create application.cpp
    cat > src/application.cpp << 'EOF'
#include "application.h"
#include "logger.h"

using namespace pg;

namespace {
    static const char *const DOM = "App";
}

GameApp::GameApp(const std::string &appName) : appName(appName) {
    LOG_THIS_MEMBER(DOM);
}

GameApp::~GameApp() {
    LOG_THIS_MEMBER(DOM);
}

std::thread *initThread;
pg::Window *mainWindow = nullptr;
std::atomic<bool> initialized = {false};
bool init = false;
bool running = true;

void initWindow(const std::string &appName) {
#ifdef __EMSCRIPTEN__
    mainWindow = new pg::Window(appName, "/save/savedData.sz");
#else
    mainWindow = new pg::Window(appName);
#endif

    LOG_INFO(DOM, "Window init...");
    initialized = true;
}

void initGame() {
    printf("Initializing engine ...\n");

#ifdef __EMSCRIPTEN__
    EM_ASM(
        console.error("Syncing... !");
        FS.mkdir('/save');
        console.error("Syncing... !");
        FS.mount(IDBFS, {autoPersist: true}, '/save');
        console.error("Syncing... !");
        FS.syncfs(true, function (err) {
            console.error("Synced !");
            if (err) {
                console.error("Initial sync error:", err);
            }
        });
        console.error("Syncing... !");
    );
#endif

    mainWindow->initEngine();
    printf("Engine initialized ...\n");

    mainWindow->ecs.dumbTaskflow();
    mainWindow->render();
    mainWindow->resize(820, 640);
    mainWindow->ecs.start();

    printf("Engine initialized\n");
}

void syncFilesystem() {
#ifdef __EMSCRIPTEN__
    EM_ASM(
        FS.syncfs(false, function (err) {
            if (err) {
                console.error("Sync error:", err);
            } else {
                console.log("Filesystem synced.");
            }
        });
    );
#endif
}

void mainloop(void *arg) {
    if (not initialized.load())
        return;

    if (not init) {
        if (initThread) {
            printf("Joining thread...\n");
            initThread->join();
            delete initThread;
            printf("Thread joined...\n");
        }

        init = true;
        mainWindow->init(820, 640, false, static_cast<SDL_Window *>(arg));
        printf("Window init done !\n");
        initGame();
    }

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        mainWindow->processEvents(event);
    }

    mainWindow->render();

#ifdef __EMSCRIPTEN__
    if (event.type == SDL_QUIT) {
        syncFilesystem();
    }
#endif

    if (mainWindow->requestQuit()) {
        LOG_ERROR("Window", "RequestQuit");
        std::terminate();
    }
}

int GameApp::exec() {
#ifdef __EMSCRIPTEN__
    printf("Start init thread...\n");
    initThread = new std::thread(initWindow, appName);
    printf("Detach init thread...\n");

    SDL_Window *pWindow = SDL_CreateWindow("ColumbaEngine Test App",
                        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                        820, 640,
                        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_SHOWN);

    emscripten_set_main_loop_arg(mainloop, pWindow, 0, 1);
#else
    LOG_THIS_MEMBER(DOM);

    initWindow(appName);
    mainWindow->init(820, 640, false);
    LOG_INFO(DOM, "Window init done !");
    initGame();

    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            mainWindow->processEvents(event);
        }

        mainWindow->render();

        if (mainWindow->requestQuit())
            break;
    }

    delete mainWindow;
#endif

    return 0;
}
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

The application will create a window using ColumbaEngine. Press ESC or close the window to exit.

## Project Structure

- `src/main.cpp` - Entry point
- `src/application.h/cpp` - Main application class
- `CMakeLists.txt` - CMake configuration
- `build.sh` - Build script with debug/release options

## Modifying

You can modify the application by editing the files in `src/`. The CMakeLists.txt is set up to automatically find and link ColumbaEngine.
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