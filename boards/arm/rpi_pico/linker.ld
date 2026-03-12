/**
 * linker.ld — Linker script for Raspberry Pi Pico (RP2040)
 *
 * Memory layout:
 *   FLASH: 2MB at 0x10000000 (XIP — execute in place)
 *   RAM:   256KB at 0x20000000
 *
 * USER: Adjust HEAP_SIZE and STACK_SIZE to match your application.
 *       If using a boot2 (second-stage bootloader), reserve 256 bytes
 *       at the start of flash: BOOT2 origin=0x10000000 LENGTH=256.
 */

MEMORY
{
    /* External QSPI flash, execute-in-place */
    FLASH (rx)  : ORIGIN = 0x10000000, LENGTH = 2048K

    /* On-chip SRAM (striped across 4 banks for best throughput) */
    RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 256K

    /* SRAM banks 4 and 5 — available for stack or device data */
    SRAM4 (rwx) : ORIGIN = 0x20040000, LENGTH = 4K
    SRAM5 (rwx) : ORIGIN = 0x20041000, LENGTH = 4K
}

HEAP_SIZE  = 0x2000;   /* 8KB heap — adjust in vibe_config.yml */
STACK_SIZE = 0x1000;   /* 4KB main stack */

ENTRY(Reset_Handler)

SECTIONS
{
    /* ---------------------------------------------------------------
     * Vector table must be at the start of flash.
     * ------------------------------------------------------------- */
    .isr_vector :
    {
        KEEP(*(.isr_vector))
        . = ALIGN(4);
    } >FLASH

    /* ---------------------------------------------------------------
     * Code and read-only data
     * ------------------------------------------------------------- */
    .text :
    {
        *(.text)
        *(.text.*)
        *(.rodata)
        *(.rodata.*)

        /* Keep C++ constructors and destructors */
        KEEP(*(.init))
        KEEP(*(.fini))

        . = ALIGN(4);
        _etext = .;
    } >FLASH

    /* ---------------------------------------------------------------
     * VibeRTOS device registry (linker section trick)
     * Devices registered with VIBE_DEVICE_DEFINE are placed here.
     * ------------------------------------------------------------- */
    ._vibe_devices :
    {
        _vibe_devices_start = .;
        KEEP(*(._vibe_devices.*))
        KEEP(*(._vibe_devices))
        _vibe_devices_end = .;
        . = ALIGN(4);
    } >FLASH

    /* ---------------------------------------------------------------
     * Log module registry
     * ------------------------------------------------------------- */
    ._vibe_log_modules :
    {
        _vibe_log_modules_start = .;
        KEEP(*(._vibe_log_modules))
        _vibe_log_modules_end = .;
        . = ALIGN(4);
    } >FLASH

    /* ---------------------------------------------------------------
     * Shell command registry
     * ------------------------------------------------------------- */
    ._vibe_shell_cmds :
    {
        _vibe_shell_cmds_start = .;
        KEEP(*(._vibe_shell_cmds))
        _vibe_shell_cmds_end = .;
        . = ALIGN(4);
    } >FLASH

    /* ---------------------------------------------------------------
     * ARM exception handling tables (C++ exceptions, unwinding)
     * ------------------------------------------------------------- */
    .ARM.extab :
    {
        *(.ARM.extab* .gnu.linkonce.armextab.*)
    } >FLASH

    .ARM :
    {
        __exidx_start = .;
        *(.ARM.exidx*)
        __exidx_end = .;
    } >FLASH

    /* ---------------------------------------------------------------
     * Initialized data — stored in flash, copied to RAM at startup
     * ------------------------------------------------------------- */
    _sidata = LOADADDR(.data);   /* Flash (LMA) address of .data */

    .data :
    {
        _sdata = .;
        *(.data)
        *(.data.*)
        . = ALIGN(4);
        _edata = .;
    } >RAM AT>FLASH

    /* ---------------------------------------------------------------
     * Uninitialized data — zeroed by startup code
     * ------------------------------------------------------------- */
    .bss :
    {
        _sbss = .;
        *(.bss)
        *(.bss.*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
    } >RAM

    /* ---------------------------------------------------------------
     * No-init region — NOT zeroed by startup code, survives warm reset.
     * Used by the coredump subsystem to persist crash records across reboot.
     * ------------------------------------------------------------- */
    .noinit (NOLOAD) :
    {
        . = ALIGN(4);
        _snoinit = .;
        *(.noinit)
        *(.noinit.*)
        . = ALIGN(4);
        _enoinit = .;
    } >RAM

    /* ---------------------------------------------------------------
     * Heap — grows upward from end of BSS
     * ------------------------------------------------------------- */
    .heap :
    {
        __heap_start = .;
        . += HEAP_SIZE;
        __heap_end = .;
        . = ALIGN(8);
    } >RAM

    /* ---------------------------------------------------------------
     * Stack — grows downward from top of RAM
     * ------------------------------------------------------------- */
    .stack (NOLOAD) :
    {
        . = ALIGN(8);
        __StackBottom = .;
        . += STACK_SIZE;
        __StackTop = .;
    } >RAM

    /* ---------------------------------------------------------------
     * Validate that everything fits
     * ------------------------------------------------------------- */
    ASSERT(__StackTop <= ORIGIN(RAM) + LENGTH(RAM),
           "ERROR: Stack overflows RAM — reduce STACK_SIZE or HEAP_SIZE")
}
