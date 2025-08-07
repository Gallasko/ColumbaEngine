#!/bin/bash

# PgEngine Installation Script
# Downloads, builds, and installs PgEngine with dependencies
# Creates a basic test application

set -e  # Exit on any error

# Configuration
PGENGINE_VERSION="${PGENGINE_VERSION:-main}"  # Can be set via environment variable
PGENGINE_REPO="${PGENGINE_REPO:-https://github.com/Gallasko/PgEngine.git}"  # Update this!
INSTALL_PREFIX="${INSTALL_PREFIX:-/usr/local}"
INSTALL_DIR="${INSTALL_DIR:-$HOME/pgengine-install}"
BUILD_JOBS="${BUILD_JOBS:-$(nproc)}"

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

# Download PgEngine source
download_pgengine() {
    log_info "Downloading PgEngine version: $PGENGINE_VERSION"
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
    
    git clone --recursive "$PGENGINE_REPO" pgengine
    cd pgengine
    
    if [[ "$PGENGINE_VERSION" != "main" ]]; then
        git checkout "$PGENGINE_VERSION"
    fi
    
    log_success "PgEngine source downloaded to: $INSTALL_DIR/pgengine"
}

# Build PgEngine
build_pgengine() {
    log_info "Building PgEngine..."
    
    cd "$INSTALL_DIR/pgengine"
    mkdir -p build
    cd build
    
    # Use Unix Makefiles instead of Ninja to avoid dependency issues
    cmake .. \
        -G "Unix Makefiles" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX="$INSTALL_PREFIX" \
        -DBUILD_EXAMPLES=OFF \
        -DBUILD_STATIC_LIB=ON
    
    make -j"$BUILD_JOBS"
    
    log_success "PgEngine built successfully"
}

# Install PgEngine
install_pgengine() {
    log_info "Installing PgEngine to $INSTALL_PREFIX..."
    
    cd "$INSTALL_DIR/pgengine/build"
    
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
    if [[ -f "$INSTALL_PREFIX/lib/libPgEngine.a" ]]; then
        log_success "✓ Main library installed: $INSTALL_PREFIX/lib/libPgEngine.a"
    else
        log_error "✗ Main library missing: $INSTALL_PREFIX/lib/libPgEngine.a"
    fi
    
    # Check headers
    if [[ -d "$INSTALL_PREFIX/include/PgEngine" ]]; then
        log_success "✓ Headers installed: $INSTALL_PREFIX/include/PgEngine/"
    else
        log_error "✗ Headers missing: $INSTALL_PREFIX/include/PgEngine/"
    fi
    
    # Check CMake config
    if [[ -f "$INSTALL_PREFIX/lib/cmake/PgEngine/PgEngineConfig.cmake" ]]; then
        log_success "✓ CMake config installed: $INSTALL_PREFIX/lib/cmake/PgEngine/"
    else
        log_error "✗ CMake config missing: $INSTALL_PREFIX/lib/cmake/PgEngine/"
        log_info "Checking what CMake files exist..."
        find "$INSTALL_PREFIX" -name "*PgEngine*" -path "*/cmake/*" 2>/dev/null || log_info "No CMake files found"
    fi
    
    # List some dependency libraries
    log_info "Checking dependency libraries..."
    ls -la "$INSTALL_PREFIX/lib/"lib{SDL2,glew,freetype}* 2>/dev/null | head -5 || log_info "Some dependency libraries may be missing"
    
    log_success "PgEngine installed successfully"
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
    
    GameApp app("PgEngine Test App");
    
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

    SDL_Window *pWindow = SDL_CreateWindow("PgEngine Test App",
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
project(PgEngineTestApp VERSION 1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Try to find PgEngine with multiple fallback strategies
find_package(PgEngine QUIET)

if(NOT PgEngine_FOUND)
    # Try with common installation paths
    set(CMAKE_PREFIX_PATH 
        /usr/local/lib/cmake/PgEngine
        /usr/lib/cmake/PgEngine
        ~/.local/lib/cmake/PgEngine
        ${CMAKE_PREFIX_PATH}
    )
    find_package(PgEngine QUIET)
endif()

if(NOT PgEngine_FOUND)
    # Try setting PgEngine_DIR directly
    if(EXISTS "/usr/local/lib/cmake/PgEngine/PgEngineConfig.cmake")
        set(PgEngine_DIR "/usr/local/lib/cmake/PgEngine")
        find_package(PgEngine REQUIRED)
    elseif(EXISTS "$ENV{HOME}/.local/lib/cmake/PgEngine/PgEngineConfig.cmake")
        set(PgEngine_DIR "$ENV{HOME}/.local/lib/cmake/PgEngine")
        find_package(PgEngine REQUIRED)
    else()
        message(FATAL_ERROR "PgEngine not found. Please ensure it's installed or set PgEngine_DIR manually.")
    endif()
endif()

# Create executable
add_executable(PgEngineTestApp
    src/main.cpp
    src/application.cpp
)

# Link with PgEngine
target_link_libraries(PgEngineTestApp PRIVATE PgEngine::PgEngine)

# Print debug info
message(STATUS "PgEngine found: ${PgEngine_FOUND}")
message(STATUS "PgEngine include dir: ${PgEngine_INCLUDE_DIR}")
message(STATUS "PgEngine library: ${PgEngine_LIBRARY}")
EOF

    # Create build script
    cat > build.sh << 'EOF'
#!/bin/bash

set -e

echo "Building PgEngine Test App..."

mkdir -p build
cd build

cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

echo "Build complete! Run with: ./PgEngineTestApp"
EOF
    chmod +x build.sh

    # Create README
    cat > README.md << 'EOF'
# PgEngine Test Application

This is a basic test application created by the PgEngine installation script.

## Building

```bash
./build.sh
```

## Running

```bash
cd build
./PgEngineTestApp
```

The application will create a window using PgEngine. Press ESC or close the window to exit.

## Project Structure

- `src/main.cpp` - Entry point
- `src/application.h/cpp` - Main application class
- `CMakeLists.txt` - CMake configuration
- `build.sh` - Build script

## Modifying

You can modify the application by editing the files in `src/`. The CMakeLists.txt is set up to automatically find and link PgEngine.
EOF

    log_success "Test application created in: $test_dir"
}

# Build and test the application
test_application() {
    log_info "Building test application..."
    
    cd "$INSTALL_DIR/test-app"
    ./build.sh 2>&1 | tee build.log
    
    # Check if build succeeded
    if [[ -f "build/PgEngineTestApp" ]]; then
        log_success "Test application built successfully!"
        log_info "You can run it with: cd $INSTALL_DIR/test-app/build && ./PgEngineTestApp"
    else
        log_warning "Test application build failed. Debugging..."
        
        # Check if PgEngine config exists
        log_info "Checking PgEngine installation..."
        if [[ -f "$INSTALL_PREFIX/lib/cmake/PgEngine/PgEngineConfig.cmake" ]]; then
            log_info "✓ PgEngine CMake config found at: $INSTALL_PREFIX/lib/cmake/PgEngine/"
        else
            log_error "✗ PgEngine CMake config not found!"
            log_info "Looking for PgEngine files..."
            find "$INSTALL_PREFIX" -name "*PgEngine*" -type f 2>/dev/null | head -10 || true
        fi
        
        # Try building with explicit path
        log_info "Attempting build with explicit PgEngine path..."
        cd build
        if cmake .. -DPgEngine_DIR="$INSTALL_PREFIX/lib/cmake/PgEngine"; then
            log_info "CMake succeeded with explicit path"
            if make; then
                log_success "Build succeeded with explicit path!"
                return 0
            fi
        fi
        
        log_warning "Test application build failed, but PgEngine installation completed"
        log_info "You may need to set PgEngine_DIR manually when building projects"
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
    log_info "PgEngine Installation Script"
    log_info "============================="
    log_info "Version: $PGENGINE_VERSION"
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
    download_pgengine

    # Build engine
    build_pgengine

    # Install engine
    install_pgengine

    # Create test app
    create_test_app

    # Test the installation
    test_application

    echo
    log_success "PgEngine installation completed successfully!"
    echo
    log_info "Installation summary:"
    log_info "- PgEngine installed to: $INSTALL_PREFIX"
    log_info "- Test application: $INSTALL_DIR/test-app"
    log_info "- Source code: $INSTALL_DIR/pgengine"
    echo
    log_info "To run the test application:"
    log_info "  cd $INSTALL_DIR/test-app/build"
    log_info "  ./PgEngineTestApp"
    echo
    log_info "To create new projects, use find_package(PgEngine REQUIRED) in CMake"
}

# Show usage information
usage() {
    echo "Usage: $0 [options]"
    echo
    echo "Options:"
    echo "  -h, --help           Show this help message"
    echo "  -v, --version VER    Specify PgEngine version/tag (default: main)"
    echo "  -r, --repo URL       Specify repository URL"
    echo "  -p, --prefix PATH    Install prefix (default: /usr/local)"
    echo "  -d, --dir PATH       Working directory (default: ~/pgengine-install)"
    echo "  -j, --jobs N         Number of build jobs (default: nproc)"
    echo
    echo "Environment variables:"
    echo "  PGENGINE_VERSION     Engine version to install"
    echo "  PGENGINE_REPO        Repository URL"
    echo "  INSTALL_PREFIX       Installation prefix"
    echo
    echo "Examples:"
    echo "  $0                                    # Install latest from main branch"
    echo "  $0 -v v1.0.0                        # Install specific version"
    echo "  $0 -p ~/.local                       # Install to user directory"
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
            PGENGINE_VERSION="$2"
            shift 2
            ;;
        -r|--repo)
            PGENGINE_REPO="$2"
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
        *)
            log_error "Unknown option: $1"
            usage
            exit 1
            ;;
    esac
done

# Run main installation
main