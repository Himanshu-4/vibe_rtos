cmake_minimum_required(VERSION 3.20)

# VibeRTOS async_pipeline Sample
#
# Demonstrates a three-stage data pipeline built with the job scheduler:
#
#   sampler_job  →  [raw ring buffer]  →  processor_job
#                                          →  [processed ring buffer]
#                                              →  printer_job
#
# Shows:
#   - ASYNC_AWAIT on data availability  (wait for bytes in a ring buffer)
#   - ASYNC_AWAIT on a tick deadline    (rate-limit the sampler)
#   - Ring buffer as a lock-free inter-job channel (no RTOS primitives needed)
#   - Three-stage cooperative pipeline within ONE RTOS task
#   - CONFIG_RING_BUFFER_STATS usage
#
# Build:
#   cmake -DVIBE_BOARD=rpi_pico  -B build .
#   cmake -DVIBE_BOARD=rpi_pico2 -B build .

set(VIBE_BOARD "rpi_pico2" CACHE STRING "Target board name")
include("${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/vibe_board_toolchain.cmake")

project(async_pipeline C ASM)

list(APPEND CMAKE_PREFIX_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../..)
find_package(vibe_rtos REQUIRED)

vibe_add_application(async_pipeline)
target_sources(async_pipeline PRIVATE main.c)

vibe_set_board(${VIBE_BOARD})

if(DEFINED VIBE_LINKER_SCRIPT)
    vibe_set_linker_script(async_pipeline ${VIBE_LINKER_SCRIPT})
endif()

if(CMAKE_CROSSCOMPILING)
    vibe_generate_binary(async_pipeline)
    vibe_generate_uf2(async_pipeline)
    vibe_flash_uf2(async_pipeline)
endif()
