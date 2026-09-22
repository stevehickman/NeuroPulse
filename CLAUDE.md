# CLAUDE.md — NeurOne Design program
**Project:** NeurOne — closed-loop multi-modal neuromodulation wearable platform  
**Revision:** 50 (current)  
**Status:** Pre-tooling design phase. No hardware committed yet. All decisions below are locked unless explicitly noted as pending.

> **This file is the always-loaded core: invariants only.** Every section keeps the decisions that
> bear on most conversations and names the file holding the rest. Read a subsidiary file when the
> task needs it — do not assume a figure or a spec detail is here.
>
> **Revision history — every revision from Rev 33 to the current Rev 48, what it changed and why —
> is `docs/reference/claude-md-revision-history.md`. None of it is summarised here.** That file is
> the only narrative of *why* an invariant below reads the way it does, which entries changed a
> locked decision, and which changed none: **read it before assuming why something is the way it
> is**, and before reopening anything a revision entry settled. Revisions before Rev 33 are in git
> history and `docs/status/completed-decisions.md`.
>
> **Three live constraints that decide whether an answer is safe to give:**
> 1. **Every T1 configuration is gross-margin negative and every cost figure is a floor** (§2.1).
>    Retail is unlocked; no new price is set, and none may be set before `OI-HEXTILE-06`
>    (`OI-COST-10`).
> 2. **UHDR is never accessible to NeurOne** (§5). When in doubt about a new field → UHDR.
> 3. **The safety MCU owns every stimulation enable line** (§4.2). No app-side path may bypass it.

---

## 📂 DOCUMENT MAP — where everything lives

**In this file (invariants):** §1 product · §2 configurations · §3 modality roster · §4 hardware
(§4.1 processor stack and §4.2 safety architecture in full) · §5 UHDR/SHDR architecture · §6 consent
· §16 naming · §17 locale rule. Everything below is a plain path (not an `@import`), so it loads
only when I `Read` it.

**Detail relocated out of the core sections — read these when working in that section:**

| Section | What moved | File |
|---------|-----------|------|
| Header | **CLAUDE.md revision history — every revision, Rev 33 onward** | `docs/reference/claude-md-revision-history.md` |
| **§2 in full** · §6.1 | **The configuration table (retail + modalities), BOM / COGS / GM%, box contents, the implied retail ladder, charger tables + intent signals, and every consumable row** — §2 keeps the rules and not one figure · clinician subscription tiers | `docs/reference/commercial-model.md` |
| §3 | Full T1 + T2 modality specifications | `docs/reference/modality-stack.md` |
| **§4.3 · §4.4 · §4.5 · §4.7** | **Per-layer shielding stack · fit-system specs · power/PD/runtime table · status-LED behaviour** | `docs/reference/hardware-detail.md` |
| §5.1 · §5.2 · §5.3 | **Full UHDR/SHDR contents enumerations** · per-field boundary resolutions · predictive maintenance · anonymization pipeline | `docs/reference/data-architecture-detail.md` |
| §6.2 · §6.3 | Layer table, screen rationale, POA workflow · research portal | `docs/reference/consent-engine.md` |
| **§17** | **Generator mechanics, placeholder + plural rules, key conventions, why the generated files are not committed** | `docs/reference/localization.md` |
| **§18** | **The two-question test in full, the worked example, retire-vs-downgrade, and the `OI-CONV-08` audit** | `docs/np_conv_001.md` §7.1 |

**Subject-matter documents:**

| Topic | Read when… | File |
|-------|-----------|------|
| **Configuration cost model** (BOM/COGS/GM% derivation, the three unsourced assumptions, term U, why OI-HUB-C08 cannot close) | quoting or acting on ANY §2.1 cost figure; any BOM, margin or pricing question | `docs/np_cost_001.md` |
| **PBM protocol power audit** (which predefined protocols fit the envelope; why the "~6 tiles" rule is really 2–32; the zone-granularity defect; what cascading can and cannot rescue) | asking whether a protocol can actually run, authoring or editing any `.npps`, or touching zone definitions | `docs/np_ses_pwr_001.md` |
| **Naming + notation conventions** (signal names, `§N`, document IDs, identifier families) | authoring or revising ANY doc, naming a signal, or reviewing an interface | `docs/np_conv_001.md` |
| **EEG electrode net** (why pod travel cannot fix 10-20 registration; net sizing model; modality interference; wiring) | ANY question about EEG electrode placement, fit across head sizes, the T1-B tile type, or `REG-1`'s scope | `docs/np_hw_eegnet_001.md` |
| PBM optical resolution floor (what boundary the hardware can actually produce) | zone sizing, lateralized protocols, any "targets region X" claim | `docs/np_opt_psf_001.md` |
| Optional accessories + companion SW (mastoid pad, Watch app) | working on accessories / app roadmap | `docs/reference/accessories-roadmap.md` |
| Durability + maintenance design changes | tooling / BOM / mechanical work | `docs/reference/durability-maintenance.md` |
| Service network (partner tiers, covers) | service / warranty / logistics work | `docs/reference/service-network.md` |
| **EMF shielding evidence base + claim substantiation** (what the literature does and does not support per layer and per modality; the per-layer value audit; why Layer 2 cannot be removed and Layer 4 is the live candidate) | quoting, publishing or acting on ANY shielding claim; any question about what the shielding buys; any proposal to add to or remove from the §4.3 stack | `docs/np_bib_emf_001.md` |
| **Enclosure cavity resonance** (the source that excites it, the band, Layer 4's requirement in dB — and why the layer supplies 1 % of it while the wearer's head supplies the rest) | any question about Layer 4, the absorber station, cavity Q, or what the §4.3 stack does above 400 MHz; before specifying or deleting that station | `docs/np_emc_cav_001.md` |
| Competitive position + claims | marketing / positioning / claims work | `docs/reference/competitive-position.md` |
| Regulatory strategy (T1 wellness / T2 510k) | regulatory / QMS / standards work | `docs/reference/regulatory-strategy.md` |
| Clinical researchers + evidence bibliography | clinical trials / evidence / researcher outreach | `docs/reference/clinical.md` |
| Marketing notes / draft copy | marketing copy work | `docs/reference/marketing-notes.md` |
| **Open items / pending decisions** (was §13.1–13.4) | checking what's blocking / unresolved | `docs/status/pending-decisions.md` |
| **Completed + locked decisions log** (was §13.5) | checking whether/how something was decided | `docs/status/completed-decisions.md` |
| **Document + firmware register** (was §14) | locating a spec doc or firmware module | `docs/status/document-register.md` |
| Formal DHF index (source of truth for design records) | 510(k) / design-control work | `docs/np_dhf_001.md` |
| **Manufactured artifact register + documentation readiness** (what we build; which tooling spec / risk register / FAI checklist exists, and what blocks the rest) | asking "does X have a spec / an FAI / a risk register yet?", or planning tooling work | `docs/np_art_001.md` |
| **Accessory + applicator hardware specifications** — audio cup · intranasal Y-probe + sleeve · auricular VNS/HRV clip · cervical VNS · TMS coil · 21-ch tACS driver. **All DRAFT and requirements-grade: they carry what the record binds and name what is missing, not dimensions** | working on any of those six artifacts; asking what a modality's *hardware* is actually specified to do — **§3's roster is not that** | `docs/np_hw_audio_001.md` · `docs/np_hw_nasal_001.md` · `docs/np_hw_vnsclip_001.md` · `docs/np_hw_cvns_001.md` · `docs/np_hw_tms_001.md` · `docs/np_hw_tacsdrv_001.md` |
| FAI programme (method, issue conditions, PDMS-bond + ingress qualifications) | writing or running any first article inspection | `docs/np_fai_001.md` |
| Risk file — the ISO 14971 index, and the disposition of every RISK-01…26 ID | any ISO 14971 / hazard question | `docs/np_risk_002.md` |
| Risk registers — hex-tile module · shell/socket/interconnect/hub | per-artifact hazard work | `docs/np_risk_003.md` · `docs/np_risk_004.md` |
| Hex-tile mould tooling specification | tile mould / BOM / mechanical work | `docs/np_tool_hextile_001.md` |
| Shell interconnect design review record (gates shell tooling first cut) | shell tooling release | `docs/np_rev_shell_001.md` |
| **Earlier document versions** — index naming the current document for each | you have a figure or a file and need to confirm it is the current one | `docs/superseded/README.md` |

> The three `docs/status/` files are large logs, not narratives — each opens with a "How to read
> this file" block giving the grep recipes to reach one entry without reading the whole file. They
> also overlap heavily with the DHF index (`docs/np_dhf_001.md`) and git history. **The DHF is the
> source of truth where they disagree**; the dedup pass against it is `OI-CONV-07`.

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

## 2. CONFIGURATIONS + PRICING (🔓 retail UNLOCKED 2026-08-16; charger policy §2.2 still locked) → `docs/reference/commercial-model.md`

**Every §2 figure and table is in `docs/reference/commercial-model.md` §2.1–§2.3** (Rev 47) — retail
in force, BOM/COGS/GM%, modalities per configuration, box contents, the implied ladder, the charger
table and its intent signals, every consumable's price/interval/margin. **§2 holds no number.** The
six configurations, by tier (§1: T1 is FDA-exempt wellness, T2 a 510(k) target) — **T1:** Core — EEG
only · Home Lite · Home Standard ★ (flagship) · Home Premium. **T2:** Pro Entry · Pro Full.

### 2.1 Integrated system configurations → `docs/reference/commercial-model.md` §2.1

**Every T1 configuration is gross-margin negative at the prices in force, and every cost figure in
the document set is a FLOOR** — each excludes the uncosted term **U**, because `OI-HEXTILE-02` has
selected no 660/808 nm emitter and **OI-HUB-C08 therefore cannot be closed**. Retail prices are the
prices in force, **not a decision**, and **`OI-HEXTILE-06` must be decided before any price is set**
(`OI-COST-10`). **No BOM, COGS, GM% or retail figure may be quoted, cited or acted on without first
reading `docs/np_cost_001.md`.**

### 2.1a Implied retail ladder (implied, NOT set) → `docs/reference/commercial-model.md` §2.1a

**Break-even binds before margin does:** every T1 break-even already exceeds its price in force, so
no margin target is reachable today and the binding number is break-even, not the target. Both Pro
rows are profitable, so Pro is where the *target*, not the cost, is the thing to question. The T1 and
T2 ladders **collide** (`OI-COST-08`), and every competitive price claim is live again (`OI-COST-09`).

### 2.2 Charger policy (locked) → `docs/reference/commercial-model.md` §2.2

**Keyed to peak draw (§4.5), not price — unaffected by the retail unlock**; auto-included at every
upgrade by serial-number tracking. **EU:** chargers are branded recommendations, never proprietary
requirements; **any PD-compliant charger must work, and the app informs ("power level: reduced"),
never blocks.**

### 2.3 Consumables + recurring revenue → `docs/reference/commercial-model.md` §2.3

Intranasal hygiene sleeves are the **only authenticated consumable** and the primary MRR driver.
**All consumable prompts are measurement-triggered (§5.2), never calendar-triggered** — a consumable
with no measurement gets no prompt; inventing a trigger is design work, not a documentation edit.

**What counts as a measurement (Rev 48, `OI-ACC-02`).** A calendar prompt is not only forbidden here,
it is **unimplementable on this device**: there is no battery, coin cell or `VBAT` rail (§4.5), so the
RT1062's SNVS RTC has no backup domain and wall time is lost on every disconnect. A replacement
prompt therefore qualifies in exactly one of two ways, and the row must say which:

1. **Condition measurement** — a sensed quantity of the part itself (hydrogel tips: impedance trend).
2. **Exposure count** — a count the device already takes, of the quantity that drives the part's
   degradation, **with that mechanism named** (VNS clip pads: electrochemical degradation from VNS
   current, 20–40 sessions). An exposure count is weaker than a condition measurement and the
   difference is not cosmetic: it cannot see a part that failed early, was damaged, or degraded off
   the device. A row taking this route says what its count cannot see.

**A threshold back-derived from a calendar interval is a calendar prompt wearing a session count**,
and is the thing this invariant forbids — the trigger *kind* and the threshold's *provenance* are
two claims, and satisfying the first does not satisfy the second. A threshold that no measurement
supports is an unvalidated placeholder and is labelled one, per `NP-FW-EMMC-002` §G.2.

**Scope: consumable replacement prompts only.** Service-network and calibration intervals are
deliberately outside it and stay calendar-denominated — the 3–5 year Tier B fluxgate visit
(`docs/reference/durability-maintenance.md`, scale-factor drift is not self-detectable) and the
$1,800/yr T2 calibration visit (`NP-PWRSRC-001` D-16, the ISO 14971 re-acknowledgement point) have
no measurement to substitute, and reading this rule onto them would delete two controls.

---

## 3. MODALITY STACK (all locked) → `docs/reference/modality-stack.md`

**T1 — 8 modalities:** ① PBM transcranial (660–670 + 808–830 nm, hex-tile lattice, dual-PD dose
metering, 1064 nm smart-module upgrade) · ② PBM intranasal (bilateral Y-probe, authenticated sleeve)
· ③ EEG neurofeedback (8-ch semi-dry, 500 Hz, ADS1299) · ④ BES / tACS *(consumer name: Brainwave
Entrainment Stimulation)* · ⑤ tDCS *(consumer name: Cortical Priming Stimulation)* · ⑥ VNS + HRV +
HRV biofeedback (auricular clip, PPG, 4 protocols) · ⑦ neural audio entrainment (planar magnetic +
bone conduction, EEG-adaptive) · ⑧ visual stimulation (108 micro-LEDs/lens, 6 zones/eye, Mode F NIR
retinal walk, snap-on shade system, EC lens option).

**T2 adds:** 21-ch qEEG wet gel · focal figure-8 TMS (0.1–0.5 T) · 1170 nm deep PBM (35–40 mm) ·
clinical tACS (≤4 mA, 21-ch) · sLORETA-guided HD-tDCS (4×1 ring) · cervical VNS accessory · HIPAA
cloud + FHIR R4 + LSL + scripting API · anonymized session tag.

**Hard limits that constrain any protocol or firmware work** (full context in the modality file, and
enforcement in §4.2):

| Modality | Ceiling |
|----------|---------|
| PBM scalp | **400 mW/cm² peak pulsed** (≤25% duty, firmware-enforced) · 200 mW/cm² CW · 42 °C limit (IEC 60601) |
| PBM deep (T2) | ≤1,000 mW/cm² (1170 nm, TEC-stabilised) |
| BES / tACS | 0.5–40 Hz · ≤1 mA T1 / ≤4 mA T2 · charge-balanced biphasic · **40 µC/cm² per phase** (see below) |
| tDCS | 0.1–2 mA DC · **150 mC/cm² per session** hardware limit · 30 s ramp · ≤3 electrode pairs |
| VNS (auricular) | 1–25 Hz · ≤2 mA · biphasic charge-balanced · **40 µC/cm² per phase** |
| Visual | IEC 62471 MPE at 50% of exempt-group threshold · photoparoxysmal halt <200 ms |

**The charge ceiling is TWO ceilings, one per waveform class** (Rev 46, `OI-CHARGE-05`). A single
40 µC/cm² figure used to be stated for the whole electrical tier; it is a **per-phase PULSED** limit
(Shannon/McCreery) and was being compared against a session-cumulative integral, which is a category
error in two directions at once. DC channels (tDCS, HD-tDCS) get a **per-session** ceiling of
**150 mC/cm²** per electrode; charge-balanced channels (BES/tACS, VNS, cervical VNS, clinical tACS)
get a **per-phase** ceiling of **40 µC/cm²** per electrode, because net charge on them is ~zero and a
session integral of |I| is not a dose. Both are enforced against the electrode area **declared in the
signed descriptor**, and both are **commanded-dose** limits — what the protocol asked for, never an
ADC measurement. Derivations, citations and the provisional status of the 150: `docs/np_dt_001.md`
§3.2.1 (DI-SAFE-01 / DI-SAFE-01a).

**Do not answer a modality question from this roster alone** — wavelengths, counts, materials,
consumables, evidence and per-modality open items are in `docs/reference/modality-stack.md`.
Whether a given protocol fits the power envelope is `docs/np_ses_pwr_001.md`.

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
| BES / tDCS / VNS / cVNS | Waveform-aware commanded-charge ceiling: **150 mC/cm² per session** (DC) · **40 µC/cm² per phase** (charge-balanced) | Safety MCU hardware — app cannot override. Fail-closed: an electrical channel whose waveform class was not declared is never granted |
| Visual / retinal | IEC 62471 MPE ceiling | IR proximity + Hall sensor + hardware current limit (3 independent layers) |
| PBM scalp | IEC 60601 42°C limit | NTC per zone → hardware current throttle at 62°C junction |
| TMS | Coil protection | EMF cancellation gated off 5ms before pulse, 50ms hold |
| VNS | Contact confirmation | Safety MCU reads impedance; holds if contacts not confirmed |
| Cervical VNS (T2) | Cardiac rhythm interlock | Safety MCU owns enable GPIO; monitors R-peak GPIO; HR change >15 BPM within 5s → GPIO cutoff <100ms; 30s re-enable lockout + app confirm + repeat impedance |
| All | Firmware anti-fragility | CSPRNG session protocol signing |

### 4.3 EMF shielding (5-layer passive + active) → `docs/reference/hardware-detail.md`

Five passive layers plus active fluxgate + Helmholtz cancellation. **Combined 35–45dB ELF magnetic /
40–60dB RF**, the figure every §1 claim rests on. Three things that bind other work: Layer 3 is
**palladium, not silver** (tarnish-immune — what makes the claim *permanent*, verified by fleet SHDR
attenuation); the shell is bonded to the EEG DRL output; the TMS coil site needs a **non-conductive
CFRP window**. Per-layer dB and the three firmware additions: §4.3 of the detail file.

> **The dB figures are design targets, not measurements — `EMF-1` has never run** — and what the
> stack is *for* is `docs/np_bib_emf_001.md` (NP-BIB-EMF-001), the evidence record. Read it before
> quoting a shielding claim, or proposing to add to or remove from this stack. Three things it
> establishes that this section does not say: the literature supports the **electric/RF** layers for
> EEG signal quality and supports **no** therapeutic-outcome benefit on any modality; **Layer 2's
> documented rationale is the one benefit the evidence does not support**, while three undocumented
> dependencies are what actually hold it in place; and the stack is **aperture-limited, not
> layer-limited**, so an unmeasured seam budget makes every layer's value unknowable. **No dB figure
> here or in the detail file may be published as *measured*.**
>
> **Layer 4 now has a requirement, and fails it — `docs/np_emc_cav_001.md` (NP-EMC-CAV-001, 2026-09-20).**
> It was the one layer with no dB figure anywhere; `REQ-CAV-02` now sets **loaded Q ≤ 20 over
> 420 MHz – 3 GHz (≥ 26.2 dB)** against a named source (the **18 cluster controllers inside the
> envelope**, not a radio) — and **the layer supplies 0.26 dB of it while the wearer's head supplies
> 49.8 dB**, because a 3 mm non-magnetic absorber on a conductor is reactive-only regardless of its
> loading. **`REQ-CAV-04`: delete the station — and the 3 mm re-loft of the outer bowl is BINDING**,
> because vacating 3 mm fills it with stagnant air at 54 % *worse* per mm than the foam (0.115 vs
> 0.075 m²K/W): without the re-loft the outward path goes 0.410 → **0.450**, with it **0.335**. **Now
> is the cheapest this decision will ever be** — no mould is cut (`NP-REV-SHELL-001` is DRAFT, and
> `OI-ART-01` already owes a re-scope), and nothing is externally published, so *"5-layer"* above is
> an internal string. **Recommended, not executed** — §1 and §4.3 still read 5-layer until the
> principal takes it, and `OI-EMCCAV-08` (`MECH-2`) is the one open question: if the cluster clamps
> cannot take up the tolerance stack without the foam, the station returns as a thin ceramic pad.

### 4.4 Fit system → `docs/reference/hardware-detail.md`

**1 adult SKU covers 52–62cm heads** (Boa occipital dial, 5-position bridge, spring-decoupled pods).
Ranges, ratings, materials: §4.4 of the detail file. Whether one SKU can register a 10-20 montage
across that range is `docs/np_hw_eegnet_001.md`, not this section.

### 4.5 Power → `docs/reference/hardware-detail.md`

USB-C PD only. **T1 peak ~45–50W (65W PD) · T2 peak ~70–74W (100W EPR)** — the two figures §2.2's
charger policy is keyed to. 22F hub supercapacitor absorbs LED duty-cycle transients; NTC aging
estimate logged in SHDR. Full mode/draw/PD/runtime table: §4.5 of the detail file.

### 4.6 Operating modes
- **Mode 1 Connected:** Real-time streaming <1ms
- **Mode 2 Programming:** App uploads session protocol <5s
- **Mode 3 Autonomous:** Pre-programmed, runs from any USB-C PD power bank, full closed-loop EEG-adaptive operation without phone or app
- **Mode 4 Download:** USB-C reconnect → EDF+ + parameter logs to app

### 4.7 Status indicators → `docs/reference/hardware-detail.md`

Green power LED · amber in-use LED (pulse mirrors session frequency) · red blink on fault.
**Stealth mode suppresses the indicators; safety faults always fire.** Detail: §4.7 of the detail
file.

---

## 5. DATA ARCHITECTURE — UHDR / SHDR (all locked)

### 5.1 Definitions

**UHDR — User Health Data Record**
- Owner: user unconditionally
- NeurOne access: **NEVER** — not for support, engineering, research, or regulatory submission
- Clinician access: per-element, per-use-case, time-limited, audited, revocable
- Researcher access: anonymized aggregate only, separate IRB + explicit research consent
- Defining test: does this record tell us something about the **person**? If yes → UHDR
- Contents (representative — full enumeration in the detail file): EEG waveforms (all channels) · HRV time series · session timestamps and duration · closed-loop adaptation events · PBM dose (J/cm²) per zone · user-entered symptom/outcome logs
- Storage: on-device eMMC UHDR partition, AES-256 encrypted with user biometric-derived key (NeurOne does not hold decryption key)
- Backup: automated nightly incremental backup to USB-C local or E2E encrypted cloud (user-held key) when on USB-C power

**SHDR — System Health Data Record**
- Owner: NeurOne
- Linked to: device ID + opaque TRNG warranty token **only** — never to user identity
- **Consent subject: warranty owner** (the entity who registered warranty — may be a clinic, institution, or individual purchaser; is NOT assumed to be the person wearing the device). Warranty consent is entirely separate from user research consent. A clinic staff member activating warranty is not consenting on behalf of any patient.
- Defining test: does this tell us about the **device's condition**, with nothing that reveals user biology? If yes → SHDR
- Contents (representative — full enumeration in the detail file): LED output ratio per zone · NTC temperature profiles · EMF shielding attenuation ratio · device session count (unsigned integer, no timestamps) · USB-C insertion counter · firmware version + OTA history · calibration coefficient history
- Storage: on-device eMMC SHDR partition, separate encryption from UHDR
- Upload: to NeurOne fleet database on USB-C connect (warranty owner consent required at device registration; unrelated to user research consent)

**Two general rules that decide most new fields:**

1. **When in doubt → UHDR.** Reclassification requires positive demonstration of no user biology
   content.
2. **A redaction applied conditionally on a sensitive predicate leaks that predicate** (2026-08-12).
   It must be unconditional, or the predicate must not be inferable from the *pattern* of redaction
   — the "no such user" vs "wrong password" failure shape. This is why fault-latch `tick_ms` is not
   SHDR-reportable at all and why `status`/`slot`/`count` go through the single fixed-shape
   marshaller `np_fault_latch_build_report()`: zeroing `tick_ms` only for
   `NP_SAFETY_STATUS_CARDIAC` made `count > 0 && tick_ms == 0` a self-interpreting one-bit cardiac
   oracle. `scripts/check-redaction-shape.ts` enforces this shape.

**Both the full contents enumerations and the per-field boundary resolutions (EEG impedance,
accelerometer, VNS impedance, cervical-VNS cross-validation, anonymization `failed_step`, fault
latch, …) are in `docs/reference/data-architecture-detail.md` §5.1** — that file, not this section,
is authoritative per field, and a field on neither list is decided by the defining tests above and
then added there.

### 5.2 Predictive maintenance system (SHDR-based) → `docs/reference/data-architecture-detail.md`

Three phases: population-average survival analysis (0–1,000 devices) → fleet-trained LSTM
(1,000–10,000) → Bayesian personalization (10,000+); all models version-stamped by hardware revision,
deployed back by OTA. **Reminder engine:** safety-critical reminders cannot be dismissed and block
session start; performance-critical snooze ×3; comfort/longevity snooze ×5; every reminder is
measurement-triggered and carries the data that triggered it. The §H characterisation cohort, its
non-coercion invariant (CHAR-4), the selection-bias limit and the known gap in the Phase 2 premise
are in `docs/reference/data-architecture-detail.md` §5.2.

### 5.3 Research data anonymization architecture (locked) → `docs/reference/data-architecture-detail.md`

**All anonymization happens on-device, in the app, before anything leaves the device** — NeurOne
cannot access raw UHDR even for research, because the biometric-derived AES-256 key is never held by
NeurOne infrastructure. Studies arrive as cryptographically signed descriptors (k≥10, date rounding
≥1 week, suppression rules); only the anonymized extract is transmitted; no linkage table exists.
**Withdrawal permanently blocks future extracts for every data period, including sessions predating
withdrawal**; already-published extracts cannot be individually removed (irreversibility notice given
at consent time). Study ID, descriptor hash, transmission timestamp and byte count are logged in SHDR
and never shared with researchers. Full data flow: `docs/reference/data-architecture-detail.md` §5.3.

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
- **"Never asked" and "said stop" are different states and must stay distinguishable** (Rev 45).
  `blanketConsentGranted == false` is true of both, so the flag alone cannot carry the rule above:
  withdrawal does not un-tick the nine L2 categories, and those stale boxes would otherwise keep
  admitting studies after the user stopped everything. `ResearchConsentState.blanketConsentWithdrawnAt`
  records the true→false transition — set by the store, never by a screen — and
  `blanketConsentWithdrawn` outranks L2 at the §6.3 ingestion gate. Re-granting L3 clears it,
  because that is a fresh decision; the marker itself stays in the record.

### 6.1 Use case subscription tiers → `docs/reference/commercial-model.md`

Four clinician tiers — Monitor $49 · Assess $149 · Full Clinical $299 /mo/patient · Research
$599/mo/study. **Key principle: clinicians select *use cases*, never data elements**; the system
derives the minimum necessary UHDR elements, and users get a plain-language document stating what the
clinician CAN and CANNOT learn per element. Expansion of access is a differential consent decision,
and **retroactive and prospective access are always presented as separate decisions** even when made
at the same time. **A grant's reach is therefore scoped in time and never derived from its tier
alone** — `tier.uhdrElements` is timeless, so a tier by itself can only answer the retroactive
question yes, and "widen it going forward, leave my earlier sessions alone" becomes unrepresentable
(Rev 42). Tier table, element lists and the expansion workflow as implemented:
`docs/reference/commercial-model.md` §6.1.

### 6.2 A priori research consent (4 layers, 2 onboarding screens)

**Layers are not screens.** L1–L4 are the four consent layers — the units of the data model,
of the withdrawal surfaces, and of every citation elsewhere in the document set. They are
presented across **two** screens. A citation to "L3" means the blanket-consent layer, wherever
it is rendered; it has never meant "the third screen."

| Screen | Layers | Question the screen asks |
|--------|--------|--------------------------|
| **S1 — What you get back** | L4 + L1 | What do you want to hear about, and how do we reach you? |
| **S2 — What you share** | L2 + L3 | Which research areas, and do you want to be asked about each study? |

| Layer | In one line | If no |
|-------|-------------|-------|
| **L1 — Contact** *(S1)* | May we reach you about future research? (contact method + frequency limit; POA holders upload a POA, human review 3 business days) | No contact; all features unchanged |
| **L2 — Category** *(S2)* | Which of the 9 research areas? Each project is still a fresh decision | Not contacted for that category |
| **L3 — Blanket** *(S2)* | Pre-approve all NeurOne-reviewed research (k≥10, no IDs, no sub-weekly timestamps); still receives per-study *engagement* notifications, not consent requests | Per-category and per-project process applies |
| **L4 — Results + community** *(S1)* | Plain-language results per study **including null results**, paper link, suggestion/voting/pledge portal | No results contact, no portal |

**Binding invariants (rationale, the full layer table, POA workflow and portal are in
`docs/reference/consent-engine.md`):**

- **L2 is scope; L3 is posture.** Selecting all nine L2 categories is *not* blanket consent —
  "everything, but ask me" is a real position and survives only while both axes do (§6.2.2).
- **Select-all does NOT auto-enable the blanket toggle** (§6.2.3). The usability objection is
  answered with copy, not state.
- **L4-first must not become an inducement** (§6.2.4): conditional framing ("if your data ever
  contributes to a study"), a symmetric exchange that names **null results** explicitly, and
  non-coercion stated on the screen — reciprocity buys information, never participation.
- **Fewer steps to grant must not mean coarser withdrawal** (§6.2.5). Withdrawal stays at study,
  category and blanket granularity, and the blanket→analytics teardown is enforced at the store
  ingestion point (`updateResearchConsent`) on a **true→false transition** — guarding the transition,
  not the value, so a category-only edit cannot trigger it. `scripts/check-consent-reachability.ts`
  guards the Rev 37 defect this was written against (correct, tested, unreachable from the iOS UI).
- **L3 carries an irreversibility notice whenever its control is on**, and per-project consent
  repeats it for vulnerable populations (45 CFR 46) — full copy in `docs/reference/consent-engine.md`.
- **L3's per-study notification is not a consent request, and the two cannot share one shape.**
  Silence means the opposite thing in each: an unanswered consent request means *not participating*;
  an unread engagement notification means *participating*, because L3 already answered. A single
  representation has to pick one default for everyone (Rev 44) — so a study reaches an L2 and an L3
  user as two different objects, and an L3 user's opt-out is a withdrawal, not a decline (§6.3).

### 6.3 Research suggestion portal (three functions) → `docs/reference/consent-engine.md`

Patient research agenda (plain-language study ideas, community voting) · pre-identified subject pool
("would participate" intent flags — recruitment is 40–60% of trial cost) · crowdfunding catalyst
(pledges are intent, not charges; escrow released only on confirmed feasibility). The per-project
contact workflow — NeurOne reviews the study, generates the eligible list from device ID and contact
prefs only (**no UHDR**), invites in NeurOne's own voice, and closes the loop with results including
null results — is in `docs/reference/consent-engine.md` §6.3.

**The device does not take that server-side review on trust.** What crosses onto the device is
§5.3's **signed study descriptor**; the invitation the user reads is derived from it on-device after
the signature verifies, and `ConsentEngine.admit()` re-checks §5.3's k≥10 / ≥1-week floors, L1, and
the user's own L2/L3 state before anything is shown (Rev 44). Two consequences a later change must
not undo: **what a study CANNOT see is computed as the complement of what it asked for**, never
supplied as prose by the party asking; and **verification is closed by default** — the only
`StudyDescriptorVerifier` shipped refuses everything, so a transport cannot be wired up without
supplying the signature check (`OI-CONSENT-07`). Gate table:
`docs/reference/consent-engine.md` §6.3.

---

## 16. NAMING CONVENTION

**Never name a health data record "HDR" or "Health Data Record"** — the term does not say whose
record it is, which is the one thing §5 turns on. There are exactly two, and every reference names
one of them: `UHDR` = User Health Data Record (user's property, never accessed by NeurOne) · `SHDR` =
System Health Data Record (NeurOne property, device-linked only, never user-linked). Both appear in
full on first use in each document, abbreviated thereafter. (`HDR` meaning a binary *header* —
`blob(n) = HDR(8) + …` in the LittleFS and NVRAM documents — is a different word and is fine.)
Signal names, document IDs, `§N` citation form and the other identifier families are
`docs/np_conv_001.md` (NP-CONV-001).

## 17. LOCALIZED STRINGS — CODE GENERATION RULE (locked 2026-09-03; single-source 2026-09-08) → `docs/reference/localization.md`

**Whenever non-firmware code is generated or edited, user-facing text goes into the locale files
and the code carries only a key.** Never write a string a person will read into a source file.

| Surface | Where the text lives | How text is read |
|---------|--------------------|------------------|
| Canonical | `locales/<bcp47>.json` — flat `KEY` → string, sorted, all 11 locales carry the same key set. **The only committed copy.** | — |
| Web | *build output* — `app/web/src/generated/locales/*.json` | `t('KEY')`, `tPlural('BASE', n)` from `app/web/src/lib/i18n.ts` |
| Apple | *build output* — `app/ios/NeurOne/Localizable.xcstrings` | `Text("KEY")`, `String(localized: "KEY")`; with values, `String(format: String(localized: "KEY"), …)` |
| Android | *build output* — `<buildDir>/generated/res/locales/values*/strings.xml` | `stringResource(R.string.key)` (lowercased key), `pluralStringResource(R.plurals.base, n, n)` |

- **`locales/*.json` is the single source of truth and the only committed copy of any user-facing
  string.** The three per-platform files are **git-ignored build outputs** regenerated by each app's
  own build from the one generator, `bun scripts/sync-locales.ts`. **Never edit one, and never
  commit one** — a generated file under version control is a second source of truth whether or not
  anyone means it to be, which is the failure this arrangement was written after (§17.5).
- **Add a key to `locales/*.json` — all eleven — then reference it.** Placeholder, plural and
  modality-key rules are `docs/reference/localization.md` §17.1–§17.3; **read it before adding a
  key**, because a malformed placeholder or plural family fails the build rather than degrading.
- **Module-level tables hold KEYS, not text.** A constant initialised at import time captures
  English before `initI18n()` resolves; resolve with `t()` at the point of render.
- **One name and one description per modality — `MODALITY_<ID>_NAME` / `MODALITY_<ID>_DESC`, and no
  others**, derived from the `.npps` grammar token; §3's two regulatory consumer names are carried
  verbatim by the `bes_tacs` and `tdcs` *descriptions*. The `.npps` parser and hub compiler keep the
  lowercase snake_case token, which stays the canonical identifier.
- **Firmware is exempt because it renders no text at all.** It carries no locale key and includes no
  locale file; the device speaks in tones (`np_zone_audio.c`), LEDs and numeric status, and the app
  does the wording. A locale reference under `firmware/` means that boundary moved — a decision, not
  a detail.

Two gates: `bun scripts/check-locale-strings.ts` (no user-facing string in source; its
`PENDING_PATHS` records the code the rule has not yet reached) and `bun scripts/sync-locales.ts
--verify-untracked` (no generated artifact tracked). Generator mechanics, the iOS two-hook
requirement, the `bun`-on-`PATH` consequence and the full rationale:
`docs/reference/localization.md`.

---

## 18. REQUIREMENTS — A REQUIREMENT MUST BE REQUIRED (locked 2026-09-21) → `docs/np_conv_001.md` §7.1

**Nothing is written as a requirement unless something requires it. Never add an unnecessary
constraint.** Before a number, limit, tolerance or "shall" enters a controlled document, two questions
must have answers and the row must carry them: **what fails if this is not met**, and **where is that
traceable** (an external standard, a measurement, a derivation from another specified value, or a hazard
control). **A Notes cell that restates the requirement is not a derivation.**

This is not tidiness. Most procurement and interface documents here open with *"all specifications are
MANDATORY unless marked ADVISORY"*, so an unrequired figure **rejects usable parts and manufactures
false failures** with the document's full authority — which is what `NP-PROC-FPC-001` §2.3's undrived
`Tj_max ≥ 125 °C` did before it was retired. **Disposition: retire, do not downgrade** (ADVISORY still
leaves something to screen against), and **retire is not delete** — the row stays marked in place so the
retirement is auditable.

**Scope limit — this is not licence to strip limits.** It governs constraints NeurOne invented. It does
**not** reach an externally imposed limit (IEC 60601-1's 42 °C, IEC 62471 MPE, the §3 charge ceilings), a
hazard control, or a requirement whose derivation exists but is merely uncited — **"I could not find the
derivation" is a reason to look, then to raise an open item, never to retire.** Removing a safety control
is an ISO 14971 decision and never follows from this section. Full rule, test, worked example and the
audit item `OI-CONV-08`: `docs/np_conv_001.md` §7.1.

---

*This CLAUDE.md is the always-loaded core of the NeurOne design program: the invariants, and a map to
everything else. Detail lives in the subsidiary files listed in the Document Map — a section here
that names a file is a pointer, not a summary you may quote figures from. When a locked decision
changes, update the owning file, log it in `docs/status/completed-decisions.md`, and add an entry to
`docs/reference/claude-md-revision-history.md`. Keep every top-level section (§1–§6, §16) and every
subsection number in place even when its content moves —* `bun scripts/check-section-refs.ts` *guards
663 inbound citations that resolve against them.*
