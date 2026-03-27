cmake_minimum_required(VERSION 3.20)

add_subdirectory(ring_buffer)
add_subdirectory(heap)
add_subdirectory(job_scheduler)

add_library(vibe_lib INTERFACE)

target_include_directories(vibe_lib INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}         # lib/ root — exposes async/ and job_scheduler/ as subdirs
    ${VIBE_RTOS_ROOT}/include
)

if(TARGET vibe_ring_buffer)
    target_link_libraries(vibe_lib INTERFACE vibe_ring_buffer)
endif()
