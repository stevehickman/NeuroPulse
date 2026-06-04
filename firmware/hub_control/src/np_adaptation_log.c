/*
 * NeuroPulse Hub Control Program — Closed-Loop Adaptation Event Ring Buffer
 * Document: NP-FW-HUB-001 Rev B (STEP-33 of NP-PRIV-REM-001)
 *
 * Implements np_adapt_log_event(), np_adapt_log_flush(), np_adapt_log_reset().
 * Events are queued here; np_log_adapt_event() (from np_session_log) writes
 * each one to the UHDR partition when flush is called.
 *
 * Ring buffer holds NP_ADAPT_LOG_MAX events (power-of-2 for mask arithmetic).
 * If the buffer fills during a session, the fault counter in SHDR increments
 * and the oldest unflushed event is overwritten (newest wins).
 *
 * Called from the session runner task only.  No mutex required.
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

    uint32_t next = (s_write + 1U) & NP_ADAPT_LOG_MASK;
    if (next == s_read) {
        /* Buffer full — advance read pointer (lose oldest), count overrun. */
        s_read = (s_read + 1U) & NP_ADAPT_LOG_MASK;
        s_overrun_count++;
    }

    s_ring[s_write] = *event;
    s_write = next;
    return (s_overrun_count == 0U);
}

void np_adapt_log_flush(void)
{
    while (s_read != s_write) {
        np_log_adapt_event(&s_ring[s_read]);
        s_read = (s_read + 1U) & NP_ADAPT_LOG_MASK;
    }
}

void np_adapt_log_reset(void)
{
    s_write = 0U;
    s_read  = 0U;
    s_overrun_count = 0U;
}
