/**
 * @file subsys/rtt/rtt.c
 * @brief SEGGER-compatible Real-Time Transfer (RTT) implementation.
 *
 * The control block is a global named _SEGGER_RTT with the standard
 * "SEGGER RTT" identifier, so J-Link, OpenOCD, and pyOCD locate it by
 * symbol or by RAM scan with zero configuration.
 *
 * The ID string is assembled at runtime (not stored as one literal) so a
 * RAM scan cannot match a stale copy of the string sitting in the flash
 * image — the same trick SEGGER's reference implementation uses.
 */

#include "vibe/rtt.h"
#include "vibe/irq.h"
#include "vibe/sys/printk.h"
#include <string.h>

#ifdef CONFIG_RTT

#ifndef CONFIG_RTT_BUFFER_SIZE_UP
#define CONFIG_RTT_BUFFER_SIZE_UP    1024
#endif
#ifndef CONFIG_RTT_BUFFER_SIZE_DOWN
#define CONFIG_RTT_BUFFER_SIZE_DOWN  64
#endif

/* -----------------------------------------------------------------------
 * Storage
 * --------------------------------------------------------------------- */

/* Channel-0 ring buffers ("Terminal"). */
static char _rtt_up0_storage[CONFIG_RTT_BUFFER_SIZE_UP];
static char _rtt_down0_storage[CONFIG_RTT_BUFFER_SIZE_DOWN];

/*
 * The control block. The symbol name _SEGGER_RTT is part of the host-tool
 * contract — J-Link looks this symbol up in the ELF, OpenOCD/pyOCD scan
 * RAM for the acID string.
 */
vibe_rtt_cb_t _SEGGER_RTT;

static bool _rtt_ready;

/* -----------------------------------------------------------------------
 * Init
 * --------------------------------------------------------------------- */

void vibe_rtt_init(void)
{
    vibe_irq_key_t key = vibe_irq_lock();

    if (_rtt_ready) {
        vibe_irq_unlock(key);
        return;
    }

    vibe_rtt_cb_t *cb = &_SEGGER_RTT;
    memset(cb, 0, sizeof(*cb));

    cb->MaxNumUpBuffers   = CONFIG_RTT_MAX_UP_BUFFERS;
    cb->MaxNumDownBuffers = CONFIG_RTT_MAX_DOWN_BUFFERS;

    cb->aUp[0].sName        = "Terminal";
    cb->aUp[0].pBuffer      = _rtt_up0_storage;
    cb->aUp[0].SizeOfBuffer = sizeof(_rtt_up0_storage);
    cb->aUp[0].Flags        = VIBE_RTT_MODE_SKIP;

    cb->aDown[0].sName        = "Terminal";
    cb->aDown[0].pBuffer      = _rtt_down0_storage;
    cb->aDown[0].SizeOfBuffer = sizeof(_rtt_down0_storage);
    cb->aDown[0].Flags        = VIBE_RTT_MODE_SKIP;

    /* Write the ID last, assembled in two pieces so the complete
     * "SEGGER RTT" string never appears in flash. Host tools only accept
     * the block once the full ID is present. */
    strcpy(cb->acID, "SEGGER");
    cb->acID[6] = ' ';
    cb->acID[7] = 'R';
    cb->acID[8] = 'T';
    cb->acID[9] = 'T';
    cb->acID[10] = '\0';

    _rtt_ready = true;

    vibe_irq_unlock(key);
}

/* -----------------------------------------------------------------------
 * Write (target -> host)
 * --------------------------------------------------------------------- */

unsigned int vibe_rtt_write(unsigned int channel,
                            const void *data, unsigned int len)
{
    if (!_rtt_ready) {
        vibe_rtt_init();
    }
    if (channel >= (unsigned int)CONFIG_RTT_MAX_UP_BUFFERS ||
        _SEGGER_RTT.aUp[channel].pBuffer == NULL) {
        return 0U;
    }

    vibe_rtt_up_t *up  = &_SEGGER_RTT.aUp[channel];
    const char    *src = data;
    unsigned int   written = 0U;

    vibe_irq_key_t key = vibe_irq_lock();

    while (written < len) {
        unsigned int wr = up->WrOff;
        unsigned int rd = up->RdOff;

        /* Free space, leaving one byte so WrOff == RdOff means "empty". */
        unsigned int space = (rd > wr) ? (rd - wr - 1U)
                                       : (up->SizeOfBuffer - wr + rd - 1U);
        if (space == 0U) {
            break; /* SKIP/TRIM: drop what does not fit. */
        }

        /* Contiguous run until wrap or until RdOff. */
        unsigned int run = up->SizeOfBuffer - wr;
        if (run > space) {
            run = space;
        }
        if (run > len - written) {
            run = len - written;
        }

        memcpy(&up->pBuffer[wr], &src[written], run);
        written += run;

        wr += run;
        if (wr >= up->SizeOfBuffer) {
            wr = 0U;
        }
        up->WrOff = wr;
    }

    vibe_irq_unlock(key);
    return written;
}

/* -----------------------------------------------------------------------
 * Read (host -> target)
 * --------------------------------------------------------------------- */

unsigned int vibe_rtt_read(unsigned int channel, void *buf, unsigned int len)
{
    if (!_rtt_ready) {
        vibe_rtt_init();
    }
    if (channel >= (unsigned int)CONFIG_RTT_MAX_DOWN_BUFFERS ||
        _SEGGER_RTT.aDown[channel].pBuffer == NULL) {
        return 0U;
    }

    vibe_rtt_down_t *down = &_SEGGER_RTT.aDown[channel];
    char            *dst  = buf;
    unsigned int     read = 0U;

    vibe_irq_key_t key = vibe_irq_lock();

    unsigned int rd = down->RdOff;
    unsigned int wr = down->WrOff;

    while (read < len && rd != wr) {
        dst[read++] = down->pBuffer[rd++];
        if (rd >= down->SizeOfBuffer) {
            rd = 0U;
        }
    }
    down->RdOff = rd;

    vibe_irq_unlock(key);
    return read;
}

/* -----------------------------------------------------------------------
 * Introspection
 * --------------------------------------------------------------------- */

unsigned int vibe_rtt_up_used(unsigned int channel)
{
    if (!_rtt_ready ||
        channel >= (unsigned int)CONFIG_RTT_MAX_UP_BUFFERS) {
        return 0U;
    }

    const vibe_rtt_up_t *up = &_SEGGER_RTT.aUp[channel];
    unsigned int wr = up->WrOff;
    unsigned int rd = up->RdOff;

    return (wr >= rd) ? (wr - rd) : (up->SizeOfBuffer - rd + wr);
}

/* -----------------------------------------------------------------------
 * Optional console routing
 * --------------------------------------------------------------------- */

#ifdef CONFIG_RTT_CONSOLE
/* Strong override of the weak hook in kernel/init.c. Do not combine with
 * a board/SoC console that also defines vibe_console_putc (link error —
 * exactly one console backend may own the hook). */
void vibe_console_putc(char c)
{
    vibe_rtt_putc(c);
}
#endif

#endif /* CONFIG_RTT */
