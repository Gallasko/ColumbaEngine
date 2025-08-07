#!/bin/bash

# PgEngine Installation Validator
# Checks if PgEngine is properly installed and working

set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

log_info() { echo -e "${BLUE}[INFO]${NC} $1"; }
log_success() { echo -e "${GREEN}[✓]${NC} $1"; }
log_warning() { echo -e "${YELLOW}[⚠]${NC} $1"; }
log_error() { echo -e "${RED}[✗]${NC} $1"; }

# Configuration
INSTALL_PREFIX="${INSTALL_PREFIX:-/usr/local}"
TEST_DIR="/tmp/pgengine-validation-$$"
ERRORS=0

# Increment error counter
error() {
    log_error "$1"
    ((ERRORS++))
}

# Test functions
check_system_dependencies() {
    log_info "Checking system dependencies..."

    local deps=("cmake" "make" "g++" "git")
    for dep in "${deps[@]}"; do
        if command -v "$dep" &> /dev/null; then
            log_success "$dep found"
        else
            error "$dep not found"
        fi
    done

    # Check CMake version
    if command -v cmake &> /dev/null; then
        local cmake_version=$(cmake --version | head -n1 | grep -oE '[0-9]+\.[0-9]+')
        local major=$(echo $cmake_version | cut -d. -f1)
        local minor=$(echo $cmake_version | cut -d. -f2)

        if [[ $major -gt 3 ]] || [[ $major -eq 3 && $minor -ge 18 ]]; then
            log_success "CMake version $cmake_version (>= 3.18 required)"
        else
            error "CMake version $cmake_version is too old (>= 3.18 required)"
        fi
    fi
}

check_pgengine_files() {
    log_info "Checking PgEngine installation files..."

    # Check library
    if [[ -f "$INSTALL_PREFIX/lib/libPgEngine.a" ]]; then
        log_success "PgEngine library found"
    else
        error "PgEngine library not found at $INSTALL_PREFIX/lib/libPgEngine.a"
    fi

    # Check headers
    if [[ -d "$INSTALL_PREFIX/include/PgEngine" ]]; then
        log_success "PgEngine headers found"

        # Check for key headers
        local headers=("window.h" "logger.h" "ECS/entitysystem.h")
        for header in "${headers[@]}"; do
            if [[ -f "$INSTALL_PREFIX/include/PgEngine/$header" ]]; then
                log_success "Header $header found"
            else
                error "Header $header not found"
            fi
        done
    else
        error "PgEngine headers not found at $INSTALL_PREFIX/include/PgEngine"
    fi

    # Check CMake config
    if [[ -f "$INSTALL_PREFIX/lib/cmake/PgEngine/PgEngineConfig.cmake" ]]; then
        log_success "CMake configuration found"
    else
        error "CMake configuration not found"
    fi

    # Check bundled dependencies
    local deps=("libSDL2.a" "libglew.a" "libfreetype.a")
    for dep in "${deps[@]}"; do
        if [[ -f "$INSTALL_PREFIX/lib/$dep" ]]; then
            log_success "Bundled dependency $dep found"
        else
            log_warning "Bundled dependency $dep not found (might be named differently)"
        fi
    done
}

check_cmake_integration() {
    log_info "Testing CMake integration..."

    mkdir -p "$TEST_DIR"
    cd "$TEST_DIR"

    # Create minimal test project
    cat > CMakeLists.txt << 'EOF'
cmake_minimum_required(VERSION 3.18)
project(PgEngineValidationTest)

set(CMAKE_CXX_STANDARD 17)

find_package(PgEngine REQUIRED)

add_executable(ValidationTest main.cpp)
target_link_libraries(ValidationTest PRIVATE PgEngine::PgEngine)
EOF

    cat > main.cpp << 'EOF'
#include <iostream>

// Test basic PgEngine includes
#include "stdafx.h"
#include "logger.h"
#include "configuration.h"

int main() {
    std::cout << "PgEngine validation test" << std::endl;

    try {
        // Test basic functionality
        pg::Logger logger;
        pg::ConfigurationManager config;

        std::cout << "✓ Basic PgEngine classes instantiated successfully" << std::endl;
        std::cout << "✓ Headers included without errors" << std::endl;
        std::cout << "✓ Libraries linked successfully" << std::endl;

        return 0;
    } catch (const std::exception& e) {
        std::cout << "✗ Runtime error: " << e.what() << std::endl;
        return 1;
    }
}
EOF

    # Test CMake configuration
    if cmake . -DCMAKE_BUILD_TYPE=Release &> cmake.log; then
        log_success "CMake configuration successful"
    else
        error "CMake configuration failed"
        log_info "CMake log:"
        cat cmake.log | tail -10
        return
    fi

    # Test compilation
    if make &> build.log; then
        log_success "Compilation successful"
    else
        error "Compilation failed"
        log_info "Build log:"
        cat build.log | tail -10
        return
    fi

    # Test execution
    if ./ValidationTest &> run.log; then
        log_success "Test execution successful"
        log_info "Test output:"
        cat run.log
    else
        error "Test execution failed"
        log_info "Runtime log:"
        cat run.log
    fi
}

check_emscripten_support() {
    log_info "Checking Emscripten support..."

    if command -v emcc &> /dev/null; then
        log_success "Emscripten compiler found"

        local em_version=$(emcc --version | head -n1)
        log_info "Emscripten version: $em_version"

        if command -v emcmake &> /dev/null && command -v emmake &> /dev/null; then
            log_success "Emscripten CMake wrappers found"
        else
            log_warning "Emscripten CMake wrappers not found"
        fi
    else
        log_warning "Emscripten not found (optional for web builds)"
    fi
}

check_runtime_dependencies() {
    log_info "Checking runtime dependencies..."

    # Check for OpenGL
    if command -v glxinfo &> /dev/null; then
        if glxinfo | grep -q "OpenGL"; then
            log_success "OpenGL support detected"
        else
            log_warning "OpenGL support unclear"
        fi
    else
        log_warning "glxinfo not available (install mesa-utils to check OpenGL)"
    fi

    # Check for X11 (Linux)
    if [[ -n "$DISPLAY" ]]; then
        log_success "X11 display available"
    else
        log_warning "No X11 display (headless system?)"
    fi

    # Check audio
    if command -v aplay &> /dev/null || command -v pactl &> /dev/null; then
        log_success "Audio system detected"
    else
        log_warning "Audio system unclear"
    fi
}

show_system_info() {
    log_info "System information:"
    echo "  OS: $(uname -s) $(uname -r)"
    echo "  Architecture: $(uname -m)"
    echo "  Install prefix: $INSTALL_PREFIX"

    if [[ -f /etc/os-release ]]; then
        . /etc/os-release
        echo "  Distribution: $NAME $VERSION"
    fi

    echo "  CMake: $(cmake --version | head -n1 | grep -oE '[0-9]+\.[0-9]+\.[0-9]+')"
    echo "  GCC: $(gcc --version | head -n1 | grep -oE '[0-9]+\.[0-9]+\.[0-9]+')"
}

cleanup() {
    if [[ -d "$TEST_DIR" ]]; then
        rm -rf "$TEST_DIR"
    fi
}

main() {
    echo
    log_info "PgEngine Installation Validator"
    log_info "==============================="
    echo

    show_system_info
    echo

    trap cleanup EXIT

    check_system_dependencies
    echo

    check_pgengine_files
    echo

    check_cmake_integration
    echo

    check_emscripten_support
    echo

    check_runtime_dependencies
    echo

    # Summary
    if [[ $ERRORS -eq 0 ]]; then
        log_success "Validation completed successfully! PgEngine is ready to use."
        echo
        log_info "Next steps:"
        log_info "1. Create a new project directory"
        log_info "2. Add find_package(PgEngine REQUIRED) to your CMakeLists.txt"
        log_info "3. Link with target_link_libraries(your_target PRIVATE PgEngine::PgEngine)"
        log_info "4. Include PgEngine headers and start coding!"
        echo
        log_info "Example project template:"
        log_info "  mkdir my-game && cd my-game"
        log_info "  # Create CMakeLists.txt and source files"
        log_info "  mkdir build && cd build"
        log_info "  cmake .. && make"
        exit 0
    else
        log_error "Validation failed with $ERRORS errors"
        echo
        log_info "Common solutions:"
        log_info "- Re-run the installation script"
        log_info "- Check that INSTALL_PREFIX is correct"
        log_info "- Ensure you have proper permissions"
        log_info "- Install missing system dependencies manually"
        echo
        log_info "For help, check the installation documentation or create an issue"
        exit 1
    fi
}

usage() {
    echo "Usage: $0 [options]"
    echo
    echo "Validate PgEngine installation and test basic functionality"
    echo
    echo "Options:"
    echo "  -h, --help           Show this help"
    echo "  -p, --prefix PATH    Installation prefix to check (default: /usr/local)"
    echo
    echo "Environment variables:"
    echo "  INSTALL_PREFIX       Installation prefix to check"
}

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -h|--help)
            usage
            exit 0
            ;;
        -p|--prefix)
            INSTALL_PREFIX="$2"
            shift 2
            ;;
        *)
            log_error "Unknown option: $1"
            usage
            exit 1
            ;;
    esac
done

main