/* Document: NP-FW-EMMC-002 Rev A §E.4 */
/*
 * NeurOne EDF+ Privacy Header — Anonymization-Pipeline File Gate
 * Document: NP-FW-EMMC-002 Rev A §E.4
 *
 * §E.4 requires that, before any EDF+ file is included in a research extract,
 * a privacy-header validator runs over it.  A file whose header violates the
 * §E.2 policy is EXCLUDED from the extract and an SHDR fault event is logged
 * (EDF_PRIVACY_HEADER_VIOLATION).  The anonymization task is NOT aborted — only
 * the offending file is dropped; processing continues with the remaining files.
 *
 * This is deliberately distinct from the atomic anon pipeline's `validate`
 * callback (np_anon_pipeline.h), which is an EXTRACT-LEVEL, all-or-nothing check
 * (k-anonymity / l-diversity / DP thresholds) and DOES abort the whole run on
 * failure.  Routing the per-file EDF header check through that callback would be
 * wrong: a single bad header would kill the entire extract instead of excluding
 * one file.  This gate is the per-file staging filter §E.4 actually calls for.
 *
 * Decoupling: the gate never calls the SHDR logger itself — it stays fully
 * host-testable and independent of hub_control.  The SHDR fault is delivered
 * through an injected callback, which the anon-pipeline integration site wires
 * to np_log_shdr_fault(NP_EDF_SHDR_EV_PRIVACY_HEADER_VIOLATION, ...).  This
 * mirrors how the anon pipeline injects all of its transform stages.
 */

#ifndef NP_EDF_ANON_GATE_H
#define NP_EDF_ANON_GATE_H

#include <stdint.h>
#include <stdbool.h>
#include "np_edf_types.h"

/* SHDR fault callback, invoked exactly once per EXCLUDED file.
 *   file_index: the file's ordinal position in the candidate array.  This is an
 *               opaque index into the current extract's file list — NOT a
 *               timestamp, session id, or user identifier — so it is SHDR-safe.
 *   ev_code:    always NP_EDF_SHDR_EV_PRIVACY_HEADER_VIOLATION.
 *   user:       the opaque pointer passed to np_edf_anon_gate_filter.
 * The callback MUST NOT derive or log any UHDR/user-biology content: the whole
 * point of §E.4 is that a header carrying PII is excluded, not recorded. */
typedef void (*np_edf_shdr_fault_cb)(uint32_t file_index,
                                     uint8_t  ev_code,
                                     void    *user);

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
 *   fault_cb  optional (may be NULL).  Invoked once per excluded file with its
 *             index and NP_EDF_SHDR_EV_PRIVACY_HEADER_VIOLATION.
 *   user      opaque pointer forwarded to fault_cb.
 *   result    optional (may be NULL).  Receives the included / excluded tally.
 *
 * Behaviour (§E.4):
 *   - Each header is checked with np_edf_validate_privacy_header().
 *   - A failing header is EXCLUDED (included[i] = false) and fault_cb fires; the
 *     pass CONTINUES with the remaining files — a violation never aborts.
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
                                        np_edf_shdr_fault_cb   fault_cb,
                                        void                  *user,
                                        np_edf_gate_result_t  *result);

#endif /* NP_EDF_ANON_GATE_H */
