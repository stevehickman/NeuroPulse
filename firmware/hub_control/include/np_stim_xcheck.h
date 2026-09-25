/*
 * NeurOne Hub Control Program — Commanded-versus-Delivered Stimulation Cross-Check
 * Document: NP-FW-HUB-001 Rev 9 §8.3.1; NP-FMEA-001 §3.3 FMEA-M03-02 (OI-FMEA-09)
 * SW item:  SW-02 (i.MX RT1062 main processor) — IEC 62304 Class B
 *
 * WHY THIS EXISTS.  The safety MCU's charge monitor (np_charge_monitor.c) is a
 * COMMANDED-dose control: it integrates the current the hub publishes, never a
 * measurement, because a measured value cannot be signed in advance
 * (NP-SW-001 SW01-M03).  So it cannot see delivered current exceeding
 * commanded — a driver stuck on, a current source out of calibration.  That is
 * FMEA-M03-02, and this module is the control its residual score depends on:
 * the hub compares the delivered current its own stim driver reads back
 * (OI-STIM-06) against the commanded current, and a sustained excess raises an
 * SHDR divergence FLAG.
 *
 * WHAT IS COMPARED.  The commanded side is fed from the same call sites that
 * publish np_safety_spi_set_channel_current(), with the same capped number, so
 * this checks against exactly what the MCU integrates.  Only the EXCESS
 * direction is checked: under-delivery reduces dose and is not the FMEA-M03-02
 * hazard.  A tDCS ramp-down keeps delivering above the new commanded level for
 * as long as the ramp runs, so a decrease holds the previous level as the bound
 * for the ramp's duration; this is why the caller passes ramp_down_ms.
 *
 * COVERAGE — ONLY CHANNELS WITH A DELIVERED-CURRENT READ.  BES/tACS and T1
 * tDCS (np_mod_stim.c) have one.  Cervical VNS reports its commanded current in
 * the telemetry `current_ua` field, and auricular VNS reports none, so neither
 * can be cross-checked.  T2 clinical stimulation waits on NP-HW-TACSDRV-001
 * A16.2's sense path.  Recorded in OI-FMEA-09.
 *
 * PRIVACY.  The flag is device condition: which channel, not how much.  The
 * magnitude of a divergence is never uploaded, only that it happened
 * (NP-FW-NVRAM-001 D-14), and np_log_shdr_fault() writes no timestamp for any
 * caller.  Delivered current itself is UHDR and stays in NP_LOG_TAG_UHDR_STIM.
 *
 * Pure C: no FreeRTOS, no HAL.  Host-tested by np_stim_xcheck_tests.
 */

#ifndef NP_STIM_XCHECK_H
#define NP_STIM_XCHECK_H

#include <stdbool.h>
#include <stdint.h>

#include "np_hub_types.h"

/* Session start: clear every channel's debounce run and latch, so each session
 * can raise its own flag.  The commanded envelope is KEPT, because it is the
 * driver's state and not the session's: a tDCS ramp-down outlives the
 * runner's shutdown wait.  A hold still running is re-anchored to the new
 * session's clock.  The runner calls this before any command is dispatched. */
void np_stim_xcheck_reset(void);

/* Forget everything, envelope included: boot state.  Tests, and any caller
 * that knows every driver output is off. */
void np_stim_xcheck_clear(void);

/*
 * Record the current just commanded on a safety-MCU channel (NP_SAFETY_CH_*).
 * Call beside np_safety_spi_set_channel_current() with the same value.
 *
 * ramp_down_ms: how long the driver takes to reach commanded_ua when it is
 * LOWER than the level before it.  0 for a channel that steps (BES/tACS).
 * The previous level remains the bound for that long, counted from the next
 * observation.  The first telemetry snapshot after the command therefore
 * starts the clock, which can only lengthen the hold by up to one telemetry
 * interval; it never shortens it.
 */
void np_stim_xcheck_commanded(uint8_t safety_ch, uint16_t commanded_ua,
                              uint32_t ramp_down_ms);

/*
 * Compare one telemetry snapshot against the commanded bound.  Records of any
 * modality without a delivered-current read are ignored.
 *
 * Returns true exactly once per channel per session: on the observation that
 * completes NP_STIM_XCHECK_CONSECUTIVE consecutive excesses.  The caller writes
 * the SHDR flag.  A non-finite delivered reading counts as an excess, since it
 * cannot show that delivery is within bounds.
 */
bool np_stim_xcheck_observe(const np_telem_record_t *rec, uint32_t now_ms);

/* Whether the channel has latched a divergence this session (tests, and any
 * later consumer that must not re-raise the flag). */
bool np_stim_xcheck_latched(uint8_t safety_ch);

#endif /* NP_STIM_XCHECK_H */
