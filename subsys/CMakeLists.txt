cmake_minimum_required(VERSION 3.20)

add_subdirectory(shell)
add_subdirectory(logging)

# Create interface library for subsystems
add_library(vibe_subsys INTERFACE)

target_include_directories(vibe_subsys INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${VIBE_RTOS_ROOT}/include
)
