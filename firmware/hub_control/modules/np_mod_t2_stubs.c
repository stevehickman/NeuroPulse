/*
 * NeurOne Hub Control — T2 and Accessory Module Stubs
 * Document: NP-FW-HUB-001 Rev 2
 *
 * Placeholder detect/init/control/telemetry/shutdown implementations for T2
 * and accessory modalities.  All detect functions return NP_HUB_ERR_NOT_PRESENT
 * until real hardware drivers are built for each modality.
 *
 * Slot assignments:
 *   NP_HUB_SLOT_QEEG        (11) — T2 21-ch wet-gel qEEG cap
 *   NP_HUB_SLOT_TMS         (12) — T2 TMS focal figure-8 coil
 *   NP_HUB_SLOT_PBM_1170NM  (13) — T2 1170nm deep PBM laser unit
 *   NP_HUB_SLOT_CLIN_TACS   (14) — T2 21-ch clinical tACS (tACS mode)
 *   NP_HUB_SLOT_HD_TDCS     (15) — T2 21-ch clinical tACS (HD-tDCS mode)
 *   NP_HUB_SLOT_VIBROTACTILE (16) — Accessory mastoid LRA 40Hz pad
 *
 * Safety MCU enable bits for T2 stimulation modalities:
 *   NP_SAFETY_EN_TMS         (bit 11)
 *   NP_SAFETY_EN_PBM_1170NM  (bit 12)
 *   NP_SAFETY_EN_CLIN_STIM   (bit 13)  — covers both CLIN_TACS and HD_TDCS
 */

#include "np_hub_types.h"
#include "np_hub_config.h"
#include <string.h>

/* ── Macro for stub body ──────────────────────────────────────────────────────── */

#define STUB_DETECT(fn)                                                    \
    np_hub_status_t fn(uint8_t slot, np_hub_mod_type_t *type_out) {       \
        (void)slot; (void)type_out;                                        \
        return NP_HUB_ERR_NOT_PRESENT;                                     \
    }

#define STUB_INIT(fn)                                                      \
    np_hub_status_t fn(uint8_t slot) {                                     \
        (void)slot;                                                        \
        return NP_HUB_ERR_NOT_PRESENT;                                     \
    }

#define STUB_CONTROL(fn)                                                   \
    np_hub_status_t fn(uint8_t slot, const void *params, uint16_t len) {  \
        (void)slot; (void)params; (void)len;                               \
        return NP_HUB_ERR_NOT_PRESENT;                                     \
    }

#define STUB_TELEMETRY(fn)                                                 \
    np_hub_status_t fn(uint8_t slot, np_telem_record_t *out) {            \
        (void)slot; (void)out;                                             \
        return NP_HUB_ERR_NOT_PRESENT;                                     \
    }

#define STUB_SHUTDOWN(fn)                                                  \
    np_hub_status_t fn(uint8_t slot) {                                     \
        (void)slot;                                                        \
        return NP_HUB_OK;                                                  \
    }

/* ── T2: 21-ch qEEG wet-gel cap ──────────────────────────────────────────────── */

STUB_DETECT   (np_mod_qeeg_detect)
STUB_INIT     (np_mod_qeeg_init)
STUB_CONTROL  (np_mod_qeeg_control)
STUB_TELEMETRY(np_mod_qeeg_telemetry)
STUB_SHUTDOWN (np_mod_qeeg_shutdown)

/* ── T2: TMS focal figure-8 coil ─────────────────────────────────────────────── */

STUB_DETECT   (np_mod_tms_detect)
STUB_INIT     (np_mod_tms_init)
STUB_CONTROL  (np_mod_tms_control)
STUB_TELEMETRY(np_mod_tms_telemetry)
STUB_SHUTDOWN (np_mod_tms_shutdown)

/* ── T2: 1170nm deep PBM laser ───────────────────────────────────────────────── */

STUB_DETECT   (np_mod_pbm_1170nm_detect)
STUB_INIT     (np_mod_pbm_1170nm_init)
STUB_CONTROL  (np_mod_pbm_1170nm_control)
STUB_TELEMETRY(np_mod_pbm_1170nm_telemetry)
STUB_SHUTDOWN (np_mod_pbm_1170nm_shutdown)

/* ── T2: 21-ch clinical tACS (tACS mode) ────────────────────────────────────── */

STUB_DETECT   (np_mod_clin_tacs_detect)
STUB_INIT     (np_mod_clin_tacs_init)
STUB_CONTROL  (np_mod_clin_tacs_control)
STUB_TELEMETRY(np_mod_clin_tacs_telemetry)
STUB_SHUTDOWN (np_mod_clin_tacs_shutdown)

/* ── T2: 21-ch clinical tACS (HD-tDCS mode) ─────────────────────────────────── */

STUB_DETECT   (np_mod_hd_tdcs_detect)
STUB_INIT     (np_mod_hd_tdcs_init)
STUB_CONTROL  (np_mod_hd_tdcs_control)
STUB_TELEMETRY(np_mod_hd_tdcs_telemetry)
STUB_SHUTDOWN (np_mod_hd_tdcs_shutdown)

/* ── Accessory: mastoid LRA 40Hz vibrotactile ───────────────────────────────── */

STUB_DETECT   (np_mod_vibrotactile_detect)
STUB_INIT     (np_mod_vibrotactile_init)
STUB_CONTROL  (np_mod_vibrotactile_control)
STUB_TELEMETRY(np_mod_vibrotactile_telemetry)
STUB_SHUTDOWN (np_mod_vibrotactile_shutdown)
