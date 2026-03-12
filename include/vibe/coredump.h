#ifndef VIBE_COREDUMP_H
#define VIBE_COREDUMP_H

/**
 * @file vibe/coredump.h
 * @brief VibeRTOS coredump subsystem.
 *
 * On a fault (HardFault, MemManage, BusFault, UsageFault, assert, stack
 * overflow) the coredump module captures the full CPU state and stores it
 * in a `.noinit` RAM region that survives a warm reset.  After reboot,
 * call vibe_coredump_dump() or run the "coredump" shell command to inspect
 * the saved state.
 *
 * Storage layout (in .noinit RAM):
 *   vibe_coredump_t   — header + registers + fault regs + stack snapshot
 *
 * Enable with CONFIG_COREDUMP=y in your sdkconfig / menuconfig.
 */

#include <stdint.h>
#include <stdbool.h>
#include "vibe/types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------
 * Magic / versioning
 * ---------------------------------------------------------------------- */

#define VIBE_COREDUMP_MAGIC    0xDEADC0DELU
#define VIBE_COREDUMP_VERSION  1U

/* -------------------------------------------------------------------------
 * Crash reason flags  (OR-able)
 * ---------------------------------------------------------------------- */

#define VIBE_COREDUMP_REASON_HARD_FAULT     (1U << 0)
#define VIBE_COREDUMP_REASON_MEM_FAULT      (1U << 1)
#define VIBE_COREDUMP_REASON_BUS_FAULT      (1U << 2)
#define VIBE_COREDUMP_REASON_USAGE_FAULT    (1U << 3)
#define VIBE_COREDUMP_REASON_STACK_OVERFLOW (1U << 4)
#define VIBE_COREDUMP_REASON_ASSERT         (1U << 5)
#define VIBE_COREDUMP_REASON_WATCHDOG       (1U << 6)
#define VIBE_COREDUMP_REASON_UNKNOWN        (1U << 31)

/* -------------------------------------------------------------------------
 * CPU register snapshot  (ARM Cortex-M)
 * ---------------------------------------------------------------------- */

typedef struct {
    /* --- Hardware exception frame (auto-pushed by CPU onto PSP/MSP) --- */
    uint32_t r0;
    uint32_t r1;
    uint32_t r2;
    uint32_t r3;
    uint32_t r12;
    uint32_t lr;    /**< LR at time of fault (EXC_RETURN value)      */
    uint32_t pc;    /**< PC at time of fault — the faulting address   */
    uint32_t xpsr;  /**< Combined Program Status Register             */
    /* --- Software-saved callee-saved registers (R4–R11) --- */
    uint32_t r4;
    uint32_t r5;
    uint32_t r6;
    uint32_t r7;
    uint32_t r8;
    uint32_t r9;
    uint32_t r10;
    uint32_t r11;
    /* --- Stack pointers and special regs at fault entry --- */
    uint32_t psp;     /**< Process Stack Pointer    */
    uint32_t msp;     /**< Main Stack Pointer       */
    uint32_t primask;
    uint32_t control;
} vibe_coredump_regs_t;

/* -------------------------------------------------------------------------
 * ARM fault status registers
 * ---------------------------------------------------------------------- */

typedef struct {
    uint32_t cfsr;   /**< Configurable Fault Status (UFSR|BFSR|MMFSR) */
    uint32_t hfsr;   /**< HardFault Status Register                    */
    uint32_t dfsr;   /**< Debug Fault Status Register                  */
    uint32_t mmfar;  /**< MemManage Fault Address  (valid if MMARVALID)*/
    uint32_t bfar;   /**< BusFault Address         (valid if BFARVALID)*/
    uint32_t afsr;   /**< Auxiliary Fault Status                       */
} vibe_coredump_fault_regs_t;

/* -------------------------------------------------------------------------
 * Stack snapshot
 * ---------------------------------------------------------------------- */

#ifndef CONFIG_COREDUMP_STACK_SIZE
#define CONFIG_COREDUMP_STACK_SIZE  128U
#endif

typedef struct {
    uint32_t sp;                               /**< SP value at fault           */
    uint32_t captured;                         /**< Actual bytes stored         */
    uint8_t  data[CONFIG_COREDUMP_STACK_SIZE]; /**< Raw stack bytes near fault  */
} vibe_coredump_stack_t;

/* -------------------------------------------------------------------------
 * Assert location
 * ---------------------------------------------------------------------- */

typedef struct {
    char     file[64];
    uint32_t line;
    char     expr[64];
} vibe_coredump_assert_t;

/* -------------------------------------------------------------------------
 * Full coredump record  — lives in .noinit RAM
 * ---------------------------------------------------------------------- */

typedef struct {
    uint32_t                   magic;      /**< VIBE_COREDUMP_MAGIC when valid  */
    uint32_t                   version;
    uint32_t                   reason;     /**< VIBE_COREDUMP_REASON_* flags    */
    uint32_t                   timestamp;  /**< vibe_tick_get() at crash        */
    char                       thread_name[32];
    uint32_t                   thread_id;
    vibe_coredump_regs_t       regs;
    vibe_coredump_fault_regs_t fault_regs;
    vibe_coredump_stack_t      stack;
    vibe_coredump_assert_t     assert_info;/**< Populated on REASON_ASSERT      */
    uint32_t                   crc32;      /**< CRC32 over all fields above     */
} vibe_coredump_t;

/* -------------------------------------------------------------------------
 * Public API
 * ---------------------------------------------------------------------- */

/**
 * @brief Initialise the coredump subsystem.
 *
 * Checks whether a valid coredump was left in .noinit RAM by a previous
 * crash and prints a one-line notice over the log if so.
 * Call early in vibe_init() / application startup.
 */
void vibe_coredump_init(void);

/**
 * @brief Return true if a valid, uncleared coredump is present in RAM.
 */
bool vibe_coredump_is_valid(void);

/**
 * @brief Print the full coredump to the debug console (vibe_printk).
 *
 * Decodes CFSR/HFSR into human-readable strings, prints all registers,
 * and hex-dumps the captured stack region.
 * Safe to call at any time (does not modify the stored coredump).
 */
void vibe_coredump_dump(void);

/**
 * @brief Erase the stored coredump (clears the magic number + CRC).
 */
void vibe_coredump_clear(void);

/**
 * @brief Return a read-only pointer to the raw coredump record.
 *
 * Returns NULL if no valid coredump is present.
 */
const vibe_coredump_t *vibe_coredump_get(void);

/* -------------------------------------------------------------------------
 * Internal / arch hooks  (not for application use)
 * ---------------------------------------------------------------------- */

/**
 * @brief Capture a coredump from an ARM exception frame.
 *
 * Called by the fault handler entry stubs in coredump_arch_arm.c.
 *
 * @param exc_frame  Pointer to the hardware exception frame on the stack.
 *                   Layout: [R0, R1, R2, R3, R12, LR, PC, xPSR]
 * @param psp        Process Stack Pointer at fault entry.
 * @param msp        Main Stack Pointer at fault entry.
 * @param reason     VIBE_COREDUMP_REASON_* flag.
 */
void _vibe_coredump_capture(uint32_t *exc_frame,
                             uint32_t  psp,
                             uint32_t  msp,
                             uint32_t  reason);

/**
 * @brief Capture a coredump from a software assertion failure.
 */
void vibe_coredump_capture_assert(const char *file, int line, const char *expr);

/**
 * @brief Capture a coredump for a detected stack overflow.
 *
 * @param thread_name  Name of the overflowing thread.
 */
void vibe_coredump_capture_stack_overflow(const char *thread_name);

#ifdef __cplusplus
}
#endif

#endif /* VIBE_COREDUMP_H */
