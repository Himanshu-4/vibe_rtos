cmake_minimum_required(VERSION 3.20)

add_subdirectory(ring_buffer)

add_library(vibe_lib INTERFACE)

target_include_directories(vibe_lib INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${VIBE_RTOS_ROOT}/include
)

target_link_libraries(vibe_lib INTERFACE vibe_ring_buffer)
