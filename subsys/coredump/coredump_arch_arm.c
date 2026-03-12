/**
 * @file subsys/coredump/coredump_arch_arm.c
 * @brief ARM Cortex-M fault handler entry stubs for VibeRTOS coredump.
 *
 * Each handler is a __naked function that:
 *   1. Determines whether the fault occurred on PSP or MSP (EXC_RETURN bit 2).
 *   2. Passes the exception frame pointer, PSP, MSP, and reason to
 *      _vibe_coredump_capture() in coredump.c.
 *   3. Spins forever (or resets) after the dump.
 *
 * These definitions are strong symbols — they override the weak stubs
 * provided by arch/arm/cortex_m/core/irq.c when CONFIG_COREDUMP=y.
 *
 * Thumb / Cortex-M0+ compatible: no IT blocks, no Thumb-2-only instructions.
 */

#include "vibe/coredump.h"
#include <stdint.h>

/* Forward declaration */
void _vibe_coredump_capture(uint32_t *exc_frame,
                             uint32_t  psp,
                             uint32_t  msp,
                             uint32_t  reason);

/* -------------------------------------------------------------------------
 * Common naked trampoline macro
 *
 * On exception entry the CPU has pushed onto the active stack:
 *   [SP+0 ] R0
 *   [SP+4 ] R1
 *   [SP+8 ] R2
 *   [SP+12] R3
 *   [SP+16] R12
 *   [SP+20] LR  (return address of faulting code)
 *   [SP+24] PC  (faulting instruction address)
 *   [SP+28] xPSR
 *
 * LR at exception entry = EXC_RETURN value:
 *   Bit 2 = 1 → exception frame is on PSP (thread stack)
 *   Bit 2 = 0 → exception frame is on MSP (handler stack)
 *
 * We read both PSP and MSP explicitly and pass all three to C.
 * ---------------------------------------------------------------------- */

/* C function called after we set up R0..R3 */
static void __attribute__((used))
fault_entry_c(uint32_t *exc_frame, uint32_t psp, uint32_t msp, uint32_t reason)
{
    _vibe_coredump_capture(exc_frame, psp, msp, reason);
    /* Spin — _vibe_coredump_capture() already printed and will not return
     * in normal operation, but just in case:                              */
    for (;;) {
        __asm__ volatile("nop");
    }
}

/* -------------------------------------------------------------------------
 * Naked trampoline — Thumb, M0+ compatible (no ITE/ITT)
 *
 * R0 = exc_frame pointer  (PSP or MSP depending on EXC_RETURN bit 2)
 * R1 = PSP
 * R2 = MSP
 * R3 = reason (set before calling the macro)
 * ---------------------------------------------------------------------- */

#define FAULT_HANDLER_NAKED(name, reason_val)                               \
    __attribute__((naked, used))                                            \
    void name(void)                                                         \
    {                                                                       \
        __asm__ volatile (                                                  \
            /* Test EXC_RETURN bit 2 to choose which stack has the frame */\
            "mov  r0, lr          \n" /* r0 = EXC_RETURN                 */\
            "movs r1, #4          \n" /* test bit 2                      */\
            "tst  r0, r1          \n"                                       \
            "beq  1f              \n" /* branch if bit2 == 0 (MSP used)  */\
            "mrs  r0, psp         \n" /* bit2 == 1 → frame on PSP        */\
            "b    2f              \n"                                       \
            "1:                   \n"                                       \
            "mrs  r0, msp         \n" /* bit2 == 0 → frame on MSP        */\
            "2:                   \n"                                       \
            /* Now r0 = exception frame pointer                          */\
            "mrs  r1, psp         \n" /* r1 = PSP                        */\
            "mrs  r2, msp         \n" /* r2 = MSP                        */\
            "movs r3, %[rsn]      \n" /* r3 = reason                     */\
            "bl   fault_entry_c   \n" /* tail-call into C                */\
            :                                                               \
            : [rsn] "I" (reason_val)                                        \
            : "memory"                                                      \
        );                                                                  \
    }

/* -------------------------------------------------------------------------
 * Fault handler definitions
 *
 * These are strong symbols — they override the weak defaults in irq.c
 * when this translation unit is linked.
 *
 * On Cortex-M0/M0+:
 *   All faults (MemManage, BusFault, UsageFault) are escalated to HardFault.
 *   Only HardFault_Handler will actually fire; the others are defined here
 *   for completeness on M3/M4/M33/M55 where they are separate vectors.
 * ---------------------------------------------------------------------- */

FAULT_HANDLER_NAKED(HardFault_Handler,  VIBE_COREDUMP_REASON_HARD_FAULT)
FAULT_HANDLER_NAKED(MemManage_Handler,  VIBE_COREDUMP_REASON_MEM_FAULT)
FAULT_HANDLER_NAKED(BusFault_Handler,   VIBE_COREDUMP_REASON_BUS_FAULT)
FAULT_HANDLER_NAKED(UsageFault_Handler, VIBE_COREDUMP_REASON_USAGE_FAULT)
