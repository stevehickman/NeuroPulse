/*
 * NeurOne Hub Control Program — the photoparoxysmal detector's channel roles, as DATA (OI-FWHUB-32 (1a), REQ-FWHUB-67 / -69)
 *
 * The detector (np_ppx_detect.h) needs two masks over the ADC channels: which are occipital, and which judge a discharge
 * generalised. This file says where those masks come from, and it says it in a table that Clinical edits, not in code:
 *
 *   - the MONTAGE says which scalp site each ADC channel carries (a fact of the hardware);
 *   - the ROLE TABLE says, for each scalp site, whether it is an occipital site and whether it judges generalisation
 *     (a clinical choice, PROVISIONAL, UC-125).
 *
 * np_ppx_roles_resolve() turns the two into the detector's masks. A channel that carries no site, or a site the table gives no
 * role, takes no part. If the montage has no occipital site the result has an empty occipital mask and the detector sees
 * nothing (REQ-FWHUB-69): that is the correct answer for a helmet that cannot look at the occipital cortex, and it is reported
 * as such (np_ppx_roles_report), not hidden.
 *
 * THE T1 MONTAGE HAS NO OCCIPITAL CHANNEL. NP_EEG_CHANNELS is 8 (Fp1/2, F3/4, C3/4, P3/4); Oz is a ninth site that the hex-tile
 * documents place but that no ADC channel carries (OI-FWHUB-34). Until Systems and Clinical decide how Oz is digitised, the
 * shipped T1 montage resolves to "cannot see", which is what keeps the 3 to 30 Hz refusal honest.
 *
 * The role table's defaults are the engineering proposal in the principal's thread of 2026-11-08, not a clinical finding:
 *   - occipital: O1, O2, Oz (where the photic response is generated);
 *   - judging generalisation: central and parietal sites (C3, C4, Cz, P3, P4, Pz), which are the cleanest places a generalised
 *     discharge shows;
 *   - NOT judging: Fp1, Fp2 (blink and frontalis-muscle artefact, including the flash-locked muscle response, would read as a
 *     simultaneous spike and halt sessions for nothing);
 *   - CLINICAL DECIDES: F3, F4, Fz. Their default is "not judging"; `clinical_decides` marks the row as one Clinical must
 *     rule on before the evidence level can rise above NONE.
 * Whether the judging channels must come from both hemispheres is a rule the detector does not have (OI-FWHUB-34).
 */
#ifndef NP_PPX_ROLES_H
#define NP_PPX_ROLES_H

#include <stdbool.h>
#include <stdint.h>

#include "np_ppx_detect.h"

typedef enum {
    NP_PPX_SITE_NONE = 0,       /* the channel carries no scalp site (unused, or not a scalp electrode) */
    NP_PPX_SITE_FP1, NP_PPX_SITE_FP2,
    NP_PPX_SITE_F3,  NP_PPX_SITE_F4,  NP_PPX_SITE_FZ,
    NP_PPX_SITE_C3,  NP_PPX_SITE_C4,  NP_PPX_SITE_CZ,
    NP_PPX_SITE_P3,  NP_PPX_SITE_P4,  NP_PPX_SITE_PZ,
    NP_PPX_SITE_O1,  NP_PPX_SITE_O2,  NP_PPX_SITE_OZ,
    NP_PPX_SITE_COUNT
} np_ppx_site_t;

typedef struct {
    np_ppx_site_t site;
    bool          occipital;          /* an occipital electrode: a discharge here alone is OCCIPITAL-ONLY                     */
    bool          judges_generalised; /* counts toward min_other_channels                                                     */
    bool          clinical_decides;   /* Clinical must rule on this row; the values are the engineering default (UC-125)      */
} np_ppx_site_role_t;

typedef struct {
    np_ppx_site_t channel_site[NP_PPX_MAX_CH];   /* the site each ADC channel carries, in ADC order */
} np_ppx_montage_t;

typedef struct {
    np_ppx_roles_t roles;
    uint8_t        n_occipital;            /* channels with an occipital role                                    */
    uint8_t        n_judging;              /* channels that judge generalisation                                 */
    uint8_t        n_clinical_open;        /* channels whose role is an unruled clinical choice (not counted)    */
    bool           can_ever_see;           /* an occipital channel and enough judging channels exist at all      */
} np_ppx_roles_report_t;

/* The role table the hub ships (PROVISIONAL, UC-125), one row per site, and its length. */
const np_ppx_site_role_t *np_ppx_role_table(uint8_t *n_rows);

/* The T1 montage as documented: ADC channels 0..7 = Fp1 Fp2 F3 F4 C3 C4 P3 P4. The channel ORDER is not documented anywhere
 * (OI-FWHUB-34); the site list is. No occipital site. */
void np_ppx_montage_t1(np_ppx_montage_t *out);

/* Resolve a montage against a role table into the detector's masks. A NULL table means the shipped one. */
np_ppx_roles_report_t np_ppx_roles_resolve(const np_ppx_montage_t *montage, const np_ppx_site_role_t *table, uint8_t n_rows);

#endif /* NP_PPX_ROLES_H */
