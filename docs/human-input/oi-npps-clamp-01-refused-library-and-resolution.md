# OI-NPPS-CLAMP-01: the protocols that no longer compile, and what the compiler still rounds

**Status:** DRAFT, not sent
**From:** App + Firmware engineering
**To:** the principal (questions 1 to 3). No clinical or regulatory addressee is named; question 1 may need one.
**Open item:** `OI-NPPS-CLAMP-01` in `docs/status/pending-decisions.md`; `NP-NPPS-REF-001` `REQ-NPPS-33`.

Nothing in the repository acts on an answer to any question here. No protocol file, limit value, range or wire
field was changed to make a protocol compile.

## What is already decided and done

The principal's rule (2026-11-11): no silent clamping anywhere, and PBM duty is refused above 25 % for now
(interim, `OI-HEXTILE-31`). The hub was changed first (`NP-FW-HUB-001` `REQ-FWHUB-78`). The shared compiler
(`common/npps-core`) now refuses every value it used to clamp, wrap or cut (`REQ-NPPS-33`).

## Question 1: nine shipped protocols state continuous wave and are refused

Continuous wave is 100 % duty. Until this change the compiler wrote a CW block with the duty register at `0x32`
(25 %) and frequency code 0, and signed it. It now refuses a duty above 25 %, as the validator already did, so
these do not compile. Each was compiled before and after the change with the same inputs.

| File | Protocol | Block and value refused | Before |
|---|---|---|---|
| `07-vascular-baseline.npps` | Vascular Baseline | `pbm_transcranial`, `frequency: 0`, duty 100 % | compiled, duty written as 25 % |
| `clinical-04-pbm-depression-cassano.npps` | PBM — Depression (Cassano 2018, low-irradiance CW) | `pbm_transcranial`, CW, duty 100 % | compiled, duty written as 25 % |
| `clinical-04-pbm-depression-schiffer.npps` | PBM — Depression (Schiffer 2009, high-irradiance) | `pbm_transcranial`, CW, duty 100 % | compiled, duty written as 25 % |
| `clinical-05-pbm-anxiety-maiello.npps` | PBM — Anxiety (Maiello 2019, low-irradiance CW) | `pbm_transcranial`, CW, duty 100 % | compiled, duty written as 25 % |
| `clinical-05-pbm-anxiety-wang.npps` | PBM — Anxiety (Wang 2023, high-irradiance) | `pbm_transcranial`, CW, duty 100 % | compiled, duty written as 25 % |
| `clinical-06-pbm-tbi.npps` | PBM — TBI (chronic) + Intranasal (Naeser 2011/2014) | `pbm_transcranial` (and `pbm_intranasal`), CW, duty 100 % | compiled, duty written as 25 % |
| `clinical-09-pbm-stroke-rehab.npps` | PBM — Stroke (chronic rehab) | `pbm_transcranial` (and `pbm_intranasal`), CW, duty 100 % | compiled, duty written as 25 % |
| `04-memory-boost.npps` | Memory Boost | CW, duty 100 % (and 250 mW/cm² on the 1064 nm channel, which delivers 28) | already refused, for the irradiance |
| `clinical-03-pbm-alzheimers-1064.npps` | PBM — Alzheimer's 1064nm (deep-cortical channel) | CW, duty 100 % (and 100 mW/cm² on the 1064 nm channel) | already refused, for the irradiance |

The composite `Full Multi-Modal RCT` layers one of these and is refused with it; it was already refused for two
layers driving transcranial PBM at once. `clinical-07-pbm-autism.npps` was and is refused for its wavelength
(850 nm) and is not part of this question. The other 58 shipped protocols compile to the same bytes as before.

**How the nine were found:** every `.npps` protocol in `protocols/predefined/` compiled through the web entry
with the zone namespace, before the change and after, and the two results compared.

**The question: what should happen to the nine until `OI-HEXTILE-31` lifts the 25 % limit?**

- **A. Leave them as they are, refused.** They state what their sources ran. Costs: nine entries in the shipped
  library cannot be run, and an app that lists them must say so.
- **B. Withdraw them from the shipped set** (the manifest) until they can run. Costs: a library change, and the
  protocols come back later.
- **C. Re-author them as pulsed at 25 %.** Not recommended: `OI-HEXTILE-31` records that Schiffer and Wang were
  once converted this way and says each protocol's mode returns to its source's. It is the reshaping the rule
  forbids, done by hand.

Known: the validator has refused all nine since 2026-10-04 (`OI-SESPWR-03`), so an app that validates before it
compiles already did not run them. Assumed: no build shipped that compiles without validating.

## Question 2: is rounding to the wire's step acceptable, or is it reshaping?

The descriptor carries PBM and visual frequency as whole Hz in one byte. A frequency of 0.5 Hz, the smallest the
validator accepts, is written as 1 Hz; 7.83 Hz as 8 Hz. Irradiance, duty, current and level are likewise rounded
to their register step (1.58 mW/cm² at 403 mW/cm² full scale, 0.5 %, 1 µA, 0.5 dB). The compiler now refuses a
value that would be written as 0, and nothing else: a rounded value is still signed with no word.

- **A. Accept rounding to the nearest step and document the step per field** (done in `NP-NPPS-REF-001` §4 for
  the fields changed here). Cost: a 0.5 Hz PBM block runs at 1 Hz.
- **B. Refuse a PBM or visual frequency that is not a whole number of Hz.** Cost: the validator's 0.5 Hz minimum
  becomes 1 Hz in effect. No shipped protocol states a fractional PBM or visual frequency.
- **C. Widen the field** (as VNS frequency was widened to 32-bit mHz). Cost: a wire-format change on both ends
  and in the tile firmware's frequency code.

## Question 3: an audio beat the validator accepts and the wire cannot carry

The validator's range for a binaural beat or isochronic tone ends at 100 Hz (`AUDIO_BEAT_MAX_HZ`, no derivation,
`UC-082`). The descriptor field is 16 bits of mHz, 65.535 Hz. The compiler used to write 65.535 Hz for anything
above it and now refuses. So 66 to 100 Hz validates and then does not compile.

- **A. Lower the validator's maximum to 65.535 Hz** (a range change in `NP-NPPS-REF-001` §4.7).
- **B. Widen `beat_mhz` to 32 bits** (a wire-format change, as for VNS).
- **C. Leave it:** the compiler's refusal names the field and the range.

## Not a question: the core does not refuse the two cervical block lengths (OI-NPPS-CLAMP-01 (h))

Recorded 2026-11-12 at the principal's request; it needs no ruling now.

The hub refuses at upload a cervical VNS block that is no longer than the library's ramp-up plus ramp-down (10 s + 5 s) and a block over 65,535 whole seconds (hub reasons 41 and 42; `NP-FW-HUB-001` `REQ-FWHUB-78` (k)). They are structural checks, not limits. The core only names those refusals after the hub makes them, so a descriptor the core signs can still be refused at upload for either length. Closing the gap means the compiler refuses both by name before signing, which needs the cervical library's ramp lengths (carried as shared constants or read from one source). Not built; the full text is `docs/status/pending-decisions.md` `OI-NPPS-CLAMP-01` (h).

## What I need back

For each question: the option, a sentence of rationale (CLAUDE.md §18), any condition that would reopen it, and
your name, role and the date. For question 1, whether a clinical owner should confirm the list.
