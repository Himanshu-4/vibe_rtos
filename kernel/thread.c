/**
 * @file kernel/thread.c
 * @brief Thread lifecycle management for VibeRTOS.
 *
 * Handles create, delete, suspend, resume, sleep, yield, restart,
 * and priority changes. Stack setup and arch-specific context frame
 * initialisation is delegated to arch_thread_stack_init().
 */

#include "vibe/thread.h"
#include "vibe/sched.h"
#include "vibe/types.h"
#include "vibe/mem.h"
#include "vibe/spinlock.h"
#include "vibe/sys/printk.h"
#include <string.h>

#include "arch/arm/cortex_m/include/arch/arm/cortex_m/arch.h"

/* -----------------------------------------------------------------------
 * Stack canary value
 * --------------------------------------------------------------------- */

#define STACK_CANARY_VALUE  0xDEADBEEFU

#ifndef CONFIG_STACK_CANARY
#define CONFIG_STACK_CANARY 1
#endif

/* -----------------------------------------------------------------------
 * Thread ID counter
 * --------------------------------------------------------------------- */

static uint32_t g_next_thread_id = 1U;

/* -----------------------------------------------------------------------
 * Internal helpers
 * --------------------------------------------------------------------- */

static void _thread_stack_setup(vibe_thread_t *thread)
{
#if CONFIG_STACK_CANARY
    /* Write canary at the bottom (lowest address) of the stack. */
    uint32_t *canary = (uint32_t *)thread->stack_base;
    *canary = STACK_CANARY_VALUE;
#endif

    /* Ask the arch layer to initialise the stack frame so that on the
     * first context switch the thread appears to be returning from an
     * exception with PC = entry, R0 = arg.
     * USER: implement arch_thread_stack_init(). */
    arch_thread_stack_init(thread);
}

static bool _thread_stack_check(const vibe_thread_t *thread)
{
#if CONFIG_STACK_CANARY
    uint32_t *canary = (uint32_t *)thread->stack_base;
    return (*canary == STACK_CANARY_VALUE);
#else
    (void)thread;
    return true;
#endif
}

/* -----------------------------------------------------------------------
 * vibe_thread_create()
 * --------------------------------------------------------------------- */

vibe_err_t vibe_thread_create(vibe_thread_t          *thread,
                               vibe_thread_entry_t     entry,
                               void                   *arg,
                               const vibe_thread_attr_t *attr,
                               void                   *stack)
{
    if (entry == NULL || attr == NULL) {
        return VIBE_EINVAL;
    }
    if (attr->priority > VIBE_PRIO_MAX) {
        return VIBE_EINVAL;
    }

    /* Determine stack buffer. */
    void    *stack_buf  = stack;
    size_t   stack_size = attr->stack_size > 0 ? attr->stack_size : 512U;

    if (stack_buf == NULL) {
#ifdef CONFIG_HEAP_MEM
        stack_buf = vibe_heap_alloc(stack_size);
        if (stack_buf == NULL) {
            return VIBE_ENOMEM;
        }
#else
        return VIBE_ENOMEM;
#endif
    }

    /* Initialise the TCB. */
    memset(thread, 0, sizeof(*thread));
    thread->id             = g_next_thread_id++;
    thread->entry          = entry;
    thread->arg            = arg;
    thread->priority       = attr->priority;
    thread->base_priority  = attr->priority;
    thread->stack_base     = stack_buf;
    thread->stack_size     = stack_size;
    thread->state          = VIBE_THREAD_SUSPENDED;
    thread->is_static      = attr->is_static ? 1 : 0;
    thread->cpu_id         = (uint8_t)(attr->cpu_id < 0 ? 0 : attr->cpu_id);

    if (attr->name != NULL) {
        strncpy(thread->name, attr->name, sizeof(thread->name) - 1);
    }

    vibe_list_node_init(&thread->sched_node);
    vibe_list_node_init(&thread->wait_node);
    vibe_list_node_init(&thread->sleep_node);

#ifdef CONFIG_TLS
    /* Place TLS block at the top of the stack (above the initial frame). */
    thread->tls = (uint8_t *)stack_buf + stack_size - sizeof(void *);
#endif

    /* Set up the initial stack frame. */
    _thread_stack_setup(thread);

    /* Move thread to the READY state and enqueue it. */
    _vibe_sched_enqueue(thread);

    return VIBE_OK;
}

/* -----------------------------------------------------------------------
 * vibe_thread_delete()
 * --------------------------------------------------------------------- */

vibe_err_t vibe_thread_delete(vibe_thread_t *thread)
{
    if (thread == NULL) {
        thread = vibe_thread_self();
    }

    vibe_irq_key_t key = arch_irq_lock();

    if (thread->state == VIBE_THREAD_READY || thread->state == VIBE_THREAD_RUNNING) {
        _vibe_sched_dequeue(thread);
    }
    thread->state = VIBE_THREAD_DEAD;

#ifdef CONFIG_HEAP_MEM
    if (!thread->is_static && thread->stack_base != NULL) {
        vibe_heap_free(thread->stack_base);
        thread->stack_base = NULL;
    }
#endif

    arch_irq_unlock(key);

    /* If deleting self, reschedule immediately. */
    if (thread == vibe_thread_self()) {
        _vibe_sched_reschedule();
    }

    return VIBE_OK;
}

/* -----------------------------------------------------------------------
 * vibe_thread_suspend() / vibe_thread_resume()
 * --------------------------------------------------------------------- */

vibe_err_t vibe_thread_suspend(vibe_thread_t *thread)
{
    if (thread == NULL) {
        thread = vibe_thread_self();
    }

    vibe_irq_key_t key = arch_irq_lock();

    if (thread->state == VIBE_THREAD_DEAD) {
        arch_irq_unlock(key);
        return VIBE_EINVAL;
    }

    if (thread->state == VIBE_THREAD_READY || thread->state == VIBE_THREAD_RUNNING) {
        _vibe_sched_dequeue(thread);
    }
    thread->state = VIBE_THREAD_SUSPENDED;

    arch_irq_unlock(key);

    if (thread == vibe_thread_self()) {
        _vibe_sched_reschedule();
    }

    return VIBE_OK;
}

vibe_err_t vibe_thread_resume(vibe_thread_t *thread)
{
    if (thread == NULL) {
        return VIBE_EINVAL;
    }

    vibe_irq_key_t key = arch_irq_lock();

    if (thread->state != VIBE_THREAD_SUSPENDED) {
        arch_irq_unlock(key);
        return VIBE_EINVAL;
    }

    _vibe_sched_enqueue(thread);
    arch_irq_unlock(key);

    /* If the resumed thread has higher priority, reschedule. */
    vibe_thread_t *self = vibe_thread_self();
    if (self != NULL && thread->priority > self->priority) {
        _vibe_sched_reschedule();
    }

    return VIBE_OK;
}

/* -----------------------------------------------------------------------
 * vibe_thread_sleep()
 * --------------------------------------------------------------------- */

void vibe_thread_sleep(uint32_t ms)
{
    vibe_thread_t *self = vibe_thread_self();
    if (self == NULL) {
        return;
    }

    vibe_tick_t now    = vibe_tick_get();
    vibe_tick_t expiry = now + VIBE_MS_TO_TICKS(ms);

    /* Remove from run queue and place in sleep list. */
    vibe_irq_key_t key = arch_irq_lock();
    _vibe_sched_dequeue(self);
    _vibe_sched_sleep(self, expiry);
    arch_irq_unlock(key);

    /* Update tickless idle if in low-power mode. */
#ifdef CONFIG_TICKLESS_IDLE
    vibe_tick_t next = _vibe_sched_next_wakeup();
    if (next != VIBE_WAIT_FOREVER) {
        arch_set_next_tick(next);
    }
#endif

    _vibe_sched_reschedule();
}

/* -----------------------------------------------------------------------
 * vibe_thread_yield()
 * --------------------------------------------------------------------- */

void vibe_thread_yield(void)
{
    vibe_thread_t *self = vibe_thread_self();
    if (self == NULL) {
        return;
    }

    vibe_irq_key_t key = arch_irq_lock();
    /* Move self to the tail of its priority queue (round-robin). */
    _vibe_sched_dequeue(self);
    _vibe_sched_enqueue(self);
    arch_irq_unlock(key);

    _vibe_sched_reschedule();
}

/* -----------------------------------------------------------------------
 * vibe_thread_restart()
 * --------------------------------------------------------------------- */

vibe_err_t vibe_thread_restart(vibe_thread_t *thread)
{
    if (thread == NULL) {
        thread = vibe_thread_self();
    }

    vibe_irq_key_t key = arch_irq_lock();

    /* Remove from wherever it is. */
    if (thread->state == VIBE_THREAD_READY) {
        _vibe_sched_dequeue(thread);
    } else if (thread->state == VIBE_THREAD_SLEEPING) {
        /* Remove from sleep list — reuse wait_node for generic removal. */
        extern vibe_list_t g_sleep_list;
        vibe_list_remove(&g_sleep_list, &thread->sleep_node);
    }

    /* Reset priority to base (undo any PI boost). */
    thread->priority = thread->base_priority;
    thread->state    = VIBE_THREAD_SUSPENDED;
    thread->timeout  = 0U;

    /* Re-initialise the stack frame so entry() runs from the beginning. */
    _thread_stack_setup(thread);

    /* Re-enqueue. */
    _vibe_sched_enqueue(thread);

    arch_irq_unlock(key);

    return VIBE_OK;
}

/* -----------------------------------------------------------------------
 * vibe_thread_set_priority()
 * --------------------------------------------------------------------- */

vibe_err_t vibe_thread_set_priority(vibe_thread_t *thread, uint8_t priority)
{
    if (thread == NULL) {
        thread = vibe_thread_self();
    }
    if (priority > VIBE_PRIO_MAX) {
        return VIBE_EINVAL;
    }

    vibe_irq_key_t key = arch_irq_lock();

    bool was_ready = (thread->state == VIBE_THREAD_READY);
    if (was_ready) {
        _vibe_sched_dequeue(thread);
    }

    thread->priority      = priority;
    thread->base_priority = priority;

    if (was_ready) {
        _vibe_sched_enqueue(thread);
    }

    arch_irq_unlock(key);
    _vibe_sched_reschedule();

    return VIBE_OK;
}

/* -----------------------------------------------------------------------
 * Misc accessors
 * --------------------------------------------------------------------- */

bool vibe_thread_is_in_isr(void)
{
    return arch_is_in_isr();
}

vibe_thread_state_t vibe_thread_get_state(const vibe_thread_t *thread)
{
    return thread ? thread->state : VIBE_THREAD_DEAD;
}

const char *vibe_thread_get_name(const vibe_thread_t *thread)
{
    return thread ? thread->name : "(null)";
}

void _vibe_thread_check_canary(const vibe_thread_t *thread)
{
    if (!_thread_stack_check(thread)) {
        vibe_printk("FATAL: stack overflow in thread '%s'\n", thread->name);
        arch_breakpoint();
        for (;;) {}
    }
}
