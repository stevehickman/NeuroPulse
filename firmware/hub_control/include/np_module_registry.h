/*
 * NeurOne Hub Control Program — Module Registry
 * Document: NP-FW-HUB-001 Rev 1 §3
 *
 * The registry is a flat table of np_mod_entry_t records, one per physical slot.
 * On powerup, np_mod_reg_scan() probes each interface, calls each module type's
 * detect function, and populates the entry with its four control function pointers.
 * The session runner calls np_mod_reg_get() to resolve slot addresses during
 * protocol execution.
 */

#ifndef NP_MODULE_REGISTRY_H
#define NP_MODULE_REGISTRY_H

#include "np_hub_types.h"

/* ── Registry entry ──────────────────────────────────────────────────────────── */

typedef struct {
    np_hub_mod_type_t   type;
    uint8_t             slot;
    bool                present;
    bool                initialized;
    np_mod_init_fn      init;
    np_mod_control_fn   control;
    np_mod_telemetry_fn telemetry;
    np_mod_shutdown_fn  shutdown;
} np_mod_entry_t;

/* ── Module detect function type ─────────────────────────────────────────────── */

/*
 * Called once per slot during np_mod_reg_scan().
 * Returns NP_HUB_OK and sets *type_out if a module is detected, NP_HUB_ERR_NOT_PRESENT
 * if the slot is empty.  Implementation is module-specific (ADC / impedance / Hall).
 */
typedef np_hub_status_t (*np_mod_detect_fn)(uint8_t slot,
                                             np_hub_mod_type_t *type_out);

/* ── API ─────────────────────────────────────────────────────────────────────── */

/*
 * np_mod_reg_init — zero the registry; call before np_mod_reg_scan().
 */
void np_mod_reg_init(void);

/*
 * np_mod_reg_scan — probe all slots and populate present entries.
 * Calls each slot's detect function, then calls init() on detected modules.
 *
 * Writes no SHDR record itself (OI-FWHUB-05, NP-FW-HUB-001 §3.2).  It used to
 * write one "zone auth" record per retired zone slot 0–4 through a callback;
 * those were classified from the ZONE_ID resistor ladder that Rev 3 hardware
 * does not carry, so every one was a fabrication.  Accessories that do
 * authenticate (intranasal, cervical VNS) log their own result from init().
 */
np_hub_status_t np_mod_reg_scan(void);

/*
 * np_mod_reg_get — return the registry entry for the given slot.
 * Returns NULL if slot is out of range or not present.
 */
np_mod_entry_t *np_mod_reg_get(uint8_t slot);

/*
 * np_mod_reg_find — return the first present entry of the given type.
 * Returns NULL if no such module is present.
 */
np_mod_entry_t *np_mod_reg_find(np_hub_mod_type_t type);

/*
 * np_mod_reg_count — number of present (detected) modules.
 */
uint8_t np_mod_reg_count(void);

/*
 * np_mod_reg_shutdown_all — call shutdown() on every present, initialized module.
 * Used on session abort and safe power-down.
 */
void np_mod_reg_shutdown_all(void);

/*
 * np_mod_reg_rescan_slot — re-probe one accessory slot (called by
 * task_module_detect while no session is in progress; OI-FWHUB-16).
 *
 * Accepts NP_HUB_SLOT_FIRST_VALID .. NP_HUB_SLOT_MAX-1.  The retired zone slots
 * 0-4 and anything out of range return NP_HUB_ERR_INVALID_ARG without probing.
 *
 * Acts only on a CHANGE in presence or type since the last probe:
 *   - newly present      → register, call init()         (one SHDR auth record
 *                                                          for intranasal / CVNS)
 *   - removed            → call shutdown() if it was initialised, deregister
 *   - type changed       → shutdown() the old occupant, init() the new one
 *   - unchanged          → nothing; detect() is the only driver call
 * A module whose init() failed is not retried until it is removed and re-seated.
 *
 * Returns NP_HUB_OK (present and initialised), NP_HUB_ERR_NOT_PRESENT (empty),
 * NP_HUB_ERR_MOD_INIT (present, init failed) or NP_HUB_ERR_INVALID_ARG.
 *
 * Fixed-hardware slots (EEG, audio, BES/tACS, tDCS) are accepted and are no-ops
 * while their detect() keeps reporting them present.
 *
 * The registry writes no SHDR record of its own: the intranasal and cervical
 * VNS drivers write theirs from init(), which runs once per insertion.
 */
np_hub_status_t np_mod_reg_rescan_slot(uint8_t slot);

#endif /* NP_MODULE_REGISTRY_H */
