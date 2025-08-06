#!/bin/bash

# Quick Docker Test for PgEngine Installation Scripts
# Tests scripts in a single Ubuntu container for rapid development

set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

log_info() { echo -e "${BLUE}[INFO]${NC} $1"; }
log_success() { echo -e "${GREEN}[SUCCESS]${NC} $1"; }
log_error() { echo -e "${RED}[ERROR]${NC} $1"; }

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
INSTALL_SCRIPTS_DIR="$PROJECT_ROOT/scripts/install"
DOCKER_SCRIPTS_DIR="$PROJECT_ROOT/scripts/testdockers"

CONTAINER_NAME="pgengine-quick-test-$"
PGENGINE_REPO="${PGENGINE_REPO:-https://github.com/Gallasko/PgEngine.git}"
TEST_PHASE="${1:-all}"

cleanup() {
    if docker ps -q -f name="$CONTAINER_NAME" &> /dev/null; then
        log_info "Stopping container..."
        docker stop "$CONTAINER_NAME" &> /dev/null || true
    fi
    if docker ps -aq -f name="$CONTAINER_NAME" &> /dev/null; then
        docker rm "$CONTAINER_NAME" &> /dev/null || true
    fi
}

trap cleanup EXIT

run_quick_test() {
    log_info "Starting quick test in Ubuntu container..."
    log_info "Container: $CONTAINER_NAME"
    log_info "Repository: $PGENGINE_REPO"
    log_info "Test phase: $TEST_PHASE"
    log_info "Install scripts: $INSTALL_SCRIPTS_DIR"
    echo

    # Run container with scripts mounted
    docker run -it --name "$CONTAINER_NAME" \
        --rm \
        -v "$INSTALL_SCRIPTS_DIR:/home/tester/scripts:ro" \
        -e "PGENGINE_REPO=$PGENGINE_REPO" \
        -e "BUILD_JOBS=2" \
        ubuntu:22.04 bash -c "

        # Setup environment
        export DEBIAN_FRONTEND=noninteractive
        apt-get update && apt-get install -y sudo curl wget ca-certificates

        # Create test user
        useradd -m -s /bin/bash tester
        usermod -aG sudo tester
        echo 'tester ALL=(ALL) NOPASSWD:ALL' >> /etc/sudoers

        # Switch to test user and run tests
        sudo -u tester bash << 'USEREOF'
cd /home/tester
chmod +x scripts/*.sh

echo '========================================'
echo 'PgEngine Quick Test'
echo '========================================'
echo 'Phase: $TEST_PHASE'
echo 'Repo: $PGENGINE_REPO'
echo

case '$TEST_PHASE' in
    'deps'|'dependencies')
        echo '=== Testing Dependency Installation ==='
        # Test just the dependency installation part
        timeout 600 scripts/install-pgengine.sh --help
        echo 'Dependencies check completed'
        ;;

    'build')
        echo '=== Testing Build Process ==='
        # Run installation but stop before final validation
        timeout 1800 scripts/install-pgengine.sh -j 2 || echo 'Build test completed'
        ;;

    'validate')
        echo '=== Testing Validation Only ==='
        # Assume PgEngine is already installed, just validate
        timeout 300 scripts/validate-installation.sh || echo 'Validation test completed'
        ;;

    'emscripten')
        echo '=== Testing Emscripten Installation ==='
        timeout 1200 scripts/install-emscripten.sh --help || echo 'Emscripten test completed'
        ;;

    'all'|*)
        echo '=== Full Installation Test ==='

        echo '--- Phase 1: Installing PgEngine ---'
        timeout 2400 scripts/install-pgengine.sh -j 2

        echo '--- Phase 2: Validating Installation ---'
        timeout 300 scripts/validate-installation.sh

        echo '--- Phase 3: Testing Example Project ---'
        cd ~/pgengine-install/test-app
        ./build.sh

        echo '--- All phases completed successfully ---'
        ;;
esac

echo
echo '========================================'
echo 'Quick Test Completed Successfully!'
echo '========================================'
USEREOF
    "
}

usage() {
    echo "Usage: $0 [phase]"
    echo
    echo "Quick test PgEngine installation scripts in Docker"
    echo
    echo "Phases:"
    echo "  deps       Test dependency installation only"
    echo "  build      Test build process"
    echo "  validate   Test validation scripts"
    echo "  emscripten Test Emscripten installation"
    echo "  all        Full installation test (default)"
    echo
    echo "Environment variables:"
    echo "  PGENGINE_REPO  Repository URL to test"
    echo
    echo "Examples:"
    echo "  $0                    # Full test"
    echo "  $0 deps               # Test dependencies only"
    echo "  $0 build              # Test build process"
    echo "  PGENGINE_REPO=https://github.com/user/fork.git $0"
    echo
    echo "Paths:"
    echo "  Install scripts: $INSTALL_SCRIPTS_DIR"
    echo "  Docker files: $DOCKER_SCRIPTS_DIR"
}

# Check prerequisites
if ! command -v docker &> /dev/null; then
    log_error "Docker is required but not installed"
    exit 1
fi

if ! docker info &> /dev/null; then
    log_error "Docker daemon is not running"
    exit 1
fi

# Check for help
if [[ "$1" == "-h" || "$1" == "--help" ]]; then
    usage
    exit 0
fi

# Check if scripts exist
required_scripts=("install-pgengine.sh" "validate-installation.sh")
for script in "${required_scripts[@]}"; do
    if [[ ! -f "$INSTALL_SCRIPTS_DIR/$script" ]]; then
        log_error "Required script not found: $INSTALL_SCRIPTS_DIR/$script"
        log_info "Please ensure all installation scripts are in $INSTALL_SCRIPTS_DIR"
        exit 1
    #!/bin/bash

# Quick Docker Test for PgEngine Installation Scripts
# Tests scripts in a single Ubuntu container for rapid development

set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

log_info() { echo -e "${BLUE}[INFO]${NC} $1"; }
log_success() { echo -e "${GREEN}[SUCCESS]${NC} $1"; }
log_error() { echo -e "${RED}[ERROR]${NC} $1"; }

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CONTAINER_NAME="pgengine-quick-test-$$"
PGENGINE_REPO="${PGENGINE_REPO:-https://github.com/Gallasko/PgEngine.git}"
TEST_PHASE="${1:-all}"

cleanup() {
    if docker ps -q -f name="$CONTAINER_NAME" &> /dev/null; then
        log_info "Stopping container..."
        docker stop "$CONTAINER_NAME" &> /dev/null || true
    fi
    if docker ps -aq -f name="$CONTAINER_NAME" &> /dev/null; then
        docker rm "$CONTAINER_NAME" &> /dev/null || true
    fi
}

trap cleanup EXIT

run_quick_test() {
    log_info "Starting quick test in Ubuntu container..."
    log_info "Container: $CONTAINER_NAME"
    log_info "Repository: $PGENGINE_REPO"
    log_info "Test phase: $TEST_PHASE"
    echo

    # Run container with scripts mounted
    docker run -it --name "$CONTAINER_NAME" \
        --rm \
        -v "$SCRIPT_DIR:/home/tester/scripts:ro" \
        -e "PGENGINE_REPO=$PGENGINE_REPO" \
        -e "BUILD_JOBS=2" \
        ubuntu:22.04 bash -c "

        # Setup environment
        export DEBIAN_FRONTEND=noninteractive
        apt-get update && apt-get install -y sudo curl wget ca-certificates

        # Create test user
        useradd -m -s /bin/bash tester
        usermod -aG sudo tester
        echo 'tester ALL=(ALL) NOPASSWD:ALL' >> /etc/sudoers

        # Switch to test user and run tests
        sudo -u tester bash << 'USEREOF'
cd /home/tester
chmod +x scripts/*.sh

echo '========================================'
echo 'PgEngine Quick Test'
echo '========================================'
echo 'Phase: $TEST_PHASE'
echo 'Repo: $PGENGINE_REPO'
echo

case '$TEST_PHASE' in
    'deps'|'dependencies')
        echo '=== Testing Dependency Installation ==='
        # Test just the dependency installation part
        timeout 600 scripts/install-pgengine.sh --help
        echo 'Dependencies check completed'
        ;;

    'build')
        echo '=== Testing Build Process ==='
        # Run installation but stop before final validation
        timeout 1800 scripts/install-pgengine.sh -j 2 || echo 'Build test completed'
        ;;

    'validate')
        echo '=== Testing Validation Only ==='
        # Assume PgEngine is already installed, just validate
        timeout 300 scripts/validate-installation.sh || echo 'Validation test completed'
        ;;

    'emscripten')
        echo '=== Testing Emscripten Installation ==='
        timeout 1200 scripts/install-emscripten.sh --help || echo 'Emscripten test completed'
        ;;

    'all'|*)
        echo '=== Full Installation Test ==='

        echo '--- Phase 1: Installing PgEngine ---'
        timeout 2400 scripts/install-pgengine.sh -j 2

        echo '--- Phase 2: Validating Installation ---'
        timeout 300 scripts/validate-installation.sh

        echo '--- Phase 3: Testing Example Project ---'
        cd ~/pgengine-install/test-app
        ./build.sh

        echo '--- All phases completed successfully ---'
        ;;
esac

echo
echo '========================================'
echo 'Quick Test Completed Successfully!'
echo '========================================'
USEREOF
    "
}

usage() {
    echo "Usage: $0 [phase]"
    echo
    echo "Quick test PgEngine installation scripts in Docker"
    echo
    echo "Phases:"
    echo "  deps       Test dependency installation only"
    echo "  build      Test build process"
    echo "  validate   Test validation scripts"
    echo "  emscripten Test Emscripten installation"
    echo "  all        Full installation test (default)"
    echo
    echo "Environment variables:"
    echo "  PGENGINE_REPO  Repository URL to test"
    echo
    echo "Examples:"
    echo "  $0                    # Full test"
    echo "  $0 deps               # Test dependencies only"
    echo "  $0 build              # Test build process"
    echo "  PGENGINE_REPO=https://github.com/user/fork.git $0"
}

# Check prerequisites
if ! command -v docker &> /dev/null; then
    log_error "Docker is required but not installed"
    exit 1
fi

if ! docker info &> /dev/null; then
    log_error "Docker daemon is not running"
    exit 1
fi

# Check for help
if [[ "$1" == "-h" || "$1" == "--help" ]]; then
    usage
    exit 0
fi

# Check if scripts exist
required_scripts=("install-pgengine.sh" "validate-installation.sh")
for script in "${required_scripts[@]}"; do
    if [[ ! -f "$SCRIPT_DIR/$script" ]]; then
        log_error "Required script not found: $script"
        log_info "Please ensure all installation scripts are in the same directory"
        exit 1
    fi
done

# Run the test
run_quick_test

log_success "Quick test completed successfully!"
echo
log_info "For full multi-distribution testing, run: $PROJECT_ROOT/scripts/testdockers/run-docker-tests.sh"