cmake_minimum_required(VERSION 3.20)

# VibeRTOS async_state_machine Sample
#
# Demonstrates coroutine composition using ASYNC_AWAIT_CORO:
#
#   - A boot sequencer runs three initialisation sub-coroutines in order,
#     waiting for each to complete before starting the next.
#   - After boot, transitions to an operational state that runs two
#     concurrent sub-coroutines (heartbeat + sensor poll) in a loop.
#   - A fault injection path shows ASYNC_RETURN (early exit) and how the
#     top-level state machine restarts the boot sequence.
#
# Shows:
#   - ASYNC_AWAIT_CORO  — sequential coroutine composition
#   - ASYNC_YIELD       — cooperative multi-step progress
#   - ASYNC_RETURN      — early exit from a coroutine
#   - async_init()      — resetting a child coroutine for re-use
#   - vibe_sched_cancel — removing a job from the scheduler
#
# Build:
#   cmake -DVIBE_BOARD=rpi_pico  -B build .
#   cmake -DVIBE_BOARD=rpi_pico2 -B build .

set(VIBE_BOARD "rpi_pico2" CACHE STRING "Target board name")
include("${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/vibe_board_toolchain.cmake")

project(async_state_machine C ASM)

list(APPEND CMAKE_PREFIX_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../..)
find_package(vibe_rtos REQUIRED)

vibe_add_application(async_state_machine)
target_sources(async_state_machine PRIVATE main.c)

vibe_set_board(${VIBE_BOARD})

if(DEFINED VIBE_LINKER_SCRIPT)
    vibe_set_linker_script(async_state_machine ${VIBE_LINKER_SCRIPT})
endif()

if(CMAKE_CROSSCOMPILING)
    vibe_generate_binary(async_state_machine)
    vibe_generate_uf2(async_state_machine)
    vibe_flash_uf2(async_state_machine)
endif()
