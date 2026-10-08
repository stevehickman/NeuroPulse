# Safety zones — safe, caution, danger

**Status:** Process record, not a controlled document. **Added:** 2026-10-07 (principal's request). **Code:** `common/npps-core/src/zones.rs`
**Boundaries:** `common/npps/constants.json`, group `SafetyZones` (every figure a **PLACEHOLDER**, register rows `UC-073` to `UC-077`)

## What it is

A protocol's electrical dose is reduced to a few **axes**, each a quantity with a damage or heating mechanism behind it. An
axis has a **caution** boundary and sometimes a **danger** boundary. **A block's zone is its worst axis; a protocol's zone is
its worst block.**

| Zone | What happens |
|---|---|
| **Safe** | Runs without a word. |
| **Caution** | The validator reports a warning carrying an acknowledgement id. The compiler refuses the protocol unless the request lists that id in `acknowledgedCautions`. |
| **Danger** | The validator reports an error and the compiler refuses the protocol, whatever is acknowledged. |

The id is `<block>:<modality>:<axis>=<value>`, so it names the dose: change the dose and the old acknowledgement stops
matching. An acknowledgement is of one dose, not of the protocol.

**Where it is enforced.** The compiler is the one gate every app shares, so the zone is enforced there (`compile_json`, the
`acknowledgedCautions` request field).

**The acknowledgement screen** (2026-10-07) is one per app with a launch path: iOS `CautionAcknowledgementView`, Android
`CautionAcknowledgementDialog`, web `CautionAckDialog`. Each lists a protocol's cautions (read from the validator's
`zones`, `common/lib/zoneCautions.ts` · `NPZoneCaution`) with **one switch per caution**, because an id names one dose.
Confirm is disabled until every switch is on; cancel, and swiping or tapping the sheet away, acknowledge nothing. The ids go
to the compiler for **that one run and are dropped**: nothing is stored, logged or sent, because an acknowledgement is a
decision about the person (`CLAUDE.md` §5). iOS and Android show it when the user selects a protocol and pass the ids to
the upload; web has no launch path, so its dialog is built and tested but not yet mounted; Windows has no UI, so its
compiler takes `acknowledgedCautions` and nothing more. watchOS authors and uploads nothing. **Either the user or a
clinician may acknowledge** (principal's decision, 2026-10-07): the screen is the same for whoever is operating the
device, and because nothing is recorded it does not say which. **No separate acknowledgement record is kept** (principal's
decision, 2026-10-08): that the protocol ran indicates it was acknowledged, because the compiler refuses a caution-zone
protocol without the ids. A separate clinician-held acknowledgement is not built (`OI-ZONE-01`). **The safety MCU still enforces its own ceilings and the app-side zone never
replaces them** (`CLAUDE.md` §4.2): a caution boundary is always below a boundary the MCU enforces, and a danger boundary is
either the MCU's own number or an app-side refusal that only makes the app stricter.

## The axes

For a charge-balanced block with peak current `I`, geometric pad area `A`, and on-fraction `δ` (from `interval_on` and
`interval_off`; 1 when the block runs throughout):

| Axis | Applies to | Value | Danger | Caution |
|---|---|---|---|---|
| Charge density per phase `D` | BES/tACS, clinical tACS, VNS, cervical VNS | `Q / A`, with `Q` = `I·w` (rectangular phase `w`) or `I/(π·f)` (sinusoid) | **40 µC/cm²** (the existing ceiling) | 10 µC/cm² |
| Shannon `k` | rectangular phases of 1 ms or less | `log₁₀ D + log₁₀ Q` | none | 1.0 |
| RMS current density | the same | `I_rms / A`; `I·√(2·w·f·δ)` for a pulse train, `I/√2·√δ` for a sinusoid | 3.0 mA/cm² | 1.0 mA/cm² |
| Mean current density | the same | `mean-rectified current / A`; `2·I·w·f·δ` for a pulse train, `2I/π·δ` for a sinusoid | 300 µA/cm² | 100 µA/cm² |
| Session charge density | tDCS | `I·t / A` | **150 mC/cm²** (the existing ceiling) | 100 mC/cm² |

**Where frequency enters.** The per-phase charge limit has no frequency in it; the literature says frequency, duty and
current density are co-factors in damage that the Shannon line does not capture (Cogan 2016, citing McCreery 1995 and 1997),
without giving a number. So frequency and duty enter **through the two current-density axes**, where pulse rate, pulse width,
amplitude and the on-fraction multiply into one heating quantity. There is no axis on frequency alone: the 1–25 Hz range is a
separate, still-open question (`OI-VNSCLIP-08`).

**Why Shannon `k` is limited to short rectangular phases.** The equation was fitted to 0.4 ms pulses on cortical-surface
electrodes of 0.01–0.5 cm². For a 50 ms sinusoid phase on a 25 cm² pad it would flag almost every tACS protocol on a
quantity it was never fitted to.

## What the placeholders do to the shipped library

66 of the 68 shipped single protocols are safe, 2 are in caution and none is in danger (2026-10-07):
`ADHD Focus` (tDCS, 102.9 mC/cm² a session) and `tACS — Sleep / Memory Consolidation (SO 0.75Hz)` (12.7 µC/cm² a phase).
The web library test names both with their acknowledgement ids, so a new caution is a deliberate act.

## What it does not do

* **It derives nothing.** Every boundary except the two existing hardware ceilings is a placeholder, held in one file so it
  can move without a code change. A placeholder is moved freely in either direction until a derivation, a measurement or a
  standard replaces it.
* **It is an app-side bound beside a 40 mA ceiling.** The flat 2 mA auricular ceiling was removed on the principal's
  instruction (Rev 67, 2026-10-08) and replaced by a 40 mA stop-gap (Rev 68), the strictest current the 40 µC/cm² per-phase
  charge limit permits at any allowed pulse width. This model's current-density danger boundaries (app-side, advisory to the
  safety MCU, placeholders) put the danger at about 6 mA at 500 µs and 25 Hz, so they bind long before 40 mA. Whether any
  ceiling is needed: GitHub #554 and #559.
* **It does not know the pad.** Every density divides by the fixed pad areas, which are provisional and unmeasured (`UC-006`).
  A smaller real pad raises every density in proportion.
* **It does not reach the cervical interlock, PBM, TMS or the optical and visual modalities.** They have their own controls.
