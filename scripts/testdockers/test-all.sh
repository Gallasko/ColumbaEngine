#!/bin/bash

# Master Test Script for PgEngine Installation
# Orchestrates all testing: quick tests, Docker tests, validation

set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
BOLD='\033[1m'
NC='\033[0m'

log_info() { echo -e "${BLUE}[INFO]${NC} $1"; }
log_success() { echo -e "${GREEN}[SUCCESS]${NC} $1"; }
log_warning() { echo -e "${YELLOW}[WARNING]${NC} $1"; }
log_error() { echo -e "${RED}[ERROR]${NC} $1"; }
log_header() { echo -e "${BOLD}${BLUE}=== $1 ===${NC}"; }

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
INSTALL_SCRIPTS_DIR="$PROJECT_ROOT/scripts/install"
DOCKER_SCRIPTS_DIR="$PROJECT_ROOT/scripts/testdockers"

TEST_LEVEL="${1:-quick}"
PGENGINE_REPO="${PGENGINE_REPO:-https://github.com/Gallasko/PgEngine.git}"
DISTROS=("ubuntu" "fedora" "arch")

# Test results tracking
declare -A test_results
total_tests=0
passed_tests=0

# Track test result
track_test() {
    local test_name="$1"
    local result="$2"

    test_results["$test_name"]="$result"
    ((total_tests++))

    if [[ "$result" == "PASS" ]]; then
        ((passed_tests++))
        log_success "✓ $test_name"
    else
        log_error "✗ $test_name: $result"
    fi
}

# Prerequisites check
check_prerequisites() {
    log_header "Checking Prerequisites"

    local missing_tools=()

    # Check Docker
    if ! command -v docker &> /dev/null; then
        missing_tools+=("docker")
    elif ! docker info &> /dev/null; then
        log_error "Docker is installed but daemon is not running"
        return 1
    fi

    # Check required scripts
    local required_scripts=(
        "install-pgengine.sh"
        "install-emscripten.sh"
        "validate-installation.sh"
    )

    for script in "${required_scripts[@]}"; do
        if [[ ! -f "$INSTALL_SCRIPTS_DIR/$script" ]]; then
            missing_tools+=("$INSTALL_SCRIPTS_DIR/$script")
        fi
    done

    # Check test runner scripts
    local test_scripts=(
        "quick-test.sh"
        "run-docker-tests.sh"
    )

    for script in "${test_scripts[@]}"; do
        if [[ ! -f "$DOCKER_SCRIPTS_DIR/$script" ]]; then
            missing_tools+=("$DOCKER_SCRIPTS_DIR/$script")
        fi
    done

    # Check Dockerfiles
    for distro in "${DISTROS[@]}"; do
        if [[ ! -f "$DOCKER_SCRIPTS_DIR/test-$distro.Dockerfile" ]]; then
            missing_tools+=("$DOCKER_SCRIPTS_DIR/test-$distro.Dockerfile")
        fi
    done

    if [[ ${#missing_tools[@]} -gt 0 ]]; then
        log_error "Missing required tools/files:"
        for tool in "${missing_tools[@]}"; do
            echo "  - $tool"
        done
        return 1
    fi

    log_success "All prerequisites satisfied"
    return 0
}

# Quick smoke tests
run_smoke_tests() {
    log_header "Running Smoke Tests"

    # Test 1: Script syntax
    log_info "Checking script syntax..."
    local syntax_errors=0

    # Check install scripts
    for script in "$INSTALL_SCRIPTS_DIR"/*.sh; do
        if [[ -f "$script" ]] && ! bash -n "$script" 2>/dev/null; then
            log_error "Syntax error in $(basename "$script")"
            ((syntax_errors++))
        fi
    done

    # Check docker test scripts
    for script in "$DOCKER_SCRIPTS_DIR"/*.sh; do
        if [[ -f "$script" ]] && ! bash -n "$script" 2>/dev/null; then
            log_error "Syntax error in $(basename "$script")"
            ((syntax_errors++))
        fi
    done

    if [[ $syntax_errors -eq 0 ]]; then
        track_test "Script Syntax" "PASS"
    else
        track_test "Script Syntax" "FAIL ($syntax_errors errors)"
        return 1
    fi

    # Test 2: Help text
    log_info "Testing help text..."
    if "$INSTALL_SCRIPTS_DIR/install-pgengine.sh" --help &> /dev/null; then
        track_test "Help Text" "PASS"
    else
        track_test "Help Text" "FAIL"
    fi

    # Test 3: Docker build test
    log_info "Testing Docker build..."
    if docker build -t pgengine-smoke-test -f "$DOCKER_SCRIPTS_DIR/test-ubuntu.Dockerfile" "$DOCKER_SCRIPTS_DIR" &> /dev/null; then
        track_test "Docker Build" "PASS"
        docker rmi pgengine-smoke-test &> /dev/null || true
    else
        track_test "Docker Build" "FAIL"
    fi
    done

    if [[ $syntax_errors -eq 0 ]]; then
        track_test "Script Syntax" "PASS"
    else
        track_test "Script Syntax" "FAIL ($syntax_errors errors)"
        return 1
    fi

    # Test 2: Help text
    log_info "Testing help text..."
    if "$INSTALL_SCRIPTS_DIR/install-pgengine.sh" --help &> /dev/null; then
        track_test "Help Text" "PASS"
    else
        track_test "Help Text" "FAIL"
    fi

    # Test 3: Docker build test
    log_info "Testing Docker build..."
    if docker build -t pgengine-smoke-test -f "$DOCKER_SCRIPTS_DIR/test-ubuntu.Dockerfile" "$DOCKER_SCRIPTS_DIR" &> /dev/null; then
        track_test "Docker Build" "PASS"
        docker rmi pgengine-smoke-test &> /dev/null || true
    else
        track_test "Docker Build" "FAIL"
    fi
}
}

# Quick functionality tests
run_quick_tests() {
    log_header "Running Quick Tests"

    local scenarios=("deps" "build" "validate")

    for scenario in "${scenarios[@]}"; do
        log_info "Testing scenario: $scenario"

        if timeout 600 "$DOCKER_SCRIPTS_DIR/quick-test.sh" "$scenario" &> /tmp/quick-test-$scenario.log; then
            track_test "Quick Test ($scenario)" "PASS"
        else
            track_test "Quick Test ($scenario)" "FAIL"
            log_warning "Check log: /tmp/quick-test-$scenario.log"
        fi
    done
}

# Full Docker tests
run_docker_tests() {
    log_header "Running Docker Tests"

    for distro in "${DISTROS[@]}"; do
        log_info "Testing $distro distribution..."

        if timeout 3600 "$DOCKER_SCRIPTS_DIR/run-docker-tests.sh" --distro "$distro" &> /tmp/docker-test-$distro.log; then
            track_test "Docker Test ($distro)" "PASS"
        else
            track_test "Docker Test ($distro)" "FAIL"
            log_warning "Check log: /tmp/docker-test-$distro.log"
        fi
    done
}

# Comprehensive tests
run_comprehensive_tests() {
    log_header "Running Comprehensive Tests"

    # Full multi-distro test
    log_info "Running full multi-distribution test..."
    if timeout 7200 "$DOCKER_SCRIPTS_DIR/run-docker-tests.sh" &> /tmp/comprehensive-test.log; then
        track_test "Multi-Distro Test" "PASS"
    else
        track_test "Multi-Distro Test" "FAIL"
        log_warning "Check log: /tmp/comprehensive-test.log"
    fi

    # Emscripten test (if available)
    log_info "Testing Emscripten support..."
    if timeout 1800 "$DOCKER_SCRIPTS_DIR/quick-test.sh" emscripten &> /tmp/emscripten-test.log; then
        track_test "Emscripten Test" "PASS"
    else
        track_test "Emscripten Test" "FAIL (expected in some environments)"
        log_warning "Emscripten test failed (this may be expected)"
    fi
}

# Performance tests
run_performance_tests() {
    log_header "Running Performance Tests"

    log_info "Testing build performance..."

    # Measure build times
    local start_time=$(date +%s)

    if timeout 1800 "$DOCKER_SCRIPTS_DIR/quick-test.sh" build &> /tmp/performance-test.log; then
        local end_time=$(date +%s)
        local duration=$((end_time - start_time))

        if [[ $duration -lt 900 ]]; then  # Less than 15 minutes
            track_test "Build Performance" "PASS (${duration}s)"
        else
            track_test "Build Performance" "SLOW (${duration}s)"
        fi
    else
        track_test "Build Performance" "FAIL"
    fi
}

# Generate test report
generate_report() {
    log_header "Test Report"

    echo
    log_info "Test Summary:"
    log_info "============="
    log_info "Total tests: $total_tests"
    log_success "Passed: $passed_tests"
    log_error "Failed: $((total_tests - passed_tests))"

    local pass_rate=$((passed_tests * 100 / total_tests))
    log_info "Pass rate: ${pass_rate}%"

    echo
    log_info "Detailed Results:"
    log_info "================"

    for test_name in "${!test_results[@]}"; do
        local result="${test_results[$test_name]}"
        if [[ "$result" == "PASS" ]]; then
            echo -e "  ${GREEN}✓${NC} $test_name"
        else
            echo -e "  ${RED}✗${NC} $test_name: $result"
        fi
    done

    echo

    # Generate JSON report
    local json_report="/tmp/pgengine-test-report.json"
    cat > "$json_report" << EOF
{
  "test_run": {
    "timestamp": "$(date -Iseconds)",
    "repository": "$PGENGINE_REPO",
    "test_level": "$TEST_LEVEL",
    "total_tests": $total_tests,
    "passed_tests": $passed_tests,
    "pass_rate": $pass_rate
  },
  "results": {
EOF

    local first=true
    for test_name in "${!test_results[@]}"; do
        if [[ "$first" == "true" ]]; then
            first=false
        else
            echo "," >> "$json_report"
        fi
        echo -n "    \"$test_name\": \"${test_results[$test_name]}\"" >> "$json_report"
    done

    cat >> "$json_report" << EOF

  }
}
EOF

    log_info "JSON report saved to: $json_report"

    # Final result
    if [[ $passed_tests -eq $total_tests ]]; then
        echo
        log_success "🎉 ALL TESTS PASSED! 🎉"
        log_info "PgEngine installation scripts are working correctly!"
        return 0
    else
        echo
        log_error "❌ Some tests failed"
        log_info "Check the logs and fix issues before releasing"
        return 1
    fi
}

# Main test runner
main() {
    log_header "PgEngine Installation Script Test Suite"

    log_info "Test level: $TEST_LEVEL"
    log_info "Repository: $PGENGINE_REPO"
    log_info "Install scripts: $INSTALL_SCRIPTS_DIR"
    log_info "Docker scripts: $DOCKER_SCRIPTS_DIR"
    echo

    # Always check prerequisites
    if ! check_prerequisites; then
        log_error "Prerequisites check failed"
        exit 1
    fi

    echo

    case "$TEST_LEVEL" in
        "smoke")
            run_smoke_tests
            ;;
        "quick")
            run_smoke_tests
            run_quick_tests
            ;;
        "docker")
            run_smoke_tests
            run_docker_tests
            ;;
        "full"|"comprehensive")
            run_smoke_tests
            run_quick_tests
            run_docker_tests
            run_comprehensive_tests
            ;;
        "performance"|"perf")
            run_smoke_tests
            run_performance_tests
            ;;
        "all")
            run_smoke_tests
            run_quick_tests
            run_docker_tests
            run_comprehensive_tests
            run_performance_tests
            ;;
        *)
            log_error "Unknown test level: $TEST_LEVEL"
            usage
            exit 1
            ;;
    esac

    echo
    generate_report
}

# Usage information
usage() {
    cat << EOF
Usage: $0 [test-level]

Test levels:
  smoke         Basic syntax and Docker build tests
  quick         Smoke tests + quick functionality tests (default)
  docker        Smoke tests + Docker distribution tests
  full          All tests except performance
  performance   Build performance tests
  all           All available tests

Environment variables:
  PGENGINE_REPO    Repository URL to test

Examples:
  $0                    # Run quick tests
  $0 smoke              # Run only smoke tests
  $0 full               # Run comprehensive tests
  $0 all                # Run everything including performance tests

  PGENGINE_REPO=https://github.com/user/fork.git $0 docker

Test logs are saved to /tmp/pgengine-test-*.log
EOF
}

# Cleanup function
cleanup() {
    log_info "Cleaning up test artifacts..."
    docker container prune -f &> /dev/null || true
    docker image prune -f &> /dev/null || true
}

trap cleanup EXIT

# Handle help
if [[ "$1" == "-h" || "$1" == "--help" ]]; then
    usage
    exit 0
fi

# Validate test level
case "${1:-quick}" in
    smoke|quick|docker|full|comprehensive|performance|perf|all)
        ;;
    *)
        log_error "Invalid test level: ${1:-quick}"
        usage
        exit 1
        ;;
esac

# Run main test suite
main "$@"