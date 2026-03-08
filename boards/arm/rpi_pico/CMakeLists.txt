# boards/arm/rpi_pico/CMakeLists.txt
# Board support for Raspberry Pi Pico (RP2040, Cortex-M0+).

set(VIBE_BOARD     "rpi_pico"   CACHE STRING "Board name")
set(VIBE_SOC       "rp2040"     CACHE STRING "SoC name")
set(VIBE_SOC_DIR   "${VIBE_RTOS_ROOT}/soc/arm/raspberrypi/rp2040")
set(VIBE_ARCH      "arm/cortex_m/m0plus")

# Include SoC CMake
include(${VIBE_SOC_DIR}/CMakeLists.txt OPTIONAL)

# Linker script
set(VIBE_LINKER_SCRIPT
    "${CMAKE_CURRENT_LIST_DIR}/linker.ld"
    CACHE FILEPATH "Board linker script")

message(STATUS "Board: Raspberry Pi Pico (RP2040)")
