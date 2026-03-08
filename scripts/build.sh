#!/usr/bin/env bash
# scripts/build.sh — VibeRTOS build helper
#
# Usage:
#   ./scripts/build.sh --board rpi_pico --app samples/hello_world
#   ./scripts/build.sh --board rpi_pico2 --app samples/blinky
#   ./scripts/build.sh --board rpi_pico --tests   (build and run host tests)
#
# Options:
#   --board <name>   Target board (rpi_pico | rpi_pico2)
#   --app <path>     Path to application directory (must have CMakeLists.txt)
#   --tests          Build and run host simulation unit tests
#   --clean          Remove build directory before building
#   -j <n>           Parallel jobs (default: nproc)
#   -v               Verbose make output

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
RTOS_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

BOARD=""
APP_PATH=""
BUILD_TESTS=0
CLEAN=0
JOBS=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
VERBOSE=0

# -----------------------------------------------------------------------
# Parse arguments
# -----------------------------------------------------------------------
while [[ $# -gt 0 ]]; do
    case "$1" in
        --board)   BOARD="$2"; shift 2 ;;
        --app)     APP_PATH="$2"; shift 2 ;;
        --tests)   BUILD_TESTS=1; shift ;;
        --clean)   CLEAN=1; shift ;;
        -j)        JOBS="$2"; shift 2 ;;
        -v)        VERBOSE=1; shift ;;
        -h|--help)
            echo "Usage: $0 [--board <name>] [--app <path>] [--tests] [--clean] [-j N] [-v]"
            exit 0 ;;
        *)
            echo "Unknown option: $1" >&2
            exit 1 ;;
    esac
done

# -----------------------------------------------------------------------
# Host simulation tests
# -----------------------------------------------------------------------
if [[ $BUILD_TESTS -eq 1 ]]; then
    echo "=== Building host simulation tests ==="
    BUILD_DIR="${RTOS_ROOT}/build/tests"

    [[ $CLEAN -eq 1 ]] && rm -rf "${BUILD_DIR}"
    mkdir -p "${BUILD_DIR}"

    cmake -S "${RTOS_ROOT}" -B "${BUILD_DIR}" \
        -DCMAKE_TOOLCHAIN_FILE="${RTOS_ROOT}/cmake/toolchains/host-gcc.cmake" \
        -DVIBE_BUILD_TESTS=ON \
        -DVIBE_BUILD_SAMPLES=OFF

    MAKE_FLAGS="-j${JOBS}"
    [[ $VERBOSE -eq 1 ]] && MAKE_FLAGS="${MAKE_FLAGS} VERBOSE=1"
    make ${MAKE_FLAGS} -C "${BUILD_DIR}"

    echo ""
    echo "=== Running tests ==="
    ctest --test-dir "${BUILD_DIR}" --output-on-failure
    exit 0
fi

# -----------------------------------------------------------------------
# Application build
# -----------------------------------------------------------------------
if [[ -z "$BOARD" ]]; then
    echo "ERROR: --board is required for application builds" >&2
    exit 1
fi

if [[ -z "$APP_PATH" ]]; then
    echo "ERROR: --app is required for application builds" >&2
    exit 1
fi

# Resolve app path
if [[ "$APP_PATH" = /* ]]; then
    APP_ABS="$APP_PATH"
else
    APP_ABS="${RTOS_ROOT}/${APP_PATH}"
fi

if [[ ! -f "${APP_ABS}/CMakeLists.txt" ]]; then
    echo "ERROR: No CMakeLists.txt found in ${APP_ABS}" >&2
    exit 1
fi

APP_NAME="$(basename "${APP_ABS}")"
BUILD_DIR="${RTOS_ROOT}/build/${BOARD}/${APP_NAME}"

echo "=== VibeRTOS Build ==="
echo "  Board:  ${BOARD}"
echo "  App:    ${APP_ABS}"
echo "  Output: ${BUILD_DIR}"
echo ""

[[ $CLEAN -eq 1 ]] && rm -rf "${BUILD_DIR}"
mkdir -p "${BUILD_DIR}"

# Select toolchain based on board
TOOLCHAIN="${RTOS_ROOT}/cmake/toolchains/arm-none-eabi.cmake"

cmake -S "${APP_ABS}" -B "${BUILD_DIR}" \
    -DCMAKE_TOOLCHAIN_FILE="${TOOLCHAIN}" \
    -DCMAKE_PREFIX_PATH="${RTOS_ROOT}" \
    -DVIBE_BOARD="${BOARD}"

MAKE_FLAGS="-j${JOBS}"
[[ $VERBOSE -eq 1 ]] && MAKE_FLAGS="${MAKE_FLAGS} VERBOSE=1"
make ${MAKE_FLAGS} -C "${BUILD_DIR}"

echo ""
echo "=== Build complete ==="
echo "  ELF: ${BUILD_DIR}/${APP_NAME}"
echo "  BIN: ${BUILD_DIR}/${APP_NAME}.bin"
echo "  HEX: ${BUILD_DIR}/${APP_NAME}.hex"
