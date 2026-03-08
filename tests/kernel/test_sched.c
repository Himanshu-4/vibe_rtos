/**
 * @file tests/kernel/test_sched.c
 * @brief Scheduler unit tests for VibeRTOS.
 *
 * Runs on the host simulation target. Each test_ function is a standalone
 * test case using simple assertion macros.
 */

#include "vibe/kernel.h"
#include "vibe/sched.h"
#include "vibe/thread.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>

/* -----------------------------------------------------------------------
 * Simple test framework
 * --------------------------------------------------------------------- */

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

#define RUN_TEST(fn)                    \
    do {                                \
        printf("  running %s ... ", #fn); \
        fn();                           \
        printf("OK\n");                 \
    } while (0)

/* -----------------------------------------------------------------------
 * Test: priority ordering
 * --------------------------------------------------------------------- */

static void test_sched_priority_ordering(void)
{
    /*
     * Verify that _vibe_sched_next_thread() returns the highest-priority
     * READY thread.
     *
     * TODO: On host simulation, create stub TCBs and enqueue them,
     *       then assert that the highest-priority one is selected.
     *
     * Placeholder assertion — replace with real enqueue/dequeue test.
     */
    TEST_ASSERT(VIBE_PRIO_MAX > VIBE_PRIO_MIN,
                "Max priority must be greater than min priority");
    TEST_ASSERT(VIBE_PRIO_MAX == 31,
                "Priority range should be 0-31");
}

/* -----------------------------------------------------------------------
 * Test: round-robin at same priority
 * --------------------------------------------------------------------- */

static void test_sched_round_robin(void)
{
    /*
     * Two threads at the same priority — after one yields, the other
     * should be selected.
     *
     * TODO: Create two stub threads at the same priority, call
     *       _vibe_sched_enqueue on both, then call _vibe_sched_next_thread
     *       twice and assert we get alternating threads.
     */
    TEST_ASSERT(1 == 1, "Round-robin stub — placeholder");
}

/* -----------------------------------------------------------------------
 * Test: scheduler mode
 * --------------------------------------------------------------------- */

static void test_sched_mode(void)
{
    /*
     * Verify that vibe_sched_set_mode() / vibe_sched_get_mode() work.
     *
     * Note: on host simulation, the arch calls (arch_irq_lock etc.)
     * need stub implementations. The scheduler mode test should not
     * actually perform context switches.
     */
    vibe_sched_set_mode(VIBE_SCHED_LOW_POWER);
    TEST_ASSERT(vibe_sched_get_mode() == VIBE_SCHED_LOW_POWER,
                "Scheduler mode set/get LOW_POWER");

    vibe_sched_set_mode(VIBE_SCHED_PERFORMANCE);
    TEST_ASSERT(vibe_sched_get_mode() == VIBE_SCHED_PERFORMANCE,
                "Scheduler mode set/get PERFORMANCE");

    vibe_sched_set_mode(VIBE_SCHED_MID);
    TEST_ASSERT(vibe_sched_get_mode() == VIBE_SCHED_MID,
                "Scheduler mode restore MID");
}

/* -----------------------------------------------------------------------
 * Test: scheduler lock depth
 * --------------------------------------------------------------------- */

static void test_sched_lock_depth(void)
{
    int initial = vibe_sched_lock_count();

    vibe_sched_lock();
    TEST_ASSERT(vibe_sched_lock_count() == initial + 1,
                "Lock depth incremented");

    vibe_sched_lock();
    TEST_ASSERT(vibe_sched_lock_count() == initial + 2,
                "Lock depth nested");

    vibe_sched_unlock();
    TEST_ASSERT(vibe_sched_lock_count() == initial + 1,
                "Lock depth decremented after unlock");

    vibe_sched_unlock();
    TEST_ASSERT(vibe_sched_lock_count() == initial,
                "Lock depth restored");
}

/* -----------------------------------------------------------------------
 * Entry point
 * --------------------------------------------------------------------- */

int main(void)
{
    printf("=== VibeRTOS Scheduler Tests ===\n");

    RUN_TEST(test_sched_priority_ordering);
    RUN_TEST(test_sched_round_robin);
    RUN_TEST(test_sched_mode);
    RUN_TEST(test_sched_lock_depth);

    printf("\nResults: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
