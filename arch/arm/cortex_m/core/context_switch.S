/**
 * @file arch/arm/cortex_m/core/context_switch.S
 * @brief Cortex-M context switch (PendSV).
 *
 * =============================================================================
 * How Cortex-M context switching works here:
 *
 *  1. The kernel calls arch_switch_to(from, to) — implemented in C in
 *     core/irq.c — which publishes &to->stack_ptr into g_arch_next_sp_loc
 *     and pends PendSV (the lowest priority exception).
 *
 *  2. PendSV fires once no higher-priority exception is active.
 *
 *  3. PendSV_Handler (below):
 *     a. The hardware has already saved xPSR, PC, LR, R12, R3-R0 onto the
 *        outgoing thread's PSP on exception entry.
 *     b. Software saves the callee-saved registers R4-R11 below that frame
 *        and stores the final stack pointer through g_arch_running_sp_loc —
 *        the thread whose registers are physically live, which may differ
 *        from the kernel's already-updated g_current_thread.
 *     c. Software loads the stack pointer through g_arch_next_sp_loc, restores
 *        R4-R11, and returns with EXC_RETURN = 0xFFFFFFFD so the hardware
 *        pops the rest of the frame and resumes the new thread on PSP.
 *
 *  g_arch_running_sp_loc == NULL marks the very first switch (entered from
 *  vibe_sched_start() running on MSP): the save step is skipped and MSP is
 *  reset to __StackTop, reclaiming the abandoned start-up context.
 *
 *  Stack frame layout built by arch_thread_stack_init() (core/irq.c):
 *
 *   High address (top of stack)
 *   +----------+
 *   | xPSR     |  0x01000000 (Thumb bit set)
 *   | PC       |  thread->entry
 *   | LR       |  _vibe_thread_exit (runs if entry returns)
 *   | R12      |
 *   | R3       |
 *   | R2       |
 *   | R1       |
 *   | R0       |  thread->arg   <- PSP after hardware pop
 *   +----------+
 *   | R4..R7   |  saved by software (higher block)
 *   | R8..R11  |  saved by software (lower block) <- stack_ptr points here
 *   +----------+
 *   Low address
 *
 *  All instructions are Thumb-1 so the same code runs on M0+/M3/M4/M33.
 *
 *  NOTE: no FPU state is saved (soft-float ABI). For hard-float builds this
 *  handler must additionally stack S16-S31 when EXC_RETURN bit 4 is clear.
 *
 * References:
 *   - ARM Cortex-M4 Devices Generic User Guide (DUI0553A) Section 2.3
 *   - "The Definitive Guide to ARM Cortex-M3/M4 Processors" by Joseph Yiu
 * =============================================================================
 */

    .syntax unified
    .thumb
    .cpu cortex-m0plus

    .extern g_arch_running_sp_loc  /* void** — &running->stack_ptr, NULL  */
                                   /*          until the first switch      */
    .extern g_arch_next_sp_loc     /* void** — &next->stack_ptr (latest    */
                                   /*          scheduling choice)          */
    .extern __StackTop             /* Top of main stack (linker script)   */

/* -----------------------------------------------------------------------
 * PendSV_Handler — performs the actual context switch
 * --------------------------------------------------------------------- */

    .section .text.PendSV_Handler
    .global PendSV_Handler
    .type   PendSV_Handler, %function

PendSV_Handler:
    cpsid   i

    /* ---- Save the context that is physically on the CPU ---- */
    ldr     r2, =g_arch_running_sp_loc
    ldr     r1, [r2]                  /* r1 = &running->stack_ptr, or NULL */
    cmp     r1, #0
    beq     _pendsv_first_switch

    mrs     r0, psp

    /* Save R4-R7 at [psp-16 .. psp-4] */
    subs    r0, r0, #16
    stmia   r0!, {r4-r7}
    subs    r0, r0, #16

    /* Save R8-R11 below at [psp-32 .. psp-20] (via low regs for M0+) */
    mov     r4, r8
    mov     r5, r9
    mov     r6, r10
    mov     r7, r11
    subs    r0, r0, #16
    stmia   r0!, {r4-r7}
    subs    r0, r0, #16

    str     r0, [r1]                  /* running->stack_ptr = final SP */
    b       _pendsv_restore

_pendsv_first_switch:
    /* Entered from MSP thread mode (vibe_sched_start). The exception
     * frame pushed on MSP is abandoned; reset MSP to the top of the main
     * stack so those bytes are reclaimed for interrupt handling.
     * (No stack use after this point.) */
    ldr     r0, =__StackTop
    msr     msp, r0

_pendsv_restore:
    /* ---- Restore the kernel's latest scheduling choice ---- */
    ldr     r2, =g_arch_next_sp_loc
    ldr     r1, [r2]                  /* r1 = &next->stack_ptr */
    ldr     r0, [r1]                  /* r0 = next->stack_ptr  */

    /* Restore R8-R11 (stored lowest) */
    ldmia   r0!, {r4-r7}
    mov     r8, r4
    mov     r9, r5
    mov     r10, r6
    mov     r11, r7

    /* Restore R4-R7 */
    ldmia   r0!, {r4-r7}

    msr     psp, r0

    /* This thread's context is now the one on the CPU. If PendSV was
     * re-pended while active, the tail-chained activation finds
     * running == next and performs a harmless identity switch. */
    ldr     r2, =g_arch_running_sp_loc
    str     r1, [r2]

    cpsie   i

    /* Return to Thread mode, resuming on PSP */
    ldr     r0, =0xFFFFFFFD
    bx      r0

    .size   PendSV_Handler, .-PendSV_Handler

/* -----------------------------------------------------------------------
 * arch_switch_to(from, to) and arch_thread_stack_init() are implemented
 * in C (core/irq.c): C can take &thread->stack_ptr directly instead of
 * relying on hard-coded TCB field offsets in assembly.
 * --------------------------------------------------------------------- */

/* End of context_switch.S */
