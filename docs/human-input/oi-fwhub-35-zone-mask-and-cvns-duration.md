# OI-FWHUB-35: two commanded fields the hub reshapes, with no stated range to refuse at

**Status:** ANSWERED 2026-11-11 by the principal for Q1 (the reserved bits) and Q2 (the block length); still open: what the twelve mask bits mean per eye (findings below, DRAFT), and the unused `ramp_s` and `baseline_req`. *Was:* DRAFT, not sent. **From:** a Claude Code session working for the principal. **To:** FW Hub + Software (Q1);
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

## Decisions, 2026-11-11 (the principal)

Recorded in `NP-FW-HUB-001` (`REQ-FWHUB-78` (k), (l); `OI-FWHUB-35`) and `docs/status/completed-decisions.md`.

- **A general rule, verbatim:** "A max length for a specific protocol should be specified in the protocol
  definition - not hardcoded. Max session should only be hardcoded for a health / safety reason - and it should be
  documented where that reason came from."
- **Q2, the cervical block, verbatim:** "for the cervical block: move the limit to the protocol definition and the
  hardcode be retired with a risk-file entry". Built: a cervical block runs for the length the signed protocol
  authors (the command's duration, or the rest of the session); `NP_CVNS_SESSION_MIN_S` and `NP_CVNS_SESSION_MAX_S`
  are removed; `REQ-CVNS-04`'s session part is retired in place; the risk entry is `NP-RISK-002` hazard 25-f, **not
  scored and not accepted**, its evaluation owed by Quality and Safety (`OI-RISK2-11`); `NP-REG-CVNS-001` §3.2
  carries a note that its session-duration row no longer describes a hardware bound. No wire field was added:
  the descriptor already carried the length. It carries no separate maximum; whether the language needs one is
  open, and is the core's.
- **Q1, the goggles, verbatim:** "for the goggles: accept the recommendation". The recommendation was to refuse a
  descriptor that sets bits with no zone, and to document the 12 bits. Built: any of the upper four bits of
  `zone_mask_hi` set is refused, at load and at dispatch; the field is documented as a 12-bit mask with four
  reserved bits. **Not decided:** what the twelve bits mean per eye (next section).
- **The two remaining cervical refusals, verbatim:** "keep the geometric cervical refusals as structural checks.
  Error messages should be clear when either of these is violated." Built: a block too short to hold its ramp-up
  and ramp-down, and a block longer than the library's 16-bit seconds field can carry, are refused at upload with a
  status of its own each (reasons 41 and 42) and a message of its own each (`cervical_block_too_short`,
  `cervical_block_too_long`). They are structural checks, not limits: the library cannot represent the block.
  The messages name no number, because the apps' refusal messages take no parameter.
- **Still open from Q2:** whether a descriptor whose `ramp_s` or `baseline_req` differ from what the driver does
  should be refused.

## What the 12 bits mean: findings 2026-11-11

**Status: DRAFT. The agent's reading of the design documents and the code, asked for by the principal ("I don't
know what the 12 bits mean. Have the agent examine relevant design documents and the code and present what it
thinks those bits mean."). Not a decision, and not a requirement.**

**What each source says.** "Stated" is written in the source; "implied by code" is what the code does; "my
inference" is neither.

| # | Source | What it says | Kind |
|---|---|---|---|
| 1 | `CLAUDE.md` §3 item 8; `docs/reference/modality-stack.md` line 90; `docs/np_dt_001.md` `DI-PERF-16` (line 142); `docs/np_hw_fitover_001.md` lines 114 and 131; `docs/np_fw_hub_001.md` §8.6 | 108 micro-LEDs per lens, **6 zones per eye** | stated |
| 2 | `firmware/hub_control/include/np_hub_types.h`, `np_mod_visual_params_t`, as it read before this change | `zone_mask_lo`: "zones 0–7 (lo byte of **12-bit per-eye** zone mask)"; `zone_mask_hi`: "zones 8–11 (hi nibble)". So twelve zones, numbered 0 to 11, called per-eye | stated, and it disagrees with row 1: twelve zones per eye is not six |
| 3 | `firmware/hub_control/modules/np_mod_visual.c` (`np_mod_visual_control`) | Builds one 12-bit number and passes it to `np_mod_visual_hal_led_set(zone_mask, wl, freq, duty)`. There is no eye argument and no second mask | implied by code: one mask serves both lenses, either shared or split |
| 4 | `firmware/platform/include/np_sw02_platform_hal.h` line 292; `firmware/platform/src/np_platform_stub.c` line 594 | The HAL takes a `uint16_t zone_mask`. The device implementation is a stub that ignores it. No LED driver, bus address or per-lens register is named anywhere | implied by code: the mapping to hardware is not written |
| 5 | `np_mod_visual.c` and the HAL: EMDR is `np_mod_visual_hal_emdr_set(rate)` | Left/right alternation is a separate call that takes only a rate. The mask is not how the eyes are alternated | implied by code: the mask does not need an eye half to do EMDR |
| 6 | `common/npps-core/src/compiler.rs`, `encode_visual` | Always writes `0xFF, 0x0F`: all twelve bits set. `common/npps/fields.json` gives `visual_stimulation` no zone or eye field, so a protocol cannot choose zones | implied by code: "everything on" needs all twelve bits, in the core's view |
| 7 | `simulator/js/safety.js` line 238; `simulator/server/fw-bridge.test.js` lines 164 to 174 and 520; `firmware/platform_sim/tests/np_sim_visual_tests.c` (`vis 0 2 50 0 63 0 …`) | The simulator's scenarios drive the goggles with `zone_lo` = 63 (`0b111111`) and `zone_hi` = 0: six bits, as if six bits were all the zones. One retinal case uses 7 | implied by code, and it disagrees with row 6: six bits for "all", not twelve |
| 8 | `firmware/platform_sim/src/np_sim_visual.c` | The simulated goggles store the number and call themselves emitting if it is not zero. They model no zones and no lenses | implied by code: no model to learn from |
| 9 | `docs/np_tool_lens_001.md`, `docs/np_art_001.md` (A10, lens rim and goggle arm) | No zone layout, LED grouping or driver was found in the lens tooling document or the artifact index | not found |

**Where the sources agree and disagree.**
- They agree that a lens has six zones (row 1) and that the helmet has two lenses.
- Row 2's old comment says "per-eye" and numbers twelve zones. It cannot be both true that an eye has six zones
  and that a per-eye mask has twelve. One of the two words in that comment is wrong.
- Rows 6 and 7 disagree about how many bits mean "all zones": the core uses twelve, the simulator's scenarios six.
  Both run without error because nothing below the driver gives the bits a meaning.
- No source gives a bit order, says which bit is which zone, or says which eye a bit belongs to.

**My reading.** The twelve bits are most plausibly **six zones for each of two eyes**: 6 × 2 = 12 is the only
arithmetic that fits both the stated six zones per eye and a 12-bit field, and the core sets all twelve to light
everything. On that reading the old comment's "per-eye" was wrong and "12-bit" was right. **This is my inference;
no source states it.** The other reading that fits some of the evidence is a six-bit mask shared by both eyes,
with bits 6 to 11 unused, which is what the simulator's scenarios look like; on that reading "12-bit" was wrong.

**How sure:** not very. Roughly two to one for six-per-eye-times-two over a shared six-bit mask, on the arithmetic
and the core's `0x0FFF`. On which six bits are the left eye, and which bit is which zone within a lens: no
evidence at all. I would not build on either.

**What would settle it.** Any one of:
- the goggles' electrical design: the LED driver part, how its channels are wired to the six zones of each lens,
  and whether the two lenses share channels (no such document was found);
- the real `np_mod_visual_hal_led_set()` when it is written, which has to map bits to driver channels;
- a statement from whoever defined `np_mod_visual_params_t`, since the repository's history is squashed and the
  origin of the comment cannot be traced.
Until then the reserved-bit refusal is safe under both readings: it only refuses bits 12 to 15.

**Consequence if the shared-six-bit reading is the right one:** the core's `0x0FFF` sets six bits that mean
nothing, and the same rule that now refuses the upper nibble would then refuse those too, which would refuse
every protocol the core signs. That is a reason to settle the meaning before narrowing the mask further, and it
is why this change stops at the twelve bits the definition names.

## Background found 2026-11-11 (lead)

These are the lead's findings, recorded as given. They are not decisions.

**(a) The goggles.**
- The core's visual encoder (`common/npps-core/src/compiler.rs`, `encode_visual`) always writes `0xFF` and `0x0F`
  for the zone mask, so a core-signed protocol never sets the upper nibble of `zone_mask_hi`.
- `np_mod_visual.c` masks it off with `& 0x0F`, so the upper four bits are dropped only for a descriptor not made
  by the core.
- `np_hub_types.h` describes the field as "zones 8–11 (hi nibble)" and does not say the upper four bits are
  reserved.
- The header comment says "12-bit per-eye zone mask" while `CLAUDE.md` §3 says 6 zones per eye. Whether 12 bits is
  both eyes together or something else is not stated, and is itself a question.
- No file says why the bits are masked rather than refused.

**(b) Cervical VNS.**
- `NP_CVNS_SESSION_MAX_S` is "2 min gammaCore protocol" in `np_cvns_config.h`. Its only basis in the repository is
  the gammaCore equivalence argument.
- The driver sets every cervical command's duration to this maximum (`np_mod_cvns.c`), so a longer authored block
  is cut silently, which is the silent cut the principal has ruled out.
- From search results, not checked against the documents (a web search on 2026-11-11; the documents were not
  read): the gammaCore labelling uses 2-minute stimulations repeated (three consecutive for an acute cluster
  headache; two treatments a day of three stimulations in the US labelling; two stimulations per treatment in the
  Canadian labelling). On that reading a 120 s maximum on one block matches one labelled stimulation and does not
  bound the exposure of a treatment or a day.
- From search results, not checked against the documents: no source found in that search gives a safety study for
  a 120 s duration.

## What to send back

For each question: the option, a short rationale (`CLAUDE.md` §18 wants the source of any new limit), what would
reopen it, and the reviewer's name, role and date. The answer is recorded in `NP-FW-HUB-001` `OI-FWHUB-35` and in
`docs/status/completed-decisions.md` before any code changes.
