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

# UF2 parameters for RP2040 bootloader
set(VIBE_UF2_FAMILY_ID "0xe48bff56" CACHE STRING "UF2 family ID")
set(VIBE_UF2_FLASH_BASE "0x10000000" CACHE STRING "Flash base address")

message(STATUS "Board: Raspberry Pi Pico (RP2040)")
