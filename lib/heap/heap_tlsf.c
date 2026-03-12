/**
 * @file lib/heap/heap_tlsf.c
 * @brief TLSF (Two-Level Segregated Fit) heap allocator for VibeRTOS.
 *
 * Based on: "TLSF: a New Dynamic Memory Allocator for Real-Time Systems"
 *            Masmano, Ripoll, Crespo, Real — ECRTS 2004.
 *
 * Properties:
 *   - O(1) alloc and free — no loops, no searching
 *   - Bounded worst-case fragmentation (~25% waste in the worst case)
 *   - Suitable for hard real-time use
 *
 * Parameters (embedded-tuned for Cortex-M):
 *   FL_INDEX_SHIFT    = 3   → minimum block payload = 8 bytes (= 2 pointers)
 *   FL_INDEX_MAX      = 22  → maximum managed block ≈ 4 MB
 *   SL_INDEX_COUNT_LOG2 = 3 → 8 second-level sub-classes per FL class
 *
 * Block layout in memory:
 *
 *   +------------------------+   <- tlsf_block_t *  (physical header)
 *   | prev_phys  (4 bytes)   |   previous physical block (NULL = first)
 *   | size_flags (4 bytes)   |   bit0=IS_FREE  bit1=PREV_FREE  bits2+=size
 *   +------------------------+   <- user pointer (when allocated)
 *   | next_free  (4 bytes)   |   \  only valid in free blocks; these
 *   | prev_free  (4 bytes)   |   /  bytes are the user payload when used
 *   | ...payload...          |
 *   +------------------------+
 *
 * The control structure (tlsf_ctl_t) is placed at the very start of the
 * heap pool.  Usable memory begins immediately after it.
 */

#include "vibe/heap.h"
#include "vibe/spinlock.h"
#include "vibe/sys/printk.h"
#include <string.h>
#include <stdint.h>

#if defined(CONFIG_HEAP_MEM) && defined(CONFIG_HEAP_ALLOCATOR_TLSF)

#ifndef CONFIG_HEAP_SIZE
#define CONFIG_HEAP_SIZE 4096
#endif

/* =========================================================================
 * TLSF parameters
 * ====================================================================== */

#define FL_INDEX_SHIFT       3          /* log2(minimum payload size = 8)  */
#define SL_INDEX_COUNT_LOG2  3          /* 8 second-level lists             */
#define SL_INDEX_COUNT       (1U << SL_INDEX_COUNT_LOG2)   /* 8  */
#define FL_INDEX_MAX         22         /* max block ~4 MB                  */
#define FL_INDEX_COUNT       (FL_INDEX_MAX - FL_INDEX_SHIFT + 1)  /* 20   */

#define HEAP_ALIGN           8U
#define HEAP_ALIGN_MASK      (HEAP_ALIGN - 1U)
#define HEAP_ALIGN_UP(x)     (((size_t)(x) + HEAP_ALIGN_MASK) & ~(size_t)HEAP_ALIGN_MASK)

/* Flags packed in the low 2 bits of size_flags */
#define BLOCK_IS_FREE        (1U)
#define BLOCK_PREV_FREE      (2U)
#define BLOCK_SIZE_MASK      (~(size_t)(BLOCK_IS_FREE | BLOCK_PREV_FREE))

/* Minimum payload must accommodate two free-list pointers */
#define BLOCK_PAYLOAD_MIN    (sizeof(void *) * 2U)
#define BLOCK_HDR_OFFSET     (offsetof(tlsf_block_t, next_free))

/* =========================================================================
 * Block structure
 * ====================================================================== */

typedef struct tlsf_block {
    struct tlsf_block *prev_phys;  /**< Previous block in physical address order */
    size_t             size_flags; /**< Payload size | IS_FREE | PREV_FREE       */
    struct tlsf_block *next_free;  /**< Next block in free list  (free only)     */
    struct tlsf_block *prev_free;  /**< Prev block in free list  (free only)     */
} tlsf_block_t;

/* Sentinel block — marks end of pool (size=0, permanently "used") */
#define TLSF_SENTINEL_SIZE 0U

/* =========================================================================
 * Control structure — lives at start of pool
 * ====================================================================== */

typedef struct {
    uint32_t     fl_bitmap;                           /* first-level bitmap  */
    uint32_t     sl_bitmap[FL_INDEX_COUNT];           /* second-level bitmap */
    tlsf_block_t *free[FL_INDEX_COUNT][SL_INDEX_COUNT]; /* free-list heads  */
    /* Null block: used as list terminator / sentinel */
    tlsf_block_t  null_block;
} tlsf_ctl_t;

/* =========================================================================
 * Global pool
 * ====================================================================== */

static uint8_t _Alignas(HEAP_ALIGN) g_heap_pool[CONFIG_HEAP_SIZE];
static vibe_spinlock_t g_lock = VIBE_SPINLOCK_INIT;
static tlsf_ctl_t     *g_ctl  = NULL;   /* points into g_heap_pool */

/* =========================================================================
 * Helper: block size accessors
 * ====================================================================== */

static inline size_t block_size(const tlsf_block_t *b)
{
    return b->size_flags & BLOCK_SIZE_MASK;
}

static inline void block_set_size(tlsf_block_t *b, size_t sz)
{
    b->size_flags = (b->size_flags & ~BLOCK_SIZE_MASK) | (sz & BLOCK_SIZE_MASK);
}

static inline int block_is_free(const tlsf_block_t *b)
{
    return (int)(b->size_flags & BLOCK_IS_FREE);
}

static inline void block_set_free(tlsf_block_t *b)
{
    b->size_flags |= BLOCK_IS_FREE;
}

static inline void block_set_used(tlsf_block_t *b)
{
    b->size_flags &= ~(size_t)BLOCK_IS_FREE;
}

static inline int block_prev_is_free(const tlsf_block_t *b)
{
    return (int)(b->size_flags & BLOCK_PREV_FREE);
}

static inline void block_set_prev_free(tlsf_block_t *b)
{
    b->size_flags |= BLOCK_PREV_FREE;
}

static inline void block_set_prev_used(tlsf_block_t *b)
{
    b->size_flags &= ~(size_t)BLOCK_PREV_FREE;
}

/* Physical next block (pointer arithmetic from header + payload). */
static inline tlsf_block_t *block_next(tlsf_block_t *b)
{
    return (tlsf_block_t *)((uint8_t *)b + BLOCK_HDR_OFFSET + block_size(b));
}

/* Convert user pointer ↔ block header */
static inline void *block_to_ptr(tlsf_block_t *b)
{
    return (void *)((uint8_t *)b + BLOCK_HDR_OFFSET);
}

static inline tlsf_block_t *ptr_to_block(void *p)
{
    return (tlsf_block_t *)((uint8_t *)p - BLOCK_HDR_OFFSET);
}

/* =========================================================================
 * FL/SL index calculation
 *
 * Given a size, compute:
 *   fl = floor(log2(size))
 *   sl = (size >> (fl - SL_INDEX_COUNT_LOG2)) - SL_INDEX_COUNT
 *
 * Uses CLZ (count leading zeros) — available on all Cortex-M via __builtin_clz.
 * ====================================================================== */

static inline void mapping(size_t size, int *fl_out, int *sl_out)
{
    int fl, sl;
    if (size < (1U << (FL_INDEX_SHIFT + SL_INDEX_COUNT_LOG2))) {
        /* Small block — all fit in FL class 0..SL_INDEX_COUNT_LOG2 */
        fl = 0;
        sl = (int)(size >> FL_INDEX_SHIFT);
    } else {
        fl = 31 - __builtin_clz((unsigned int)size);
        sl = (int)((size >> (fl - SL_INDEX_COUNT_LOG2)) - SL_INDEX_COUNT);
        fl -= FL_INDEX_SHIFT;
    }
    *fl_out = fl;
    *sl_out = sl;
}

/* Round size UP to the next TLSF bucket boundary (guarantees no under-allocation). */
static inline size_t round_up_size(size_t size)
{
    if (size < (1U << (FL_INDEX_SHIFT + SL_INDEX_COUNT_LOG2))) {
        return size;
    }
    int fl = 31 - __builtin_clz((unsigned int)size);
    size_t round = (1U << (fl - SL_INDEX_COUNT_LOG2)) - 1U;
    return (size + round) & ~round;
}

/* =========================================================================
 * Free-list insert / remove
 * ====================================================================== */

static void free_list_insert(tlsf_ctl_t *ctl, tlsf_block_t *blk)
{
    int fl, sl;
    mapping(block_size(blk), &fl, &sl);

    tlsf_block_t *head      = ctl->free[fl][sl];
    blk->next_free          = head;
    blk->prev_free          = &ctl->null_block;
    if (head != &ctl->null_block) {
        head->prev_free = blk;
    }
    ctl->free[fl][sl] = blk;

    ctl->fl_bitmap        |= (1U << fl);
    ctl->sl_bitmap[fl]    |= (1U << sl);
}

static void free_list_remove(tlsf_ctl_t *ctl, tlsf_block_t *blk)
{
    int fl, sl;
    mapping(block_size(blk), &fl, &sl);

    blk->prev_free->next_free = blk->next_free;
    blk->next_free->prev_free = blk->prev_free;

    if (ctl->free[fl][sl] == blk) {
        ctl->free[fl][sl] = blk->next_free;
        if (blk->next_free == &ctl->null_block) {
            ctl->sl_bitmap[fl] &= ~(1U << sl);
            if (ctl->sl_bitmap[fl] == 0U) {
                ctl->fl_bitmap &= ~(1U << fl);
            }
        }
    }
}

/* =========================================================================
 * Find a suitable free block (the O(1) core of TLSF)
 * ====================================================================== */

static tlsf_block_t *locate_free(tlsf_ctl_t *ctl, size_t size)
{
    if (size == 0U) {
        return NULL;
    }

    int fl, sl;
    mapping(round_up_size(size), &fl, &sl);

    /* Mask off smaller SL entries in this FL class */
    uint32_t sl_map = ctl->sl_bitmap[fl] & (~0U << sl);
    if (sl_map == 0U) {
        /* No block in this FL — try a larger FL class */
        uint32_t fl_map = ctl->fl_bitmap & (~0U << (fl + 1));
        if (fl_map == 0U) {
            return NULL; /* OOM */
        }
        fl     = __builtin_ctz(fl_map);   /* lowest set bit */
        sl_map = ctl->sl_bitmap[fl];
    }
    sl = __builtin_ctz(sl_map);           /* lowest set bit in SL */

    tlsf_block_t *blk = ctl->free[fl][sl];
    return (blk != &ctl->null_block) ? blk : NULL;
}

/* =========================================================================
 * Merge helpers
 * ====================================================================== */

static tlsf_block_t *merge_prev(tlsf_ctl_t *ctl, tlsf_block_t *blk)
{
    if (block_prev_is_free(blk)) {
        tlsf_block_t *prev = blk->prev_phys;
        free_list_remove(ctl, prev);
        prev->size_flags = (prev->size_flags & ~BLOCK_SIZE_MASK)
                         | (block_size(prev) + BLOCK_HDR_OFFSET + block_size(blk));
        block_next(prev)->prev_phys = prev;
        blk = prev;
    }
    return blk;
}

static tlsf_block_t *merge_next(tlsf_ctl_t *ctl, tlsf_block_t *blk)
{
    tlsf_block_t *nxt = block_next(blk);
    if (block_is_free(nxt)) {
        free_list_remove(ctl, nxt);
        block_set_size(blk, block_size(blk) + BLOCK_HDR_OFFSET + block_size(nxt));
        block_next(blk)->prev_phys = blk;
    }
    return blk;
}

/* =========================================================================
 * Split a block, returning the residual free block
 * ====================================================================== */

static tlsf_block_t *block_split(tlsf_block_t *blk, size_t size)
{
    tlsf_block_t *rem = (tlsf_block_t *)((uint8_t *)block_to_ptr(blk) + size);
    size_t rem_size    = block_size(blk) - size - BLOCK_HDR_OFFSET;

    rem->size_flags = rem_size;     /* free, prev not free (blk will be used) */
    rem->prev_phys  = blk;
    block_set_size(blk, size);

    /* Link rem into physical chain */
    tlsf_block_t *after = block_next(rem);
    after->prev_phys = rem;

    return rem;
}

/* =========================================================================
 * One-time pool initialisation
 * ====================================================================== */

static void tlsf_init(void)
{
    tlsf_ctl_t *ctl = (tlsf_ctl_t *)g_heap_pool;

    /* Zero the control structure */
    memset(ctl, 0, sizeof(tlsf_ctl_t));

    /* Initialise null block (free-list terminator) */
    ctl->null_block.next_free  = &ctl->null_block;
    ctl->null_block.prev_free  = &ctl->null_block;
    ctl->null_block.size_flags = 0U;

    /* Initialise all free-list heads to null_block */
    for (unsigned i = 0; i < FL_INDEX_COUNT; i++) {
        for (unsigned j = 0; j < SL_INDEX_COUNT; j++) {
            ctl->free[i][j] = &ctl->null_block;
        }
    }

    /*
     * The usable pool starts right after the control structure, aligned up.
     * We create one large free block followed by a sentinel (size=0, used).
     */
    uint8_t *pool_start = (uint8_t *)ctl + HEAP_ALIGN_UP(sizeof(tlsf_ctl_t));
    uint8_t *pool_end   = g_heap_pool + CONFIG_HEAP_SIZE;

    /* Sentinel is the very last BLOCK_HDR_OFFSET bytes (no payload) */
    tlsf_block_t *sentinel = (tlsf_block_t *)(pool_end - BLOCK_HDR_OFFSET);
    sentinel->size_flags   = TLSF_SENTINEL_SIZE | BLOCK_PREV_FREE;
    sentinel->prev_phys    = NULL;  /* filled later */

    /* Main free block */
    tlsf_block_t *main_blk = (tlsf_block_t *)pool_start;
    size_t main_size       = (size_t)(pool_end - pool_start)
                           - BLOCK_HDR_OFFSET   /* sentinel header */
                           - BLOCK_HDR_OFFSET;  /* main block header */

    main_blk->size_flags = main_size | BLOCK_IS_FREE;
    main_blk->prev_phys  = NULL;

    /* Wire sentinel's prev_phys */
    sentinel->prev_phys = main_blk;
    block_set_prev_free(sentinel);

    free_list_insert(ctl, main_blk);
    g_ctl = ctl;
}

/* =========================================================================
 * Public API
 * ====================================================================== */

void *vibe_heap_alloc(size_t size)
{
    if (size == 0U) {
        return NULL;
    }
    size = HEAP_ALIGN_UP(size);
    if (size < BLOCK_PAYLOAD_MIN) {
        size = BLOCK_PAYLOAD_MIN;
    }

    vibe_irq_key_t key = vibe_spinlock_lock_irqsave(&g_lock);

    if (g_ctl == NULL) {
        tlsf_init();
    }

    void *ret = NULL;
    tlsf_block_t *blk = locate_free(g_ctl, size);
    if (blk != NULL) {
        free_list_remove(g_ctl, blk);

        /* Split if there's enough residual space */
        size_t rem_size = block_size(blk) - size;
        if (rem_size >= (BLOCK_HDR_OFFSET + BLOCK_PAYLOAD_MIN)) {
            tlsf_block_t *rem = block_split(blk, size);
            block_set_prev_used(rem);
            free_list_insert(g_ctl, rem);
        }

        block_set_used(blk);
        block_set_prev_free(block_next(blk)); /* clear — blk is now used */
        block_set_prev_used(block_next(blk));
        ret = block_to_ptr(blk);
    }

    vibe_spinlock_unlock_irqrestore(&g_lock, key);
    return ret;
}

void vibe_heap_free(void *ptr)
{
    if (ptr == NULL) {
        return;
    }

    tlsf_block_t *blk = ptr_to_block(ptr);

    vibe_irq_key_t key = vibe_spinlock_lock_irqsave(&g_lock);

    if (block_is_free(blk)) {
        vibe_printk("heap(tlsf): double-free %p\n", ptr);
        vibe_spinlock_unlock_irqrestore(&g_lock, key);
        return;
    }

    block_set_free(blk);
    blk = merge_prev(g_ctl, blk);
    blk = merge_next(g_ctl, blk);
    block_set_prev_free(block_next(blk));
    free_list_insert(g_ctl, blk);

    vibe_spinlock_unlock_irqrestore(&g_lock, key);
}

void *vibe_heap_realloc(void *ptr, size_t size)
{
    if (ptr == NULL) {
        return vibe_heap_alloc(size);
    }
    if (size == 0U) {
        vibe_heap_free(ptr);
        return NULL;
    }

    tlsf_block_t *blk     = ptr_to_block(ptr);
    size_t        cur_size = block_size(blk);
    size_t        new_size = HEAP_ALIGN_UP(size);

    if (new_size <= cur_size) {
        return ptr;
    }

    void *np = vibe_heap_alloc(size);
    if (np == NULL) {
        return NULL;
    }
    memcpy(np, ptr, cur_size < size ? cur_size : size);
    vibe_heap_free(ptr);
    return np;
}

void *vibe_heap_calloc(size_t nmemb, size_t size)
{
    if (nmemb == 0U || size == 0U) {
        return NULL;
    }
    if (nmemb > (SIZE_MAX / size)) {
        return NULL;
    }
    size_t total = nmemb * size;
    void  *p     = vibe_heap_alloc(total);
    if (p != NULL) {
        memset(p, 0, total);
    }
    return p;
}

void vibe_heap_stats(vibe_heap_stats_t *s)
{
    if (s == NULL) {
        return;
    }

    memset(s, 0, sizeof(*s));

#if defined(CONFIG_HEAP_STATS)
    s->total = CONFIG_HEAP_SIZE;

    vibe_irq_key_t key = vibe_spinlock_lock_irqsave(&g_lock);

    if (g_ctl == NULL) {
        s->free = CONFIG_HEAP_SIZE;
        vibe_spinlock_unlock_irqrestore(&g_lock, key);
        return;
    }

    /* Walk physical blocks from the first one after the control structure */
    uint8_t *cur = (uint8_t *)g_ctl + HEAP_ALIGN_UP(sizeof(tlsf_ctl_t));
    for (;;) {
        tlsf_block_t *b = (tlsf_block_t *)cur;
        size_t bsz = block_size(b);
        if (bsz == TLSF_SENTINEL_SIZE) {
            break;
        }
        if (block_is_free(b)) {
            s->free += bsz;
            s->free_count++;
            if (bsz > s->largest_free) {
                s->largest_free = bsz;
            }
        } else {
            s->used += bsz;
            s->alloc_count++;
        }
        cur += BLOCK_HDR_OFFSET + bsz;
    }

    vibe_spinlock_unlock_irqrestore(&g_lock, key);
#endif /* CONFIG_HEAP_STATS */
}

#endif /* CONFIG_HEAP_MEM && CONFIG_HEAP_ALLOCATOR_TLSF */
