/**
 * @file kernel/smp.c
 * @brief Symmetric Multi-Processing (SMP) support stubs for VibeRTOS.
 *
 * When CONFIG_SMP=y, this file provides per-CPU data structures and
 * IPI (inter-processor interrupt) support for cross-core reschedule.
 *
 * On single-core builds (CONFIG_SMP=n) all functions are trivial stubs.
 *
 * USER: Fill in the arch-specific IPI mechanism for your SoC
 *       (e.g., RP2350 SIO FIFO for inter-core messaging).
 */

#include "vibe/types.h"
#include "vibe/thread.h"
#include "vibe/spinlock.h"
#include "vibe/sys/printk.h"

#ifdef CONFIG_SMP

#ifndef CONFIG_SMP_NUM_CPUS
#define CONFIG_SMP_NUM_CPUS  2
#endif

/* -----------------------------------------------------------------------
 * Per-CPU data structure
 * --------------------------------------------------------------------- */

typedef struct vibe_cpu {
    uint32_t       cpu_id;          /**< CPU index (0-based). */
    vibe_thread_t *current_thread;  /**< Thread currently running on this CPU. */
    vibe_thread_t *idle_thread;     /**< Idle thread for this CPU. */
    int            sched_lock;      /**< Per-CPU scheduler lock depth. */
    uint32_t       irq_nest_count;  /**< Nested IRQ depth counter. */
} vibe_cpu_t;

static vibe_cpu_t g_cpus[CONFIG_SMP_NUM_CPUS];

static vibe_spinlock_t g_smp_lock = VIBE_SPINLOCK_INIT;

/* -----------------------------------------------------------------------
 * vibe_smp_cpu_id()
 * --------------------------------------------------------------------- */

uint32_t vibe_smp_cpu_id(void)
{
    /*
     * TODO: Return the current CPU ID.
     *
     * On RP2350 (dual-core Cortex-M33), read SIO CPUID register:
     *   return *((volatile uint32_t *)0xD0000000);
     *
     * On other SoCs this may be in MPIDR or a similar register.
     */
    return 0U; /* Stub: always CPU 0. */
}

/* -----------------------------------------------------------------------
 * vibe_smp_init()
 * --------------------------------------------------------------------- */

void vibe_smp_init(void)
{
    for (int i = 0; i < CONFIG_SMP_NUM_CPUS; i++) {
        g_cpus[i].cpu_id         = (uint32_t)i;
        g_cpus[i].current_thread = NULL;
        g_cpus[i].idle_thread    = NULL;
        g_cpus[i].sched_lock     = 0;
        g_cpus[i].irq_nest_count = 0U;
    }

    vibe_printk("[smp] SMP init: %d CPUs\n", CONFIG_SMP_NUM_CPUS);

    /*
     * TODO: Start secondary CPU cores.
     *
     * On RP2040/RP2350:
     *   - Use the SIO FIFO or ROM bootrom_launch_secondary() to boot core 1.
     *   - Secondary core should call _vibe_smp_secondary_init() after startup.
     *
     * Example (RP2040):
     *   multicore_launch_core1(_vibe_smp_secondary_entry);
     */
}

/* -----------------------------------------------------------------------
 * vibe_smp_ipi_reschedule()
 * --------------------------------------------------------------------- */

void vibe_smp_ipi_reschedule(uint32_t target_cpu)
{
    if (target_cpu >= CONFIG_SMP_NUM_CPUS) {
        return;
    }

    /*
     * TODO: Send an IPI (inter-processor interrupt) to target_cpu to
     * trigger a reschedule on that core.
     *
     * On RP2040/RP2350: write to the SIO FIFO for the target core.
     * On other SoCs: configure a dedicated software IRQ in the GIC or NVIC.
     *
     * Example (RP2040 — core 0 sending to core 1):
     *   volatile uint32_t *sio_fifo_wr = (volatile uint32_t *)0xD0000054;
     *   *sio_fifo_wr = VIBE_SMP_IPI_RESCHED;
     */
    (void)target_cpu;
}

/* -----------------------------------------------------------------------
 * Secondary CPU entry (called by the boot-ROM on secondary cores)
 * --------------------------------------------------------------------- */

void _vibe_smp_secondary_entry(void)
{
    uint32_t cpu_id = vibe_smp_cpu_id();

    vibe_printk("[smp] CPU %u online\n", cpu_id);

    /*
     * TODO:
     * 1. Initialise the CPU's vector table / VTOR.
     * 2. Enable the SysTick (or use a separate timer) for this core.
     * 3. Create the idle thread for this core.
     * 4. Enter the scheduler.
     */

    /* For now, spin. */
    for (;;) {}
}

#else /* !CONFIG_SMP — single-core stubs */

uint32_t vibe_smp_cpu_id(void)
{
    return 0U;
}

void vibe_smp_init(void)
{
    /* No-op on single-core builds. */
}

void vibe_smp_ipi_reschedule(uint32_t target_cpu)
{
    (void)target_cpu;
}

#endif /* CONFIG_SMP */
