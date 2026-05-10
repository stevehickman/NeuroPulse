# CLAUDE.md — NeuroPulse Design Programme
**Project:** NeuroPulse — closed-loop multi-modal neuromodulation wearable platform  
**Revision:** 4 (current)  
**Status:** Pre-tooling design phase. No hardware committed yet. All decisions below are locked unless explicitly noted as pending.

---

## 1. PRODUCT OVERVIEW

Two-tier platform sharing a single chassis, processor stack, app, and USB-C connectivity.

| Tier | Name | Regulatory | Modalities | Price range | Timeline |
|------|------|-----------|------------|-------------|----------|
| T1 | NeuroPulse Home | FDA-exempt wellness | 8 | $449–$1,199 | 12–18 months |
| T2 | NeuroPulse Pro | FDA 510(k) target | 11 | $4,999–$13,999 + $1,800/yr | 18–36 months post T1 |

**Founding design principles:**
- Shared platform (one production line, two markets)
- Wired-first USB-C default (zero RF at scalp)
- Autonomous closed-loop EEG-adaptive stimulation without phone (primary competitive moat)
- 5-layer EMF shielding + active Helmholtz cancellation (only consumer brain wearable with measured shielding)
- Modular field-upgradeability via snap-in zone modules
- No mandatory subscription — all core functions offline-capable permanently
- UHDR/SHDR data separation (user health data never accessed by NeuroPulse)

---

## 2. CONFIGURATIONS + PRICING (all locked)

### 2.1 Integrated system configurations

| Config | BOM | COGS | Retail | GM% | Modalities included |
|--------|-----|------|--------|-----|---------------------|
| Core — EEG only | $168–169 | $258–260 | $449 | 42% | 4-ch EEG · all connectivity · EMF shielding · processor stack · 8GB eMMC |
| Home Lite | $265–266 | $370–372 | $599 | 38% | Core + PBM 5 zones (660+810nm, 600 LEDs) · 8-ch EEG · VNS+HRV clip |
| Home Standard ★ (flagship) | $405 | $540 | $849 | 36% | All T1 modalities (see §3) |
| Home Premium | $460 | $622 | $1,199 | 48% | All T1 + EC lens (+$89 value) · 2yr warranty · priority support |
| Pro Entry | $833 | $1,365 | $4,999 | 73% | All T1 + 21-ch qEEG · 1170nm deep PBM · clinical tACS · HIPAA cloud · sLORETA |
| Pro Full | $1,506 | $2,628 | $13,999 | 81% | All T2 + TMS hub · multi-patient dashboard · scripting API · FHIR R4 · $1,800/yr service |

**★ Home Standard box contents:** All T1 modules · hard clamshell case · braided aramid USB-C cable (spare in box) · **45W NeuroPulse branded GaN charger** · S1 opaque shade · interface covers (installed + spare set each type) · mesh cleaning brush · Boa replacement cable + hook tool · moisture-barrier electrode tip hydration caps · humidity indicator card · pre-impregnated cleaning cloth packets

### 2.2 Charger policy (locked)

Charger scaled to peak draw of configuration. Auto-included at every upgrade by serial number tracking. Upfront 65W upgrade option ($19 at-cost) offered at checkout as intent signal.

| Config | Charger included | BOM |
|--------|-----------------|-----|
| Core | 15W USB-C (unbranded) | $3–4 |
| Home Lite | 30W GaN (unbranded) | $5–6 |
| Home Standard ★ | 45W NeuroPulse GaN (branded) | $10 |
| Home Premium | 45W NeuroPulse GaN (branded) | $10 |
| Pro Entry | 65W NeuroPulse GaN (branded) | $13 |
| Pro Full | 65W NeuroPulse GaN (branded) × 2 | $26 |

**Charger upgrade intent signals:**
- Core buyer selects 30W upfront → PBM intent → 14-day follow-up
- Core buyer selects 45W upfront → Full T1 intent → 7-day completion bundle offer
- Any buyer selects 65W upfront → T2 intent → human clinical sales call within 48 hours

**EU note:** Chargers are branded recommendations, not proprietary requirements. Any PD-compliant charger must work. App displays "power level: reduced" informatively, never blocks.

### 2.3 Consumables + recurring revenue

| Item | Price | Interval | GM% | Notes |
|------|-------|----------|-----|-------|
| Intranasal sleeves (30-pack) | $19/pack or $19/mo sub | Single use | 68–79% | Only authenticated consumable. COGS $4–6. Primary MRR driver. |
| Electrode hydrogel tips (8-pack) | $12–16 or $9.99/mo sub | 30–60 sessions | 60–72% | App impedance trend prompts. Bayonet snap, zero training. |
| VNS clip pads (2-pack) | $8/pack | 20–40 sessions | 65% | Electrochemical degradation from VNS current. |
| Audio cup foam (set) | $24/set | 6–12 months | 58% | Calendar reminder. |
| Audio cup mesh frame (pair) | $9.99/pair | Annual | 62% | App driver impedance flags fouling. Snap-in, user-replaceable. |
| Interface protection covers (complete kit) | $22.99 or $19.99/yr bundle | Annual / as lost | 70% | All tethered — loss prevention by design. |
| S3 prescription Rx insert | $49–139 | 12–24 months | Variable | Optician partner network. Zero marginal marketing cost per renewal. |
| T2 service contract | $1,800/yr | Annual | ~75% | Same-day loaner, priority support, annual calibration. |

---

## 3. MODALITY STACK (all locked)

### T1 — 8 modalities

**1. PBM Transcranial**
- 660–670nm + 808–830nm LEDs
- 5 independently addressable zones
- **600 total LEDs: 300×660nm + 300×808–830nm** on FPC strips (launches at 600 from day one — no Rev A/Rev B)
- 6mm inter-LED pitch → ±15–25% irradiance variation (near-uniform field)
- 120–180mA per LED → L70 80,000–100,000 hours
- **400 mW/cm² peak pulsed** (≤25% duty cycle, firmware-enforced) / 200 mW/cm² CW max
- **Dual photodiode dose-metering (RISK-14 Option B):** PD1 behind PDMS window (measures forward emission) + PD2 on scalp-facing surface (measures backscattered tissue power). PD1/PD2 ratio separates PDMS fouling from LED aging in firmware. Pin 19 (PD2_CATHODE). BOM +$0.75–1.50/headset. T1 and T2 use identical zone module mould.
- Plasma-activated anti-fouling PDMS optical windows. **PDMS–PI bond uses 75 nm SiO₂ interlayer (RF magnetron sputter) + O₂ plasma activation — achieves 174–860 N/m peel force.** 200-cycle IEC 60068-2-14 thermal cycling qualification required before production (BLOCKING).
- Real-time J/cm² dose metering — primary differentiator over Vielight
- 7 frequency presets: Gamma clarity (40Hz), Alpha calm (10Hz), Theta memory (6Hz), Sleep deep (2Hz), Gamma+theta coupled (40+6Hz split-zone), Focus prime (20Hz), Vascular baseline (CW)

**2. PBM Intranasal**
- Bilateral Y-probe · 660nm + 808–830nm per probe
- 15/20/25mm silicone over-moulded depth stop rings (wear-resistant)
- Photodiode contact/dose sensing + reference LED at probe base
- Optical code + pogo pin resistive sleeve authentication (no NFC, no EMF)
- Hub dock storage (in hub tooling from day one — prevents Y-junction fracture)
- Hygiene sleeve consumable: 30-pack $19/pack or $19/mo subscription
- Silicone over-mould at Y-junction for impact protection

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
  - Resonance frequency breathing pacer: default 6 breaths/min (0.1 Hz); personalised to user's peak HRV frequency during first-session sweep (4–7 breaths/min range)
  - Breathing cue delivery: visual ring expanding/contracting in app + optional bone conduction audio cue (uses existing audio hardware)
  - Real-time coherence score: LF peak power / (LF + HF total power), displayed 0–10 colour-coded
  - RMSSD displayed per session; session trend graph over 30 sessions
  - **Four protocols:**
    - Standalone coherence training (5–20 min, breathing pacer + coherence display)
    - HRV + taVNS synchronised: stimulation pulses timed to inspiration phase (PPG R-R interval detects respiratory cycle); optimises noradrenergic modulation window
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
- **Clinical tACS:** ≤4mA · 16-ch arbitrary waveform
- **sLORETA-guided HD-tDCS:**
  - 4×1 ring montage: center anode + 4 return cathodes positioned by sLORETA source map — provides ~3–5× spatial focality vs standard 2-electrode tDCS
  - Electrode: Ag/AgCl sintered 3.5mm diameter, dual-rated for EEG recording AND stimulation current (simultaneous or sequential); part of T2 qEEG wet-gel cap
  - Current sourcing: 16-ch tACS driver (already in T2) provides independently controlled channels — no additional stimulation hardware
  - Workflow: (1) T2 21-ch qEEG resting-state session → (2) sLORETA computes cortical source map (real-time or post-session) → (3) app identifies target region (e.g., DLPFC hypoactivity, anterior cingulate hyperactivation) → (4) firmware maps MNI target to nearest 10-20 electrode positions → (5) configures 4×1 current distribution automatically → (6) delivers personalised tDCS session
  - Montage options: 4×1 ring (most focal, ~1.5cm FWHM), bilateral 4×1 (dual hemisphere), standard 2-electrode (T1-compatible fallback)
  - Safety: 40µC/cm² charge density limit enforced by safety MCU; ≤2mA per electrode; focal electrode density ≤6 A/m² (within Bikson lab safety limits for 3.5mm electrode geometry)
  - Clinical evidence: Jog/UCLA 2025 (n=71, personalised MRI-guided HD-tDCS, significant depression improvement + gray matter changes); BRIGhTMIND 2024 (n=255, connectivity-guided iTBS shows personalised targeting outperforms fixed F3)
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
- **Anonymised session tag:** Random session identifier for clinical multi-patient environments — clinic holds patient-to-tag mapping, NeuroPulse cannot cross-reference

---

## 3b. OPTIONAL ACCESSORIES + COMPANION SOFTWARE (provisional specs)

### 40Hz Vibrotactile — Mastoid LRA Pad (provisional)

Purpose-built accessory delivering precisely characterized 40Hz somatosensory stimulation, aligned to Tsai lab (MIT) GENUS multi-sensory protocol. **Provisional — release contingent on HOPE Phase 3 results (mid-2026). Hardware provision in first revision at zero tooling cost.**

- **Actuator:** Linear resonant actuator (LRA), 8–10mm diameter, e.g. Jinlong JMC0834 or equivalent. LRA preferred over ERM: precise frequency control, low distortion, flat resonance profile at 40Hz drive.
- **Driver IC:** Texas Instruments DRV2605L (I2C, open-loop mode). Open-loop drive at 40Hz ± 0.5Hz with amplitude control via gain register. No resonant frequency tuning required (drive frequency set in firmware, not actuator resonance).
- **Placement:** Posterior temporal / mastoid process. Rationale: (1) direct bone coupling to skull — better transmission to somatosensory cortex than wrist; (2) adjacent to temporal lobe somatosensory representation; (3) compatible with existing temporal stability wing anchor — no new shell tooling required if anchor boss provisioned at first cut.
- **Output spec:** 0.6–1.2G peak acceleration at skin surface (DRV2605L gain register configurable); 40Hz ± 0.5Hz; duty cycle 100% continuous (20-minute session target).
- **Form factor:** 30mm diameter silicone overmoulded pad; clip attachment to temporal wing; 3-pin pogo or JST connector to hub accessory port. Shore 30A silicone contact face for comfort.
- **Power draw:** ~80–120mW continuous. Supplied from hub accessory port (existing 500mA capability).
- **BOM estimate:** LRA $1.50–2.50 + DRV2605L $1.20–1.80 + PCB/passives $0.50 + silicone housing $0.80–1.20 = **$4–7 total per pad**. Pair (bilateral option): $8–14.
- **Retail accessory price (projected):** $49–79 per pad / $79–119 bilateral.
- **Firmware:** 40Hz square-wave drive pattern; session start/stop synchronised with audio/visual 40Hz channels via hub; amplitude ramp 2s up/down (comfort).
- **Marketing note:** Apple Watch sync app is available as a free companion — but the mastoid pad delivers results the Watch cannot. Key differences: mastoid placement couples directly to skull (vs wrist-to-brain soft tissue attenuation); 40Hz ± 0.5Hz locked precision (vs uncharacterised Taptic Engine output); hub-powered (vs Watch battery drain from 20 min continuous haptics). Message: "Use the Watch app as an extra layer — for the full experience, the pad is what delivers it." See §15 Marketing Notes for full draft copy.
- **Status:** PROVISIONAL — await HOPE Phase 3 (Cognito Therapeutics, n=670, mid-2026). If positive: release mastoid pad within 6 months. Hardware anchor boss in temporal wing tooling at first cut (zero incremental tooling cost).

### Apple Watch Sync App (provisional)

Companion watchOS app extending NeuroPulse session experience to Apple Watch. Three sync channels. Does **not** replace purpose-built NeuroPulse hardware for any therapeutic function — supplements it.

- **Communication:** BT 5.3 LE (hub already has BT radio, antennas in hub); WatchConnectivity framework via paired iPhone app; session sync protocol over BLE GATT custom service.
- **Channel 1 — Haptic sync:** watchOS Core Haptics delivers 40Hz pattern in synchronisation with NeuroPulse hub session clock. Adds wrist somatosensory channel on top of mastoid LRA pad. Not a standalone therapeutic — supplement only. Caveat in app: "Works best with NeuroPulse mastoid vibrotactile accessory."
- **Channel 2 — Audio sync:** Watch app plays binaural beats / isochronic tones / breathing pacer audio through AirPods or earphones paired to Watch, synchronised to hub session. Useful when user wants bone conduction reserved for breathing cue while earphones handle binaural beats, or for sessions away from the hub speaker range.
- **Channel 3 — Visual sync:** Watch display shows 40Hz visual flicker (reduced brightness, GENUS-compatible) or EMDR left/right indicator arrow synchronised to goggle session. Also: session status, coherence score live feed, HRV biofeedback breathing ring (complementary to app display for wrist-glance UX).
- **Additional Watch functions:** Session timer + haptic end-of-session alert; protocol selector (basic, without phone); quick impedance check result notification; consumable low reminders.
- **Regulatory note:** All Watch-delivered functions are declared as session monitoring / user interface aids, not therapeutic delivery. Therapeutic claims attach to NeuroPulse hardware only.
- **BOM delta:** $0 hardware. Software development cost only.
- **Status:** PROVISIONAL. Prioritise after core iOS app ships. Haptic and audio channels first; visual flicker second (screen brightness characterisation needed for 40Hz at ≥100 nits).

---

## 4. HARDWARE SPECIFICATIONS (all locked)

### 4.1 Processor stack
- **Main:** NXP i.MX RT1062 · Cortex-M7 · 600MHz · FPU+DSP+SIMD · 1MB on-chip SRAM + 32MB LPSDR4 · USB-HS OTG · FreeRTOS · ~1.1% CPU at full load (98.9% headroom for future ML)
- **Safety MCU:** STM32G071 (NOT G031 — G031 has only 8KB SRAM, insufficient for EMF firmware) · Cortex-M0+ · 64MHz · 36KB SRAM · 128KB flash · bare-metal · owns all stimulation GPIO enable lines · +$0.45 BOM
- **Storage:** 8GB industrial eMMC (SLC cache, 30,000+ P/E cycles) · LittleFS filesystem · firmware partition write-protected · separate UHDR/SHDR partitions from first firmware line
- **Connectivity:** USB-C 3.2 Gen1 (default, zero RF, <1ms) · BT 5.3 LE Audio · Wi-Fi 6 · antennas in control hub NOT headset · single rear toggle

### 4.2 Safety architecture
- Safety MCU physically owns all stimulation enable GPIO — app crash cannot cause unsafe stimulation
- SPI heartbeat from main processor every 200ms; 1.5s watchdog → all-stimulation cutoff <50ms
- Dual-processor isolation: IEC 62304 Class C (safety MCU, ~500 lines bare-metal) + Class B (main processor) separately certified
- Session protocol cryptographically signed by app — headset rejects unsigned or corrupted protocols

**Modality-specific interlocks:**
| Modality | Interlock | Implementation |
|----------|-----------|----------------|
| EEG + Visual | Photoparoxysmal detection → goggle halt | Oz electrode, <200ms, clinician-unlock for 3–30Hz |
| BES / tDCS | 40µC/cm² charge density limit | Safety MCU hardware — app cannot override |
| Visual / retinal | IEC 62471 MPE ceiling | IR proximity + Hall sensor + hardware current limit (3 independent layers) |
| PBM scalp | IEC 60601 42°C limit | NTC per zone → hardware current throttle at 62°C junction |
| TMS | Coil protection | EMF cancellation gated off 5ms before pulse, 50ms hold |
| VNS | Contact confirmation | Safety MCU reads impedance; holds if contacts not confirmed |
| All | Firmware anti-fragility | CSPRNG session protocol signing |

### 4.3 EMF shielding (5-layer passive + active)
- Layer 1: CFRP outer (30–50dB RF)
- Layer 2: 0.2mm mu-metal liner (15–25dB ELF magnetic) — PETG laminate encapsulation, silicone RTV sealant at all cutout edges
- Layer 3: **Palladium-coated polyester** inner liner (replaces silver — tarnish-immune for device lifetime, 40–60dB RF) — permanent shielding claim, verified by fleet SHDR attenuation monitoring
- Layer 4: Carbon-loaded EMI absorber foam (cavity resonance suppression)
- Layer 5: USB-C + accessory port filters (30–50dB)
- **Active:** 3-axis fluxgate magnetometers + Helmholtz coil pairs · Combined: 35–45dB ELF magnetic, 40–60dB RF
- Shell bonded to EEG DRL output (active EEG shield)
- Non-conductive CFRP window at TMS coil site (prevents eddy current field loss)
- Three firmware additions: TMS-gated cancellation · adaptive notch at BES/tACS stimulus frequency · synchronous Helmholtz subtraction from EEG

### 4.4 Fit system
- Boa-style occipital dial · 10cm range · 0.5mm/click · 50,000-cycle rated · enclosed PTFE-lined cable channel (prevents hair entanglement) · Boa replacement cable + tool in box · regrease kit available ($4.99 accessory)
- 5-position forehead bridge (5mm steps)
- Spring-decoupled electrode pods (80–120g, ±12mm, Shore 30A silicone)
- Temporal stability wings (snap-on, stored in hub dock)
- 1 adult SKU covers 52–62cm heads

### 4.5 Power
| Mode | Draw | Min USB-C PD | Power bank runtime (10,000mAh) |
|------|------|-------------|-------------------------------|
| Standby | 1W | 5V/0.5A | ~330 hours |
| EEG only | 2.5W | 5V/1A | ~130 hours |
| Standard T1 ★ | ~17–20W | 15V/2A (45W) | ~95–110 min |
| T1 peak | ~45–50W | 20V/3A (65W) | ~38–42 min |
| T2 standard | ~44–46W | 20V/3A (65W) | ~41–43 min |
| T2 peak | ~70–74W | 20V/5A (100W EPR) | ~24–27 min |

- 22F supercapacitor in control hub (absorbs LED duty-cycle transients, allows 50% aging over 5 years)
- Hub NTC thermistor for supercapacitor aging estimation (logged in SHDR)

### 4.6 Operating modes
- **Mode 1 Connected:** Real-time streaming <1ms
- **Mode 2 Programming:** App uploads session protocol <5s
- **Mode 3 Autonomous:** Pre-programmed, runs from any USB-C PD power bank, full closed-loop EEG-adaptive operation without phone or app
- **Mode 4 Download:** USB-C reconnect → EDF+ + parameter logs to app

### 4.7 Status indicators
- Left temple: green power LED (breathes at idle)
- Right temple: amber in-use LED (pulse rate mirrors session frequency — caregiver can confirm correct protocol across room)
- Fault: power LED red blink
- Stealth mode: app-controlled suppress (safety faults always fire)

---

## 5. DATA ARCHITECTURE — UHDR / SHDR (all locked)

### 5.1 Definitions

**UHDR — User Health Data Record**
- Owner: user unconditionally
- NeuroPulse access: **NEVER** — not for support, engineering, research, or regulatory submission
- Clinician access: per-element, per-use-case, time-limited, audited, revocable
- Researcher access: anonymised aggregate only, separate IRB + explicit research consent
- Defining test: does this record tell us something about the **person**? If yes → UHDR
- Contents: EEG waveforms (all channels) · HRV time series · PPG optical signal · neurofeedback performance scores · session timestamps and duration · protocol parameters used · closed-loop adaptation events · PBM dose (J/cm²) per zone · user-entered symptom/outcome logs · eye-open/closed state during sessions
- Storage: on-device eMMC UHDR partition, AES-256 encrypted with user biometric-derived key (NeuroPulse does not hold decryption key)
- Backup: automated nightly incremental backup to USB-C local or E2E encrypted cloud (user-held key) when on USB-C power

**SHDR — System Health Data Record**
- Owner: NeuroPulse
- Linked to: device ID + warranty owner ID **only** — never to user identity
- Defining test: does this tell us about the **device's condition**, with nothing that reveals user biology? If yes → SHDR
- Contents: LED output ratio per zone · NTC temperature profiles · EMF shielding attenuation ratio · device session count (unsigned integer, no timestamps) · consumable session counts · USB-C insertion counter · PD negotiation log · impact events (g-force, orientation — between sessions only) · fan RPM · supercapacitor cycles · firmware version history · OTA log · accessory authentication pass/fail · calibration coefficient history
- Storage: on-device eMMC SHDR partition, separate encryption from UHDR
- Upload: to NeuroPulse fleet database on USB-C connect (warranty consent required)

**Boundary case resolution rule:** When in doubt → UHDR. Reclassification requires positive demonstration of no user biology content.

Specific boundary resolutions:
- Raw EEG impedance → UHDR; derived trend slope → SHDR
- Accelerometer during active sessions → UHDR; impact events between sessions → SHDR
- Raw ambient light → UHDR; cumulative UV exposure index → SHDR
- Raw VNS impedance → UHDR; contact resistance trend → SHDR
- IR eye state during sessions → UHDR; safety interlock log → SHDR
- Device session count (unsigned integer) → SHDR; session timestamps → UHDR

### 5.2 Predictive maintenance system (SHDR-based)

Three phases:
- **Phase 1** (0–1,000 devices, Year 1): Population-average survival analysis on time-to-failure data
- **Phase 2** (1,000–10,000 devices, Year 2): Fleet-trained LSTM on HDR sensor trajectories
- **Phase 3** (10,000+ devices, Year 3+): Bayesian personalisation — continuously revised RUL predictions

All models version-stamped by hardware revision. New revision falls back to Phase 1 until fleet data accumulates. Models deployed back to devices via OTA — competitive moat grows automatically with fleet size.

**Reminder engine rules:**
- Safety-critical: cannot be dismissed — blocks session start
- Performance-critical: snooze max 3×
- Comfort/longevity: snooze max 5×
- All reminders measurement-triggered, not calendar-triggered
- Every reminder includes measured data that triggered it + one-tap order link

### 5.3 Research data anonymization architecture (locked)

All anonymization of UHDR data for research purposes must occur **on-device**, within the NeuroPulse app, before any data leaves the device. NeuroPulse cannot access raw UHDR at any point — including for research purposes — because the biometric-derived AES-256 key is never held by NeuroPulse infrastructure.

**Data flow per approved study:**
1. NeuroPulse server sends device a signed study descriptor (study ID, approved UHDR element list, anonymization parameters: k≥10, suppression rules, date-rounding ≥1-week interval). Descriptor is cryptographically signed.
2. App reads encrypted UHDR partition in-app, applies on-device anonymization transformations: k-anonymity grouping, date/time rounding, direct identifier removal, quasi-identifier suppression per study descriptor.
3. Only the pre-anonymized, signed extract is transmitted to NeuroPulse research infrastructure. Raw UHDR never leaves the device.
4. NeuroPulse servers store extract keyed to study ID and device ID only. No persistent per-user anonymized data store. No linkage table exists that could re-identify users.
5. Researchers access aggregated study datasets with no device ID fields.

**Consent withdrawal effect:** Because each study extract is generated on-device on-demand, withdrawing consent permanently blocks the device from processing future study descriptors. No further extracts are generated or transmitted — **for any data period, including sessions predating withdrawal**. Already-published extracts cannot be individually removed from datasets (irreversibility notice given at consent time); no new data flows ever.

**Audit trail (SHDR):** Study ID, study descriptor hash, extract transmission timestamp, and extract byte count are logged in SHDR. User can inspect all studies their device has contributed to via the app. This log is never shared with researchers.

---

## 6. CLINICAL CONSENT ENGINE (all locked)

### 6.1 Use case subscription tiers

| Tier | Price | Use cases | UHDR elements | Target clinician |
|------|-------|-----------|---------------|-----------------|
| Monitor | $49/mo/patient | Adherence monitoring, protocol compliance | Session timestamps, duration, protocol parameters | Primary care, wellness, coordinators |
| Assess | $149/mo/patient | All Monitor + EEG review, neurofeedback, efficacy | Adds EEG waveforms, neurofeedback scores, dose logs | Neurologists, psychiatrists |
| Full Clinical | $299/mo/patient | All Assess + HRV, closed-loop events, outcomes | Adds HRV, PPG, adaptation events, outcome logs | TMS clinics, neuromodulation programmes |
| Research | $599/mo/study | IRB-defined custom (NeuroPulse review required) | IRB-approved minimum, k≥10 anonymisation, no IDs | Academic trials, observational studies |

**Key principle:** Clinicians select **use cases** (not data elements). System determines minimum necessary UHDR elements. Users receive plain-language decision support document listing what clinician CAN learn, CANNOT learn, and privacy implications per element.

**Expansion workflow:** Differential consent document → persistent user notification → user approves/denies/asks questions → retroactive access is a separate decision. Retroactive and prospective access presented as separate consent decisions even if made simultaneously.

### 6.2 A priori research consent (4 onboarding screens)

| Layer | Question | If yes | If no | Brand ambassador mechanism |
|-------|----------|--------|-------|--------------------------|
| L1 — Contact consent | Can we reach you about future research opportunities? | Provide contact method + frequency limit. POA holders upload POA (human review, 3 business days, jurisdiction-flagged, annual re-verification) | No contact. All features unchanged. | Being asked creates perceived agency → trust baseline |
| L2 — Category consent | Which research areas? (9 categories: AD/dementia, Depression, PTSD, TBI, Sleep, Attention, Parkinson's, Healthy ageing, Visual health) | Per-project contact for selected categories only. Each project is a fresh decision. | Not contacted for that category. | Personal category choice deepens engagement |
| L3 — Blanket consent | Pre-approve all NeuroPulse-reviewed research? | Data included in all studies. **Still receives per-study engagement notifications** (not consent requests — maintains engagement, can opt out per-study). Anonymisation: k≥10, no IDs, no sub-weekly timestamps. **Irreversibility notice displayed at this screen:** "Once your anonymised data has been included in a published study, it cannot be individually withdrawn from that dataset. However, because NeuroPulse anonymises your data fresh from your device for each study, withdrawing consent immediately and permanently stops any further data flowing to any future dataset — including data from sessions that occurred before your withdrawal." | Per-category and per-project process applies. | Blanket patients kept engaged — not taken for granted |
| L4 — Results + community | Hear study results? Join suggestion portal? | Plain-language results notification per study (including null results) + paper link + "suggest next steps" link. Access to suggestion/voting/pledge portal. | No results contact, no portal. | Results notification is the highest-value brand moment |

**POA workflow:** POA holder uploads executed healthcare POA → human review 3 business days → jurisdiction flagging → scope limitation noted → annual re-verification. If patient regains capacity, all proxy consent decisions presented for ratification or revocation. Research contact goes to POA holder only.

**Vulnerable population disclosure:** At per-project consent time, explicitly state: "Once your anonymised data is included in a study, individual withdrawal is not possible from that dataset — this is a fundamental property of k-anonymised aggregate data and is required by Common Rule (45 CFR 46). However, because NeuroPulse anonymises your data fresh from your device for each new study, withdrawing consent immediately and permanently prevents any further data from flowing to any future dataset — including data from sessions that occurred before your withdrawal. Your historical sessions remain on your device under your sole control."

### 6.3 Research suggestion portal (three functions)

1. **Patient research agenda:** Patients submit study ideas in plain language, community votes ("interested"), comments, expresses participation intent. Top suggestions visible to researcher community.

2. **Pre-identified subject pool:** "Would participate" intent flag creates pre-screened, device-familiar, motivated cohort. Researcher portal shows willing participant count, geographic distribution, anonymised device usage profiles per suggestion. Solves researchers' hardest problem (recruitment = 40–60% of trial cost) before grant is written.

3. **Crowdfunding catalyst:** Pledges ($10–$100+) are intent, not charges. When researcher confirms pilot feasibility, formal campaign activates. Escrow held until target met; refunded if not. Released to institution research account. NeuroPulse contribution matching for strategic studies. Pilot data (even n=20–30) supports NIH SBIR/R21 application. Funders receive results notification + paper acknowledgement as "NeuroPulse Patient Research Fund contributors."

**Per-project contact workflow:**
1. NeuroPulse reviews study (use case library, minimum necessary data, IRB verification)
2. Eligible patient list generated by device ID + contact prefs only (no UHDR)
3. Personalised invitation from NeuroPulse (not researcher) — personal tone, specific about study, explicit about what researchers CAN and CANNOT see
4. Patient decision: Yes / No / Ask a question (secure message to NeuroPulse liaison, 2 business day response). Invitation includes irreversibility notice: data already included in published studies cannot be individually removed; consent withdrawal blocks all future data flows from any time period.
5. Results notification closes loop for all who opted in (including null results). Users who later withdrew consent still receive results for studies they previously participated in — notification only, no new data.
6. Consent withdrawal effect: device immediately stops processing study descriptors; no further extracts generated or transmitted, for any data period including historical sessions.

---

## 7. DURABILITY + MAINTENANCE (all locked)

### 7.1 Critical design changes (must be in tooling specifications before first cut)

| Change | BOM delta | Why critical |
|--------|-----------|-------------|
| ITO → AgNW conductive lens coating | +$8–12/lens | ITO: 0.5% strain-to-failure, cracks on flex or point impact. AgNW: 5–10% flex, compatible with hard coat, maintains 85–90% transmission |
| Hard clamshell case (replaces soft pouch) | +$8–14 | Lens scratching certain within first month without case. Includes probe dock. Doubles as shipping container. |
| Intranasal probe hub dock (moulded) | Hub retool | Y-probe dropped probe-first fractures junction. Cannot be retrofitted. |
| Reference photodiode per zone (behind PDMS window) | +$2 total | Detects LED aging AND PDMS window fouling simultaneously. Eliminates 3-year service calibration visit. Protects J/cm² dose metering claim. |
| **Second scalp-side photodiode PD2 per zone (RISK-14 Option B)** | +$0.75–1.50/headset total | On scalp-facing PDMS surface, pin 19. PD1/PD2 ratio separates fouling (PD1↓ PD2 stable) from LED aging (both↓). T1 and T2 share identical zone module mould — firmware flag only. |
| Zone module connectors: 1,000-cycle rated (Hirose FH34S-20S-0.5SH or JAE FF03 — 0.5 mm pitch, back-flip lever ZIF, ≥1,000 insertion cycles) | +$2.00 | Molex SlimStack is a board-to-board connector — not an FPC family. Standard Molex ZIF FPC connectors rated only 20–30 cycles. Hirose FH34S confirmed as correct family. Confirm ≥1,000-cycle rating from full datasheet before BOM lock. 0.35 mm pitch insufficient — current per pin too high. Must specify before PCB layout. See NP-HW-FPC-001. |
| Lever-actuated ZIF for zone modules | Included above | Back-flip lever ZIF (Hirose FH34S mechanism) — zero insertion force, tool-free extraction. Enables user self-service zone module swaps. |
| **Zone module sliding eject lever (RISK-22 Option A)** | +$0.40/module | 10–12mm lever arm, 3:1 mechanical advantage, ≤1N extraction force. Recessed flush when closed; snap-fit detent prevents accidental ejection. 316SS hinge pin. Required for users with Parkinson's/post-stroke hand weakness. |
| **Self-sealing co-moulded silicone gasket per zone module (RISK-16 Option A)** | +$0.30–0.60/module | Shore 40–50A medical silicone, D-section 2.5×2.0mm, 20% compression when seated. No user RTV required. IPX4 compliant after 10 field swap cycles (FAI-IPX-02 BLOCKING test). Gasket retention groove + silicone primer prevent delamination. |
| **Five-layer zone module keying (RISK-15)** | Included in tooling | Layer 1: asymmetric mechanical key (unique per zone, prevents physical mis-insertion). Layer 2: ZONE_ID resistor (ZM-01=10kΩ through ZM-05=220kΩ, 1%, pin 18; firmware debounce 3×ADC at 100ms). Layer 3: ISO 17049 braille + raised numeral. Layer 4: N tactile dots on shell at each slot. Layer 5: bone conduction audio ("Frontal Left connected"). Covers colour-blind AND blind users. |
| **EEG cable routing channel in shell (RISK-21)** | $0 (tooling) | Dedicated 8×5mm moulded channel on outer CFRP surface (opposite side from FPC bundle). ≥15mm separation from zone module FPCs required (DRC-18). Must be in shell tooling spec before first cut. |
| **PDMS SiO₂ interlayer bonding process** | Process cost | 75 nm RF magnetron sputtered SiO₂ on PI surface before O₂ plasma activation. Achieves 174–860 N/m peel force. 200-cycle IEC 60068-2-14 qualification required before production. See NP-FAI-ZM-001 §3e. |
| Interface protection covers (all tethered) | +$8–9 total | Anchor posts moulded into shell at zero cost if specified before first cut. |
| Sliding rail lens mount | +$1.20 | Eliminates alignment jig. User self-install. |
| Dual-bank OTA firmware + USB-C DFU recovery | $0 (software) | Must be in bootloader from first firmware line. Cannot be added later. |
| Separate UHDR/SHDR eMMC partitions | $0 (firmware) | Must be in firmware specification before any storage architecture is written. |

### 7.2 Other locked design changes

| Change | BOM delta | Rationale |
|--------|-----------|-----------|
| Palladium-coated EMF shielding fabric | +$6/headset | Silver tarnishes 12–18 months. Palladium tarnish-immune for device lifetime. Fleet SHDR verifies stable attenuation — marketable, measurable claim. |
| N52 → N42 magnets in lens rim | −$0.80 | N52 brittle under corner drop. N42 more impact-tolerant, ≥1mm polymer wall required on all faces. |
| Braided aramid USB-C cable + dual silicone strain relief | +$3–4/cable | Commodity cables fail at strain relief within 6–18 months. 50,000+ flex cycle rating. Spare in box. |
| MagSafe hard gold contacts (>0.5µm cobalt-alloyed) | +$1.20 | Oxidised contacts → power throttling. Contact resistance monitored in SHDR. |
| 22F supercapacitor (from 10F) | +$1.80 | Allows 50% degradation over 5 years while maintaining transient absorption. |
| Industrial eMMC + LittleFS | +$2.40 | 30,000+ P/E cycles. Write endurance monitored in SHDR. |
| Bone conduction driver silicone isolator | +$1.80 | Piezoelectric element brittle — Shore 20–30A silicone mount absorbs impact. |
| Silicone over-mould at Y-probe junction | +$1.20 | Flex without fracture. Minimum 20mm bend radius marked on probe shaft. |
| Silicone potting for micro-LED array | +$2.40/lens | 1,800+ thermal cycles over device lifetime without delamination. |
| Hard coat on EC lens (3–5µm silicone) | +$4.00/lens | Standard in automotive EC mirrors. Prevents scratch damage to active EC layer. |
| Aluminium bayonet for audio cups | +$1.60 | Plastic detent flattens at 500–1,000 cycles. Metal-to-metal rated for device lifetime. |
| Moisture-barrier electrode tip hydration caps (WVTR <0.5) | +$0.80 | Extends factory-sealed electrode storage life to 24+ months. |
| Audio cup user-replaceable snap-in mesh frame | +$0.60 | Snap-in frame; user pops out, rinses, reinserts or replaces ($9.99/pair). |
| Mesh cleaning brush in box | Minimal | Standard maintenance tool for mesh surfaces. |
| Hub air filter (30-micron foam, snap-out) | +$0.45 | Captures carpet fibres in clinical environments. Hub NTC temperature trend flags cleaning need. |
| Automated nightly UHDR backup (incremental) | $0 | When on USB-C power. Failure becomes hardware swap not data loss. |
| ADS1299 self-calibration at session start | $0 | Internal reference routed to all channels. Eliminates EEG amplitude drift. Coefficients in SHDR. |
| Plasma-activated PDMS for optical windows | Minimal | Hydrophilic surface repels sebum. Anti-fouling without additional parts. |
| EC rim contact hard gold plating | Included in contact spec | EC driver monitors transition time as contact resistance proxy. Cleaning prompt when >3s transition. |
| Hub NTC thermistor | +$0.15 | Cross-calibrates headset NTCs, monitors supercapacitor aging, enables hub temperature-based SHDR alerts. |
| Enclosed PTFE-lined Boa cable channel | $0 (tooling) | Prevents hair entanglement — must be in occipital arch tooling. |
| Tool-free hub fan (quarter-turn captive fastener) | +$0.80 | No screwdriver required for fan replacement. |
| Humidity indicator card in box | +$0.30 | Visual confirmation package integrity during shipping and storage. |
| IPX4 rating target for headset | Testing cost | Splash-proof from all directions. Enables "workout-safe" marketing claim. |

### 7.3 Calibration self-maintenance

| Sensor | Self-calibration method | Residual service requirement |
|--------|------------------------|------------------------------|
| PBM photodiode | Dual-PD: PD1 (behind PDMS, forward emission) + PD2 (scalp-facing, backscatter). PD1/PD2 ratio separates fouling from LED aging. PD1↓ PD2 stable → fouling prompt. Both↓ proportional → LED aging correction. | None — dual-PD eliminates ambiguity that single PD could not resolve |
| EEG amplifier (ADS1299) | Internal reference routed to all channels at session start — gain/offset correction applied | None — fully self-calibrating |
| NTC thermistors | Hub NTC cross-calibration: compare headset NTCs vs hub reference at ambient equilibrium (>10 min since last session) | None — flag at ±1.5°C offset |
| Fluxgate magnetometers | Zero-field nulling at session start + geomagnetic field magnitude comparison via phone GPS | **3–5 year Tier B service visit** (scale factor drift requires Helmholtz test coil) |
| EC lens contacts | EC driver monitors transition time — flag when >3 seconds (vs 2s spec) | Cleaning prompt; no service visit |
| Audio cup mesh | Driver impedance monitoring — detects fouling pattern | User-replaceable snap-in frame — no service visit |

---

## 8. SERVICE NETWORK (all locked)

### 8.1 Partner tiers

| Tier | Examples | Service tasks | Certification | Equipment | Revenue/yr at scale | Launch timing |
|------|----------|---------------|---------------|-----------|---------------------|---------------|
| A — Optical centers | LensCrafters, Pearle Vision, independent opticians | S3 Rx clip manufacture + fitting (primary) · Lens replacement (standard + EC) · Calibration (secondary) | 4-hr initial · 1-hr annual online | $400–600 calibration reference (loaned) + $80–120 jig (optional with sliding rail) | $8K–35K/yr | Year 1 — already engaged via S3 programme |
| B — Electronics repair | uBreakiFix/Asurion · iFixit partners | Zone module FPC swap · DFU recovery · eMMC data recovery · Impact inspection · Fluxgate calibration | 6-hr initial · 2-hr annual practical | ESD workstation (existing) + eMMC adapter ($150–200) + DFU software (downloaded) | $4K–18K/yr | Year 2 — major metros first |
| C — Retail triage | Best Buy Geek Squad · Apple Authorized Service | Warranty intake + triage · Routing to Tier A/B · Consumable sales | 2-hr initial · 30-min annual online | None — partner app access only | $1K–7K/yr | Year 1–2 broadly — legitimacy signal |
| Depot — NeuroPulse mail-in | Backstop | All tasks · T2 same-day loaner · Precision fluxgate calibration | Full internal training | All in-house | Highest margin per task — backstop not primary | Day 1 |

### 8.2 Design changes that reduce service dependency

- Reference photodiode → eliminates 3-year PBM calibration service visit
- Sliding rail lens mount → user self-install, eliminates Tier A lens installation visit
- Lever ZIF connectors → user zone module swap, eliminates Tier B visit for upgrades
- Tool-free hub fan → user self-service
- Automated nightly UHDR backup → eliminates most data recovery emergencies

**Residual mandatory service per T1 user over 5 years:** 2–5 optician visits (Rx clip, already part of their workflow) + 0–1 fluxgate calibration + 0–2 damage-driven events.

### 8.3 Interface protection covers

Three cover types, all tethered to headset:

| Cover type | Material | Retention | Count in box | Replacement |
|-----------|----------|-----------|-------------|-------------|
| Zone slot plugs (5 per headset) | Shore 30A medical silicone, 5 colours (position-coded) | Friction/compression in slot, IP54 | 5 installed + 5 spare | 5-pack $9.99 |
| Accessory port covers (3 per headset) | Shore 40A TPE + encapsulated steel disc + Shore 20A silicone fins | N42 magnetic attraction via steel disc, ~400g pull | 3 installed + 2 spare | 3-pack $7.99 |
| Lens rim guards (2 per headset) | Shore 85A UV-stable TPU, clear | Mechanical snap-fit over rim profile | 2 installed + 1 spare pair | Pair $6.99 |

**Anchor posts:** Moulded into headset shell at zero incremental tooling cost if specified before first cut. All tethered — cannot be permanently lost without deliberate cutting.

---

## 9. COMPETITIVE POSITION

| Feature | NeuroPulse Home | NeuroPulse Pro | Vielight Neuro Pro 2 (~$5K) | Neuronic 1070 ($3K–5K) | Sens.ai (~$1.5–2K + sub) |
|---------|----------------|----------------|------------------------------|------------------------|--------------------------|
| PBM wavelengths | 660+810nm (2λ) | 660+810+1170nm (3λ) | 810nm (1λ) | 1070nm (1λ) | ~810nm (1λ) |
| Total LED count | 600 (300/wavelength) | 600 + 1170nm LDs | ~12 transcranial | 256–300 (1 wavelength) | ~7 midline |
| Peak irradiance | 400 mW/cm² pulsed* | 400 + 1,000 mW/cm² | 400 mW/cm² | Not specified | Not specified |
| Real-time dose (J/cm²) | Yes — per zone | Yes | No | No | No |
| EEG channels | 8 semi-dry 24-bit | 21 wet gel 24-bit | None | None | 3 dry midline |
| Closed-loop EEG | Yes — autonomous | Yes — all modalities | No | No | EEG→PBM only |
| BES/tACS | Yes | Yes clinical | No | No | No |
| tDCS | Yes | Yes + HD-tDCS | No | No | No |
| TMS | No | Yes focal | No | No | No |
| VNS + HRV | Auricular electrical + PPG | + cervical option | Optical VNS (separate) | No | HRV only |
| Audio entrainment | Binaural + bone conduction | + clinical EMDR | No | No | No |
| Visual stimulation | 108 LEDs/lens + EMDR + retinal PBM + Mode F | + EEG-adaptive, seizure detection | No | No | No |
| EMF shielding | 5-layer palladium + active | Same | None | None | None |
| Autonomous mobile | Yes — power bank | Yes | BT only | BT only | BT only |
| No mandatory subscription | Yes | Yes | Yes | Paywall on PLUS | Required $99–199/yr |
| Published clinical trials | None (new product)* | None (new product)* | 35+ RCTs | Limited | Ongoing |

*See §10 for pending actions on irradiance claim and evidence gap.

**Key competitive claims:**
- "50× more transcranial LEDs than Vielight at 17% of the price"
- "300 LEDs per wavelength — matching Neuronic's total LED count at each of the two CCO absorption peaks they don't cover"
- "Real-time J/cm² dose metering — the only device that shows you the exact dose your brain received"
- "Only consumer brain device with palladium-fabric EMF shielding verified by continuous fleet monitoring"
- "Autonomous closed-loop operation from any power bank — no phone required"

---

## 10. REGULATORY STRATEGY

### T1 — FDA-exempt wellness pathway
- General wellness device (same category as Muse, sens.ai, Apollo Neuro)
- Consumer naming: "Brainwave entrainment stimulation" (not tACS), "Cortical priming stimulation" (not tDCS)
- Required standards: IEC 60601-1, IEC 60601-2-10, IEC 62471, IEC 62133, FCC Part 15
- Cybersecurity: SBOM, vulnerability disclosure policy, documented OTA update approach
- FTC claims substantiation: 33-entry bibliography maps each marketing claim to supporting citations

### T2 — FDA 510(k)
- Modular predicate: TMS (NeuroStar K083538, BrainsWay K122288) + tACS (Soterix K142485, Neuroelectrics K173185) + taVNS (electroCore K163334, K173323)
- Timeline: 18–36 months from T1 launch, $2–5M budget
- QMS (21 CFR Part 820 / ISO 13485:2016): must begin NOW — cannot be retroactive
- Pre-Submission (Q-Sub) meeting with FDA at ~Month 20: free, prevents filing on avoidable grounds
- IEC 62304 software classification: Safety MCU → Class C · Main processor → Class B · App → Class B
- Clinical data: required for TMS modality; seeded T2 units into research institutions (Years 2–3) generate this data
- Human factors engineering (FDA 2016 HFE Guidance): URRA + formative + summative testing

---

## 11. CLINICAL TRIAL RESEARCHER CANDIDATES

Priority first contacts (in order):

1. **Neda Rashidi-Ranjbar** (neda.rashidi-ranjbar@unityhealth.to) — St. Michael's Hospital Toronto · 2025 MCI PBM RCT PI · early-career, device-ready, motivated collaborator · CIHR funding pathway · most direct PBM upgrade path
2. **Mayank Jog** (mjog@mednet.ucla.edu) — UCLA Brain Mapping Center · active K99/R00 NIH (MH128572) aligned to HD-tDCS protocols · JAMA Network Open 2025 RCT PI
3. **Mark George** (georgem@musc.edu, 843-876-5142) — MUSC Brain Stimulation Lab · highest-credibility TMS infrastructure · 55 active studies · 15 TMS machines on-site
4. **Margaret Naeser** (mnaeser@bu.edu) — VA Boston · foundational TBI/PTSD PBM cohort · 47 years VA-funded · existing veteran patient cohort

**SAB priority:**
- **Li-Huei Tsai** (617-324-0305, MIT Picower) — SAB role ONLY (Cognito Therapeutics conflict prevents PI role) · founded GENUS field · Nature 2016 paper is scientific basis for 40Hz visual protocol
- **Glen Jeffery** (g.jeffery@ucl.ac.uk, UCL Institute of Ophthalmology) — world's leading retinal PBM researcher · foundational 670nm human studies

Full researcher candidate list (12 researchers, 7 modalities, contact info, cost estimates, funding sources): `neuropulse_researchers.docx`

---

## 12. CLINICAL EVIDENCE BIBLIOGRAPHY

33-entry bibliography across 11 modality sections available: `neuropulse_bibliography.docx`

**Evidence summary:**
- TMS for depression: strongest — FDA pivotal trial (n=301), CPT reimbursement codes, 50–55% response rate
- EMDR for PTSD: strong — WHO recommendation, multiple meta-analyses
- EEG neurofeedback for ADHD: strong — 21 RCTs, n=1,261
- Transcranial PBM: good but concentrated in Vielight/BU/Toronto group — Rashidi-Ranjbar 2025 RCT (n=20, multimodal neuroimaging) is strongest
- taVNS: good — scoping review 109 studies (n=3,231), largest epilepsy RCT (n=150)
- 40Hz gamma: foundational animal study (Nature 2016) + Phase 2A human RCT (n=15 AD)
- **Critical gap:** No RCT of combined multi-modal protocol (PBM + BES + VNS + audio + visual simultaneously) exists anywhere

---

## 13. REMAINING WEAKNESSES + OPEN ITEMS

### 13.1 Critical — action required immediately

| Issue | Action | Cost/Timeline |
|-------|--------|---------------|
| **Zero published clinical trials** — 35-trial gap vs Vielight | Commission SBIR Phase I at company formation. First contacts: Rashidi-Ranjbar → Jog → Naeser | 2–3 years to published data |
| **400 mW/cm² regulatory opinion not obtained (RISK-03)** — cannot appear in ANY public material until cleared | Commission outside regulatory counsel (PBM/digital health specialist). Also assess Vielight comparison claim under FTC implied claim doctrine | $8,000–15,000 · 3–5 weeks |
| **"NeuroPulse" is an uncleared placeholder** — trademark not searched | Trademark search and clearance: US, EU, Canada, Australia. Required before ANY external conversation. | $15,000–25,000 |
| **CFRP shell slot rim Ra ≤ 1.6 µm unconfirmed (RISK-20)** — BLOCKING for tooling release | Obtain written confirmation from CFRP shell tooling supplier (letter + Ra measurement data from representative coupon). Supplier qualification item SUP-M-07 in NP-PROC-SUP-001. If Ra > 1.6 µm unavoidable: escalate to ME + EE; gasket or shell geometry must be revised. | $0 — supplier engagement required |
| **PDMS CAT-C supplier not selected (RISK-04)** — BLOCKING for production start | Select PDMS bonding supplier per NP-PROC-SUP-001 CAT-C criteria. Supplier must confirm IEC 60068-2-14 thermal cycling qualification capability. 200-cycle qualification (FAI-TC02) must pass before any production FPCs are built. | Supplier lead time 8 weeks from selection |

### 13.2 Moderate

| Issue | Status |
|-------|--------|
| SAB not formed — no scientific credentialing | Tsai outreach at 617-324-0305 (SAB role only). Jeffery and Naeser natural SAB candidates. Budget $50,000–80,000/yr for 5-person SAB. |
| Vulnerable population withdrawal edge case in research consent | **Resolved and locked** — irreversibility notice added at L3 blanket consent screen AND at per-project invitation (step 4). Forward-effectiveness guarantee added: on-device fresh-per-study anonymization makes consent withdrawal fully effective for all data periods. |
| 45W charger in box | **Decided and locked** — included in BOM across all configurations at appropriate wattage. Weakness resolved. |
| Zone module mould complexity (RISK-23) | NP-TOOL-ZM-001 created consolidating all 8 moulded features (F-01 through F-08). 12-item mould design review checklist must be completed before steel is cut (NP-COORD-001 G1-05). |

### 13.3 Structural (accepted, managed)

| Issue | Mitigation |
|-------|-----------|
| Fluxgate calibration requires Tier B service visit (every 3–5 years) | Geomagnetic comparison detects severe drift. Fleet SHDR predicts need with months of lead time. 3–5 year interval means Tier B network established before first visits needed. |
| iOS/Android OS update dependency | Apple/Google developer beta participation. 7-day OS compatibility SLA. Autonomous Mode 3 as structural fallback. |

### 13.4 Pending decisions — must resolve before tooling is cut

- [ ] Product name trademark clearance
- [ ] 400 mW/cm² regulatory opinion letter (RISK-03 — BLOCKING for all public material)
- [ ] LED emitter pulse current rating verification (660nm + 808–830nm FPC candidates at 120–180mA)
- [x] Zone module FPC layout specification — freeze layout so PD2 aperture position (F-04 in NP-TOOL-ZM-001) can be specified in mould design
- [ ] Zone module mould design review — NP-TOOL-ZM-001 §5 checklist (all 8 features F-01 through F-08) signed off before steel cut
- [ ] **CFRP shell slot rim Ra ≤ 1.6 µm — written supplier confirmation (RISK-20 BLOCKING)**
- [ ] **PDMS CAT-C supplier selection + 200-cycle IEC 60068-2-14 thermal cycling qualification (RISK-04 BLOCKING)**
- [ ] ZONE_ID firmware debounce spec written into firmware requirements document (3×ADC at 100ms, ≥2/3 pass)
- [ ] HFE formative study for sliding eject lever — 5 subjects with Parkinson's/post-stroke (NP-TOOL-ZM-001 OI-4, NP-FAI-ZM-001 FAI-A15)
- [x] EEG cable routing path in shell CAD model — OI-09 in NP-DRV-SHELL-001 (required for DRC-18 verification, RISK-21)
- [ ] Hub tooling: probe dock + anchor posts + large-radius Boa cable channel + tool-free fan (quarter-turn captive fastener)
- [x] Shell tooling: anchor posts for all interface covers (5 zone + 3 port positions, colour-coded for zones) + EEG cable channel (§2.4) — **NP-TOOL-SHELL-001 Rev A written 2026-05-10; CAD verification (OI-01–OI-07) and temporal wing boss (F-04, PROVISIONAL) specified; see `docs/neuropulse_tool_shell_001.docx`**
- [x] Lens tooling: sliding rail + N42 magnet positions (≥1mm polymer wall all faces) + AgNW spec + hard coat + EC driver contacts — **NP-TOOL-LENS-001 Rev A created (2026-05-10)**
- [x] Goggle arm tooling: anchor hook for lens rim guard tether — **covered in NP-TOOL-LENS-001 Rev A (F-07)**
- [ ] Partner optician network contract (S3 Rx programme)
- [ ] eMMC partition architecture + separate UHDR/SHDR encryption in firmware specification (before any storage code is written)
- [ ] Dual-bank OTA bootloader (must be from first firmware line)
- [ ] SBIR Phase I application
- [ ] First researcher contacts
- [ ] SAB formation (Tsai outreach first)

### 13.5 Completed and locked decisions

- Two-tier product strategy with shared platform
- 600-LED FPC zone modules from launch (no Rev A/Rev B)
- 400 mW/cm² peak pulsed via firmware (Path A, $0 BOM) — pending regulatory opinion
- 21-channel EEG montage with FC3/FC4/Oz/A1/A2
- STM32G071 safety MCU (not G031)
- No NFC anywhere — optical code + resistive pogo authentication
- USB-C wired-first, BT/Wi-Fi toggle, antennas in hub not headset
- Boa dial fit system with spring-decoupled pods
- 5-layer passive + active Helmholtz EMF shielding with palladium fabric
- UHDR/SHDR terminology framework — complete separation, never linked by design
- Clinical consent engine: use case library → minimum necessary data mapper → plain-language consent document
- Research partnership: a priori consent (4 layers), results opt-in, suggestion portal, crowdfunding
- POA workflow with human document review
- Service network: Tier A optical + Tier B electronics + Tier C retail + depot
- All interface protection covers tethered to headset
- Predictive maintenance system via SHDR fleet telemetry
- Charger policy: auto-include correct charger at every upgrade + $19 upfront 65W option at checkout
- All durability changes listed in §7
- **FPC 20-pin pinout locked:** Pin 18 = ZONE_ID (resistor per zone, 1% 0402: ZM-01=10kΩ, ZM-02=22kΩ, ZM-03=47kΩ, ZM-04=100kΩ, ZM-05=220kΩ); Pin 19 = PD2_CATHODE (scalp-side reference photodiode, RISK-14 Option B)
- **Zone module dual photodiode (RISK-14 Option B):** PD2 on scalp-facing surface, pin 19; PD1/PD2 ratio separates fouling from LED aging; T1/T2 identical mould
- **Five-layer zone module keying (RISK-15):** mechanical key + ZONE_ID resistor + braille/numeral + shell tactile dots + bone conduction audio; works for colour-blind and blind users
- **Self-sealing gasket (RISK-16 Option A):** co-moulded Shore 40–50A silicone, no user RTV, IPX4 rated after 10 swap cycles
- **Sliding eject lever (RISK-22 Option A):** 10–12mm, 3:1 mechanical advantage, ≤1N extraction force; accessibility target: Parkinson's Hoehn & Yahr II–III
- **ZONE_ID firmware debounce:** 3× ADC reads at 100ms intervals before FAULT; ≥2/3 must pass (RISK-18)
- **PDMS bonding process:** SiO₂ 75nm interlayer + O₂ plasma, ≥150 N/m peel strength; 200-cycle IEC 60068-2-14 qualification BLOCKING before production (RISK-04)
- **EEG cable routing (RISK-21, OI-09 CLOSED — NP-DRV-SHELL-001 Rev B, 2026-05-10):** Dedicated 8×5mm main trunk + 6×4mm lateral branch channels on outer CFRP inner surface (opposite side from FPC bundle, scalp-side). Routing: Hub PCB EEG connector → occipital arch main trunk (~120mm) → crown junction → left branch (P3/C3/F3/Fp1) and right branch (P4/C4/F4/Fp2), each ~90–200mm arc. Shell wall routing achieves 18–22mm point-to-point FPC-to-EEG separation — DRC-18 PASSES without barrier. Snap-in retention clips at 80mm intervals. Channel spec handed to shell tooling supplier. DRC-22 CLOSED. DRC-23 CLOSED. G2-11 CLOSED in NP-COORD-001.
- **Multi-FPC bundle management (RISK-17):** ≥2mm inter-FPC separation; ≥15mm FPC-to-EEG (or grounded Al foil barrier); 3 anchor bosses per FPC; all 5 Hub ZIF connectors on same PCB edge
- **Risk register documented:** 24 risks total (RISK-01 through RISK-24); 22 MITIGATED; 2 OPEN: RISK-03 (regulatory opinion, external) and RISK-20 (CFRP Ra confirmation, external)
- **HRV biofeedback protocol (software only):** Resonance frequency breathing pacer (6 breaths/min default, personalised sweep); real-time coherence score; four protocols (standalone, HRV+taVNS synchronised, HRV+EEG dual biofeedback, HRV+PBM); no additional hardware; bone conduction delivers breathing cue; uses existing VNS clip PPG. Locks the VNS+HRV modality as the only NeuroPulse modality with multi-modal trial evidence (2025 RCT: PBM + qEEG NF + HRV biofeedback simultaneously).
- **sLORETA-guided HD-tDCS (T2):** 4×1 ring montage; Ag/AgCl 3.5mm dual-rated electrodes in T2 qEEG cap; 16-ch tACS driver provides independently controlled channels (no additional hardware); sLORETA source map → MNI target → automatic 10-20 electrode mapping → personalised current distribution; 40µC/cm² safety MCU limit; ≤2mA/electrode; Jog/UCLA 2025 and BRIGhTMIND 2024 as clinical evidence base.
- **Cervical VNS (T2 accessory):** Neck-worn tcVNS module, carotid sheath stimulation; safety MCU owns enable with cardiac monitor interlock; 510(k) predicate = electroCore gammaCore K163334/K173323; gel pad consumable; +$35–55 BOM.
- **Research data anonymization architecture:** On-device, per-study, fresh per request. NeuroPulse never holds or accesses raw UHDR at any point (biometric-derived key never leaves device). Consent withdrawal is immediately effective for all future data flows from any time period. Irreversibility notice given at L3 blanket consent + per-project invitation. Audit trail of contributed studies in SHDR (user-readable, never shared with researchers).
- **FPC layout frozen (NP-HW-FPC-001 Rev D, 2026-05-09):** PD2 (RISK-14 Option B) position locked at X = 33.0 mm, Y = 39.0 mm from module reference corner (geometric centre of 66 × 78 mm LED array, co-located in XY with PD1 on opposite face). FPC contact pad: 1.6 mm annular ring, scalp-facing copper layer, hard gold ≥ 0.5 µm. F-04 aperture in NP-TOOL-ZM-001 updated with this coordinate (±0.2 mm tolerance). OI-3 closed. NP-COORD-001 G1-15 closed. DRC-18 CAD overlay verified (EEG routing locked — G2-11 closed, OI-09 closed).
- **Lens and goggle assembly tooling (NP-TOOL-LENS-001 Rev A, 2026-05-10):** All five locked design changes consolidated: (1) Sliding rail lens mount F-01/F-02 — T-slot groove in lens rim + tongue in goggle arm, user self-install, ≤5N extraction, 500-cycle rated, eliminates Tier A alignment jig; (2) N42 magnet pockets F-03/F-04 — 6× per lens rim, ≥1mm polymer wall all faces (BLOCKING verification), 10,000-cycle snap durability, polarity pattern enforces shade orientation; (3) EC driver contacts F-05/F-06 — 2× hard gold ≥0.5µm pads in lens rim + BeCu pogo pins in goggle arm, ≤50mΩ, EC transition time >3s flags contact degradation in SHDR; (4) AgNW outer coating P-01 — replaces ITO (ITO 0.5% strain-to-failure vs. AgNW ≥5%), 20–30Ω/sq, ≥82% transmission at 660nm and 808nm (Mode F PBM wavelengths), supplier qualification BLOCKING; (5) Hard coat P-02 — 3–5µm silicone, ≥3H pencil hardness, applied over AgNW (outer) and EC film (inner on EC lens); (6) Inner PDMS diffuser P-03 — plasma-activated anti-fouling, SiO₂ interlayer on PC substrate (separate qualification from PI substrate result); (7) Lens rim guard anchor hooks F-07 — integral to goggle arm, tether lanyard attachment. 18-item mould design review checklist; lens rim + goggle arm reviews concurrent; 9 open items tracked.

---

## 14. DOCUMENTS GENERATED

| Document | Location | Contents |
|----------|----------|---------|
| Design Brief Revision 1 | `docs/neuropulse_design_brief.docx` | Initial complete design specification |
| Design Brief Revision 2 | `docs/neuropulse_design_brief_r2.docx` | Updated with LED count, irradiance, EEG, EMF decisions |
| Design Brief Revision 3 | `docs/neuropulse_brief_r3.docx` | Adds UHDR/SHDR, consent systems, durability, service network |
| Design Brief Revision 4 | `docs/neuropulse_brief_r4.docx` | Current — zone module engineering package; dual-PD, keying, gasket, lever, PDMS qual, 24-risk register |
| Clinical Evidence Bibliography | `docs/neuropulse_bibliography.docx` | 33 entries, 11 modality sections, DOI links, NeuroPulse-specific summaries |
| Researcher Candidate List | `docs/neuropulse_researchers.docx` | 12 researchers, 7 modalities, contact info, cost estimates, funding sources |
| SBIR Phase I Draft | `docs/neuropulse_sbir_phase1_draft.docx` | NIH SBIR Phase I proposal draft; references Hirose FH34S (not Molex SlimStack) |
| FPC Zone Module Specification | `docs/neuropulse_fpc_zone_module_spec_revA.docx` | NP-HW-FPC-001 Rev C — 20-pin pinout, dual-PD architecture (§8.4), PDMS bonding (§9), thermal cycling qualification (§9.3), multi-FPC routing (§11.3), five-layer keying (§11.4), gasket (§12) |
| FPC Procurement Requirements | `docs/neuropulse_fpc_procurement_requirements.docx` | NP-PROC-FPC-001 Rev A — LED Vf binning, Hirose FH34S exclusions, BCR421W spec, RA copper |
| Zone Module Risk Register | `docs/neuropulse_fpc_zone_module_risks_revA.docx` | 24 risks (RISK-01 through RISK-24); 22 MITIGATED; RISK-03 and RISK-20 OPEN |
| Shell FPC Routing Review | `docs/neuropulse_shell_fpc_routing_review.docx` | NP-DRV-SHELL-001 Rev B (2026-05-10) — bend radius, multi-FPC bundle mgmt (§2.3), EEG cable routing (§2.4), 23-item DRC checklist; §2.4.5 locked routing path (OI-09 CLOSED, DRC-22 CLOSED, DRC-23 CLOSED) |
| FAI Zone Module Checklist | `docs/neuropulse_fai_zone_module.docx` | NP-FAI-ZM-001 Rev A — §3d PDMS adhesion (FAI-M01–M03), §3e PDMS thermal cycling qualification (FAI-TC01–TC06, TC02 BLOCKING), §4a accessibility (FAI-A09–A15), §4c IPX4 (FAI-IPX-01–04, IPX-02 BLOCKING), §5 lifecycle, §6 system test, §9 risk cross-reference |
| Engineering Coordination Checklist | `docs/neuropulse_eng_coordination_checklist.docx` | NP-COORD-001 Rev A.2 (2026-05-10) — G1 (14 items), G2 (11 items), G3 (6 items) gate structure; G2-11 CLOSED |
| Zone Module Tooling Specification | `docs/neuropulse_tool_zone_module_001.docx` | NP-TOOL-ZM-001 Rev A — 8 mandatory moulded features (F-01 through F-08), critical dimensions, 12-item mould design review checklist, FAI cross-reference |
| Supplier Selection Checklist | `docs/neuropulse_supplier_selection_checklist.docx` | NP-PROC-SUP-001 Rev A — CAT-A (moulding), CAT-B (CFRP shell), CAT-C (PDMS bonding); SUP-M-07 and SUP-B-01 BLOCKING for RISK-20; SUP-C-08 BLOCKING for RISK-04; §9 RISK-20 tracking table |
| Clinical Trials Strategy | `docs/neuropulse_clinical_trials_strategy.docx` | NP-CLIN-001 Rev A — all-in-one commercial rationale; Vielight trials scope/limitation; individual modality evidence (9 modalities); multi-modal combination evidence; researcher profiles (9 researchers); 6-trial priority plan; key bibliography |
| Additional Modalities | `docs/neuropulse_additional_modalities.docx` | NP-MOD-EXT-001 Rev A — 6 modalities not in current stack: 40Hz vibrotactile (HIGH priority — await HOPE results), HRV biofeedback (HIGH priority — software only, no BOM), 1064nm PBM (MEDIUM — watch UT Dallas), tFUS (LOW — 5–8yr horizon), sLORETA-guided HD-tDCS (MEDIUM T2), cervical VNS (MEDIUM T2) |
| Shell Tooling Specification | `docs/neuropulse_tool_shell_001.docx` | NP-TOOL-SHELL-001 Rev A — 4 mandatory shell features: F-01 zone slot plug anchor posts (×5, colour-coded), F-02 accessory port cover anchor posts (×3), F-03 EEG cable routing channel (8×5mm, outer CFRP surface, DRC-18 cross-ref), F-04 temporal wing anchor boss (×2 bilateral, PROVISIONAL pending HOPE Phase 3). 23-item design review checklist (SH-01–SH-23), critical dimensions, FAI cross-reference. Addresses Issue #16 and §13.4 pending decision. |
| Lens and Goggle Assembly Tooling Specification | `docs/neuropulse_tool_lens_001.docx` | NP-TOOL-LENS-001 Rev A — 7 features (F-01 through F-07 + P-01 through P-03) across lens rim, goggle arm, shade assembly tooling; sliding rail geometry (F-01/F-02); N42 magnet pockets (F-03/F-04, ≥1mm wall BLOCKING); EC driver contacts (F-05/F-06, hard gold, pogo pin); AgNW coating spec (P-01, replaces ITO); hard coat spec (P-02, 3–5µm silicone); inner PDMS diffuser (P-03, plasma-activated); lens rim guard anchor hooks (F-07); 18-item mould design review checklist; 9 open items; FAI cross-reference; risk register cross-reference. Addresses Issue #17. |

---

## 15. MARKETING NOTES

### Mastoid LRA pad vs Apple Watch — messaging for marketing literature

**Framing:** Lead with why our add-on is better, not with why Apple Watch is worse. Acknowledge the Apple Watch sync app exists and is available — but be clear about what it does and doesn't do.

---

**Draft marketing copy (vibrotactile accessory page / FAQ):**

*"We offer a free Apple Watch sync app that delivers 40Hz haptic feedback in time with your NeuroPulse session — and you're welcome to use it. But if you want results that match the science, the NeuroPulse vibrotactile pad is the right tool.*

*Here's the difference: the mastoid pad is a purpose-built linear actuator positioned at the bone just behind your ear. That placement matters — the mastoid process couples vibration directly into the skull and temporal bone, putting the 40Hz signal right where the brain research was done. The Apple Watch sits on your wrist. Getting a 40Hz vibration from your wrist to your somatosensory cortex means travelling through soft tissue, tendons, and bone across your entire arm and shoulder. Most of the signal doesn't make it.*

*The pad also delivers 40Hz ± 0.5Hz precision. Apple's Taptic Engine was designed for notification taps, not continuous therapeutic vibration — its output at 40Hz is uncharacterised and variable. Our driver IC runs open-loop at a locked frequency with calibrated amplitude, matched to the laboratory protocol.*

*One more practical difference: running continuous haptics on an Apple Watch for 20 minutes will drain a significant portion of its battery. The mastoid pad draws its power from the NeuroPulse hub.*

*Use the Watch app if you want an extra sensory layer on top of the pad. Use it alone if you're curious. But if 40Hz vibrotactile therapy is the goal, the mastoid pad is what delivers it."*

---

**Key messages (bullet form for web/app copy):**
- Purpose-built for the mastoid — where bone meets skull, where the science was done
- 40Hz ± 0.5Hz locked precision vs uncharacterised wrist haptics
- Powered by the NeuroPulse hub — doesn't drain your Watch battery
- The Apple Watch sync app is a free bonus, not the full experience

---

## 16. NAMING CONVENTION CHANGES

**Retired term:** "Health Data Record (HDR)" — ambiguous, replaced throughout all documents

**Replacement:**
- `UHDR` = User Health Data Record (user's property, never accessed by NeuroPulse)
- `SHDR` = System Health Data Record (NeuroPulse property, device-linked only, never user-linked)

Both terms appear in full on first use in each document, abbreviated thereafter.

---

*This CLAUDE.md is the authoritative project memory file for the NeuroPulse design programme. All decisions marked as "locked" or "decided" have been through full design review. Decisions marked "pending" require resolution before first tooling cut. Update this file when any locked decision changes.*
