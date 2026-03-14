cmake_minimum_required(VERSION 3.20)

# VibeRTOS async_hello Sample
#
# Demonstrates the async/job-scheduler library basics:
#   - Scheduling multiple coroutines inside one RTOS task
#   - ASYNC_YIELD  — periodic work (LED blink, log counter)
#   - ASYNC_AWAIT  — time-gated wake-up without blocking the RTOS
#   - Priority     — higher-priority jobs pre-empt lower-priority peers
#   - Stats        — optional scheduler diagnostics (CONFIG_JOB_SCHEDULER_STATS)
#
# Build:
#   cmake -DVIBE_BOARD=rpi_pico  -B build .
#   cmake -DVIBE_BOARD=rpi_pico2 -B build .

set(VIBE_BOARD "rpi_pico2" CACHE STRING "Target board name")
include("${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/vibe_board_toolchain.cmake")

project(async_hello C ASM)

list(APPEND CMAKE_PREFIX_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../..)
find_package(vibe_rtos REQUIRED)

vibe_add_application(async_hello)
target_sources(async_hello PRIVATE main.c)

vibe_set_board(${VIBE_BOARD})

if(DEFINED VIBE_LINKER_SCRIPT)
    vibe_set_linker_script(async_hello ${VIBE_LINKER_SCRIPT})
endif()

if(CMAKE_CROSSCOMPILING)
    vibe_generate_binary(async_hello)
    vibe_generate_uf2(async_hello)
    vibe_flash_uf2(async_hello)
endif()
