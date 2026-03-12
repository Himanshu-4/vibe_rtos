# boards/arm/rpi_pico2/CMakeLists.txt
# Board support for Raspberry Pi Pico 2 (RP2350, Cortex-M33).

set(VIBE_BOARD     "rpi_pico2"  CACHE STRING "Board name")
set(VIBE_SOC       "rp2350"     CACHE STRING "SoC name")
set(VIBE_SOC_DIR   "${VIBE_RTOS_ROOT}/soc/arm/raspberrypi/rp2350")
set(VIBE_ARCH      "arm/cortex_m/m33")

include(${VIBE_SOC_DIR}/CMakeLists.txt OPTIONAL)

set(VIBE_LINKER_SCRIPT
    "${CMAKE_CURRENT_LIST_DIR}/linker.ld"
    CACHE FILEPATH "Board linker script")

# UF2 parameters for RP2350 ARM bootloader
set(VIBE_UF2_FAMILY_ID "0xe48bff59" CACHE STRING "UF2 family ID")
set(VIBE_UF2_FLASH_BASE "0x10000000" CACHE STRING "Flash base address")

message(STATUS "Board: Raspberry Pi Pico 2 (RP2350)")
