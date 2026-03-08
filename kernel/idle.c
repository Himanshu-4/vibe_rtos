/**
 * @file kernel/idle.c
 * @brief Idle thread for VibeRTOS.
 *
 * The idle thread runs at the lowest priority (VIBE_PRIO_MIN) and is
 * always schedulable. It calls arch_cpu_idle() (WFI on Cortex-M) to
 * put the CPU into a low-power wait state until the next interrupt.
 *
 * When CONFIG_TICKLESS_IDLE is enabled, the idle thread also programs
 * the hardware timer for the next scheduled wakeup event, allowing
 * the system tick to be suppressed during deep idle periods.
 *
 * For SMP, one idle thread is created per CPU core.
 */

#include "vibe/thread.h"
#include "vibe/sched.h"
#include "vibe/types.h"
#include "vibe/sys/printk.h"

#include "arch/arm/cortex_m/include/arch/arm/cortex_m/arch.h"

/* External reference to the idle thread TCB — set by kernel/init.c. */
extern vibe_thread_t *g_idle_thread;

void _vibe_idle_entry(void *arg)
{
    (void)arg;

    /* Register this thread as the idle thread for this CPU. */
    g_idle_thread = vibe_thread_self();

    vibe_printk("[idle] idle thread started\n");

    for (;;) {
#ifdef CONFIG_TICKLESS_IDLE
        /*
         * Compute the number of ticks until the next scheduled wakeup
         * (earliest of: sleeping threads, running timers).
         */
        vibe_tick_t next = _vibe_sched_next_wakeup();

        if (next != VIBE_WAIT_FOREVER && next > vibe_tick_get()) {
            vibe_tick_t ticks_to_sleep = next - vibe_tick_get();
            /*
             * Program the hardware timer to generate an interrupt after
             * ticks_to_sleep ticks. The SysTick is optionally suppressed
             * during this interval to save power.
             *
             * USER: implement arch_set_next_tick() to program the hardware.
             */
            arch_set_next_tick(ticks_to_sleep);
        }
#endif /* CONFIG_TICKLESS_IDLE */

        /*
         * Enter CPU low-power state.
         * On Cortex-M this is WFI (Wait For Interrupt).
         * The CPU wakes on the next IRQ (tick, GPIO, UART, etc.).
         *
         * USER: implement arch_cpu_idle() — typically a single WFI instruction.
         */
        arch_cpu_idle();

        /*
         * After wakeup: the tick ISR has already run (it woke us via PendSV
         * if a thread became ready), so just loop back and let the scheduler
         * pick the right thread.
         */
    }
}
