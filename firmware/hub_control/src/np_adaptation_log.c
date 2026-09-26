/*
 * NeurOne Hub Control Program — Closed-Loop Adaptation Event Ring Buffer
 * Document: NP-FW-HUB-001 Rev 1 §6.4 (STEP-33 of NP-PRIV-REM-001)
 *
 * Implements np_adapt_log_event(), np_adapt_log_flush(), np_adapt_log_reset().
 * Events are queued here; np_log_adapt_event() (from np_session_log) writes
 * each one to the UHDR partition when flush is called.
 *
 * Ring buffer holds NP_ADAPT_LOG_MAX events (power-of-2 for mask arithmetic).
 * If the buffer fills during a session, the fault counter in SHDR increments
 * and the oldest unflushed event is overwritten (newest wins).
 *
 * Shared by more than one task: the ring is drained both by np_log_session_end()
 * in the runner task and by np_log_flush() in task_telemetry.  Every function
 * here therefore runs under the session logger's recursive lock
 * (np_log_lock(), OI-FWHUB-19, NP-FW-HUB-001 §6.7).  (Nothing queues an event
 * yet.)
 */

#include "np_session_log.h"   /* pulls in np_adaptation_log.h transitively */
#include <string.h>

/* ── Ring buffer ─────────────────────────────────────────────────────────────── */

#define NP_ADAPT_LOG_MAX  64U   /* must be a power of 2 */
#define NP_ADAPT_LOG_MASK (NP_ADAPT_LOG_MAX - 1U)

static np_adaptation_event_t s_ring[NP_ADAPT_LOG_MAX];
static uint32_t s_write = 0U;  /* next write index */
static uint32_t s_read  = 0U;  /* next read (flush) index */
static uint32_t s_overrun_count = 0U;   /* SHDR-bound; no user biology */

/* ── API implementation ───────────────────────────────────────────────────────── */

bool np_adapt_log_event(const np_adaptation_event_t *event)
{
    if (event == NULL) { return false; }

    np_log_lock();
    uint32_t next = (s_write + 1U) & NP_ADAPT_LOG_MASK;
    if (next == s_read) {
        /* Buffer full — advance read pointer (lose oldest), count overrun. */
        s_read = (s_read + 1U) & NP_ADAPT_LOG_MASK;
        s_overrun_count++;
    }

    s_ring[s_write] = *event;
    s_write = next;
    bool ok = (s_overrun_count == 0U);
    np_log_unlock();
    return ok;
}

void np_adapt_log_flush(void)
{
    np_log_lock();
    while (s_read != s_write) {
        np_log_adapt_event(&s_ring[s_read]);
        s_read = (s_read + 1U) & NP_ADAPT_LOG_MASK;
    }
    np_log_unlock();
}

void np_adapt_log_reset(void)
{
    np_log_lock();
    s_write = 0U;
    s_read  = 0U;
    s_overrun_count = 0U;
    np_log_unlock();
}
