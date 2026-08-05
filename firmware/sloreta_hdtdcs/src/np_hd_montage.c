/*
 * NeurOne HD-tDCS — Electrode Montage Selection
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

/* ── tACS driver channel assignment (T2 cap wiring, 21-ch driver) ────────────── */
/*
 * Index = np_hd_electrode_t; value = driver channel 0–20.  One electrode, one
 * driver channel, no sharing.
 *
 * This replaces a 16-channel placeholder that wrapped electrodes 16–20 back onto
 * channels 11–15.  That wrap was an arbitrary modulo rather than a wiring design,
 * and it aliased geometric NEIGHBOURS: Cz↔Pz, C4↔P4, T8↔P8, P7↔O1, P3↔O2.  Since
 * a 4×1 ring draws its cathodes from the electrodes nearest its anode, aliasing
 * neighbours guarantees collisions — C4↔P4 in particular made the M1_R ring
 * undeliverable, because P4 is one of C4's four nearest electrodes.
 *
 * 16 was never the binding number.  A bilateral 4×1 energises 10 electrodes, so
 * 16 channels were always sufficient; every collision came from the mapping, not
 * the count.  Going to 21 removes the question entirely and lets each ring take
 * its geometrically nearest cathodes instead of routing around a wiring artifact.
 *
 * The T2 cap already carries 21 conductors — its Ag/AgCl electrodes are dual-rated
 * for EEG recording and stimulation current (CLAUDE.md §3 T2), so the harness side
 * of this was never 16-channel.  Only the driver was.
 *
 * If cost pressure ever forces sharing again, the rule this table now embodies is:
 * never alias two electrodes within one ring radius (~60 mm) of each other.
 */
static const uint8_t k_driver_channel[NP_HD_CH_COUNT] = {
    /*FP1*/ 0, /*FP2*/ 1, /*F7*/  2, /*F3*/  3, /*FZ*/  4, /*F4*/  5, /*F8*/  6,
    /*FC3*/ 7, /*FC4*/ 8, /*T7*/  9, /*C3*/ 10, /*CZ*/ 11, /*C4*/ 12, /*T8*/ 13,
    /*P7*/ 14, /*P3*/ 15, /*PZ*/ 16, /*P4*/ 17, /*P8*/ 18, /*O1*/ 19, /*O2*/ 20,
};

/* ── Euclidean distance squared (integer, mm²) ───────────────────────────────── */

static int32_t mni_dist_sq(const np_hd_mni_t *a, const np_hd_mni_t *b)
{
    int32_t dx = (int32_t)a->x - (int32_t)b->x;
    int32_t dy = (int32_t)a->y - (int32_t)b->y;
    int32_t dz = (int32_t)a->z - (int32_t)b->z;
    return dx*dx + dy*dy + dz*dz;
}

/* ── Electrode / driver-channel claim tracking ───────────────────────────────── */
/*
 * A montage may contain more than one ring, and NP-FW-HD-001 §6.3 drives every
 * ring of a bilateral montage SIMULTANEOUSLY.  Two claims therefore have to hold
 * across the whole montage, not merely within one ring:
 *
 *   electrodes — one physical electrode carries one net current.  A 3.5 mm
 *                Ag/AgCl pellet cannot be a cathode for two independently driven
 *                rings at once, whatever the wiring says.
 *   channels   — one tACS driver channel is one current source, and §7.1 asks
 *                each montage electrode for its own current (+I_a at the anode,
 *                −I_a/4 at each cathode).
 *
 * Under the current 21-channel mapping the two are equivalent: one electrode maps
 * to exactly one channel, so a repeated channel can only mean a repeated
 * electrode.  The electrode claim is nonetheless the one that matters — it is a
 * statement about physics and holds under ANY wiring, whereas the channel claim
 * only bites if the mapping ever shares a channel between electrodes again.
 *
 * Both are kept.  §6.4 states distinct driver channels as a requirement in its own
 * right, and if cost pressure ever reintroduces sharing the check is already here
 * rather than needing to be rediscovered by another field failure.
 */
typedef struct {
    uint32_t channels;    /* bit c set → driver channel c already claimed         */
    uint32_t electrodes;  /* bit e set → electrode e already claimed              */
} np_hd_claims_t;

static bool claim_taken(const np_hd_claims_t *claims, np_hd_electrode_t electrode)
{
    return ((claims->electrodes & (1UL << (uint8_t)electrode)) != 0UL) ||
           ((claims->channels   & (1UL << k_driver_channel[electrode])) != 0UL);
}

static void claim_take(np_hd_claims_t *claims, np_hd_electrode_t electrode)
{
    claims->electrodes |= (1UL << (uint8_t)electrode);
    claims->channels   |= (1UL << k_driver_channel[electrode]);
}

/* ── Shared 4×1 ring cathode selection ───────────────────────────────────────── */
/*
 * Implements NP-FW-HD-001 §6.2 steps 2, 3 and 5 for ONE ring centred on `center`,
 * against a claim set that may already hold another ring's electrodes.
 *
 * This is the single copy of the selection loop.  It previously existed three
 * times — once in np_hd_montage_select_ring() and again, in its pre-step-5 form,
 * inside np_hd_montage_select_bilateral().  The duplicate is how the bilateral
 * right ring came to reproduce the very C4/P4 driver collision that step 5 was
 * added to prevent: the fix landed on one copy and not the other.
 *
 * With the 21-channel mapping a single ring can no longer collide with itself, so
 * for NP_HD_MONTAGE_RING_4X1 the claim check never fires and selection is pure
 * nearest-neighbour.  It still does real work for bilateral, where the second ring
 * must avoid the first ring's electrodes.
 *
 * On success `center` and all four cathodes are claimed, so a subsequent call for
 * another ring of the same montage cannot select any of them.  On failure the
 * claim set is left partially updated and must be discarded by the caller — every
 * caller here abandons the whole montage on failure.
 */
static np_hd_status_t ring_select_cathodes(
    np_hd_electrode_t  center,
    np_hd_claims_t    *claims,
    np_hd_electrode_t  out_cathodes[NP_HD_RING_CATHODE_COUNT])
{
    if (center >= NP_HD_CH_COUNT) {
        return NP_HD_ERR_MONTAGE_INVALID;
    }
    /*
     * The centre itself may already belong to another ring.  This is what rejects
     * a bilateral montage whose two targets collapse onto one electrode — exactly
     * what a midline target does, since mirroring x = 0 yields x = 0 and the
     * "contralateral" ring is the ring you already have.  Handling it here rather
     * than with an explicit `x == 0` test also covers near-midline targets (x = ±1
     * or ±2), where the mirrored coordinate is distinct but still resolves to the
     * same nearest electrode.
     */
    if (claim_taken(claims, center)) {
        return NP_HD_ERR_MONTAGE_INVALID;
    }
    claim_take(claims, center);

    /* Step 2: candidates = all electrodes except the centre, by distance to it.  */
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

    if (n_candidates < NP_HD_RING_CATHODE_COUNT) {
        return NP_HD_ERR_MONTAGE_INVALID;
    }

    /*
     * Steps 3 and 5 together: take the 4 nearest candidates, but only from those
     * not already claimed.  Applying step 5 during selection rather than after it
     * is what makes np_hd_montage_validate() a genuine post-condition instead of a
     * gate that some clinical targets simply fail.  Skipped candidates fall through
     * to the next-nearest, preserving the nearest-first ordering step 3 specifies.
     */
    for (uint8_t i = 0U; i < NP_HD_RING_CATHODE_COUNT; i++) {
        bool    found   = false;
        uint8_t min_idx = 0U;

        for (uint8_t j = i; j < n_candidates; j++) {
            if (claim_taken(claims, (np_hd_electrode_t)order[j])) {
                continue;   /* electrode or its driver already claimed          */
            }
            if (!found || dist[j] < dist[min_idx]) {
                min_idx = j;
                found   = true;
            }
        }

        if (!found) {
            /* Every remaining candidate collides; no 4×1 ring is deliverable.  */
            return NP_HD_ERR_MONTAGE_INVALID;
        }

        if (min_idx != i) {
            int32_t  tmp_d = dist[i];  dist[i]  = dist[min_idx];  dist[min_idx]  = tmp_d;
            uint8_t  tmp_o = order[i]; order[i] = order[min_idx]; order[min_idx] = tmp_o;
        }
        claim_take(claims, (np_hd_electrode_t)order[i]);
        out_cathodes[i] = (np_hd_electrode_t)order[i];
    }

    /* Step 3 spread rule: cathodes must cover ≥ 2 quadrants around the centre.   */
    /* Quadrant defined by sign of (cathode_mni - center_mni) in x and y.        */
    uint8_t quadrant_mask = 0U;
    for (uint8_t k = 0U; k < NP_HD_RING_CATHODE_COUNT; k++) {
        int16_t dx = k_electrode_mni[out_cathodes[k]].x - k_electrode_mni[center].x;
        int16_t dy = k_electrode_mni[out_cathodes[k]].y - k_electrode_mni[center].y;
        uint8_t q  = (uint8_t)((dx >= 0 ? 1U : 0U) | ((dy >= 0 ? 1U : 0U) << 1));
        quadrant_mask |= (uint8_t)(1U << q);
    }
    if (__builtin_popcount(quadrant_mask) < 2) {
        return NP_HD_ERR_MONTAGE_INVALID;
    }

    return NP_HD_OK;
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

    /* Steps 2, 3 and 5: one ring, starting from an empty claim set.            */
    np_hd_claims_t    claims = { 0U, 0U };
    np_hd_electrode_t cathodes[NP_HD_RING_CATHODE_COUNT];

    np_hd_status_t ret = ring_select_cathodes(center, &claims, cathodes);
    if (ret != NP_HD_OK) {
        return ret;
    }

    memset(out, 0, sizeof(*out));
    out->type          = NP_HD_MONTAGE_RING_4X1;
    out->center        = center;
    out->cathode_count = NP_HD_RING_CATHODE_COUNT;
    out->target_mni    = *target_mni;

    for (uint8_t k = 0U; k < NP_HD_RING_CATHODE_COUNT; k++) {
        out->cathodes[k] = cathodes[k];
    }

    return NP_HD_OK;
}

np_hd_status_t np_hd_montage_select_bilateral(const np_hd_mni_t *target_mni,
                                               np_hd_montage_t   *out)
{
    if (!target_mni || !out) {
        return NP_HD_ERR_INVALID_ARG;
    }

    /*
     * NP-FW-HD-001 §6.3 drives both hemispheres SIMULTANEOUSLY, so the two rings
     * are ten electrodes energised at once, not two montages that happen to be
     * described together.  Both are therefore selected against ONE claim set:
     * whatever the first ring takes, the second cannot have.  The invariant then
     * holds by construction and np_hd_montage_validate() is a real post-condition.
     *
     * Contention policy — the PRIMARY ring wins, the MIRROR ring yields.
     *
     * "Primary" is the ring built at the caller's actual target; "mirror" is the
     * one built from the reflected coordinate this function derives.  Priority
     * follows provenance rather than laterality: the caller asked for the primary
     * target, and the mirror is inferred from it.  The contested electrode is in
     * practice a midline one — Cz for M1_L/M1_R, where both rings want it as a
     * cathode — and the mirror ring falls through to its next-nearest candidate.
     *
     * The alternative, rejecting bilateral outright whenever the hemispheres
     * contend, was considered and not taken.  It would leave M1_L and M1_R — named
     * motor-rehabilitation targets — with no deliverable bilateral montage in order
     * to preserve geometric symmetry between the rings, and §6.3 never required
     * that symmetry.  It says the hemispheres are driven "from separate driver
     * channel pairs", which is a distinctness requirement, not a mirror-image one.
     *
     * With 21 driver channels for 10 simultaneously active electrodes, the only
     * contention left is for the electrodes themselves — the two rings must not
     * share a pellet — so the mirror ring yields only where the rings genuinely
     * overlap, which on this cap means the midline.  A target whose mirror ring
     * still cannot be completed is reported as NP_HD_ERR_MONTAGE_INVALID rather
     * than emitting a ring that validate() would reject.
     */
    /* Copy the target before zeroing `out`: a caller may legitimately refresh a  */
    /* montage in place with np_hd_montage_select_bilateral(&m.target_mni, &m),   */
    /* in which case target_mni aliases into the buffer about to be cleared.      */
    const np_hd_mni_t target = *target_mni;

    memset(out, 0, sizeof(*out));

    np_hd_claims_t    claims = { 0U, 0U };
    np_hd_electrode_t primary_cathodes[NP_HD_RING_CATHODE_COUNT];
    np_hd_electrode_t mirror_cathodes[NP_HD_RING_CATHODE_COUNT];

    np_hd_electrode_t primary_center = np_hd_nearest_electrode(&target, NULL);

    np_hd_status_t ret = ring_select_cathodes(primary_center, &claims,
                                               primary_cathodes);
    if (ret != NP_HD_OK) {
        return ret;
    }

    /* Mirror ring: negate x.  A midline target mirrors onto itself, so its       */
    /* centre is already claimed and ring_select_cathodes() rejects it — bilateral */
    /* is undefined for a target with no contralateral homologue, not merely       */
    /* contended.  ACC (0, 28, 28) and MPFC (0, 52, 6) are the two predefined      */
    /* clinical targets that hit this; both previously returned a unilateral       */
    /* montage with every electrode duplicated and validate() reported it OK.      */
    np_hd_mni_t target_r = target;
    target_r.x = (int16_t)(-target.x);

    np_hd_electrode_t mirror_center = np_hd_nearest_electrode(&target_r, NULL);

    ret = ring_select_cathodes(mirror_center, &claims, mirror_cathodes);
    if (ret != NP_HD_OK) {
        return ret;   /* `out` stays zeroed — never a half-built bilateral ring */
    }

    out->type          = NP_HD_MONTAGE_BILATERAL_4X1;
    out->center        = primary_center;
    out->center_r      = mirror_center;
    out->cathode_count = NP_HD_RING_CATHODE_COUNT;
    out->target_mni    = target;
    out->bilateral     = 1U;

    for (uint8_t k = 0U; k < NP_HD_RING_CATHODE_COUNT; k++) {
        out->cathodes[k]   = primary_cathodes[k];
        out->cathodes_r[k] = mirror_cathodes[k];
    }

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
    if (montage->cathode_count > NP_HD_RING_CATHODE_COUNT) {
        return NP_HD_ERR_MONTAGE_INVALID;  /* would over-read cathodes[]        */
    }

    /*
     * One claim set spans the WHOLE montage.  For a bilateral montage that is all
     * ten electrodes of both rings together (NP-FW-HD-001 §6.3 energises them
     * simultaneously), so a conflict between hemispheres is as disqualifying as one
     * inside a hemisphere.  Checking the rings separately would miss exactly the
     * case that matters: a midline electrode such as Cz named as a cathode by both.
     *
     * claim_taken() rejects on either a repeated electrode or a repeated driver
     * channel; see the note above the claim helpers for why both are required.
     */
    np_hd_claims_t claims = { 0U, 0U };
    claim_take(&claims, montage->center);

    for (uint8_t k = 0U; k < montage->cathode_count; k++) {
        np_hd_electrode_t ch = montage->cathodes[k];
        if (ch >= NP_HD_CH_COUNT) {
            return NP_HD_ERR_MONTAGE_INVALID;
        }
        if (claim_taken(&claims, ch)) {
            return NP_HD_ERR_MONTAGE_INVALID;  /* duplicate electrode or driver  */
        }
        claim_take(&claims, ch);
    }

    if (!montage->bilateral) {
        return NP_HD_OK;
    }

    /*
     * A bilateral montage is two full 4×1 rings (NP_HD_BILATERAL_ELEC_COUNT = 10).
     * The loop below walks cathodes_r[] with cathode_count, so a montage claiming
     * `bilateral` with a shorter count would leave the tail of the right ring
     * unexamined while still reporting OK.
     */
    if (montage->cathode_count != NP_HD_RING_CATHODE_COUNT) {
        return NP_HD_ERR_MONTAGE_INVALID;
    }

    /* Right-hemisphere ring, against the same claims — never a fresh set.       */
    if (montage->center_r >= NP_HD_CH_COUNT) {
        return NP_HD_ERR_MONTAGE_INVALID;
    }
    if (claim_taken(&claims, montage->center_r)) {
        return NP_HD_ERR_MONTAGE_INVALID;
    }
    claim_take(&claims, montage->center_r);

    for (uint8_t k = 0U; k < montage->cathode_count; k++) {
        np_hd_electrode_t ch = montage->cathodes_r[k];
        if (ch >= NP_HD_CH_COUNT) {
            return NP_HD_ERR_MONTAGE_INVALID;
        }
        if (claim_taken(&claims, ch)) {
            return NP_HD_ERR_MONTAGE_INVALID;
        }
        claim_take(&claims, ch);
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
