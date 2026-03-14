cmake_minimum_required(VERSION 3.20)

# CMake-level dependency guard: Kconfig enforces this via "depends on ASYNC",
# but a hard error here catches misconfigured sdkconfig.cmake edge cases.
if(CONFIG_JOB_SCHEDULER AND NOT CONFIG_ASYNC)
    message(FATAL_ERROR
        "CONFIG_JOB_SCHEDULER=y requires CONFIG_ASYNC=y. "
        "Enable via menuconfig → Libraries → Async / Coroutines.")
endif()

add_library(vibe_job_scheduler STATIC)

target_include_directories(vibe_job_scheduler
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}         # job_scheduler.h  → #include "job_scheduler.h"
        ${CMAKE_CURRENT_SOURCE_DIR}/../async # async.h          → #include "async.h"
        ${CMAKE_BINARY_DIR}/include/generated
    PRIVATE
        ${VIBE_RTOS_ROOT}/include
)

if(CONFIG_JOB_SCHEDULER)
    target_sources(vibe_job_scheduler PRIVATE job_scheduler.c)
else()
    target_sources(vibe_job_scheduler PRIVATE job_scheduler_stub.c)
endif()
