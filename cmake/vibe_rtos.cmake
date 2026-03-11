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
    # Guards ensure each subdirectory is only added once.
    if(NOT TARGET vibe_lib)
        add_subdirectory(${VIBE_RTOS_ROOT}/lib     ${CMAKE_BINARY_DIR}/_vibe/lib)
    endif()
    if(NOT TARGET vibe_kernel)
        add_subdirectory(${VIBE_RTOS_ROOT}/kernel  ${CMAKE_BINARY_DIR}/_vibe/kernel)
    endif()
    if(NOT TARGET vibe_arch)
        add_subdirectory(${VIBE_RTOS_ROOT}/arch    ${CMAKE_BINARY_DIR}/_vibe/arch)
    endif()
    if(NOT TARGET vibe_drivers)
        add_subdirectory(${VIBE_RTOS_ROOT}/drivers ${CMAKE_BINARY_DIR}/_vibe/drivers)
    endif()
    if(NOT TARGET vibe_subsys)
        add_subdirectory(${VIBE_RTOS_ROOT}/subsys  ${CMAKE_BINARY_DIR}/_vibe/subsys)
    endif()

    # Create the executable target
    add_executable(${APP_NAME})

    # Add VibeRTOS include directories
    target_include_directories(${APP_NAME} PRIVATE
        ${VIBE_INCLUDE_DIRS}
        ${CMAKE_BINARY_DIR}/include/generated
    )

    # Link against the kernel and architecture libraries.
    # Order matters for static libs: higher-level libs first.
    target_link_libraries(${APP_NAME} PRIVATE
        vibe_kernel
        vibe_arch
        vibe_subsys
        vibe_drivers
        vibe_lib
        vibe_ring_buffer
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
