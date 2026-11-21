/* Document: NP-FW-EMMC-002 Rev 1 §E.4 */
/*
 * NeurOne EDF+ Privacy Header — Anonymization-Pipeline File Gate
 * Document: NP-FW-EMMC-002 Rev 1 §E.4
 *
 * §E.4 requires that, before any EDF+ file is included in a research extract,
 * a privacy-header validator runs over it.  A file whose header violates the
 * §E.2 policy is EXCLUDED from the extract.  The anonymization task is NOT
 * aborted — only the offending file is dropped; processing continues with the
 * remaining files.
 *
 * NOTHING ABOUT THE PASS REACHES SHDR (OI-PRIV-38 (NP-PRIV-ANALYSIS-004) part 2,
 * REQ-SW02-02 (b)).  Until 2026-11-21 the gate fired an injected callback once
 * per excluded file, with the file's index, for the integration site to write
 * as an SHDR fault (0xE1).  Any such record says that a research extract ran on
 * this device and roughly how large it was, which is study participation under
 * the device-linked SHDR token (CLAUDE.md §5.1, OI-PRIV-07), and the per-file
 * index adds the extract's size.  The callback is removed rather than left for a
 * caller to wire.  The tally in np_edf_gate_result_t is the caller's, and stays
 * on the UHDR side with the extract, as the pipeline's failed_step does
 * (data-architecture-detail.md §5.1).
 *
 * This is deliberately distinct from the atomic anon pipeline's `validate`
 * callback (np_anon_pipeline.h), which is an EXTRACT-LEVEL, all-or-nothing check
 * (k-anonymity / l-diversity / DP thresholds) and DOES abort the whole run on
 * failure.  Routing the per-file EDF header check through that callback would be
 * wrong: a single bad header would kill the entire extract instead of excluding
 * one file.  This gate is the per-file staging filter §E.4 actually calls for.
 */

#ifndef NP_EDF_ANON_GATE_H
#define NP_EDF_ANON_GATE_H

#include <stdint.h>
#include <stdbool.h>
#include "np_edf_types.h"

/* Outcome tally for one gate pass. */
typedef struct {
    uint32_t included;  /* files that passed the §E.2 privacy policy */
    uint32_t excluded;  /* files rejected on a header violation      */
} np_edf_gate_result_t;

/*
 * np_edf_anon_gate_filter — run the §E.4 per-file privacy-header gate.
 *
 *   headers   array of `count` candidate EDF+ headers (read-only).
 *   count     number of candidates (0 is a valid no-op).
 *   included  caller-owned bool array of length `count`.  On return,
 *             included[i] == true iff headers[i] passed the §E.2 policy and may
 *             be emitted into the extract; false means the file is excluded.
 *   result    optional (may be NULL).  Receives the included / excluded tally.
 *
 * Behaviour (§E.4):
 *   - Each header is checked with np_edf_validate_privacy_header().
 *   - A failing header is EXCLUDED (included[i] = false); the pass CONTINUES
 *     with the remaining files — a violation never aborts.
 *   - Nothing is written to, or handed out for, SHDR.
 *   - Only files with included[i] == true are eligible for the extract.
 *
 * Returns:
 *   NP_EDF_OK          on a completed pass, EVEN WHEN files were excluded (a
 *                      per-file exclusion is not a run failure — this is the
 *                      "do not abort" invariant, and is asserted by the tests).
 *   NP_EDF_ERR_INVALID_ARG if count > 0 and headers or included is NULL.
 */
np_edf_status_t np_edf_anon_gate_filter(const np_edf_header_t *headers,
                                        uint32_t               count,
                                        bool                  *included,
                                        np_edf_gate_result_t  *result);

#endif /* NP_EDF_ANON_GATE_H */
