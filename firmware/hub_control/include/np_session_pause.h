/*
 * NeurOne Hub Control Program — pause bookkeeping (SWR-SS-05, OI-UI-KMP-09 R5)
 * Document: NP-SW-001 §6.3 (SWR-SS-05); NP-FW-HUB-001 §5
 * SW item:  SW-02 (i.MX RT1062 main processor) — IEC 62304 Class B
 *
 * The two pieces of state a pause needs that are not hardware, kept here as pure C so they are host-tested
 * (the session runner needs the scheduler and is not):
 *
 * 1. The SESSION CLOCK.  Paused time does not count (principal ruling 2026-10-13): the session clock stops
 *    when the session is paused and carries on from the same session time when it resumes, so a command's
 *    absolute start and stop times (start_ms, start_ms + duration_ms) stay valid across any number of
 *    pauses and the dose the protocol authored is the dose delivered.  Elapsed time, the time left, and
 *    the dose figures all follow this clock.  Time is counted in ticks of whatever the caller's tick is (the
 *    runner passes ms); unsigned subtraction makes the wrap safe.
 *
 * 2. The SLOT TRACKER.  Which command is driving each module slot right now.  A pause stops every driven
 *    slot, remembers which command each was running, and a resume re-dispatches exactly those commands
 *    through the ordinary dispatch path.  A resumed modality therefore starts again from its own start-up
 *    (a ramp begins again) and goes through every check a fresh command goes through, including the safety
 *    MCU's: nothing is restored, so nothing can be restored past an interlock.
 *
 * What a pause does NOT do: it does not change what the safety MCU holds.  The hub withdraws its enable
 * requests, the MCU withholds the lines, and a resume only asks again; whether a line is granted again is the
 * MCU's decision alone (a cervical line held after a cardiac cutoff stays held, SWR-SS-05).
 */

#ifndef NP_SESSION_PAUSE_H
#define NP_SESSION_PAUSE_H

#include <stdbool.h>
#include <stdint.h>

#include "np_hub_config.h"   /* NP_HUB_SLOT_MAX */

/* ── The session clock ───────────────────────────────────────────────────────── */

typedef struct {
    uint32_t start;        /* tick at session start                                  */
    uint32_t paused_total; /* ticks spent paused, closed pauses only                 */
    uint32_t pause_start;  /* tick the current pause began; meaningful while paused  */
    bool     paused;
} np_session_clock_t;

void     np_session_clock_start(np_session_clock_t *c, uint32_t now);
/* Returns false (and changes nothing) if already paused. */
bool     np_session_clock_pause(np_session_clock_t *c, uint32_t now);
/* Returns false (and changes nothing) if not paused. */
bool     np_session_clock_resume(np_session_clock_t *c, uint32_t now);
/* Session ticks so far: real time since start, less every pause; frozen at the pause instant while paused. */
uint32_t np_session_clock_elapsed(const np_session_clock_t *c, uint32_t now);
/* Close an open pause without resuming (the session is ending while paused). */
void     np_session_clock_close(np_session_clock_t *c, uint32_t now);

/* ── The slot tracker ────────────────────────────────────────────────────────── */

#define NP_SLOT_NONE   (-1)

typedef struct {
    int16_t current[NP_HUB_SLOT_MAX];   /* command index driving the slot now, or NP_SLOT_NONE   */
    int16_t held[NP_HUB_SLOT_MAX];      /* what each slot was running when it was paused         */
} np_slot_track_t;

void    np_slot_track_reset(np_slot_track_t *t);
/* A drive command was accepted on `slot`. */
void    np_slot_track_started(np_slot_track_t *t, uint8_t slot, uint16_t cmd_index);
/* A stop command was dispatched, or the command's own duration ran out. */
void    np_slot_track_stopped(np_slot_track_t *t, uint8_t slot);
/*
 * Pause: move every driven slot's command to `held` and clear `current`.  Fills `slots_out` with the slots to
 * stop (ascending) and returns how many.  Nothing is held twice: pausing again with nothing running holds nothing.
 */
uint8_t np_slot_track_pause(np_slot_track_t *t, uint8_t slots_out[NP_HUB_SLOT_MAX]);
/*
 * Resume: the commands to re-dispatch, in ascending slot order.  Returns how many and clears `held`.  The caller
 * re-dispatches each through the normal dispatch path, which records it as driving again.
 */
uint8_t np_slot_track_resume(np_slot_track_t *t, uint16_t cmds_out[NP_HUB_SLOT_MAX]);

#endif /* NP_SESSION_PAUSE_H */
