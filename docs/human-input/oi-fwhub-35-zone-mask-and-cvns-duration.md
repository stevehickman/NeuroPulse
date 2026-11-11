# OI-FWHUB-35: two commanded fields the hub reshapes, with no stated range to refuse at

**Status:** DRAFT, not sent. **From:** a Claude Code session working for the principal. **To:** FW Hub + Software (Q1);
Clinical with FW Hub (Q2). No addressee is named.

The principal's rule (2026-11-11): no silent clamping anywhere. A value outside a limit is refused, never reshaped
(`CLAUDE.md` §3; `NP-FW-HUB-001` `REQ-FWHUB-78`). Two places in the hub still change what a signed command says.
In both, **no document states the range a refusal would enforce**, and `CLAUDE.md` §18 does not allow a limit that
nothing requires. So neither was changed. Each needs an answer.

## Q1. Goggles: what must the upper four bits of `zone_mask_hi` be?

`np_mod_visual_params_t` (`firmware/hub_control/include/np_hub_types.h`) carries a zone mask in two bytes:
`zone_mask_lo` ("zones 0–7, lo byte of 12-bit per-eye zone mask") and `zone_mask_hi` ("zones 8–11 (hi nibble)").
`np_mod_visual_control()` builds the mask as `zone_mask_lo | ((zone_mask_hi & 0x0F) << 8)`, so bits 4–7 of
`zone_mask_hi` are dropped without a word.

What is known:
- The shared core always writes `0xFF, 0x0F` (`encode_visual`, `common/npps-core/src/compiler.rs`), so no protocol
  it signs has those bits set.
- The struct comment gives a 12-bit mask. It does not say the upper four bits are reserved or must be zero.
  (`np_mod_clin_tacs_params_t.channel_mask_ext` does say so: "bits 5–7 reserved, must be 0".)
- `CLAUDE.md` §3 gives 6 zones per eye. How 12 mask bits map to them was not looked up.

| Option | What it means | Cost |
|---|---|---|
| **A. Reserved, must be 0; refuse otherwise** | The wire definition and `NP-FW-HUB-001` §4.6 say so, the driver and the load-time check refuse a non-zero upper nibble (`REQ-FWHUB-78`) | One comment, one line in `np_cmd_limits_check()`, tests. A descriptor from another producer that sets them is refused |
| **B. Ignored by definition** | The definition says the bits are don't-care. The mask stays | Nothing changes; a producer that sets them learns nothing |
| **C. They mean something** (a second eye, a future zone) | The definition says what | The driver must then honour them |

**Leaning: A.** It matches how the other reserved bits on this wire are defined, and it is the reading under which
nothing is reshaped. It is not acted on.

## Q2. Cervical VNS: is there a longest block, and what happens to the fields the driver does not use?

`np_mod_cvns_build_config()` (`firmware/hub_control/modules/np_mod_cvns.c`) sets the library's `duration_s` to
`NP_CVNS_SESSION_MAX_S` (120 s) for every command. The session runner normally stops the driver at the end of the
block. If the block is longer than the stimulation the library allows, the library ramps down by itself 120 s after
stimulation starts, and the rest of the block delivers nothing. The person is not told. The same function ignores
the descriptor's `ramp_s` (the library ramps at its fixed `NP_CVNS_RAMP_UP_S`) and `baseline_req` (the cardiac
baseline is mandatory).

What is known:
- `NP-FW-CVNS-001` gives `NP_CVNS_SESSION_MAX_S` = 120 and a configuration `duration_s` of 60–120 s, from the
  gammaCore protocol. That is the library's limit on stimulation time.
- The 120 s runs from the start of stimulation, after the impedance and baseline checks, so it is not the block's
  length. A block of exactly 2 minutes is not cut short. A longer one may be.
- No maximum block length for `cervical_vns` was found in `NP-NPPS-REF-001`, the validator or
  `common/npps/constants.json`. The one shipped protocol that uses it
  (`protocols/predefined/clinical-26-tcvns-migraine.npps`) has a 2 minute block.
- The core always writes `ramp_s` = 10 and `baseline_req` = 1, which is what the driver does anyway.

| Option | What it means | Cost |
|---|---|---|
| **A. A stated maximum block length, refused before signing and at upload** | `NP-NPPS-REF-001` gives `cervical_vns` a maximum duration (120 s, or 120 s plus the pre-checks), the validator and `np_cmd_limits_admit()` refuse a longer block | A new limit, which needs its source (the predicate device's 2 minute dose is the obvious one). A longer authored block becomes an error |
| **B. Repeat doses** | A longer block is delivered as 120 s doses with a stated gap | A dosing decision for Clinical, and new driver behaviour |
| **C. Leave it, and say so** | The IFU and the app state that a cervical block delivers at most 120 s | The truncation stays silent at the helmet |

For `ramp_s` and `baseline_req`: should the hub refuse a descriptor whose `ramp_s` is not the library's ramp and
whose `baseline_req` is 0, since it will not do what they say? **Leaning: yes**, for the same reason as Q1, once
the definition says the fields are fixed.

## What to send back

For each question: the option, a short rationale (`CLAUDE.md` §18 wants the source of any new limit), what would
reopen it, and the reviewer's name, role and date. The answer is recorded in `NP-FW-HUB-001` `OI-FWHUB-35` and in
`docs/status/completed-decisions.md` before any code changes.
