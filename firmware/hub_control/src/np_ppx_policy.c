/*
 * NeurOne Hub Control Program — what the detector is allowed to replace. See np_ppx_policy.h.
 */
#include "np_ppx_policy.h"

static np_ppx_evidence_t s_evidence = NP_PPX_EVIDENCE_IN_FORCE;
static np_ppx_config_t   s_config   = { .stop_on_occipital_only = true, .manual_override = false };

void np_ppx_policy_init(np_ppx_evidence_t evidence, np_ppx_config_t config)
{
    s_evidence = evidence;
    s_config = config;
}

void              np_ppx_policy_set_config(np_ppx_config_t config) { s_config = config; }
np_ppx_config_t   np_ppx_policy_config(void)   { return s_config; }
np_ppx_evidence_t np_ppx_policy_evidence(void) { return s_evidence; }

bool np_ppx_zone_permitted(void)
{
    switch (s_evidence) {
    case NP_PPX_EVIDENCE_CLINICAL: return true;
    case NP_PPX_EVIDENCE_BENCH:    return s_config.manual_override;
    case NP_PPX_EVIDENCE_NONE:
    default:                       return false;     /* fail closed on anything unknown too */
    }
}
