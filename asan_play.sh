#!/bin/bash
# Build + run a target under AddressSanitizer / UndefinedBehaviorSanitizer.
#
# Usage:
#   ./asan_play.sh GameDevJs2026
#
# First run configures the asan/ build directory; subsequent runs just rebuild
# and relaunch. Skips component generation so the build doesn't depend on
# PgCompilerBootstrap (which fails to link in the minimal-engine config).

set -e

TARGET="${1:-GameDevJs2026}"
BUILD_DIR="asan"

cd "$(dirname "$0")"

# Pick a build dir that already has generated component sources so we can reuse
# them via PREGENERATED_COMPONENTS_DIR (avoids needing PgCompilerBootstrap, which
# fails to link in the minimal-engine config).
PREGEN_DIR=""
for candidate in release build; do
    if [[ -f "${candidate}/generated/Components/PositionComponent.generated.cpp" ]]; then
        PREGEN_DIR="$(cd "${candidate}" && pwd)/generated/Components"
        break
    fi
done

if [[ -z "${PREGEN_DIR}" ]]; then
    echo "[asan_play] ERROR: no pre-generated components found in release/ or build/."
    echo "[asan_play] Run your normal release/debug build at least once first so the .generated.cpp files exist."
    exit 1
fi

# Force gcc/g++ so the ASan runtime (libasan.so.6 from libasan6) matches the
# compiler. The system default /usr/bin/c++ is clang 11, but clang's runtime
# (libclang_rt.asan-*.so) isn't installed → at runtime clang would pull in
# gcc's libasan anyway, triggering "incompatible ASan runtimes".
export CC="${CC:-gcc}"
export CXX="${CXX:-g++}"

# Configure once. CMakeCache.txt is our "is this already configured" marker.
if [[ ! -f "${BUILD_DIR}/CMakeCache.txt" ]]; then
    echo "[asan_play] First-time configure of ${BUILD_DIR}/ using pregen=${PREGEN_DIR}, CXX=${CXX} ..."
    mkdir -p "${BUILD_DIR}"
    cmake -S . -B "${BUILD_DIR}" \
        -DCMAKE_BUILD_TYPE=RelWithDebInfo \
        -DSKIP_COMPONENT_GENERATION=ON \
        -DPREGENERATED_COMPONENTS_DIR="${PREGEN_DIR}" \
        -DCMAKE_C_COMPILER="${CC}" \
        -DCMAKE_CXX_COMPILER="${CXX}" \
        -DCMAKE_CXX_FLAGS="-O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined -fno-sanitize-recover=all" \
        -DCMAKE_C_FLAGS="-O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined -fno-sanitize-recover=all" \
        -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined" \
        -DCMAKE_SHARED_LINKER_FLAGS="-fsanitize=address,undefined" \
        -DCMAKE_C_STANDARD_LIBRARIES="-ldl -lpthread" \
        -DCMAKE_CXX_STANDARD_LIBRARIES="-ldl -lpthread"
fi

echo "[asan_play] Building ${TARGET} ..."
cmake --build "${BUILD_DIR}" --target "${TARGET}" -j8

echo "[asan_play] Launching ${TARGET} under ASan/UBSan ..."
# Runtime sanitizer options:
#   detect_leaks=0           : skip leak report at exit (noisy; turn on for a dedicated leak hunt)
#   halt_on_error=1          : stop on first error so the stack trace is clean
#   abort_on_error=0         : print + exit instead of abort()-ing (cleaner for crash logs)
#   strict_string_checks=1   : flag bad strcpy/strncpy patterns
#   check_initialization_order=1 : catch static init order fiasco
#   print_stacktrace=1       : UBSan stack traces
ASAN_OPTIONS="detect_leaks=0:halt_on_error=1:abort_on_error=0:strict_string_checks=1:check_initialization_order=1" \
UBSAN_OPTIONS="print_stacktrace=1:halt_on_error=1" \
"./${BUILD_DIR}/${TARGET}"
