# kconfig.cmake
# VibeRTOS Kconfig integration.
#
# Provides:
#   - Configure-time generation of autoconf.h, sdkconfig.cmake, sdkconfig.json
#   - Build targets: menuconfig, guiconfig, defconfig, saveconfig
#
# Generated files (all in CMAKE_BINARY_DIR):
#   sdkconfig                         — Kconfig .config (source of truth)
#   sdkconfig.cmake                   — CMake set(CONFIG_*) variables
#   sdkconfig.json                    — JSON representation
#   include/generated/autoconf.h      — C #define CONFIG_* header
#
# Usage from an app CMakeLists.txt:
#   ninja menuconfig     # interactive ncurses UI  (requires: pip install kconfiglib)
#   ninja guiconfig      # Tkinter GUI
#   ninja defconfig      # reset to board defaults
#   ninja saveconfig     # write build sdkconfig back to board defconfig

cmake_minimum_required(VERSION 3.20)

# ---------------------------------------------------------------------------
# Python 3 — prefer the project venv if it exists
# ---------------------------------------------------------------------------
set(_VIBE_VENV ${VIBE_RTOS_ROOT}/.venv)
if(EXISTS ${_VIBE_VENV}/bin/python3)
    set(Python3_EXECUTABLE ${_VIBE_VENV}/bin/python3)
    message(STATUS "Kconfig: using venv Python: ${Python3_EXECUTABLE}")
else()
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
endif()

# ---------------------------------------------------------------------------
# Check kconfiglib
# ---------------------------------------------------------------------------
execute_process(
    COMMAND ${Python3_EXECUTABLE} -c "import kconfiglib"
    RESULT_VARIABLE _KCONFIGLIB_RC
    OUTPUT_QUIET ERROR_QUIET
)
if(NOT _KCONFIGLIB_RC EQUAL 0)
    message(WARNING
        "kconfiglib not found — menuconfig/guiconfig targets will not work.\n"
        "Install it with:  pip install kconfiglib")
endif()

# ---------------------------------------------------------------------------
# Paths
# ---------------------------------------------------------------------------
set(KCONFIG_ROOT        ${VIBE_RTOS_ROOT}/Kconfig)
set(KCONFIG_SCRIPTS_DIR ${VIBE_RTOS_ROOT}/scripts/kconfig)
set(SDKCONFIG           ${CMAKE_BINARY_DIR}/sdkconfig)
set(SDKCONFIG_CMAKE     ${CMAKE_BINARY_DIR}/sdkconfig.cmake)
set(SDKCONFIG_JSON      ${CMAKE_BINARY_DIR}/sdkconfig.json)
set(AUTOCONF_H          ${CMAKE_BINARY_DIR}/include/generated/autoconf.h)

if(DEFINED VIBE_BOARD)
    set(_BOARD_DEFCONFIG
        ${VIBE_RTOS_ROOT}/boards/arm/${VIBE_BOARD}/${VIBE_BOARD}_defconfig)
else()
    set(_BOARD_DEFCONFIG "")
endif()

# ---------------------------------------------------------------------------
# Build argument list for gen_configs.py (shared by all targets)
# ---------------------------------------------------------------------------
set(_KCONFIG_GEN_CMD
    ${Python3_EXECUTABLE}
    ${KCONFIG_SCRIPTS_DIR}/gen_configs.py
    --kconfig  ${KCONFIG_ROOT}
    --config   ${SDKCONFIG}
    --header   ${AUTOCONF_H}
    --cmake    ${SDKCONFIG_CMAKE}
    --json     ${SDKCONFIG_JSON}
    --srctree  ${VIBE_RTOS_ROOT}
)
if(_BOARD_DEFCONFIG)
    list(APPEND _KCONFIG_GEN_CMD --board-defconfig ${_BOARD_DEFCONFIG})
endif()

# ---------------------------------------------------------------------------
# Configure-time generation
# Runs gen_configs.py immediately during cmake configure so that autoconf.h
# and sdkconfig.cmake exist before any targets are built.
# ---------------------------------------------------------------------------
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/include/generated)

if(_KCONFIGLIB_RC EQUAL 0)
    execute_process(
        COMMAND           ${_KCONFIG_GEN_CMD}
        WORKING_DIRECTORY ${VIBE_RTOS_ROOT}
        RESULT_VARIABLE   _GEN_RC
    )
    if(NOT _GEN_RC EQUAL 0)
        message(WARNING "Kconfig generation failed — falling back to CMake parser")
        set(_KCONFIGLIB_RC 1)   # trigger fallback below
    endif()
endif()

# Fallback: if kconfiglib is missing or failed, use the pure-CMake parser
# to produce a minimal autoconf.h so the build does not break.
if(NOT _KCONFIGLIB_RC EQUAL 0)
    _vibe_kconfig_cmake_fallback()
endif()

# ---------------------------------------------------------------------------
# Include generated sdkconfig.cmake so CONFIG_* are CMake variables
# ---------------------------------------------------------------------------
if(EXISTS ${SDKCONFIG_CMAKE})
    include(${SDKCONFIG_CMAKE})
else()
    message(STATUS "Kconfig: sdkconfig.cmake not found — CONFIG_* variables unavailable")
endif()

# ---------------------------------------------------------------------------
# Re-trigger cmake configure whenever sdkconfig or any Kconfig file changes.
#
# This causes cmake to re-run gen_configs.py at configure time, re-include
# sdkconfig.cmake (updating all CONFIG_* CMake variables), and regenerate
# the build system — so subsequent ninja invocations reflect the new config.
# ---------------------------------------------------------------------------
file(GLOB_RECURSE _ALL_KCONFIG_FILES
    ${VIBE_RTOS_ROOT}/Kconfig
    ${VIBE_RTOS_ROOT}/*/Kconfig
    ${VIBE_RTOS_ROOT}/*/*/Kconfig
    ${VIBE_RTOS_ROOT}/*/*/*/Kconfig
)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    ${SDKCONFIG}
    ${_ALL_KCONFIG_FILES}
)

# ---------------------------------------------------------------------------
# Force-include autoconf.h into every translation unit.
#
# This ensures Ninja's implicit dependency scanner connects every compiled
# object file to autoconf.h.  When autoconf.h is regenerated (because
# sdkconfig changed), Ninja recompiles every object — giving a true
# "rebuild everything on config change" guarantee.
# ---------------------------------------------------------------------------
add_compile_options(-include ${AUTOCONF_H})

# ---------------------------------------------------------------------------
# Build-time regen: re-run gen_configs.py whenever sdkconfig changes
# (e.g. after running menuconfig) so autoconf.h / sdkconfig.cmake stay in sync.
# All three output files are listed so Ninja tracks them as build products.
# ---------------------------------------------------------------------------
if(_KCONFIGLIB_RC EQUAL 0)
    add_custom_command(
        OUTPUT  ${AUTOCONF_H} ${SDKCONFIG_CMAKE} ${SDKCONFIG_JSON}
        COMMAND ${_KCONFIG_GEN_CMD}
        WORKING_DIRECTORY ${VIBE_RTOS_ROOT}
        DEPENDS ${SDKCONFIG}
        COMMENT "Regenerating Kconfig outputs from sdkconfig"
        VERBATIM
    )
    add_custom_target(kconfig_gen ALL
        DEPENDS ${AUTOCONF_H} ${SDKCONFIG_CMAKE} ${SDKCONFIG_JSON}
        COMMENT "Kconfig outputs up to date"
    )
else()
    # No kconfiglib — create a no-op target so vibe libraries can still
    # add_dependencies(... kconfig_gen) unconditionally.
    add_custom_target(kconfig_gen ALL
        COMMENT "kconfig_gen: kconfiglib unavailable, using fallback autoconf.h"
    )
endif()

# ---------------------------------------------------------------------------
# menuconfig — interactive ncurses-based configuration UI
#
#   ninja menuconfig
#
# After saving and exiting, gen_configs.py is re-run automatically to
# update autoconf.h / sdkconfig.cmake / sdkconfig.json.
# ---------------------------------------------------------------------------
add_custom_target(menuconfig
    COMMAND
        ${CMAKE_COMMAND} -E env
            srctree=${VIBE_RTOS_ROOT}
            KCONFIG_CONFIG=${SDKCONFIG}
        ${Python3_EXECUTABLE} -m menuconfig ${KCONFIG_ROOT}
    COMMAND ${_KCONFIG_GEN_CMD}
    WORKING_DIRECTORY ${VIBE_RTOS_ROOT}
    USES_TERMINAL
    COMMENT "menuconfig — use arrow keys to navigate, S to save, Q to quit"
    VERBATIM
)

# ---------------------------------------------------------------------------
# guiconfig — Tkinter GUI configuration (requires Tkinter)
#
#   ninja guiconfig
# ---------------------------------------------------------------------------
add_custom_target(guiconfig
    COMMAND
        ${CMAKE_COMMAND} -E env
            srctree=${VIBE_RTOS_ROOT}
            KCONFIG_CONFIG=${SDKCONFIG}
        ${Python3_EXECUTABLE} -m guiconfig ${KCONFIG_ROOT}
    COMMAND ${_KCONFIG_GEN_CMD}
    WORKING_DIRECTORY ${VIBE_RTOS_ROOT}
    USES_TERMINAL
    COMMENT "guiconfig — close the window to save and regenerate"
    VERBATIM
)

# ---------------------------------------------------------------------------
# defconfig — reset build sdkconfig to board defaults
#
#   ninja defconfig
# ---------------------------------------------------------------------------
add_custom_target(defconfig
    COMMAND ${CMAKE_COMMAND} -E remove_if_exists ${SDKCONFIG}
    COMMAND ${_KCONFIG_GEN_CMD}
    WORKING_DIRECTORY ${VIBE_RTOS_ROOT}
    COMMENT "defconfig — resetting to board default configuration"
    VERBATIM
)

# ---------------------------------------------------------------------------
# saveconfig — write build sdkconfig back to the board defconfig in source
#
#   ninja saveconfig
# ---------------------------------------------------------------------------
if(DEFINED VIBE_BOARD AND _BOARD_DEFCONFIG)
    add_custom_target(saveconfig
        COMMAND ${CMAKE_COMMAND} -E copy ${SDKCONFIG} ${_BOARD_DEFCONFIG}
        COMMENT "saveconfig — saved ${SDKCONFIG} -> ${_BOARD_DEFCONFIG}"
        VERBATIM
    )
else()
    add_custom_target(saveconfig
        COMMAND ${CMAKE_COMMAND} -E echo
            "saveconfig: VIBE_BOARD not set — pass -DVIBE_BOARD=<board> to cmake"
    )
endif()

# ---------------------------------------------------------------------------
# Fallback: pure-CMake minimal autoconf.h generator (no kconfiglib needed)
# ---------------------------------------------------------------------------
function(_vibe_kconfig_cmake_fallback)
    # Find board defconfig or use built-in defaults
    if(_BOARD_DEFCONFIG AND EXISTS ${_BOARD_DEFCONFIG})
        set(_cfg_src ${_BOARD_DEFCONFIG})
        message(STATUS "Kconfig fallback: using ${_cfg_src}")
    else()
        # Write a hardcoded minimal .config
        set(_cfg_src ${SDKCONFIG})
        file(WRITE ${_cfg_src}
"# VibeRTOS minimal default configuration (cmake fallback)
CONFIG_ARCH_ARM=y
CONFIG_CPU_CORTEX_M0PLUS=y
CONFIG_BOARD_RPI_PICO=y
CONFIG_SOC_RP2040=y
CONFIG_KERNEL_PREEMPT_ENABLED=y
CONFIG_NUM_PRIORITIES=32
CONFIG_TIMESLICE_MS=10
CONFIG_TICKLESS_IDLE=y
CONFIG_SMP=n
CONFIG_HEAP_MEM=y
CONFIG_HEAP_SIZE=4096
CONFIG_STACK_CANARY=y
CONFIG_TLS=y
CONFIG_SCHED_DYNAMIC_PRIORITY=y
CONFIG_SYS_CLOCK_HZ=1000
CONFIG_LOG=n
CONFIG_SHELL=n
CONFIG_UART=y
CONFIG_GPIO=y
")
        message(STATUS "Kconfig fallback: wrote minimal sdkconfig")
    endif()

    # Parse the .config into autoconf.h
    file(STRINGS ${_cfg_src} _lines)
    set(_h "/* Auto-generated by kconfig.cmake (fallback) — do not edit */\n")
    set(_h "${_h}#ifndef VIBE_AUTOCONF_H\n#define VIBE_AUTOCONF_H\n\n")
    foreach(_line ${_lines})
        if(_line MATCHES "^(CONFIG_[A-Za-z0-9_]+)=y$")
            set(_h "${_h}#define ${CMAKE_MATCH_1} 1\n")
        elseif(_line MATCHES "^(CONFIG_[A-Za-z0-9_]+)=n$")
            set(_h "${_h}/* ${CMAKE_MATCH_1} is not set */\n")
        elseif(_line MATCHES "^(CONFIG_[A-Za-z0-9_]+)=\"(.*)\"$")
            set(_h "${_h}#define ${CMAKE_MATCH_1} \"${CMAKE_MATCH_2}\"\n")
        elseif(_line MATCHES "^(CONFIG_[A-Za-z0-9_]+)=([0-9]+)$")
            set(_h "${_h}#define ${CMAKE_MATCH_1} ${CMAKE_MATCH_2}\n")
        elseif(_line MATCHES "^(CONFIG_[A-Za-z0-9_]+)=(0x[0-9A-Fa-f]+)$")
            set(_h "${_h}#define ${CMAKE_MATCH_1} ${CMAKE_MATCH_2}\n")
        endif()
    endforeach()
    set(_h "${_h}\n#endif /* VIBE_AUTOCONF_H */\n")
    file(WRITE ${AUTOCONF_H} ${_h})
    message(STATUS "Kconfig fallback: wrote ${AUTOCONF_H}")
endfunction()
