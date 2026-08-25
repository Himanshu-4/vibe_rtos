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
#include "vibe/trace.h"
#include "vibe/sys/printk.h"

/* Include the arch interface — must be provided by the selected arch. */
#include "vibe/arch.h"

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
            vibe_printk("FATAL: failed to create idle thread (%ld)\n", err);
            /* Halt — cannot continue without idle thread. */
            for (;;) {}
        }

        /* Register the idle thread with the scheduler NOW — it must be
         * known before the first scheduling decision, not only once the
         * idle thread has run (which would be a chicken-and-egg). */
        extern vibe_thread_t *g_idle_thread;
        g_idle_thread = &_idle_thread;
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
            vibe_printk("WARN: failed to create system work queue (%ld)\n", err);
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
    VIBE_TRACE_TICK(g_tick_count);

    _vibe_timer_tick();
    _vibe_sched_wake_expired(g_tick_count);

    /* Round-robin time-slicing between same-priority threads. */
    _vibe_sched_timeslice_tick();

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
    /* Walk the ._vibe_devices linker section and initialise only the
     * devices registered at this level. vibe_init() calls this once per
     * level in ascending order, so each device's init_fn runs exactly
     * once, at the right point in the boot sequence. */
    const vibe_device_t *dev;
    for (dev = _vibe_devices_start; dev < _vibe_devices_end; dev++) {
        if (dev->init_level != level) {
            continue;
        }
        if (dev->init_fn != NULL) {
            vibe_err_t err = dev->init_fn(dev);
            if (err != VIBE_OK) {
                VIBE_LOG_WRN("device '%s' init failed: %ld", dev->name, err);
            }
        }
    }
}

/* -----------------------------------------------------------------------
 * vibe_printk minimal implementation (calls arch UART or semihosting)
 * --------------------------------------------------------------------- */

#include <stdarg.h>

/**
 * @brief Weak console output hook — one character to the console.
 *
 * The default is a no-op (output is discarded). Board/SoC support code
 * overrides this with a strong definition that writes to a UART, RTT,
 * or semihosting channel (see soc/arm/arm/mps2_an386/soc.c for the
 * QEMU emulation console).
 */
__attribute__((weak)) void vibe_console_putc(char c)
{
    (void)c;
}

/*
 * Very small printf core — %d, %i, %u, %x, %X, %s, %c, %p, %% plus the
 * 'l' length modifier (long == 32-bit on Cortex-M). All output goes
 * through an emit callback so the same core serves both the console
 * (vibe_printk) and string buffers (vibe_vsnprintk, used by logging).
 */

typedef void (*_fmt_emit_fn)(char c, void *ctx);

static void _fmt_puts(_fmt_emit_fn emit, void *ctx, const char *s)
{
    while (*s) {
        emit(*s++, ctx);
    }
}

static void _fmt_putuint(_fmt_emit_fn emit, void *ctx, uint32_t v, int base)
{
    char buf[11];
    int  i = sizeof(buf) - 1;
    buf[i] = '\0';
    if (v == 0) {
        emit('0', ctx);
        return;
    }
    while (v && i > 0) {
        int d = v % base;
        buf[--i] = (char)(d < 10 ? '0' + d : 'a' + d - 10);
        v /= base;
    }
    _fmt_puts(emit, ctx, &buf[i]);
}

static void _fmt_core(_fmt_emit_fn emit, void *ctx,
                      const char *fmt, va_list args)
{
    for (const char *p = fmt; *p; p++) {
        if (*p != '%') {
            emit(*p, ctx);
            continue;
        }
        p++;

        /* 'l' length modifier — long is 32-bit on Cortex-M, so the
         * va_arg type is the same width; just consume the modifier. */
        bool is_long = false;
        if (*p == 'l') {
            is_long = true;
            p++;
        }

        switch (*p) {
        case 'd': case 'i': {
            long v = is_long ? va_arg(args, long) : (long)va_arg(args, int);
            if (v < 0) { emit('-', ctx); v = -v; }
            _fmt_putuint(emit, ctx, (uint32_t)v, 10);
            break;
        }
        case 'u': {
            unsigned long v = is_long ? va_arg(args, unsigned long)
                                      : (unsigned long)va_arg(args, unsigned int);
            _fmt_putuint(emit, ctx, (uint32_t)v, 10);
            break;
        }
        case 'x': case 'X': {
            unsigned long v = is_long ? va_arg(args, unsigned long)
                                      : (unsigned long)va_arg(args, unsigned int);
            _fmt_putuint(emit, ctx, (uint32_t)v, 16);
            break;
        }
        case 's': {
            const char *s = va_arg(args, const char *);
            _fmt_puts(emit, ctx, s ? s : "(null)");
            break;
        }
        case 'c':
            emit((char)va_arg(args, int), ctx);
            break;
        case 'p':
            _fmt_puts(emit, ctx, "0x");
            _fmt_putuint(emit, ctx,
                         (uint32_t)(uintptr_t)va_arg(args, void *), 16);
            break;
        case '%':
            emit('%', ctx);
            break;
        case '\0':
            return; /* Trailing '%' at end of format string. */
        default:
            emit('%', ctx);
            if (is_long) { emit('l', ctx); }
            emit(*p, ctx);
            break;
        }
    }
}

/* --- Console output path --- */

static void _console_emit(char c, void *ctx)
{
    (void)ctx;
    vibe_console_putc(c);
}

void vibe_vprintk(const char *fmt, va_list args)
{
    _fmt_core(_console_emit, NULL, fmt, args);
}

void vibe_printk(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vibe_vprintk(fmt, args);
    va_end(args);
}

/* --- Buffer output path (used by the logging subsystem) --- */

typedef struct {
    char   *buf;
    size_t  size;   /* Total buffer size, including space for NUL. */
    size_t  pos;    /* Characters written so far (excluding NUL). */
} _snprintk_ctx_t;

static void _buffer_emit(char c, void *vctx)
{
    _snprintk_ctx_t *ctx = vctx;
    if (ctx->pos + 1U < ctx->size) {
        ctx->buf[ctx->pos] = c;
    }
    ctx->pos++;
}

size_t vibe_vsnprintk(char *buf, size_t size, const char *fmt, va_list args)
{
    _snprintk_ctx_t ctx = { .buf = buf, .size = size, .pos = 0U };

    _fmt_core(_buffer_emit, &ctx, fmt, args);

    if (size > 0U) {
        buf[(ctx.pos < size - 1U) ? ctx.pos : size - 1U] = '\0';
    }
    return ctx.pos;
}

size_t vibe_snprintk(char *buf, size_t size, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    size_t n = vibe_vsnprintk(buf, size, fmt, args);
    va_end(args);
    return n;
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
