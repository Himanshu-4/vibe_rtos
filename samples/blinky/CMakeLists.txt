cmake_minimum_required(VERSION 3.20)

# VibeRTOS Blinky Sample
#
# Blinks the on-board LED (GPIO 25) on the Raspberry Pi Pico / Pico 2.
#
# Build (toolchain auto-selected from VIBE_BOARD):
#   cmake -DVIBE_BOARD=rpi_pico  -B build .
#   cmake -DVIBE_BOARD=rpi_pico2 -B build .
#
# Or pass the toolchain explicitly (overrides auto-selection):
#   cmake -DCMAKE_TOOLCHAIN_FILE=../../cmake/toolchains/arm-none-eabi.cmake \
#         -DVIBE_BOARD=rpi_pico -B build .
#
# Flash:
#   ../../scripts/flash.sh --board rpi_pico --elf blinky

# --- Board and toolchain: MUST come before project() ---
set(VIBE_BOARD "rpi_pico" CACHE STRING "Target board name")
include("${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/vibe_board_toolchain.cmake")

project(blinky C ASM)

# Make vibe_rtos findable from this directory
list(APPEND CMAKE_PREFIX_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../..)

find_package(vibe_rtos REQUIRED)

# Create the application using the VibeRTOS CMake helper
vibe_add_application(blinky)

target_sources(blinky PRIVATE main.c)

# Set board (configures SoC headers, linker script path, CPU flags)
vibe_set_board(${VIBE_BOARD})

# Apply the board linker script
if(DEFINED VIBE_LINKER_SCRIPT)
    vibe_set_linker_script(blinky ${VIBE_LINKER_SCRIPT})
endif()

# Generate .bin and .hex for flashing when cross-compiling
if(CMAKE_CROSSCOMPILING)
    vibe_generate_binary(blinky)
endif()
