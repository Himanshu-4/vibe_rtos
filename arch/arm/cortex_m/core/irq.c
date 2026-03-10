/**
 * @file arch/arm/cortex_m/core/irq.c
 * @brief ARM Cortex-M IRQ, NVIC, and arch interface stub implementations.
 *
 * =============================================================================
 * USER: Fill in the TODO sections with your hardware register addresses.
 *
 * Most functions contain correct inline assembly for standard Cortex-M.
 * The NVIC register addresses below follow the ARM CMSIS definitions and
 * should work for any Cortex-M device without modification.
 *
 * What you MUST provide:
 *  - arch_systick_init(): program SysTick for the board's core clock speed.
 *  - arch_reboot(): NVIC system reset (already stubbed correctly).
 *  - arch_thread_stack_init(): initial exception frame on a thread's stack.
 * =============================================================================
 */

#include "arch/arm/cortex_m/arch.h"
#include "vibe/types.h"
#include "vibe/sys/printk.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

/* Forward declaration from kernel */
extern void _vibe_sys_tick_handler(void);

/* -----------------------------------------------------------------------
 * Cortex-M core peripheral register map (CMSIS-compatible)
 * --------------------------------------------------------------------- */

/* System Control Block */
typedef struct {
    volatile uint32_t CPUID;    /* 0x00 */
    volatile uint32_t ICSR;     /* 0x04 — interrupt control and state */
    volatile uint32_t VTOR;     /* 0x08 — vector table offset */
    volatile uint32_t AIRCR;    /* 0x0C — application interrupt/reset control */
    volatile uint32_t SCR;      /* 0x10 — system control */
    volatile uint32_t CCR;      /* 0x14 — configuration and control */
    volatile uint32_t SHPR[3];  /* 0x18 — system handler priority */
    volatile uint32_t SHCSR;    /* 0x24 — system handler control/state */
    volatile uint32_t CFSR;     /* 0x28 — configurable fault status */
    volatile uint32_t HFSR;     /* 0x2C — hardfault status */
    volatile uint32_t DFSR;     /* 0x30 — debug fault status */
    volatile uint32_t MMFAR;    /* 0x34 — MemManage address */
    volatile uint32_t BFAR;     /* 0x38 — BusFault address */
} _SCB_t;

#define SCB  ((_SCB_t *)0xE000ED00U)

/* SysTick */
typedef struct {
    volatile uint32_t CTRL;     /* 0x00 */
    volatile uint32_t LOAD;     /* 0x04 */
    volatile uint32_t VAL;      /* 0x08 */
    volatile uint32_t CALIB;    /* 0x0C */
} _SysTick_t;

#define SYSTICK  ((_SysTick_t *)0xE000E010U)

#define SYSTICK_CTRL_ENABLE     BIT(0)
#define SYSTICK_CTRL_TICKINT    BIT(1)
#define SYSTICK_CTRL_CLKSOURCE  BIT(2)  /* 1 = processor clock */

/* NVIC */
typedef struct {
    volatile uint32_t ISER[8];  /* 0x000 — interrupt set-enable */
    uint32_t          _RSVD0[24];
    volatile uint32_t ICER[8];  /* 0x080 — interrupt clear-enable */
    uint32_t          _RSVD1[24];
    volatile uint32_t ISPR[8];  /* 0x100 — interrupt set-pending */
    uint32_t          _RSVD2[24];
    volatile uint32_t ICPR[8];  /* 0x180 — interrupt clear-pending */
    uint32_t          _RSVD3[24];
    volatile uint32_t IABR[8];  /* 0x200 — interrupt active-bit */
    uint32_t          _RSVD4[56];
    volatile uint8_t  IP[240];  /* 0x300 — interrupt priority */
} _NVIC_t;

#define NVIC  ((_NVIC_t *)0xE000E100U)

/* SCB AIRCR fields */
#define SCB_AIRCR_VECTKEY    (0x05FA0000U)
#define SCB_AIRCR_SYSRESETREQ BIT(2)

/* SCB ICSR fields */
#define SCB_ICSR_PENDSVSET  BIT(28)
#define SCB_ICSR_IPSR_MASK  0x1FFU

/* -----------------------------------------------------------------------
 * System core clock (Hz) — set by SystemInit or BSP clock init
 *
 * USER: Update this to match your actual system clock after PLL init.
 *       Or provide a runtime variable updated by SystemInit().
 * --------------------------------------------------------------------- */
static uint32_t g_system_core_clock = 12000000U; /* Default: 12 MHz (XOSC) */

/* Called by BSP to update after PLL init */
void arch_set_core_clock(uint32_t hz)
{
    g_system_core_clock = hz;
}

/* -----------------------------------------------------------------------
 * arch_init()
 * --------------------------------------------------------------------- */

void arch_init(void)
{
    /*
     * Set PendSV to the lowest possible priority so it only runs when
     * no other exceptions are active. This ensures context switches
     * always happen in a clean state.
     *
     * SCB->SHPR[2] holds priorities for SysTick (bits 31:24) and
     * PendSV (bits 23:16).
     *
     * For Cortex-M0+ (2 priority bits in bits [7:6]):
     *   PendSV priority = 0xC0 (lowest)
     *   SysTick priority = 0x40 (above PendSV)
     */
    SCB->SHPR[2] = (0xC0U << 24) | (0x40U << 16); /* SysTick=0x40, PendSV=0xC0 */

    /*
     * TODO (USER): Set VTOR if your vector table is not at 0x00000000.
     *
     * For RP2040 with boot2 in flash:
     *   SCB->VTOR = 0x10000100;   // after 256-byte boot2
     *
     * For RP2350:
     *   SCB->VTOR = 0x10000000;   // flash start
     *
     * Cortex-M0+ does NOT have VTOR — skip for M0+.
     * Cortex-M0 and M0+ use the vector table at 0x00000000 only.
     * For M0+ with flash remapping, configure via the SCS alias if available.
     */

    /* Enable Usage, Bus, MemManage fault handlers (Cortex-M3/4/33 only).
     * On M0+ these do not exist; all faults go to HardFault. */
#if !defined(CPU_CORTEX_M0PLUS) && !defined(CPU_CORTEX_M0)
    SCB->SHCSR |= (1U << 18) | (1U << 17) | (1U << 16);
#endif
}

/* -----------------------------------------------------------------------
 * IRQ lock / unlock
 * --------------------------------------------------------------------- */

uint32_t arch_irq_lock(void)
{
    uint32_t key;
    __asm__ volatile(
        "mrs %0, PRIMASK \n"
        "cpsid i         \n"
        : "=r"(key)
        :
        : "memory"
    );
    return key;
}

void arch_irq_unlock(uint32_t key)
{
    __asm__ volatile(
        "msr PRIMASK, %0 \n"
        :
        : "r"(key)
        : "memory"
    );
}

/* -----------------------------------------------------------------------
 * ISR detection
 * --------------------------------------------------------------------- */

bool arch_is_in_isr(void)
{
    uint32_t ipsr;
    __asm__ volatile("mrs %0, IPSR" : "=r"(ipsr));
    return (ipsr & SCB_ICSR_IPSR_MASK) != 0U;
}

/* -----------------------------------------------------------------------
 * CPU idle (WFI)
 * --------------------------------------------------------------------- */

void arch_cpu_idle(void)
{
    __asm__ volatile(
        "dsb \n"    /* Data Synchronisation Barrier before WFI */
        "wfi \n"    /* Wait For Interrupt */
        ::: "memory"
    );
}

/* -----------------------------------------------------------------------
 * Tickless idle — set next hardware timer wakeup
 * --------------------------------------------------------------------- */

void arch_set_next_tick(uint32_t ticks_from_now)
{
    /*
     * TODO (USER): Reprogram the SysTick (or a dedicated low-power timer)
     * to fire after ticks_from_now ticks.
     *
     * Simple approach — reload SysTick:
     *   SYSTICK->CTRL &= ~SYSTICK_CTRL_TICKINT;  // stop
     *   SYSTICK->LOAD  = ticks_from_now * (g_system_core_clock / CONFIG_SYS_CLOCK_HZ);
     *   SYSTICK->VAL   = 0;
     *   SYSTICK->CTRL |= SYSTICK_CTRL_TICKINT;   // restart
     *
     * For deeper low-power modes, use a RTC or always-on timer instead of SysTick.
     */
    (void)ticks_from_now;
}

/* -----------------------------------------------------------------------
 * SysTick init
 * --------------------------------------------------------------------- */

void arch_systick_init(uint32_t ticks_per_sec)
{
    /*
     * TODO (USER): Set g_system_core_clock to your actual CPU frequency
     * before calling vibe_init(). Default above is 12 MHz (bare XOSC).
     *
     * Example for RP2040 at 125 MHz:
     *   arch_set_core_clock(125000000U);
     */
    uint32_t reload = (g_system_core_clock / ticks_per_sec) - 1U;

    SYSTICK->CTRL = 0U;           /* Disable while configuring */
    SYSTICK->LOAD = reload;
    SYSTICK->VAL  = 0U;           /* Clear current value */
    SYSTICK->CTRL = SYSTICK_CTRL_ENABLE
                  | SYSTICK_CTRL_TICKINT
                  | SYSTICK_CTRL_CLKSOURCE;  /* processor clock */
}

/* -----------------------------------------------------------------------
 * SysTick_Handler — calls kernel tick handler
 * --------------------------------------------------------------------- */

void SysTick_Handler(void)
{
    _vibe_sys_tick_handler();
}

/* -----------------------------------------------------------------------
 * NVIC helpers
 * --------------------------------------------------------------------- */

void arch_nvic_enable_irq(uint32_t irq)
{
    NVIC->ISER[irq >> 5U] = BIT(irq & 0x1FU);
}

void arch_nvic_disable_irq(uint32_t irq)
{
    NVIC->ICER[irq >> 5U] = BIT(irq & 0x1FU);
}

void arch_nvic_set_priority(uint32_t irq, uint32_t priority)
{
    /* IP registers are byte-wide; priority in upper bits. */
    NVIC->IP[irq] = (uint8_t)(priority & 0xFFU);
}

/* -----------------------------------------------------------------------
 * arch_reboot()
 * --------------------------------------------------------------------- */

void arch_reboot(void)
{
    /* NVIC system reset request */
    SCB->AIRCR = SCB_AIRCR_VECTKEY | SCB_AIRCR_SYSRESETREQ;
    /* Data and instruction barriers */
    __asm__ volatile("dsb" ::: "memory");
    __asm__ volatile("isb" ::: "memory");
    for (;;) {} /* Should not reach here */
}

/* -----------------------------------------------------------------------
 * arch_breakpoint()
 * --------------------------------------------------------------------- */

void arch_breakpoint(void)
{
#ifdef DEBUG
    __asm__ volatile("bkpt #0" ::: "memory");
#endif
    for (;;) {}
}

/* -----------------------------------------------------------------------
 * arch_thread_stack_init()
 *
 * Sets up an initial exception return stack frame so that when PendSV
 * restores the thread's context, the CPU will call entry(arg).
 * --------------------------------------------------------------------- */

/* Offsets into vibe_thread_t — must match the struct layout in thread.h */
#include "vibe/thread.h"

void arch_thread_stack_init(vibe_thread_t *thread)
{
    /*
     * Place the initial frame at the top of the stack.
     * Cortex-M hardware exception frame (from high to low address):
     *   xPSR   <- highest (top of stack before frame)
     *   PC     (= entry function)
     *   LR     (= EXC_RETURN 0xFFFFFFFD = return to thread, PSP)
     *   R12
     *   R3
     *   R2
     *   R1
     *   R0     (= arg)   <- PSP after hardware pop
     *
     * Then below that (saved by PendSV software):
     *   R11, R10, R9, R8, R7, R6, R5, R4  <- stack_ptr saved here
     */
    uint8_t  *stack_top = (uint8_t *)thread->stack_base + thread->stack_size;
    /* Align to 8 bytes */
    stack_top = (uint8_t *)((uintptr_t)stack_top & ~7U);

    /* Hardware exception frame (8 words = 32 bytes) */
    uint32_t *frame = (uint32_t *)stack_top - 8;
    frame[0] = (uint32_t)(uintptr_t)thread->arg;     /* R0 */
    frame[1] = 0x00000001U;                            /* R1 */
    frame[2] = 0x00000002U;                            /* R2 */
    frame[3] = 0x00000003U;                            /* R3 */
    frame[4] = 0x0000000CU;                            /* R12 */
    frame[5] = 0xFFFFFFFDU;                            /* LR (EXC_RETURN) */
    frame[6] = (uint32_t)(uintptr_t)thread->entry;    /* PC */
    frame[7] = 0x01000000U;                            /* xPSR (Thumb bit) */

    /* Software-saved registers (R4-R11): below the hardware frame */
    uint32_t *sw_frame = frame - 8;
    for (int i = 0; i < 8; i++) {
        sw_frame[i] = 0U; /* R4..R11 = 0 */
    }

    /* PSP will be restored to sw_frame (pointing at R4 slot). */
    thread->stack_ptr = sw_frame;
}

/* -----------------------------------------------------------------------
 * Default fault handler — prints minimal fault info
 * --------------------------------------------------------------------- */

__attribute__((weak))
void vibe_default_fault_handler(const char *fault_name)
{
    vibe_printk("\n!!! FAULT: %s !!!\n", fault_name);
    vibe_printk("  SCB->CFSR = 0x%x\n", (unsigned)SCB->CFSR);
    vibe_printk("  SCB->HFSR = 0x%x\n", (unsigned)SCB->HFSR);
    arch_breakpoint();
    for (;;) {}
}
