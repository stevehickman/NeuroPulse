/*
 * NeuroPulse Hub Control Program — Module Registry Implementation
 * Document: NP-FW-HUB-001 Rev A §3
 */

#include "np_module_registry.h"
#include "np_session_log.h"
#include <string.h>

/* ── Forward declarations of per-module detect+init functions ─────────────────── */

/* Each module driver file provides these; declared extern here to keep the
 * registry the single place that wires detect → register. */

/* PBM zone modules (base and smart) */
extern np_hub_status_t np_mod_pbm_detect(uint8_t slot, np_hub_mod_type_t *type_out);
extern np_hub_status_t np_mod_pbm_init(uint8_t slot);
extern np_hub_status_t np_mod_pbm_control(uint8_t slot, const void *params, uint16_t len);
extern np_hub_status_t np_mod_pbm_telemetry(uint8_t slot, np_telem_record_t *out);
extern np_hub_status_t np_mod_pbm_shutdown(uint8_t slot);

/* Intranasal */
extern np_hub_status_t np_mod_intranasal_detect(uint8_t slot, np_hub_mod_type_t *type_out);
extern np_hub_status_t np_mod_intranasal_init(uint8_t slot);
extern np_hub_status_t np_mod_intranasal_control(uint8_t slot, const void *params, uint16_t len);
extern np_hub_status_t np_mod_intranasal_telemetry(uint8_t slot, np_telem_record_t *out);
extern np_hub_status_t np_mod_intranasal_shutdown(uint8_t slot);

/* EEG */
extern np_hub_status_t np_mod_eeg_detect(uint8_t slot, np_hub_mod_type_t *type_out);
extern np_hub_status_t np_mod_eeg_init(uint8_t slot);
extern np_hub_status_t np_mod_eeg_control(uint8_t slot, const void *params, uint16_t len);
extern np_hub_status_t np_mod_eeg_telemetry(uint8_t slot, np_telem_record_t *out);
extern np_hub_status_t np_mod_eeg_shutdown(uint8_t slot);

/* BES/tACS and tDCS share one driver (np_mod_stim.c) */
extern np_hub_status_t np_mod_stim_detect(uint8_t slot, np_hub_mod_type_t *type_out);
extern np_hub_status_t np_mod_stim_init(uint8_t slot);
extern np_hub_status_t np_mod_stim_control(uint8_t slot, const void *params, uint16_t len);
extern np_hub_status_t np_mod_stim_telemetry(uint8_t slot, np_telem_record_t *out);
extern np_hub_status_t np_mod_stim_shutdown(uint8_t slot);

/* VNS + HRV */
extern np_hub_status_t np_mod_vns_detect(uint8_t slot, np_hub_mod_type_t *type_out);
extern np_hub_status_t np_mod_vns_init(uint8_t slot);
extern np_hub_status_t np_mod_vns_control(uint8_t slot, const void *params, uint16_t len);
extern np_hub_status_t np_mod_vns_telemetry(uint8_t slot, np_telem_record_t *out);
extern np_hub_status_t np_mod_vns_shutdown(uint8_t slot);

/* Audio */
extern np_hub_status_t np_mod_audio_detect(uint8_t slot, np_hub_mod_type_t *type_out);
extern np_hub_status_t np_mod_audio_init(uint8_t slot);
extern np_hub_status_t np_mod_audio_control(uint8_t slot, const void *params, uint16_t len);
extern np_hub_status_t np_mod_audio_telemetry(uint8_t slot, np_telem_record_t *out);
extern np_hub_status_t np_mod_audio_shutdown(uint8_t slot);

/* Visual */
extern np_hub_status_t np_mod_visual_detect(uint8_t slot, np_hub_mod_type_t *type_out);
extern np_hub_status_t np_mod_visual_init(uint8_t slot);
extern np_hub_status_t np_mod_visual_control(uint8_t slot, const void *params, uint16_t len);
extern np_hub_status_t np_mod_visual_telemetry(uint8_t slot, np_telem_record_t *out);
extern np_hub_status_t np_mod_visual_shutdown(uint8_t slot);

/* Cervical VNS */
extern np_hub_status_t np_mod_cvns_detect(uint8_t slot, np_hub_mod_type_t *type_out);
extern np_hub_status_t np_mod_cvns_init(uint8_t slot);
extern np_hub_status_t np_mod_cvns_control(uint8_t slot, const void *params, uint16_t len);
extern np_hub_status_t np_mod_cvns_telemetry(uint8_t slot, np_telem_record_t *out);
extern np_hub_status_t np_mod_cvns_shutdown(uint8_t slot);

/* T2: 21-ch qEEG wet-gel cap */
extern np_hub_status_t np_mod_qeeg_detect(uint8_t slot, np_hub_mod_type_t *type_out);
extern np_hub_status_t np_mod_qeeg_init(uint8_t slot);
extern np_hub_status_t np_mod_qeeg_control(uint8_t slot, const void *params, uint16_t len);
extern np_hub_status_t np_mod_qeeg_telemetry(uint8_t slot, np_telem_record_t *out);
extern np_hub_status_t np_mod_qeeg_shutdown(uint8_t slot);

/* T2: TMS focal figure-8 coil */
extern np_hub_status_t np_mod_tms_detect(uint8_t slot, np_hub_mod_type_t *type_out);
extern np_hub_status_t np_mod_tms_init(uint8_t slot);
extern np_hub_status_t np_mod_tms_control(uint8_t slot, const void *params, uint16_t len);
extern np_hub_status_t np_mod_tms_telemetry(uint8_t slot, np_telem_record_t *out);
extern np_hub_status_t np_mod_tms_shutdown(uint8_t slot);

/* T2: 1170nm deep PBM laser unit */
extern np_hub_status_t np_mod_pbm_1170nm_detect(uint8_t slot, np_hub_mod_type_t *type_out);
extern np_hub_status_t np_mod_pbm_1170nm_init(uint8_t slot);
extern np_hub_status_t np_mod_pbm_1170nm_control(uint8_t slot, const void *params, uint16_t len);
extern np_hub_status_t np_mod_pbm_1170nm_telemetry(uint8_t slot, np_telem_record_t *out);
extern np_hub_status_t np_mod_pbm_1170nm_shutdown(uint8_t slot);

/* T2: 16-ch clinical tACS driver (tACS mode) */
extern np_hub_status_t np_mod_clin_tacs_detect(uint8_t slot, np_hub_mod_type_t *type_out);
extern np_hub_status_t np_mod_clin_tacs_init(uint8_t slot);
extern np_hub_status_t np_mod_clin_tacs_control(uint8_t slot, const void *params, uint16_t len);
extern np_hub_status_t np_mod_clin_tacs_telemetry(uint8_t slot, np_telem_record_t *out);
extern np_hub_status_t np_mod_clin_tacs_shutdown(uint8_t slot);

/* T2: 16-ch clinical tACS driver (HD-tDCS mode — same PCB, different firmware config) */
extern np_hub_status_t np_mod_hd_tdcs_detect(uint8_t slot, np_hub_mod_type_t *type_out);
extern np_hub_status_t np_mod_hd_tdcs_init(uint8_t slot);
extern np_hub_status_t np_mod_hd_tdcs_control(uint8_t slot, const void *params, uint16_t len);
extern np_hub_status_t np_mod_hd_tdcs_telemetry(uint8_t slot, np_telem_record_t *out);
extern np_hub_status_t np_mod_hd_tdcs_shutdown(uint8_t slot);

/* Accessory: mastoid LRA 40Hz vibrotactile pad */
extern np_hub_status_t np_mod_vibrotactile_detect(uint8_t slot, np_hub_mod_type_t *type_out);
extern np_hub_status_t np_mod_vibrotactile_init(uint8_t slot);
extern np_hub_status_t np_mod_vibrotactile_control(uint8_t slot, const void *params, uint16_t len);
extern np_hub_status_t np_mod_vibrotactile_telemetry(uint8_t slot, np_telem_record_t *out);
extern np_hub_status_t np_mod_vibrotactile_shutdown(uint8_t slot);

/* ── Slot probe table ─────────────────────────────────────────────────────────── */

/*
 * Maps each slot index to its detect function and the function pointers that
 * will be installed when detection succeeds.  A slot may support more than one
 * module type (zone slots can be base or smart); the detect function returns
 * the actual type detected and the registry uses a common function table
 * (np_mod_pbm_* handles both base and smart through the type parameter).
 */
typedef struct {
    np_mod_detect_fn    detect;
    np_mod_init_fn      init;
    np_mod_control_fn   control;
    np_mod_telemetry_fn telemetry;
    np_mod_shutdown_fn  shutdown;
} np_slot_probe_t;

static const np_slot_probe_t k_slot_probes[NP_HUB_SLOT_MAX] = {
    /* Zone slots 0-4: both base and smart PBM modules */
    [NP_HUB_SLOT_ZONE_0]    = { np_mod_pbm_detect, np_mod_pbm_init, np_mod_pbm_control,
                                 np_mod_pbm_telemetry, np_mod_pbm_shutdown },
    [NP_HUB_SLOT_ZONE_1]    = { np_mod_pbm_detect, np_mod_pbm_init, np_mod_pbm_control,
                                 np_mod_pbm_telemetry, np_mod_pbm_shutdown },
    [NP_HUB_SLOT_ZONE_2]    = { np_mod_pbm_detect, np_mod_pbm_init, np_mod_pbm_control,
                                 np_mod_pbm_telemetry, np_mod_pbm_shutdown },
    [NP_HUB_SLOT_ZONE_3]    = { np_mod_pbm_detect, np_mod_pbm_init, np_mod_pbm_control,
                                 np_mod_pbm_telemetry, np_mod_pbm_shutdown },
    [NP_HUB_SLOT_ZONE_4]    = { np_mod_pbm_detect, np_mod_pbm_init, np_mod_pbm_control,
                                 np_mod_pbm_telemetry, np_mod_pbm_shutdown },
    /* Fixed-hardware slots */
    [NP_HUB_SLOT_EEG]       = { np_mod_eeg_detect, np_mod_eeg_init, np_mod_eeg_control,
                                 np_mod_eeg_telemetry, np_mod_eeg_shutdown },
    [NP_HUB_SLOT_AUDIO]     = { np_mod_audio_detect, np_mod_audio_init, np_mod_audio_control,
                                 np_mod_audio_telemetry, np_mod_audio_shutdown },
    /* Detected accessories */
    [NP_HUB_SLOT_VISUAL]    = { np_mod_visual_detect, np_mod_visual_init, np_mod_visual_control,
                                 np_mod_visual_telemetry, np_mod_visual_shutdown },
    [NP_HUB_SLOT_VNS_HRV]  = { np_mod_vns_detect, np_mod_vns_init, np_mod_vns_control,
                                 np_mod_vns_telemetry, np_mod_vns_shutdown },
    [NP_HUB_SLOT_INTRANASAL]= { np_mod_intranasal_detect, np_mod_intranasal_init,
                                 np_mod_intranasal_control, np_mod_intranasal_telemetry,
                                 np_mod_intranasal_shutdown },
    [NP_HUB_SLOT_CVNS]        = { np_mod_cvns_detect, np_mod_cvns_init, np_mod_cvns_control,
                                   np_mod_cvns_telemetry, np_mod_cvns_shutdown },
    /* T2 modalities */
    [NP_HUB_SLOT_QEEG]        = { np_mod_qeeg_detect, np_mod_qeeg_init, np_mod_qeeg_control,
                                   np_mod_qeeg_telemetry, np_mod_qeeg_shutdown },
    [NP_HUB_SLOT_TMS]         = { np_mod_tms_detect, np_mod_tms_init, np_mod_tms_control,
                                   np_mod_tms_telemetry, np_mod_tms_shutdown },
    [NP_HUB_SLOT_PBM_1170NM]  = { np_mod_pbm_1170nm_detect, np_mod_pbm_1170nm_init,
                                   np_mod_pbm_1170nm_control, np_mod_pbm_1170nm_telemetry,
                                   np_mod_pbm_1170nm_shutdown },
    [NP_HUB_SLOT_CLIN_TACS]   = { np_mod_clin_tacs_detect, np_mod_clin_tacs_init,
                                   np_mod_clin_tacs_control, np_mod_clin_tacs_telemetry,
                                   np_mod_clin_tacs_shutdown },
    [NP_HUB_SLOT_HD_TDCS]     = { np_mod_hd_tdcs_detect, np_mod_hd_tdcs_init,
                                   np_mod_hd_tdcs_control, np_mod_hd_tdcs_telemetry,
                                   np_mod_hd_tdcs_shutdown },
    /* Accessory */
    [NP_HUB_SLOT_VIBROTACTILE] = { np_mod_vibrotactile_detect, np_mod_vibrotactile_init,
                                    np_mod_vibrotactile_control, np_mod_vibrotactile_telemetry,
                                    np_mod_vibrotactile_shutdown },
};

/* ── Registry storage ─────────────────────────────────────────────────────────── */

static np_mod_entry_t s_registry[NP_HUB_SLOT_MAX];

/* ── Internal helpers ─────────────────────────────────────────────────────────── */

static np_hub_status_t probe_and_register(uint8_t                  slot,
                                            np_mod_reg_shdr_cb_t     shdr_log_cb)
{
    np_hub_mod_type_t type  = NP_MOD_NONE;
    np_hub_status_t   rc    = k_slot_probes[slot].detect(slot, &type);
    bool              pass  = (rc == NP_HUB_OK && type != NP_MOD_NONE);

    /* Log zone-slot auth events to SHDR (no auth for fixed-hardware slots). */
    if (slot < NP_HUB_ZONE_SLOT_COUNT && shdr_log_cb != NULL) {
        shdr_log_cb(slot, type, pass);
    }

    if (!pass) {
        s_registry[slot].present = false;
        return NP_HUB_ERR_NOT_PRESENT;
    }

    s_registry[slot].type        = type;
    s_registry[slot].slot        = slot;
    s_registry[slot].present     = true;
    s_registry[slot].initialised = false;
    s_registry[slot].init        = k_slot_probes[slot].init;
    s_registry[slot].control     = k_slot_probes[slot].control;
    s_registry[slot].telemetry   = k_slot_probes[slot].telemetry;
    s_registry[slot].shutdown    = k_slot_probes[slot].shutdown;

    /* Initialise hardware for this module. */
    rc = s_registry[slot].init(slot);
    if (rc == NP_HUB_OK) {
        s_registry[slot].initialised = true;
    }
    return rc;
}

/* ── Public API ───────────────────────────────────────────────────────────────── */

void np_mod_reg_init(void)
{
    memset(s_registry, 0, sizeof(s_registry));
}

np_hub_status_t np_mod_reg_scan(np_mod_reg_shdr_cb_t shdr_log_cb)
{
    uint8_t n_present = 0U;

    for (uint8_t slot = 0U; slot < NP_HUB_SLOT_MAX; slot++) {
        np_hub_status_t rc = probe_and_register(slot, shdr_log_cb);
        if (rc == NP_HUB_OK) {
            n_present++;
        }
    }

    return (n_present > 0U) ? NP_HUB_OK : NP_HUB_ERR_NOT_PRESENT;
}

np_mod_entry_t *np_mod_reg_get(uint8_t slot)
{
    if (slot >= NP_HUB_SLOT_MAX) {
        return NULL;
    }
    if (!s_registry[slot].present || !s_registry[slot].initialised) {
        return NULL;
    }
    return &s_registry[slot];
}

np_mod_entry_t *np_mod_reg_find(np_hub_mod_type_t type)
{
    for (uint8_t i = 0U; i < NP_HUB_SLOT_MAX; i++) {
        if (s_registry[i].present && s_registry[i].initialised &&
            s_registry[i].type == type) {
            return &s_registry[i];
        }
    }
    return NULL;
}

uint8_t np_mod_reg_count(void)
{
    uint8_t count = 0U;
    for (uint8_t i = 0U; i < NP_HUB_SLOT_MAX; i++) {
        if (s_registry[i].present && s_registry[i].initialised) {
            count++;
        }
    }
    return count;
}

void np_mod_reg_shutdown_all(void)
{
    for (uint8_t slot = 0U; slot < NP_HUB_SLOT_MAX; slot++) {
        if (s_registry[slot].present &&
            s_registry[slot].initialised &&
            s_registry[slot].shutdown != NULL) {
            (void)s_registry[slot].shutdown(slot);
        }
    }
}

np_hub_status_t np_mod_reg_rescan_zone(uint8_t              zone_slot,
                                         np_mod_reg_shdr_cb_t shdr_log_cb)
{
    if (zone_slot >= NP_HUB_ZONE_SLOT_COUNT) {
        return NP_HUB_ERR_INVALID_ARG;
    }
    /* Shut down any previous occupant of this slot. */
    if (s_registry[zone_slot].present && s_registry[zone_slot].shutdown != NULL) {
        (void)s_registry[zone_slot].shutdown(zone_slot);
    }
    memset(&s_registry[zone_slot], 0, sizeof(np_mod_entry_t));
    return probe_and_register(zone_slot, shdr_log_cb);
}
