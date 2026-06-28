/*
 * NeuroPulse Hub Control Program — Module Registry
 * Document: NP-FW-HUB-001 Rev A §3
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
 * Writes an SHDR auth log entry per zone slot (pass/fail) via shdr_log_cb.
 *
 * shdr_log_cb may be NULL (auth events skipped).
 */
typedef void (*np_mod_reg_shdr_cb_t)(uint8_t slot, np_hub_mod_type_t type, bool auth_pass);

np_hub_status_t np_mod_reg_scan(np_mod_reg_shdr_cb_t shdr_log_cb);

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
 * np_mod_reg_rescan_zone — re-probe a single zone slot (called by zone_announce
 * insert callback).  Updates the registry without full re-scan.
 */
np_hub_status_t np_mod_reg_rescan_zone(uint8_t zone_slot,
                                        np_mod_reg_shdr_cb_t shdr_log_cb);

#endif /* NP_MODULE_REGISTRY_H */
