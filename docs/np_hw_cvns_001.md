# Cervical VNS Accessory — Electrode Assembly, Cable and Connector Hardware Specification

**Project:** NeurOne
**Document:** NP-HW-CVNS-001
**Revision:** 2
**Date:** 2026-09-22
**Status:** DRAFT — **requirements-grade. The electrode assembly is undimensioned and its area is a PROVISIONAL firmware constant; §8 names each absence rather than supplying a value.**
**Effective Date:** —
**Author:** NeurOne Systems Engineering
**Approved By:** — (new document)
**References:** CLAUDE.md §3 T2 additions (cervical VNS accessory), §4.2 (cardiac rhythm interlock), §3 hard-limits table (40 µC/cm² per phase); `docs/reference/modality-stack.md` (cervical VNS bullet); NP-FW-CVNS-001 Rev 5 §3, §5, §6.2, §7.1, §9 (FAI-CV01…CV03), §13 (RISK-25), §14 (OI-CVNS-08…11); NP-REG-CVNS-001 Rev 1 §2.4 (device description), §3.2 (predicate comparison); NP-FW-HUB-001 Rev 1 §8.8 (`np_mod_cvns.c`); NP-HW-VNSCLIP-001 Rev 1 (A13 — the PPG source this accessory's interlock depends on); NP-DT-001 §3.2.1 DI-SAFE-01a; NP-SW-001 Rev 8 §5.1 SW01-M03; NP-RISK-002 Rev 3 §3 RISK-25, §4; NP-ART-001 Rev 3 §2.4 A14, §3.2, OI-ART-04; NP-FAI-001 Rev 3 §2, §2.1.1, OI-FAI-07; NP-CONV-001 Rev 6 §4, §5, §6; `firmware/hub_control/include/np_hub_config.h` (`NP_CVNS_ELECTRODE_AREA_MCM2`); electroCore gammaCore K163334, K173323
**Related Issues:** GitHub Issue #332 (A14 hardware specification — this document is that specification); GitHub Issue #24 (cervical VNS regulatory half); `OI-CHARGE-07`
**Gate:** NP-COORD-001 G3-08 (the gate `NP-FW-CVNS-001` closes for the firmware half); G2 for the artifact
**IEC 62304 Class:** N/A (hardware specification). It supplies the electrode area the **Class C** charge monitor divides by (§5.3) and the electrode assembly the **Class C** cardiac interlock protects.
**Supersedes:** None — first issue. It ends the state `NP-ART-001` §2.4 records as *"firmware + regulatory specified"* with the hardware absent, and it is the document `NP-FAI-001` §2.1.1 and OI-FAI-07 name as the missing precondition for `NP-FAI-CVNS-001`.
**Parent Document:** None

---

> **Why this document exists.** A14 is the best-documented artifact in the programme along two axes
> and undocumented along the third. It has a **BASELINED firmware specification**
> (`NP-FW-CVNS-001` Rev 5: constants, interlock state machine, SPI command set, three FAI test
> procedures) and a **regulatory submission strategy** (`NP-REG-CVNS-001`: gammaCore predicate,
> comparison table, Q-Sub questions). **The electrode assembly, the cable and the connector — the
> things that are manufactured — have never been specified.** That absence is cited by name in four
> places: `NP-ART-001` §2.4 and §3.2, `NP-FAI-001` §2.1.1 (*"F1 fails absolutely"*) and OI-FAI-07,
> and `NP-RISK-002` RISK-25, whose FAI bench cannot be scheduled because of it. This is that
> specification.
>
> **What it changes and what it does not.** `NP-FAI-CVNS-001` remains unwritable — F1 now fails
> because this document is DRAFT rather than because nothing exists, which is a smaller and
> differently-owned absence (§8.1). No constant, limit, criterion or clinical claim moves.
>
> **The finding is §5.2: the cardiac interlock's R-peak source is a different artifact.** The
> control that distinguishes NeurOne from its own 510(k) predicate takes its heartbeat from the
> **auricular clip's PPG sensor** (A13, `NP-HW-VNSCLIP-001`) — a T1 accessory whose skin contact is
> made through a consumable pad with a 20–40 session life. No document states that A14's safety
> case has a dependency on A13 being worn and working.

---

## 1. Scope

**In scope, and binding:**

| § | What it pins |
|---|---|
| §2 | The artifact and its sub-parts, as `NP-REG-CVNS-001` §2.4 enumerates them |
| §3 | The requirements the record already binds, and their provenance — several are **predicate-matched** and cannot be changed without a regulatory consequence |
| §4 | Interfaces: the 4-conductor cable, and the enable-line question `OI-CVNS-09` leaves to hardware |
| §5 | Safety: the cardiac interlock, its cross-artifact R-peak dependency, and the unmeasured electrode area |
| §6 | The gel-pad consumable |
| §7 | Hazard inputs — RISK-25 exists; this section is what the artifact register it lacks would hold |

**Out of scope:** the cardiac interlock firmware and its constants (`NP-FW-CVNS-001`, BASELINED —
nothing here restates or amends it); the 510(k) strategy and predicate analysis
(`NP-REG-CVNS-001`, issue #24); and the auricular clip, which is artifact A13
(`NP-HW-VNSCLIP-001`) and is referenced here only where A14 depends on it.

---

## 2. The artifact

`NP-REG-CVNS-001` §2.4 already enumerates the components for the regulatory reader. This section is
that enumeration read as a **manufacturing** list, with the status of each part's specification
stated.

| Sub-part | What the record says | Status |
|---|---|---|
| **A14.1 — neck module housing** | *"Shore 30A silicone over-moulded unit; bilateral gel electrode assembly"* | Durometer and process named. **No geometry, no retention method, no mass** |
| **A14.2 — electrode contacts (×2)** | `NP_CVNS_ELECTRODE_COUNT` = 2, bilateral or unilateral use; applied over the carotid sheath **2 cm inferior to the angle of the jaw** (FAI-CV01) | Placement defined by an FAI procedure. **No electrode material, geometry or measured area** (§5.3) |
| **A14.3 — gel pads (consumable)** | Self-adhesive hydrogel, 5-pack, *"single-use per electrode contact surface"* | Commercially defined. **No formulation, adhesion or impedance specification** |
| **A14.4 — cable** | *"4-conductor shielded cable to NeurOne hub accessory port"* | **Conductor count and shielding are fixed** — the only accessory in the set whose cable construction is stated (§4.2). Pin assignment is not |
| **A14.5 — connector** | Hub accessory port | Not selected. The shared accessory-connector gap |
| **A14.6 — current source and sense** | *"Stimulation current generated by NeurOne Pro (T2) hub; safety MCU owns enable GPIO"* | **The driver is not in the accessory** — A14 carries no active electronics of record (§4.3) |

> **`REQ-CVNS-01`** — A14 is a **passive applicator**: electrodes, over-mould, cable, connector. All
> current generation, charge balancing, impedance measurement and enable ownership are hub-side
> (`NP-REG-CVNS-001` §2.4; `NP-FW-CVNS-001` §7.1). Any proposal to site electronics in the neck
> module is a change to the safety architecture, not a packaging choice, because the safety MCU's
> enable GPIO and its current-sense ADC channel both assume the hub-side path.

---

## 3. Requirements the record already binds

**Several of these rows are predicate-matched.** `NP-REG-CVNS-001` §3.2 argues substantial
equivalence to gammaCore partly on the grounds that NeurOne's parameters are **within or more
conservative than** the cleared device's. A row marked *predicate* below cannot be relaxed without
re-running that argument.

| ID | Requirement | Source | Predicate-coupled? |
|---|---|---|---|
| **REQ-CVNS-02** | Two electrodes, bilateral or unilateral operation | `NP-FW-CVNS-001` §3.2; `NP-REG-CVNS-001` §3.2 | Bilateral is a **difference** from the predicate and is argued as one |
| **REQ-CVNS-03** | Biphasic charge-balanced, **cathodic-first**, 100 µs inter-phase gap; charge balance **hardware-enforced** to ±1 µC, independently monitored by the safety MCU via a current-sense ADC | `NP-FW-CVNS-001` §7.1 | **Yes** — waveform topology matches the predicate |
| **REQ-CVNS-04** | 1–25 Hz · ≤ 2 mA · pulse width 200–1000 µs · session 60–120 s | `NP-FW-CVNS-001` §3.1 | **Yes** — every bound is argued in `NP-REG-CVNS-001` §3.2 as equal to or more conservative than gammaCore |
| **REQ-CVNS-05** | Per-phase commanded-charge ceiling **40 µC/cm²** against the declared electrode area | `CLAUDE.md` §3, §4.2; `NP-DT-001` DI-SAFE-01a | No |
| **REQ-CVNS-06** | Electrode contact impedance **≤ 5.0 kΩ** measured at **1 kHz AC**, per electrode, before enable | `NP-FW-CVNS-001` §3.2 | No |
| **REQ-CVNS-07** | Placement over the carotid sheath, **2 cm inferior to the angle of the jaw**; a 5 cm superior misplacement must read **> 5 kΩ** and block enable | `NP-FW-CVNS-001` §9 FAI-CV01 | No — but it is the accept criterion the FAI is written to |
| **REQ-CVNS-08** | Single-electrode removal must present as an open circuit or out-of-range impedance **within 500 ms** | `NP-FW-CVNS-001` §9 FAI-CV01-B | No |
| **REQ-CVNS-09** | Cardiac interlock: HR change **> 15 BPM within 5 s** → enable GPIO low **< 100 ms**; 30 s lockout, app confirmation and repeat impedance before re-enable | `CLAUDE.md` §4.2; `NP-FW-CVNS-001` §3.4, §3.5 | **Yes** — presented to FDA as a safety enhancement over the predicate |
| **REQ-CVNS-10** | Ramp up 10 s, ramp down 5 s | `NP-FW-CVNS-001` §3.3 | No |
| **REQ-CVNS-11** | Shore 30A silicone over-mould; 4-conductor **shielded** cable | `NP-REG-CVNS-001` §2.4 | No |

> **`REQ-CVNS-12`** — `REQ-CVNS-07` and `REQ-CVNS-08` are **hardware acceptance criteria written as
> firmware test steps.** FAI-CV01 inspects the *electrode assembly* — placement geometry, contact
> impedance, open-circuit detection — through a firmware procedure, because the firmware
> specification was the only document A14 had. With this document in place they are A14's criteria,
> and `NP-FAI-CVNS-001` inherits them when F1 is met (§8.1). `NP-FW-CVNS-001` §9 stays the test
> *procedure* of record; nothing moves out of it.

---

## 4. Interfaces

### 4.1 Firmware and hub

| Parameter | Value | Source |
|---|---|---|
| Hub slot | 10, `NP_MOD_CVNS` = `0x0A`, detected on the accessory port (T2) | `np_hub_config.h`; `NP-FW-HUB-001` §8.8 |
| Safety enable | `NP_SAFETY_EN_CVNS` = bit 10, requested **only** after impedance → cardiac baseline → MCU grant | `NP-FW-HUB-001` §8.8 |
| Declared electrode area | `NP_CVNS_ELECTRODE_AREA_MCM2` = **2000** (2 cm²), **PROVISIONAL — NOT MEASURED** | `np_hub_config.h`; `OI-CHARGE-07` |
| Impedance cross-validation | Hub measurement vs safety-MCU measurement; divergence beyond `NP_CVNS_IMPEDANCE_CROSSVAL_KOHM` emits `NP_CVNS_SHDR_EV_IMP_CROSSVAL` — **a flag, no kΩ values** | `NP-FW-HUB-001` §8.8 |
| R-peak path | Main processor PPG → Pan-Tompkins → 5 ms pulse on the MCU's R-peak input | `NP-FW-CVNS-001` §5.2, §6.2 |

### 4.2 The cable — four conductors, no pin-out

`NP-REG-CVNS-001` §2.4 fixes **4 conductors, shielded**. The record requires the cable to carry: two
electrode drives, a return path, per-electrode impedance sense at 1 kHz, and a shield.

**Four conductors plus shield admits at least one consistent assignment** — two drives, one common
return, one sense or guard — and **no document assigns them.** Per-electrode impedance sense
(`REQ-CVNS-06`, `REQ-CVNS-08`) is the constraint most likely to decide it, because independently
measuring two electrodes through a shared return is a circuit decision, not a wiring one.

> **`OI-CVNSHW-01`** — Assign the four conductors, and confirm the assignment supports per-electrode
> impedance measurement and single-electrode open detection within 500 ms. If it does not, the
> conductor count is a **regulatory-visible** change: `NP-REG-CVNS-001` §2.4 states it in the device
> description.

### 4.3 Where `OI-CVNS-09` actually lands

`NP-FW-CVNS-001` OI-CVNS-09 asks whether there is **one** CVNS enable line or **two**
(`CVNS_ENABLE_L` / `CVNS_ENABLE_R`). Rev 1 of that document instructed asserting both; the firmware
has a single `NP_EN_CVNS`, so the two-line assertion is unimplementable as written, and the item was
escalated rather than silently resolved.

**It is a hardware question, and it belongs here.** One enable line is correct if the two electrodes
are driven from one source and cannot be energised independently; two are needed if they can. The
record points both ways — `REQ-CVNS-02` allows **unilateral** operation, which implies the sides are
separable, while `REQ-CVNS-01` puts a single current source in the hub.

> **`OI-CVNSHW-02`** — Decide the drive topology (one source with a side selector, or two
> independent sources), and `OI-CVNS-09` follows from it rather than the other way round. Note the
> safety consequence of each: one source means a side-select fault mis-routes current with the
> enable still legitimately asserted; two sources mean two enables and a second Class C line.

---

## 5. Safety

### 5.1 What is enforced, and why this artifact is the most interlocked in the set

Current near the carotid sheath is the highest-consequence path in the product, and the control set
reflects it: pre-enable impedance on both electrodes, an independently computed cardiac baseline on
**both** processors cross-validated at ±5 BPM, a 200 Hz cardiac poll on the Class C MCU, a
< 100 ms cutoff, and a three-condition re-enable. All of it is `NP-FW-CVNS-001`'s, all of it is
BASELINED, and **none of it is restated or modified here.**

### 5.2 The interlock's heartbeat comes from another artifact — finding

`NP-FW-CVNS-001` §6.2 states the R-peak source exactly: *"Pan-Tompkins-derived bandpass detection on
the PPG signal from the **VNS accessory PPG sensor** (808–830 nm, co-located in the clip mount)."*
That sensor is **A13**, the auricular VNS/HRV clip (`NP-HW-VNSCLIP-001` §2, A13.3).

So the cardiac interlock — the control that mitigates RISK-25, and the *"key safety differentiator
from predicate"* in `NP-REG-CVNS-001` §2.4 — has a **cross-artifact dependency that no document
states**:

1. **A14's safety case requires A13 to be worn** during every cervical VNS session. Nothing in
   `NP-REG-CVNS-001`'s device description says the accessory requires a second accessory.
2. **A13's skin contact is made through a consumable** with a 20–40 session life whose degradation
   mechanism is electrochemical attack by *auricular* VNS current (`NP-HW-VNSCLIP-001` §6). A pad
   degraded by unrelated use is a degraded R-peak source for this interlock.
3. **The two artifacts' readiness differs by a tier.** A14 is T2 and regulated; A13 is a T1
   accessory with — until this issue — no specification at all.
4. **The dependency is invisible in the enable path.** The MCU refuses to arm until 8 valid
   intervals accumulate (`NP-FW-CVNS-001` §5.3), so *absent* PPG fails safe. **Degraded** PPG is
   the case worth analysing, and `OI-CVNS-11` already records that the safety MCU applies **no R-R
   validity filter** of its own.

> **`OI-CVNSHW-03`** — State A13-worn-and-functioning as an explicit **precondition of A14's safety
> case**, in `NP-REG-CVNS-001`'s device description, in RISK-25's control set, and in the IFU — or
> give A14 its own cardiac sensing. This document does not choose between them: the first is a
> documentation and regulatory act, the second is a hardware addition to a 510(k)-track accessory.
> **The cross-artifact dependency itself is not a defect** — sharing a sensor is a reasonable
> design. Leaving it unstated in the safety case is.

### 5.3 The unmeasured electrode area

As for the auricular clip (`NP-HW-VNSCLIP-001` §5.2), the per-phase ceiling divides by a
**PROVISIONAL, never-measured** constant — here `NP_CVNS_ELECTRODE_AREA_MCM2` = 2 cm². It is
legitimately a firmware constant under `OI-CHARGE-07`'s test (a fixed product part, not a
user-chosen consumable), and it is legitimately **this document's** value to supply.

> **`REQ-CVNS-13`** — The gel electrode's **wetted contact area** is specified here, measured as
> manufactured, with a tolerance. Until it is, `NP_CVNS_ELECTRODE_AREA_MCM2` may be revised **down**
> freely and **up** only against a measurement, per the fail-safe assertion in
> `np_safety_spi_proto_tests.c`. **`OI-CVNSHW-04`** — and it closes `OI-CHARGE-07`'s cervical limb.
>
> Note the interaction with the consumable: the *pad* (A14.3) is what contacts skin, so the area
> the interlock needs is the **pad's** wetted area, not the electrode's — and the pad is a 5-pack
> item whose specification does not exist either (§6).

---

## 6. The gel-pad consumable

`NP-REG-CVNS-001` §2.4 and `docs/reference/modality-stack.md`: *gel pad consumable (5-pack),
single-use per electrode contact surface.* It does **not** appear in
`docs/reference/commercial-model.md` §2.3's consumables table, which lists intranasal sleeves,
hydrogel tips, VNS clip pads, audio foam, mesh frames, covers, the S3 insert and the T2 service
contract — **but no cervical VNS gel pad.**

That is a gap in the commercial record rather than in this one, and it is recorded rather than
filled: pricing a consumable is `docs/reference/commercial-model.md`'s act, and `CLAUDE.md` §2.1's
`OI-COST-10` sequencing constraint means no price may be set here in any case.

> **`OI-CVNSHW-05`** — The cervical VNS gel pad is a shipping T2 consumable with no row in
> `docs/reference/commercial-model.md` §2.3 — no price, no interval, no margin, and therefore no
> trigger assessed against `CLAUDE.md` §2.3's two admissible kinds. Since it is **single-use**, the
> likely answer is that it is gated rather than prompted (as the intranasal sleeve is), and that
> raises the same enforcement question `NP-HW-NASAL-001` §6.2 raises — **with the difference that
> this pad is not authenticated at all.** Route to Product + Quality.

> **Rev 2 (2026-09-22) — the commercial half is closed; the enforcement half is what remains.**
> `NP-ACC-PRIORITY-001` `OI-ACC-03` raised the same gap from the commercial side and is closed:
> `docs/reference/commercial-model.md` §2.3 now carries the row — single use, price and GM% not set
> (`OI-COST-10`), and a trigger assessed as a **condition measurement at point of use**: `REQ-CVNS-06`'s
> per-electrode ≤ 5.0 kΩ check, on which the safety MCU refuses enable. So the pad *is* gated, as
> this item predicted — but on its **contact**, not its **use count**. A dried, lifted or missing pad
> is refused; a reused pad whose hydrogel still conducts is not. **`OI-CVNSHW-05` is re-scoped to that
> question alone:** does single use need enforcement beyond the IFU (authentication as the intranasal
> sleeve has it, or some other means), and if so what? Nothing here assumes the answer.

---

## 7. Hazard inputs

Unlike A11–A13, A14 **has** a risk entry: **RISK-25** (cervical VNS cardiac reflex —
bradycardia/asystole near the carotid sheath), LOW / MITIGATED, CARRIED and held in
`NP-RISK-002` §4 *"with no artifact register."* `NP-RISK-002` §4's stated reason for holding it
there is `OI-ART-04` — the artifact has no specification document. **That reason is now spent.**

Inputs a per-artifact register for A14 would hold, beyond RISK-25:

| Candidate hazard | Why it is on the list |
|---|---|
| Cardiac reflex from stimulation near the carotid sheath | **RISK-25**, already in the file |
| Degraded or absent R-peak source (A13's PPG through a worn pad) | §5.2 — a dependency of the RISK-25 control itself |
| Charge-density overexposure at the true pad area | §5.3 |
| Misplacement over the carotid body rather than the sheath | `REQ-CVNS-07` detects a 5 cm *superior* error by impedance; impedance does not distinguish *anatomy* at the correct depth |
| Single-electrode detachment mid-session | Detected within 500 ms (`REQ-CVNS-08`); the hazard is the 500 ms, and the current distribution during it |
| Skin irritation, burns or sensitisation under a single-use hydrogel on the neck | No pad specification exists (§6) |
| Cable entrapment or tension at the neck | A neck-worn tethered accessory; no strain relief or breakaway specified |
| Reuse of a single-use pad | Not authenticated, unlike the intranasal sleeve (§6) |

---

## 8. What this document does **not** specify, and why

| Not specified | Why | Where it goes |
|---|---|---|
| Neck module geometry, retention, mass, size range | No geometry exists for A14 anywhere | **`OI-CVNSHW-06`** |
| Electrode material and geometry; **measured** contact area | §5.3 | **`OI-CVNSHW-04`** |
| Gel pad formulation, adhesion, shelf life, biocompatibility | §6 | **`OI-CVNSHW-05`**, **`OI-CVNSHW-07`** |
| Cable pin assignment | §4.2 | **`OI-CVNSHW-01`** |
| Connector selection and strain relief | Shared accessory-connector gap (`NP-HW-AUDIO-001` §8, `NP-HW-NASAL-001` §8, `NP-HW-VNSCLIP-001` §8) | **`OI-CVNSHW-08`** |
| Drive topology, and therefore the enable-line count | §4.3 — `OI-CVNS-09`'s hardware half | **`OI-CVNSHW-02`** |
| Whether A14 carries its own cardiac sensing | §5.2 | **`OI-CVNSHW-03`** |
| Everything in `NP-FW-CVNS-001` and `NP-REG-CVNS-001` | Those documents are BASELINED/active and own their content | — |

### 8.1 FAI readiness — and what changes for `NP-FAI-CVNS-001`

`NP-FAI-001` §2.1.1's verdict on A14 reads **"F1 fails absolutely — no mechanical or electrical
specification exists for the electrode assembly, cable or connector (GitHub #332)."** The clause
after the dash is what this document changes.

| | Before | Now |
|---|---|---|
| **F1** | Fails **absolutely** — no specification of any kind | **Still fails** — this document is DRAFT, and `NP-FAI-001` §2 F1 requires `BASELINED` or `ACTIVE` |
| **F2** | Nothing to dimension against | **Still fails** — but `REQ-CVNS-07`, `-08` and `REQ-CVNS-04`'s bounds are now *this artifact's* criteria rather than borrowed from a firmware test (`REQ-CVNS-12`) |
| **F3** | Not assessable | **Not assessable** — silicone over-moulding and hydrogel supply have no `NP-PROC-SUP-001` category |
| **F4** | RISK-25 held in `NP-RISK-002` §4 with no artifact register | **Unblocked** — §7 is the input list, and `NP-RISK-002` §4's stated reason for the hold is discharged |

> **`NP-FAI-CVNS-001` stays a named absence in `NP-ART-001` §3.2, as `NP-FAI-001` §2's closing rule
> requires.** `OI-FAI-07`'s wording — *"until A14 has a hardware specification"* — is satisfied in
> its letter and not in its substance: A14 now has one, and it is DRAFT with no dimensioned
> geometry. The item is re-scoped rather than closed, in `NP-FAI-001` Rev 3.

---

## 9. Open items

| ID | Description | Owner | Blocking |
|---|---|---|---|
| **OI-CVNSHW-01** | Assign the four cable conductors and confirm the assignment supports per-electrode 1 kHz impedance measurement and 500 ms single-electrode open detection. A change to the count is regulatory-visible (`NP-REG-CVNS-001` §2.4) | EE | A14 tooling; 510(k) device description |
| **OI-CVNSHW-02** | **Decide the drive topology — one source with a side selector, or two independent sources.** `OI-CVNS-09`'s single-vs-dual enable line follows from it, not the reverse, and each topology has a distinct Class C failure mode (§4.3) | EE + Safety | `OI-CVNS-09`; PCB layout (G1) |
| **OI-CVNSHW-03** | **State A13-worn-and-functioning as an explicit precondition of A14's safety case**, in `NP-REG-CVNS-001`'s device description, RISK-25's control set and the IFU — or give A14 its own cardiac sensing (§5.2). The dependency is reasonable; leaving it unwritten is not | Systems + Regulatory + Quality | RISK-25 control integrity; 510(k) |
| **OI-CVNSHW-04** | Specify and measure the electrode/pad wetted contact area with a tolerance, so `NP_CVNS_ELECTRODE_AREA_MCM2` stops being PROVISIONAL. Closes `OI-CHARGE-07`'s cervical limb. Note the area that matters is the **pad's**, and the pad is unspecified | ME + Clinical | `OI-CHARGE-07`; Class C safety argument |
| **OI-CVNSHW-05** | **Re-scoped Rev 2.** ~~No row in `docs/reference/commercial-model.md` §2.3~~ — row added, and trigger assessed, under `OI-ACC-03` (closed 2026-09-22). **Remaining:** single use is unenforced. The pad is not authenticated, and the only measurement on it — `REQ-CVNS-06`'s pre-enable impedance — refuses a pad that does not conduct, not one that has been used. Decide whether single use needs enforcement beyond the IFU, and by what means (§6) | Product + Quality | Consumable record completeness; reuse hazard (§7) |
| **OI-CVNSHW-06** | Specify neck module geometry, retention, mass and the neck-size range served | ME + HFE | A14 tooling; hazard analysis |
| **OI-CVNSHW-07** | Specify gel pad formulation, adhesion, shelf life and biocompatibility basis (single-use hydrogel on neck skin) | ME + Quality | Hazard analysis; 510(k) biocompatibility |
| **OI-CVNSHW-08** | Select connector and strain relief. Worth deciding **once** across A11, A12, A13 and A14 | EE + ME | A14 tooling |

> **On the `OI-CVNSHW-` prefix.** `OI-CVNS-08…11` (`NP-FW-CVNS-001` §14) and `OI-CVNS-HUB-11`
> (`NP-FW-HUB-001`) are in use. Open-item IDs are append-only and never renumbered
> (`NP-CONV-001` §6), so this document takes a distinct prefix rather than extending a firmware
> family with hardware items. `OI-CVNS-09` is cited here by its own name and is not adopted.

---

## 10. Cross-references

- **Artifact register row:** `docs/np_art_001.md` §2.4 (A14), §3.2
- **Firmware specification (BASELINED):** `docs/np_fw_cvns_001.md` — §9 is the FAI test procedure of record
- **Regulatory strategy:** `docs/np_reg_cvns_001.md` (GitHub #24)
- **The artifact this one depends on for R-peaks:** `docs/np_hw_vnsclip_001.md`
- **FAI issue conditions and OI-FAI-07:** `docs/np_fai_001.md` §2, §2.1.1
- **RISK-25:** `docs/np_risk_002.md` §3, §4
- **Charge ceiling derivation:** `docs/np_dt_001.md` §3.2.1 DI-SAFE-01a

---

## 11. Revision history

| Rev | Date | Author | Description |
|---|---|---|---|
| 1 | 2026-09-20 | NeurOne Systems Engineering | Initial release, against GitHub #332. **Gives artifact A14's hardware an owning specification for the first time** — the absence cited by name in `NP-ART-001` §2.4/§3.2, `NP-FAI-001` §2.1.1 and OI-FAI-07, and `NP-RISK-002` RISK-25. §2 reads `NP-REG-CVNS-001` §2.4's device description as a manufacturing list and states each sub-part's specification status; §3 carries eleven requirements from the record and marks the four that are **predicate-coupled** and cannot be relaxed without re-running the substantial-equivalence argument. **Principal finding (§5.2): the cardiac interlock's R-peak source is a different artifact** — `NP-FW-CVNS-001` §6.2 takes it from the auricular clip's PPG (A13), so the control that mitigates RISK-25 and is presented to FDA as the key differentiator from the gammaCore predicate depends on a T1 accessory being worn, whose skin contact is a consumable degraded by unrelated use. The dependency is reasonable; its absence from the device description, the risk controls and the IFU is not (`OI-CVNSHW-03`). Also recorded: `OI-CVNS-09`'s single-vs-dual enable line is **a hardware topology question and is re-seated here** (`OI-CVNSHW-02`); the 4-conductor cable has no pin assignment and its count is regulatory-visible (`OI-CVNSHW-01`); `NP_CVNS_ELECTRODE_AREA_MCM2` = 2 cm² is PROVISIONAL and never measured, and the area that matters is the **pad's** (`OI-CVNSHW-04`, closing `OI-CHARGE-07`'s cervical limb); and the cervical gel pad is a shipping T2 consumable with **no row in the consumables table at all** (`OI-CVNSHW-05`). §8.1 states the FAI consequence precisely: **`NP-FAI-CVNS-001` remains unwritable**, F1 failing now on DRAFT status rather than on absence, so `OI-FAI-07` is re-scoped rather than closed. **No constant, limit, criterion, clinical claim or locked decision changes; nothing in `NP-FW-CVNS-001` or `NP-REG-CVNS-001` is amended.** Raises OI-CVNSHW-01…08. |
| 2 | 2026-09-22 | NeurOne Systems Engineering | **`OI-CVNSHW-05` re-scoped, not closed.** Its commercial half — no row in the consumables table — is closed by `NP-ACC-PRIORITY-001` `OI-ACC-03`: `docs/reference/commercial-model.md` §2.3 now lists the pad as single use, price not set (`OI-COST-10`), with `REQ-CVNS-06`'s impedance check assessed as its trigger. §6 gains a note saying the pad is gated on contact, not on use count, and the item now carries only the enforcement question. **No requirement, constant or limit changed.** |
