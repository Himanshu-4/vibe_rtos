#ifndef ARCH_ARM_CORTEX_M_ARCH_H
#define ARCH_ARM_CORTEX_M_ARCH_H

/**
 * @file arch/arm/cortex_m/include/arch/arm/cortex_m/arch.h
 * @brief Architecture interface that the VibeRTOS kernel calls into.
 *
 * ALL functions declared here must be implemented by the user in the
 * arch/arm/cortex_m/core/ source files.
 *
 * This header is the only architectural dependency of the kernel.
 * Porting to a new architecture means providing a new version of this
 * interface with the same function signatures.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declaration */
struct vibe_thread;

/* -----------------------------------------------------------------------
 * Utility macros used by arch code
 * --------------------------------------------------------------------- */

#ifndef BIT
#define BIT(n)  (1UL << (n))
#endif

/* -----------------------------------------------------------------------
 * Core arch interface — MUST be implemented by the user
 * --------------------------------------------------------------------- */

/**
 * @brief Architecture early initialisation.
 *
 * Called first from vibe_init() before any kernel objects are created.
 * Typical tasks:
 *  - Set VTOR (Vector Table Offset Register)
 *  - Configure NVIC priority grouping
 *  - Set PendSV to the lowest priority
 *
 * USER: implement in core/irq.c
 */
void arch_init(void);

/**
 * @brief Initialise a thread's stack frame for first-time execution.
 *
 * Sets up an exception return stack frame on the thread's stack so that
 * when the context switch code restores it, the CPU will start executing
 * thread->entry(thread->arg).
 *
 * On Cortex-M the initial frame contains (from low to high address):
 *   R0 (arg), R1, R2, R3, R12, LR (0xFFFFFFFD = EXC_RETURN),
 *   PC (entry), xPSR (thumb bit set = 0x01000000)
 *
 * thread->stack_ptr should be set to the top of this frame after init.
 *
 * USER: implement in core/context_switch.S or core/irq.c
 *
 * @param thread  Thread whose stack to initialise.
 */
void arch_thread_stack_init(struct vibe_thread *thread);

/**
 * @brief Perform a context switch from one thread to another.
 *
 * Save the current CPU state (R4-R11, LR, PSP) into from->stack_ptr,
 * then restore it from to->stack_ptr and resume execution.
 *
 * On Cortex-M this is typically triggered by pending PendSV (which runs
 * at the lowest priority). The PendSV_Handler saves/restores context.
 *
 * @param from  Thread being switched away from (may be NULL on first switch).
 * @param to    Thread to switch to.
 *
 * USER: implement in core/context_switch.S
 */
void arch_switch_to(struct vibe_thread *from, struct vibe_thread *to);

/**
 * @brief Put the CPU into a low-power wait state until the next interrupt.
 *
 * On Cortex-M: execute the WFI (Wait For Interrupt) instruction.
 * Called from the idle thread.
 *
 * USER: implement in core/irq.c (trivially: __asm__("wfi"))
 */
void arch_cpu_idle(void);

/**
 * @brief Disable all maskable interrupts and return the previous state.
 *
 * On Cortex-M: CPSID I (sets PRIMASK = 1).
 * Returns the old PRIMASK value so it can be restored.
 *
 * @return  Previous PRIMASK value (0 = IRQs were enabled, 1 = disabled).
 *
 * USER: implement with inline assembly in core/irq.c
 */
uint32_t arch_irq_lock(void);

/**
 * @brief Restore interrupt state from a previously saved PRIMASK value.
 *
 * On Cortex-M: write key back to PRIMASK (CPSIE I if key == 0).
 *
 * @param key  Value returned by arch_irq_lock().
 *
 * USER: implement with inline assembly in core/irq.c
 */
void arch_irq_unlock(uint32_t key);

/**
 * @brief Check whether the CPU is currently executing in an ISR.
 *
 * On Cortex-M: read the IPSR register. If IPSR != 0, we are in an exception.
 *
 * @return  true if in ISR/exception context.
 *
 * USER: implement in core/irq.c
 */
bool arch_is_in_isr(void);

/**
 * @brief Program the hardware timer for the next tickless-idle wakeup.
 *
 * Called by the idle thread (and by thread sleep) when CONFIG_TICKLESS_IDLE
 * is enabled. The hardware timer should fire after ticks_from_now system
 * ticks, generating the next SysTick (or equivalent) interrupt.
 *
 * @param ticks_from_now  Number of ticks until next required wakeup.
 *
 * USER: implement in core/irq.c — typically reprograms SysTick LOAD register.
 */
void arch_set_next_tick(uint32_t ticks_from_now);

/**
 * @brief Trigger a system reset / reboot.
 *
 * On Cortex-M: write to SCB->AIRCR with SYSRESETREQ.
 * This function does not return.
 *
 * USER: implement in core/irq.c
 */
void arch_reboot(void) __attribute__((noreturn));

/**
 * @brief Insert a breakpoint / halt for debug.
 *
 * On Cortex-M: execute BKPT #0 or an infinite loop.
 * Called by VIBE_ASSERT on failure.
 *
 * USER: implement in core/irq.c
 */
void arch_breakpoint(void);

/* -----------------------------------------------------------------------
 * NVIC helpers
 * --------------------------------------------------------------------- */

/**
 * @brief Enable an IRQ in the Nested Vectored Interrupt Controller.
 *
 * Writes to NVIC->ISER[irq / 32], setting bit (irq % 32).
 *
 * @param irq  IRQ number (0-based, not including Cortex-M system exceptions).
 *
 * USER: implement in core/irq.c
 */
void arch_nvic_enable_irq(uint32_t irq);

/**
 * @brief Disable an IRQ in the NVIC.
 *
 * @param irq  IRQ number.
 *
 * USER: implement in core/irq.c
 */
void arch_nvic_disable_irq(uint32_t irq);

/**
 * @brief Set the priority of an IRQ in the NVIC.
 *
 * @param irq       IRQ number.
 * @param priority  Priority value (0 = highest, 255 = lowest; only upper
 *                  CONFIG_NVIC_PRIO_BITS bits are significant).
 *
 * USER: implement in core/irq.c
 */
void arch_nvic_set_priority(uint32_t irq, uint32_t priority);

/* -----------------------------------------------------------------------
 * SysTick
 * --------------------------------------------------------------------- */

/**
 * @brief Initialise and start the SysTick timer.
 *
 * Configures the SysTick reload value so that it fires at ticks_per_sec Hz.
 * Also sets SysTick priority to just above PendSV.
 *
 * Assumes the SysTick clock source is the processor clock (AHB).
 *
 * @param ticks_per_sec  Desired tick frequency in Hz (e.g., 1000 for 1ms tick).
 *
 * USER: implement in core/irq.c
 *   Typical: SysTick->LOAD = (SystemCoreClock / ticks_per_sec) - 1;
 *            SysTick->VAL  = 0;
 *            SysTick->CTRL = SYSTICK_CTRL_ENABLE | SYSTICK_CTRL_TICKINT | SYSTICK_CTRL_CLKSOURCE;
 */
void arch_systick_init(uint32_t ticks_per_sec);

/* -----------------------------------------------------------------------
 * SysTick ISR — implemented in core/irq.c, calls kernel tick handler
 * --------------------------------------------------------------------- */

/**
 * @brief SysTick exception handler.
 *
 * Linked into the vector table as SysTick_Handler.
 * Calls _vibe_sys_tick_handler() (defined in kernel/init.c).
 *
 * USER: declared here for reference; define in core/irq.c or startup.S.
 */
void SysTick_Handler(void);

#ifdef __cplusplus
}
#endif

#endif /* ARCH_ARM_CORTEX_M_ARCH_H */
