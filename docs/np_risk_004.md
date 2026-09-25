# Shell, Socket, Interconnect and Hub — Risk Register and Problem Analysis

**Project:** NeurOne
**Document:** NP-RISK-004
**Revision:** 4
**Date:** 2026-09-25
**Status:** ACTIVE
**Effective Date:** 2026-08-16
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
**A5** L1 inner-bowl laminate · **A6** outer bowl (stations L2 + L3) and EMF stack · **A7** cluster clamp plate and
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

> **Resolved 2026-09-25 — §2.2 adopted.** The disagreement was two nets joined by an inverting
> buffer that no hardware document specified. It is now specified (`NP-HW-HUB-001` HUB-REQ-C06).
> The re-score is `OI-RISK4-06`. The text below is retained as the finding.

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
| **RISK-SHELL-03** | **CRITICAL** *(score NOT re-assessed — see note)* | `SAFE_EN[n]` polarity inverted between the interconnect spec and the safety MCU | `NP-DRV-SHELL-002` §6 declares active-high; `np_safety_config.h:7-8` declares active-low-enables | Implementing to the MCU convention makes power-on reset **enable** cranial stimulation across the lattice | **Polarity resolved 2026-09-25 (§2.2 adopted):** the MCU pin `PBM_CRANIAL_EN#` is inverted to an active-high `PBM_CRANIAL_PERMIT`, which ANDs with an active-high `SAFE_EN[n]`. Pull-downs hold both disabled when undriven, and the pull-up and buffer share one rail (`NP-HW-HUB-001` **HUB-REQ-C06**). *Previously: none; `OI-CONV-01`. `OI-HUB-C07` closed 2026-08-16 and the basis changed, see §2.1* | Safety + EE Lead | **OPEN — polarity resolved 2026-09-25; score not yet re-assessed (`OI-RISK4-06`)** |
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

### 2.1 RISK-SHELL-03 — basis changed by the OI-HUB-C07 decision, score deliberately not re-assessed

**OI-HUB-C07 / OI-HEXTILE-13 closed on 2026-08-16** (`NP-HW-HUB-001` Rev 4 §7.2.1;
`NP-HW-HEXTILE-001` Rev 5 §8.4.1; `NP-DRV-SHELL-002` Rev 3 §6). The cranial PBM safety enable is one
Class C broadcast bit; the 18 per-cluster `SAFE_EN[n]` gates are retained as **IEC 62304 Class B**
availability gates in series with it.

**What changes for this entry.** The conflicted line is no longer a Class C stimulation enable. The
consequence column's *"power-on reset enables cranial stimulation across the lattice"* was written
when `SAFE_EN[n]` was the safety cut; under the decision, a `SAFE_EN[n]` that comes up enabled at
reset energises a cluster rail **behind** a Class C line that is separately de-asserted.

**What does not change.** The item stays **OPEN** and stays owned by Safety + EE Lead under
**OI-RISK4-01**. Two reasons: a Class B gate defaulting to *enabled* still energises a rail, and the
whole safety argument for that being tolerable rests on the series Class C line — whose own reset
polarity is the same unresolved convention question (`OI-CONV-01`, `OI-FMEA-01`). Resolving the
convention is still the fix; the decision narrows what is downstream of it, it does not supply it.

**The severity is NOT re-scored here, deliberately.** Re-scoring is hazard analysis, not editorial
correction — the precedent set by `NP-FMEA-001` Rev 4, which froze §3.4's scores and marked them
not-to-be-relied-on rather than silently updating them. **Treat CRITICAL as the standing score until
OI-RISK4-01 re-assesses it** *(that duty moved to `OI-RISK4-06` when OI-RISK4-01 closed on 2026-09-25)*, and do not read this note as a downgrade.

**One new input for that re-assessment: `HUB-REQ-C05`** (`NP-HW-HUB-001` §7.2.2) requires the Class B
gate to be commanded from a tier above the cluster controller that carries it. Reset polarity and
commanding authority are the same question asked twice, and should be assessed together.

### 2.2 RISK-SHELL-03 — resolution ADOPTED 2026-09-25 (GitHub #437); score NOT yet re-assessed

**Status: ADOPTED 2026-09-25 by the principal (Steve Hickman, CEO; interim Quality authority and
approver of this register).** It was proposed earlier the same day under `OI-RISK4-01`, and that
item and `OI-CONV-01` are now closed. The adoption changes no firmware line and renames no firmware
identifier. It **re-scores nothing**: the re-score is a separate hazard-analysis act, `OI-RISK4-06`.
What adoption carried out is listed at the end of this subsection. The analysis below is kept as
it was proposed.

**The finding is that the two "conventions" describe two different nets.** An inverter joins them,
and no hardware document specifies that inverter. It appears in exactly one place in the tree, a
Class C source comment: `np_gpio_mgr.c:5-7` says *"All lines are active-LOW open-drain with external
pull-ups to Vcc. Asserting a GPIO LOW enables the downstream stimulation driver **via an inverting
buffer**."* Read that way, the firmware and `NP-DRV-SHELL-002` §6 do not disagree:

| Node | Name under `NP-CONV-001` §1.1 | Active level | Undriven state held by | Specified in |
|---|---|---|---|---|
| Safety-MCU pin | `PBM_CRANIAL_EN#` | LOW | external **pull-up** → HIGH → disabled | `np_safety_config.h:7-8`, `np_safety_hal.h` (`np_hal_gpio_write_pin`) |
| Inverting-buffer output | `PBM_CRANIAL_PERMIT` *(proposed)* | HIGH | buffer output LOW, plus a receiver-side **pull-down** *(proposed)* | **Nowhere.** Firmware comment only (`np_gpio_mgr.c:5-7`) |
| Per-cluster Class B gate | `SAFE_EN[n]` | HIGH | **pull-down** → LOW → disabled | `NP-DRV-SHELL-002` §6 |

Each cluster's 24 V load switch conducts only when `PBM_CRANIAL_PERMIT` **AND** `SAFE_EN[n]` are both
HIGH. Both nets are active-high downstream of the inverter. So `SAFE_EN[n]` needs **no rename**, and
SH2-DRC-13's *"defaults LOW at reset"* is the safe state, as `NP-DRV-SHELL-002` §6 says.

**The tree already mixes the two nodes under one name.** `NP-FMEA-001` **OI-FMEA-01** sets its pass
criterion as *"GPIO cutoff latency from IWDG reset to all stimulation GPIO **LOW**"*. At the MCU pin
LOW means *enabled* (`np_safety_config.h:7-8`), so the criterion is correct only at the
buffer-output node. That is the defect `OI-CONV-01` names, found in a third document.

**What adopting the proposal would require.** Each item answers `NP-CONV-001` §7.1's two
questions: what fails if it is not met, and where that is traceable.

1. **Specify the inverting buffer in hardware** (`NP-HW-HUB-001` §3.1/§7.2, and the Hub PCB Rev C
   schematic). *Fails:* suppose a board is built to `NP-DRV-SHELL-002` §6 without the buffer and
   runs the firmware as written. The lattice is then enabled whenever the pin is pulled up, which is
   the disabled state. *Traced:* this subsection's table and `np_gpio_mgr.c:5-7`.
2. **Power the `PBM_CRANIAL_EN#` pull-up and the buffer from the same rail.** *Fails:* suppose the
   pull-up rail is lost while the buffer stays powered. The buffer input then falls LOW and its
   output goes HIGH, so one fault reaches *enabled*. On a shared rail, the same fault de-powers the
   buffer instead. *Traced:* IEC 60601-1 single-fault condition. The pull-up *value* is already
   `NP-SW-CI-001` **OI-SWCI-27**. This item adds its *supply domain* to that review.
3. **Put a pull-down at every receiver of an active-high enable** (`PBM_CRANIAL_PERMIT` and
   `SAFE_EN[n]`). *Fails:* an open connector or an unpowered driver would otherwise leave the gate
   input floating. *Traced:* `NP-DRV-SHELL-002` §6 already relies on this pull-down for
   `SAFE_EN[n]` (*"this one relies on a pull-down and a de-energized gate"*) but specifies no value
   or placement.
4. **Split the name at the inverter.** The MCU pin stays `PBM_CRANIAL_EN#`, and no firmware
   identifier is renamed, so `NP-CONV-001` §3's Class C boundary holds. Hardware documents call the
   buffer output `PBM_CRANIAL_PERMIT`.
   **The two names must differ in their stem, not just in the `#`.** A one-character difference
   is the defect `NP-CONV-001` §1.3 retired `_n` for: it vanishes in a plain-text diff and in
   many fonts. It has already happened to this line. Until its Rev 10, `NP-FW-HUB-001`'s
   References cited `NP-HW-HUB-001` §7.2's active-low cranial line as `PBM_CRANIAL_EN`, with the
   `#` dropped.
   *PERMIT* was chosen for three reasons. It occurs nowhere else in the tree, and *ARM* (the
   obvious alternative) collides with the processor architecture. It also states the net's role:
   the Class C tier *permits* the lattice, while the Class B `SAFE_EN[n]` *enables* each cluster,
   and a cluster runs only with both. `NP-HW-HUB-001` §3.1's diagram, which today shows
   `PBM_CRANIAL_EN#` gating the LED drive stage directly, is redrawn with the buffer.
   **OI-FMEA-01's pass criterion is restated against a named node.** As written, it passes on the
   unsafe pin level.

**What the proposal does NOT settle.** Carry these into the re-assessment:

- **Residual single fault.** An open pull-up, or a pin shorted to GND, with the buffer powered,
  reaches *enabled* on `PBM_CRANIAL_PERMIT`. This is the pin-level form of `NP-HW-HUB-001`
  **OI-HUB-C21**: one bit at Hamming distance 1 from enabled. **The safety MCU does not read its
  enable pins back today.** The primitive exists (`np_hal_pin_read()`, `np_hal_internal.h`), but
  its only caller is the SPI NSS watch in `np_hal_spi.c`. Nothing detects a pin stuck LOW against a
  commanded HIGH. **Decide whether a read-back is a required control before re-scoring.**
- **Commanding authority.** `HUB-REQ-C05` (who commands `SAFE_EN[n]`) is unchanged. The polarity
  becomes answerable, but commanding authority stays open under `NP-FW-HUB-001` `OI-FWHUB-11`
  (GitHub #384).
- **`np_safety_config.h:8`'s wording.** It says reset *"drives HIGH"*. `NP-SW-CI-001` §4.4.2
  established that the pull-up **holds** the line HIGH until `np_hal_gpio_init()` presets it via
  `BSRR`. The comment is Class C source and is not edited from here.

**What adoption carried out (2026-09-25):**

| Proposal item | Landed in | As |
|---|---|---|
| Close the conflict | `NP-CONV-001` **OI-CONV-01**, this register **OI-RISK4-01** | Both **closed** |
| 1 — buffer specified in hardware | `NP-HW-HUB-001` §7.2.5 | **HUB-REQ-C06** (a), (d). Schematic realisation is **OI-HUB-C22** |
| 2 — pull-up and buffer on one rail | `NP-HW-HUB-001` §7.2.5; `NP-SW-CI-001` **OI-SWCI-27** | **HUB-REQ-C06** (b); OI-SWCI-27's pull-up review now covers the supply domain as well as the value |
| 3 — receiver pull-downs | `NP-HW-HUB-001` §7.2.5; `NP-DRV-SHELL-002` §6 | **HUB-REQ-C06** (c) |
| 4 — name split at the inverter | `NP-HW-HUB-001` §3.1 diagram; `NP-CONV-001` §1.1; `NP-FMEA-001` **OI-FMEA-01** | Diagram redrawn with the buffer; `PBM_CRANIAL_PERMIT` recorded; OI-FMEA-01's pass criterion restated against named nodes |
| Unblock the review | `NP-DRV-SHELL-002` §11 SH2-DRC-13; `NP-REV-SHELL-001` | SH2-DRC-13 restated (it named the Safety MCU as `SAFE_EN[n]`'s commander, which HUB-REQ-C05 had already overturned) and moved **BLOCKED → open** |
| Re-score RISK-SHELL-03 | this register | **OI-RISK4-06, OPEN.** Not done here, by design |

**Not carried out, and why:** `np_safety_config.h:8`'s *"drives HIGH"* wording is Class C source,
so it is not edited from a risk register. Correcting it is a firmware change under
`NP-CONV-001` §3.

---

## 3. Verification map

| Risk | Verified by |
|---|---|
| RISK-SHELL-01 | SH2-DRC-10b (partial-insertion sweep with PD readback) |
| RISK-SHELL-03 | **Defined 2026-09-25 (§2.2 adopted):** (1) `NP-DRV-SHELL-002` **SH2-DRC-13**, as restated, at schematic + BSP review; (2) `NP-HW-HUB-001` **HUB-REQ-C06** at the Hub PCB Rev C schematic review (**OI-HUB-C22**); (3) `NP-FMEA-001` **OI-FMEA-01** bench cutoff timing, measured at the named nodes. *Previously: no verification defined, pending `OI-CONV-01` / `OI-FMEA-01`, see §2.1* |
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
| ~~**OI-RISK4-01**~~ | **✅ CLOSED 2026-09-25 — §2.2 adopted by the principal.** The polarity is resolved and the requirements it produced are HUB-REQ-C06 and OI-HUB-C22. **The re-score this item's text asks for moves to `OI-RISK4-06`** and was not done at closure. Original text: **Resolve `SAFE_EN[n]` polarity (RISK-SHELL-03 / `OI-CONV-01`).** The highest-severity entry in this register and the only CRITICAL one. Two internally-coherent fail-safe conventions that are inverted with respect to each other. Assess jointly with `OI-FMEA-01`; a rename to `SAFE_EN#[n]` may be part of the resolution but is not the resolution. **Updated 2026-08-16:** `OI-HUB-C07` is **closed**, so this item no longer waits on it — the basis changed (the line is now a **Class B** availability gate with a Class C broadcast line in series, §2.1) but the conflict did not resolve, and **the score is deliberately not re-assessed**. Two things to fold into the assessment: the reset polarity of the **Class C broadcast line itself**, which is the one that now carries the safety claim; and **`HUB-REQ-C05`** (commanding authority for the Class B gate) — polarity and commanding authority are the same question asked twice. **Resolution (§2.2, GitHub #437), proposed and adopted 2026-09-25:** the two conventions sit on two nets joined by an inverting buffer that exists only in a firmware comment (`np_gpio_mgr.c:5-7`). `SAFE_EN[n]` stays active-high and unrenamed, and the buffer, its supply domain and the receiver pull-downs become hardware requirements. | Safety + EE Lead | **Cluster-carrier schematic; Hub PCB Rev 4** |
| **OI-RISK4-02** | Budget per-controller dissipation and run a CFD case with the source on the gap-facing side of L1 (RISK-SHELL-02 / `OI-SHELL2-11`). Must be assessed as **one** budget with RISK-26, not separately — they share the outward path, and `OI-HUB-C17c`'s still-open half asks whether this silicon belongs on the tile instead. | Thermal + EE Lead | THERM-1a |
| **OI-RISK4-03** | Close RISK-22's accessibility question (`OI-SHELL2-03(b)`). The retired design had a per-module lever specified to ≤ 1 N at the tip precisely for this population; the replacement has no equivalent number yet. | ME + HFE | MECH-2 |
| **OI-RISK4-04** | Set the hub enclosure environmental rating (RISK-SHELL-04 / `OI-HTOOL-03`) and obtain the Boa lace OD (`OI-HTOOL-02`). Both unblock currently-unsignable FAI items in an otherwise complete checklist. | ME | `NP-FAI-HUB-001` completion |
| **OI-RISK4-05** | RISK-20 has been OPEN since 2026-05-06 and is BLOCKING two supplier-qualification items. Its scope grew from 5 shell slot rims to ~80 socket rims without being re-examined. | ME / Procurement | **G2; shell tooling** |
| **OI-RISK4-06** | **Re-score RISK-SHELL-03 now that its polarity is resolved (§2.2, adopted 2026-09-25).** This must be a hazard-analysis act, never a consequence of the adoption, following the `NP-FMEA-001` Rev 4 precedent that §2.1 applies. **Decide first whether an enable-pin read-back is a required control.** §2.2's residual single fault (a pull-up open or pin shorted LOW, with the buffer powered) reaches *enabled*, and nothing detects it: `np_hal_pin_read()`'s only caller is the SPI NSS watch. That decision is the pin-level half of `NP-HW-HUB-001` **OI-HUB-C21**, so assess the two together. `NP-FMEA-001` FMEA-M01-05 is the same gap from the FMEA side. Re-authored under OI-FMEA-11 (Rev 11), its residual is now **S5×P1 ALARP, with justification pending this item**: the read-back it once claimed never existed. **CRITICAL stands until this closes.** | Safety + EE Lead | RISK-SHELL-03 closure |

---

## 5. Revision history

| Rev | Date | Author | Description |
|---|---|---|---|
| 4 | 2026-09-25 | NeurOne Systems Engineering + principal | **`RISK-SHELL-03`'s polarity resolved: §2.2 proposed and ADOPTED the same day (GitHub #437); the score is NOT re-assessed.** New §2.2 finds that the two polarity conventions describe two nets. An inverting buffer joins them, and only `np_gpio_mgr.c:5-7` stated it; no hardware document did. `PBM_CRANIAL_EN#` (MCU pin, active-low, pull-up) is inverted to the active-high **`PBM_CRANIAL_PERMIT`**, which ANDs with the active-high `SAFE_EN[n]`. So SH2-DRC-13's "defaults LOW" is safe, and no `SAFE_EN#[n]` rename is needed. The buffer output's name differs from the pin's in its stem, not just the `#`, because that slip had already happened in `NP-FW-HUB-001`. **Adopted by the principal 2026-09-25:** `OI-RISK4-01` and `NP-CONV-001` OI-CONV-01 closed. Requirements raised as `NP-HW-HUB-001` **HUB-REQ-C06** / **OI-HUB-C22**. The supply domain was added to `NP-SW-CI-001` OI-SWCI-27. `NP-FMEA-001` OI-FMEA-01's criterion was restated (it passed on the enabled pin level). `NP-DRV-SHELL-002` SH2-DRC-13 was restated and unblocked. Register row, §1.3 and the verification map updated. **Found in passing:** no enable-pin read-back exists in the safety MCU. **New `OI-RISK4-06`** holds the re-score and the read-back decision, and CRITICAL stands until it closes. |
| 3 | 2026-09-25 | NeurOne Systems Engineering | **Label only (GitHub #331): A6 was called the "L0 outer bowl"; it is the outer bowl, stations L2 + L3.** `NP-HELMET-GEOM-001` §2 owns the radial stack and puts L0 at the scalp face. `NP-TOOL-SHELL-002` §1.4 records the collision. No hazard, score or mitigation changes. `NP-TOOL-SHELL-002` §5.6 asks `OI-RISK4-05` to check whether RISK-20's CFRP scope shrinks to the outer-bowl lip seat, because the ~80 socket rims are on the glass-filled PBT inner bowl. §5.2 finds no hazard row for a session enabled with the bowls open (`OI-TSHELL2-02`). Neither is decided here. |
| 2 | 2026-08-16 | SmartyPants / PAI | **`RISK-SHELL-03` basis changed by the OI-HUB-C07 decision; no score re-assessed, no hazard added or removed.** `OI-HUB-C07` / `OI-HEXTILE-13` closed 2026-08-16 (`NP-HW-HUB-001` Rev 4 §7.2.1): the cranial PBM safety enable is **one Class C broadcast bit**, and the 18 per-cluster `SAFE_EN[n]` gates are retained as **IEC 62304 Class B** availability gates in series with it. The conflicted `SAFE_EN[n]` line is therefore no longer a Class C stimulation enable, which changes what the entry's consequence column describes. **The entry stays OPEN, stays CRITICAL, and is deliberately NOT re-scored** — re-scoring is hazard analysis rather than editorial correction, per the `NP-FMEA-001` Rev 4 precedent; the standing score holds until `OI-RISK4-01` re-assesses it. New §2.1 records the changed basis in full, including that the safety claim now rests on the **Class C broadcast line's** own reset polarity — the same unresolved convention question (`OI-CONV-01`, `OI-FMEA-01`) one level up. `OI-RISK4-01` updated: it no longer waits on OI-HUB-C07, and gains two inputs — the Class C line's reset polarity, and **`HUB-REQ-C05`** (`NP-HW-HUB-001` §7.2.2), which requires the Class B gate to be commanded from a tier above the cluster controller carrying it. Verification map updated to say the item still has no verification defined and why. **No firmware changed; no other register touched.** |
| 1 | 2026-08-11 | NeurOne Quality | Initial release. Holds the shell/routing/hub subset of the retired `NP-RISK-001` risk file per `NP-RISK-002` §3: RISK-10, -12, -13, -17, -18, -20, -21, -22, -26 carried with their original IDs, plus six new hazards under prefixed IDs (RISK-SHELL-01…04, RISK-HUB-01…03). **Problem analysis (§1) records that the interconnect stopped being a cable and became a distributed system** — 18 active controllers laminated into L1 — which deleted the cable risks (RISK-11) and created system ones, chiefly an unbudgeted continuous heat source behind ~59 % of the outward thermal resistance. **§1.2 records the pattern by which the ≥ 15 mm PBM-to-EEG separation requirement was correctly withdrawn**: 15 mm was a proxy for < 5 µVpp, the proxy became unsatisfiable, and the threshold was kept while the mechanism was replaced. **§1.3 records `RISK-SHELL-03`, the only CRITICAL entry** — `SAFE_EN[n]` polarity is inverted between `NP-DRV-SHELL-002` §6 and the safety MCU, so a power-on reset that is safe under one convention is *stimulation enabled at reset* under the other; it was found by applying a naming convention, not by review. §1.4 names two hub hazards created by their own mitigations. Raises OI-RISK4-01…05. |
