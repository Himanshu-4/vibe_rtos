# soc/arm/raspberrypi/rp2040/CMakeLists.txt
# RP2040 SoC support package.
#
# TODO (USER): Add RP2040 SoC clock init, memory map, peripheral base addresses.
# When ready, add soc.c to vibe_arch target:
#
#   target_sources(vibe_arch PRIVATE soc.c)
#
# Set RP2040-specific compiler flags:
set(VIBE_CPU_FLAGS "-mcpu=cortex-m0plus -mfloat-abi=soft" CACHE STRING "" FORCE)

message(STATUS "SoC: RP2040 (Cortex-M0+, 264KB SRAM, 2MB Flash)")
