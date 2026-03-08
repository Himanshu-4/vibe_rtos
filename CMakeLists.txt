cmake_minimum_required(VERSION 3.20)

project(vibe_rtos
    VERSION 0.1.0
    LANGUAGES C CXX ASM
    DESCRIPTION "VibeRTOS - A real-time operating system for ARM Cortex-M"
)

# Language standards
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Project root
set(VIBE_RTOS_ROOT ${CMAKE_CURRENT_SOURCE_DIR} CACHE PATH "VibeRTOS root directory")

# Add cmake module path
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

# Include Kconfig integration
include(kconfig)

# Include DTS processing
include(dts)

# Global include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_BINARY_DIR}/include/generated
)

# Build configuration
option(VIBE_BUILD_TESTS    "Build kernel and driver tests"   OFF)
option(VIBE_BUILD_SAMPLES  "Build sample applications"       OFF)

# Add subdirectories
add_subdirectory(arch)
add_subdirectory(kernel)
add_subdirectory(drivers)
add_subdirectory(subsys)
add_subdirectory(lib)

if(VIBE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(VIBE_BUILD_SAMPLES)
    add_subdirectory(samples)
endif()

# Export package so out-of-tree apps can find_package(vibe_rtos)
export(PACKAGE vibe_rtos)

# Install cmake helper files
install(FILES
    cmake/vibe_rtos.cmake
    cmake/vibe_rtos-config.cmake
    DESTINATION lib/cmake/vibe_rtos
)

install(DIRECTORY include/
    DESTINATION include
)
