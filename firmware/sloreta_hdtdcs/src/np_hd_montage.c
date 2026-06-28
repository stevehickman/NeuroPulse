/*
 * NeuroPulse HD-tDCS — Electrode Montage Selection
 * Document: NP-FW-HD-001 Rev A §6
 *
 * MNI scalp coordinates from Jurcak et al. (2007) NeuroImage — 10-20 system
 * scalp-to-MNI atlas for the MNI152 standard head.
 */

#include "np_hd_montage.h"
#include <string.h>
#include <math.h>

/* ── 10-20 MNI scalp coordinate table ───────────────────────────────────────── */
/* One entry per np_hd_electrode_t enum value (0–20).                           */

static const np_hd_mni_t k_electrode_mni[NP_HD_CH_COUNT] = {
    /* FP1  */ { -21,  66,   5 },
    /* FP2  */ {  21,  66,   5 },
    /* F7   */ { -51,  26,  -2 },
    /* F3   */ { -35,  36,  64 },
    /* FZ   */ {   0,  31,  75 },
    /* F4   */ {  35,  36,  64 },
    /* F8   */ {  51,  26,  -2 },
    /* FC3  */ { -40,  14,  67 },
    /* FC4  */ {  40,  14,  67 },
    /* T7   */ { -70, -17,  -2 },
    /* C3   */ { -55,   0,  67 },
    /* CZ   */ {   0, -10,  83 },
    /* C4   */ {  55,   0,  67 },
    /* T8   */ {  70, -17,  -2 },
    /* P7   */ { -51, -55,  -2 },
    /* P3   */ { -35, -55,  64 },
    /* PZ   */ {   0, -65,  75 },
    /* P4   */ {  35, -55,  64 },
    /* P8   */ {  51, -55,  -2 },
    /* O1   */ { -21, -85,   5 },
    /* O2   */ {  21, -85,   5 },
};

static const char *const k_electrode_names[NP_HD_CH_COUNT] = {
    "Fp1", "Fp2", "F7", "F3", "Fz", "F4", "F8",
    "FC3", "FC4", "T7", "C3", "Cz", "C4", "T8",
    "P7", "P3", "Pz", "P4", "P8", "O1", "O2",
};

/* ── Predefined clinical target MNI coordinates ──────────────────────────────── */

static const np_hd_mni_t k_clinical_targets[] = {
    /* DLPFC_L  */ { -46,  36,  20 },
    /* DLPFC_R  */ {  46,  36,  20 },
    /* VLPFC_L  */ { -51,  15,   0 },
    /* ACC      */ {   0,  28,  28 },
    /* MPFC     */ {   0,  52,   6 },
    /* M1_L     */ { -37, -21,  58 },
    /* M1_R     */ {  37, -21,  58 },
};

/* ── tACS driver channel assignment (T2 cap wiring, 16-ch driver) ────────────── */
/* Index = np_hd_electrode_t; value = driver channel 0–15.                      */
static const uint8_t k_driver_channel[NP_HD_CH_COUNT] = {
    /*FP1*/0, /*FP2*/1, /*F7*/2,  /*F3*/3,  /*FZ*/4,  /*F4*/5,  /*F8*/6,
    /*FC3*/7, /*FC4*/8, /*T7*/9,  /*C3*/10, /*CZ*/11, /*C4*/12, /*T8*/13,
    /*P7*/14, /*P3*/15,
    /* P3–O2: channels 15 and above wrap to shared channels for standard 2-elec.*/
    /* In 4×1 montages, only 5 of the 16 channels are active simultaneously.   */
    /*PZ*/11, /*P4*/12, /*P8*/13, /*O1*/14, /*O2*/15,
};

/* ── Euclidean distance squared (integer, mm²) ───────────────────────────────── */

static int32_t mni_dist_sq(const np_hd_mni_t *a, const np_hd_mni_t *b)
{
    int32_t dx = (int32_t)a->x - (int32_t)b->x;
    int32_t dy = (int32_t)a->y - (int32_t)b->y;
    int32_t dz = (int32_t)a->z - (int32_t)b->z;
    return dx*dx + dy*dy + dz*dz;
}

/* ── Public API ──────────────────────────────────────────────────────────────── */

np_hd_status_t np_hd_electrode_mni(np_hd_electrode_t electrode, np_hd_mni_t *out)
{
    if (!out || electrode >= NP_HD_CH_COUNT) {
        return NP_HD_ERR_INVALID_ARG;
    }
    *out = k_electrode_mni[electrode];
    return NP_HD_OK;
}

np_hd_electrode_t np_hd_nearest_electrode(const np_hd_mni_t *target_mni,
                                           float             *out_dist_mm)
{
    if (!target_mni) {
        return NP_HD_CH_NONE;
    }
    uint8_t  best_ch   = 0U;
    int32_t  best_dsq  = mni_dist_sq(target_mni, &k_electrode_mni[0]);

    for (uint8_t ch = 1U; ch < NP_HD_CH_COUNT; ch++) {
        int32_t dsq = mni_dist_sq(target_mni, &k_electrode_mni[ch]);
        if (dsq < best_dsq) {
            best_dsq = dsq;
            best_ch  = ch;
        }
    }
    if (out_dist_mm) {
        *out_dist_mm = sqrtf((float)best_dsq);
    }
    return (np_hd_electrode_t)best_ch;
}

np_hd_status_t np_hd_montage_select_ring(const np_hd_mni_t *target_mni,
                                          np_hd_montage_t   *out)
{
    if (!target_mni || !out) {
        return NP_HD_ERR_INVALID_ARG;
    }

    /* Step 1: find center electrode (anode). */
    np_hd_electrode_t center = np_hd_nearest_electrode(target_mni, NULL);

    /* Step 2: find 4 nearest electrodes to the center, excluding itself.       */
    /* Sort by distance to center using a simple insertion selection.            */
    int32_t  dist[NP_HD_CH_COUNT];
    uint8_t  order[NP_HD_CH_COUNT];
    uint8_t  n_candidates = 0U;

    for (uint8_t ch = 0U; ch < NP_HD_CH_COUNT; ch++) {
        if (ch == (uint8_t)center) {
            continue;
        }
        dist[n_candidates]  = mni_dist_sq(&k_electrode_mni[center],
                                           &k_electrode_mni[ch]);
        order[n_candidates] = ch;
        n_candidates++;
    }

    /* Partial insertion sort: put the 4 smallest distances first.               */
    for (uint8_t i = 0U; i < NP_HD_RING_CATHODE_COUNT && i < n_candidates; i++) {
        uint8_t min_idx = i;
        for (uint8_t j = i + 1U; j < n_candidates; j++) {
            if (dist[j] < dist[min_idx]) {
                min_idx = j;
            }
        }
        if (min_idx != i) {
            int32_t  tmp_d = dist[i];  dist[i]  = dist[min_idx];  dist[min_idx]  = tmp_d;
            uint8_t  tmp_o = order[i]; order[i] = order[min_idx]; order[min_idx] = tmp_o;
        }
    }

    if (n_candidates < NP_HD_RING_CATHODE_COUNT) {
        return NP_HD_ERR_MONTAGE_INVALID;
    }

    /* Validate angular spread: cathodes must cover ≥ 2 quadrants around center.*/
    /* Quadrant defined by sign of (cathode_mni - center_mni) in x and y.       */
    uint8_t quadrant_mask = 0U;
    for (uint8_t k = 0U; k < NP_HD_RING_CATHODE_COUNT; k++) {
        uint8_t ch = order[k];
        int16_t dx = k_electrode_mni[ch].x - k_electrode_mni[center].x;
        int16_t dy = k_electrode_mni[ch].y - k_electrode_mni[center].y;
        uint8_t q  = (uint8_t)((dx >= 0 ? 1U : 0U) | ((dy >= 0 ? 1U : 0U) << 1));
        quadrant_mask |= (uint8_t)(1U << q);
    }
    if (__builtin_popcount(quadrant_mask) < 2U) {
        return NP_HD_ERR_MONTAGE_INVALID;
    }

    memset(out, 0, sizeof(*out));
    out->type          = NP_HD_MONTAGE_RING_4X1;
    out->center        = center;
    out->cathode_count = NP_HD_RING_CATHODE_COUNT;
    out->target_mni    = *target_mni;

    for (uint8_t k = 0U; k < NP_HD_RING_CATHODE_COUNT; k++) {
        out->cathodes[k] = (np_hd_electrode_t)order[k];
    }

    return NP_HD_OK;
}

np_hd_status_t np_hd_montage_select_bilateral(const np_hd_mni_t *target_mni,
                                               np_hd_montage_t   *out)
{
    if (!target_mni || !out) {
        return NP_HD_ERR_INVALID_ARG;
    }

    /* Left hemisphere ring (use provided target). */
    np_hd_status_t ret = np_hd_montage_select_ring(target_mni, out);
    if (ret != NP_HD_OK) {
        return ret;
    }

    /* Right hemisphere ring: mirror x-coordinate. */
    np_hd_mni_t target_r = *target_mni;
    target_r.x = (int16_t)(-target_mni->x);

    out->center_r = np_hd_nearest_electrode(&target_r, NULL);

    int32_t  dist[NP_HD_CH_COUNT];
    uint8_t  order[NP_HD_CH_COUNT];
    uint8_t  n_candidates = 0U;

    for (uint8_t ch = 0U; ch < NP_HD_CH_COUNT; ch++) {
        if (ch == (uint8_t)out->center_r) {
            continue;
        }
        dist[n_candidates]  = mni_dist_sq(&k_electrode_mni[out->center_r],
                                           &k_electrode_mni[ch]);
        order[n_candidates] = ch;
        n_candidates++;
    }
    for (uint8_t i = 0U; i < NP_HD_RING_CATHODE_COUNT && i < n_candidates; i++) {
        uint8_t min_idx = i;
        for (uint8_t j = i + 1U; j < n_candidates; j++) {
            if (dist[j] < dist[min_idx]) {
                min_idx = j;
            }
        }
        if (min_idx != i) {
            int32_t  td = dist[i];  dist[i]  = dist[min_idx];  dist[min_idx]  = td;
            uint8_t  to = order[i]; order[i] = order[min_idx]; order[min_idx] = to;
        }
    }
    for (uint8_t k = 0U; k < NP_HD_RING_CATHODE_COUNT; k++) {
        out->cathodes_r[k] = (np_hd_electrode_t)order[k];
    }

    out->type      = NP_HD_MONTAGE_BILATERAL_4X1;
    out->bilateral = 1U;
    return NP_HD_OK;
}

np_hd_status_t np_hd_montage_select_standard(const np_hd_mni_t *target_mni,
                                              np_hd_montage_t   *out)
{
    if (!target_mni || !out) {
        return NP_HD_ERR_INVALID_ARG;
    }
    memset(out, 0, sizeof(*out));
    out->type          = NP_HD_MONTAGE_STANDARD_2E;
    out->center        = np_hd_nearest_electrode(target_mni, NULL);
    out->cathode_count = NP_HD_STD_2ELEC_COUNT - 1U;  /* 1 cathode             */
    out->target_mni    = *target_mni;

    /* Contralateral homologue: nearest electrode with x-sign inverted.         */
    np_hd_mni_t mirror = *target_mni;
    mirror.x = (int16_t)(-target_mni->x);
    out->cathodes[0] = np_hd_nearest_electrode(&mirror, NULL);

    if (out->cathodes[0] == out->center) {
        return NP_HD_ERR_MONTAGE_INVALID;
    }
    return NP_HD_OK;
}

np_hd_status_t np_hd_montage_from_mni(const np_hd_mni_t    *target_mni,
                                        np_hd_montage_type_t  montage_type,
                                        np_hd_montage_t      *out)
{
    switch (montage_type) {
        case NP_HD_MONTAGE_RING_4X1:
            return np_hd_montage_select_ring(target_mni, out);
        case NP_HD_MONTAGE_BILATERAL_4X1:
            return np_hd_montage_select_bilateral(target_mni, out);
        case NP_HD_MONTAGE_STANDARD_2E:
            return np_hd_montage_select_standard(target_mni, out);
        default:
            return NP_HD_ERR_INVALID_ARG;
    }
}

np_hd_status_t np_hd_montage_validate(const np_hd_montage_t *montage)
{
    if (!montage) {
        return NP_HD_ERR_INVALID_ARG;
    }
    if (montage->center >= NP_HD_CH_COUNT) {
        return NP_HD_ERR_MONTAGE_INVALID;
    }

    uint32_t used_channels = (1UL << k_driver_channel[montage->center]);

    for (uint8_t k = 0U; k < montage->cathode_count; k++) {
        uint8_t ch = (uint8_t)montage->cathodes[k];
        if (ch >= NP_HD_CH_COUNT) {
            return NP_HD_ERR_MONTAGE_INVALID;
        }
        uint32_t mask = (1UL << k_driver_channel[ch]);
        if (used_channels & mask) {
            return NP_HD_ERR_MONTAGE_INVALID;  /* driver channel conflict        */
        }
        used_channels |= mask;
    }
    return NP_HD_OK;
}

np_hd_status_t np_hd_montage_assign_channels(np_hd_montage_t *montage)
{
    if (!montage) {
        return NP_HD_ERR_INVALID_ARG;
    }
    /* Channel assignments are static (from T2 cap wiring spec).                */
    /* np_hd_stim_init() reads them directly from k_driver_channel[] via        */
    /* np_hd_electrode_t indices — no further action needed here.               */
    return np_hd_montage_validate(montage);
}

np_hd_status_t np_hd_clinical_target_mni(np_hd_clinical_target_t target,
                                          np_hd_mni_t            *out)
{
    if (!out || target >= NP_HD_TARGET_CUSTOM) {
        return NP_HD_ERR_INVALID_ARG;
    }
    *out = k_clinical_targets[target];
    return NP_HD_OK;
}

const char *np_hd_electrode_name(np_hd_electrode_t electrode)
{
    if (electrode >= NP_HD_CH_COUNT) {
        return "??";
    }
    return k_electrode_names[electrode];
}
