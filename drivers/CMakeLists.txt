cmake_minimum_required(VERSION 3.20)

set(_DRIVER_SRCS "")

add_subdirectory(gpio)
add_subdirectory(uart)
add_subdirectory(spi)
add_subdirectory(i2c)
add_subdirectory(timer)
add_subdirectory(adc)
add_subdirectory(flash)
add_subdirectory(rtc)
add_subdirectory(watchdog)

if(NOT _DRIVER_SRCS)
    return()
endif()

add_library(vibe_drivers STATIC ${_DRIVER_SRCS})

target_include_directories(vibe_drivers PUBLIC
    ${VIBE_RTOS_ROOT}/include
    ${VIBE_RTOS_ROOT}/include/drivers
    ${CMAKE_BINARY_DIR}/include/generated
)
