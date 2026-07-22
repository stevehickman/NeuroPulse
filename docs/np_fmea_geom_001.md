# NP-FMEA-GEOM-001 Rev A — Helmet Layer-Stack Hardware FMEA

**Program:** NeurOne chassis / mechanical stack
**Status:** DRAFT — first hardware FMEA for the four-station layer stack. Complements the
**software** FMEA (NP-FMEA-001, SW-01 Safety MCU) and the system risk register
(NP-RISK-001, RISK-01…25). Uses the NP-RM-001 §4 severity/probability/acceptability scales.
**Parent:** `docs/np_helmet_geom_001.md` (this closes its §8 **FMEA-RECON** gate)
**Sources:** `docs/np_fmea_001.md` §2 (scales), `docs/np_rm_001.md` §4/§7 (RISK register,
RISK-14/15/20/25), `docs/np_therm_bezel_001.md` (THERM-1 coupling), `CLAUDE.md` §4.2/§4.3.
**Date:** 2026-07-21

---

## 1. Why this document exists (the FMEA-RECON finding)

The parent brief §6 carried **design-stage MTBF estimates** with an ad-hoc `$/$$/$$$` cost tag.
Reconciling them against the repo's FMEA revealed a scope gap, not a numbers mismatch:

- **NP-FMEA-001 covers only SW-01 firmware** (IEC 62304 Class C). Its §1.2 explicitly excludes
  "hardware-level failure modes outside firmware behaviour (covered in NP-RISK-001)."
- **NP-RISK-001** (RISK-01…25) is the system risk register, but it predates the per-module hex /
  layer-stack redesign and has **no failure-mode-level entries for the L0–L3 mechanical stack**.
- So there is **no hardware FMEA to merge into.** The correct reconciliation is to (a) re-express
  the parent §6 failure modes in the shared NP-RM-001 S×P framework, (b) cross-reference each to the
  existing RISK IDs and software backstops, and (c) hand the quantitative MTBF/L10 numbers to the
  reliability tests that actually produce them (they are not assertable at design stage).

**MTBF vs. FMEA — the honest split.** An FMEA gives *risk* (severity × probability) now; a *number*
(hours, L10 cycles) comes only from reliability testing. This document supplies the S×P risk and
**names the test that will yield each MTBF** (§4 "MTBF source"), replacing the parent's estimates.

Scales (from NP-RM-001 §4 / NP-FMEA-001 §2): S1–S5, P1–P5; **≤4 ACCEPTABLE · 5–9 ALARP ·
≥10 UNACCEPTABLE**.

---

## 2. Headline: one new, un-interlocked failure mode

**FMEA-G07-01 — outward thermal-path (fan/vent) degradation diverts heat to the scalp while the
junction NTC still reads safe.** The existing thermal interlock (NP-FMEA-001 FMEA-M04) throttles on
**junction** temperature at 62 °C. THERM-1 (np_therm_bezel_001 §4) established that scalp safety is a
**face-temperature** limit (~44 °C at a 0.6 mm bezel), and that the split between scalp-bound and
outward-bound heat depends on forced convection. If the fan/vents foul, outward resistance rises, a
larger fraction of module heat is forced scalp-ward — and the junction can still sit at or below
62 °C, so **no throttle fires** even as the scalp approaches unsafe temperature. This is a genuine
gap between the two analyses and is the most important output of FMEA-RECON. Mitigation is proposed
in §4 (G07) and raised as **OI-GEOM-FMEA-01**.

---

## 3. Method note

One "module" per layer station, mirroring NP-FMEA-001's module-by-module structure:
G01 L0 optical window · G02 L0 electrode face + cap · G03 L0 gap seal / plugs · G04 L1 socket-layer
structure · G05 L1 clamps/latches + ground bond · G06 L2/L3 outer bowl (EMF+shell) · **G07 thermal
path** (spans L1→L3). The fit system (Boa, wings) is already in NP-RISK-001 (mechanical row) and is
cross-referenced, not re-derived.

---

## 4. Layer-by-layer failure modes

### G01 — L0 optical window (PDMS / COC)

| FM-ID | Failure mode | Effect | S | P | Risk | Mitigation | Res S | Res P | Res | MTBF source |
|---|---|---|---|---|---|---|---|---|---|---|
| FMEA-G01-01 | PDMS–PI bond peel under thermal cycling → window delaminates, IP seal + anti-fouling lost | Module fouling; dose-coupling drift (PD-metered); no injury | S2 | P3 | 6 ALARP | **PDMS-QUAL** 200-cycle IEC 60068-2-14 (BLOCKING); PD1/PD2 ratio flags fouling/aging; per-module replaceable | S2 | P1 | 2 | PDMS-QUAL peel-vs-cycles → L10 |
| FMEA-G01-02 | Face scratch/abrasion from cleaning → optical scatter, dose non-uniformity | Mild dose non-uniformity (PD-metered; PBM already ±15–25 %) | S2 | P3 | 6 ALARP | Hard-coat (COC/PC) or self-healing PDMS; **sacrificial bezel** shields the window (np_therm_bezel §5); PD metering | S2 | P1 | 2 | Taber abrasion → haze vs cycles |

### G02 — L0 electrode face + fouling cap (T1-B / T2)

| FM-ID | Failure mode | Effect | S | P | Risk | Mitigation | Res S | Res P | Res | MTBF source |
|---|---|---|---|---|---|---|---|---|---|---|
| FMEA-G02-01 | Electrode fouled / hydrogel dried (cap not used) → high impedance | On tES: uneven current, **skin heating/burn**; on EEG: bad signal | S3 | P3 | 9 ALARP | **Software backstop: NP-FMEA-001 FMEA-M06** pre-session + 1 Hz mid-session impedance, cutoff >5× baseline; tethered hydration caps (WVTR <0.5); consumable prompts | S2 | P1 | 2 | Electrode life = consumable spec (30–60 sessions) |
| FMEA-G02-02 | Ag/AgCl chloride depletion / corrosion over life | Half-cell drift → EEG offset, tES current error | S2 | P2 | 4 ACCEPT | Sintered Ag/AgCl; ADS1299 self-cal each session; consumable tip | S2 | P1 | 2 | Accelerated chloride-cycling bench |

### G03 — L0 gap seal / blanking plugs

| FM-ID | Failure mode | Effect | S | P | Risk | Mitigation | Res S | Res P | Res | MTBF source |
|---|---|---|---|---|---|---|---|---|---|---|
| FMEA-G03-01 | Per-socket LSR gasket compression-set / tear → liquid/solid ingress into L1 | Socket corrosion/short (module poll fails closed); local, swappable | S2 | P3 | 6 ALARP | **SEAL-1** ingress test with modules seated/swapped/plugged; LSR compression-set spec; per-socket isolation; module UID health poll fail-closed | S2 | P1 | 2 | SEAL-1 IPX cycling → seal L10 |
| FMEA-G03-02 | Empty socket left unplugged (plug lost/omitted) → ingress + exposed socket contacts | Ingress; user touches socket contacts | S2 | P3 | 6 ALARP | Tethered/retained plugs; app inventory poll flags unplugged socket; socket contacts de-energized unless module UID authenticates | S2 | P1 | 2 | Plug retention pull-cycle test |

### G04 — L1 socket-layer structure (glass-filled PBT)

| FM-ID | Failure mode | Effect | S | P | Risk | Mitigation | Res S | Res P | Res | MTBF source |
|---|---|---|---|---|---|---|---|---|---|---|
| FMEA-G04-01 | Thermal/load creep → socket misalignment → 10-20 registration drift | Wrong-site dosing / degraded EEG placement; efficacy, no injury | S2 | P2 | 4 ACCEPT | Low-creep glass-filled PBT; ±12 mm pod travel absorbs drift; **REG-1** registration margin verified vs shell CAD | S2 | P1 | 2 | Creep test at 43 °C + load → dim. drift |

### G05 — L1 cluster clamps / latches + ground bond

| FM-ID | Failure mode | Effect | S | P | Risk | Mitigation | Res S | Res P | Res | MTBF source |
|---|---|---|---|---|---|---|---|---|---|---|
| FMEA-G05-01 | Clamp spring/detent fatigue → cluster loosens → module ejection / intermittent contact during use | Module unseats (RISK-001 mechanical: "zone module ejection"); session halts on PD/presence loss; no injury | S2 | P2 | 4 ACCEPT | 50k-cycle rating (as Boa); snap-fit detent (RISK-15 keying); module presence poll → fail-closed; 4–12 actuators, not 30–42 | S2 | P1 | 2 | Clamp fatigue cycling → L10 |
| FMEA-G05-02 | BeCu ground-finger fretting/oxidation over clamp cycles → contact R > 50 mΩ | Driven-shield/DRL reference degrades → EEG noise + RF bond loss | S2 | P3 | 6 ALARP | Hard-gold plating (specced); ≤50 mΩ target; SHDR EMF-attenuation fleet monitoring flags drift | S2 | P1 | 2 | Contact-R vs mate cycles |

### G06 — L2/L3 outer bowl (EMF stack + CFRP shell)

| FM-ID | Failure mode | Effect | S | P | Risk | Mitigation | Res S | Res P | Res | MTBF source |
|---|---|---|---|---|---|---|---|---|---|---|
| FMEA-G06-01 | CFRP slot-rim roughness Ra > 1.6 µm → parting-plane RF slot-antenna leakage | RF shielding claim compromised; EEG noise; no injury | S2 | **P?** | **OPEN** | **= RISK-20 (OPEN, BLOCKING for tooling).** Supplier Ra letter + coupon data; conductive elastomer bead + labyrinth lip; SHDR attenuation monitoring | S2 | P1 | 2 | RISK-20 coupon Ra + RF chamber |
| FMEA-G06-02 | Mu-metal liner corrosion/crack at a cutout edge → ELF magnetic shield leak | Reduced ELF shielding | S2 | P2 | 4 ACCEPT | PETG laminate encapsulation + silicone RTV at cutouts (specced); Pd-polyester tarnish-immune (permanent claim); SHDR monitoring | S2 | P1 | 2 | Salt-fog + thermal-cycle on coupon |

### G07 — Thermal path (spans L1→L3) — **new**

| FM-ID | Failure mode | Effect | S | P | Risk | Mitigation | Res S | Res P | Res | MTBF source |
|---|---|---|---|---|---|---|---|---|---|---|
| **FMEA-G07-01** | Fan/vent fouling or fan failure → outward thermal resistance rises → heat diverted scalp-ward → **scalp face > 42 °C while junction NTC ≤ 62 °C (no throttle)** | **Scalp thermal injury** not caught by the junction interlock | **S3** | **P3** | **9 ALARP** | **PROPOSED (OI-GEOM-FMEA-01):** (a) fan-RPM health interlock — SHDR already logs fan RPM → firmware duty-derate on low RPM; (b) THERM-1a CFD to prove junction-62 °C conservatively bounds face ≤42 °C even at worst-case degraded airflow; if not provable, (c) add scalp-side thermistor co-located with PD2 | S2 | P1 | 2 | THERM-1a/1b CFD + scalp-phantom bench |

---

## 5. Cross-reference to existing risks & software backstops

| Layer-stack FM | Existing RISK-001 / FMEA-001 tie-in | Relationship |
|---|---|---|
| G01 (optical window) | RISK-14 (PBM dose metering, dual PD) | PD1/PD2 metering is the shared mitigation |
| G02 (electrode/cap) | NP-FMEA-001 **FMEA-M06** (impedance interlock); RISK-001 biological row (ISO 10993) | Software impedance cutoff is the injury backstop |
| G03 (gap seal/plugs) | RISK-15 (module keying); module UID health poll | Fail-closed on socket fault |
| G04 (socket creep) | **REG-1** (10-20 registration) | Registration margin absorbs drift |
| G05 (clamps/bond) | RISK-001 mechanical (ejection); RISK-15; EMF stack | 50k-cycle + presence poll + hard-gold |
| G06 (outer bowl) | **RISK-20** (CFRP Ra, OPEN BLOCKING); RISK-03 (PBM reg) | G06-01 **is** RISK-20 — do not duplicate; track there |
| **G07 (thermal path)** | **NP-FMEA-001 FMEA-M04** (junction NTC); IEC 60601 42 °C | **Gap:** M04 bounds junction, not face — G07 closes it |
| Fit system (Boa/wings) | RISK-001 mechanical (Boa 50k-cycle) | Already covered; not re-derived here |

**Reconciliation verdict:** every parent §6 mechanical failure mode maps to either an existing RISK
ID, a software backstop in NP-FMEA-001, or (G07) a newly identified gap. All residuals are ACCEPTABLE
**except** G06-01 (= RISK-20, OPEN/BLOCKING, tracked in the register) and G07-01 (ALARP until
OI-GEOM-FMEA-01 mitigation lands). No mechanical failure mode reaches UNACCEPTABLE residual.

---

## 6. Open items

| ID | Description | Owner | Blocking |
|---|---|---|---|
| **OI-GEOM-FMEA-01** | Close FMEA-G07-01: implement fan-RPM health interlock (duty-derate on low RPM) **and/or** prove via THERM-1a CFD that junction-62 °C throttle bounds face ≤42 °C at worst-case degraded airflow; if not, add scalp-side thermistor at PD2. **Requirement drafted → NP-REQ-FANHEALTH-001 (SR-FAN-01…06);** remaining work is OI-FAN-01…04 there. | FW + Thermal | G2 firmware / THERM-1 close |
| OI-GEOM-FMEA-02 | Author L0–L3 hardware failure modes into NP-RISK-001 proper (this doc is the feeder); assign RISK-2x IDs under QMS change control. | Quality | Risk-file baseline update |
| OI-GEOM-FMEA-03 | Populate the "MTBF source" column with numbers as each reliability test completes (PDMS-QUAL, SEAL-1, clamp/creep/contact-R benches). | Reliability | Quantitative MTBF claims |
| — | G06-01 tracked as **RISK-20** (already OPEN, BLOCKING) — no new OI. | VP Eng | Tooling release |

## 7. Cross-references

np_helmet_geom_001 §6/§8 · np_therm_bezel_001 (THERM-1) · np_fmea_001 (SW-01; FMEA-M04, M06) ·
np_rm_001 §4/§7 (scales, RISK-14/15/20) · NP-RISK-001 (system register) · CLAUDE.md §4.2/§4.3.
