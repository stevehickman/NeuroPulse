/*
 * NeurOne Hub Control Program — the detector's clinician settings (REQ-FWHUB-71)
 * Document: NP-FW-HUB-001 §8.6; NP-FW-NVRAM-001 (Config)
 * SW item:  SW-02 (i.MX RT1062 main processor) — IEC 62304 Class B
 *
 * Two switches, the hardware owner's (as the pause limit is), kept in one replicated Config record:
 *
 *   bit 0  STOP ON OCCIPITAL-ONLY  halt on a discharge on an occipital channel alone. ON by default.
 *   bit 1  MANUAL OVERRIDE         lift the refusal of REQ-FWHUB-65 / -66 where the detector has bench evidence only
 *                                  (np_ppx_policy.h). OFF by default. With no evidence it lifts nothing.
 *
 * Wire (PPX_CONFIG, encrypted link only). READ: two bytes, the flags then the evidence level in force (0 none, 1 bench, 2
 * clinical), so the app can say what the override can and cannot do today. WRITE: one byte of flags. A write with any other
 * bit set is refused, so a future flag is never half-understood.
 *
 * SAFE ON EVERY FAILURE. An absent record, an unreadable one and one from a firmware this one does not know all leave the
 * defaults above, which are the protective ones, and an unreadable record is never overwritten with them.
 *
 * NO PRE-SET OVERRIDE. A write that sets the override while the evidence level is NONE is refused, and a stored override is
 * not applied while it is NONE. The override is a clinician's decision about a detector that has earned it; it must not be set
 * ahead of the evidence and then take effect, unasked, when a later firmware raises the level.
 *
 * ASYMMETRY. Turning "stop on occipital-only" ON takes effect at once, in a session already running; turning it OFF takes
 * effect at the next session. A change that protects more is not made to wait, and one that protects less never reaches
 * back into a drive being watched.
 *
 * WHO MAY WRITE. As for the pause limit: the hub cannot tell the owner from anyone else holding the bonded link, so it
 * accepts the write on an encrypted link and nothing more; restricting it to the warranty owner is the app's. The override
 * lifts a safety refusal, so whether the hub should also demand a signed owner credential is an open question (OI-FWHUB-32).
 * It lifts nothing while the evidence level is NONE, which it is.
 *
 * Persistence is from np_ppx_config_poll() in the heartbeat task, never from the BLE write callback.
 */
#ifndef NP_PPX_CONFIG_H
#define NP_PPX_CONFIG_H

#include <stddef.h>
#include <stdint.h>

#include "np_hub_types.h"

#define NP_PPX_CFG_STOP_OCCIPITAL   0x01U
#define NP_PPX_CFG_OVERRIDE         0x02U
#define NP_PPX_CFG_KNOWN_BITS       (NP_PPX_CFG_STOP_OCCIPITAL | NP_PPX_CFG_OVERRIDE)
#define NP_PPX_CFG_DEFAULT          NP_PPX_CFG_STOP_OCCIPITAL

/* Load the stored flags and hand them to the policy; call once at bring-up, after the Config store is bound. */
void np_ppx_config_init(void);

/* Persist changed flags; retry a load that failed. Call from the heartbeat task. */
void np_ppx_config_poll(void);

uint8_t np_ppx_config_flags(void);

np_hub_status_t np_ppx_config_read(uint8_t *buf, size_t cap, size_t *len_out);
np_hub_status_t np_ppx_config_write(const uint8_t *data, size_t len);

#endif /* NP_PPX_CONFIG_H */
