# NP-ENV-001 Rev A — Environmental Envelopes: Survival/Warranty vs. Operating

**Program:** NeurOne chassis / systems
**Status:** DRAFT — corrects a conflation in the earlier specs: the original "60–110 °F, 0–100 % RH"
was written as if it were an operating range; it is re-designated here as the **survival/warranty
(non-degradation)** envelope, and a separate **operating** envelope framework is defined (per modality
→ per module → per protocol). Numbers marked *pending* await final material selection + component
datasheets + THERM-1a.
**Parent:** NP-HELMET-GEOM-001 (§0 environmental line now points here).
**Sources:** CLAUDE.md §3 (modalities), §4.2/§4.3/§4.5, §5.1; NP-THERM-BEZEL-001 / NP-THERM-CFD-001
(THERM-1a), NP-REQ-FANHEALTH-001 (SR-FAN), NP-FMEA-GEOM-001, IEC 60601-1 (42 °C applied part),
IEC 60068-2-14 (thermal cycling).
**Date:** 2026-07-21

---

## 1. Two independent envelopes (the correction)

| Envelope | Power state | Question it answers | Bound by |
|----------|-------------|---------------------|----------|
| **Survival / warranty** (non-degradation) | OFF — storage, transport, pre-use | Over what conditions do we warrant no *permanent* material degradation? | Material/bond durability (CTE-mismatch stress, Tg, freeze-thaw, corrosion) |
| **Operating** | ON — delivering therapy | Under what conditions can a given protocol run *safely and effectively*? | Modality + module active-component limits + self-heating headroom to 42 °C scalp |

**Operating ⊂ Survival, always** (operating adds self-heating on top of ambient), and the operating
envelope is **not single-valued** — see §3.

---

## 2. Survival / warranty (non-degradation) envelope

### 2.1 Spec

- **Current (re-designated):** 15.6–43.3 °C (60–110 °F), 0–100 % RH, non-condensing→condensing per §2.2.
- **Recommended widening:** **−20 to +60 °C** (qualify to **−30/+70 °C** for margin). Rationale: 60–110 °F
  is narrow for real logistics (trailers, tarmac, unheated warehouses see −20…+60 °C); widening protects
  the product in transit **and** in customer storage (hot car, cold garage), which packaging cannot.

### 2.2 Thermal-gradient specs (two, both material-derived)

The single "max gradient" splits into two distinct limits, both computed from the **final** material set:

- **Spatial ΔT across dissimilar-CTE bonded interfaces → thermal *stress*** (the real degradation driver:
  delamination, bond peel, cracking). Set by CTE mismatch × geometry vs. the weakest bond's peel/shear
  strength. Governing interface today: the **PDMS–PI optical-window bond** (75 nm SiO₂ interlayer,
  174–860 N/m peel); others: mu-metal/PETG laminate, Pd-polyester, CFRP, glass-filled PBT, LSR seals.
  **The 200-cycle IEC 60068-2-14 qual (PDMS-QUAL) is the anchor test** — its profile defines the survival
  range + spatial-gradient capability for the most fragile interface. *(Max allowable spatial ΔT: pending
  final CTE/adhesive selection.)*
- **Temporal rate dT/dt → thermal *shock* + condensation** (cold unit → warm humid room: fast surface
  warm-up, lagging interior, water on cold optics/electronics). Limits the allowable ramp during shipping
  transitions and drives the power-up warm-up/anti-condensation hold. *(Max dT/dt: pending final thermal
  mass + bond data.)*

### 2.3 Per-unit cost of widening to −20/+60 °C

**Key structural fact: there is no battery** (USB-C powered; only a 22 F hub supercap), so the usual
dominant cold/hot survival cost driver (Li-ion) is absent. The widening is therefore **mostly one-time
qualification NRE**, with a small per-unit BOM delta:

| Component | Why the range bites | Per-unit Δ (est.) |
|---|---|---|
| Structural adhesives at critical bonds | wider-temp grade (−40/+80) at PDMS/laminate joints | +$0.10–0.40 |
| **Mu-metal PETG laminate** (§4.3) | **PETG Tg ≈ 78 °C / HDT ≈ 70 °C — +60 °C sustained is marginal; may need PET/PI** | +$0.10–0.50 |
| Supercap grade | verify −40/+65 storage; likely already adequate | +$0–0.50 |
| LSR seals/gaskets | silicone good to −40/+200 — **no change** | $0 |
| PDMS / CFRP / glass-filled PBT | fine to −40 °C+/+120 °C+; bond qual is NRE not per-unit | $0 |
| **Non-EC configs (Core/Home) total** | | **≈ +$0.50–1.00 / unit** (< 0.3 % of the $405 BOM) |
| **EC lens (Premium/standalone SKU only)** | automotive-lineage stack likely already survives; spec to automotive storage qual (see §2.5) | **+$0–5, EC-SKU only** *(revised down from +$5–15)* |
| Hydrogel consumables (not device BOM) | freeze-thaw at −20 °C → humectant formulation or "do not freeze" guidance | +$0.20–1.00/pack or $0 |

**NRE (one-time, not per-unit):** re-run PDMS-QUAL + thermal-cycle quals at −30/+70; adhesive/EC
re-selection + coupons; fit-tolerance re-validation at extremes. Order tens of $k.

### 2.4 Widen-the-device vs. insulated-packaging tradeoff

Insulated/thermal-mass packaging is **~$1–4/shipment** (EPS/EPE) to **~$8–20/shipment** (phase-change /
vacuum panels) **+ weight/dimensional premium**, and protects **transit only**. Because the device delta
is so small (no battery), **widening the material spec is very likely cheaper over product life than
recurring packaging** and protects everywhere permanently. **Recommendation: widen the device to
−20/+60 °C; use packaging only for the freeze-sensitive consumables.** The two per-unit drivers (PETG
laminate, EC-lens stack) are resolved to an engineering determination in §2.5 (both favorable);
vendor-datasheet sign-off is the formal close.

### 2.5 Material determinations for the two per-unit drivers (OI-ENV-01/02 — engineering close; vendor datasheet = formal close)

**(1) Mu-metal PETG laminate — inadequate at +60/+70; switch the film.** PETG has Tg ≈ 78–81 °C,
HDT ≈ 64–70 °C, continuous service ~65–70 °C. A +60 °C survival spec **qualified to +70** sits at/above
PETG's HDT and near its Tg, so sustained exposure risks creep/relaxation of the thermoforming stress →
encapsulation distortion or PETG-to-mu-metal delamination (which would also break the corrosion seal the
encapsulation provides). **Determination: replace the encapsulation film with BoPET** (biaxially-oriented
PET, continuous service ~105–150 °C) — commodity-priced (≈ $0–0.30 Δ), clears +70 comfortably; **or thin
polyimide** where forming demands it (higher cost). Watch item: PETG was likely chosen for
thermoformability; BoPET forms less easily, so verify the encapsulation geometry is manufacturable in
BoPET (or use a formable higher-Tg copolymer). Residual: vendor datasheet + a reformed-coupon
thermal-cycle qual.

**(2) EC-lens stack — likely already adequate; cost revised down.** The spec already cites a "3–5 µm hard
coat over EC film, **standard in automotive EC mirrors**" — i.e., the stack is from the auto-dimming-mirror
lineage, which is routinely qualified to **−40/+85 °C storage**. So −20/+60 °C **survival** is very likely
met with **$0–5 Δ** by specifying the stack to an automotive storage qual — **not** the +$5–15 carried
conservatively (that applied only if the baseline were a cheap consumer smart-film rated ~0/+50). Separate
point: cold **switching** speed (ion mobility falls at low temp) is an **operating**-envelope limit already
captured as the EC low-temp bound in §3.2 — it is not a survival-damage mechanism. Residual: confirm the
actually-sourced stack's datasheet storage rating.

---

## 3. Operating envelope framework

### 3.1 The intersection rule

```
protocol_operating_envelope = ∩ over every module the protocol activates of:
    module_operating_envelope = ( ∩ over the module's modalities ) ∩ module_construction_limit
```

The **most-limiting** modality/module sets the protocol envelope. **Module construction matters
independently of modality** (the user's point): e.g., T1-C (1064 smart) adds on-module electronics
(ATtiny + FETs) limits the base T1-A lacks; T2-D (1170) adds a TEC. The whole envelope is ⊂ survival (§1).

### 3.2 First-pass modality drivers (numbers pending datasheets + THERM-1a)

| Modality | Pulls in the… | Driver | Class |
|----------|---------------|--------|-------|
| PBM 660/808 LED | **high-temp** bound | headroom to 42 °C scalp / 62 °C junction shrinks as ambient rises — **= THERM-1a output** | **Safety** (burn) |
| PBM 1064 smart (T1-C) | high-temp | above + on-module driver IC operating range | Safety + component |
| PBM 1170 laser + TEC (T2-D) | **high-temp (tightest)** | TEC heat-pumping is finite; high ambient → can't hold laser setpoint → drift/shutdown | Safety + efficacy |
| EEG hydrogel (ADS1299) | **low-humidity**, high-temp | hydrogel dry-out → impedance ↑ (**already impedance-monitored**); ADS operating range | Efficacy |
| BES/tACS, tDCS | (interlocked) | sweat/humidity shifts skin impedance — handled by charge-density + impedance interlocks | Safety (interlocked) |
| VNS/HRV (PPG clip) | — | wide; rarely limiting | Efficacy |
| Neural audio | — | wide; not limiting | Comfort |
| Visual stim (micro-LED goggles) | — (MPE is temp-independent) | LED operating range; safety is IEC 62471 MPE | Safety (optical) |
| EC lens | **low-temp** | bistable EC switching slows/stalls cold (failsafe clears to 75 %) | Efficacy/function |
| Active EMF cancel (fluxgate) | edges | fluxgate temp drift degrades cancellation accuracy | Efficacy (shielding) |
| **Shared electronics** (RT1062 / STM32G071 / eMMC / supercap) | **base envelope (all protocols)** | industrial-grade parts set the absolute floor/ceiling every protocol inherits | Safety/function |

**Reading:** the *high-temp* bound is set by the thermal modalities (PBM, and 1170 TEC tightest) and is an
**output of the THERM-1a CFD** (which sweeps ambient) — so we are not deriving it from scratch. The
*low-humidity* bound is an EEG-hydrogel efficacy limit, already sensed via **electrode impedance** (dry gel
→ high impedance → existing app prompt). The *low-temp* bound is mainly the EC lens.

---

## 4. Gating strategy — recommendation: **Hybrid**

**Split protocol gating by consequence:**

- **Safety-critical limits** (scalp burn, component damage) → **hard-gate, on-device, non-dismissible.**
  Lives in the **Class C** thermal-safety domain already being built (junction NTC + SR-FAN), fed by sensed
  ambient. This is the *same* computation as THERM-1a: at a given ambient, is the protocol's PBM/laser duty
  within the 42 °C-scalp envelope? If not, the protocol is blocked.
- **Efficacy/comfort limits** (EEG signal quality, EC switching speed, mild dose reduction) → **soft:**
  display the protocol's safe operating range + a **dismissible** warning; let the user proceed.

**Why hybrid over the pure options:**
- Mirrors the **existing reminder tiering** (CLAUDE.md §5.2: safety-critical = non-dismissible, comfort =
  snoozable) — architectural consistency, not a new paradigm.
- **Autonomous Mode 3** (no phone): the hard safety gate must run on-device from sensed conditions; the
  soft/informational layer is app-side. Hybrid allocates cleanly: safety = firmware, info = app.
- **Fail-safe degradation:** absent/failed ambient sensing → fall back to the most conservative envelope
  (protocol evaluated at worst-case ambient), consistent with the SR-FAN-06 fail-safe pattern.
- Pure sensor-gate is needlessly hard on efficacy-only limits (frustrates users near a boundary); pure
  informational is too weak for a device with real thermal-burn potential.

---

## 5. Humidity — recommendation: **temperature-gated; humidity survival-only (no live RH sensor)**

- **Temperature sensing is near-free:** reuse the per-zone/hub NTCs read at **session start** (before
  self-heating), or add a dedicated ambient NTC (~$0.05–0.20). A humidity sensor is a real **+$1–3 BOM** +
  a part to place and seal.
- **The humidity-sensitive operating items are already covered without a live RH sensor:**
  - EEG hydrogel dry-out → **already sensed via electrode impedance** (dry gel → high impedance → existing
    prompt). Impedance is a *better* proxy than ambient RH because it measures the actual contact.
  - Optics/electronics condensation → a **transient / thermal-shock** phenomenon (§2.2 dT/dt), handled by a
    power-up warm-up hold + sealing, **not** a steady-state RH limit.
- So a live RH gate buys little that impedance + the dT/dt spec don't already provide. **Keep humidity in
  the survival/storage spec** (sealing, hydration caps, condensation guidance).
- **Revisit triggers** (would flip this to add a sensor): a future modality with a genuine *hard*
  humidity-dependent **safety** limit; or evidence that impedance monitoring is insufficient for hydrogel
  state; or a condensation failure mode not caught by the warm-up hold.

---

## 6. Open items

| ID | Description | Owner |
|----|-------------|-------|
| OI-ENV-01 | **Engineering-closed (§2.5): PETG inadequate at +70 → switch to BoPET (≈$0–0.30) / PI.** Residual: vendor datasheet + reformed-coupon thermal-cycle qual; verify BoPET formability of the encapsulation geometry | ME + Materials |
| OI-ENV-02 | **Engineering-closed (§2.5): automotive-lineage EC stack survives −20/+60 if spec'd to automotive storage qual (−40/+85); Δ revised to $0–5, EC-SKU only.** Residual: confirm sourced-stack datasheet | EE + ME |
| OI-ENV-03 | Compute max spatial ΔT (stress) + max dT/dt (shock) from final materials; set the survival-gradient spec + power-up warm-up hold | Thermal + Materials |
| OI-ENV-04 | **Working spec drafted → NP-ENV-OPRANGE-001** (per-modality/module/protocol table, first-pass numbers). Residual: replace † bounds with THERM-1a C3/C4, ‡ bounds with datasheets (OI-OPR-01…05) | Systems + Thermal |
| OI-ENV-05 | Ambient-sensing source decision: reuse NTCs at session start vs. dedicated ambient NTC | EE |
| OI-ENV-06 | Freeze-tolerant hydrogel formulation vs. "do not freeze" packaging for consumables | Consumables |

## 7. Cross-references

NP-HELMET-GEOM-001 · NP-THERM-BEZEL-001 / NP-THERM-CFD-001 (THERM-1a → high-temp operating bound) ·
NP-REQ-FANHEALTH-001 (SR-FAN, Class C gate) · NP-FMEA-GEOM-001 · CLAUDE.md §3/§4.2/§4.3/§4.5/§5.1/§5.2 ·
IEC 60601-1 · IEC 60068-2-14.
