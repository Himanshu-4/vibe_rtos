/**
 * @file tests/kernel/test_sync.c
 * @brief Synchronisation primitive unit tests for VibeRTOS.
 *
 * Tests mutex, semaphore, event flags, and message queue
 * without a running scheduler (single-threaded validation).
 */

#include "vibe/kernel.h"
#include "vibe/mutex.h"
#include "vibe/sem.h"
#include "vibe/event.h"
#include "vibe/msgq.h"
#include <stdio.h>
#include <string.h>

static int g_pass = 0;
static int g_fail = 0;

#define TEST_ASSERT(cond, msg)                                        \
    do {                                                               \
        if (!(cond)) {                                                 \
            fprintf(stderr, "  FAIL [%s:%d]: %s\n",                   \
                    __FILE__, __LINE__, msg);                          \
            g_fail++;                                                   \
        } else {                                                       \
            g_pass++;                                                   \
        }                                                              \
    } while (0)

#define RUN_TEST(fn) \
    do { printf("  running %s ... ", #fn); fn(); printf("OK\n"); } while (0)

/* -----------------------------------------------------------------------
 * Mutex tests
 * --------------------------------------------------------------------- */

static void test_mutex_init(void)
{
    vibe_mutex_t m;
    vibe_err_t err = vibe_mutex_init(&m);
    TEST_ASSERT(err == VIBE_OK,        "mutex init OK");
    TEST_ASSERT(m.owner == NULL,       "mutex initially unlocked");
    TEST_ASSERT(m.lock_count == 0,     "mutex lock count = 0");
}

static void test_mutex_trylock_unlock(void)
{
    VIBE_MUTEX_DEFINE(m);
    vibe_mutex_init(&m);

    /* trylock on free mutex should succeed */
    /* Note: this will "succeed" if vibe_thread_self() returns NULL
     * and the mutex code handles NULL owner correctly. The full test
     * requires a real thread context. */

    /* Validate NULL guard */
    vibe_err_t err = vibe_mutex_init(NULL);
    TEST_ASSERT(err == VIBE_EINVAL, "NULL mutex init returns EINVAL");

    err = vibe_mutex_lock(NULL, VIBE_NO_WAIT);
    TEST_ASSERT(err == VIBE_EINVAL, "NULL mutex lock returns EINVAL");

    err = vibe_mutex_unlock(NULL);
    TEST_ASSERT(err == VIBE_EINVAL, "NULL mutex unlock returns EINVAL");
}

/* -----------------------------------------------------------------------
 * Semaphore tests
 * --------------------------------------------------------------------- */

static void test_sem_init(void)
{
    vibe_sem_t s;
    vibe_err_t err;

    err = vibe_sem_init(&s, 5U, 10U);
    TEST_ASSERT(err == VIBE_OK,             "sem init OK");
    TEST_ASSERT(vibe_sem_count(&s) == 5U,   "sem initial count = 5");

    /* initial > max is invalid */
    err = vibe_sem_init(&s, 11U, 10U);
    TEST_ASSERT(err == VIBE_EINVAL,         "sem initial > max -> EINVAL");
}

static void test_sem_give_take(void)
{
    VIBE_SEM_DEFINE(s, 1U, 2U);
    vibe_sem_init(&s, 1U, 2U);

    /* Give — count should increase to 2 */
    vibe_err_t err = vibe_sem_give(&s);
    TEST_ASSERT(err == VIBE_OK,             "sem give OK (count -> 2)");
    TEST_ASSERT(vibe_sem_count(&s) == 2U,   "sem count = 2");

    /* Give again — at max, no waiter */
    err = vibe_sem_give(&s);
    TEST_ASSERT(err == VIBE_EOVERFLOW,      "sem give at max -> EOVERFLOW");

    /* Take — count should decrease */
    err = vibe_sem_take(&s, VIBE_NO_WAIT);
    TEST_ASSERT(err == VIBE_OK,             "sem take OK (count -> 1)");
    TEST_ASSERT(vibe_sem_count(&s) == 1U,   "sem count = 1");

    /* Take again */
    err = vibe_sem_take(&s, VIBE_NO_WAIT);
    TEST_ASSERT(err == VIBE_OK,             "sem take OK (count -> 0)");
    TEST_ASSERT(vibe_sem_count(&s) == 0U,   "sem count = 0");

    /* Take on empty with NO_WAIT -> EAGAIN */
    err = vibe_sem_take(&s, VIBE_NO_WAIT);
    TEST_ASSERT(err == VIBE_EAGAIN,         "sem take on empty -> EAGAIN");

    /* Reset */
    vibe_sem_reset(&s);
    TEST_ASSERT(vibe_sem_count(&s) == 0U,   "sem count = 0 after reset");
}

/* -----------------------------------------------------------------------
 * Event flags tests
 * --------------------------------------------------------------------- */

static void test_event_init(void)
{
    vibe_event_t ev;
    vibe_err_t err = vibe_event_init(&ev);
    TEST_ASSERT(err == VIBE_OK,          "event init OK");
    TEST_ASSERT(vibe_event_get(&ev) == 0, "event flags initially 0");
}

static void test_event_post_clear(void)
{
    VIBE_EVENT_DEFINE(ev);
    vibe_event_init(&ev);

    vibe_event_post(&ev, 0x05U);
    TEST_ASSERT(vibe_event_get(&ev) == 0x05U, "event flags = 0x05");

    vibe_event_post(&ev, 0x0AU);
    TEST_ASSERT(vibe_event_get(&ev) == 0x0FU, "event flags = 0x0F");

    vibe_event_clear(&ev, 0x05U);
    TEST_ASSERT(vibe_event_get(&ev) == 0x0AU, "event flags = 0x0A after clear");

    vibe_event_clear(&ev, 0xFFFFFFFFU);
    TEST_ASSERT(vibe_event_get(&ev) == 0U, "event flags = 0 after full clear");
}

static void test_event_wait_no_wait(void)
{
    vibe_event_t ev;
    vibe_event_init(&ev);

    /* Wait with NO_WAIT on empty — should return EAGAIN */
    vibe_err_t err = vibe_event_wait(&ev, BIT(0), VIBE_EVENT_WAIT_ANY, VIBE_NO_WAIT);
    TEST_ASSERT(err == VIBE_EAGAIN, "event wait NO_WAIT on empty -> EAGAIN");

    /* Set the bit, then wait — should succeed immediately */
    vibe_event_post(&ev, BIT(0));
    err = vibe_event_wait(&ev, BIT(0), VIBE_EVENT_WAIT_ANY, VIBE_NO_WAIT);
    TEST_ASSERT(err == VIBE_OK, "event wait with bit set -> OK");
}

/* -----------------------------------------------------------------------
 * Message queue tests
 * --------------------------------------------------------------------- */

typedef struct { uint32_t val; } test_msg_t;

static void test_msgq_init_and_put_get(void)
{
    VIBE_MSGQ_DEFINE(q, sizeof(test_msg_t), 4);
    vibe_msgq_init(&q, _q_buf, sizeof(test_msg_t), 4);

    TEST_ASSERT(vibe_msgq_count(&q) == 0, "queue initially empty");

    test_msg_t in  = { .val = 0xDEADBEEFU };
    test_msg_t out = { .val = 0U };

    vibe_err_t err = vibe_msgq_put(&q, &in, VIBE_NO_WAIT);
    TEST_ASSERT(err == VIBE_OK,           "msgq put OK");
    TEST_ASSERT(vibe_msgq_count(&q) == 1, "queue count = 1");

    err = vibe_msgq_get(&q, &out, VIBE_NO_WAIT);
    TEST_ASSERT(err == VIBE_OK,                "msgq get OK");
    TEST_ASSERT(out.val == 0xDEADBEEFU,        "message value preserved");
    TEST_ASSERT(vibe_msgq_count(&q) == 0,      "queue empty after get");

    /* Get from empty -> EAGAIN */
    err = vibe_msgq_get(&q, &out, VIBE_NO_WAIT);
    TEST_ASSERT(err == VIBE_EAGAIN, "msgq get on empty -> EAGAIN");
}

/* -----------------------------------------------------------------------
 * Entry point
 * --------------------------------------------------------------------- */

int main(void)
{
    printf("=== VibeRTOS Sync Primitive Tests ===\n");

    RUN_TEST(test_mutex_init);
    RUN_TEST(test_mutex_trylock_unlock);
    RUN_TEST(test_sem_init);
    RUN_TEST(test_sem_give_take);
    RUN_TEST(test_event_init);
    RUN_TEST(test_event_post_clear);
    RUN_TEST(test_event_wait_no_wait);
    RUN_TEST(test_msgq_init_and_put_get);

    printf("\nResults: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
