# soc/arm/raspberrypi/rp2350/CMakeLists.txt
# RP2350 SoC support package (Raspberry Pi Pico 2).
#
# TODO (USER): Add RP2350 SoC clock init, memory map, peripheral base addresses.

set(VIBE_CPU_FLAGS "-mcpu=cortex-m33 -mfpu=fpv5-sp-d16 -mfloat-abi=hard" CACHE STRING "" FORCE)

message(STATUS "SoC: RP2350 (Cortex-M33 x2 + RISC-V x2, 520KB SRAM, 2MB Flash)")
