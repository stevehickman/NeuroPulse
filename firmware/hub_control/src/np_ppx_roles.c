/*
 * NeurOne Hub Control Program — the photoparoxysmal detector's channel roles as data. See np_ppx_roles.h.
 */
#include "np_ppx_roles.h"

#include <stddef.h>

/* PROVISIONAL (UC-125): the engineering proposal, not a clinical finding. Edit THIS TABLE to change a role; nothing else
 * encodes one. */
static const np_ppx_site_role_t ROLE_TABLE[] = {
    /* site            occipital judges  clinical_decides */
    { NP_PPX_SITE_FP1, false,    false,  false },   /* blink and frontalis artefact: excluded */
    { NP_PPX_SITE_FP2, false,    false,  false },
    { NP_PPX_SITE_F3,  false,    false,  true  },   /* Clinical decides */
    { NP_PPX_SITE_F4,  false,    false,  true  },
    { NP_PPX_SITE_FZ,  false,    false,  true  },
    { NP_PPX_SITE_C3,  false,    true,   false },
    { NP_PPX_SITE_C4,  false,    true,   false },
    { NP_PPX_SITE_CZ,  false,    true,   false },
    { NP_PPX_SITE_P3,  false,    true,   false },
    { NP_PPX_SITE_P4,  false,    true,   false },
    { NP_PPX_SITE_PZ,  false,    true,   false },
    { NP_PPX_SITE_O1,  true,     false,  false },
    { NP_PPX_SITE_O2,  true,     false,  false },
    { NP_PPX_SITE_OZ,  true,     false,  false },
};

const np_ppx_site_role_t *np_ppx_role_table(uint8_t *n_rows)
{
    if (n_rows != NULL) { *n_rows = (uint8_t)(sizeof ROLE_TABLE / sizeof ROLE_TABLE[0]); }
    return ROLE_TABLE;
}

void np_ppx_montage_t1(np_ppx_montage_t *out)
{
    static const np_ppx_site_t T1[NP_PPX_MAX_CH] = {
        NP_PPX_SITE_FP1, NP_PPX_SITE_FP2, NP_PPX_SITE_F3, NP_PPX_SITE_F4,
        NP_PPX_SITE_C3,  NP_PPX_SITE_C4,  NP_PPX_SITE_P3, NP_PPX_SITE_P4,
    };
    for (uint8_t i = 0U; i < NP_PPX_MAX_CH; i++) { out->channel_site[i] = T1[i]; }
}

np_ppx_roles_report_t np_ppx_roles_resolve(const np_ppx_montage_t *montage, const np_ppx_site_role_t *table, uint8_t n_rows)
{
    np_ppx_roles_report_t r = { { 0U, 0U }, 0U, 0U, 0U, false };
    if (montage == NULL) { return r; }
    if (table == NULL) { table = np_ppx_role_table(&n_rows); }

    for (uint8_t ch = 0U; ch < NP_PPX_MAX_CH; ch++) {
        const np_ppx_site_role_t *row = NULL;
        for (uint8_t i = 0U; i < n_rows; i++) {
            if (table[i].site == montage->channel_site[ch] && table[i].site != NP_PPX_SITE_NONE) { row = &table[i]; break; }
        }
        if (row == NULL) { continue; }                 /* no site, or a site the table gives no role: takes no part */
        if (row->occipital) {
            r.roles.occipital_mask = (uint8_t)(r.roles.occipital_mask | (1U << ch));
            r.n_occipital++;
        } else if (row->judges_generalised) {
            r.roles.other_mask = (uint8_t)(r.roles.other_mask | (1U << ch));
            r.n_judging++;
        } else if (row->clinical_decides) {
            r.n_clinical_open++;
        }
    }
    np_ppx_params_t p;
    np_ppx_params_provisional(&p);
    r.can_ever_see = r.n_occipital >= 1U && p.min_other_channels >= 1U && r.n_judging >= p.min_other_channels;
    return r;
}
