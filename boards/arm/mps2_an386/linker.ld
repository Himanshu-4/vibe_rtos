/**
 * linker.ld — Linker script for ARM MPS2 AN386 (QEMU emulation target)
 *
 * Memory layout (QEMU machine `mps2-an386`):
 *   FLASH: 4MB ZBT SSRAM1 at 0x00000000 (code — the vector table must be
 *          here; the Cortex-M4 fetches SP/PC from address 0 at reset)
 *   RAM:   4MB ZBT SSRAM2/3 at 0x20000000 (data)
 *
 * There is no real flash: QEMU loads the ELF segments directly into the
 * SRAMs, so .data still uses the standard LMA-in-"FLASH" scheme and is
 * copied to RAM by Reset_Handler exactly as on real hardware.
 */

MEMORY
{
    /* ZBT SSRAM1 — code memory, mapped at the reset vector address */
    FLASH (rx)  : ORIGIN = 0x00000000, LENGTH = 4096K

    /* ZBT SSRAM2/3 — data memory */
    RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 4096K
}

HEAP_SIZE  = 0x2000;   /* 8KB heap — keep in sync with CONFIG_HEAP_SIZE */
STACK_SIZE = 0x1000;   /* 4KB main stack */

ENTRY(Reset_Handler)

SECTIONS
{
    /* ---------------------------------------------------------------
     * Vector table must be at address 0.
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
     * Used by the coredump subsystem to persist crash records.
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
