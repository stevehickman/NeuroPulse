# CLAUDE.md — NeurOne Design program
**Project:** NeurOne — closed-loop multi-modal neuromodulation wearable platform  
**Revision:** 34 (current)  
**Status:** Pre-tooling design phase. No hardware committed yet. All decisions below are locked unless explicitly noted as pending.

> **Rev 34 (2026-08-11) — zone-module-era documents retired; document conventions changed. No design decision changed.** Four documents that specified, inspected, risk-assessed or tooled the retired 5-zone-slot module were moved to `docs/superseded/` and replaced per artifact: `NP-FAI-ZM-001` → `NP-FAI-001` + `NP-ART-001` + `NP-FAI-HUB-001`; **`NP-RISK-001` — which was the ISO 14971 baseline risk file, not merely a stale spec** → `NP-RISK-002` (disposition of all 26 RISK IDs — 5 retired, 20 carried, 1 closed-confirmed) + `NP-RISK-003` + `NP-RISK-004`; `NP-DRV-SHELL-001` → `NP-DRV-SHELL-002` (already) + `NP-REV-SHELL-001` (the review *record*, which had no successor); `NP-TOOL-ZM-001` → `NP-TOOL-HEXTILE-001`. **Every retired and superseded document now lives in `docs/superseded/`**, indexed with its successor. **Three conventions changed (`NP-CONV-001` Rev 3 §4):** the **filename of a controlled document is its serial and nothing else** (§4.0), and the rule is *exclusive* — nothing that is not a serialed document may look like one — enforced by `bun scripts/check-doc-filenames.ts`; 25 files were renamed to their serial (`neurone_shell_fpc_routing_review.docx` → `np_drv_shell_001.docx`, and 24 more). Vendored files are the sole exception (§4.0.3). Also: document revisions are now **positive integers**, not letters (the alphabet had run out at `Rev AA`), with a published positional mapping so historical citations still resolve — PCB, database-schema and vendor revisions deliberately keep letters; and **no active filename carries revision information** (`neurone_design_brief_r5.docx` → `np_db_005.docx`). **Read `docs/np_art_001.md` before asking whether an artifact has a spec** — it records that nine of fifteen manufactured artifacts have no owning document, four of them shipping T1 modalities.

> **Rev 33 (2026-07-15) — structural reorganization.** This core file now holds only the always-relevant invariants (product, config, modalities, hardware, data architecture, consent). The large archival/reference sections were moved to subsidiary files under `docs/` and are listed in the Document Map below — read them on demand only when a conversation needs them. This keeps the context loaded every session small. **No design decisions changed; content was relocated, not edited.** When a locked decision changes, update the relevant subsidiary file (and note it in `docs/status/completed-decisions.md`).

---

## 📂 DOCUMENT MAP — where everything lives

The core sections (§1–§6, §16) stay in this file — they are invariants that matter in most conversations. Everything else is a plain path (not an `@import`), so it loads only when I `Read` it for a task that needs it.

| Topic | Read when… | File |
|-------|-----------|------|
| **Product overview** | always available | §1 below |
| **Configurations + pricing** | always available | §2 below |
| **Modality stack** | always available | §3 below |
| **Hardware specifications** | always available | §4 below |
| **Data architecture (UHDR/SHDR)** | always available | §5 below |
| **Clinical consent engine** | always available | §6 below |
| **Naming conventions (UHDR/SHDR)** | always available | §16 below |
| **Naming + notation conventions** (signal names, `§N`, document IDs, identifier families) | authoring or revising ANY doc, naming a signal, or reviewing an interface | `docs/np_conv_001.md` |
| Optional accessories + companion SW (mastoid pad, Watch app) | working on accessories / app roadmap | `docs/reference/accessories-roadmap.md` |
| Durability + maintenance design changes | tooling / BOM / mechanical work | `docs/reference/durability-maintenance.md` |
| Service network (partner tiers, covers) | service / warranty / logistics work | `docs/reference/service-network.md` |
| Competitive position + claims | marketing / positioning / claims work | `docs/reference/competitive-position.md` |
| Regulatory strategy (T1 wellness / T2 510k) | regulatory / QMS / standards work | `docs/reference/regulatory-strategy.md` |
| Clinical researchers + evidence bibliography | clinical trials / evidence / researcher outreach | `docs/reference/clinical.md` |
| PBM optical resolution floor (what boundary the hardware can actually produce) | zone sizing, lateralized protocols, any "targets region X" claim | `docs/np_opt_psf_001.md` |
| Marketing notes / draft copy | marketing copy work | `docs/reference/marketing-notes.md` |
| **Open items / pending decisions** (was §13.1–13.4) | checking what's blocking / unresolved | `docs/status/pending-decisions.md` |
| **Completed + locked decisions log** (was §13.5) | checking whether/how something was decided | `docs/status/completed-decisions.md` |
| **Document + firmware register** (was §14) | locating a spec doc or firmware module | `docs/status/document-register.md` |
| Formal DHF index (source of truth for design records) | 510(k) / design-control work | `docs/np_dhf_001.md` |
| **Manufactured artifact register + documentation readiness** (what we build; which tooling spec / risk register / FAI checklist exists, and what blocks the rest) | asking "does X have a spec / an FAI / a risk register yet?", or planning tooling work | `docs/np_art_001.md` |
| FAI programme (method, issue conditions, PDMS-bond + ingress qualifications) | writing or running any first article inspection | `docs/np_fai_001.md` |
| Risk file — index + disposition of the retired RISK-01…26 register | any ISO 14971 / hazard question | `docs/np_risk_002.md` |
| Risk registers — hex-tile module · shell/socket/interconnect/hub | per-artifact hazard work | `docs/np_risk_003.md` · `docs/np_risk_004.md` |
| Hex-tile mould tooling specification | tile mould / BOM / mechanical work | `docs/np_tool_hextile_001.md` |
| Shell interconnect design review record (gates shell tooling first cut) | shell tooling release | `docs/np_rev_shell_001.md` |
| **Retired documents** — index with successors named | tracing why something changed | `docs/superseded/README.md` |

> Note: `docs/status/completed-decisions.md` and `docs/status/document-register.md` overlap heavily with the formal DHF index (`docs/np_dhf_001.md`) and git history. They were relocated faithfully; a dedup pass against the DHF is a flagged follow-up (see the header notes in those files).

---

## 1. PRODUCT OVERVIEW

Two-tier platform sharing a single chassis, processor stack, app, and USB-C connectivity.

| Tier | Name | Regulatory | Modalities | Price range | Timeline |
|------|------|-----------|------------|-------------|----------|
| T1 | NeurOne Home | FDA-exempt wellness | 8 | $449–$1,199 | 12–18 months |
| T2 | NeurOne Pro | FDA 510(k) target | 11 | $4,999–$13,999 + $1,800/yr | 18–36 months post T1 |

**Founding design principles:**
- Shared platform (one production line, two markets)
- Wired-first USB-C default (zero RF at scalp)
- Autonomous closed-loop EEG-adaptive stimulation without phone (primary competitive moat)
- 5-layer EMF shielding + active Helmholtz cancellation (only consumer brain wearable with measured shielding)
- Modular field-upgradeability via snap-in zone modules
- No mandatory subscription — all core functions offline-capable permanently
- UHDR/SHDR data separation (user health data never accessed by NeurOne)

---

## 2. CONFIGURATIONS + PRICING (all locked)

### 2.1 Integrated system configurations

| Config | BOM | COGS | Retail | GM% | Modalities included |
|--------|-----|------|--------|-----|---------------------|
| Core — EEG only | $168–169 | $258–260 | $449 | 42% | 4-ch EEG · all connectivity · EMF shielding · processor stack · 8GB eMMC |
| Home Lite | $265–266 | $370–372 | $599 | 38% | Core + PBM tiles (660+810nm) · 8-ch EEG · VNS+HRV clip |
| Home Standard ★ (flagship) | $405 | $540 | $849 | 36% | All T1 modalities (see §3) |
| Home Premium | $460 | $622 | $1,199 | 48% | All T1 + EC lens (+$89 value) · 2yr warranty · priority support |
| Pro Entry | $833 | $1,365 | $4,999 | 73% | All T1 + 21-ch qEEG · 1170nm deep PBM · clinical tACS · HIPAA cloud · sLORETA |
| Pro Full | $1,506 | $2,628 | $13,999 | 81% | All T2 + TMS hub · multi-patient dashboard · scripting API · FHIR R4 · $1,800/yr service |

**★ Home Standard box contents:** All T1 modules · hard clamshell case · braided aramid USB-C cable (spare in box) · **45W NeurOne branded GaN charger** · S1 opaque shade · interface covers (installed + spare set each type) · mesh cleaning brush · Boa replacement cable + hook tool · moisture-barrier electrode tip hydration caps · humidity indicator card · pre-impregnated cleaning cloth packets

### 2.2 Charger policy (locked)

Charger scaled to peak draw of configuration. Auto-included at every upgrade by serial number tracking. Upfront 65W upgrade option ($19 at-cost) offered at checkout as intent signal.

| Config | Charger included | BOM |
|--------|-----------------|-----|
| Core | 15W USB-C (unbranded) | $3–4 |
| Home Lite | 30W GaN (unbranded) | $5–6 |
| Home Standard ★ | 45W NeurOne GaN (branded) | $10 |
| Home Premium | 45W NeurOne GaN (branded) | $10 |
| Pro Entry | 65W NeurOne GaN (branded) | $13 |
| Pro Full | 65W NeurOne GaN (branded) × 2 | $26 |

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

> Optional/provisional accessories (40Hz mastoid vibrotactile pad, Apple Watch sync app) live in `docs/reference/accessories-roadmap.md`.

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

## 4. HARDWARE SPECIFICATIONS (all locked)

### 4.1 Processor stack
- **Main:** NXP i.MX RT1062 · Cortex-M7 · 600MHz · FPU+DSP+SIMD · 1MB on-chip SRAM + 32MB LPSDR4 · USB-HS OTG · FreeRTOS-Kernel V11.3.0 (LTS 202604.00, vendored `firmware/vendor/freertos/`) · ~1.1% CPU at full load (98.9% headroom for future ML)
- **Safety MCU:** STM32G071 (NOT G031 — G031 has only 8KB SRAM, insufficient for EMF firmware) · Cortex-M0+ · 64MHz · 36KB SRAM · 128KB flash · bare-metal · owns all stimulation GPIO enable lines · +$0.45 BOM
- **Storage:** 8GB industrial eMMC (SLC cache, 30,000+ P/E cycles) · LittleFS filesystem · firmware partition write-protected · separate UHDR/SHDR partitions from first firmware line
- **Connectivity:** USB-C 3.2 Gen1 (default, zero RF, <1ms) · BT 5.3 LE Audio · Wi-Fi 6 · antennas in control hub NOT headset · single rear toggle

### 4.2 Safety architecture
- Safety MCU physically owns all stimulation enable GPIO — app crash cannot cause unsafe stimulation
- SPI heartbeat from main processor every 200ms; 1.5s watchdog → all-stimulation cutoff <50ms
- Dual-processor isolation: IEC 62304 Class C (safety MCU, bare-metal — ~1,600 physical lines across 9 modules as of 2026-08; `wc -l firmware/safety_mcu/src/*.c` is the source of truth, not this line) + Class B (main processor) separately certified
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
| Cervical VNS (T2) | Cardiac rhythm interlock | Safety MCU owns enable GPIO; monitors R-peak GPIO; HR change >15 BPM within 5s → GPIO cutoff <100ms; 30s re-enable lockout + app confirm + repeat impedance |
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
- NeurOne access: **NEVER** — not for support, engineering, research, or regulatory submission
- Clinician access: per-element, per-use-case, time-limited, audited, revocable
- Researcher access: anonymized aggregate only, separate IRB + explicit research consent
- Defining test: does this record tell us something about the **person**? If yes → UHDR
- Contents: EEG waveforms (all channels) · HRV time series · PPG optical signal · neurofeedback performance scores · session timestamps and duration · protocol parameters used · closed-loop adaptation events · PBM dose (J/cm²) per zone · user-entered symptom/outcome logs · eye-open/closed state during sessions
- Storage: on-device eMMC UHDR partition, AES-256 encrypted with user biometric-derived key (NeurOne does not hold decryption key)
- Backup: automated nightly incremental backup to USB-C local or E2E encrypted cloud (user-held key) when on USB-C power

**SHDR — System Health Data Record**
- Owner: NeurOne
- Linked to: device ID + opaque TRNG warranty token **only** — never to user identity
- **Consent subject: warranty owner** (the entity who registered warranty — may be a clinic, institution, or individual purchaser; is NOT assumed to be the person wearing the device). Warranty consent is entirely separate from user research consent. A clinic staff member activating warranty is not consenting on behalf of any patient.
- Defining test: does this tell us about the **device's condition**, with nothing that reveals user biology? If yes → SHDR
- Contents: LED output ratio per zone · NTC temperature profiles · EMF shielding attenuation ratio · device session count (unsigned integer, no timestamps) · consumable session counts · USB-C insertion counter · PD negotiation log · impact events (g-force, orientation — between sessions only) · fan RPM · supercapacitor cycles · firmware version history · OTA log · accessory authentication pass/fail · calibration coefficient history
- Storage: on-device eMMC SHDR partition, separate encryption from UHDR
- Upload: to NeurOne fleet database on USB-C connect (warranty owner consent required at device registration; unrelated to user research consent)

**Boundary case resolution rule:** When in doubt → UHDR. Reclassification requires positive demonstration of no user biology content.

Specific boundary resolutions:
- Raw EEG impedance → UHDR; derived trend slope → SHDR
- Accelerometer during active sessions → UHDR; impact events between sessions → SHDR
- Raw ambient light → UHDR; cumulative UV exposure index → SHDR
- Raw VNS impedance → UHDR; contact resistance trend → SHDR
- Cervical VNS per-electrode impedance reported by the safety MCU to the hub for cross-validation (OI-CVNS-HUB-11) → UHDR (raw tissue impedance), transferred device-internally only, NEVER written to SHDR; the hub-vs-MCU divergence FLAG (`NP_CVNS_SHDR_EV_IMP_CROSSVAL`, no kΩ values, suppressed timestamp) → SHDR
- IR eye state during sessions → UHDR; safety interlock log → SHDR
- Device session count (unsigned integer) → SHDR; session timestamps → UHDR
- Research anonymization pipeline `failed_step` (which stage aborted, esp. `NP_ANON_STEP_VALIDATE`) → UHDR/app-side only (drives user retry prompt). A per-device count of validate failures weakly signals the wearer is a re-identification outlier (small anonymity set) — health-adjacent under WA MHMD / GDPR Art. 9. If a device-health signal is needed in SHDR, log only a coarse `anonymization_failed: bool` without the stage. See `firmware/anon/include/np_anon_pipeline.h` (`np_anon_step_t`).
- Fault-latch `count` (distinct-fault-transition tally) → SHDR; fault-latch `tick_ms` (event time) → SHDR in general, but SUPPRESSED to 0 when the latched fault carries `NP_SAFETY_STATUS_CARDIAC` (CVNS cardiac cutoff co-locates the event time with a UHDR-class health event). Enforced via `np_fault_latch_report_tick_ms()`/`_report_count()`.

### 5.2 Predictive maintenance system (SHDR-based)

Three phases:
- **Phase 1** (0–1,000 devices, Year 1): Population-average survival analysis on time-to-failure data
- **Phase 2** (1,000–10,000 devices, Year 2): Fleet-trained LSTM on HDR sensor trajectories
- **Phase 3** (10,000+ devices, Year 3+): Bayesian personalization — continuously revised RUL predictions

All models version-stamped by hardware revision. New revision falls back to Phase 1 until fleet data accumulates. Models deployed back to devices via OTA — competitive moat grows automatically with fleet size.

**Reminder engine rules:**
- Safety-critical: cannot be dismissed — blocks session start
- Performance-critical: snooze max 3×
- Comfort/longevity: snooze max 5×
- All reminders measurement-triggered, not calendar-triggered
- Every reminder includes measured data that triggered it + one-tap order link

### 5.3 Research data anonymization architecture (locked)

All anonymization of UHDR data for research purposes must occur **on-device**, within the NeurOne app, before any data leaves the device. NeurOne cannot access raw UHDR at any point — including for research purposes — because the biometric-derived AES-256 key is never held by NeurOne infrastructure.

**Data flow per approved study:**
1. NeurOne server sends device a signed study descriptor (study ID, approved UHDR element list, anonymization parameters: k≥10, suppression rules, date-rounding ≥1-week interval). Descriptor is cryptographically signed.
2. App reads encrypted UHDR partition in-app, applies on-device anonymization transformations: k-anonymity grouping, date/time rounding, direct identifier removal, quasi-identifier suppression per study descriptor.
3. Only the pre-anonymized, signed extract is transmitted to NeurOne research infrastructure. Raw UHDR never leaves the device.
4. NeurOne servers store extract keyed to study ID and device ID only. No persistent per-user anonymized data store. No linkage table exists that could re-identify users.
5. Researchers access aggregated study datasets with no device ID fields.

**Consent withdrawal effect:** Because each study extract is generated on-device on-demand, withdrawing consent permanently blocks the device from processing future study descriptors. No further extracts are generated or transmitted — **for any data period, including sessions predating withdrawal**. Already-published extracts cannot be individually removed from datasets (irreversibility notice given at consent time); no new data flows ever.

**Audit trail (SHDR):** Study ID, study descriptor hash, extract transmission timestamp, and extract byte count are logged in SHDR. User can inspect all studies their device has contributed to via the app. This log is never shared with researchers.

---

## 6. CLINICAL CONSENT ENGINE (all locked)

### 6.0 Two consent subjects (locked)

NeurOne has **two distinct consent subjects** that must never be conflated:

| Subject | Who | Data | Consent granted at | Managed by |
|---------|-----|------|--------------------|------------|
| **Warranty owner** | Entity that purchased/registered the device — may be a clinic, institution, or individual; is **NOT assumed to be the device user** | SHDR fleet telemetry only | Device warranty registration | `SHDRUploader` (device-linked opaque token, no user identity) |
| **User** | Person wearing the device during sessions | UHDR (EEG, HRV, PBM dose, adaptation events — user biology) | Per-user research consent flow (L1–L4 below) | `ConsentStore` (per user, on-device) |

**Invariants:**
- A clinic that registers a device warranty has NOT consented on behalf of any patient.
- A patient using a clinic-owned device has their own independent `ConsentStore` state.
- SHDR and UHDR consent gates are code-structurally independent — `SHDRUploader` has no reference to `ConsentStore`.
- Revoking user research consent has no effect on SHDR uploads; revoking warranty consent has no effect on user research participation.

**Research consent withdrawal scoping:**
- Withdraw from specific study → stops data flows for that study only; app analytics unaffected.
- Withdraw from specific category → stops data flows for that category; app analytics unaffected.
- Withdraw blanket research consent (L3) → stops ALL research data flows AND tears down research analytics (`ConsentStore.withdrawBlanketResearchConsent()` calls `revokeResearchAnalytics()`), because blanket withdrawal signals the user does not want any data collection beyond basic device function.

### 6.1 Use case subscription tiers

| Tier | Price | Use cases | UHDR elements | Target clinician |
|------|-------|-----------|---------------|-----------------|
| Monitor | $49/mo/patient | Adherence monitoring, protocol compliance | Session timestamps, duration, protocol parameters | Primary care, wellness, coordinators |
| Assess | $149/mo/patient | All Monitor + EEG review, neurofeedback, efficacy | Adds EEG waveforms, neurofeedback scores, dose logs | Neurologists, psychiatrists |
| Full Clinical | $299/mo/patient | All Assess + HRV, closed-loop events, outcomes | Adds HRV, PPG, adaptation events, outcome logs | TMS clinics, neuromodulation programmes |
| Research | $599/mo/study | IRB-defined custom (NeurOne review required) | IRB-approved minimum, k≥10 anonymization, no IDs | Academic trials, observational studies |

**Key principle:** Clinicians select **use cases** (not data elements). System determines minimum necessary UHDR elements. Users receive plain-language decision support document listing what clinician CAN learn, CANNOT learn, and privacy implications per element.

**Expansion workflow:** Differential consent document → persistent user notification → user approves/denies/asks questions → retroactive access is a separate decision. Retroactive and prospective access presented as separate consent decisions even if made simultaneously.

### 6.2 A priori research consent (4 onboarding screens)

| Layer | Question | If yes | If no | Brand ambassador mechanism |
|-------|----------|--------|-------|--------------------------|
| L1 — Contact consent | Can we reach you about future research opportunities? | Provide contact method + frequency limit. POA holders upload POA (human review, 3 business days, jurisdiction-flagged, annual re-verification) | No contact. All features unchanged. | Being asked creates perceived agency → trust baseline |
| L2 — Category consent | Which research areas? (9 categories: AD/dementia, Depression, PTSD, TBI, Sleep, Attention, Parkinson's, Healthy ageing, Visual health) | Per-project contact for selected categories only. Each project is a fresh decision. | Not contacted for that category. | Personal category choice deepens engagement |
| L3 — Blanket consent | Pre-approve all NeurOne-reviewed research? | Data included in all studies. **Still receives per-study engagement notifications** (not consent requests — maintains engagement, can opt out per-study). anonymization: k≥10, no IDs, no sub-weekly timestamps. **Irreversibility notice displayed at this screen:** "Once your anonymized data has been included in a published study, it cannot be individually withdrawn from that dataset. However, because NeurOne anonymises your data fresh from your device for each study, withdrawing consent immediately and permanently stops any further data flowing to any future dataset — including data from sessions that occurred before your withdrawal." | Per-category and per-project process applies. | Blanket patients kept engaged — not taken for granted |
| L4 — Results + community | Hear study results? Join suggestion portal? | Plain-language results notification per study (including null results) + paper link + "suggest next steps" link. Access to suggestion/voting/pledge portal. | No results contact, no portal. | Results notification is the highest-value brand moment |

**POA workflow:** POA holder uploads executed healthcare POA → human review 3 business days → jurisdiction flagging → scope limitation noted → annual re-verification. If patient regains capacity, all proxy consent decisions presented for ratification or revocation. Research contact goes to POA holder only.

**Vulnerable population disclosure:** At per-project consent time, explicitly state: "Once your anonymized data is included in a study, individual withdrawal is not possible from that dataset — this is a fundamental property of k-anonymized aggregate data and is required by Common Rule (45 CFR 46). However, because NeurOne anonymises your data fresh from your device for each new study, withdrawing consent immediately and permanently prevents any further data from flowing to any future dataset — including data from sessions that occurred before your withdrawal. Your historical sessions remain on your device under your sole control."

### 6.3 Research suggestion portal (three functions)

1. **Patient research agenda:** Patients submit study ideas in plain language, community votes ("interested"), comments, expresses participation intent. Top suggestions visible to researcher community.

2. **Pre-identified subject pool:** "Would participate" intent flag creates pre-screened, device-familiar, motivated cohort. Researcher portal shows willing participant count, geographic distribution, anonymized device usage profiles per suggestion. Solves researchers' hardest problem (recruitment = 40–60% of trial cost) before grant is written.

3. **Crowdfunding catalyst:** Pledges ($10–$100+) are intent, not charges. When researcher confirms pilot feasibility, formal campaign activates. Escrow held until target met; refunded if not. Released to institution research account. NeurOne contribution matching for strategic studies. Pilot data (even n=20–30) supports NIH SBIR/R21 application. Funders receive results notification + paper acknowledgement as "NeurOne Patient Research Fund contributors."

**Per-project contact workflow:**
1. NeurOne reviews study (use case library, minimum necessary data, IRB verification)
2. Eligible patient list generated by device ID + contact prefs only (no UHDR)
3. personalized invitation from NeurOne (not researcher) — personal tone, specific about study, explicit about what researchers CAN and CANNOT see
4. Patient decision: Yes / No / Ask a question (secure message to NeurOne liaison, 2 business day response). Invitation includes irreversibility notice: data already included in published studies cannot be individually removed; consent withdrawal blocks all future data flows from any time period.
5. Results notification closes loop for all who opted in (including null results). Users who later withdrew consent still receive results for studies they previously participated in — notification only, no new data.
6. Consent withdrawal effect: device immediately stops processing study descriptors; no further extracts generated or transmitted, for any data period including historical sessions.

---

## 16. NAMING CONVENTION CHANGES

**Retired term:** "Health Data Record (HDR)" — ambiguous, replaced throughout all documents

**Replacement:**
- `UHDR` = User Health Data Record (user's property, never accessed by NeurOne)
- `SHDR` = System Health Data Record (NeurOne property, device-linked only, never user-linked)

Both terms appear in full on first use in each document, abbreviated thereafter.

---

*This CLAUDE.md is the authoritative core of the NeurOne design program. The always-relevant invariants live here; archival/reference material lives in the subsidiary files listed in the Document Map (§ near top). All decisions marked "locked" have been through full design review; "pending" decisions require resolution before first tooling cut (see `docs/status/pending-decisions.md`). When a locked decision changes, update the relevant file and log it in `docs/status/completed-decisions.md`.*
