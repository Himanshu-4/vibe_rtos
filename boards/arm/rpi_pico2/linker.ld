/**
 * linker.ld — Linker script for Raspberry Pi Pico 2 (RP2350, Cortex-M33)
 *
 * Memory:
 *   FLASH: 2MB at 0x10000000
 *   RAM:   520KB at 0x20000000
 */

MEMORY
{
    FLASH (rx)  : ORIGIN = 0x10000000, LENGTH = 2048K
    RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 512K
    SRAM_EXT (rwx) : ORIGIN = 0x20080000, LENGTH = 8K   /* Extra SRAM banks */
}

HEAP_SIZE  = 0x4000;   /* 16KB heap */
STACK_SIZE = 0x2000;   /* 8KB main stack */

ENTRY(Reset_Handler)

SECTIONS
{
    .isr_vector :
    {
        KEEP(*(.isr_vector))
        . = ALIGN(4);
    } >FLASH

    .text :
    {
        *(.text)
        *(.text.*)
        *(.rodata)
        *(.rodata.*)
        KEEP(*(.init))
        KEEP(*(.fini))
        . = ALIGN(4);
        _etext = .;
    } >FLASH

    ._vibe_devices :
    {
        _vibe_devices_start = .;
        KEEP(*(._vibe_devices.*))
        KEEP(*(._vibe_devices))
        _vibe_devices_end = .;
        . = ALIGN(4);
    } >FLASH

    ._vibe_log_modules :
    {
        _vibe_log_modules_start = .;
        KEEP(*(._vibe_log_modules))
        _vibe_log_modules_end = .;
        . = ALIGN(4);
    } >FLASH

    ._vibe_shell_cmds :
    {
        _vibe_shell_cmds_start = .;
        KEEP(*(._vibe_shell_cmds))
        _vibe_shell_cmds_end = .;
        . = ALIGN(4);
    } >FLASH

    .ARM.extab : { *(.ARM.extab*) } >FLASH
    .ARM :
    {
        __exidx_start = .;
        *(.ARM.exidx*)
        __exidx_end = .;
    } >FLASH

    _sidata = LOADADDR(.data);

    .data :
    {
        _sdata = .;
        *(.data)
        *(.data.*)
        . = ALIGN(4);
        _edata = .;
    } >RAM AT>FLASH

    .bss (NOLOAD) :
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

    .heap :
    {
        __heap_start = .;
        . += HEAP_SIZE;
        __heap_end = .;
        . = ALIGN(8);
    } >RAM

    .stack (NOLOAD) :
    {
        . = ALIGN(8);
        __StackBottom = .;
        . += STACK_SIZE;
        __StackTop = .;
    } >RAM

    ASSERT(__StackTop <= ORIGIN(RAM) + LENGTH(RAM),
           "ERROR: Stack overflows RAM")
}
