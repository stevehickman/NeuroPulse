# Shell, Socket, Interconnect and Hub — Risk Register and Problem Analysis

**Project:** NeurOne
**Document:** NP-RISK-004
**Revision:** 1
**Date:** 2026-08-11
**Status:** ACTIVE
**Effective Date:** 2026-08-11
**Author:** NeurOne Quality (interim: Steve Hickman, CEO)
**Approved By:** Steve Hickman, CEO
**References:** NP-RISK-002 Rev 1 §3 (disposition source); NP-RM-001 Rev 1 §4 (scales); NP-DRV-SHELL-002 Rev 2 §3–§11 (REQ-BR2, REQ-SKT, REQ-EMI, SH2-DRC, OI-SHELL2-01…11); NP-HW-HEXTILE-001 Rev 3 §7–§8; NP-HW-HUB-001 Rev 3 (OI-HUB-C01…C19); NP-HELMET-GEOM-001 Rev 1; NP-TOOL-HUB-001 Rev 1; NP-FAI-HUB-001 Rev 1 §9; NP-REQ-FANHEALTH-001 Rev 1; NP-FMEA-GEOM-001 Rev 1 (FMEA-G07-01); NP-THERM-CFD-C2-001 Rev 1 §7; NP-CONV-001 Rev 2 (OI-CONV-01); NP-PROC-SUP-001 Rev 1; ISO 14971:2019
**Related Issues:** —
**Gate:** NP-COORD-001 G2 (pre-tooling)
**IEC 62304 Class:** N/A (hardware; Class C firmware implications flagged, not specified here)
**Supersedes:** Partially supersedes `NP-RISK-001` Rev 3 — the shell/routing/hub subset, per `NP-RISK-002` §3
**Parent Document:** NP-RISK-002 Rev 1

---

**Artifacts covered:** **A3** socket + 19-contact array · **A4** cluster controller board (18 off) ·
**A5** L1 inner-bowl laminate · **A6** L0 outer bowl and EMF stack · **A7** cluster clamp plate and
actuator · **A8** hub enclosure · **A9** Hub PCB Rev C.

---

## 1. Problem analysis

### 1.1 The interconnect stopped being a cable and became a distributed system

The retired design ran five FPC tails from five modules to one Hub PCB. Every risk it carried was a
*cable* risk: bend radius, chafing, separation from EEG conductors, connector cycle life.

`NP-DRV-SHELL-002` replaced that with **five physically separated networks (N1–N5) aggregated by 18
cluster controllers**, each an active board with an STM32G071, a PCA9548A, a 16:1 PD mux, a shared
switched-gain TIA, an 8:1 NTC mux and a 24 V power gate, laminated into the L1 bowl. The cable risks
went away almost entirely. What replaced them are **system** risks — a computation and an actuator
tier sitting inside the shielded envelope, in a thermally stagnant gap, on the same layer as
microvolt electrode traces.

The two most consequential facts about this change:

1. **Nothing flexes any more.** `NP-DRV-SHELL-002` §8.2: the module interconnect's dynamic-flex set
   is empty. RISK-11 is not mitigated, it is deleted.
2. **Eighteen boards now dissipate continuously.** Emitters are duty-cycled; controllers are not.
   That is a heat source nobody has budgeted, on the *outward* side of L1 — behind
   `NP-THERM-CFD-C2-001` §7's stagnant-air 0.231 m²K/W, which is ~59 % of the entire outward
   resistance and ~4× more resistive than the inward path to the perfused scalp. `NP-DRV-SHELL-002`
   §4.1 claims gap-facing components are "out of the scalp thermal path"; §10.3 and OI-SHELL2-11
   state plainly that this is **not established**.

### 1.2 The EEG separation requirement did not survive, and that is the correct outcome

RISK-21 and RISK-24 both existed to protect a **≥ 15 mm physical separation** between LED drive
conductors and EEG signal conductors. In the hex architecture that separation is not achievable at
any geometry: electrodes live *inside* T1-B tiles on L1, on the same layer as the N1 bus, at
millimetre scale.

`NP-DRV-SHELL-002` §9.1's response is the right shape and worth recording as a pattern — **it kept
the threshold and replaced the mechanism**. The requirement was never really "15 mm"; 15 mm was a
proxy for "< 5 µVpp of artifact in the EEG band with all LEDs at full PWM load". The threshold is
retained and verified directly by SH2-DRC-16, and four mechanisms replace the distance: broadside
loop-area control (REQ-EMI-06, ≤ 25 mm²), a DRL-driven guard plane on the scalp-facing face,
sense-quiet acquisition windows (REQ-EMI-03), and a **prohibition on spread-spectrum PWM**
(REQ-EMI-04) — which inverts the usual EMI remedy, because dither smears emitter energy across the
acquisition band instead of parking it outside.

A proxy requirement that becomes unsatisfiable is a signal to go back to what it was a proxy *for*.
That is what happened, and RISK-24 closes as **confirmed** rather than mitigated.

### 1.3 The polarity inversion is the most serious open item in this register

`NP-CONV-001` §1.1 required every active-low signal to end in `#`. Applying that rule mechanically
exposed a disagreement nobody had noticed:

- `NP-DRV-SHELL-002` §6 specifies `SAFE_EN[n]` as **LOW = rail removed = disabled**, so SH2-DRC-13's
  "defaults LOW at reset" is the **safe** state.
- The safety MCU specifies the opposite for its enable lines: *"Active-LOW open-drain: LOW =
  stimulation enabled"* (`np_safety_config.h:7-8`).

Both are internally coherent and fail-safe **on their own terms**. Together they are inverted.
Anyone implementing `SAFE_EN[n]` to the safety MCU's house convention makes LOW *enable* the cluster
— turning "defaults LOW at power-on reset" from the safe state into **stimulation enabled at
power-on reset**, across the whole cranial lattice.

This is `RISK-SHELL-03`. It is a Class C safety-architecture question, it was found by a naming
convention rather than by review, and it is the clearest available argument for `NP-CONV-001` §8's
rule that interface agreement is verified by mechanical diff and never by reading.

### 1.4 Two hazards created by their own mitigations

The hub enclosure carries a pattern worth naming because it recurs: **a control that introduces the
hazard class it was not designed for.**

- Tethered port covers exist for loss prevention (CLAUDE.md §2.3). A tether is a captive object; the
  hub also contains a rotating fan. `RISK-HUB-01`.
- A tool-free fan door exists so users can act on the SR-FAN-05 predictive-maintenance alert —
  without it the alert is useless. A service door is an access path to a rotating blade.
  `RISK-HUB-02`.

Both are bounded by geometry (≤ 20 mm tether that cannot reach the intake; ≤ 6 mm louvres against
the IEC 60601-1 test finger) and both are `[BLOCKING]` FAI items. Neither was in the retired risk
file, because the hub had no risk entries at all.

---

## 2. Risk register

| ID | Sev | Hazard | Cause | Consequence | Control | Owner | Status |
|---|---|---|---|---|---|---|---|
| **RISK-SHELL-03** | **CRITICAL** | `SAFE_EN[n]` polarity inverted between the interconnect spec and the safety MCU | `NP-DRV-SHELL-002` §6 declares active-high; `np_safety_config.h:7-8` declares active-low-enables | Implementing to the MCU convention makes power-on reset **enable** cranial stimulation across the lattice | **None yet.** `OI-CONV-01`; assess with `OI-FMEA-01` and `OI-HUB-C07`. May require renaming to `SAFE_EN#[n]`, which is a specification change | Safety + EE Lead | **OPEN — new** |
| **RISK-26** | HIGH | Fan/heatsink airflow loss → scalp face > 42 °C while junction NTC ≤ 62 °C | Junction throttle at 62 °C leaves the scalp-facing face 14–21 °C over the IEC 60601 applied-part limit at the worst fault | Thermal injury to the scalp with no sensor reading out of range — sensor nominal while hazard grows (FMEA-G07-01) | Path B1: scalp-facing NTC co-located with PD2 + SW01-M04 duty derate; SR-FAN-05 predictive alert; FAI-HUB-25/26 verify the telemetry. Constants provisional pending verification-grade CFD + THERM-1b | Thermal + FW | **ALARP — path selected** |
| **RISK-SHELL-02** | HIGH | 18 cluster controllers dissipate continuously behind the dominant outward thermal resistance | Rev 2 made the carrier active; emitters duty-cycle, controllers do not | Unbudgeted heat inside the inter-bowl gap; §4.1's "out of the scalp thermal path" claim unestablished; interacts with RISK-26 on the same budget | **None yet.** `OI-SHELL2-11`: needs a per-controller dissipation budget and a CFD case with the source on the gap-facing side of L1 | Thermal + EE Lead | **OPEN — new** |
| **RISK-SHELL-01** | HIGH | A partially-seated tile answers I2C and returns a plausible but wrong dose | Contacts mate progressively; `PD1_K` at elevated resistance still reads | Silent dose under-read — a wrong number, not a missing one. Worse than a detected fault | `SEAT#` asserts only when every other contact is home (`NP-DRV-SHELL-002` §5.1.3a); verified by SH2-DRC-10b partial-insertion sweep | EE + FW | **MITIGATED — unverified** |
| **RISK-20** | HIGH | CFRP rim Ra ≤ 1.6 µm may require secondary operations | Moulded CFRP surface finish unconfirmed by any tooling manufacturer in writing | Gasket seat non-conforming → RISK-16; or unbudgeted secondary machining across ~80 rims | Written confirmation required from tooling manufacturer before G2 (`NP-PROC-SUP-001` SUP-M-07, SUP-B-01, both BLOCKING) | ME / Procurement | **OPEN — never mitigated** |
| **RISK-22** | MEDIUM | Cluster clamp force exceeds one-handed capability for users with reduced grip or tremor | 34.2–57.0 N plate load on a 6-tile plate; retired per-module lever (≤ 1 N at the tip) is gone | Users with Parkinson's H&Y II–III cannot service their own device — an accessibility failure in a device sold for neurological conditions | Over-centre lever-throw actuator with per-module spring plungers (`NP-HEX-ZM-001` §5.4a). **Unverified**: `OI-SHELL2-03(b)`, SH2-DRC-10, HFE formative | ME + HFE | **OPEN — mechanism changed, unverified** |
| **RISK-13** | MEDIUM | LED PWM contaminates the EEG band | Drive switching energy couples into microvolt traces on the same layer | Neurofeedback and closed-loop adaptation act on artifact | REQ-EMI-03 sense-quiet windows; **REQ-EMI-04 prohibits spread-spectrum/dither**; REQ-EMI-06 loop area ≤ 25 mm²; DRL guard plane. Verified SH2-DRC-16 at **< 5 µVpp** | EE + FW | **MITIGATED — unverified** |
| **RISK-21** | HIGH | EEG signal integrity against LED drive, with no physical separation available | Electrodes are inside tiles on L1, millimetres from the N1 bus | As RISK-13 | Same control set. The ≥ 15 mm separation requirement is **withdrawn** — see §1.2 and RISK-24 | EE | **MITIGATED — mechanism replaced** |
| **RISK-12** | MEDIUM | Blind-mate alignment not held across a cluster | ±0.4 mm lateral tolerance must hold for up to 6 tiles simultaneously, not one | Contact mis-mate; intermittent dose or EEG channel | REQ-SKT-01 two staggered rows; SH2-DRC-05a CAD verification across a full cluster | ME + EE | **CARRIED — unverified** |
| **RISK-18** | HIGH | Presence-detect false negative blocks a session | `SEAT#` debounce; hazard shape inherited from ZONE_ID | Availability failure — user cannot start a session with functional hardware | `OI-HEXTILE-08` holds the debounce requirement, re-scoped from ZONE_ID | FW | **CARRIED** |
| **RISK-17** | MEDIUM | Interconnect design not coordinated with shell tooling before first cut | The coordination hazard survives the loss of the FPC it was written about | Shell retool: one-way decision, $15–40 k and 6–8 weeks per feature | `NP-DRV-SHELL-002` requirements + 33-item DRC; `NP-REV-SHELL-001` is the review record that gates first cut | ME + EE | **MITIGATED** |
| **RISK-10** | MEDIUM | Flex fabricator defaults to ED copper unless RA is specified | Fab default; RA must be an explicit drawing note | Reduced flex fatigue life on the cluster tails and L1 rigid-flex | RA per IPC-4204/11 Type I as a fab note + lot certificate (carried from `NP-PROC-FPC-001`) | HW EE | **MITIGATED** |
| **RISK-HUB-01** | MEDIUM | Tethered port cover reaches the fan intake | A mitigation (loss-prevention tether) creates a captive object near a rotating blade | Foreign object drawn into the fan; fan damage or debris | Tether ≤ 20 mm free length, anchor position chosen so the reachable envelope excludes the intake; **FAI-HUB-23 [BLOCKING]**; `OI-HTOOL-05` 3D sweep | ME | **MITIGATED — unverified** |
| **RISK-HUB-02** | MEDIUM | Finger contact with the fan blade through the open service door | A mitigation (tool-free access for SR-FAN-05) creates an access path | Laceration | Louvre slot ≤ 6.0 mm against the IEC 60601-1 Fig. 6 test finger; **FAI-HUB-24 [BLOCKING]** | ME / Safety | **MITIGATED — unverified** |
| **RISK-HUB-03** | LOW | Boa lace fatigue at a bend inside the hub housing | Hub segment continues the shell's lace channel through a new set of turns | Lace fracture in the one segment that is **not** field-replaceable with the in-box spare and hook tool | ≥ 12 × lace OD at every hub turn; **FAI-HUB-11 is [GATED] on `OI-HTOOL-02`** — the OD is not on file; FAI-HUB-21 50,000-cycle test | ME | **OPEN — criterion not derivable** |
| **RISK-SHELL-04** | MEDIUM | Hub enclosure has no environmental rating | No NeurOne document sets one; CLAUDE.md's IPX4 scope is the module connector | Ingress path into the hub, which holds the antennas, the PDN and the fan | **None.** `OI-HTOOL-03`; FAI-HUB-16 is `[GATED]` on it | ME + Quality | **OPEN — new** |

---

## 3. Verification map

| Risk | Verified by |
|---|---|
| RISK-SHELL-01 | SH2-DRC-10b (partial-insertion sweep with PD readback) |
| RISK-SHELL-03 | **No verification defined** — needs a safety-architecture decision first |
| RISK-SHELL-02 | THERM-1a CFD case with source on the gap-facing side of L1 — case does not exist |
| RISK-12 | SH2-DRC-05a |
| RISK-13, RISK-21 | SH2-DRC-16 (oscilloscope, all LEDs at full PWM load, < 5 µVpp) |
| RISK-22 | SH2-DRC-10 + HFE formative |
| RISK-26 | FAI-HUB-25, FAI-HUB-26; THERM-1b bench |
| RISK-HUB-01, -02 | FAI-HUB-23, FAI-HUB-24 — both `[BLOCKING]` |
| RISK-HUB-03, RISK-SHELL-04 | FAI-HUB-11, FAI-HUB-16 — both `[GATED]` |
| RISK-20 | `NP-PROC-SUP-001` SUP-M-07 / SUP-B-01 |

All `SH2-DRC-*` items are recorded, with reviewer and evidence, in `NP-REV-SHELL-001`.

---

## 4. Open items

| ID | Description | Owner | Blocking |
|---|---|---|---|
| **OI-RISK4-01** | **Resolve `SAFE_EN[n]` polarity (RISK-SHELL-03 / `OI-CONV-01`).** The highest-severity entry in this register and the only CRITICAL one. Two internally-coherent fail-safe conventions that are inverted with respect to each other, on the cranial stimulation enable. Assess jointly with `OI-FMEA-01` and `OI-HUB-C07`; a rename to `SAFE_EN#[n]` may be part of the resolution but is not the resolution. | Safety + EE Lead | **Cluster-carrier schematic; Hub PCB Rev C** |
| **OI-RISK4-02** | Budget per-controller dissipation and run a CFD case with the source on the gap-facing side of L1 (RISK-SHELL-02 / `OI-SHELL2-11`). Must be assessed as **one** budget with RISK-26, not separately — they share the outward path, and `OI-HUB-C17c`'s still-open half asks whether this silicon belongs on the tile instead. | Thermal + EE Lead | THERM-1a |
| **OI-RISK4-03** | Close RISK-22's accessibility question (`OI-SHELL2-03(b)`). The retired design had a per-module lever specified to ≤ 1 N at the tip precisely for this population; the replacement has no equivalent number yet. | ME + HFE | MECH-2 |
| **OI-RISK4-04** | Set the hub enclosure environmental rating (RISK-SHELL-04 / `OI-HTOOL-03`) and obtain the Boa lace OD (`OI-HTOOL-02`). Both unblock currently-unsignable FAI items in an otherwise complete checklist. | ME | `NP-FAI-HUB-001` completion |
| **OI-RISK4-05** | RISK-20 has been OPEN since 2026-05-06 and is BLOCKING two supplier-qualification items. Its scope grew from 5 shell slot rims to ~80 socket rims without being re-examined. | ME / Procurement | **G2; shell tooling** |

---

## 5. Revision history

| Rev | Date | Author | Description |
|---|---|---|---|
| 1 | 2026-08-11 | NeurOne Quality | Initial release. Holds the shell/routing/hub subset of the retired `NP-RISK-001` risk file per `NP-RISK-002` §3: RISK-10, -12, -13, -17, -18, -20, -21, -22, -26 carried with their original IDs, plus six new hazards under prefixed IDs (RISK-SHELL-01…04, RISK-HUB-01…03). **Problem analysis (§1) records that the interconnect stopped being a cable and became a distributed system** — 18 active controllers laminated into L1 — which deleted the cable risks (RISK-11) and created system ones, chiefly an unbudgeted continuous heat source behind ~59 % of the outward thermal resistance. **§1.2 records the pattern by which the ≥ 15 mm PBM-to-EEG separation requirement was correctly withdrawn**: 15 mm was a proxy for < 5 µVpp, the proxy became unsatisfiable, and the threshold was kept while the mechanism was replaced. **§1.3 records `RISK-SHELL-03`, the only CRITICAL entry** — `SAFE_EN[n]` polarity is inverted between `NP-DRV-SHELL-002` §6 and the safety MCU, so a power-on reset that is safe under one convention is *stimulation enabled at reset* under the other; it was found by applying a naming convention, not by review. §1.4 names two hub hazards created by their own mitigations. Raises OI-RISK4-01…05. |
