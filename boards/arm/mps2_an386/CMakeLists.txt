# boards/arm/mps2_an386/CMakeLists.txt
# Board support for ARM MPS2 AN386 (Cortex-M4) — the QEMU emulation target.
#
# Run with:  qemu-system-arm -M mps2-an386 -nographic -semihosting -kernel app.elf
# See scripts/emulation/README.md for the full emulation workflow.

set(VIBE_BOARD     "mps2_an386" CACHE STRING "Board name")
set(VIBE_SOC       "mps2_an386" CACHE STRING "SoC name")
set(VIBE_SOC_DIR   "${VIBE_RTOS_ROOT}/soc/arm/arm/mps2_an386")
set(VIBE_ARCH      "arm/cortex_m/m4")

# Include SoC CMake (sets VIBE_CPU_FLAGS and VIBE_SOC_SOURCES)
include(${VIBE_SOC_DIR}/CMakeLists.txt OPTIONAL)

# Linker script
set(VIBE_LINKER_SCRIPT
    "${CMAKE_CURRENT_LIST_DIR}/linker.ld"
    CACHE FILEPATH "Board linker script")

# No UF2 bootloader on MPS2 — QEMU loads the ELF directly.

message(STATUS "Board: ARM MPS2 AN386 (Cortex-M4, QEMU emulation)")
