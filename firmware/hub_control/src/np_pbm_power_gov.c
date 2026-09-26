/*
 * NeurOne Hub Control Program — Transcranial PBM Power Governor (CLOSED)
 * Document: NP-FW-HUB-001 Rev 2 §5.6; NP-HW-HEXTILE-001 §9.3 (OI-HEXTILE-09)
 *
 * ── Why this refuses everything ─────────────────────────────────────────────
 *
 * NP-HW-HEXTILE-001 §9.3 finding 1: "A global concurrent-power governor is
 * required in firmware. ... The compiler and the session runner both need a
 * power-budget check against the negotiated USB-C PD contract." That is not a
 * hypothetical: NP-SES-PWR-001 ran the per-tile model over the predefined
 * library and found 17 of 20 transcranial protocols over the 40 W emitter
 * budget by 1.25x to 40x; scripts/check-pbm-power.ts, which re-runs it on the
 * live library, read 20 of 23 on 2026-09-23. Every one of them compiles clean.
 *
 * Until OI-FWHUB-01 closed, that gap was latent — no socket-addressed command
 * could reach an emitter. np_socket_dispatch.c removes that barrier, so the
 * governor is now the only thing between those protocols and the rail.
 *
 * It cannot be written yet, and writing a plausible one would be worse than
 * refusing:
 *   - it must be denominated in WATTS against the negotiated PD contract, not
 *     in a tile count (OI-HEXTILE-09) — and there is no PD-contract seam yet;
 *   - its input is undefined for `frequency: 0Hz` combined with `duty_cycle:`,
 *     a 4x swing across a fifth of the library (OI-SESPWR-03, which blocks
 *     OI-HEXTILE-09);
 *   - its per-tile watts come from emitters that are not selected
 *     (OI-HEXTILE-02), so every coefficient would be a design target presented
 *     as a limit.
 *
 * So the dispatch path ships complete and CLOSED BY DEFAULT: this definition
 * admits nothing, and np_sock_disp_command() returns NP_HUB_ERR_POWER_BUDGET,
 * which the runner logs to SHDR and which keeps the command out of UHDR's
 * delivered-modality mask. A transcranial PBM protocol therefore still does not
 * run on this firmware — but the reason is now exactly one named function
 * rather than a missing registry. Replacing this body is OI-FWHUB-09.
 *
 * Stops never come here: np_sock_disp_command() admits a stop before asking.
 *
 * ── Where the governor's numbers come from (OI-PWRSRC-13) ───────────────────
 *
 * np_pbm_model.generated.h carries the per-tile watts, the emitter budget, the
 * non-PBM overhead, the provisional 28 W thermal ceiling and the thermal
 * constants, emitted from hardware/np_pbm_model.json — the same file the
 * TypeScript audits read. scripts/check-pbm-model.ts fails CI if the two
 * disagree. When OI-FWHUB-09 replaces this body it reads those macros and
 * carries no literal of its own; the include is here now so the header is
 * compiled by the ARM cross-build from the day it exists.
 */

#include "np_socket_dispatch.h"
#include "np_pbm_model.generated.h"

bool np_pbm_power_admit(const np_session_cmd_t      *cmd,
                        const np_sock_disp_socket_t *current)
{
    (void)cmd;
    (void)current;
    return false;
}
