# host-gcc.cmake
# CMake toolchain for native host (Linux/macOS) simulation and unit tests.
# Use with: cmake -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/host-gcc.cmake

# Use the host system
set(CMAKE_SYSTEM_NAME ${CMAKE_HOST_SYSTEM_NAME})

# Find host GCC/G++
find_program(CMAKE_C_COMPILER   gcc   DOC "Host GCC")
find_program(CMAKE_CXX_COMPILER g++   DOC "Host G++")
find_program(CMAKE_ASM_COMPILER gcc   DOC "Host GAS")

# Compile flags for simulation/testing
set(VIBE_HOST_FLAGS
    "-Wall"
    "-Wextra"
    "-Wpedantic"
    "-Wno-unused-parameter"
    "-g3"
    "-fsanitize=address,undefined"
    "-fno-omit-frame-pointer"
)

string(JOIN " " VIBE_HOST_FLAGS_STR ${VIBE_HOST_FLAGS})

set(CMAKE_C_FLAGS_INIT   "${VIBE_HOST_FLAGS_STR} -DVIBE_HOST_SIMULATION=1")
set(CMAKE_CXX_FLAGS_INIT "${VIBE_HOST_FLAGS_STR} -DVIBE_HOST_SIMULATION=1")
set(CMAKE_EXE_LINKER_FLAGS_INIT "-fsanitize=address,undefined")

message(STATUS "VibeRTOS host simulation toolchain configured")
