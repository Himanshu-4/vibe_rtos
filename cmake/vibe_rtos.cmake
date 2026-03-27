# vibe_rtos.cmake
# CMake helper for out-of-tree application inheritance.
# Users include this via find_package(vibe_rtos) in their CMakeLists.txt.

cmake_minimum_required(VERSION 3.20)

# Locate the VibeRTOS root (set by vibe_rtos-config.cmake or manually)
if(NOT DEFINED VIBE_RTOS_ROOT)
    get_filename_component(VIBE_RTOS_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
endif()

message(STATUS "VibeRTOS root: ${VIBE_RTOS_ROOT}")

# Add cmake modules
list(APPEND CMAKE_MODULE_PATH ${VIBE_RTOS_ROOT}/cmake/modules)
list(APPEND CMAKE_MODULE_PATH ${VIBE_RTOS_ROOT}/cmake)

# Run Kconfig integration — generates autoconf.h, sdkconfig.cmake, sdkconfig.json
# and registers menuconfig / guiconfig / defconfig / saveconfig targets.
include(kconfig)

# Global include paths
set(VIBE_INCLUDE_DIRS
    ${VIBE_RTOS_ROOT}/include
)

# Toolchain directory
set(VIBE_TOOLCHAIN_DIR ${VIBE_RTOS_ROOT}/cmake/toolchains)

# Board and SoC directories
set(VIBE_BOARDS_DIR   ${VIBE_RTOS_ROOT}/boards)
set(VIBE_SOC_DIR      ${VIBE_RTOS_ROOT}/soc)

# -----------------------------------------------------------------------
# vibe_add_application(name)
#
# Macro to set up a VibeRTOS application target.
# Must be called after project() so that C/ASM languages are enabled.
#
# Usage in an out-of-tree CMakeLists.txt:
#
#   project(my_app C ASM)
#   find_package(vibe_rtos REQUIRED)
#   vibe_add_application(my_app)
#   target_sources(my_app PRIVATE main.c)
#
# -----------------------------------------------------------------------
macro(vibe_add_application APP_NAME)
    # Build VibeRTOS libraries as part of this application build.
    # Always-present targets (lib, kernel, arch) are guarded by TARGET checks.
    # Optional targets (drivers, subsys) may not be created if their CONFIG is
    # disabled, so we use a CACHE variable to prevent duplicate add_subdirectory.
    if(NOT TARGET vibe_lib)
        add_subdirectory(${VIBE_RTOS_ROOT}/lib     ${CMAKE_BINARY_DIR}/_vibe/lib)
    endif()
    if(NOT TARGET vibe_kernel)
        add_subdirectory(${VIBE_RTOS_ROOT}/kernel  ${CMAKE_BINARY_DIR}/_vibe/kernel)
    endif()
    if(NOT TARGET vibe_arch)
        add_subdirectory(${VIBE_RTOS_ROOT}/arch    ${CMAKE_BINARY_DIR}/_vibe/arch)
    endif()
    get_property(_DRIVERS_DIR_ADDED GLOBAL PROPERTY _VIBE_DRIVERS_DIR_ADDED)
    if(NOT _DRIVERS_DIR_ADDED)
        set_property(GLOBAL PROPERTY _VIBE_DRIVERS_DIR_ADDED TRUE)
        add_subdirectory(${VIBE_RTOS_ROOT}/drivers ${CMAKE_BINARY_DIR}/_vibe/drivers)
    endif()
    get_property(_SUBSYS_DIR_ADDED GLOBAL PROPERTY _VIBE_SUBSYS_DIR_ADDED)
    if(NOT _SUBSYS_DIR_ADDED)
        set_property(GLOBAL PROPERTY _VIBE_SUBSYS_DIR_ADDED TRUE)
        add_subdirectory(${VIBE_RTOS_ROOT}/subsys  ${CMAKE_BINARY_DIR}/_vibe/subsys)
    endif()

    # Ensure Kconfig outputs are regenerated before any library or app source
    # is compiled.  kconfig_gen is an ALL target that fires gen_configs.py
    # whenever sdkconfig is newer than autoconf.h / sdkconfig.cmake.
    if(TARGET kconfig_gen)
        add_dependencies(vibe_lib    kconfig_gen)
        add_dependencies(vibe_kernel kconfig_gen)
        add_dependencies(vibe_arch   kconfig_gen)
        foreach(_DEP_TARGET vibe_heap vibe_ring_buffer vibe_job_scheduler
                            vibe_subsys vibe_drivers)
            if(TARGET ${_DEP_TARGET})
                add_dependencies(${_DEP_TARGET} kconfig_gen)
            endif()
        endforeach()
    endif()

    # Create the executable target
    add_executable(${APP_NAME})

    # Add VibeRTOS include directories
    target_include_directories(${APP_NAME} PRIVATE
        ${VIBE_INCLUDE_DIRS}
        ${CMAKE_BINARY_DIR}/include/generated
    )

    # Link kernel + arch always; link optional libraries only if their target
    # was created (i.e. their CONFIG was enabled).  All go inside --start-group
    # so the linker makes multiple passes for any circular references.
    set(_VIBE_LINK_LIBS vibe_kernel vibe_arch vibe_lib)
    foreach(_OPT_LIB vibe_heap vibe_ring_buffer vibe_job_scheduler
                     vibe_subsys vibe_drivers)
        if(TARGET ${_OPT_LIB})
            list(APPEND _VIBE_LINK_LIBS ${_OPT_LIB})
        endif()
    endforeach()

    target_link_libraries(${APP_NAME} PRIVATE
        -Wl,--start-group
        ${_VIBE_LINK_LIBS}
        -Wl,--end-group
    )

    # Compile definitions
    target_compile_definitions(${APP_NAME} PRIVATE
        VIBE_RTOS_VERSION_MAJOR=0
        VIBE_RTOS_VERSION_MINOR=1
        VIBE_RTOS_VERSION_PATCH=0
    )

    # C/CXX standards
    set_target_properties(${APP_NAME} PROPERTIES
        C_STANDARD   11
        CXX_STANDARD 17
    )

    message(STATUS "VibeRTOS application target '${APP_NAME}' configured")
endmacro()

# -----------------------------------------------------------------------
# vibe_set_board(board_name)
#
# Sets the target board. Includes the board's CMakeLists.txt and
# applies its linker script.
# -----------------------------------------------------------------------
macro(vibe_set_board BOARD_NAME)
    if("${BOARD_NAME}" STREQUAL "")
        message(FATAL_ERROR "vibe_set_board: BOARD_NAME is empty. Pass -DVIBE_BOARD=<board> to cmake.")
    endif()

    set(VIBE_BOARD ${BOARD_NAME})
    set(VIBE_BOARD_DIR ${VIBE_BOARDS_DIR}/arm/${BOARD_NAME})

    if(NOT EXISTS ${VIBE_BOARD_DIR})
        message(FATAL_ERROR "Board '${BOARD_NAME}' not found at ${VIBE_BOARD_DIR}")
    endif()

    message(STATUS "VibeRTOS board: ${BOARD_NAME}")
    include(${VIBE_BOARD_DIR}/CMakeLists.txt OPTIONAL)
endmacro()

# -----------------------------------------------------------------------
# vibe_set_linker_script(script_path)
#
# Apply a custom linker script to the application.
# -----------------------------------------------------------------------
macro(vibe_set_linker_script APP_NAME SCRIPT_PATH)
    set_target_properties(${APP_NAME} PROPERTIES
        LINK_FLAGS "-T${SCRIPT_PATH} -Wl,--gc-sections"
    )
endmacro()

# -----------------------------------------------------------------------
# vibe_generate_uf2(TARGET)
#
#   Post-build step: convert TARGET.bin → TARGET.uf2 for drag-and-drop
#   flashing via the BOOTSEL bootloader.
#
#   Requires VIBE_UF2_FAMILY_ID and VIBE_UF2_FLASH_BASE (set by the board
#   CMakeLists.txt, e.g. boards/arm/rpi_pico/CMakeLists.txt).
#   vibe_generate_binary() must be called first so the .bin input exists.
# -----------------------------------------------------------------------
function(vibe_generate_uf2 TARGET)
    if(NOT DEFINED VIBE_UF2_FAMILY_ID)
        message(WARNING "vibe_generate_uf2: VIBE_UF2_FAMILY_ID not set — skipping UF2 generation")
        return()
    endif()
    if(NOT DEFINED VIBE_UF2_FLASH_BASE)
        set(VIBE_UF2_FLASH_BASE "0x10000000")
    endif()

    # Locate Python interpreter (prefer project venv)
    if(EXISTS "${VIBE_RTOS_ROOT}/.venv/bin/python3")
        set(_UF2_PYTHON "${VIBE_RTOS_ROOT}/.venv/bin/python3")
    elseif(DEFINED Python3_EXECUTABLE AND Python3_EXECUTABLE)
        set(_UF2_PYTHON "${Python3_EXECUTABLE}")
    else()
        find_program(_UF2_PYTHON NAMES python3 python REQUIRED)
    endif()

    set(_UF2_SCRIPT "${VIBE_RTOS_ROOT}/scripts/uf2/uf2conv.py")
    # Use CMAKE_CURRENT_BINARY_DIR (resolved at configure time) for paths so
    # generator expressions are not needed in BYPRODUCTS, which CMake evaluates
    # before the build graph is fully wired.
    set(_BIN "${CMAKE_CURRENT_BINARY_DIR}/${TARGET}.bin")
    set(_UF2 "${CMAKE_CURRENT_BINARY_DIR}/${TARGET}.uf2")

    add_custom_command(TARGET ${TARGET} POST_BUILD
        COMMAND ${_UF2_PYTHON} ${_UF2_SCRIPT}
            --bin    ${_BIN}
            --out    ${_UF2}
            --family ${VIBE_UF2_FAMILY_ID}
            --base   ${VIBE_UF2_FLASH_BASE}
        BYPRODUCTS ${_UF2}
        COMMENT "Generating ${TARGET}.uf2 (family=${VIBE_UF2_FAMILY_ID}, base=${VIBE_UF2_FLASH_BASE})"
        VERBATIM
    )
endfunction()

# -----------------------------------------------------------------------
# vibe_flash_uf2(TARGET)
#
#   Adds a "flash_uf2" convenience target: copies the UF2 file to the
#   Pico mass-storage drive that appears when BOOTSEL is held.
#   Drive is auto-detected: macOS → /Volumes/RPI-RP2, Linux → /media/…
# -----------------------------------------------------------------------
function(vibe_flash_uf2 TARGET)
    set(_UF2 "${CMAKE_CURRENT_BINARY_DIR}/${TARGET}.uf2")

    if(APPLE)
        set(_PICO_DRIVE "/Volumes/RPI-RP2")
    else()
        set(_PICO_DRIVE "/media/$ENV{USER}/RPI-RP2")
    endif()

    add_custom_target(flash_uf2
        COMMAND ${CMAKE_COMMAND} -E copy ${_UF2} ${_PICO_DRIVE}/
        DEPENDS ${TARGET}
        COMMENT "Flashing ${TARGET}.uf2 to ${_PICO_DRIVE} (hold BOOTSEL before connecting USB)"
        VERBATIM
    )
endfunction()
