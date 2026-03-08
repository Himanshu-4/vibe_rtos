add_library(vibe_ring_buffer STATIC ring_buffer.c)
target_include_directories(vibe_ring_buffer PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${VIBE_RTOS_ROOT}/include
)
