/**
 * @file subsys/coredump/coredump.c
 * @brief VibeRTOS coredump subsystem — capture, store, decode, dump.
 *
 * The coredump record lives in a .noinit RAM section so it survives a
 * warm reset.  After reboot, vibe_coredump_init() notices the valid magic
 * and CRC, logs a warning, and the app (or shell) can call
 * vibe_coredump_dump() to print the full decoded crash report.
 */

#include "vibe/coredump.h"
#include "vibe/sys/printk.h"
#include "vibe/thread.h"
#include "vibe/arch.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* -------------------------------------------------------------------------
 * .noinit placement — this section must NOT be zeroed by startup code.
 * Add  .noinit (NOLOAD) : { *(.noinit) }  to your linker script.
 * ---------------------------------------------------------------------- */

__attribute__((section(".noinit"), used))
static vibe_coredump_t g_coredump;

/* -------------------------------------------------------------------------
 * CRC-32 (IEEE 802.3 polynomial, software implementation)
 * ---------------------------------------------------------------------- */

static uint32_t crc32_update(uint32_t crc, const uint8_t *data, size_t len)
{
    crc = ~crc;
    while (len--) {
        crc ^= *data++;
        for (int i = 0; i < 8; i++) {
            crc = (crc >> 1) ^ (0xEDB88320U & -(crc & 1U));
        }
    }
    return ~crc;
}

static uint32_t coredump_crc(const vibe_coredump_t *cd)
{
    /* CRC over everything except the crc32 field itself */
    size_t len = offsetof(vibe_coredump_t, crc32);
    return crc32_update(0U, (const uint8_t *)cd, len);
}

/* -------------------------------------------------------------------------
 * Validation
 * ---------------------------------------------------------------------- */

bool vibe_coredump_is_valid(void)
{
    if (g_coredump.magic != VIBE_COREDUMP_MAGIC) {
        return false;
    }
    if (g_coredump.version != VIBE_COREDUMP_VERSION) {
        return false;
    }
    return g_coredump.crc32 == coredump_crc(&g_coredump);
}

/* -------------------------------------------------------------------------
 * Init
 * ---------------------------------------------------------------------- */

void vibe_coredump_init(void)
{
    if (vibe_coredump_is_valid()) {
        vibe_printk("\n[coredump] *** CRASH DETECTED from previous boot ***\n");
        vibe_printk("[coredump]     reason=0x%08x  thread='%s'  tick=%u\n",
                    (unsigned)g_coredump.reason,
                    g_coredump.thread_name,
                    (unsigned)g_coredump.timestamp);
        vibe_printk("[coredump]     PC=0x%08x  LR=0x%08x\n",
                    (unsigned)g_coredump.regs.pc,
                    (unsigned)g_coredump.regs.lr);
        vibe_printk("[coredump]     Run 'coredump' shell command for full report.\n\n");
    }
}

/* -------------------------------------------------------------------------
 * Clear
 * ---------------------------------------------------------------------- */

void vibe_coredump_clear(void)
{
    g_coredump.magic = 0U;
    g_coredump.crc32 = 0U;
}

/* -------------------------------------------------------------------------
 * Get raw pointer
 * ---------------------------------------------------------------------- */

const vibe_coredump_t *vibe_coredump_get(void)
{
    return vibe_coredump_is_valid() ? &g_coredump : NULL;
}

/* -------------------------------------------------------------------------
 * Reason string
 * ---------------------------------------------------------------------- */

static const char *reason_str(uint32_t reason)
{
    if (reason & VIBE_COREDUMP_REASON_HARD_FAULT)     return "HardFault";
    if (reason & VIBE_COREDUMP_REASON_MEM_FAULT)      return "MemManageFault";
    if (reason & VIBE_COREDUMP_REASON_BUS_FAULT)      return "BusFault";
    if (reason & VIBE_COREDUMP_REASON_USAGE_FAULT)    return "UsageFault";
    if (reason & VIBE_COREDUMP_REASON_STACK_OVERFLOW) return "StackOverflow";
    if (reason & VIBE_COREDUMP_REASON_ASSERT)         return "AssertFailed";
    if (reason & VIBE_COREDUMP_REASON_WATCHDOG)       return "Watchdog";
    return "Unknown";
}

/* -------------------------------------------------------------------------
 * CFSR decoder
 *
 * CFSR = Configurable Fault Status Register (SCB at 0xE000ED28)
 *   Bits  7: 0  — MMFSR  (MemManage Fault Status)
 *   Bits 15: 8  — BFSR   (BusFault Status)
 *   Bits 31:16  — UFSR   (UsageFault Status)
 * ---------------------------------------------------------------------- */

static void decode_cfsr(uint32_t cfsr)
{
    vibe_printk("  CFSR = 0x%08x\n", (unsigned)cfsr);

    /* MMFSR — MemManage */
    if (cfsr & 0xFFU) {
        vibe_printk("    [MMFSR]\n");
        if (cfsr & (1U << 0)) vibe_printk("      IACCVIOL  — instruction access violation\n");
        if (cfsr & (1U << 1)) vibe_printk("      DACCVIOL  — data access violation\n");
        if (cfsr & (1U << 3)) vibe_printk("      MUNSTKERR — fault on exception unstacking\n");
        if (cfsr & (1U << 4)) vibe_printk("      MSTKERR   — fault on exception stacking\n");
        if (cfsr & (1U << 5)) vibe_printk("      MLSPERR   — lazy FP state preserve fault\n");
        if (cfsr & (1U << 7)) vibe_printk("      MMARVALID — MMFAR holds valid address\n");
    }

    /* BFSR — BusFault */
    if (cfsr & 0xFF00U) {
        vibe_printk("    [BFSR]\n");
        if (cfsr & (1U <<  8)) vibe_printk("      IBUSERR    — instruction bus error\n");
        if (cfsr & (1U <<  9)) vibe_printk("      PRECISERR  — precise data bus error\n");
        if (cfsr & (1U << 10)) vibe_printk("      IMPRECISERR— imprecise data bus error\n");
        if (cfsr & (1U << 11)) vibe_printk("      UNSTKERR   — fault on exception unstacking\n");
        if (cfsr & (1U << 12)) vibe_printk("      STKERR     — fault on exception stacking\n");
        if (cfsr & (1U << 13)) vibe_printk("      LSPERR     — lazy FP state preserve fault\n");
        if (cfsr & (1U << 15)) vibe_printk("      BFARVALID  — BFAR holds valid address\n");
    }

    /* UFSR — UsageFault */
    if (cfsr & 0xFFFF0000U) {
        vibe_printk("    [UFSR]\n");
        if (cfsr & (1U << 16)) vibe_printk("      UNDEFINSTR — undefined instruction\n");
        if (cfsr & (1U << 17)) vibe_printk("      INVSTATE   — invalid EPSR state (bad thumb bit?)\n");
        if (cfsr & (1U << 18)) vibe_printk("      INVPC      — invalid PC load (bad EXC_RETURN?)\n");
        if (cfsr & (1U << 19)) vibe_printk("      NOCP       — no coprocessor\n");
        if (cfsr & (1U << 24)) vibe_printk("      UNALIGNED  — unaligned memory access\n");
        if (cfsr & (1U << 25)) vibe_printk("      DIVBYZERO  — divide by zero\n");
    }
}

static void decode_hfsr(uint32_t hfsr)
{
    vibe_printk("  HFSR = 0x%08x\n", (unsigned)hfsr);
    if (hfsr & (1U <<  1)) vibe_printk("    VECTTBL   — fault on vector table read\n");
    if (hfsr & (1U << 30)) vibe_printk("    FORCED    — escalated from configurable fault\n");
    if (hfsr & (1U << 31)) vibe_printk("    DEBUGEVT  — debug event (DFSR valid)\n");
}

/* -------------------------------------------------------------------------
 * Hex + ASCII stack dump
 * ---------------------------------------------------------------------- */

static void dump_stack(const vibe_coredump_stack_t *st)
{
    vibe_printk("  Stack at SP=0x%08x  (%u bytes captured)\n",
                (unsigned)st->sp, (unsigned)st->captured);
    uint32_t n = st->captured;
    for (uint32_t i = 0; i < n; i += 16U) {
        vibe_printk("  %08x: ", (unsigned)(st->sp + i));
        for (uint32_t j = 0; j < 16U; j++) {
            if (i + j < n) {
                vibe_printk("%02x ", (unsigned)st->data[i + j]);
            } else {
                vibe_printk("   ");
            }
            if (j == 7U) vibe_printk(" ");
        }
        vibe_printk(" |");
        for (uint32_t j = 0; j < 16U && (i + j) < n; j++) {
            uint8_t c = st->data[i + j];
            vibe_printk("%c", (c >= 0x20U && c < 0x7FU) ? (char)c : '.');
        }
        vibe_printk("|\n");
    }
}

/* -------------------------------------------------------------------------
 * vibe_coredump_dump  — full human-readable crash report
 * ---------------------------------------------------------------------- */

void vibe_coredump_dump(void)
{
    if (!vibe_coredump_is_valid()) {
        vibe_printk("[coredump] No valid coredump found.\n");
        return;
    }

    const vibe_coredump_t *cd = &g_coredump;

    vibe_printk("\n");
    vibe_printk("========================================\n");
    vibe_printk("         VibeRTOS Crash Report          \n");
    vibe_printk("========================================\n");

    /* --- Header --- */
    vibe_printk("Reason   : %s (0x%08x)\n",
                reason_str(cd->reason), (unsigned)cd->reason);
    vibe_printk("Timestamp: %u ticks\n", (unsigned)cd->timestamp);
    vibe_printk("Thread   : '%s' (id=%u)\n",
                cd->thread_name, (unsigned)cd->thread_id);

    /* --- Assert info --- */
    if (cd->reason & VIBE_COREDUMP_REASON_ASSERT) {
        vibe_printk("Assert   : %s:%u — %s\n",
                    cd->assert_info.file,
                    (unsigned)cd->assert_info.line,
                    cd->assert_info.expr);
    }

    /* --- CPU Registers --- */
    vibe_printk("\n--- CPU Registers ---\n");
    vibe_printk("  PC  = 0x%08x   LR  = 0x%08x\n",
                (unsigned)cd->regs.pc, (unsigned)cd->regs.lr);
    vibe_printk("  SP  = 0x%08x   PSP = 0x%08x   MSP = 0x%08x\n",
                (unsigned)cd->regs.psp,
                (unsigned)cd->regs.psp,
                (unsigned)cd->regs.msp);
    vibe_printk("  xPSR= 0x%08x   PRIMASK=0x%x  CONTROL=0x%x\n",
                (unsigned)cd->regs.xpsr,
                (unsigned)cd->regs.primask,
                (unsigned)cd->regs.control);
    vibe_printk("  R0  = 0x%08x   R1  = 0x%08x\n",
                (unsigned)cd->regs.r0, (unsigned)cd->regs.r1);
    vibe_printk("  R2  = 0x%08x   R3  = 0x%08x\n",
                (unsigned)cd->regs.r2, (unsigned)cd->regs.r3);
    vibe_printk("  R4  = 0x%08x   R5  = 0x%08x\n",
                (unsigned)cd->regs.r4, (unsigned)cd->regs.r5);
    vibe_printk("  R6  = 0x%08x   R7  = 0x%08x\n",
                (unsigned)cd->regs.r6, (unsigned)cd->regs.r7);
    vibe_printk("  R8  = 0x%08x   R9  = 0x%08x\n",
                (unsigned)cd->regs.r8, (unsigned)cd->regs.r9);
    vibe_printk("  R10 = 0x%08x   R11 = 0x%08x   R12 = 0x%08x\n",
                (unsigned)cd->regs.r10,
                (unsigned)cd->regs.r11,
                (unsigned)cd->regs.r12);

    /* --- Fault Status Registers --- */
    vibe_printk("\n--- Fault Status Registers ---\n");
    decode_cfsr(cd->fault_regs.cfsr);
    decode_hfsr(cd->fault_regs.hfsr);
    if (cd->fault_regs.cfsr & (1U << 7)) {  /* MMARVALID */
        vibe_printk("  MMFAR = 0x%08x  (MemManage fault address)\n",
                    (unsigned)cd->fault_regs.mmfar);
    }
    if (cd->fault_regs.cfsr & (1U << 15)) { /* BFARVALID */
        vibe_printk("  BFAR  = 0x%08x  (BusFault address)\n",
                    (unsigned)cd->fault_regs.bfar);
    }

    /* --- Stack snapshot --- */
    vibe_printk("\n--- Stack Snapshot ---\n");
    dump_stack(&cd->stack);

    vibe_printk("========================================\n\n");
}

/* -------------------------------------------------------------------------
 * _vibe_coredump_capture  — called from arch fault entry stubs
 * ---------------------------------------------------------------------- */

void _vibe_coredump_capture(uint32_t *exc_frame,
                             uint32_t  psp,
                             uint32_t  msp,
                             uint32_t  reason)
{
    vibe_coredump_t *cd = &g_coredump;

    /* Invalidate first so a power cut during write doesn't leave garbage */
    cd->magic = 0U;

    cd->version   = VIBE_COREDUMP_VERSION;
    cd->reason    = reason;

    /* Tick counter — may be 0 if fault happened very early */
    extern vibe_tick_t vibe_tick_get(void);
    cd->timestamp = (uint32_t)vibe_tick_get();

    /* Current thread info */
    extern vibe_thread_t *vibe_thread_self(void);
    vibe_thread_t *t = vibe_thread_self();
    if (t && t->name[0]) {
        strncpy(cd->thread_name, t->name, sizeof(cd->thread_name) - 1U);
        cd->thread_name[sizeof(cd->thread_name) - 1U] = '\0';
        cd->thread_id = (uint32_t)(uintptr_t)t;
    } else {
        strncpy(cd->thread_name, "<none>", sizeof(cd->thread_name) - 1U);
        cd->thread_id = 0U;
    }

    /* CPU registers from exception frame */
    if (exc_frame) {
        cd->regs.r0   = exc_frame[0];
        cd->regs.r1   = exc_frame[1];
        cd->regs.r2   = exc_frame[2];
        cd->regs.r3   = exc_frame[3];
        cd->regs.r12  = exc_frame[4];
        cd->regs.lr   = exc_frame[5];
        cd->regs.pc   = exc_frame[6];
        cd->regs.xpsr = exc_frame[7];
    }
    cd->regs.psp = psp;
    cd->regs.msp = msp;

    /* Callee-saved regs: not available without deeper stack unwinding.
     * Leave zeroed — a debugger can reconstruct from the stack snapshot. */

    /* Special registers */
    __asm__ volatile("mrs %0, PRIMASK"  : "=r"(cd->regs.primask));
    __asm__ volatile("mrs %0, CONTROL"  : "=r"(cd->regs.control));

    /* Fault status registers (SCB base 0xE000ED00) */
    volatile uint32_t *SCB_CFSR  = (volatile uint32_t *)0xE000ED28U;
    volatile uint32_t *SCB_HFSR  = (volatile uint32_t *)0xE000ED2CU;
    volatile uint32_t *SCB_DFSR  = (volatile uint32_t *)0xE000ED30U;
    volatile uint32_t *SCB_MMFAR = (volatile uint32_t *)0xE000ED34U;
    volatile uint32_t *SCB_BFAR  = (volatile uint32_t *)0xE000ED38U;
    volatile uint32_t *SCB_AFSR  = (volatile uint32_t *)0xE000ED3CU;

    cd->fault_regs.cfsr  = *SCB_CFSR;
    cd->fault_regs.hfsr  = *SCB_HFSR;
    cd->fault_regs.dfsr  = *SCB_DFSR;
    cd->fault_regs.mmfar = *SCB_MMFAR;
    cd->fault_regs.bfar  = *SCB_BFAR;
    cd->fault_regs.afsr  = *SCB_AFSR;

    /* Clear fault status registers so they don't linger */
    *SCB_CFSR = cd->fault_regs.cfsr;  /* write-1-to-clear */
    *SCB_HFSR = cd->fault_regs.hfsr;

    /* Stack snapshot: capture bytes at the faulting SP */
    uint32_t sp = psp ? psp : msp;
    cd->stack.sp = sp;
    uint32_t avail = CONFIG_COREDUMP_STACK_SIZE;
    cd->stack.captured = avail;
    /* Safe byte-by-byte copy — SP may point near end of RAM */
    uint8_t *src = (uint8_t *)(uintptr_t)sp;
    for (uint32_t i = 0; i < avail; i++) {
        cd->stack.data[i] = src[i];
    }

    /* Seal with magic + CRC */
    cd->magic = VIBE_COREDUMP_MAGIC;
    cd->crc32 = coredump_crc(cd);

    /* Immediately print to UART before any reset */
    vibe_coredump_dump();
}

/* -------------------------------------------------------------------------
 * Assert capture
 * ---------------------------------------------------------------------- */

void vibe_coredump_capture_assert(const char *file, int line, const char *expr)
{
    vibe_coredump_t *cd = &g_coredump;
    cd->magic  = 0U;
    cd->reason = VIBE_COREDUMP_REASON_ASSERT;

    strncpy(cd->assert_info.file, file ? file : "?",
            sizeof(cd->assert_info.file) - 1U);
    cd->assert_info.file[sizeof(cd->assert_info.file) - 1U] = '\0';
    cd->assert_info.line = (uint32_t)line;
    strncpy(cd->assert_info.expr, expr ? expr : "?",
            sizeof(cd->assert_info.expr) - 1U);
    cd->assert_info.expr[sizeof(cd->assert_info.expr) - 1U] = '\0';

    /* Capture with zeroed registers — no exception frame for asserts */
    _vibe_coredump_capture(NULL, 0U, 0U, VIBE_COREDUMP_REASON_ASSERT);
}

/* -------------------------------------------------------------------------
 * Stack overflow capture
 * ---------------------------------------------------------------------- */

void vibe_coredump_capture_stack_overflow(const char *thread_name)
{
    vibe_coredump_t *cd = &g_coredump;
    cd->magic = 0U;
    if (thread_name) {
        strncpy(cd->thread_name, thread_name, sizeof(cd->thread_name) - 1U);
        cd->thread_name[sizeof(cd->thread_name) - 1U] = '\0';
    }
    _vibe_coredump_capture(NULL, 0U, 0U, VIBE_COREDUMP_REASON_STACK_OVERFLOW);
}
