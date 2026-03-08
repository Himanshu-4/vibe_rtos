cmake_minimum_required(VERSION 3.20)

# Host simulation kernel tests
# These run on the development machine, not on the target hardware.
# Compile with: -DVIBE_HOST_SIMULATION=1

set(TEST_SOURCES
    test_sched.c
    test_thread.c
    test_sync.c
)

add_executable(vibe_kernel_tests ${TEST_SOURCES})

target_include_directories(vibe_kernel_tests PRIVATE
    ${VIBE_RTOS_ROOT}/include
    ${CMAKE_BINARY_DIR}/include/generated
)

target_compile_definitions(vibe_kernel_tests PRIVATE
    VIBE_HOST_SIMULATION=1
    CONFIG_NUM_PRIORITIES=32
    CONFIG_SYS_CLOCK_HZ=1000
    CONFIG_HEAP_MEM=1
    CONFIG_HEAP_SIZE=65536
    CONFIG_STACK_CANARY=1
)

target_link_libraries(vibe_kernel_tests PRIVATE vibe_kernel vibe_lib)

add_test(NAME kernel_tests COMMAND vibe_kernel_tests)
