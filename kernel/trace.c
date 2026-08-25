/**
 * @file kernel/trace.c
 * @brief Default (weak) trace hook implementations and stats accessor.
 *
 * Every hook is a WEAK empty function: with CONFIG_TRACE=y the kernel is
 * fully instrumented but records nothing until a trace library (SEGGER
 * SystemView, Percepio Tracealyzer, or a custom RTT recorder) provides
 * strong overrides for the hooks it cares about.
 */

#include "vibe/trace.h"
#include "vibe/thread.h"
#include <string.h>

#ifdef CONFIG_TRACE

__attribute__((weak)) void vibe_trace_thread_create(vibe_thread_t *thread)
{
    (void)thread;
}

__attribute__((weak)) void vibe_trace_thread_exit(vibe_thread_t *thread)
{
    (void)thread;
}

__attribute__((weak)) void vibe_trace_thread_ready(vibe_thread_t *thread)
{
    (void)thread;
}

__attribute__((weak)) void vibe_trace_thread_switch_out(vibe_thread_t *thread)
{
    (void)thread;
}

__attribute__((weak)) void vibe_trace_thread_switch_in(vibe_thread_t *thread)
{
    (void)thread;
}

__attribute__((weak)) void vibe_trace_thread_sleep(vibe_thread_t *thread,
                                                   vibe_tick_t wake_tick)
{
    (void)thread;
    (void)wake_tick;
}

__attribute__((weak)) void vibe_trace_isr_enter(void)
{
}

__attribute__((weak)) void vibe_trace_isr_exit(void)
{
}

__attribute__((weak)) void vibe_trace_tick(vibe_tick_t tick)
{
    (void)tick;
}

__attribute__((weak)) void vibe_trace_idle(void)
{
}

#endif /* CONFIG_TRACE */

vibe_err_t vibe_trace_thread_stats(vibe_thread_t *thread,
                                   vibe_trace_thread_stats_t *stats)
{
    if (stats == NULL) {
        return VIBE_EINVAL;
    }
    if (thread == NULL) {
        thread = vibe_thread_self();
    }
    if (thread == NULL) {
        return VIBE_EINVAL;
    }

    memset(stats, 0, sizeof(*stats));
#ifdef CONFIG_TRACE
    stats->switch_in_count = thread->switch_in_count;
    stats->last_switch_in  = thread->last_switch_in;
    stats->total_runtime   = thread->total_runtime;
#endif
    return VIBE_OK;
}
