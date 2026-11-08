/*
 * NeurOne Hub Control Program — what the detector is allowed to replace (REQ-FWHUB-70, REQ-FWHUB-71)
 *
 * The refusal of REQ-FWHUB-65 / -66 (photic driving at 3 to 30 Hz) stays until the detector has earned the right to stand in
 * for it, and what it has earned is its EVIDENCE LEVEL:
 *
 *   NONE      no validation. The refusal stands, whatever the clinician's switches say, because nothing guards the drive.
 *   BENCH     validated against recorded photoparoxysmal EEG and/or synthetic signals. The refusal stands, and is lifted only
 *             where a clinician has set the MANUAL OVERRIDE in configuration (then the detector is the guard).
 *   CLINICAL  a clinical study supports it. The refusal is replaced outright.
 *
 * The level the hub ships is NP_PPX_EVIDENCE_IN_FORCE, and it is NONE: nothing has been validated, so this module changes
 * nothing today. Raising it is a deliberate edit that cites the validation it rests on.
 */
#ifndef NP_PPX_POLICY_H
#define NP_PPX_POLICY_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    NP_PPX_EVIDENCE_NONE     = 0,
    NP_PPX_EVIDENCE_BENCH    = 1,
    NP_PPX_EVIDENCE_CLINICAL = 2
} np_ppx_evidence_t;

/* The evidence the shipped detector has. NONE: no validation exists (OI-FWHUB-32). */
#define NP_PPX_EVIDENCE_IN_FORCE  NP_PPX_EVIDENCE_NONE

typedef struct {
    bool stop_on_occipital_only;   /* the clinician's switch: halt on an occipital-only discharge too; ON by default */
    bool manual_override;          /* the clinician's override: with bench evidence only, lift the refusal; OFF by default */
    uint8_t sensitivity;           /* 0 the validated setting, up to NP_PPX_SENS_MAX more sensitive; never below 0 (REQ-FWHUB-68) */
} np_ppx_config_t;

/* Bring-up (and tests): the evidence level and the clinician's configuration. Without a call: NONE, and the defaults above. */
void            np_ppx_policy_init(np_ppx_evidence_t evidence, np_ppx_config_t config);
void            np_ppx_policy_set_config(np_ppx_config_t config);
np_ppx_config_t np_ppx_policy_config(void);
np_ppx_evidence_t np_ppx_policy_evidence(void);

/* May photic driving in the risk zone be admitted at all, with the detector as its guard? */
bool np_ppx_zone_permitted(void);

#endif /* NP_PPX_POLICY_H */
