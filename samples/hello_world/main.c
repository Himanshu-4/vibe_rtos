/**
 * @file samples/hello_world/main.c
 * @brief VibeRTOS Hello World example.
 *
 * Demonstrates:
 *  - Defining a thread statically with VIBE_THREAD_DEFINE
 *  - Starting the kernel with vibe_init()
 *  - Using vibe_printk for output
 *  - Using vibe_thread_sleep for periodic execution
 */

#include <vibe/kernel.h>

/* -----------------------------------------------------------------------
 * Thread definition
 * --------------------------------------------------------------------- */

/**
 * hello_thread_entry — runs forever, printing "Hello from VibeRTOS!" every second.
 */
static void hello_thread_entry(void *arg)
{
    (void)arg;

    uint32_t count = 0;

    for (;;) {
        vibe_printk("[hello] Hello from VibeRTOS! count=%u uptime=%ums\n",
                    count, vibe_uptime_ms());
        count++;
        vibe_thread_sleep(1000);  /* Sleep 1 second */
    }
}

/* Statically allocate the thread with 1KB stack, priority 16 */
VIBE_THREAD_DEFINE(hello_thread, 1024, hello_thread_entry, NULL, 16);

/* -----------------------------------------------------------------------
 * main()
 * --------------------------------------------------------------------- */

int main(void)
{
    /*
     * vibe_init() performs all kernel startup:
     *   - arch_init() (CPU/NVIC setup)
     *   - Device initialisation
     *   - Idle thread and system work queue creation
     *   - Starts the scheduler — never returns
     *
     * Threads defined with VIBE_THREAD_DEFINE are automatically started.
     */
    vibe_init();

    /* Unreachable */
    return 0;
}
