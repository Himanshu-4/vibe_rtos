/**
 * @file kernel/init.c
 * @brief VibeRTOS kernel initialisation.
 *
 * Implements vibe_init(), the top-level kernel entry point called from
 * the arch startup code (Reset_Handler -> main -> vibe_init).
 */

#include "vibe/kernel.h"
#include "vibe/types.h"
#include "vibe/thread.h"
#include "vibe/sched.h"
#include "vibe/timer.h"
#include "vibe/workq.h"
#include "vibe/device.h"
#include "vibe/log.h"
#include "vibe/sys/printk.h"

/* Include the arch interface — must be provided by the selected arch. */
#include "arch/arm/cortex_m/include/arch/arm/cortex_m/arch.h"

VIBE_LOG_MODULE_REGISTER(kernel_init, VIBE_LOG_LEVEL_INF);

/* -----------------------------------------------------------------------
 * Global system tick counter
 * --------------------------------------------------------------------- */

static volatile vibe_tick_t g_tick_count = 0U;

/* -----------------------------------------------------------------------
 * Idle thread (one per CPU — defined here for CPU0; SMP extends this)
 * --------------------------------------------------------------------- */

#ifndef CONFIG_IDLE_STACK_SIZE
#define CONFIG_IDLE_STACK_SIZE  256
#endif

static uint8_t _idle_stack[CONFIG_IDLE_STACK_SIZE] __attribute__((aligned(8)));
static vibe_thread_t _idle_thread;

/* Forward declaration — idle entry is in kernel/idle.c */
extern void _vibe_idle_entry(void *arg);

/* -----------------------------------------------------------------------
 * System work queue stack (defined here so kernel owns the memory)
 * --------------------------------------------------------------------- */

#ifndef CONFIG_SYS_WORKQ_STACK_SIZE
#define CONFIG_SYS_WORKQ_STACK_SIZE  1024
#endif
#ifndef CONFIG_SYS_WORKQ_PRIORITY
#define CONFIG_SYS_WORKQ_PRIORITY    30
#endif

static uint8_t _sys_workq_stack[CONFIG_SYS_WORKQ_STACK_SIZE]
    __attribute__((aligned(8)));

/* The global system work queue instance (extern declared in workq.h). */
vibe_workq_t vibe_sys_workq;

/* -----------------------------------------------------------------------
 * vibe_init()
 * --------------------------------------------------------------------- */

void vibe_init(void)
{
    /*
     * Step 1: Architecture-level early init.
     * Sets up the SysTick timer, NVIC, and any other CPU-level resources.
     * USER: implement arch_init() in arch/arm/cortex_m/core/irq.c.
     */
    arch_init();

    /*
     * Step 2: Initialise kernel data structures before any devices.
     */
    /* (Scheduler run queues and sleep list are zeroed at link time,
     *  but we call explicit init for clarity.) */

    /*
     * Step 3: Init all PRE_KERNEL_1 devices (clocks, memory controllers).
     */
    _vibe_device_init_level(VIBE_INIT_PRE_KERNEL_1);

    /*
     * Step 4: Init PRE_KERNEL_2 devices (anything that needs clocks ready).
     */
    _vibe_device_init_level(VIBE_INIT_PRE_KERNEL_2);

    /*
     * Step 5: Create the idle thread (lowest priority, CPU-bound).
     */
    {
        vibe_thread_attr_t attr = {
            .stack_size = CONFIG_IDLE_STACK_SIZE,
            .priority   = VIBE_PRIO_MIN,
            .name       = "idle",
            .is_static  = true,
            .cpu_id     = -1,
        };
        vibe_err_t err = vibe_thread_create(&_idle_thread,
                                             _vibe_idle_entry,
                                             NULL,
                                             &attr,
                                             _idle_stack);
        if (err != VIBE_OK) {
            vibe_printk("FATAL: failed to create idle thread (%d)\n", err);
            /* Halt — cannot continue without idle thread. */
            for (;;) {}
        }
    }

    /*
     * Step 6: Create the system work queue.
     */
    {
        vibe_err_t err = vibe_workq_init(&vibe_sys_workq,
                                          _sys_workq_stack,
                                          CONFIG_SYS_WORKQ_STACK_SIZE,
                                          CONFIG_SYS_WORKQ_PRIORITY,
                                          "sys_workq");
        if (err != VIBE_OK) {
            vibe_printk("WARN: failed to create system work queue (%d)\n", err);
        }
    }

    /*
     * Step 7: Init POST_KERNEL devices (UART, GPIO, etc.).
     */
    _vibe_device_init_level(VIBE_INIT_POST_KERNEL);

    /*
     * Step 8: Logging subsystem.
     */
    _vibe_log_init();

    VIBE_LOG_INF("VibeRTOS v%s starting", VIBE_RTOS_VERSION_STRING);

    /*
     * Step 9: Init APPLICATION-level devices.
     */
    _vibe_device_init_level(VIBE_INIT_APPLICATION);

    /*
     * Step 10: Configure SysTick to fire at CONFIG_SYS_CLOCK_HZ.
     * USER: implement arch_systick_init() in arch cortex_m.
     */
    arch_systick_init(CONFIG_SYS_CLOCK_HZ);

    VIBE_LOG_INF("Scheduler starting at %d Hz", CONFIG_SYS_CLOCK_HZ);

    /*
     * Step 11: Hand control to the scheduler. Never returns.
     */
    vibe_sched_start();

    /* Should never reach here. */
    __builtin_unreachable();
}

/* -----------------------------------------------------------------------
 * System tick handler (called from SysTick_Handler in arch)
 * --------------------------------------------------------------------- */

/**
 * @brief Called every system tick from the SysTick ISR.
 *
 * Increments the tick counter, advances software timers, and wakes
 * sleeping threads whose timeout has expired.
 */
void _vibe_sys_tick_handler(void)
{
    g_tick_count++;
    _vibe_timer_tick();
    _vibe_sched_wake_expired(g_tick_count);

    /* Trigger PendSV to reschedule if a higher-priority thread is now ready. */
    _vibe_sched_reschedule();
}

/* -----------------------------------------------------------------------
 * Public time APIs
 * --------------------------------------------------------------------- */

vibe_tick_t vibe_tick_get(void)
{
    return g_tick_count;
}

uint32_t vibe_uptime_ms(void)
{
    return VIBE_TICKS_TO_MS(g_tick_count);
}

/* -----------------------------------------------------------------------
 * System reboot (weak — override for board-specific reset)
 * --------------------------------------------------------------------- */

__attribute__((weak, noreturn))
void vibe_reboot(void)
{
    arch_reboot();
    for (;;) {} /* Should not reach here */
}

/* -----------------------------------------------------------------------
 * Device init walker
 * --------------------------------------------------------------------- */

void _vibe_device_init_level(vibe_init_level_t level)
{
    /* Walk the ._vibe_devices linker section.
     * Devices are stored contiguously; we compare their init level.
     * In practice the linker script creates subsections per level. */
    const vibe_device_t *dev;
    for (dev = _vibe_devices_start; dev < _vibe_devices_end; dev++) {
        /* NOTE: with proper linker sections the level is encoded in the
         * section name. Here we call all init_fn; a real implementation
         * would compare level ordering using section start/end symbols.
         * USER: extend this with per-level section symbols if needed. */
        if (dev->init_fn != NULL) {
            vibe_err_t err = dev->init_fn(dev);
            if (err != VIBE_OK) {
                VIBE_LOG_WRN("device '%s' init failed: %d", dev->name, err);
            }
        }
    }
    (void)level; /* Used for documentation; extend for real level gating. */
}

/* -----------------------------------------------------------------------
 * vibe_printk minimal implementation (calls arch UART or semihosting)
 * --------------------------------------------------------------------- */

#include <stdarg.h>

/* Very small printf — only %d, %u, %x, %s, %c are needed for kernel output. */
static void _printk_putchar(char c)
{
    /* USER: replace with actual UART or RTT output. */
    (void)c;
}

static void _printk_puts(const char *s)
{
    while (*s) {
        _printk_putchar(*s++);
    }
}

static void _printk_putuint(uint32_t v, int base)
{
    char buf[11];
    int  i = sizeof(buf) - 1;
    buf[i] = '\0';
    if (v == 0) {
        _printk_putchar('0');
        return;
    }
    while (v && i > 0) {
        int d = v % base;
        buf[--i] = (char)(d < 10 ? '0' + d : 'a' + d - 10);
        v /= base;
    }
    _printk_puts(&buf[i]);
}

void vibe_vprintk(const char *fmt, va_list args)
{
    for (const char *p = fmt; *p; p++) {
        if (*p != '%') {
            _printk_putchar(*p);
            continue;
        }
        p++;
        switch (*p) {
        case 'd': case 'i': {
            int v = va_arg(args, int);
            if (v < 0) { _printk_putchar('-'); v = -v; }
            _printk_putuint((uint32_t)v, 10);
            break;
        }
        case 'u':
            _printk_putuint(va_arg(args, unsigned int), 10);
            break;
        case 'x': case 'X':
            _printk_putuint(va_arg(args, unsigned int), 16);
            break;
        case 's': {
            const char *s = va_arg(args, const char *);
            _printk_puts(s ? s : "(null)");
            break;
        }
        case 'c':
            _printk_putchar((char)va_arg(args, int));
            break;
        case 'p':
            _printk_puts("0x");
            _printk_putuint((uint32_t)(uintptr_t)va_arg(args, void *), 16);
            break;
        case '%':
            _printk_putchar('%');
            break;
        default:
            _printk_putchar('%');
            _printk_putchar(*p);
            break;
        }
    }
}

void vibe_printk(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vibe_vprintk(fmt, args);
    va_end(args);
}

void vibe_assert_halt(void)
{
    arch_breakpoint();
    for (;;) {}
}

/* Device section symbols — defined by the linker script. */
/* Provide weak defaults so the kernel links on host simulation without a ld script. */
__attribute__((weak)) const vibe_device_t _vibe_devices_start[0];
__attribute__((weak)) const vibe_device_t _vibe_devices_end[0];

const vibe_device_t *vibe_device_get(const char *name)
{
    for (const vibe_device_t *d = _vibe_devices_start; d < _vibe_devices_end; d++) {
        if (d->name && __builtin_strcmp(d->name, name) == 0) {
            return d;
        }
    }
    return NULL;
}

bool vibe_device_is_ready(const vibe_device_t *dev)
{
    return (dev != NULL && dev->init_fn != NULL);
}
