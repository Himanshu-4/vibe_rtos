cmake_minimum_required(VERSION 3.20)

# Out-of-tree application using find_package(vibe_rtos)
# Usage:
#   mkdir build && cd build
#   cmake -DCMAKE_TOOLCHAIN_FILE=../../cmake/toolchains/arm-none-eabi.cmake \
#         -DVIBE_BOARD=rpi_pico ..
#   make

project(hello_world C CXX ASM)

# Make vibe_rtos findable from this directory
list(APPEND CMAKE_PREFIX_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../..)

find_package(vibe_rtos REQUIRED)

# Load YAML config if present
include(yaml_config)
vibe_load_yaml_config(${CMAKE_CURRENT_SOURCE_DIR}/vibe_config.yml)

# Create the application using the VibeRTOS CMake helper
vibe_add_application(hello_world)

target_sources(hello_world PRIVATE main.c)

# Set board (reads linker script, SoC, etc.)
vibe_set_board(rpi_pico)

# Apply linker script
if(DEFINED VIBE_LINKER_SCRIPT)
    vibe_set_linker_script(hello_world ${VIBE_LINKER_SCRIPT})
endif()

# Generate .bin and .hex for flashing
if(CMAKE_CROSSCOMPILING)
    vibe_generate_binary(hello_world)
endif()
