/**
 * @file lib/job_scheduler/job_scheduler.c
 * @brief Cooperative job scheduler implementation.
 */

#include "job_scheduler.h"
#include "vibe/sys/util.h"   /* STATIC_ASSERT */
#include <string.h>

#if defined(CONFIG_JOB_SCHEDULER)

/* Each slot is tracked by a bit in a uint64_t during the priority scan. */
STATIC_ASSERT(CONFIG_JOB_SCHEDULER_MAX_JOBS <= 64,
              "CONFIG_JOB_SCHEDULER_MAX_JOBS must not exceed 64");

/* -------------------------------------------------------------------------
 * Internal helpers
 * ---------------------------------------------------------------------- */

static void _exec(vibe_job_sched_t *sched, vibe_job_t *j)
{
    async_status_t st = j->fn(j->ctx, &j->co);

#if defined(CONFIG_JOB_SCHEDULER_STATS)
    j->run_count++;
#endif

    switch (st) {
    case ASYNC_DONE:
        j->state = JOB_IDLE;
#if defined(CONFIG_JOB_SCHEDULER_STATS)
        sched->stats.completed++;
        sched->stats.active--;
#endif
        break;

    case ASYNC_BLOCKED:
        j->state = JOB_BLOCKED;
        break;

    case ASYNC_RUNNING:
    default:
        j->state = JOB_READY;
        break;
    }

    /* suppress unused-parameter warning when stats are disabled */
    (void)sched;
}

/* -------------------------------------------------------------------------
 * Public API
 * ---------------------------------------------------------------------- */

void vibe_sched_init(vibe_job_sched_t *sched)
{
    memset(sched, 0, sizeof(*sched));
    sched->capacity = CONFIG_JOB_SCHEDULER_MAX_JOBS;
}

vibe_job_t *vibe_sched_submit(vibe_job_sched_t *sched,
                               async_fn_t fn, void *ctx, uint8_t prio)
{
    int n = (int)sched->capacity;
    for (int i = 0; i < n; i++) {
        if (sched->jobs[i].state == JOB_IDLE) {
            vibe_job_t *j = &sched->jobs[i];
            j->fn    = fn;
            j->ctx   = ctx;
            j->prio  = prio;
            j->state = JOB_READY;
            async_init(&j->co);
#if defined(CONFIG_JOB_SCHEDULER_STATS)
            j->run_count = 0;
            sched->stats.submitted++;
            sched->stats.active++;
#endif
            return j;
        }
    }
    return NULL; /* pool full */
}

void vibe_sched_cancel(vibe_job_sched_t *sched, vibe_job_t *job)
{
    if (job == NULL || job->state == JOB_IDLE) {
        return;
    }
#if defined(CONFIG_JOB_SCHEDULER_STATS)
    sched->stats.active--;
#else
    (void)sched;
#endif
    job->state = JOB_IDLE;
}

int vibe_sched_run(vibe_job_sched_t *sched)
{
    int      ran  = 0;
    int      n    = (int)sched->capacity;
    uint64_t done = 0ULL; /* bitmask: 1 = slot already ran this tick */

#if defined(CONFIG_JOB_SCHEDULER_STATS)
    sched->stats.ticks++;
#endif

    /*
     * Phase 1 — READY jobs, highest priority first.
     *
     * Selection-scan: O(N²) but N ≤ 64 and typical N ≤ 8, so negligible.
     * The done-bitmask prevents any slot from running twice in Phase 1.
     */
    for (;;) {
        int     best = -1;
        uint8_t maxp = 0U;

        for (int i = 0; i < n; i++) {
            if (done & (1ULL << i)) { continue; }
            if (sched->jobs[i].state != JOB_READY) { continue; }
            if (best < 0 || sched->jobs[i].prio > maxp) {
                best = i;
                maxp = sched->jobs[i].prio;
            }
        }
        if (best < 0) { break; } /* no more ready jobs */

        done |= 1ULL << best;
        _exec(sched, &sched->jobs[best]);
        ran++;
    }

    /*
     * Phase 2 — BLOCKED jobs.
     *
     * Re-invoke each blocked job so it can re-evaluate its ASYNC_AWAIT
     * condition.  Jobs that transition to READY or DONE are updated by _exec.
     * No priority ordering needed here: all blocked jobs are checked once.
     */
    for (int i = 0; i < n; i++) {
        if (sched->jobs[i].state != JOB_BLOCKED) { continue; }
        _exec(sched, &sched->jobs[i]);
        ran++;
    }

    return ran;
}

bool vibe_sched_is_idle(const vibe_job_sched_t *sched)
{
    int n = (int)sched->capacity;
    for (int i = 0; i < n; i++) {
        if (sched->jobs[i].state == JOB_READY) {
            return false;
        }
    }
    return true;
}

int vibe_sched_active_count(const vibe_job_sched_t *sched)
{
    int count = 0;
    int n     = (int)sched->capacity;
    for (int i = 0; i < n; i++) {
        if (sched->jobs[i].state != JOB_IDLE) {
            count++;
        }
    }
    return count;
}

#if defined(CONFIG_JOB_SCHEDULER_STATS)
void vibe_sched_get_stats(const vibe_job_sched_t *sched,
                           vibe_sched_stats_t *s)
{
    if (s == NULL) { return; }
    *s = sched->stats;
}
#endif

#endif /* CONFIG_JOB_SCHEDULER */
