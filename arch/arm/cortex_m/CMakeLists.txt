cmake_minimum_required(VERSION 3.20)

set(CORTEX_M_SOURCES
    core/startup.S
    core/context_switch.S
    core/irq.c
    core/mpu.c
)

add_library(vibe_arch STATIC ${CORTEX_M_SOURCES})

target_include_directories(vibe_arch PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${VIBE_RTOS_ROOT}/include
    ${CMAKE_BINARY_DIR}/include/generated
)

target_compile_options(vibe_arch PRIVATE
    -Wall
    -Wextra
    -Wno-unused-parameter
)
