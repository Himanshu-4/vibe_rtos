cmake_minimum_required(VERSION 3.20)

# Tests use the host simulation toolchain
# Run with: cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/toolchains/host-gcc.cmake ..

add_subdirectory(kernel)
