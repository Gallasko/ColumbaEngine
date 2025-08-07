#!/bin/bash

# Docker Test Runner for PgEngine Installation Scripts
# Builds and runs tests in fresh Docker containers

set -e

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

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SCRIPTS_TO_TEST=("install-pgengine.sh" "install-emscripten.sh" "validate-installation.sh")
DISTROS=("ubuntu" "fedora" "arch")
TEST_TIMEOUT=3600  # 1 hour timeout
CLEANUP_CONTAINERS=true
PGENGINE_REPO="${PGENGINE_REPO:-https://github.com/Gallasko/PgEngine.git}"

# Parse command line options
while [[ $# -gt 0 ]]; do
    case $1 in
        --no-cleanup)
            CLEANUP_CONTAINERS=false
            shift
            ;;
        --timeout)
            TEST_TIMEOUT="$2"
            shift 2
            ;;
        --distro)
            DISTROS=("$2")
            shift 2
            ;;
        --repo)
            PGENGINE_REPO="$2"
            shift 2
            ;;
        -h|--help)
            echo "Usage: $0 [options]"
            echo "Options:"
            echo "  --no-cleanup      Don't remove containers after testing"
            echo "  --timeout SECS    Set test timeout (default: 3600)"
            echo "  --distro DISTRO   Test only specific distro (ubuntu/fedora/arch)"
            echo "  --repo URL        PgEngine repository URL"
            echo "  -h, --help        Show this help"
            exit 0
            ;;
        *)
            log_error "Unknown option: $1"
            exit 1
            ;;
    esac
done

# Check if required files exist
check_prerequisites() {
    log_info "Checking prerequisites..."

    # Check Docker
    if ! command -v docker &> /dev/null; then
        log_error "Docker is not installed or not in PATH"
        exit 1
    fi

    # Check if Docker is running
    if ! docker info &> /dev/null; then
        log_error "Docker daemon is not running"
        exit 1
    fi

    # Check for required scripts
    local missing_scripts=()
    for script in "${SCRIPTS_TO_TEST[@]}"; do
        if [[ ! -f "$SCRIPT_DIR/$script" ]]; then
            missing_scripts+=("$script")
        fi
    done

    if [[ ${#missing_scripts[@]} -gt 0 ]]; then
        log_error "Missing required scripts: ${missing_scripts[*]}"
        log_info "Please ensure all installation scripts are in the same directory as this test script"
        exit 1
    fi

    # Check for Dockerfiles
    for distro in "${DISTROS[@]}"; do
        if [[ ! -f "$SCRIPT_DIR/test-$distro.Dockerfile" ]]; then
            log_error "Missing Dockerfile: test-$distro.Dockerfile"
            exit 1
        fi
    done

    log_success "Prerequisites check passed"
}

# Build Docker image for testing
build_test_image() {
    local distro="$1"
    local image_name="pgengine-test-$distro"

    log_info "Building Docker image for $distro..."

    if docker build -t "$image_name" -f "$SCRIPT_DIR/test-$distro.Dockerfile" "$SCRIPT_DIR"; then
        log_success "Built Docker image: $image_name"
        return 0
    else
        log_error "Failed to build Docker image for $distro"
        return 1
    fi
}

# Run installation test in container
run_installation_test() {
    local distro="$1"
    local image_name="pgengine-test-$distro"
    local container_name="pgengine-test-$distro-$$"

    log_info "Running installation test on $distro..."

    # Create container with scripts mounted
    local docker_run_cmd=(
        docker run
        --name "$container_name"
        --rm
        -v "$SCRIPT_DIR:/home/tester/scripts:ro"
        -e "PGENGINE_REPO=$PGENGINE_REPO"
        -e "BUILD_JOBS=2"
        -e "INSTALL_PREFIX=/usr/local"
        "$image_name"
    )

    # Test script
    local test_script='
set -e
export PATH="/home/tester/scripts:$PATH"

echo "=== Starting PgEngine Installation Test ==="
echo "Distribution: '$distro'"
echo "Repository: $PGENGINE_REPO"
echo "Build Jobs: $BUILD_JOBS"
echo

# Make scripts executable
chmod +x /home/tester/scripts/*.sh

echo "=== Phase 1: Installing PgEngine ==="
timeout 2700 bash /home/tester/scripts/install-pgengine.sh -v main -j $BUILD_JOBS || {
    echo "Installation failed or timed out"
    exit 1
}

echo
echo "=== Phase 2: Validating Installation ==="
timeout 300 bash /home/tester/scripts/validate-installation.sh || {
    echo "Validation failed"
    exit 1
}

echo
echo "=== Phase 3: Testing Emscripten Support (Optional) ==="
if timeout 1800 bash /home/tester/scripts/install-emscripten.sh ~/pgengine-install/pgengine 2>&1; then
    echo "Emscripten installation successful"
else
    echo "Emscripten installation failed (this is optional)"
fi

echo
echo "=== Test Completed Successfully ==="
echo "PgEngine installation and validation passed on '"$distro"'"
'

    # Run the test with timeout
    if timeout "$TEST_TIMEOUT" "${docker_run_cmd[@]}" bash -c "$test_script"; then
        log_success "Installation test passed on $distro"
        return 0
    else
        log_error "Installation test failed on $distro"
        return 1
    fi
}

# Run quick validation test
run_validation_test() {
    local distro="$1"
    local image_name="pgengine-test-$distro"
    local container_name="pgengine-validation-$distro-$$"

    log_info "Running quick validation test on $distro..."

    local docker_run_cmd=(
        docker run
        --name "$container_name"
        --rm
        -v "$SCRIPT_DIR:/home/tester/scripts:ro"
        "$image_name"
    )

    local test_script='
set -e
chmod +x /home/tester/scripts/validate-installation.sh

echo "=== Quick System Check ==="
echo "OS: $(cat /etc/os-release | grep PRETTY_NAME | cut -d= -f2 | tr -d \")"
echo "Architecture: $(uname -m)"
echo "Available packages:"

# Check what build tools are available
for tool in cmake make gcc g++ git; do
    if command -v $tool &> /dev/null; then
        echo "  ✓ $tool: $(command -v $tool)"
    else
        echo "  ✗ $tool: not found"
    fi
done

echo "Test environment ready for '"$distro"'"
'

    if "${docker_run_cmd[@]}" bash -c "$test_script"; then
        log_success "Validation test passed on $distro"
        return 0
    else
        log_error "Validation test failed on $distro"
        return 1
    fi
}

# Cleanup function
cleanup_containers() {
    if [[ "$CLEANUP_CONTAINERS" == "true" ]]; then
        log_info "Cleaning up Docker containers and images..."

        for distro in "${DISTROS[@]}"; do
            local image_name="pgengine-test-$distro"
            if docker images -q "$image_name" &> /dev/null; then
                docker rmi "$image_name" &> /dev/null || true
            fi
        done

        # Clean up any leftover containers
        docker container prune -f &> /dev/null || true
    fi
}

# Main test runner
main() {
    log_info "PgEngine Docker Test Runner"
    log_info "============================"
    log_info "Testing distributions: ${DISTROS[*]}"
    log_info "Repository: $PGENGINE_REPO"
    log_info "Timeout: ${TEST_TIMEOUT}s"
    echo

    # Setup cleanup trap
    trap cleanup_containers EXIT

    # Check prerequisites
    check_prerequisites
    echo

    local failed_distros=()
    local passed_distros=()

    # Test each distribution
    for distro in "${DISTROS[@]}"; do
        log_info "=========================================="
        log_info "Testing $distro"
        log_info "=========================================="

        # Build test image
        if ! build_test_image "$distro"; then
            failed_distros+=("$distro (build failed)")
            continue
        fi

        # Run quick validation
        if ! run_validation_test "$distro"; then
            failed_distros+=("$distro (validation failed)")
            continue
        fi

        # Run full installation test
        if run_installation_test "$distro"; then
            passed_distros+=("$distro")
            log_success "✓ $distro: ALL TESTS PASSED"
        else
            failed_distros+=("$distro (installation failed)")
            log_error "✗ $distro: TESTS FAILED"
        fi

        echo
    done

    # Final summary
    echo
    log_info "=========================================="
    log_info "TEST SUMMARY"
    log_info "=========================================="

    if [[ ${#passed_distros[@]} -gt 0 ]]; then
        log_success "Passed (${#passed_distros[@]}): ${passed_distros[*]}"
    fi

    if [[ ${#failed_distros[@]} -gt 0 ]]; then
        log_error "Failed (${#failed_distros[@]}): ${failed_distros[*]}"
        echo
        log_info "To debug failures:"
        log_info "1. Run with --no-cleanup to keep containers"
        log_info "2. Use --distro <name> to test specific distribution"
        log_info "3. Check Docker logs for detailed error messages"
        exit 1
    else
        echo
        log_success "🎉 ALL TESTS PASSED! 🎉"
        log_info "PgEngine installation scripts work correctly on all tested distributions"
        exit 0
    fi
}

# Show usage if no arguments and not in CI
if [[ $# -eq 0 && -t 1 ]]; then
    echo "PgEngine Docker Test Runner"
    echo
    echo "This script tests the PgEngine installation scripts in fresh Docker containers"
    echo "for multiple Linux distributions to ensure they work correctly."
    echo
    echo "Usage: $0 [options]"
    echo
    echo "Quick start:"
    echo "  $0                    # Test all distributions"
    echo "  $0 --distro ubuntu    # Test only Ubuntu"
    echo "  $0 --no-cleanup       # Keep containers for debugging"
    echo
    echo "Run '$0 --help' for full options."
    echo
    read -p "Press Enter to start testing, or Ctrl+C to cancel..."
fi

main "$@"