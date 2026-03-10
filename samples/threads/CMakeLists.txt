cmake_minimum_required(VERSION 3.20)

# VibeRTOS Threads + Message Queue Sample
#
# Demonstrates a producer/consumer pair communicating through a message
# queue.  The producer sends timestamped messages every 200 ms; the
# consumer blocks indefinitely waiting for them.
#
# Build:
#   mkdir build && cd build
#   cmake -DCMAKE_TOOLCHAIN_FILE=../../cmake/toolchains/arm-none-eabi.cmake \
#         -DVIBE_BOARD=rpi_pico ..
#   make
#
# Flash:
#   ../../scripts/flash.sh --board rpi_pico --elf threads

project(threads C ASM)

# Make vibe_rtos findable from this directory
list(APPEND CMAKE_PREFIX_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../..)

find_package(vibe_rtos REQUIRED)

# Default board if not provided via -DVIBE_BOARD=<board>
if(NOT DEFINED VIBE_BOARD OR VIBE_BOARD STREQUAL "")
    set(VIBE_BOARD "rpi_pico" CACHE STRING "Target board (e.g. rpi_pico, rpi_pico2)")
    message(STATUS "VIBE_BOARD not set, defaulting to '${VIBE_BOARD}'")
endif()

# Create the application using the VibeRTOS CMake helper
vibe_add_application(threads)

target_sources(threads PRIVATE main.c)

# Set board (configures SoC headers, linker script path, CPU flags)
vibe_set_board(${VIBE_BOARD})

# Apply the board linker script
if(DEFINED VIBE_LINKER_SCRIPT)
    vibe_set_linker_script(threads ${VIBE_LINKER_SCRIPT})
endif()

# Generate .bin and .hex for flashing when cross-compiling
if(CMAKE_CROSSCOMPILING)
    vibe_generate_binary(threads)
endif()
