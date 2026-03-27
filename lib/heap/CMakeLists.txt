cmake_minimum_required(VERSION 3.20)

if(NOT CONFIG_HEAP_MEM)
    return()
endif()

add_library(vibe_heap STATIC)

target_include_directories(vibe_heap PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${VIBE_RTOS_ROOT}/include
    ${VIBE_RTOS_ROOT}/arch/arm/cortex_m/include
    ${CMAKE_BINARY_DIR}/include/generated
)

if(CONFIG_HEAP_ALLOCATOR_TLSF)
    target_sources(vibe_heap PRIVATE heap_tlsf.c)
elseif(CONFIG_HEAP_ALLOCATOR_BEST_FIT)
    target_sources(vibe_heap PRIVATE heap_best_fit.c)
else()
    target_sources(vibe_heap PRIVATE heap_first_fit.c)
endif()
