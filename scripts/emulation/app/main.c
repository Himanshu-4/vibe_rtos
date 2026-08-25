/**
 * @file scripts/emulation/app/main.c
 * @brief VibeRTOS QEMU emulation test application.
 *
 * Two-phase kernel self-test suite on the ARM MPS2 AN386 machine:
 *
 *   Phase 1 (pre-scheduler) — runs as an APPLICATION-level device init
 *   inside vibe_init(): printk formatting, heap allocator, ring buffer,
 *   device registry, RTT channels, trace stats API.
 *
 *   Phase 2 (scheduler) — runs in real threads once vibe_sched_start()
 *   has switched context: preemptive scheduling, sleep/wake, round-robin
 *   time-slicing, thread exit, and the kernel trace hooks/fields.
 *
 * Output markers consumed by scripts/emulation/test.py:
 *   "*** EMULATION TESTS PASSED ***"  → suite succeeded
 *   "*** EMULATION TESTS FAILED ***"  → at least one check failed
 *
 * Exit behaviour:
 *   Default             — terminate QEMU via semihosting (exit code 0/1).
 *   -DEMU_CONTINUE_BOOT — keep the demo threads running instead of exiting.
 */

#include <vibe/kernel.h>
#include <vibe/heap.h>
#include <vibe/device.h>
#include <vibe/rtt.h>
#include <vibe/trace.h>
#include <vibe/sys/printk.h>
#include <string.h>

#include "soc.h"          /* vibe_emu_exit() — MPS2 AN386 semihosting   */
#include "ring_buffer.h"  /* from lib/ring_buffer (PUBLIC include dir)  */

VIBE_LOG_MODULE_REGISTER(emu_test, VIBE_LOG_LEVEL_INF);

/* -----------------------------------------------------------------------
 * Tiny test harness
 * --------------------------------------------------------------------- */

static unsigned g_checks;
static unsigned g_failures;

static void check(bool ok, const char *what)
{
    g_checks++;
    if (ok) {
        vibe_printk("[emu_test]   ok: %s\n", what);
    } else {
        g_failures++;
        vibe_printk("[emu_test] FAIL: %s\n", what);
    }
}

/* -----------------------------------------------------------------------
 * Trace hook overrides — strong definitions replacing the weak no-ops in
 * kernel/trace.c, exactly how a trace library plugs in. They count events
 * so phase 2 can verify the kernel instrumentation actually fires.
 * --------------------------------------------------------------------- */

static volatile uint32_t g_hook_ticks;
static volatile uint32_t g_hook_switch_ins;

void vibe_trace_tick(vibe_tick_t tick)
{
    (void)tick;
    g_hook_ticks++;
}

void vibe_trace_thread_switch_in(vibe_thread_t *thread)
{
    (void)thread;
    g_hook_switch_ins++;
}

/* -----------------------------------------------------------------------
 * Phase 1 — pre-scheduler tests
 * --------------------------------------------------------------------- */

static void test_printk(void)
{
    vibe_printk("[emu_test] -- printk formatting --\n");
    vibe_printk("[emu_test]   fmt: d=%d u=%u x=%x s=%s c=%c lu=%lu pct=%%\n",
                -42, 42U, 0xBEEFU, "str", 'Z', (unsigned long)123456UL);
    check(true, "printk executed without fault");

    char buf[32];
    size_t n = vibe_snprintk(buf, sizeof(buf), "%s=%d", "x", 7);
    check(n == 3U && strcmp(buf, "x=7") == 0, "snprintk formats into buffer");

    n = vibe_snprintk(buf, 4, "abcdef");
    check(n == 6U && strcmp(buf, "abc") == 0, "snprintk truncates safely");
}

static void test_heap(void)
{
    vibe_printk("[emu_test] -- heap allocator --\n");

    void *a = vibe_heap_alloc(64);
    void *b = vibe_heap_alloc(128);
    check(a != NULL, "heap_alloc(64) returns memory");
    check(b != NULL, "heap_alloc(128) returns memory");
    check(a != b, "allocations are distinct");

    if (a != NULL) {
        memset(a, 0xA5, 64);
        check(((const uint8_t *)a)[63] == 0xA5, "allocation is writable");
    }

    void *c = vibe_heap_calloc(4, 16);
    if (c != NULL) {
        bool zeroed = true;
        for (size_t i = 0; i < 64; i++) {
            if (((const uint8_t *)c)[i] != 0) {
                zeroed = false;
                break;
            }
        }
        check(zeroed, "calloc memory is zeroed");
    } else {
        check(false, "heap_calloc(4, 16) returns memory");
    }

    b = vibe_heap_realloc(b, 256);
    check(b != NULL, "realloc grows allocation");

    vibe_heap_stats_t s;
    vibe_heap_stats(&s);
    check(s.used > 0, "heap stats report usage");

    vibe_heap_free(a);
    vibe_heap_free(b);
    vibe_heap_free(c);

    vibe_heap_stats_t s2;
    vibe_heap_stats(&s2);
    check(s2.used < s.used, "heap usage drops after free");
}

static void test_ring_buffer(void)
{
    vibe_printk("[emu_test] -- ring buffer --\n");

    static uint8_t storage[16];
    vibe_rb_t rb;

    check(vibe_rb_init(&rb, storage, sizeof(storage)), "rb_init succeeds");

    const uint8_t out[] = { 1, 2, 3, 4, 5 };
    size_t put = vibe_rb_put_buf(&rb, out, sizeof(out));
    check(put == sizeof(out), "rb_put_buf stores all bytes");
    check(vibe_rb_used(&rb) == sizeof(out), "rb_used reflects contents");

    uint8_t in[sizeof(out)] = { 0 };
    size_t got = vibe_rb_get_buf(&rb, in, sizeof(in));
    check(got == sizeof(out), "rb_get_buf returns all bytes");
    check(memcmp(in, out, sizeof(out)) == 0, "data round-trips intact");
    check(vibe_rb_used(&rb) == 0, "buffer empty after drain");
}

static void test_device_registry(void)
{
    vibe_printk("[emu_test] -- device registry --\n");

#ifdef CONFIG_UART
    const vibe_device_t *uart = vibe_device_get("uart0");
    check(uart != NULL, "uart0 found in device registry");
    check(vibe_device_is_ready(uart), "uart0 reports ready");
    if (uart != NULL) {
        check(uart->init_level == VIBE_INIT_POST_KERNEL,
              "uart0 registered at POST_KERNEL level");
    }
#endif
    check(vibe_device_get("no_such_device") == NULL,
          "unknown device lookup returns NULL");
}

static void test_rtt(void)
{
    vibe_printk("[emu_test] -- RTT channels --\n");

    extern vibe_rtt_cb_t _SEGGER_RTT;

    vibe_rtt_init();
    check(strcmp(_SEGGER_RTT.acID, "SEGGER RTT") == 0,
          "control block carries the SEGGER RTT ID");
    check(_SEGGER_RTT.aUp[0].pBuffer != NULL &&
          _SEGGER_RTT.aUp[0].SizeOfBuffer > 0U,
          "up channel 0 (Terminal) is configured");

    const char msg[] = "hello-rtt";
    unsigned int before = vibe_rtt_up_used(0U);
    unsigned int n = vibe_rtt_write(0U, msg, sizeof(msg) - 1U);
    check(n == sizeof(msg) - 1U, "rtt_write accepts all bytes");
    check(vibe_rtt_up_used(0U) == before + n,
          "up-buffer occupancy tracks the write");

    /* Simulate a host writing into the down buffer, then read it back. */
    vibe_rtt_down_t *down = &_SEGGER_RTT.aDown[0];
    const char host_cmd[] = "ok";
    memcpy(&down->pBuffer[down->WrOff], host_cmd, sizeof(host_cmd) - 1U);
    down->WrOff += sizeof(host_cmd) - 1U;

    char rx[8] = { 0 };
    unsigned int r = vibe_rtt_read(0U, rx, sizeof(rx));
    check(r == 2U && rx[0] == 'o' && rx[1] == 'k',
          "rtt_read drains the down channel");
}

static void test_trace_api(void)
{
    vibe_printk("[emu_test] -- trace API --\n");

    vibe_trace_thread_stats_t stats;
    check(vibe_trace_thread_stats(NULL, &stats) == VIBE_EINVAL,
          "stats before scheduler start reports EINVAL (no thread)");
    check(vibe_trace_thread_stats(NULL, NULL) == VIBE_EINVAL,
          "stats rejects NULL output");
}

/* -----------------------------------------------------------------------
 * Phase 1 entry — APPLICATION-level device init inside vibe_init()
 * --------------------------------------------------------------------- */

static vibe_err_t emu_test_run(const vibe_device_t *dev)
{
    (void)dev;

    vibe_printk("\n[emu_test] VibeRTOS emulation self-test (MPS2 AN386 / QEMU)\n");
    vibe_printk("[emu_test] == phase 1: pre-scheduler ==\n");

    test_printk();
    test_heap();
    test_ring_buffer();
    test_device_registry();
    test_rtt();
    test_trace_api();

    vibe_printk("[emu_test] phase 1 done: %u checks, %u failures\n",
                g_checks, g_failures);
    return VIBE_OK;
}

VIBE_DEVICE_DEFINE(emu_test, emu_test_run, NULL, NULL, NULL,
                   VIBE_INIT_APPLICATION, NULL);

/* -----------------------------------------------------------------------
 * Phase 2 — scheduler tests (real threads)
 * --------------------------------------------------------------------- */

static volatile uint32_t g_spin_a_count;
static volatile uint32_t g_spin_b_count;
static volatile uint32_t g_sleeper_count;
static volatile bool     g_stop_spinners;

static vibe_thread_t g_spin_a, g_spin_b, g_sleeper, g_oneshot, g_orchestrator;
static uint8_t g_spin_a_stack[1024]  __attribute__((aligned(8)));
static uint8_t g_spin_b_stack[1024]  __attribute__((aligned(8)));
static uint8_t g_sleeper_stack[1024] __attribute__((aligned(8)));
static uint8_t g_oneshot_stack[512]  __attribute__((aligned(8)));
static uint8_t g_orch_stack[2048]    __attribute__((aligned(8)));

/* Two CPU-bound spinners at the same priority — only round-robin
 * time-slicing lets both make progress. */
static void spin_entry(void *arg)
{
    volatile uint32_t *counter = arg;
    while (!g_stop_spinners) {
        (*counter)++;
    }
    for (;;) {
        vibe_thread_sleep(1000);
    }
}

/* Higher-priority sleeper — must preempt the spinners on every wakeup. */
static void sleeper_entry(void *arg)
{
    (void)arg;
    for (;;) {
        g_sleeper_count++;
        vibe_thread_sleep(10);
    }
}

/* Returns immediately — exercises the thread-exit trampoline. */
static void oneshot_entry(void *arg)
{
    (void)arg;
}

static void orchestrator_entry(void *arg)
{
    (void)arg;

    vibe_printk("[emu_test] == phase 2: scheduler ==\n");
    check(true, "first context switch reached a thread");

    /* Let the workers run for ~300ms of simulated time. */
    vibe_thread_sleep(300);
    g_stop_spinners = true;

    vibe_tick_t ticks = vibe_tick_get();
    vibe_printk("[emu_test]   ticks=%u sleeper=%u spinA=%u spinB=%u\n",
                (unsigned)ticks, (unsigned)g_sleeper_count,
                (unsigned)g_spin_a_count, (unsigned)g_spin_b_count);

    check(ticks >= 250U, "system tick advances (SysTick alive)");
    check(g_sleeper_count >= 10U, "sleep/wake works (sleeper made progress)");
    check(g_spin_a_count > 0U && g_spin_b_count > 0U,
          "time-slicing shares the CPU between equal-priority spinners");
    check(vibe_thread_get_state(&g_oneshot) == VIBE_THREAD_DEAD,
          "returning from entry lands in the exit trampoline (thread DEAD)");

    /* Trace hooks (overridden above) and per-thread stats fields. */
    vibe_printk("[emu_test]   trace: hook_ticks=%u hook_switch_ins=%u\n",
                (unsigned)g_hook_ticks, (unsigned)g_hook_switch_ins);
    check(g_hook_ticks >= 250U, "vibe_trace_tick hook fires every tick");
    check(g_hook_switch_ins >= 20U, "vibe_trace_thread_switch_in hook fires");

    vibe_trace_thread_stats_t stats;
    vibe_err_t err = vibe_trace_thread_stats(&g_sleeper, &stats);
    check(err == VIBE_OK && stats.switch_in_count >= 10U,
          "TCB trace fields: sleeper switch_in_count accumulates");
    err = vibe_trace_thread_stats(NULL, &stats);
    check(err == VIBE_OK && stats.switch_in_count >= 1U,
          "TCB trace fields: self stats readable");

    /* Log subsystem formats varargs now (was: raw format string). */
    VIBE_LOG_INF("formatted log: value=%d name=%s", 42, "vibe");

    /* Final verdict. */
    vibe_printk("[emu_test] %u checks, %u failures\n", g_checks, g_failures);
    if (g_failures == 0U) {
        vibe_printk("*** EMULATION TESTS PASSED ***\n");
    } else {
        vibe_printk("*** EMULATION TESTS FAILED ***\n");
    }

#ifdef EMU_CONTINUE_BOOT
    vibe_printk("[emu_test] EMU_CONTINUE_BOOT set — demo threads keep running\n");
    for (;;) {
        vibe_thread_sleep(1000);
        vibe_printk("[emu_test] alive, uptime=%ums sleeper=%u\n",
                    (unsigned)vibe_uptime_ms(), (unsigned)g_sleeper_count);
    }
#else
    vibe_emu_exit(g_failures == 0U ? 0 : 1);
#endif
}

/* -----------------------------------------------------------------------
 * main() — Reset_Handler calls this; create threads, then start the kernel
 * --------------------------------------------------------------------- */

static void create_thread(vibe_thread_t *t, vibe_thread_entry_t entry,
                          void *arg, const char *name, uint8_t prio,
                          void *stack, size_t stack_size)
{
    vibe_thread_attr_t attr = {
        .stack_size = stack_size,
        .priority   = prio,
        .name       = name,
        .is_static  = true,
        .cpu_id     = -1,
    };
    vibe_err_t err = vibe_thread_create(t, entry, arg, &attr, stack);
    if (err != VIBE_OK) {
        vibe_printk("[emu_test] FATAL: cannot create %s (%ld)\n", name, err);
    }
}

int main(void)
{
    create_thread(&g_spin_a, spin_entry, (void *)&g_spin_a_count,
                  "spin_a", 8, g_spin_a_stack, sizeof(g_spin_a_stack));
    create_thread(&g_spin_b, spin_entry, (void *)&g_spin_b_count,
                  "spin_b", 8, g_spin_b_stack, sizeof(g_spin_b_stack));
    create_thread(&g_sleeper, sleeper_entry, NULL,
                  "sleeper", 12, g_sleeper_stack, sizeof(g_sleeper_stack));
    create_thread(&g_oneshot, oneshot_entry, NULL,
                  "oneshot", 14, g_oneshot_stack, sizeof(g_oneshot_stack));
    create_thread(&g_orchestrator, orchestrator_entry, NULL,
                  "orchestrator", 20, g_orch_stack, sizeof(g_orch_stack));

    vibe_init();  /* Never returns */
    return 0;
}
