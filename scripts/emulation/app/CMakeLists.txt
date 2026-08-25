cmake_minimum_required(VERSION 3.20)

# QEMU emulation test application for VibeRTOS.
# Target board: mps2_an386 (ARM MPS2 AN386, Cortex-M4 — QEMU `mps2-an386`).
#
# Build via scripts/emulation/build.sh, or manually:
#   cmake -S scripts/emulation/app -B build/mps2_an386/emu_test \
#         -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/arm-none-eabi.cmake \
#         -DCMAKE_PREFIX_PATH=$PWD -DVIBE_BOARD=mps2_an386
#   make -C build/mps2_an386/emu_test

project(emu_test C CXX ASM)

# Make vibe_rtos findable from this directory
list(APPEND CMAKE_PREFIX_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../../..)

find_package(vibe_rtos REQUIRED)

# Load YAML config if present
include(yaml_config)
vibe_load_yaml_config(${CMAKE_CURRENT_SOURCE_DIR}/vibe_config.yml)

# Create the application using the VibeRTOS CMake helper
vibe_add_application(emu_test)

target_sources(emu_test PRIVATE main.c)

# Set board (reads linker script, SoC, etc.)
vibe_set_board(mps2_an386)

# Link the SoC support sources (SystemInit, console putc, semihosting exit)
# directly into the application so their strong symbols always override the
# weak defaults in vibe_arch / vibe_kernel.
if(DEFINED VIBE_SOC_SOURCES)
    target_sources(emu_test PRIVATE ${VIBE_SOC_SOURCES})
endif()
target_include_directories(emu_test PRIVATE ${VIBE_SOC_DIR})

# Pass -DEMU_CONTINUE_BOOT=ON to keep booting into the scheduler after the
# self-test suite instead of exiting QEMU via semihosting.
option(EMU_CONTINUE_BOOT "Continue kernel boot after emulation tests" OFF)
if(EMU_CONTINUE_BOOT)
    target_compile_definitions(emu_test PRIVATE EMU_CONTINUE_BOOT=1)
endif()

# Apply linker script
if(DEFINED VIBE_LINKER_SCRIPT)
    vibe_set_linker_script(emu_test ${VIBE_LINKER_SCRIPT})
endif()

# Generate .bin and .hex (QEMU loads the ELF directly, but these are handy)
if(CMAKE_CROSSCOMPILING)
    vibe_generate_binary(emu_test)
endif()
