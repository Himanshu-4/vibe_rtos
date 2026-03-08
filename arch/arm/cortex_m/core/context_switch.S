/**
 * @file arch/arm/cortex_m/core/context_switch.S
 * @brief Cortex-M context switch assembly shell.
 *
 * =============================================================================
 * USER: Implement PendSV_Handler and arch_switch_to for context switching.
 *
 * How Cortex-M context switching works:
 *
 *  1. The kernel calls arch_switch_to(from, to) which pends PendSV
 *     (lowest priority exception).
 *
 *  2. PendSV fires (after all higher-priority ISRs complete).
 *
 *  3. PendSV_Handler:
 *     a. The hardware has already saved xPSR, PC, LR, R12, R3-R0 on PSP.
 *     b. Save the remaining callee-saved registers: R11-R4 (and S16-S31
 *        if FPU is in use) onto the current thread's PSP.
 *     c. Save PSP into from->stack_ptr.
 *     d. Load to->stack_ptr into PSP.
 *     e. Restore R11-R4 (and S16-S31) from the new thread's stack.
 *     f. Return with EXC_RETURN = 0xFFFFFFFD (return to Thread mode, PSP).
 *
 * Initial stack frame (set by arch_thread_stack_init):
 *
 *   High address (top of stack)
 *   +----------+
 *   | xPSR     |  0x01000000 (Thumb bit set)
 *   | PC       |  thread->entry
 *   | LR       |  0xFFFFFFFD (EXC_RETURN, thread mode PSP)
 *   | R12      |  0x00000000
 *   | R3       |  0x00000000
 *   | R2       |  0x00000000
 *   | R1       |  0x00000000
 *   | R0       |  thread->arg   <- PSP points here after hardware pop
 *   +----------+
 *   | R11      |  (saved by software: callee-saved regs)
 *   | R10      |
 *   | R9       |
 *   | R8       |
 *   | R7       |
 *   | R6       |
 *   | R5       |
 *   | R4       |  <- stack_ptr saved here
 *   +----------+
 *   Low address (stack bottom / canary)
 *
 * References:
 *   - ARM Cortex-M4 Devices Generic User Guide (DUI0553A) Section 2.3
 *   - "The Definitive Guide to ARM Cortex-M3/M4 Processors" by Joseph Yiu
 * =============================================================================
 */

    .syntax unified
    .thumb
    .cpu cortex-m0plus    /* Change to cortex-m33/m4 as needed */

/* -----------------------------------------------------------------------
 * External symbols provided by the kernel
 * --------------------------------------------------------------------- */

    /* Pointer to the current running thread TCB (vibe_thread_t *) */
    /* Defined in kernel/sched.c as g_current_thread — but the arch
     * context switch needs direct access. Provide a helper or use
     * the pointer returned by vibe_thread_self(). */

/* -----------------------------------------------------------------------
 * PendSV_Handler — performs the actual context switch
 * --------------------------------------------------------------------- */

    .section .text.PendSV_Handler
    .global PendSV_Handler
    .type   PendSV_Handler, %function

PendSV_Handler:
    /*
     * TODO (USER): Implement Cortex-M context switch here.
     *
     * Cortex-M0+ (no FPU) implementation outline:
     *
     *   ; Disable interrupts during switch
     *   cpsid  i
     *
     *   ; Get current PSP
     *   mrs    r0, psp
     *
     *   ; Save callee-saved regs R4-R7 (low regs, Thumb16 push)
     *   subs   r0, r0, #16
     *   stmia  r0!, {r4-r7}    ; stores then increments (post-index)
     *   subs   r0, r0, #16     ; reset pointer to R4 slot
     *
     *   ; Save R8-R11 (high regs — requires mov to low first on M0+)
     *   mov    r4, r8
     *   mov    r5, r9
     *   mov    r6, r10
     *   mov    r7, r11
     *   subs   r0, r0, #16
     *   stmia  r0!, {r4-r7}
     *   subs   r0, r0, #16
     *
     *   ; Save PSP into from->stack_ptr
     *   ldr    r1, =g_from_thread  ; address of "from" pointer
     *   ldr    r1, [r1]
     *   cmp    r1, #0
     *   beq    _no_save
     *   str    r0, [r1, #OFFSET_OF_STACK_PTR]  ; TCB.stack_ptr offset
     * _no_save:
     *
     *   ; Load new thread's PSP from to->stack_ptr
     *   ldr    r1, =g_to_thread
     *   ldr    r1, [r1]
     *   ldr    r0, [r1, #OFFSET_OF_STACK_PTR]
     *
     *   ; Restore R8-R11
     *   ldmia  r0!, {r4-r7}
     *   mov    r8,  r4
     *   mov    r9,  r5
     *   mov    r10, r6
     *   mov    r11, r7
     *
     *   ; Restore R4-R7
     *   ldmia  r0!, {r4-r7}
     *
     *   ; Update PSP
     *   msr    psp, r0
     *
     *   ; Re-enable interrupts
     *   cpsie  i
     *
     *   ; Return using EXC_RETURN for thread mode + PSP
     *   ldr    r0, =0xFFFFFFFD
     *   bx     r0
     *
     * For Cortex-M4 (with optional FPU), also save/restore S16-S31 and FPSCR
     * if CONTROL.FPCA is set.
     *
     * For Cortex-M33 (ARMv8-M), use the push/pop syntax and check
     * EXC_RETURN[4] to determine if FP registers need saving.
     */

    /* Stub — spin (replace with real implementation above) */
_ctx_switch_stub:
    b       _ctx_switch_stub

    .size   PendSV_Handler, .-PendSV_Handler

/* -----------------------------------------------------------------------
 * arch_switch_to(from, to)
 *   R0 = from (vibe_thread_t * or NULL on first switch)
 *   R1 = to   (vibe_thread_t *)
 *
 * Pends PendSV to perform the actual register save/restore.
 * The kernel calls this with IRQs disabled; the actual switch happens
 * when PendSV fires after IRQs are re-enabled.
 * --------------------------------------------------------------------- */

    .section .text.arch_switch_to
    .global arch_switch_to
    .type   arch_switch_to, %function

arch_switch_to:
    /*
     * TODO (USER): Pend PendSV to schedule the context switch.
     *
     * Save the "from" and "to" pointers so PendSV_Handler can access them:
     *   str    r0, [pc-relative-to-g_from_thread]
     *   str    r1, [pc-relative-to-g_to_thread]
     *
     * Then pend PendSV via SCB->ICSR bit 28:
     *   ldr    r2, =0xE000ED04     ; SCB->ICSR
     *   ldr    r3, =0x10000000     ; PENDSVSET bit
     *   str    r3, [r2]
     *
     * PendSV will fire when interrupts are enabled.
     *
     * Note: on first call (from == NULL), PendSV must handle NULL gracefully
     * and skip the save step.
     */

    /* Stub — return immediately (replace with real pend-PendSV code) */
    bx      lr

    .size   arch_switch_to, .-arch_switch_to

/* -----------------------------------------------------------------------
 * Global pointers used by PendSV_Handler
 * (kept in .data so PendSV can access them efficiently)
 * --------------------------------------------------------------------- */

    .section .data
    .global g_arch_from_thread
    .global g_arch_to_thread
    .align  2

g_arch_from_thread:
    .word   0x00000000    /* Pointer to "from" vibe_thread_t */

g_arch_to_thread:
    .word   0x00000000    /* Pointer to "to"   vibe_thread_t */

/* -----------------------------------------------------------------------
 * arch_thread_stack_init stub
 * (actual implementation is in core/irq.c for C convenience)
 * --------------------------------------------------------------------- */
/*
 * The initial stack frame is set up in C (irq.c) because it involves
 * struct field offsets that are cleaner to handle in C with offsetof().
 * See core/irq.c: arch_thread_stack_init().
 */

/* End of context_switch.S */
