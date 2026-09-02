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
else
    print_error "Build failed!"
    exit 1
fi
