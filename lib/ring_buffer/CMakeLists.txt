add_library(vibe_ring_buffer STATIC)

target_include_directories(vibe_ring_buffer PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${VIBE_RTOS_ROOT}/include
    ${CMAKE_BINARY_DIR}/include/generated
)

if(CONFIG_RING_BUFFER)
    target_sources(vibe_ring_buffer PRIVATE ring_buffer.c)
else()
    target_sources(vibe_ring_buffer PRIVATE ring_buffer_stub.c)
endif()
