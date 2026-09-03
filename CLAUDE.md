# CLAUDE.md — NeurOne Design program
**Project:** NeurOne — closed-loop multi-modal neuromodulation wearable platform  
**Revision:** 40 (current)  
**Status:** Pre-tooling design phase. No hardware committed yet. All decisions below are locked unless explicitly noted as pending.

> **This file is the always-loaded core: invariants only.** Every section keeps the decisions that
> bear on most conversations and names the file holding the rest. Read a subsidiary file when the
> task needs it — do not assume a figure or a spec detail is here.
>
> **Revision history (Rev 33–40, what changed and why): `docs/reference/claude-md-revision-history.md`.**
> Rev 40 (2026-09-01) relocated detail out of this file; no design decision changed. Read the
> history file before assuming *why* something is the way it is.
>
> **Three live constraints that decide whether an answer is safe to give:**
> 1. **Every T1 configuration is gross-margin negative and every cost figure is a floor** (§2.1).
>    Retail is unlocked; no new price is set, and none may be set before `OI-HEXTILE-06`
>    (`OI-COST-10`).
> 2. **UHDR is never accessible to NeurOne** (§5). When in doubt about a new field → UHDR.
> 3. **The safety MCU owns every stimulation enable line** (§4.2). No app-side path may bypass it.

---

## 📂 DOCUMENT MAP — where everything lives

**In this file (invariants):** §1 product · §2 configurations + pricing · §3 modality roster ·
§4 hardware · §5 UHDR/SHDR architecture · §6 consent · §16 naming. Everything below is a plain path
(not an `@import`), so it loads only when I `Read` it.

**Detail relocated out of the core sections — read these when working in that section:**

| Section | What moved | File |
|---------|-----------|------|
| Header | CLAUDE.md revision history, Rev 33–40 | `docs/reference/claude-md-revision-history.md` |
| §2.1a · §2.2 · §2.3 · §6.1 | Implied retail ladder · charger tables + intent signals · consumables · clinician subscription tiers | `docs/reference/commercial-model.md` |
| §3 | Full T1 + T2 modality specifications | `docs/reference/modality-stack.md` |
| §5.1 · §5.2 · §5.3 | Per-field boundary resolutions · predictive maintenance · anonymization pipeline | `docs/reference/data-architecture-detail.md` |
| §6.2 · §6.3 | Layer table, screen rationale, POA workflow · research portal | `docs/reference/consent-engine.md` |

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
| Competitive position + claims | marketing / positioning / claims work | `docs/reference/competitive-position.md` |
| Regulatory strategy (T1 wellness / T2 510k) | regulatory / QMS / standards work | `docs/reference/regulatory-strategy.md` |
| Clinical researchers + evidence bibliography | clinical trials / evidence / researcher outreach | `docs/reference/clinical.md` |
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

> The three `docs/status/` files are large logs, not narratives — each opens with a "How to read
> this file" block giving the grep recipes to reach one entry without reading the whole file. They
> also overlap heavily with the DHF index (`docs/np_dhf_001.md`) and git history; a dedup pass
> against the DHF is a flagged follow-up.

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

## 2. CONFIGURATIONS + PRICING (🔓 retail UNLOCKED 2026-08-16; charger policy §2.2 still locked)

### 2.1 Integrated system configurations

> **⚠ BOM / COGS / GM% below are FLOORS, not estimates**, re-derived against the hex-tile
> architecture; the pre-hex figures (Core $168–169 / 42% … Pro Full $1,506 / 81%) are superseded.
> **Every T1 configuration is gross-margin negative at the prices in force.** Retail prices are the
> prices currently in force, **not a decision** — unlocking the constraint set no price, and
> **`OI-HEXTILE-06` must be decided before any price is set** (`OI-COST-10`).
>
> **Read `docs/np_cost_001.md` before quoting, citing or acting on any number here.** Three things
> it carries that the table cannot: GM% is an *output* derived under the lock, not a target; every
> row excludes the uncosted term **U** (the emitter-count delta — the 660/808 nm emitters are not
> selected per `OI-HEXTILE-02`, so **OI-HUB-C08 cannot be closed**); and the dominant recoverable
> term is the ~$10 InGaAs photodiode pair of the $11.53/tile driver + metering — that is
> `OI-HEXTILE-06`, and none of its three options, alone or combined, restores a positive T1 margin.

| Config | BOM (floor) | COGS (floor) | Retail (in force, 🔓 unlocked) | GM% (floor) | Modalities included |
|--------|-----|------|--------|-----|---------------------|
| Core — EEG only | $360–423 | $554–650 | $449 | **−23% to −45%** | 4-ch EEG · all connectivity · EMF shielding · processor stack · 8GB eMMC |
| Home Lite | $642–705 | $896–984 | $599 | **−50% to −64%** | Core + PBM tiles (660+810nm) · 8-ch EEG · VNS+HRV clip |
| Home Standard ★ (flagship) | $897–959 | $1,196–1,278 | $849 | **−41% to −51%** | All T1 modalities (see §3) |
| Home Premium | $952–1,014 | $1,287–1,371 | $1,199 | **−7% to −14%** | All T1 + EC lens (+$89 value) · 2yr warranty · priority support |
| Pro Entry | $1,463–1,525 | $2,398–2,500 | $4,999 | **+50% to +52%** | All T1 + 21-ch qEEG · 1170nm deep PBM · clinical tACS · HIPAA cloud · sLORETA |
| Pro Full | $2,136–2,198 | $3,728–3,836 | $13,999 | **+73%** | All T2 + TMS hub · multi-patient dashboard · scripting API · FHIR R4 · $1,800/yr service |

**★ Home Standard box contents:** All T1 modules · hard clamshell case · braided aramid USB-C cable (spare in box) · **45W NeurOne branded GaN charger** · S1 opaque shade · interface covers (installed + spare set each type) · mesh cleaning brush · Boa replacement cable + hook tool · moisture-barrier electrode tip hydration caps · humidity indicator card · pre-impregnated cleaning cloth packets

### 2.1a Implied retail ladder (implied, NOT set) → `docs/reference/commercial-model.md`

Retail = COGS ÷ (1 − original GM target), inheriting §2.1's floor status. **Break-even binds before
margin does:** Home Standard cannot be sold below **~$1,196** at any margin — already 1.4× its $849
price in force; at the original 36% target it is $1,869–1,997. Both Pro rows are profitable today
(+$2,499, +$10,163/unit), so Pro is where the *target*, not the cost, is the thing to question. Two
consequences the lock was concealing: the T1 and T2 ladders **collide** (`OI-COST-08`), and every
competitive price claim is live again (`OI-COST-09`). Full ladder, per-configuration figures and the
four things to weigh first: `docs/reference/commercial-model.md` §2.1a; derivation: `NP-COST-001` §8.

### 2.2 Charger policy (locked) → `docs/reference/commercial-model.md`

Charger scaled to **peak draw** of the configuration (15W Core → 65W ×2 Pro Full), auto-included at
every upgrade by serial-number tracking; a $19 at-cost 65W upgrade at checkout doubles as an intent
signal. **Keyed to peak draw, not price — unaffected by the retail unlock.** **EU:** chargers are
branded recommendations, never proprietary requirements; any PD-compliant charger must work and the
app informs ("power level: reduced"), never blocks. Per-config tables and the intent-signal
follow-ups: `docs/reference/commercial-model.md` §2.2.

### 2.3 Consumables + recurring revenue → `docs/reference/commercial-model.md`

Intranasal hygiene sleeves ($19/pack or $19/mo, 68–79% GM) are the **only authenticated consumable**
and the primary MRR driver; electrode hydrogel tips, VNS clip pads, audio foam/mesh, interface covers,
S3 Rx inserts and the $1,800/yr T2 service contract follow. All consumable prompts are
measurement-triggered (§5.2), never calendar-triggered. Full price/interval/GM table:
`docs/reference/commercial-model.md` §2.3.

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
| BES / tACS | 0.5–40 Hz · ≤1 mA T1 / ≤4 mA T2 · charge-balanced biphasic |
| tDCS | 0.1–2 mA DC · **40 µC/cm²** hardware limit · 30 s ramp · ≤3 electrode pairs |
| VNS (auricular) | 1–25 Hz · ≤2 mA · biphasic charge-balanced |
| Visual | IEC 62471 MPE at 50% of exempt-group threshold · photoparoxysmal halt <200 ms |

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

**Per-field boundary resolutions (EEG impedance, accelerometer, VNS impedance, cervical-VNS
cross-validation, anonymization `failed_step`, fault latch, …) are in
`docs/reference/data-architecture-detail.md` §5.1** — that list, not this section, is authoritative
per field, and a field not on it is decided by the defining tests above and then added there.

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

### 6.1 Use case subscription tiers → `docs/reference/commercial-model.md`

Four clinician tiers — Monitor $49 · Assess $149 · Full Clinical $299 /mo/patient · Research
$599/mo/study. **Key principle: clinicians select *use cases*, never data elements**; the system
derives the minimum necessary UHDR elements, and users get a plain-language document stating what the
clinician CAN and CANNOT learn per element. Expansion of access is a differential consent decision,
and **retroactive and prospective access are always presented as separate decisions** even when made
at the same time. Tier table and element lists: `docs/reference/commercial-model.md` §6.1.

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

### 6.3 Research suggestion portal (three functions) → `docs/reference/consent-engine.md`

Patient research agenda (plain-language study ideas, community voting) · pre-identified subject pool
("would participate" intent flags — recruitment is 40–60% of trial cost) · crowdfunding catalyst
(pledges are intent, not charges; escrow released only on confirmed feasibility). The per-project
contact workflow — NeurOne reviews the study, generates the eligible list from device ID and contact
prefs only (**no UHDR**), invites in NeurOne's own voice, and closes the loop with results including
null results — is in `docs/reference/consent-engine.md` §6.3.

---

## 16. NAMING CONVENTION CHANGES

**Retired term:** "Health Data Record (HDR)" — ambiguous, replaced throughout all documents.
**Replacement:** `UHDR` = User Health Data Record (user's property, never accessed by NeurOne) ·
`SHDR` = System Health Data Record (NeurOne property, device-linked only, never user-linked). Both
appear in full on first use in each document, abbreviated thereafter. Signal names, document IDs,
`§N` citation form and the other identifier families are `docs/np_conv_001.md` (NP-CONV-001).

## 17. LOCALIZED STRINGS — CODE GENERATION RULE (locked 2026-09-03)

**Whenever non-firmware code is generated or edited, user-facing text goes into the locale files
and the code carries only a key.** Never write a string a person will read into a source file.

| Surface | How text is written | How text is read |
|---------|--------------------|------------------|
| Canonical | `locales/<bcp47>.json` — flat `KEY` → string, sorted, all 11 locales carry the same key set | — |
| Web | — | `t('KEY')`, `tPlural('BASE', n)` from `app/web/src/lib/i18n.ts` |
| Apple | — | `Text("KEY")`, `String(localized: "KEY")`; with values, `String(format: String(localized: "KEY"), …)` |

- **Add a key to `locales/*.json` — all eleven** — then reference it. `bun scripts/sync-locales.ts`
  regenerates the String Catalog and the web copies; canonical is the only place a string is edited.
- **Placeholders are `{0}`, `{1}`** in canonical. `sync-locales` rewrites them to `%1$@` for Apple,
  so **a numeric argument must be converted at the call site** (`String(count)`) — `%@` takes an
  object. Plural keys take `_ONE` / `_OTHER` (`_ZERO` is optional and falls back to `_OTHER`).
- **Module-level tables hold KEYS, not text** (`MODALITY_META.displayNameKey`, `ELEMENT_TYPE_LABEL`,
  `PRESETS.labelKey`). A constant initialised at import time captures English before `initI18n()`
  resolves; resolve with `t()` at the point of render.
- **Not translated, and deliberately literal:** unit symbols and numbers (`Hz`, `mA`, `42%`,
  `1064nm`), product/tier designations and part numbers (`T1`, `ZM-PBM-DUAL`), enum and identifier
  values, single glyphs used as icons, and `.npps` parser / hub-compiler diagnostics — those name
  grammar keywords that are English by definition and read as compiler output.
- **An unused key is deleted from every locale file**, `_metadata.json` included. A key referenced
  by nothing is untranslated weight that translators are still asked to pay for.
- **Firmware is exempt because it renders no text at all.** It carries no locale key and includes no
  locale file; the device speaks in tones (`np_zone_audio.c`), LEDs and numeric status, and the app
  does the wording. A locale reference under `firmware/` means that boundary moved — a decision, not
  a detail.

`bun scripts/check-locale-strings.ts` enforces all of the above and fails CI on a violation; its
`PENDING_PATHS` names the code the rule has not yet reached (Android, Windows, the iOS
`Protocol/` and `Models/` display tables) so the gate's reach stays legible.

---

*This CLAUDE.md is the always-loaded core of the NeurOne design program: the invariants, and a map to
everything else. Detail lives in the subsidiary files listed in the Document Map — a section here
that names a file is a pointer, not a summary you may quote figures from. When a locked decision
changes, update the owning file, log it in `docs/status/completed-decisions.md`, and add an entry to
`docs/reference/claude-md-revision-history.md`. Keep every top-level section (§1–§6, §16) and every
subsection number in place even when its content moves —* `bun scripts/check-section-refs.ts` *guards
663 inbound citations that resolve against them.*
