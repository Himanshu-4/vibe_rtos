cmake_minimum_required(VERSION 3.20)

if(NOT CONFIG_JOB_SCHEDULER)
    return()
endif()

if(NOT CONFIG_ASYNC)
    message(FATAL_ERROR
        "CONFIG_JOB_SCHEDULER=y requires CONFIG_ASYNC=y. "
        "Enable via menuconfig → Libraries → Async / Coroutines.")
endif()

add_library(vibe_job_scheduler STATIC job_scheduler.c)

target_include_directories(vibe_job_scheduler
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}          # job_scheduler.h
        ${CMAKE_CURRENT_SOURCE_DIR}/../async # async.h
        ${CMAKE_BINARY_DIR}/include/generated
    PRIVATE
        ${VIBE_RTOS_ROOT}/include
)
