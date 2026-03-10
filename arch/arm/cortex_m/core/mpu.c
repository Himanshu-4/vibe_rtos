/**
 * @file arch/arm/cortex_m/core/mpu.c
 * @brief ARM Cortex-M MPU (Memory Protection Unit) stub.
 *
 * =============================================================================
 * USER: Implement the MPU region configuration for your device.
 *
 * The MPU allows you to:
 *  - Protect kernel code from application writes
 *  - Detect stack overflows via a guard region at the stack bottom
 *  - Enforce read-only regions (e.g., flash)
 *  - Isolate threads from each other (requires per-context MPU reconfigure)
 *
 * Not all Cortex-M cores have an MPU:
 *  - Cortex-M0+: No MPU (RP2040 does not have one)
 *  - Cortex-M33: Has MPU (RP2350)
 *  - Cortex-M4:  Has MPU
 *
 * This file is compiled when CONFIG_HAS_MPU=y.
 * =============================================================================
 */

#include "arch/arm/cortex_m/arch.h"
#include "vibe/types.h"
#include "vibe/sys/printk.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef CONFIG_HAS_MPU

/* -----------------------------------------------------------------------
 * MPU register map (Cortex-M ARMv7-M / ARMv8-M compatible)
 * --------------------------------------------------------------------- */

typedef struct {
    volatile uint32_t TYPE;     /* 0x00 — MPU type (num regions) */
    volatile uint32_t CTRL;     /* 0x04 — MPU control */
    volatile uint32_t RNR;      /* 0x08 — region number */
    volatile uint32_t RBAR;     /* 0x0C — region base address */
    volatile uint32_t RASR;     /* 0x10 — region attribute and size */
} _MPU_t;

#define MPU  ((_MPU_t *)0xE000ED90U)

/* MPU CTRL bits */
#define MPU_CTRL_ENABLE         BIT(0)
#define MPU_CTRL_HFNMIENA       BIT(1)  /* Enable MPU during HardFault/NMI */
#define MPU_CTRL_PRIVDEFENA     BIT(2)  /* Default memory map for privileged */

/* MPU RBAR bits */
#define MPU_RBAR_VALID          BIT(4)

/* MPU RASR bits */
#define MPU_RASR_ENABLE         BIT(0)
#define MPU_RASR_SIZE_SHIFT     1U
#define MPU_RASR_AP_SHIFT       24U
#define MPU_RASR_XN             BIT(28) /* Execute Never */

/* MPU Access Permissions (AP field) */
#define MPU_AP_NO_ACCESS        (0x0U << MPU_RASR_AP_SHIFT)
#define MPU_AP_PRIV_RW          (0x1U << MPU_RASR_AP_SHIFT)
#define MPU_AP_PRIV_RW_UNPRIV_RO (0x2U << MPU_RASR_AP_SHIFT)
#define MPU_AP_FULL_ACCESS      (0x3U << MPU_RASR_AP_SHIFT)
#define MPU_AP_PRIV_RO          (0x5U << MPU_RASR_AP_SHIFT)
#define MPU_AP_RO               (0x6U << MPU_RASR_AP_SHIFT)

/* -----------------------------------------------------------------------
 * MPU region configuration struct
 * --------------------------------------------------------------------- */

typedef struct {
    uint32_t base_addr;    /**< Region base address (must be size-aligned). */
    uint32_t size_bytes;   /**< Region size (must be power of 2, >= 32). */
    uint32_t ap;           /**< Access permissions (MPU_AP_* macros). */
    bool     execute;      /**< Allow instruction fetch from this region. */
    bool     cacheable;    /**< Memory is cacheable (set B, C bits). */
    bool     bufferable;   /**< Memory is bufferable. */
} vibe_mpu_region_t;

/* -----------------------------------------------------------------------
 * Internal: size in bytes to MPU SIZE field value
 * --------------------------------------------------------------------- */

static uint32_t _bytes_to_mpu_size(uint32_t bytes)
{
    /*
     * MPU SIZE field encodes log2(size) - 1.
     * E.g., 32 bytes = 2^5 -> SIZE = 4
     *       256 bytes = 2^8 -> SIZE = 7
     *       4 KB = 2^12    -> SIZE = 11
     */
    if (bytes < 32U) {
        return 4U; /* Minimum region size */
    }
    uint32_t sz = 0U;
    uint32_t v  = bytes;
    while (v > 1U) {
        v >>= 1U;
        sz++;
    }
    return sz - 1U;
}

/* -----------------------------------------------------------------------
 * arch_mpu_enable()
 * --------------------------------------------------------------------- */

void arch_mpu_enable(void)
{
    uint32_t num_regions = (MPU->TYPE >> 8U) & 0xFFU;

    if (num_regions == 0U) {
        vibe_printk("[mpu] No MPU regions available — MPU not present\n");
        return;
    }

    vibe_printk("[mpu] Enabling MPU (%u regions)\n", (unsigned)num_regions);

    /*
     * TODO (USER): Configure your regions before calling this function.
     *
     * Suggested region layout for RP2350:
     *   Region 0: Flash   — 0x10000000, 2MB, RO, executable
     *   Region 1: SRAM    — 0x20000000, 520KB, RW, not executable
     *   Region 2: APB     — 0x40000000, 256MB, RW device, not executable
     *   Region 3: SIO     — 0xD0000000, 4KB,  RW device, not executable
     *   Region 4: Stack guard — bottom of current thread stack, 32B, NO ACCESS
     */

    MPU->CTRL = MPU_CTRL_ENABLE | MPU_CTRL_PRIVDEFENA;

    /* Data synchronisation and instruction synchronisation barriers */
    __asm__ volatile("dsb" ::: "memory");
    __asm__ volatile("isb" ::: "memory");
}

/* -----------------------------------------------------------------------
 * arch_mpu_configure_region()
 * --------------------------------------------------------------------- */

void arch_mpu_configure_region(uint32_t region_num, const vibe_mpu_region_t *cfg)
{
    if (cfg == NULL || region_num >= 8U) {
        return;
    }

    /*
     * TODO (USER): Validate that base_addr is aligned to size.
     * The hardware silently ignores misaligned regions.
     */

    uint32_t size_enc = _bytes_to_mpu_size(cfg->size_bytes);

    /* Select the region */
    MPU->RNR  = region_num;

    /* Set base address */
    MPU->RBAR = cfg->base_addr & ~0x1FU; /* Clear lower 5 bits */

    /* Set attributes */
    uint32_t rasr = MPU_RASR_ENABLE
                  | (size_enc << MPU_RASR_SIZE_SHIFT)
                  | cfg->ap;

    if (!cfg->execute) {
        rasr |= MPU_RASR_XN;
    }
    if (cfg->cacheable) {
        rasr |= BIT(17); /* C bit */
    }
    if (cfg->bufferable) {
        rasr |= BIT(16); /* B bit */
    }

    MPU->RASR = rasr;
}

/* -----------------------------------------------------------------------
 * arch_mpu_disable_region()
 * --------------------------------------------------------------------- */

void arch_mpu_disable_region(uint32_t region_num)
{
    if (region_num >= 8U) {
        return;
    }
    MPU->RNR  = region_num;
    MPU->RASR = 0U; /* Clear ENABLE bit */
}

#else /* !CONFIG_HAS_MPU */

/* Stubs for platforms without an MPU (e.g., Cortex-M0+) */

void arch_mpu_enable(void)
{
    /* No MPU on this platform */
}

void arch_mpu_configure_region(uint32_t region_num,
                                const void *cfg)
{
    (void)region_num;
    (void)cfg;
}

#endif /* CONFIG_HAS_MPU */
