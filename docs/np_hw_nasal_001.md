# Intranasal Bilateral Y-Probe and Hygiene Sleeve — Hardware Specification

**Project:** NeurOne
**Document:** NP-HW-NASAL-001
**Revision:** 3
**Date:** 2026-09-25
**Status:** DRAFT — **requirements-grade. No probe geometry, no emitter selection and no mucosal exposure limit exist; §8 names each absence rather than supplying a value.**
**Effective Date:** —
**Author:** NeurOne Systems Engineering
**Approved By:** — (new document)
**References:** CLAUDE.md §3 modality ② (PBM intranasal) and its hard-limits table, §2.3 (consumables — the sleeve is the only authenticated one), §4.2 (safety architecture), §5.1 (UHDR/SHDR); `docs/reference/modality-stack.md` (modality 2 detail); `docs/reference/commercial-model.md` §2.3 (sleeve price, interval, GM%); NP-FW-HUB-001 Rev 1 §8.7 (`np_mod_intranasal.c`); NP-TOOL-HUB-001 Rev 2 F-01 (probe dock, BASELINED; probe-contact geometry GATED on OI-NASAL-09), §7 OI-HTOOL-07; NP-FAI-HUB-001 Rev 2 (FAI-HUB-09, -10, -20); NP-NPPS-REF-001 Rev 18 §4.2 (protocol block — absolute irradiance); NP-ENV-OPRANGE-001 (shared thermal envelope, probe included); NP-ART-001 Rev 3 §2.4 A12, §3.2, §4, OI-ART-04; NP-FAI-001 Rev 3 §2 (F1–F4); NP-RISK-002 Rev 3 §4, OI-RISK2-02; NP-CONV-001 Rev 6 §4, §5, §6; NP-HW-HEXTILE-001 Rev 5 (the tile's dual-PD dose-metering precedent); `firmware/hub_control/modules/np_mod_intranasal.c`
**Related Issues:** GitHub Issue #332 (OI-ART-04 — A12 has no owning document); OI-NASAL-01 (closed Rev 2 — the baselined dock recorded as gated)
**Gate:** NP-COORD-001 G2 (pre-tooling) — A12's entry in the G2 completeness check, not a release of it
**IEC 62304 Class:** N/A (hardware specification). It constrains SW-02 Class B code (`np_mod_intranasal.c`) and sits behind the Class C enable `NP_SAFETY_EN_INTRANASAL` (§5).
**Supersedes:** None — first issue. It removes `CLAUDE.md` §3 modality ② from the role of governing specification for artifact A12.
**Parent Document:** None

---

> **Why this document exists.** `NP-ART-001` §4's finding was that nine of fifteen manufactured
> artifacts have no owning specification. A12 is the one with the most riding on the gap: the probe
> is a **shipping T1 modality**, its hygiene sleeve is **the only authenticated consumable and the
> primary MRR driver** (`CLAUDE.md` §2.3), and `NP-TOOL-HUB-001` F-01 — a **BASELINED** tooling
> specification — already moulds a dock whose saddle radius is *"sized to the Y-junction body OD"*
> for a Y-junction whose OD no document states. **A baselined tool is dimensioned against an
> unspecified part.** `OI-ART-04` asked for the specification; this is it.
>
> **Two findings are new here and both are consequential.**
>
> - **§5.2 — the probe has no exposure ceiling of its own.** `CLAUDE.md` §3's hard-limits table has
>   a row for *PBM scalp* and a row for *PBM deep (T2)*. **There is no intranasal row.** The
>   firmware's duty ceiling is inherited from the cranial tiles (*"same ceiling as zone modules"*),
>   the 42 °C limit is enforced by a per-zone NTC the probe does not have, and the tissue is
>   mucosa, not scalp.
> - **§6.2 — the sleeve is authenticated but not enforceable as single-use.** The authentication
>   interface of record is a **boolean**. A valid sleeve reads valid forever, so nothing in the
>   design distinguishes a fresh sleeve from the same sleeve on its thirtieth session. The
>   consumable the revenue model leans on hardest is the one whose single-use property is
>   unimplementable as specified.

---

## 1. Scope

**In scope, and binding:**

| § | What it pins |
|---|---|
| §2 | The artifact and its sub-parts, including the consumable and the dock interface |
| §3 | The requirements the record already binds |
| §4 | Electrical, optical and firmware interfaces as the shipped driver has them |
| §5 | Safety: the Class C enable, and the exposure-limit gap §5.2 records |
| §6 | The hygiene sleeve — authentication scheme, and the single-use finding |
| §7 | Hazard-analysis inputs for `OI-RISK2-02` |

**Out of scope:** protocol authoring (`NP-NPPS-REF-001` §4.2), the hub PCB's optical drive
electronics (artifact A9, `NP-HW-HUB-001`), the dock *moulding* (artifact A8,
`NP-TOOL-HUB-001` F-01 — this document supplies the **mating requirement**, §2.2, not the
cradle geometry), and clinical dosimetry.

---

## 2. The artifact

**A12 is a hand-placed bilateral optical applicator with a consumable hygienic barrier.** It is not
cranial-tiled, occupies no socket, and is the only NeurOne applicator that enters a body cavity.

| Sub-part | What it is | Status of the record |
|---|---|---|
| **A12.1 — probe tips (×2)** | One per nostril; **660 nm + 808–830 nm per probe** | Wavelengths only. No emitter selected, no optical power, no beam geometry, no tip envelope |
| **A12.2 — Y-junction body** | Bilateral split, **silicone over-moulded for impact protection**; the part the dock exists to protect | Material and purpose named. **No OD**, which `NP-TOOL-HUB-001` F-01 needs (§2.2) |
| **A12.3 — depth-stop rings** | **15 / 20 / 25 mm**, silicone over-moulded, wear-resistant | The one dimensioned family in the whole artifact — three depths, user-selected |
| **A12.4 — sensing** | **Photodiode contact/dose sensing per probe + reference LED at the probe base** | Topology named; no responsivity, no calibration path, and see §5.3 |
| **A12.5 — authentication contacts** | **Optical code + pogo-pin resistive sleeve ID** — explicitly *no NFC, no RF, no EMF* | Scheme named; per-unit identity is undecided (§6.2) |
| **A12.6 — hygiene sleeve (consumable)** | Single-use barrier, 30-pack | Price, interval and margin are stated; **material, fit, barrier property and code format are not** |
| **A12.7 — cable and hub connector** | To the hub accessory port | Nothing selected — the shared connector gap (§8) |

> **`REQ-NASAL-01`** — A12.6 is a **separately manufactured consumable in its own right**, not an
> accessory of A12.1. It has its own supplier, its own incoming inspection and its own failure
> modes, which is `NP-ART-001` §1's own test for an artifact. Its tooling and inspection cannot be
> folded into the probe's.

### 2.1 Why the depth-stop rings are the load-bearing dimension

Three rings at 15 / 20 / 25 mm are a **user-selected insertion depth**, and the selection is
mechanical — there is no sensor that reports which ring is fitted, and no field in
`np_mod_intranasal_params_t` that carries it. So insertion depth is outside the signed protocol,
outside the safety MCU's view, and outside UHDR.

> **`REQ-NASAL-02`** — The depth-stop ring is a **hard stop**, not a guide: it must prevent
> insertion beyond its nominal depth under any force a user can apply by hand, and it must not
> detach inside the nostril. Both are inspection properties, and neither is dimensioned today
> (**`OI-NASAL-05`**).

### 2.2 The dock interface — a baselined tool is waiting on this document

`NP-TOOL-HUB-001` F-01 is **BASELINED** and specifies a cradle with two probe-tip receptacles and a
central Y-junction saddle whose *"radius is sized to the Y-junction body OD (not the thinner
bilateral leads), so the junction bears its own weight in storage rather than the leads."* It
further specifies **Shore 30–40A silicone insert pads** at probe-tip contact *"protecting the
probe-tip PD window and the optical-code/pogo-pin authentication contacts from abrasion"*, and
**≤ 2 N** snap-detent retention.

Every one of those requirements is stated **against features of A12**, and A12 states none of them.

> **`REQ-NASAL-03`** — This document owes `NP-TOOL-HUB-001` F-01 four values before the hub mould is
> cut: the **Y-junction body OD**, the **probe-tip envelope**, the **location and finish of the PD
> window**, and the **location of the authentication contacts**. Until they exist, F-01's saddle
> radius and insert-pad placement are dimensioned against nothing. **`OI-NASAL-01`** — and it is
> the one open item in this document that gates an already-baselined tool rather than a future one.

**Resolution (Rev 2, 2026-09-23) — `OI-NASAL-01` closed by its second branch: F-01 is recorded as
gated.** The item offered two exits: supply the four values, or record F-01's geometry as gated.
**The values cannot be supplied honestly today.** The probe tip's envelope follows from an emitter
package, a PD, a reference LED and a sleeve wall, and none of those is selected (`OI-NASAL-06`,
`OI-NASAL-03`); the Y-junction OD follows from the conductor count and the over-mould wall that
protects them, and no cable is selected (`OI-NASAL-07`). A number written here would be the invented
figure `NP-FAI-001` §2 F2 and `NP-CONV-001` §7.1 exist to keep out — and it would be worse than
none, because the baselined tool would then be cut to it. So the second exit was taken:

| Where | What changed |
|---|---|
| `NP-TOOL-HUB-001` Rev 2 §3 F-01 | Gate note: the four probe-dependent features are relations to a part with no dimensions. No F-01 value changed |
| `NP-TOOL-HUB-001` Rev 2 §4, §5 | HUB-MDR-01, -03 and FAI-HTOOL-01 gated; HUB-MDR-02 gated as to placement only |
| `NP-TOOL-HUB-001` Rev 2 §7 | OI-HTOOL-06 notes it cannot close; **OI-HTOOL-07** raised (below) |
| `NP-FAI-HUB-001` Rev 2 | FAI-HUB-09, -10, -20 carry `[GATED: NP-HW-NASAL-001 OI-NASAL-09]`; §0's claim that the saddle is dimensioned is corrected |

**What this closes, and what it does not.** It closes the defect `OI-NASAL-01` named — a baselined
tool *silently* dimensioned against nothing: every F-01 item that depends on A12 now says so and
names the item that releases it. **It does not release the hub mould.** `OI-HTOOL-06` gates the
steel cut on the §4 checklist being fully signed, and three of those items are now visibly
unsignable. The four values are still owed, and are carried forward as **`OI-NASAL-09`**.

**The way out that does not wait for the probe is on the hub side, not here.** `NP-TOOL-HUB-001`
`OI-HTOOL-07` recommends — without executing — moving every probe-contact surface of the dock into
the silicone inserts F-01 already bonds to the housing, so that the hard mould carries only a pocket
whose envelope the hub allocates. If that is taken, the relationship reverses: this document would
gain a requirement that A12 fits the allocated envelope, traceable to that allocation (the
`NP-CONV-001` §7.1 test answered by *"the probe does not dock"*), and `OI-NASAL-09` would gate two
soft parts instead of a steel cut. **Until it is taken, `REQ-NASAL-03` stands as written**: this
document owes the hub the four values.

---

## 3. Requirements the record already binds

| ID | Requirement | Source |
|---|---|---|
| **REQ-NASAL-04** | Bilateral Y-probe: two independently addressable probes, **660 nm and 808–830 nm on each** | `CLAUDE.md` §3 modality ②; `np_mod_intranasal_params_t` carries `side` ∈ {bilateral, left, right} |
| **REQ-NASAL-05** | Depth-stop rings in **15 / 20 / 25 mm**, silicone over-moulded, wear-resistant | `docs/reference/modality-stack.md` |
| **REQ-NASAL-06** | Photodiode contact **and** dose sensing per probe, plus a **reference LED at the probe base** | `docs/reference/modality-stack.md` |
| **REQ-NASAL-07** | Sleeve authentication is **optical code + pogo-pin resistive**, with **no NFC and no RF** anywhere in the applicator | `CLAUDE.md` §3 modality ②; `CLAUDE.md` §1 wired-first, zero RF at scalp |
| **REQ-NASAL-08** | Silicone over-mould at the Y-junction for impact protection | `docs/reference/modality-stack.md` |
| **REQ-NASAL-09** | The probe stores in the hub dock, and the dock ships from day one in every configuration | `CLAUDE.md` §3 modality ②; `NP-TOOL-HUB-001` F-01 |
| **REQ-NASAL-10** | Session admission requires **both** an authentication pass and a photodiode contact confirmation | `np_mod_intranasal_detect()`; `NP-FW-HUB-001` §8.7 |
| **REQ-NASAL-11** | **Pulsed** duty ceiling **25 %** (`INS_DUTY_MAX` = `0x32`), and the protocol grammar bounds `duty_cycle` 1–25 % on a pulsed block. **CW (`frequency: 0`) is continuous** — 100 % on-time, a `duty_cycle` beside it is a parse error (Rev 3, the same rule as `CLAUDE.md` §3 for the scalp). The cap was always a pulsed-duty cap and never an exposure limit (§5.2 item 3) | `firmware/hub_control/modules/np_mod_intranasal.c`; `NP-NPPS-REF-001` §4.2 |
| **REQ-NASAL-12** | Ambient envelope: full dose ≤ +30 °C, derate +30 → +35 °C, block > +35 °C — **the probe carries the same band as every helmet module** | `NP-ENV-OPRANGE-001` |

---

## 4. Interfaces

| Parameter | Value | Source |
|---|---|---|
| Hub slot | `NP_HUB_SLOT_INTRANASAL` = 9, module type `NP_MOD_INTRANASAL` = `0x03` | `np_hub_config.h`, `np_hub_types.h` |
| Command block | `np_mod_intranasal_params_t` — `side`, `freq_code`, `duty` (≤ `0x32` pulsed; ignored for CW, driven at 100 %), `irr_660`, `irr_808` — **absolute irradiance, mW/cm² at the probe exit face** (Rev 3; was `cur_660`/`cur_808`, raw 8-bit drive codes) | `np_hub_types.h` |
| Irradiance → drive | `np_ins_irr_to_code()`: CUR code = irradiance × 255 / exit-face irradiance at code 255 (`NP_INS_IRR_FS_DMW`). **That constant is 0 — no emitter is selected (`OI-NASAL-06`) — so every non-zero request returns `NP_HUB_ERR_UNCHARACTERISED` and nothing is driven** | `np_emission_cal.h/.c` |
| Detection | `np_mod_ins_hal_auth_check()` **AND** `np_mod_ins_hal_pd_contact()`, both `bool` | `np_mod_intranasal.c` (`OI-INS-01`, `OI-INS-02`) |
| Drive | `np_mod_ins_hal_pwm_set(side, cur_660, cur_808, freq, duty)` | `OI-INS-03` |
| Dose read | `np_mod_ins_hal_pd_read(side, wl)` → `uint16_t` | `OI-INS-05` |
| Safety enable | `NP_SAFETY_EN_INTRANASAL` = bit 9 | `np_hub_config.h` |
| SHDR | Authentication pass/fail, via `np_log_shdr_zone_auth()` | `np_mod_intranasal.c` |
| Accessory poll | `NP_DETECT_ACCESSORY_POLL_MS` = 500 ms | `np_hub_config.h` |

> **`REQ-NASAL-13`** — Every HAL entry point above is **unimplemented** (`OI-INS-01…05`). The
> *shapes* are nonetheless a hardware contract, and §6.2 is what happens when a shape decides a
> commercial property: `auth_check()` returns `bool`, and a boolean cannot carry a serial.

---

## 5. Safety

### 5.1 What is enforced

The intranasal channel has its **own Class C enable line**, `NP_SAFETY_EN_INTRANASAL` (bit 9) —
it is not folded into `NP_SAFETY_EN_PBM_CRANIAL`. `REQ-NASAL-10`'s two-condition admission is a
hub-side gate ahead of that enable, on the same pattern as the VNS clip's contact confirmation:
the hub check exists to avoid asking, not to substitute for the MCU's refusal.

### 5.2 What is not — the missing exposure ceiling

`CLAUDE.md` §3's hard-limits table is the programme's statement of every modality ceiling. Its
optical rows are:

| Row in `CLAUDE.md` §3 | Ceiling |
|---|---|
| **PBM scalp** | 400 mW/cm² peak pulsed (≤ 25 % duty, firmware-enforced) · 200 mW/cm² CW · 42 °C (IEC 60601) |
| **PBM deep (T2)** | ≤ 1,000 mW/cm² (1170 nm, TEC-stabilised) |
| **PBM intranasal** | **— no row —** |

Three things follow, and they are separate:

1. **No irradiance ceiling.** Until Rev 3 the protocol carried `cur_660` and `cur_808`, uncalibrated
   8-bit drive registers with no mW/cm² of record at either end. Since Rev 3 it carries mW/cm² at the
   probe exit face, so a ceiling now has a quantity to be written in — but none is written, and the
   hub has no scale to convert with (`OI-NASAL-06`), so it refuses. The scalp figure is not transferable by default: it was
   chosen for skin over bone, and `RISK-03`'s regulatory opinion — still uncommissioned since
   2026-05-06 — is **scoped to 660/808 nm at the scalp**, not to mucosa.
2. **No temperature limit, and no way to enforce one.** The 42 °C IEC 60601 limit is realised on the
   cranial side by *"NTC per zone → hardware current throttle at 62 °C junction"* (`CLAUDE.md`
   §4.2). **The probe has no NTC** (§2.4 lists a PD and a reference LED, and nothing else), so even
   if the 42 °C limit is read onto it, no sensor in the design can observe it.
3. **The duty ceiling is inherited, not derived.** `INS_DUTY_MAX`'s own comment reads *"same ceiling
   as zone modules."* That is a reasonable interim default and it is **not** a mucosal exposure
   limit; it bounds duty, not irradiance, and 25 % duty at an unbounded drive current bounds
   nothing.

> **`OI-NASAL-02` — BLOCKING for T1 release.** The intranasal probe needs its own exposure ceiling
> (irradiance, and a thermal limit with a sensor that can observe it), stated in `CLAUDE.md` §3's
> hard-limits table as its own row. **This document does not supply one**: choosing a mucosal
> exposure limit is a clinical and regulatory decision with the same surface as `RISK-03`, and
> `RISK-03`'s existing engagement is explicitly *"do not open a parallel engagement — extend the
> existing instruction"* (`NP-REG-PBM1064-001` §2). The right move is to extend that brief.

### 5.3 Dose metering is single-PD, and the tile's own precedent says why that matters

The cranial tile carries **two** photodiodes by decision — PD1 behind the PDMS window measuring
forward emission, PD2 on the scalp-facing surface measuring backscatter — because *"the PD1/PD2
ratio separates PDMS fouling from LED aging in firmware"* (`docs/reference/modality-stack.md`,
RISK-14 Option B). **The probe has one PD and a reference LED.** That is a different topology
solving a different half of the problem: a reference LED lets the PD's own drift be separated from
the emitter's, but nothing separates **window fouling** — or a sleeve of the wrong optical
transmittance (§6.3) — from a dimming emitter.

> **`OI-NASAL-03`** — Establish whether single-PD-plus-reference-LED can meet the *"real-time J/cm²
> dose metering"* claim `CLAUDE.md` §3 makes for PBM, **through a consumable sleeve**, or whether
> the probe needs the tile's two-detector treatment. This is a dose-integrity question, not a
> component choice.

---

## 6. The hygiene sleeve

### 6.1 What the record fixes

`CLAUDE.md` §2.3: the sleeve is the **only authenticated consumable** and the **primary MRR
driver**. `docs/reference/commercial-model.md` §2.3: 30-pack, **single use**, 68–79 % GM, COGS
$4–6. `CLAUDE.md` §3: authentication is optical code + pogo-pin resistive, *no NFC, no EMF*.
**No price, interval or margin is set, changed or quoted by this document** beyond the citation
above; §2.3 of the commercial model remains the only place they live.

### 6.2 Single use is a commercial property with no mechanism behind it — finding

`CLAUDE.md` §2.3's Rev 48 rule governs *replacement prompts*, and a single-use barrier is not
prompted — it is **gated**. The gate is `np_mod_ins_hal_auth_check()`, and its interface of record
is:

```c
OI-INS-01: np_mod_ins_hal_auth_check() → bool (pogo-pin optical code match)
```

A boolean answers *"is a valid sleeve present?"* It cannot answer *"is this sleeve new?"*, because:

- there is **no per-unit identity** in the scheme as specified — an *optical code* and a *resistive
  code* are both naturally **type** codes, and nothing in the record says either is unique per unit;
- there is **no consumed state** — nothing in the record describes a fuse, a tear-off, a one-shot
  optical mark, or any irreversible change at first use;
- there is **no use counter** anywhere — `np_log_shdr_zone_auth()` logs pass/fail, not a serial, and
  SHDR carries no sleeve record;
- and a per-sleeve serial **could not** simply be moved into SHDR without thought: a per-unit
  consumable log is a usage trace of a person, which `CLAUDE.md` §5.1's defining test pushes toward
  **UHDR** — where, by design, NeurOne can never read it.

**So the design as specified authenticates the sleeve *type* and cannot enforce the sleeve *unit*.**
A user who refits one sleeve for thirty sessions passes every check the device makes. The
consequence is both hygienic (the barrier's purpose) and commercial (the MRR the model leans on).

> **`OI-NASAL-04`** — Decide what the sleeve authentication actually asserts: **type only**
> (in which case the single-use property is an IFU instruction and the revenue model should say so),
> or **per-unit with consumed state** (in which case the sleeve needs a mechanism, the HAL needs a
> wider return type than `bool`, and the privacy question above needs answering before a serial is
> logged anywhere). **This document does not choose**, because both options change the consumable's
> BOM and one of them changes the revenue model. Decision owner: Product + EE + Privacy.

### 6.3 The sleeve is in the optical path

A hygienic barrier over a probe tip sits **between the emitter and the tissue, and between the
tissue and the PD**. Nothing in the record states its optical transmittance at 660 nm or
808–830 nm, or its tolerance.

> **`REQ-NASAL-14`** — The sleeve's transmittance at both wavelengths, and its unit-to-unit
> tolerance, are **dose parameters**, not packaging properties. They belong in the sleeve's incoming
> inspection, and until they exist the delivered J/cm² has an uncharacterised multiplier in it.
> Carried by **`OI-NASAL-03`** with the dose-metering question, because the two are the same
> measurement problem seen from either end.

---

## 7. Hazard-analysis inputs

`OI-RISK2-02`'s block — *"no specification to analyse"* — is lifted for A12 by this document. The
analysis is still owed. Inputs, unscored:

| Candidate hazard | Why it is on the list |
|---|---|
| Mucosal overexposure (optical) | No irradiance ceiling exists (§5.2) |
| Mucosal thermal injury | No temperature limit and **no temperature sensor** in the probe (§5.2) |
| Over-insertion / nasal trauma | Depth is set by a mechanical ring outside the protocol, the safety MCU and UHDR (§2.1) |
| Depth-stop ring detachment in the nostril | Over-moulded silicone part inside a body cavity; retention unspecified (`REQ-NASAL-02`) |
| Cross-contamination via reused sleeve | Single use is unenforceable as specified (§6.2) |
| Cross-contamination via the hub dock | The dock touches the probe tips and the PD window every session (`NP-TOOL-HUB-001` F-01); no cleaning regime is specified for it |
| Dose error through an uncharacterised sleeve | §6.3 |
| Y-junction fracture | The failure the dock exists to prevent; no drop, fatigue or cycle requirement exists |
| Biocompatibility of every mucosa-contacting surface | Probe tip, ring, sleeve — no material of record for any of them |

---

## 8. What this document does **not** specify, and why

| Not specified | Why | Where it goes |
|---|---|---|
| Probe tip envelope, Y-junction OD, overall probe geometry | No geometry exists, and it follows from selections not yet made (§2.2 resolution). `NP-TOOL-HUB-001` F-01 is baselined against it and, since its Rev 2, **gated on it** | **`OI-NASAL-09`** (was `OI-NASAL-01`, closed Rev 2) |
| Emitter selection, optical power, beam geometry | The same absence as `OI-HEXTILE-02` on the cranial side — no 660/808 nm emitter is selected anywhere in the programme | **`OI-NASAL-06`**, with `OI-HEXTILE-02` |
| Mucosal irradiance and temperature ceilings | Clinical/regulatory decision, `RISK-03` surface (§5.2) | **`OI-NASAL-02`** (BLOCKING) |
| Depth-stop ring retention force and stop strength | §2.1 | **`OI-NASAL-05`** |
| Sleeve material, barrier property, fit, and optical transmittance | §6.3 | **`OI-NASAL-03`** |
| Sleeve code format and whether it is per-unit | §6.2 | **`OI-NASAL-04`** |
| Cable, connector, strain relief | No connector is selected for any accessory port — shared with `NP-HW-AUDIO-001` §8 and `NP-HW-VNSCLIP-001` §8 | **`OI-NASAL-07`** |
| Cleaning and disinfection regime for probe and dock | Body-cavity applicator with a shared storage cradle | **`OI-NASAL-08`** |

### 8.1 FAI readiness — the honest verdict

| | Verdict |
|---|---|
| **F1** — governing spec `BASELINED`/`ACTIVE` | **FAILS** — this document is DRAFT |
| **F2** — dimensions dimensioned or gated | **FAILS** — only the 15/20/25 mm ring family is dimensioned, and its tolerances are not |
| **F3** — supplier category exists | **NOT ASSESSABLE** — no process named for any sub-part |
| **F4** — failure modes in a risk register | **FAILS**, now unblocked — §7 is the input list |

> **`NP-FAI-NASAL-001` still cannot be written.** The absence narrows from *"no owning document"* to
> *"a DRAFT owning document with no dimensioned geometry and no exposure ceiling"*, and
> `NP-ART-001` §3.2 keeps it named.

---

## 9. Open items

| ID | Description | Owner | Blocking |
|---|---|---|---|
| ~~OI-NASAL-01~~ | **✅ CLOSED 2026-09-23 (Rev 2) by its second branch — F-01's geometry recorded as gated (§2.2 resolution); the four values carried forward as `OI-NASAL-09`.** The item read: **a BASELINED tool is dimensioned against an unspecified part.** `NP-TOOL-HUB-001` F-01's saddle radius, insert-pad placement and ≤ 2 N detent are all stated against A12 features — Y-junction OD, probe-tip envelope, PD window and authentication-contact locations — none of which exists. Supply the four values, or record F-01's geometry as gated | — (closed) | — |
| **OI-NASAL-02** | **BLOCKING — the intranasal probe has no exposure ceiling.** No irradiance limit, no thermal limit, and no sensor that could enforce a thermal limit (§5.2). `CLAUDE.md` §3's hard-limits table needs an intranasal row. Extend the existing `RISK-03` regulatory instruction (`NP-REG-PBM1064-001` §2 forbids a parallel engagement) rather than deriving one here | Clinical + Regulatory + Systems | **T1 release**; hazard analysis |
| **OI-NASAL-03** | Establish whether single-PD-plus-reference-LED meets the real-time J/cm² dose claim **through a consumable sleeve of uncharacterised transmittance**, or whether the probe needs the tile's dual-PD treatment. Includes specifying sleeve transmittance at 660 nm and 808–830 nm as an incoming-inspection parameter | EE + Optical | Dose claim integrity |
| **OI-NASAL-04** | **Decide what sleeve authentication asserts — type, or unit with consumed state (§6.2).** As specified it is a boolean type check, so single use is unenforceable, which touches hygiene and the primary MRR line at once. A per-unit serial additionally raises a UHDR/SHDR classification question that must be answered *before* anything is logged | Product + EE + Privacy | Consumable revenue model; hazard analysis |
| **OI-NASAL-05** | Specify depth-stop ring retention (must not detach in the nostril) and stop strength (must not yield under hand force), with tolerances. Three sizes, one interface | ME | Hazard analysis; A12 tooling |
| **OI-NASAL-06** | Select the 660 nm and 808–830 nm emitters for the probe. Shares the cranial-side absence (`OI-HEXTILE-02`) but not necessarily the part: a probe-tip emitter and a tile emitter have different thermal and envelope constraints. **Rev 3:** the selection must also yield the **exit-face irradiance at full drive, through a sleeve**, per wavelength — `NP_INS_IRR_FS_DMW` in `np_emission_cal.h` — because protocols are now absolute and the hub refuses every intranasal request until that constant is measured | EE + Optical | A12 BOM; `NP-FAI-NASAL-001` F2; **every intranasal session (hub refuses)** |
| **OI-NASAL-07** | Select cable, connector and strain relief to the hub accessory port. Worth deciding **once** across A11, A12, A13 and A14 rather than four times | EE + ME | A12 tooling |
| **OI-NASAL-08** | Specify the cleaning and disinfection regime for the probe and for the hub dock that touches it every session. A body-cavity applicator stored in a moulded cradle on the hub exterior has a contamination path the rest of the product does not | Quality + Product | IFU; hazard analysis |
| **OI-NASAL-09** | **Dimension the probe features the hub dock is stated against** — Y-junction body OD, probe-tip envelope, PD-window location and finish, authentication-contact location (`REQ-NASAL-03`). Succeeds `OI-NASAL-01`. Not derivable until the emitter (`OI-NASAL-06`), the sleeve wall and PD topology (`OI-NASAL-03`) and the cable (`OI-NASAL-07`) are selected. **Its blocking weight depends on `NP-TOOL-HUB-001` OI-HTOOL-07:** under route (a) it gates the hub mould steel cut; under route (b) it gates only the dock's bonded silicone parts, and the probe is instead designed into the hub's allocated envelope | ME + Systems | `NP-TOOL-HUB-001` HUB-MDR-01…03 → OI-HTOOL-06 (**hub mould cut**, unless OI-HTOOL-07 route (b) is taken); `NP-FAI-HUB-001` FAI-HUB-09, -10, -20 |

---

## 10. Cross-references

- **Artifact register row:** `docs/np_art_001.md` §2.4 (A12), §3.2
- **Tooling that depends on this document:** `docs/np_tool_hub_001.md` F-01
- **FAI issue conditions:** `docs/np_fai_001.md` §2
- **Risk gap this unblocks:** `docs/np_risk_002.md` §4, OI-RISK2-02
- **Driver behaviour:** `docs/np_fw_hub_001.md` §8.7; `firmware/hub_control/modules/np_mod_intranasal.c`
- **Protocol grammar:** `docs/np_npps_ref_001.md` §4.2
- **Consumable row:** `docs/reference/commercial-model.md` §2.3

---

## 11. Revision history

| Rev | Date | Author | Description |
|---|---|---|---|
| 3 | 2026-09-25 | NeurOne Systems Engineering | **The probe is commanded in absolute irradiance** (principal direction 2026-09-25: *"all emissions … must be specified in absolute terms to be safe from HW part changes"*; `NP-NPPS-REF-001` Rev 18). A protocol states on-state **mW/cm² at the probe exit face** (`irradiance_mw_cm2`); `intensity` / `intensity_percent` are parse errors. The wire carries `irr_660`/`irr_808` in place of the raw CUR codes, and `np_ins_irr_to_code()` converts against the exit-face irradiance at full drive — **which does not exist (`OI-NASAL-06`), so the hub refuses every intranasal request with `NP_HUB_ERR_UNCHARACTERISED`**. `REQ-NASAL-11` corrected: the 25 % cap is a *pulsed* cap; **CW is continuous** (100 % on-time), as on the scalp. The predefined protocols' values are **PROVISIONAL, from evidence** (22 and 20 mW/cm² CW from the cited trials; 100 mW/cm² pulsed borrowed from the scalp band) and carry their source in a comment. **No exposure ceiling is set — `OI-NASAL-02` is unchanged and still BLOCKING.** |
| 2 | 2026-09-23 | NeurOne Systems Engineering | **`OI-NASAL-01` closed by its second branch: the baselined hub dock is recorded as gated, not given invented dimensions** (§2.2 resolution). The four values the dock needs cannot be supplied honestly — each follows from an emitter, PD/sleeve or cable selection not yet made — so `NP-TOOL-HUB-001` Rev 2 now marks F-01's probe-contact geometry, HUB-MDR-01…03 and FAI-HTOOL-01 as gated, and `NP-FAI-HUB-001` Rev 2 gates FAI-HUB-09, -10 and -20 and corrects its §0 claim that the saddle radius was dimensioned. **The hub mould is not released by this**: its steel cut stays blocked through `OI-HTOOL-06`, now visibly rather than silently. The values are carried forward as **`OI-NASAL-09`**; `NP-TOOL-HUB-001` raises **OI-HTOOL-07**, recommending (not executing) moving every probe-contact surface into the dock's bonded silicone inserts so the hard mould no longer depends on the probe. **No engineering value is set; no requirement, price or locked decision changes.** |
| 1 | 2026-09-20 | NeurOne Systems Engineering | Initial release, against GitHub #332 / `NP-ART-001` OI-ART-04. **Gives artifact A12 an owning specification for the first time**, displacing `CLAUDE.md` §3 modality ② from that role, and treats the hygiene sleeve as a separately manufactured artifact rather than an accessory of the probe (`REQ-NASAL-01`). **Three findings, none of which existed as a written statement before.** (i) **`NP-TOOL-HUB-001` F-01 is BASELINED and dimensioned against features A12 does not specify** — saddle radius to a Y-junction OD nobody has stated, insert pads to a PD window with no location (`OI-NASAL-01`, and the one item here that gates a tool already at baseline). (ii) **The probe has no exposure ceiling of its own** — `CLAUDE.md` §3's hard-limits table has a scalp row and a deep-PBM row and no intranasal row; the duty ceiling is inherited from the cranial tiles by a firmware comment; the 42 °C limit is enforced by an NTC the probe does not have; and the tissue is mucosa (`OI-NASAL-02`, BLOCKING, routed to the existing `RISK-03` engagement rather than a parallel one). (iii) **The only authenticated consumable cannot be enforced as single-use** — the authentication interface of record is a `bool`, there is no per-unit identity, no consumed state and no counter, and a per-unit serial would itself raise a UHDR classification question (`OI-NASAL-04`). §5.3 additionally records that the probe's single-PD-plus-reference-LED topology does not reproduce the tile's dual-PD separation of window fouling from emitter ageing, and that a consumable sleeve of unstated transmittance sits in the optical path (`OI-NASAL-03`). §7 supplies the hazard-analysis input list `OI-RISK2-02` was blocked for. §8.1: **`NP-FAI-NASAL-001` still cannot be written** (F1, F2, F4 fail) — the absence is narrowed and re-owned, not closed. **No engineering value is set, no figure is invented, no price, interval, margin or locked decision changes.** Raises OI-NASAL-01…08. |
