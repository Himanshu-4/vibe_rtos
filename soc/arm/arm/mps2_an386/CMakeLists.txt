# soc/arm/arm/mps2_an386/CMakeLists.txt
# ARM MPS2 AN386 (Cortex-M4) SoC support package — QEMU emulation target.

# MPS2 AN386-specific compiler flags (soft-float keeps context switching
# simple; enable hard-float once FPU state save/restore is implemented).
set(VIBE_CPU_FLAGS "-mcpu=cortex-m4 -mfloat-abi=soft" CACHE STRING "" FORCE)

# SoC sources that the application must link directly (NOT via a static
# library archive) so that strong overrides of weak symbols — SystemInit,
# vibe_console_putc — are guaranteed to be pulled into the link.
set(VIBE_SOC_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/soc.c"
    CACHE INTERNAL "SoC source files linked directly into the application")

message(STATUS "SoC: ARM MPS2 AN386 (Cortex-M4, 25 MHz, QEMU mps2-an386)")
