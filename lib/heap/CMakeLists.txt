cmake_minimum_required(VERSION 3.20)

add_library(vibe_heap STATIC)

target_include_directories(vibe_heap PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${VIBE_RTOS_ROOT}/include
    ${VIBE_RTOS_ROOT}/arch/arm/cortex_m/include
    ${CMAKE_BINARY_DIR}/include/generated
)

# Warning flags are set globally by the toolchain file.

if(CONFIG_HEAP_MEM)
    if(CONFIG_HEAP_ALLOCATOR_TLSF)
        target_sources(vibe_heap PRIVATE heap_tlsf.c)
    elseif(CONFIG_HEAP_ALLOCATOR_BEST_FIT)
        target_sources(vibe_heap PRIVATE heap_best_fit.c)
    else()
        # Default / FIRST_FIT
        target_sources(vibe_heap PRIVATE heap_first_fit.c)
    endif()
else()
    # Heap disabled — empty library so targets that link it still build
    target_sources(vibe_heap PRIVATE heap_stub.c)
endif()
