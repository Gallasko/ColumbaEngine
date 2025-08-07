#!/bin/bash

# Emscripten Installation Script for PgEngine
# Installs Emscripten SDK and builds PgEngine for web

set -e

# Configuration
EMSDK_DIR="${EMSDK_DIR:-$HOME/emsdk}"
PGENGINE_DIR="${1:-./pgengine}"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

log_info() { echo -e "${BLUE}[INFO]${NC} $1"; }
log_success() { echo -e "${GREEN}[SUCCESS]${NC} $1"; }
log_warning() { echo -e "${YELLOW}[WARNING]${NC} $1"; }
log_error() { echo -e "${RED}[ERROR]${NC} $1"; }

install_emscripten() {
    log_info "Installing Emscripten SDK..."

    if [[ -d "$EMSDK_DIR" ]]; then
        log_warning "Emscripten SDK already exists at $EMSDK_DIR"
        read -p "Update it? (y/N): " -n 1 -r
        echo
        if [[ ! $REPLY =~ ^[Yy]$ ]]; then
            log_info "Using existing Emscripten installation"
            cd "$EMSDK_DIR"
            source ./emsdk_env.sh
            return
        fi
    else
        mkdir -p "$(dirname "$EMSDK_DIR")"
    fi

    # Install dependencies
    if command -v apt &> /dev/null; then
        sudo apt update
        sudo apt install -y python3 nodejs npm git
    elif command -v dnf &> /dev/null; then
        sudo dnf install -y python3 nodejs npm git
    elif command -v pacman &> /dev/null; then
        sudo pacman -S --needed --noconfirm python nodejs npm git
    fi

    # Download and install Emscripten
    if [[ ! -d "$EMSDK_DIR" ]]; then
        git clone https://github.com/emscripten-core/emsdk.git "$EMSDK_DIR"
    fi

    cd "$EMSDK_DIR"
    ./emsdk install latest
    ./emsdk activate latest
    source ./emsdk_env.sh

    log_success "Emscripten SDK installed"
}

build_pgengine_web() {
    log_info "Building PgEngine for web..."

    if [[ ! -d "$PGENGINE_DIR" ]]; then
        log_error "PgEngine directory not found: $PGENGINE_DIR"
        log_info "Please run the main install script first or specify PgEngine directory"
        exit 1
    fi

    cd "$PGENGINE_DIR"

    mkdir -p build-emscripten
    cd build-emscripten

    emcmake cmake .. \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_EXAMPLES=OFF \
        -DBUILD_STATIC_LIB=ON

    emmake make -j$(nproc)

    log_success "PgEngine built for web"
}

create_web_test() {
    log_info "Creating web test application..."

    local test_dir="$PGENGINE_DIR/web-test"
    mkdir -p "$test_dir/src"
    cd "$test_dir"

    # Create CMakeLists.txt for web
    cat > CMakeLists.txt << 'EOF'
cmake_minimum_required(VERSION 3.18)
project(PgEngineWebTest VERSION 1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Emscripten-specific settings
if(${CMAKE_SYSTEM_NAME} MATCHES "Emscripten")
    set(USE_FLAGS "-O3 -sUSE_SDL=2 -sUSE_SDL_MIXER=2 -sUSE_SDL_NET=2 -sUSE_FREETYPE=1 -fwasm-exceptions -sUSE_PTHREADS=1")

    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${USE_FLAGS}")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${USE_FLAGS}")

    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${USE_FLAGS} \
        -lidbfs.js -s FORCE_FILESYSTEM=1 -sPTHREAD_POOL_SIZE=4 -pthread \
        -s ALLOW_MEMORY_GROWTH=1 -s USE_WEBGL2=1 -s FULL_ES3=1")

    set(CMAKE_EXECUTABLE_SUFFIX .html)

    # Build PgEngine as subdirectory for Emscripten
    add_subdirectory(../build-emscripten pgengine_web)
    set(PGENGINE_TARGET PgEngine)
else()
    find_package(PgEngine REQUIRED)
    set(PGENGINE_TARGET PgEngine::PgEngine)
endif()

# Create executable
add_executable(PgEngineWebTest
    src/main.cpp
    src/application.cpp
)

# Link with PgEngine
target_link_libraries(PgEngineWebTest PRIVATE ${PGENGINE_TARGET})

# Emscripten-specific settings
if(${CMAKE_SYSTEM_NAME} MATCHES "Emscripten")
    set_target_properties(PgEngineWebTest PROPERTIES
        SUFFIX ".html"
        LINK_FLAGS "${CMAKE_EXE_LINKER_FLAGS}"
    )
endif()
EOF

    # Copy source files from the main test app if they exist
    if [[ -f "../test-app/src/main.cpp" ]]; then
        cp ../test-app/src/* src/
    else
        # Create minimal web test
        cat > src/main.cpp << 'EOF'
#include <iostream>
#include <emscripten.h>
#include <SDL.h>

int main() {
    std::cout << "PgEngine Web Test - Hello World!" << std::endl;

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cout << "SDL_Init failed: " << SDL_GetError() << std::endl;
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow(
        "PgEngine Web Test",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        800, 600,
        SDL_WINDOW_SHOWN
    );

    if (!window) {
        std::cout << "Window creation failed: " << SDL_GetError() << std::endl;
        return 1;
    }

    std::cout << "Web test initialized successfully!" << std::endl;

    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
EOF

        touch src/application.cpp src/application.h
    fi

    # Create build script
    cat > build-web.sh << 'EOF'
#!/bin/bash

set -e

# Source Emscripten environment
if [[ -f "$HOME/emsdk/emsdk_env.sh" ]]; then
    source "$HOME/emsdk/emsdk_env.sh"
else
    echo "Emscripten not found. Please install it first."
    exit 1
fi

echo "Building PgEngine Web Test..."

mkdir -p build-web
cd build-web

emcmake cmake .. -DCMAKE_BUILD_TYPE=Release
emmake make -j$(nproc)

echo "Build complete!"
echo "Files generated:"
ls -la PgEngineWebTest.*

echo ""
echo "To run the web application:"
echo "1. Start a web server: python3 -m http.server 8000"
echo "2. Open http://localhost:8000/PgEngineWebTest.html"
EOF
    chmod +x build-web.sh

    log_success "Web test application created"
}

setup_shell_profile() {
    log_info "Setting up shell profile for Emscripten..."

    local shell_rc
    if [[ -n "$ZSH_VERSION" ]]; then
        shell_rc="$HOME/.zshrc"
    else
        shell_rc="$HOME/.bashrc"
    fi

    local emsdk_line="source \"$EMSDK_DIR/emsdk_env.sh\" 2>/dev/null || true"

    if ! grep -q "emsdk_env.sh" "$shell_rc" 2>/dev/null; then
        echo "" >> "$shell_rc"
        echo "# Emscripten SDK" >> "$shell_rc"
        echo "$emsdk_line" >> "$shell_rc"
        log_success "Added Emscripten to $shell_rc"
    else
        log_info "Emscripten already configured in $shell_rc"
    fi
}

main() {
    log_info "PgEngine Emscripten Setup"
    log_info "========================="
    log_info "Emscripten SDK directory: $EMSDK_DIR"
    log_info "PgEngine directory: $PGENGINE_DIR"
    echo

    install_emscripten
    build_pgengine_web
    create_web_test
    setup_shell_profile

    echo
    log_success "Emscripten setup completed successfully!"
    echo
    log_info "To build the web test:"
    log_info "  cd $PGENGINE_DIR/web-test"
    log_info "  ./build-web.sh"
    echo
    log_info "To run web applications, use: python3 -m http.server 8000"
    log_info "Then open http://localhost:8000/filename.html in your browser"
}

usage() {
    echo "Usage: $0 [pgengine_directory]"
    echo
    echo "Install Emscripten SDK and build PgEngine for web"
    echo
    echo "Arguments:"
    echo "  pgengine_directory   Path to PgEngine source (default: ./pgengine)"
    echo
    echo "Environment variables:"
    echo "  EMSDK_DIR           Emscripten SDK directory (default: ~/emsdk)"
}

if [[ "$1" == "-h" || "$1" == "--help" ]]; then
    usage
    exit 0
fi

main "$@"