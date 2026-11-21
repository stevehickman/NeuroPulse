/*
 * NeurOne Hub Control Program — the status indicators and stealth mode (REQ-FWHUB-84, REQ-FWHUB-85)
 * Document: NP-FW-HUB-001 §5.5a; CLAUDE.md §4.7; docs/reference/hardware-detail.md §4.7
 * SW item:  SW-02 (i.MX RT1062 main processor) — IEC 62304 Class B
 * Open item: OI-PRIV-37 (NP-PRIV-ANALYSIS-004), GitHub #965.  The LED HAL itself is OI-HUB-MAIN-02.
 *
 * The one place that decides what the temple indicators show.  It keeps two facts:
 *
 *   - what the hub is doing: IDLE, in a SESSION, or showing a FAULT (latched until the next session starts);
 *   - whether stealth mode is on.
 *
 * and shows: FAULT whenever a fault is latched, whatever stealth says (CLAUDE.md §4.7: "safety faults always fire";
 * NP-IFU-001 IFU-10: "Stealth mode never hides a fault"); otherwise NP_LED_OFF when stealth is on; otherwise IDLE or
 * SESSION.  Every fault fires, not a chosen subset: NP-SW-FAULTMSG-001 §1 has one fault state for every fault, so no
 * fault is classed as hideable here.
 *
 * A session that ends because the person stopped it, or because a pause outlasted the owner's limit, is not a fault
 * (np_runner_end_status(), REQ-FWHUB-84) and goes back to IDLE.
 *
 * STEALTH IS HELD IN RAM ONLY, and every boot starts with it OFF (indicators visible).  Nothing is written to Config,
 * SHDR or UHDR.  A stealth preference is a fact about the person (CLAUDE.md §5.1: when in doubt, UHDR), and persisting
 * it is not built: where it would live is recorded in OI-PRIV-37.  Failing to visible is the conservative direction:
 * an indicator shown that the wearer wanted hidden is a nuisance; one hidden that a carer relied on is not detectable.
 *
 * Wire (STEALTH, READ/WRITE 1 B, encrypted link only, CLAUDE.md §23): bit 0 stealth on.  Any other bit set is refused,
 * so a future meaning is never half-understood.  The platform seam refuses the write on an unencrypted link
 * (NP_GATT_PROP_ENC, REQ-FWHUB-81).  The hub cannot tell who holds the bonded link; which person the app lets set it is
 * the app's (OI-PRIV-37).
 *
 * Indicators this module does not yet drive, and which stealth must not suppress when they are built: the Mode F
 * triple pulse (NP-FW-EMMC-002 §F), the bench-mode state (NP-FW-BENCH-001 D-8) and the gated-protocol annunciator
 * (NP-PWRSRC-001 §6.12).  The amber pulse that mirrors session frequency (hardware-detail.md §4.7) is not built either;
 * when it is, it is a SESSION indicator and stealth suppresses it.
 *
 * Output: np_status_led_bind() names the function that drives the LEDs (np_hal_status_led_set on the device).  With
 * none bound the module still keeps its state, so a host test that links the GATT table needs no HAL.  The output is
 * called inside the transport critical section so two callers cannot leave the LEDs showing the older state.
 */

#ifndef NP_STATUS_LED_H
#define NP_STATUS_LED_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "np_hub_types.h"
#include "np_sw02_platform_hal.h"   /* np_led_state_t */

#define NP_STEALTH_FLAG_ON   0x01U   /* bit 0 of the STEALTH byte */

typedef void (*np_status_led_out_fn)(np_led_state_t state);

/* Bind the output (NULL unbinds).  Call before np_status_led_init(). */
void np_status_led_bind(np_status_led_out_fn out);

/* Boot: stealth off, nothing latched, IDLE shown. */
void np_status_led_init(void);

/* A session is starting: clears a latched fault and shows SESSION (or OFF in stealth). */
void np_status_led_session_started(void);

/* A session ended with this np_runner_run() status: NP_HUB_OK and NP_HUB_ERR_STOPPED go back to IDLE; any other
 * status latches FAULT, which shows whatever stealth says. */
void np_status_led_session_ended(np_hub_status_t rc);

/* A fault seen outside the runner (the heartbeat task's non-recoverable safety fault): latches FAULT at once. */
void np_status_led_fault(void);

/* Stealth on or off.  Takes effect at once; never hides a latched fault and never clears one. */
void np_status_led_set_stealth(bool on);
bool np_status_led_stealth(void);

/* What the indicators are showing now (the last value sent to the output). */
np_led_state_t np_status_led_shown(void);

/* The GATT read and write of STEALTH. */
np_hub_status_t np_status_led_read(uint8_t *buf, size_t cap, size_t *len_out);
np_hub_status_t np_status_led_write(const uint8_t *data, size_t len);

#endif /* NP_STATUS_LED_H */
