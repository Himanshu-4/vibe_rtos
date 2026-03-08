cmake_minimum_required(VERSION 3.20)

# Kernel source files
set(KERNEL_SOURCES
    init.c
    sched.c
    thread.c
    timer.c
    mutex.c
    sem.c
    event.c
    msgq.c
    pipe.c
    workq.c
    mem_pool.c
    heap.c
    idle.c
    smp.c
    spinlock.c
)

# Create the kernel static library
add_library(vibe_kernel STATIC ${KERNEL_SOURCES})

target_include_directories(vibe_kernel PUBLIC
    ${VIBE_RTOS_ROOT}/include
    ${CMAKE_BINARY_DIR}/include/generated
)

target_compile_options(vibe_kernel PRIVATE
    -Wall
    -Wextra
    -Wno-unused-parameter
)

# Kernel depends on architecture library for arch_* symbols
# (arch lib links against kernel for kernel_* callbacks — circular resolved at link)
target_link_libraries(vibe_kernel PUBLIC vibe_lib)
