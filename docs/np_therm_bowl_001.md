# Outer-Bowl Heat Budget After the Layer 4 Deletion — Bowl and Coil Temperature, and the Helmholtz Transfer Function

**Project:** NeurOne
**Document:** NP-THERM-BOWL-001
**Revision:** 2
**Date:** 2026-09-23
**Status:** DESIGN STUDY — answers `NP-EMC-CAV-001` `OI-EMCCAV-07` (GitHub #403). A resistance-network analysis on the model of record (`NP-THERM-SINK-001`); **asserts no measurement.** `EMF-1` has never run, and every dB figure used here is a design target.
**Effective Date:** —
**Author:** NeurOne Thermal / Systems Engineering
**Approved By:** — (pending design review)
**References:** CLAUDE.md §4.2 (42 °C / 62 °C interlocks), §4.3 (4-layer passive + active; combined 35–45 dB ELF), §4.5 (T1 peak ~45–50 W, T2 peak ~70–74 W); `docs/reference/hardware-detail.md` §4.3 (D1, D2), §4.5; `NP-EMC-CAV-001` §8.2 (the 0.410 → 0.335 path), §9 item 6, §10 (`OI-EMCCAV-07`); `NP-THERM-COOL-001` Rev 13 (banner, §2 the outward allocation, §4 the via split, `OI-THCOOL-21`); `NP-THERM-SINK-001` Rev 1 (§3.1 the solid-stack decomposition, §4 `SPEC-SINK-01…04`, §5 the refined network, §6 the idle baseline, §8.1 the 31.4 W aggregate); `NP-PWR-THERM-001` §3 (the T1-peak row is not thermally spendable); `NP-HW-HEXTILE-001` §9.1 (6–8 W non-PBM overhead); `NP-PWRSRC-001` §4.4 (the hub heat row); `NP-ENV-OPRANGE-001` §2 (active EMF cancel: −10 / +50 °C, SOFT); `NP-HEX-ZM-001` §5.3.1 (coils on the outer bowl, reasons 2 and 4); `NP-DRV-SHELL-002` §9.5 (`REQ-EMI-05`, `REQ-EMI-11`); `docs/reference/durability-maintenance.md` (fluxgate session-start nulling); `NP-HELMET-GEOM-001` §4 (Helmholtz formers, fixed geometry); IEC 60028 (annealed copper, α₂₀ = 0.00393 /K); `scripts/check-thermal-bowl.ts`; `scripts/check-thermal-sink.ts`
**Related Issues:** GitHub #403 (this item); GitHub #391 (the decision, `REQ-CAV-04`); GitHub #407 (`OI-THCOOL-21`, the parallel re-baseline)
**Gate:** No gate. Answers `OI-EMCCAV-07`; raises `OI-THBOWL-01…05`, none BLOCKING.
**IEC 62304 Class:** — (analysis document; no code ships from it)
**Supersedes:** None — new document
**Parent Document:** NP-EMC-CAV-001 §10 (`OI-EMCCAV-07`)

---

> **⚠ READ FIRST — the one-paragraph version.**
>
> `OI-EMCCAV-07` feared that deleting Layer 4 would push heat into the outer bowl, beside the mu-metal
> and the Helmholtz coils. **On the network of record it does not, measurably:** the BN-boss via
> already lands ~90 % of tile heat on the outer bowl, and it sits **in parallel** with the station,
> so taking 0.410 → 0.335 moves the bowl by **0.002 K** at equal drive and buys **< 0.01 W** of
> admissible drive. The bowl's temperature is set by the **external film and the face-42 °C
> governor**, not by the station. At the CLAUDE.md §4.5 peaks taken literally, the coils reach
> **44–46 °C (T1) and 53–54 °C (T2)**, but every one of those rows puts the face past 42 °C, so the
> interlocks throttle first. **The steady-state coil maximum the device can actually hold is ~42 °C
> at +25 °C *and* at +35 °C ambient, and ~43 °C at the +50 °C top of the SOFT envelope** with PBM
> blocked. **The real problem is not the deletion. It is that the drift question turns on two things
> the record never states:** the coil **drive topology**, and the **temperature at which
> `REQ-EMI-11` calibrates**. Under **voltage-mode** drive, copper moves the gain at 0.39 %/K. One
> session then drifts **3.9 %**, which caps the feed-forward path at **28 dB**. Calibrated cold and
> used hot across the envelope, it drifts **19.5 %**, a **14 dB** ceiling, below the ~20 dB active share
> the 35–45 dB target needs. `REQ-EMI-11` recalibrates on insertion changes only, so the cold case
> can be reached. Under **current-mode** drive the resistance drops out at first order and what
> remains is **+24 % compliance headroom** and the mu-metal's own temperature coefficient. **Both
> are raised as open items (`OI-THBOWL-01`, `-02`), not designed.**

---

## 1. Headline

1. **The deletion is invisible to the bowl (§2).** Junction → bowl runs through the via (0.0059
   m²K/W) in parallel with gap + station (0.295 → 0.235). The station changes that parallel pair by
   < 1 %. Bowl-mean shift at 20 W distributed: **+0.002 K**, in both allocations. Admissible-drive
   gain at 25 °C: **+0.005 W (SPEC) / +0.009 W (COOL)**.
2. **The bowl is bounded by the safety ceiling, not by the supply (§3).** The literal peaks are
   inadmissible (face 44–54 °C). Under the face-42 °C governor the bowl reaches **41.3–41.9 °C** and
   the coil **42.1–42.2 °C**, at 25 and at 35 °C ambient, before and after the deletion alike.
3. **The coil-temperature span is set by the ambient envelope, not by PBM (§4).** It runs from
   **−10 °C** (off-head at the SOFT low bound) to **43.1 °C** (+50 °C ambient, PBM blocked), plus
   **+0.6 K per 0.5 W** of coil self-dissipation, which nothing in the record specifies. Copper:
   **R(hot)/R(cold) = 1.237–1.247**; R(hot)/R(25 °C idle) = **1.044**.
4. **Drift is bounded only once the drive topology is known (§5).** Voltage mode: one session
   **3.9 % → 28.2 dB**, hot room **4.2 % → 27.5 dB**, envelope **19.5 % → 14.2 dB**, against an active
   share of **10–30 dB**. Current mode: first-order immune; needs **+24 %** compliance and a μ(T) figure.
5. **Against `NP-ENV-OPRANGE-001` §2 and the recalibration scheme (§6).** The SOFT designation
   survives, because L2 is always present (D2) and a degraded active loop is an efficacy loss, not
   a hazard. But the envelope is stated in **ambient** and the coil runs at ambient **plus** the bowl
   rise, and `REQ-EMI-11` has **no temperature trigger**. So nothing in the record ties the calibration
   to the temperature it is used at. **`OI-THBOWL-02`.**

---

## 2. Where the heat goes — the via makes the bowl deaf to the station

`NP-THERM-SINK-001` §5's network routes each socket's junction heat three ways: **inward** to the
perfused core (0.11), **through the via** to the outer bowl's skin node X (0.0059), and **through the
cavity**, gap then station then shell, to the same node X. X then rejects through the external film.
**The via and the cavity leg are parallel paths between the same two nodes**, and the via is fifty
times less resistive. That is why `NP-THERM-COOL-001` §4 finds only **~2 %** of heat in the cavity
once the via is fitted.

Two allocations of R_OUT_BASE = 0.41 are in the record and disagree on the foam by 0.015 m²K/W. Both
are carried here (`bun scripts/check-thermal-bowl.ts` §1):

| Allocation | C → X | film | R_out | R_sink aggregate |
|---|---:|---:|---:|---:|
| `NP-THERM-COOL-001` §2, absorber in | 0.080 | 0.100 | **0.410** | 0.90 K/W |
| `NP-THERM-COOL-001` §2, **deleted + re-loft** | 0.005 | 0.100 | **0.335** ← the figure #403 names | 0.90 K/W |
| `SPEC-SINK-01`, absorber in | 0.065 | 0.120 | 0.415 | 1.08 K/W |
| `SPEC-SINK-01`, **deleted + re-loft** | 0.005 | 0.120 | **0.355** | 1.08 K/W |

The COOL allocation takes the foam at 0.075 (k 0.04); `NP-THERM-SINK-001` §3.1 takes it at 0.060
(k 0.05) and fixes the film from a correlation rather than by balance. **For the bowl, the larger film
is the conservative case**, so the SPEC allocation bounds every temperature below. Reconciling the two
foam figures is historical now that the foam is gone. The film is not, and it is `OI-THCOOL-21`'s.

**At equal drive** (20 W emitter-electrical, fully distributed, 25 °C) the bowl mean is **36.5 °C
(COOL) / 37.7 °C (SPEC)** before and after the deletion. The shift is **+0.002 K**. The admissible
drive moves by **+0.005–0.009 W**.

> **A hand-off, not a finding against the deletion.** `REQ-CAV-04` was taken because the layer does
> not do its RF job (`NP-EMC-CAV-001` §6). That reason is untouched here. But `OI-THCOOL-21`'s premise, that every
> capability figure carried at 0.410 is *understated*, is true only of the **no-via** models
> (`NP-THERM-COOL-001` §5's 1/R_out cavity ceiling, R1's BASE row). **On the via network of record the
> deletion buys essentially nothing, thermally.** `OI-THBOWL-05` routes that to #407.

---

## 3. Bowl and coil temperatures at the §4.5 peaks

### 3.1 Literal — the question as asked

Peak draw less the **7 W** non-PBM overhead (`NP-HW-HEXTILE-001` §9.1, 6–8 W, sited in the hub per
`NP-PWRSRC-001` §4.4), all to emitters, fully distributed, 25 °C, SPEC-post, bare bowl. Heat at the
model's η_wp = 0.35 (the light leaves into the wearer); η = 0 bracket (everything becomes heat in the
helmet). The **coil** figure charges the heat crossing the 0.005 m²K/W shell solid, because the coils
sit on the bowl's **inner** face.

| Row | Emitter W | Heat W | Face max | Bowl max | **Coil max** | η = 0: bowl / coil |
|---|---:|---:|---:|---:|---:|---:|
| T1 peak 45 W | 38.0 | 24.7 | 44.2 | 43.6 | **44.4** | 50.3 / 51.4 |
| T1 peak 50 W | 43.0 | 27.9 | 45.8 | 45.2 | **46.1** | 52.9 / 54.0 |
| T2 peak 70 W | 63.0 | 41.0 | 52.4 | 51.8 | **53.0** | 63.0 / 64.6 |
| T2 peak 74 W | 67.0 | 43.6 | 53.7 | 53.1 | **54.3** | 65.0 / 66.7 |

**Every row puts the face past 42 °C.** The per-zone NTC throttle and the Path B1 face NTC act first
(CLAUDE.md §4.2), so these are **upper bounds the device cannot hold in steady state.** That matches
`NP-PWR-THERM-001` §3's finding that the T1-peak row is an electrical peak the assembly cannot spend
thermally. The T2 rows also hold the 1170 nm laser zone's heat, which has its own TEC path
(`OI-PWR-05`), so spreading it uniformly over the lattice is itself an assumption.

### 3.2 Admissible — what the governor lets the bowl reach

The largest fully-distributed drive that holds the face at 42 °C. The face ceiling applies to both
tiers, so this is the **steady-state bowl maximum under PBM on T1 or T2**:

| Allocation | Ambient | Admissible W | Bowl max | Bowl mean | **Coil max** |
|---|---:|---:|---:|---:|---:|
| SPEC, pre = post, bare or S3 | 25 °C | 31.4 | 41.4 | 41.4 | **42.1** |
| SPEC, pre = post, bare or S3 | 35 °C | 17.9 | 41.9 | 41.9 | **42.2** |
| COOL, pre = post, bare or S3 | 25 °C | 35.9 | 41.3 | 41.3 | **42.1** |
| COOL, pre = post, bare or S3 | 35 °C | 19.8 | 41.8 | 41.8 | **42.2** |

The first row reproduces `NP-THERM-SINK-001` §8.1's **31.4 W**, which anchors the model. Three
observations:

- **The bowl sits ~0.5 K under the face at the ceiling.** The via ties X to the junction, so a
  face-limited junction is also a limited bowl. **The governor that protects the scalp also bounds
  the coils.**
- **Ambient barely moves it** (42.1 → 42.2 °C). The admissible drive falls as ambient rises, which
  holds the bowl near the face limit either way.
- **Uniform loading makes the spreader irrelevant here.** S3 matters for concentrated montages
  (`NP-THERM-SINK-001` §7). Under a concentrated montage the local bowl above the driven tiles runs
  hotter than the mean, but the admissible watts fall with it. An enclosing coil's resistance follows
  the mean temperature along its winding, so the uniform case is the right one for the transfer
  function.

---

## 4. Coil temperature span, and the resistance change

`NP-ENV-OPRANGE-001` §2 bounds active EMF cancellation on **ambient**, −10 / +50 °C, SOFT. The coil
runs at ambient **plus** the bowl rise, so the envelope never states what coil temperature it
tolerates. SPEC-post, steady state (`check-thermal-bowl.ts` §4):

| State | Coil °C |
|---|---:|
| Off-head, unpowered, at the −10 °C SOFT low bound | −10.0 |
| On-head, idle, 25 °C (the wearer's 5.6 W into the shell, `NP-THERM-SINK-001` §6) | 31.4 |
| Admissible PBM at the +35 °C block | 42.2 |
| +50 °C ambient, PBM blocked, head below ambient | **43.1** |

**Coil self-dissipation is not specified anywhere.** No conductor, turn count, drive current or
resistance is recorded, and `NP-PWRSRC-001` §4.4 books "Helmholtz coils" under the **hub** heat row
while `NP-HEX-ZM-001` §5.3.1 puts the coils on the **outer bowl**, so their I²R lands on the bowl
whatever the drivers dissipate (`OI-THBOWL-04`). The table below sweeps it as a uniform dissipation on
the bowl, charged to the film alone (an upper bound):

| P_coil | Hottest coil | R(hot)/R(cold) | R(hot)/R(25 °C idle) |
|---:|---:|---:|---:|
| 0 W | 43.1 °C | 1.237 | 1.044 |
| 0.5 W | 43.7 °C | 1.239 | 1.044 |
| 1.0 W | 44.2 °C | 1.242 | 1.044 |
| 2.0 W | 45.4 °C | 1.247 | 1.044 |

**Copper is assumed.** The task and the physics both point to it, but the record names no conductor.
α₂₀ = 0.00393 /K (IEC 60028, annealed copper). An aluminium winding would move these figures by
under 3 %.

---

## 5. Drift of the `REQ-EMI-11` transfer function, and what it costs

### 5.1 It depends on the drive topology, which is unstated

- **Voltage-mode drive** (field ∝ V/|Z|): the gain moves as R(cal)/R(use). The resistive limit
  (ωL ≪ R) is the worst case at ELF, because |d ln Z / d ln R| = R²/|Z|² ≤ 1.
- **Current-mode drive** (field ∝ I): resistance **drops out at first order**. What remains is
  **compliance headroom**: the driver must deliver I·R(hot), **24.2 % above** I·R at the cold end.
  Two second-order terms remain that this analysis cannot bound. One is the **mu-metal's permeability
  versus temperature**, which D1 makes part of the transfer function: *"the calibrated quantity… is
  the coil-drive → field transfer function, defined WITH the mu-metal present"*. The other is former
  expansion, which is below 0.01 %/K for CFRP or PETG and immaterial.

### 5.2 What a gain error costs

The cancellation has two paths, and they are not equally exposed:

- **A fluxgate-closed feedback loop** sees an actuator gain error divided by its loop gain, so it is
  largely immune.
- **`REQ-EMI-05`'s feed-forward self-field subtraction** and CLAUDE.md §4.3's *"synchronous Helmholtz
  subtraction from EEG"* use the **calibrated** transfer directly and take the error in full. The
  residual is |ε| of the field being subtracted, so the path's ceiling is **−20 log₁₀|ε|**.

**What that ceiling must clear.** The ceiling must clear the active loop's share of the ELF target.
CLAUDE.md §4.3 gives the combined target as 35–45 dB with L2 at 15–25 dB. That leaves the active loop
**10 dB (35 − 25) to 30 dB (45 − 15), ~20 dB mid.** These are design targets; `EMF-1` has never run.
This is a **derivation from a stated target, not a new requirement** (CLAUDE.md §18). No figure below
is written as a "shall".

### 5.3 The numbers — voltage mode, P_coil = 1 W at the hot end

| Case | T_cal | T_use | ε | Feed-forward ceiling |
|---|---:|---:|---:|---:|
| One session: calibrated at 25 °C idle, runs to admissible PBM at 25 °C | 32.5 | 43.3 | −3.9 % | **28.2 dB** |
| Hot room: calibrated at 25 °C idle, used at +50 °C (PBM blocked) | 32.5 | 44.2 | −4.2 % | **27.5 dB** |
| **Envelope: calibrated cold (−10 °C), used at the hottest state** | −10.0 | 44.2 | **−19.5 %** | **14.2 dB** |

Calibration-to-use coil ΔT tolerated under voltage drive: **8.5 K for 30 dB · 28.8 K for 20 dB ·
120 K for 10 dB.**

**Reading it.** A voltage-driven feed-forward path keeps the **mid** share (~20 dB) through any single
session and any hot room, but not the **top** of the band (30 dB), because one session's rise already
exceeds 8.5 K. **Across the envelope it falls below the mid share**, and whether that case can occur
is the recalibration question in §6.

---

## 6. Against `NP-ENV-OPRANGE-001` §2 and the recalibration scheme

**The SOFT designation survives, and it is right.** Active cancellation is an **Efficacy(shield)**
bound. L2 is always present (D2; the Layer 4 deletion never touched the ELF fallback, as the §2 row
already records). A degraded active loop costs EEG signal quality, not wearer safety. Nothing here
makes it a HARD bound, and `NP-ENV-001`'s operating range is not reopened.

**What the envelope and the scheme do not do:**

1. **The envelope is stated in ambient.** The coil runs at ambient plus a bowl rise of **~6 K idle to
   ~17 K at the face ceiling**. The row's −10 / +50 °C therefore implies a **coil** span of about
   −10 to +44 °C, which is stated nowhere.
2. **`REQ-EMI-11` has no temperature trigger.** It recalibrates on `np_module_map` rebuild, an
   **insertion change**. The session-start step in `durability-maintenance.md` zeroes fluxgate
   **offset**, not actuator gain. The calibration temperature is therefore whatever the bowl was at
   the last insertion, which can be a cold room months earlier. **The "envelope" row of §5.3 is
   reachable.**
3. **Whether that matters is decided by `OI-THBOWL-01`.** Under current-mode drive it mostly does
   not; under voltage mode it caps the feed-forward path at 14 dB. **A compensation or a temperature
   trigger may be needed. This document raises that need (`OI-THBOWL-02`) and does not design it.**

**Verdict on #403's "Done when":** outer-bowl and coil temperatures are estimated at both peaks with
the 0.335 path (§3). The drift is **bounded, conditional on the drive topology**: ≤ 4.2 % within any
session or room, ≤ 19.5 % across the envelope under voltage drive, and first-order nil under current
drive. **The recalibration need is raised** (`OI-THBOWL-02`).

---

## 7. What this document does not establish

1. **It measures nothing.** `EMF-1`, `OI-R1-01` (mesh-independent CFD) and `OI-R1-02` (THERM-1b
   correlation) are open. Every temperature is a network output; every dB figure is a target.
2. **Steady state is the bound, not the trajectory.** The bowl's time constant is not computed. Steady
   state is the maximum any session can reach, which is what the drift bound needs. When within a
   session the drift accrues is not established.
3. **The coil's self-heat is swept, not known** (`OI-THBOWL-04`).
4. **The mu-metal's μ(T) is not bounded** (`OI-THBOWL-03`). Under current-mode drive it may be the
   dominant term.
5. **`OI-THCOOL-21` / #407 had not merged to main** when this was computed. The 0.335 case is built
   locally on `NP-THERM-SINK-001`'s constants (an optional C → X override added to
   `check-thermal-sink.ts`, default unchanged). **#407 will need to reconcile its re-baselined
   constants with these** (`OI-THBOWL-05`).

---

## 8. Open items

| ID | Item | Owner | Blocking? |
|---|---|---|---|
| **OI-THBOWL-01** | **Specify the Helmholtz coil drive topology (voltage- or current-mode) and the conductor.** Copper's 0.39 %/K enters the `REQ-EMI-11` transfer function at first order under voltage drive (§5.3: ≤ 19.5 % across the envelope) and not at all under current drive, which instead needs **≥ 24 % compliance headroom** over a cold-calibrated I·R. The record states neither the topology nor the conductor, turns or resistance | EE Lead | No, but it decides whether `OI-THBOWL-02` is live |
| **OI-THBOWL-02** | **`REQ-EMI-11` recalibrates on insertion change only, and `NP-ENV-OPRANGE-001` §2 bounds cancellation on ambient, not coil temperature.** Under voltage drive, calibration at −10 °C and use at the hottest state (§4) caps the feed-forward path (`REQ-EMI-05`, EEG subtraction) at **14.2 dB**, against a ~20 dB mid active share. **A compensation or a temperature trigger may be needed. Raised, not designed.** Options belong to the owner, e.g. a coil-temperature term in the calibration, or a bowl-ΔT recalibration trigger. Measured resistance is also an option, since a current-mode driver already measures it | EE Lead + FW | No, conditional on `OI-THBOWL-01` |
| **OI-THBOWL-03** | **Bound the mu-metal's permeability versus temperature, and its share of the coil-drive → field transfer** (D1). Under current-mode drive it is likely the dominant residual term. A bench measurement on the `EMF-1` fixture across the §4 span | EMC (`EMF-1`) | No |
| **OI-THBOWL-04** | **Coil self-dissipation is unspecified, and the heat budget books it in the wrong place.** `NP-PWRSRC-001` §4.4 lists "Helmholtz coils" in the **hub** enclosure row, while `NP-HEX-ZM-001` §5.3.1 puts the coils on the **outer bowl**. The drivers may sit in the hub; the coil I²R lands on the bowl. Here it is swept at 0–2 W (+1.2 K per W on the coil) | EE + Thermal | No |
| **OI-THBOWL-05** | **✅ ANSWERED 2026-09-23 by #407 (`NP-THERM-COOL-001` Rev 16, `NP-THERM-SINK-001` Rev 3).** (a) Agreed. (b) The current path is **0.335** (the decision record's k 0.04 foam); SINK §3.1's k 0.05 foam gives **0.350** on R1's film, and `check-thermal-sink.ts --validate` now asserts the 0.015 is exactly that k difference. (c) Reconciled: `check-thermal-bowl.ts` imports the COOL constants from `scripts/thermal-outward-path.ts` and the SPEC allocations from the sink model's explicit as-was exports; **every figure in this document reproduces unchanged**. **One caveat #407 found and this document inherits:** its "post" cases hold R1's 12 mm exterior, but the binding 3 mm re-loft shrinks the exterior 4.6 % (`SPEC-SINK-01` 1.08 → 1.14 K/W, `NP-THERM-SINK-001` §3a), which warms the skin — so the bowl and coil temperatures here are slight **under**-estimates for the re-lofted design (≈ +0.2 K on the skin at N = 6 on the library floor). Routed with `OI-SINK-10`. *Original text:* **Hand-off to `OI-THCOOL-21` / GitHub #407.** (a) On `NP-THERM-SINK-001`'s via network the deletion moves the bowl **0.002 K** and the admissible drive **< 0.01 W**, so "every capability figure is understated" holds only for the no-via models. (b) The record carries the foam at **0.060** (`NP-THERM-SINK-001` §3.1) and at **0.075** (`NP-THERM-COOL-001` §2), so the post-deletion path reads **0.355** or **0.335**. (c) Reconcile #407's re-baselined constants with `check-thermal-bowl.ts` | Thermal | No |

---

## 9. Reproduction

```bash
bun scripts/check-thermal-bowl.ts             # full report, §0–§5
bun scripts/check-thermal-bowl.ts --validate  # anchors only, exit 1 on drift
```

Anchors checked: R_out 0.410 / 0.335 (`NP-THERM-COOL-001` §2, `NP-EMC-CAV-001` §8.2); `SPEC-SINK-01`
1.08 K/W; `NP-THERM-SINK-001` §3.1 solid stack 0.065; the C → X override reproduces the sink model
exactly at its default; `NP-THERM-SINK-001` §8.1's 31.4 W and §6's 31.1 °C idle skin; copper
α₂₀ 0.00393. `CI-Kind: report`: the script prints a model and is not run by a workflow.

---

## 10. Revision history

| Rev | Date | Author | Change |
|-----|------|--------|--------|
| 1 | 2026-09-23 | NeurOne Thermal / Systems Engineering | **Initial release, answering `NP-EMC-CAV-001` `OI-EMCCAV-07` (GitHub #403).** Estimates outer-bowl and Helmholtz-coil temperatures after the Layer 4 deletion (`REQ-CAV-04`), at the CLAUDE.md §4.5 T1 and T2 peaks, on the 0.335 path. **(1) The deletion is invisible to the bowl.** The BN-boss via runs parallel to the station between junction and bowl, so the change moves the bowl 0.002 K and the admissible drive by < 0.01 W. **(2) The literal peaks are inadmissible** (face 44–54 °C; coil 44–46 °C T1, 53–54 °C T2). **Under the face-42 °C governor the coil holds ~42 °C at 25 and 35 °C ambient**, and **43.1 °C at the +50 °C SOFT bound** with PBM blocked. **(3) The drift turns on two unstated things**, the coil drive topology and `REQ-EMI-11`'s calibration temperature. Voltage mode gives 3.9 % per session (28.2 dB feed-forward ceiling) and 19.5 % across the envelope (14.2 dB); current mode is first-order immune with +24 % compliance headroom. Raises `OI-THBOWL-01…05`, none blocking. Adds `scripts/check-thermal-bowl.ts` (`CI-Kind: report`, 8 anchors) and an optional C → X override in `scripts/check-thermal-sink.ts` (default unchanged, verified exact). **No requirement created; no locked section modified; no measurement asserted.** |
| 2 | 2026-09-23 | NeurOne Thermal / Systems Engineering | **`OI-THBOWL-05` answered by `OI-THCOOL-21` (GitHub #407); no figure changes.** `scripts/check-thermal-bowl.ts` reconciled to the shared `scripts/thermal-outward-path.ts` and to `check-thermal-sink.ts`'s explicit as-was exports — its report is byte-identical to Rev 1's. The 0.335 / 0.355 question: the current path is 0.335; SINK's k 0.05 foam gives 0.350 on R1's film (asserted). **Caveat inherited:** the "post" cases hold R1's exterior; the re-loft's 4.6 % smaller exterior makes the bowl and coil figures slight under-estimates (`OI-SINK-10`). |
