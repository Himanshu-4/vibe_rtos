cmake_minimum_required(VERSION 3.20)

# Subsystem sources — always compiled; individual files guard features with
# weak symbols so the binary links even when CONFIG_* is not set.
set(SUBSYS_SOURCES
    logging/log.c
    shell/shell.c
)

add_library(vibe_subsys STATIC ${SUBSYS_SOURCES})

target_include_directories(vibe_subsys PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/shell
    ${CMAKE_CURRENT_SOURCE_DIR}/logging
    ${VIBE_RTOS_ROOT}/include
    ${VIBE_RTOS_ROOT}/arch/arm/cortex_m/include
    ${CMAKE_BINARY_DIR}/include/generated
)

# Warning flags are set globally by the toolchain file.

# Coredump subsystem — sources conditionally added by its own CMakeLists.txt
add_subdirectory(coredump)
