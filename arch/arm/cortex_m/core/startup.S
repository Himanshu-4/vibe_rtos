/**
 * @file arch/arm/cortex_m/core/startup.S
 * @brief ARM Cortex-M startup code and vector table shell.
 *
 * =============================================================================
 * USER: You must implement the following before this RTOS will run:
 *
 *  1. SystemInit()   — Configure PLLs, clocks, and flash wait-states.
 *                      Called before C runtime initialisation.
 *
 *  2. Reset_Handler  — The entry point after power-on / reset:
 *       a. Load the initial stack pointer (already done by the vector table
 *          on Cortex-M — but verify __StackTop is correct in your linker script).
 *       b. Call SystemInit() to set up clocks.
 *       c. Copy .data section from flash to RAM.
 *       d. Zero .bss section.
 *       e. Call vibe_init() — this starts the scheduler and never returns.
 *
 *  3. Vector table  — Add your SoC-specific IRQ handlers (UART, SPI, etc.)
 *                     after the Cortex-M system exceptions listed below.
 *
 * =============================================================================
 *
 * The assembly uses the unified ARM/Thumb syntax and targets Thumb-2 (or
 * Thumb for M0+). Adjust .cpu and .fpu directives for your specific core.
 *
 * References:
 *   - ARM Cortex-M Programming Guide (ARM DDI 0489)
 *   - CMSIS Device Templates
 * =============================================================================
 */

    .syntax unified
    .thumb

/* Select the appropriate CPU. Override from your board CMakeLists.txt
 * by passing -mcpu=cortex-m33 etc. on the compile line. */
    .cpu cortex-m0plus
    /* .fpu softvfp */       /* Uncomment for M0+/M0; use fpv4-sp-d16 for M4F */

/* -----------------------------------------------------------------------
 * External symbols
 * --------------------------------------------------------------------- */
    .extern vibe_init          /* Kernel entry — never returns */
    .extern SystemInit         /* Board/SoC clock init (weak stub below) */

    /* Linker script symbols for data/bss initialisation */
    .extern _sdata             /* Start of .data in RAM */
    .extern _edata             /* End   of .data in RAM */
    .extern _sidata            /* Load address of .data in flash */
    .extern _sbss              /* Start of .bss  in RAM */
    .extern _ebss              /* End   of .bss  in RAM */
    .extern __StackTop         /* Top of the main stack (defined in linker script) */

/* -----------------------------------------------------------------------
 * Vector table
 * placed in the .isr_vector section (linker script must place this first)
 * --------------------------------------------------------------------- */
    .section .isr_vector, "a", %progbits
    .type  _vibe_vector_table, %object
    .size  _vibe_vector_table, .-_vibe_vector_table

_vibe_vector_table:
    /* Cortex-M system exceptions (entries 0–15) */
    .word  __StackTop              /* 0: Initial stack pointer */
    .word  Reset_Handler           /* 1: Reset */
    .word  NMI_Handler             /* 2: NMI */
    .word  HardFault_Handler       /* 3: Hard Fault */
    .word  MemManage_Handler       /* 4: MemManage Fault (not on M0+) */
    .word  BusFault_Handler        /* 5: Bus Fault    (not on M0+) */
    .word  UsageFault_Handler      /* 6: Usage Fault  (not on M0+) */
    .word  0                       /* 7: Reserved */
    .word  0                       /* 8: Reserved */
    .word  0                       /* 9: Reserved */
    .word  0                       /* 10: Reserved */
    .word  SVCall_Handler          /* 11: SVCall */
    .word  DebugMon_Handler        /* 12: Debug Monitor */
    .word  0                       /* 13: Reserved */
    .word  PendSV_Handler          /* 14: PendSV — context switch */
    .word  SysTick_Handler         /* 15: SysTick — kernel tick */

    /*
     * TODO (USER): Add SoC-specific IRQ handlers here (entries 16+).
     * Example for RP2040:
     *   .word  TIMER_IRQ_0_Handler
     *   .word  UART0_IRQ_Handler
     *   ...
     *
     * Unused vectors may point to Default_Handler (weak alias below).
     */

/* -----------------------------------------------------------------------
 * Reset_Handler
 * --------------------------------------------------------------------- */
    .section .text.Reset_Handler
    .weak   Reset_Handler
    .type   Reset_Handler, %function

Reset_Handler:
    /*
     * TODO (USER): Implement full startup sequence here.
     *
     * Minimal sequence for Cortex-M:
     *
     *   1. The hardware has already loaded SP from vector_table[0].
     *
     *   2. Call SystemInit to configure clocks and flash latency.
     *      (Must not use any RAM-based globals before .data is copied.)
     *
     *   3. Copy .data from flash (LMA=_sidata) to RAM (VMA=_sdata.._edata).
     *
     *   4. Zero .bss (_sbss.._ebss).
     *
     *   5. Call vibe_init() — starts the scheduler, never returns.
     *
     * Stub implementation (does nothing useful — replace with real code):
     */

    /* Step 2: Call SystemInit */
    bl      SystemInit

    /* Step 3: Copy .data section */
    /* TODO: implement data copy loop */
    /* ldr r0, =_sidata    ; flash source */
    /* ldr r1, =_sdata     ; RAM dest start */
    /* ldr r2, =_edata     ; RAM dest end */
    /* ... */

    /* Step 4: Zero .bss */
    /* TODO: implement bss zero loop */
    /* ldr r0, =_sbss */
    /* ldr r1, =_ebss */
    /* movs r2, #0 */
    /* ... */

    /* Step 5: Enter VibeRTOS — never returns */
    bl      vibe_init
    /* If vibe_init returns (it should not), loop forever */
_reset_loop:
    b       _reset_loop

    .size   Reset_Handler, .-Reset_Handler

/* -----------------------------------------------------------------------
 * SystemInit — weak stub
 * Override in your board BSP with actual clock/PLL initialisation.
 * --------------------------------------------------------------------- */
    .section .text.SystemInit
    .weak   SystemInit
    .type   SystemInit, %function

SystemInit:
    /*
     * TODO (USER): Initialise the system clock here.
     *
     * For RP2040 (Raspberry Pi Pico):
     *   - Enable XOSC (crystal oscillator, 12 MHz)
     *   - Configure PLL_SYS to 125 or 133 MHz
     *   - Switch clk_sys to PLL_SYS
     *   - Configure flash XIP clock divisor for chosen system clock
     *
     * For RP2350 (Raspberry Pi Pico 2):
     *   - Similar but also configure clk_ref and optionally HF XOSC
     *
     * Leave empty for now to run at the default boot clock.
     */
    bx      lr

    .size   SystemInit, .-SystemInit

/* -----------------------------------------------------------------------
 * Fault and default handlers — all weak, override as needed
 * --------------------------------------------------------------------- */

    .section .text.Default_Handler
    .weak   Default_Handler
    .type   Default_Handler, %function

Default_Handler:
    /*
     * TODO (USER): Add fault diagnostics here.
     * Read SCB->CFSR, SCB->HFSR for fault type.
     * Print or store the fault info then halt.
     */
_default_loop:
    b       _default_loop

    .size   Default_Handler, .-Default_Handler

/* Provide weak aliases so every exception vector has a valid target.
 * User code overrides by defining the same name without __attribute__((weak)). */
    .weak   NMI_Handler
    .thumb_set NMI_Handler, Default_Handler

    .weak   HardFault_Handler
    .thumb_set HardFault_Handler, Default_Handler

    .weak   MemManage_Handler
    .thumb_set MemManage_Handler, Default_Handler

    .weak   BusFault_Handler
    .thumb_set BusFault_Handler, Default_Handler

    .weak   UsageFault_Handler
    .thumb_set UsageFault_Handler, Default_Handler

    .weak   SVCall_Handler
    .thumb_set SVCall_Handler, Default_Handler

    .weak   DebugMon_Handler
    .thumb_set DebugMon_Handler, Default_Handler

    /* PendSV_Handler and SysTick_Handler are implemented in context_switch.S
     * and irq.c respectively — do NOT alias them to Default_Handler. */

/* End of startup.S */
