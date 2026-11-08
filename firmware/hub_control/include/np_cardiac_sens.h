/*
 * NeurOne Hub Control Program — the cervical VNS cardiac cutoff's sensitivity (REQ-FWHUB-74, REQ-CVNS-14)
 * Document: NP-FW-HUB-001 §8.4; NP-FW-CVNS-001 §14.8
 * SW item:  SW-02 (i.MX RT1062 main processor) — IEC 62304 Class B (the cutoff itself is the safety MCU's, Class C)
 *
 * How sensitive the safety MCU makes the cardiac cutoff: a level from 0, the cutoff as validated (REQ-CVNS-09, a change of more
 * than 15 BPM), up to NP_SAFETY_CARDIAC_SENS_MAX. THERE IS NO LEVEL BELOW 0: the hardware owner can make the cutoff fire on a
 * smaller change and never on a larger one, because a less sensitive cutoff would weaken a hazard control that is presented to
 * FDA as a safety enhancement. The cost of a higher level is nuisance cutoffs (OI-CVNS-13), which is the owner's to weigh.
 *
 * The hub only holds the setting and passes it on: it rides in the heart-rate report the hub already sends the safety MCU every
 * second while cervical VNS is in use (np_safety_spi_send_hr_report). The MCU raises its level at once and lowers it only when a
 * new cervical VNS request starts, so a session already running is never made less protected; the hub does not decide that.
 *
 * Wire (CARDIAC_SENSITIVITY, encrypted link only): READ two bytes, the level then the highest level; WRITE one byte, the level.
 * A level above the highest is refused. Safe on every failure: an absent, unreadable or unknown record leaves level 0, the
 * validated cutoff, and an unreadable record is never overwritten.
 *
 * WHO MAY WRITE: as for the pause limit, the hub accepts any bonded, encrypted link; restricting it to the warranty owner is the
 * app's. This setting only ever makes the cutoff more protective, so the stakes are lower than the detector's override.
 *
 * Persistence is from np_cardiac_sens_poll() in the heartbeat task, never from the BLE write callback.
 */
#ifndef NP_CARDIAC_SENS_H
#define NP_CARDIAC_SENS_H

#include <stddef.h>
#include <stdint.h>

#include "np_hub_types.h"

void    np_cardiac_sens_init(void);
void    np_cardiac_sens_poll(void);

/* The level to send the safety MCU: 0 to NP_SAFETY_CARDIAC_SENS_MAX. */
uint8_t np_cardiac_sens_level(void);

np_hub_status_t np_cardiac_sens_read(uint8_t *buf, size_t cap, size_t *len_out);
np_hub_status_t np_cardiac_sens_write(const uint8_t *data, size_t len);

#endif /* NP_CARDIAC_SENS_H */
