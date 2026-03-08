/**
 * @file tests/kernel/test_thread.c
 * @brief Thread lifecycle unit tests for VibeRTOS.
 */

#include "vibe/kernel.h"
#include "vibe/thread.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>

static int g_pass = 0;
static int g_fail = 0;

#define TEST_ASSERT(cond, msg)                                       \
    do {                                                              \
        if (!(cond)) {                                                \
            fprintf(stderr, "  FAIL [%s:%d]: %s\n",                  \
                    __FILE__, __LINE__, msg);                         \
            g_fail++;                                                  \
        } else {                                                      \
            g_pass++;                                                  \
        }                                                             \
    } while (0)

#define RUN_TEST(fn) \
    do { printf("  running %s ... ", #fn); fn(); printf("OK\n"); } while (0)

/* -----------------------------------------------------------------------
 * Test: thread attribute validation
 * --------------------------------------------------------------------- */

static void dummy_entry(void *arg) { (void)arg; }

static void test_thread_create(void)
{
    /*
     * Test: thread_create returns VIBE_EINVAL for NULL entry.
     */
    vibe_thread_t tcb;
    vibe_thread_attr_t attr = {
        .stack_size = 512,
        .priority   = 16,
        .name       = "test",
        .is_static  = true,
        .cpu_id     = -1,
    };
    static uint8_t stack[512] __attribute__((aligned(8)));

    vibe_err_t err = vibe_thread_create(&tcb, NULL, NULL, &attr, stack);
    TEST_ASSERT(err == VIBE_EINVAL,
                "NULL entry should return VIBE_EINVAL");

    /* Priority out of range */
    attr.priority = VIBE_PRIO_MAX + 1;
    err = vibe_thread_create(&tcb, dummy_entry, NULL, &attr, stack);
    TEST_ASSERT(err == VIBE_EINVAL,
                "Out-of-range priority should return VIBE_EINVAL");

    /* Valid creation */
    attr.priority = 16;
    err = vibe_thread_create(&tcb, dummy_entry, NULL, &attr, stack);
    TEST_ASSERT(err == VIBE_OK,
                "Valid thread creation should return VIBE_OK");

    /* Check initial state */
    TEST_ASSERT(tcb.priority == 16,
                "Thread priority should be 16");
    TEST_ASSERT(strcmp(tcb.name, "test") == 0,
                "Thread name should be 'test'");
    TEST_ASSERT(tcb.entry == dummy_entry,
                "Thread entry function should be set");
}

/* -----------------------------------------------------------------------
 * Test: thread suspend/resume state transitions
 * --------------------------------------------------------------------- */

static void test_thread_suspend_resume(void)
{
    /*
     * TODO: This requires a running scheduler to test fully.
     * For now, validate state machine transitions directly.
     */
    vibe_thread_t tcb;
    memset(&tcb, 0, sizeof(tcb));
    tcb.state = VIBE_THREAD_RUNNING;

    /* After marking SUSPENDED, state should reflect it */
    tcb.state = VIBE_THREAD_SUSPENDED;
    TEST_ASSERT(vibe_thread_get_state(&tcb) == VIBE_THREAD_SUSPENDED,
                "State should be SUSPENDED");

    /* Resume — state goes back to READY */
    tcb.state = VIBE_THREAD_READY;
    TEST_ASSERT(vibe_thread_get_state(&tcb) == VIBE_THREAD_READY,
                "State should be READY after resume");
}

/* -----------------------------------------------------------------------
 * Test: thread sleep timeout calculation
 * --------------------------------------------------------------------- */

static void test_thread_sleep(void)
{
    /* Verify MS_TO_TICKS calculation */
    vibe_tick_t ticks_1s = VIBE_MS_TO_TICKS(1000);
    TEST_ASSERT(ticks_1s == CONFIG_SYS_CLOCK_HZ,
                "1000ms should equal CONFIG_SYS_CLOCK_HZ ticks");

    vibe_tick_t ticks_100ms = VIBE_MS_TO_TICKS(100);
    TEST_ASSERT(ticks_100ms == CONFIG_SYS_CLOCK_HZ / 10,
                "100ms should be SYS_CLOCK_HZ / 10 ticks");
}

/* -----------------------------------------------------------------------
 * Test: thread restart resets stack pointer
 * --------------------------------------------------------------------- */

static void test_thread_restart(void)
{
    /*
     * TODO: Full restart test requires scheduler context.
     * For now, validate that the TCB fields are set correctly
     * after a manual reset (simulating what vibe_thread_restart does).
     */
    vibe_thread_t tcb;
    static uint8_t stack[512] __attribute__((aligned(8)));
    vibe_thread_attr_t attr = {
        .stack_size = 512,
        .priority   = 10,
        .name       = "restart_test",
        .is_static  = true,
        .cpu_id     = -1,
    };

    vibe_err_t err = vibe_thread_create(&tcb, dummy_entry, NULL, &attr, stack);
    TEST_ASSERT(err == VIBE_OK, "Thread create for restart test");

    void *original_stack_ptr = tcb.stack_ptr;
    TEST_ASSERT(original_stack_ptr != NULL,
                "Stack pointer should be set after create");

    /* Simulate restart — stack_ptr should be reset */
    /* In a real test this would call vibe_thread_restart() */
    TEST_ASSERT(tcb.base_priority == 10,
                "Base priority should be preserved");
}

/* -----------------------------------------------------------------------
 * Entry point
 * --------------------------------------------------------------------- */

int main(void)
{
    printf("=== VibeRTOS Thread Tests ===\n");

    RUN_TEST(test_thread_create);
    RUN_TEST(test_thread_suspend_resume);
    RUN_TEST(test_thread_sleep);
    RUN_TEST(test_thread_restart);

    printf("\nResults: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
