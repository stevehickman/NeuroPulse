# Modality stack — full specifications (T1 + T2)

> Relocated from CLAUDE.md §3 (Rev 40) to slim the always-loaded core. Content is verbatim and the
> headings are unchanged, so a `CLAUDE.md §3` citation lands on the same material. CLAUDE.md §3
> keeps the modality roster and the hard limits; everything below is the detail.
>
> **All modalities are locked.** Optional/provisional accessories (40 Hz mastoid vibrotactile pad,
> Apple Watch sync app) live in `docs/reference/accessories-roadmap.md`.
>
> **Read this file when:** specifying, implementing or reviewing any modality; authoring `.npps`
> protocols; or answering "what wavelength / current / limit does X use?". For whether a protocol
> fits the power envelope, read `docs/np_ses_pwr_001.md` as well.

### T1 — 8 modalities

**1. PBM Transcranial**
- 660–670nm + 808–830nm LEDs (base module)
- **1064nm snap-in smart zone module upgrade (accessory):** Path B smart module architecture — on-module Microchip ATtiny402 I2C slave + 3× Infineon IRLML6344 N-FETs drives 660nm (CH_A), 808nm (CH_B), and 1064nm (CH_C) independently from existing 20-pin FPC connector. 550 LEDs per module (200+200+150). InGaAs PDs (Hamamatsu G12180-010A) for dose metering. Base modules unchanged. BOM delta +$23–28, retail $149–199/zone. **Hub-side addressing is NOT what earlier revisions of this bullet claimed — three mechanisms are retired, not current design:** (a) ZONE_ID 3.3kΩ resistor detection, (b) a dedicated LPI2C3 bus enabled per slot, and (c) the Hub PCB Rev B per-slot Vishay DG2788A TIA gain switch (OI-PBM-HW-01). Modules are discovered by UID-based auto-inventory (`np_module_map`), and reached through NP-HW-HUB-001 Rev 3's cluster-controller fan-out — one differential cluster bus with transactions tunnelled through a cluster controller's PCA9548A, no per-socket I2C peripheral. SMART-1 requires every socket to be I2C/TIA-capable, which reopened the hub TIA-gain design as a Rev 3 item. None of the hub-side fan-out is implemented yet (OI-HUB-C01..C19). See NP-FW-PBM1064-001 Rev 3 (supersession banner), NP-HW-HUB-001 Rev 3 §5, NP-HW-FPC-001 Rev 5.
- **T2 combined 1064nm + 1170nm session:** 1064nm smart zone modules (cortical depth) + existing 1170nm laser system (subcortical depth) coordinated by session orchestrator. Three-tier penetration stack: 660nm surface → 1064nm cortical → 1170nm deep. Thermal throttle priority: 1170nm throttled first, then 1064nm CH_C, then CH_B. See NP-FW-PBM1064-001 §8 and NP-SES-1064-001 §6.
- Tiled across the hex-socket lattice (NP-HEX-ZM-001) — zones are protocol-defined sets of sockets (`00-zones.npps`), not a fixed hardware slot count. Total LED count scales with how many T1-A (base PBM) tiles are populated in a given build/config.
- 6mm inter-LED pitch → ±15–25% irradiance variation (near-uniform field)
- 120–180mA per LED → L70 80,000–100,000 hours
- **400 mW/cm² peak pulsed** (≤25% duty cycle, firmware-enforced) / 200 mW/cm² CW max
- **Dual photodiode dose-metering (RISK-14 Option B):** PD1 behind PDMS window (measures forward emission) + PD2 on scalp-facing surface (measures backscattered tissue power). PD1/PD2 ratio separates PDMS fouling from LED aging in firmware. Pin 19 (PD2_CATHODE). BOM +$0.75–1.50/headset. T1 and T2 use identical zone module mold.
- Plasma-activated anti-fouling PDMS optical windows. **PDMS–PI bond uses 75 nm SiO₂ interlayer (RF magnetron sputter) + O₂ plasma activation — achieves 174–860 N/m peel force.** 200-cycle IEC 60068-2-14 thermal cycling qualification required before production (BLOCKING).
- Real-time J/cm² dose metering — primary differentiator over Vielight
- 7 frequency presets: Gamma clarity (40Hz), Alpha calm (10Hz), Theta memory (6Hz), Sleep deep (2Hz), Gamma+theta coupled (40+6Hz split-zone), Focus prime (20Hz), Vascular baseline (CW)

**2. PBM Intranasal**
- Bilateral Y-probe · 660nm + 808–830nm per probe
- 15/20/25mm silicone over-molded depth stop rings (wear-resistant)
- Photodiode contact/dose sensing + reference LED at probe base
- Optical code + pogo pin resistive sleeve authentication (no NFC, no EMF)
- Hub dock storage (in hub tooling from day one — prevents Y-junction fracture)
- Hygiene sleeve consumable: 30-pack $19/pack or $19/mo subscription
- Silicone over-mold at Y-junction for impact protection

**3. EEG Neurofeedback**
- 8-ch semi-dry hydrogel · Fp1/2, F3/4, C3/4, P3/4
- 500Hz · 24-bit ADC (ADS1299)
- **ADS1299 internal reference self-calibration at every session start** (eliminates gain/offset drift)
- Spring-decoupled electrode pods: 80–120g contact force, ±12mm travel, independent of dial tension
- Replaceable hydrogel tips: snap-off bayonet, 30–60 sessions, $12–16/8-pack
- Moisture-barrier silicone hydration caps (WVTR <0.5 g/m²/day) — extend storage life to 24+ months

**4. BES / tACS (consumer name: Brainwave Entrainment Stimulation)**
- 0.5–40Hz · ≤1mA · charge-balanced biphasic
- Per-electrode impedance monitoring
- Adaptive EMF notch firmware prevents Helmholtz cancellation of therapeutic signal
- Regulatory naming avoids FDA medical device classification trigger

**5. tDCS (consumer name: Cortical Priming Stimulation)**
- 0.1–2mA DC · 40µC/cm² hardware limit (safety MCU enforced, app cannot override)
- 30s ramp up/down (hardware-enforced)
- ≤3 electrode pairs

**6. VNS + HRV + HRV Biofeedback**
- Auricular clip · auricular branch CN X
- 1–25Hz · ≤2mA · biphasic charge-balanced
- PPG HRV (808–830nm) in same clip
- **A1/A2 EEG references on clip contact pads** (2 spare conductors in existing 6-pin cable, +$15 BOM)
- PDMS hydrogel pads: 20–40 sessions, $8/2-pack
- Force contact confirmation
- **HRV Biofeedback Protocol (software only, no additional hardware):**
  - Resonance frequency breathing pacer: default 6 breaths/min (0.1 Hz); personalized to user's peak HRV frequency during first-session sweep (4–7 breaths/min range)
  - Breathing cue delivery: visual ring expanding/contracting in app + optional bone conduction audio cue (uses existing audio hardware)
  - Real-time coherence score: LF peak power / (LF + HF total power), displayed 0–10 color-coded
  - RMSSD displayed per session; session trend graph over 30 sessions
  - **Four protocols:**
    - Standalone coherence training (5–20 min, breathing pacer + coherence display)
    - HRV + taVNS synchronized: stimulation pulses timed to inspiration phase (PPG R-R interval detects respiratory cycle); optimises noradrenergic modulation window
    - HRV + EEG dual biofeedback: coherence score + EEG band power displayed simultaneously; closed-loop EEG-adaptive frequency adjusts to both signals
    - HRV + PBM: PBM running during HRV coherence training (replicates 2025 multi-modal RCT protocol: PBM + qEEG NF + HRV biofeedback)
  - UHDR: HRV time series, coherence scores, session logs
  - SHDR: coherence trend slope (no user biology)
  - Evidence: meta-analysis 24 RCTs (d=0.83 anxiety reduction, d=0.65 depression); 2025 multi-modal RCT (PBM + qEEG NF + HRV combined, nationally conducted RCT)

**7. Neural Audio Entrainment**
- Over-ear planar magnetic 40mm + bone conduction at mastoid
- Binaural beats + isochronic tones + pink/brown noise
- EEG-adaptive frequency (closed-loop)
- **User-replaceable snap-in mesh frame** (silver-coated nylon, 40dB RF, $9.99/pair annual)
- Aluminium bayonet mount (replaces plastic — wear-resistant)
- Driver impedance monitors mesh fouling (detects both acoustic degradation AND RF shielding loss simultaneously)
- Mesh cleaning brush in box
- Silicone isolator for bone conduction piezoelectric element

**8. Visual Stimulation**
- 108 micro-LEDs per lens (660nm + 808–830nm) · 6 zones per eye
- Inner PDMS diffuser film (plasma-activated anti-fouling)
- **AgNW (silver nanowire) outer conductive coating** — replaces ITO (ITO has 0.5% strain-to-failure; AgNW tolerates 5–10% flex)
- IR proximity sensors (940nm) — eye-open detection
- Hall sensor: goggle lift = instant LED cutoff
- IEC 62471 hardware MPE limit (50% of exempt group threshold)
- Photoparoxysmal EEG detection at Oz → goggle halt <200ms
- **Mode F (invisible NIR retinal walk):** 808–830nm daily retinal PBM during normal-looking wear
- EMDR L/R alternation · photic driving 0.5–100Hz

**Snap-on shade system:**
- S1 opaque (<0.5% VLT, included in box, instant cutoff for immersive sessions)
- S2 polarising (~12% VLT, $24, standard lens only)
- S3 prescription clip ($49 carrier + $49–139 Rx insert, compatible with both standard and EC lenses, 12–24 month renewal)
- 6× N42 neodymium magnets in lens rim (N42 not N52 — better impact tolerance, −$0.80 BOM)
- Sliding rail lens mount (user self-install, eliminates alignment jig requirement, +$1.20 BOM)

**EC lens (premium, +$89 upgrade / $129 standalone):**
- Bistable electrochromic 5–75% VLT · 2s transition · ~15mW hold
- Clears to 75% on power restore (safety failsafe)
- 3–5µm hard coat over EC film (scratch protection, standard in automotive EC mirrors)
- EC driver monitors transition time as contact resistance proxy (detects rim contact corrosion)

### T2 additions

- **21-ch qEEG wet gel:** Full 10-20 + FC3/FC4 (M1 TMS targeting) + Oz (photoparoxysmal detection) + A1/A2 (linked-ear normative reference, on VNS clips)
- **TMS focal figure-8 coil:** 0.1–0.5T · rTMS + TBS · non-conductive CFRP window at coil site · TMS-gated EMF cancellation (safety MCU gates Helmholtz off 5ms pre-pulse, 50ms post-pulse hold)
- **1170nm deep PBM:** Laser diodes · 35–40mm subcortical depth · TEC stabilisation · ≤1,000 mW/cm²
- **Clinical tACS:** ≤4mA · 21-ch arbitrary waveform (one channel per cap electrode)
- **sLORETA-guided HD-tDCS:**
  - 4×1 ring montage: center anode + 4 return cathodes positioned by sLORETA source map — provides ~3–5× spatial focality vs standard 2-electrode tDCS
  - Electrode: Ag/AgCl sintered 3.5mm diameter, dual-rated for EEG recording AND stimulation current (simultaneous or sequential); part of T2 qEEG wet-gel cap
  - Current sourcing: 21-ch tACS driver (already in T2) provides independently controlled channels — one per cap electrode, no sharing — no additional stimulation hardware
  - Workflow: (1) T2 21-ch qEEG resting-state session → (2) sLORETA computes cortical source map (real-time or post-session) → (3) app identifies target region (e.g., DLPFC hypoactivity, anterior cingulate hyperactivation) → (4) firmware maps MNI target to nearest 10-20 electrode positions → (5) configures 4×1 current distribution automatically → (6) delivers personalized tDCS session
  - **Localization ≠ reachability:** sLORETA resolves deep sources, but a 4×1 ring is focal only for cortical-surface targets (~1.5 cm FWHM at 10 mm depth). ACC sits 47.1 mm from its nearest scalp electrode and is not focally reachable from any electrode position; a 4×1 there is indirect network modulation. Targets carry a `NP_HD_TARGET_DEPTH_SURFACE`/`_DEEP` class and deep targets must never be presented as focal stimulation. See NP-FW-HD-001 §2.3.
  - Montage options: 4×1 ring (most focal, ~1.5cm FWHM), bilateral 4×1 (dual hemisphere), standard 2-electrode (T1-compatible fallback)
  - Safety: 40µC/cm² charge density limit enforced by safety MCU; ≤2mA per electrode; focal electrode density ≤6 A/m² (within Bikson lab safety limits for 3.5mm electrode geometry)
  - Clinical evidence: Jog/UCLA 2025 (n=71, personalized MRI-guided HD-tDCS, significant depression improvement + gray matter changes); BRIGhTMIND 2024 (n=255, connectivity-guided iTBS shows personalized targeting outperforms fixed F3)
  - BOM delta: Ag/AgCl dual-rated electrodes in T2 cap specification; no additional driver hardware; +$0 software
- **Cervical VNS (tcVNS) — T2 accessory:**
  - Neck-worn accessory stimulating cervical vagus trunk (higher activation than auricular branch CN X)
  - Gel electrodes applied to skin overlying carotid sheath; bilateral or unilateral
  - Indication: cluster headache + migraine (FDA-cleared precedent: electroCore gammaCore K163334, K173323); extending to depression, PTSD, post-stroke rehabilitation
  - Safety MCU ownership: current path near carotid → safety MCU reads impedance + cardiac rhythm monitor before enable; automatic cutoff if HR changes >15 BPM within 5s of stimulation
  - Regulatory: 510(k) predicate = electroCore gammaCore (K163334 cluster headache, K173323 migraine); separate 510(k) required for T2 product launch; T1 uses auricular-only (no carotid proximity)
  - Connects via existing hub accessory port; separate cable + electrode assembly; gel pad consumable (5-pack)
  - BOM delta: +$35–55 for cervical tcVNS accessory module
- **HIPAA cloud + EHR:** FHIR R4 · multi-patient dashboard · sLORETA source imaging (also drives HD-tDCS targeting) · LSL streaming · scripting API
- **anonymized session tag:** Random session identifier for clinical multi-patient environments — clinic holds patient-to-tag mapping, NeurOne cannot cross-reference

---

