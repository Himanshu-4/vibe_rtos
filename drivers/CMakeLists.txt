cmake_minimum_required(VERSION 3.20)

# Add each driver subdirectory.
# Each driver CMakeLists.txt guards its sources with an appropriate CONFIG_ check.

add_subdirectory(gpio)
add_subdirectory(uart)
add_subdirectory(spi)
add_subdirectory(i2c)
add_subdirectory(timer)
add_subdirectory(adc)
add_subdirectory(flash)
add_subdirectory(rtc)
add_subdirectory(watchdog)

# Create an interface library that bundles all driver includes.
# The kernel and applications link against this for driver headers.
add_library(vibe_drivers INTERFACE)

target_include_directories(vibe_drivers INTERFACE
    ${VIBE_RTOS_ROOT}/include
)
