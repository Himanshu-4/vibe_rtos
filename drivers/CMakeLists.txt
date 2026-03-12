cmake_minimum_required(VERSION 3.20)

# Create the static library FIRST so subdirectories can add sources to it.
# drivers_dummy.c ensures the lib is never empty (which would be a linker error).
add_library(vibe_drivers STATIC drivers_dummy.c)

target_include_directories(vibe_drivers PUBLIC
    ${VIBE_RTOS_ROOT}/include
    ${VIBE_RTOS_ROOT}/include/drivers
)

# Warning flags are set globally by the toolchain file.

# Add driver subdirectories — each appends its sources via target_sources PRIVATE
add_subdirectory(gpio)
add_subdirectory(uart)
add_subdirectory(spi)
add_subdirectory(i2c)
add_subdirectory(timer)
add_subdirectory(adc)
add_subdirectory(flash)
add_subdirectory(rtc)
add_subdirectory(watchdog)
