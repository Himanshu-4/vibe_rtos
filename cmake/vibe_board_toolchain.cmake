# cmake/vibe_board_toolchain.cmake
#
# Auto-select CMAKE_TOOLCHAIN_FILE based on VIBE_BOARD.
#
# Include this file BEFORE the first project() call so CMake activates the
# correct cross-compiler from the start.
#
# Usage in an application CMakeLists.txt:
#
#   cmake_minimum_required(VERSION 3.20)
#
#   # --- Set board (default: rpi_pico) BEFORE including this module ---
#   set(VIBE_BOARD "rpi_pico" CACHE STRING "Target board name")
#
#   # --- Auto-select toolchain from VIBE_BOARD ---
#   include("${CMAKE_CURRENT_LIST_DIR}/<path-to-vibe_rtos>/cmake/vibe_board_toolchain.cmake")
#
#   project(my_app C CXX ASM)
#   ...
#
# When VIBE_BOARD is passed on the cmake command line:
#   cmake -DVIBE_BOARD=rpi_pico2 -B build .
#
# The module is idempotent: if CMAKE_TOOLCHAIN_FILE is already set (e.g. the
# user passed -DCMAKE_TOOLCHAIN_FILE=... explicitly), it does nothing.

# -------------------------------------------------------------------------
# Board → toolchain table
# Add new boards here. The toolchain file path is relative to this module.
# -------------------------------------------------------------------------
set(_VIBE_ROOT "${CMAKE_CURRENT_LIST_DIR}/..")

set(_VIBE_BOARD_TOOLCHAIN_rpi_pico   "${_VIBE_ROOT}/cmake/toolchains/arm-none-eabi.cmake")
set(_VIBE_BOARD_TOOLCHAIN_rpi_pico2  "${_VIBE_ROOT}/cmake/toolchains/arm-none-eabi.cmake")
# Add future boards here:
# set(_VIBE_BOARD_TOOLCHAIN_my_board  "${_VIBE_ROOT}/cmake/toolchains/<toolchain>.cmake")

# -------------------------------------------------------------------------
# Default board
# -------------------------------------------------------------------------
if(NOT DEFINED VIBE_BOARD OR "${VIBE_BOARD}" STREQUAL "")
    set(VIBE_BOARD "rpi_pico" CACHE STRING "Target board name" FORCE)
    message(STATUS "VIBE_BOARD not set — defaulting to 'rpi_pico'")
endif()

# -------------------------------------------------------------------------
# Auto-select toolchain (only if not already specified)
# -------------------------------------------------------------------------
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE OR "${CMAKE_TOOLCHAIN_FILE}" STREQUAL "")
    set(_VIBE_TC_VAR "_VIBE_BOARD_TOOLCHAIN_${VIBE_BOARD}")
    if(DEFINED ${_VIBE_TC_VAR})
        set(CMAKE_TOOLCHAIN_FILE "${${_VIBE_TC_VAR}}"
            CACHE FILEPATH "CMake toolchain file (auto-selected for VIBE_BOARD=${VIBE_BOARD})" FORCE)
        message(STATUS "Auto-selected toolchain for '${VIBE_BOARD}': ${CMAKE_TOOLCHAIN_FILE}")
    else()
        message(FATAL_ERROR
            "No toolchain mapping found for board '${VIBE_BOARD}'.\n"
            "Add an entry to cmake/vibe_board_toolchain.cmake or pass "
            "-DCMAKE_TOOLCHAIN_FILE=<path> explicitly.")
    endif()
endif()
