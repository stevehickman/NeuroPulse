# CLAUDE.md — NeurOne Design program
**Project:** NeurOne — closed-loop multi-modal neuromodulation wearable platform  
**Revision:** 65 (current)  
**Status:** Pre-tooling design phase. No hardware committed yet. All decisions below are locked unless explicitly noted as pending.

> **This file is the always-loaded core: invariants only.** Each section states the rule and names
> the file that holds the figures, the rationale and the history. A section that names a file is a
> pointer, not a summary you may quote figures from. Read that file when the task needs it.
>
> **Why an invariant reads the way it does** is `docs/reference/claude-md-revision-history.md`
> (Rev 33 onward; earlier in git history and `docs/status/completed-decisions.md`). Read it before
> assuming why something is the way it is, or before reopening anything a revision settled.
>
> **Three live constraints that decide whether an answer is safe to give:**
> 1. **Every T1 configuration is gross-margin negative and every cost figure is a floor** (§2.1).
>    Retail is unlocked; no new price is set, and none may be set before `OI-HEXTILE-06`
>    (`OI-COST-10`).
> 2. **UHDR is never accessible to NeurOne** (§5). When in doubt about a new field → UHDR.
> 3. **The safety MCU owns every stimulation enable line** (§4.2). No app-side path may bypass it.

---

## 📂 DOCUMENT MAP — where everything lives

Plain paths, not `@import`s: nothing below loads until it is `Read`.

**Section detail** (read when working in that section):

| § | File |
|---|------|
| §2 in full (every figure), §6.1 | `docs/reference/commercial-model.md` |
| §3 | `docs/reference/modality-stack.md` |
| §4.1 · §4.2 (cVNS figures) · §4.3 · §4.4 · §4.5 · §4.7 | `docs/reference/hardware-detail.md` |
| §5.1 – §5.3 (per-field UHDR/SHDR rulings) | `docs/reference/data-architecture-detail.md` |
| §6.2 · §6.3 | `docs/reference/consent-engine.md` |
| §17 | `docs/reference/localization.md` |
| §18 | `docs/np_conv_001.md` §7.1 |

**Subject documents** (read before acting on the topic):

| Topic | File |
|-------|------|
| **Any BOM / COGS / GM% / price figure** | `docs/np_cost_001.md` |
| **Whether a PBM protocol fits the power envelope; any `.npps` or zone edit** | `docs/np_ses_pwr_001.md` |
| **Naming, signal names, `§N` form, document IDs** (authoring any doc) | `docs/np_conv_001.md` |
| **EEG electrode placement / 10-20 fit / T1-B tile / `REG-1`** | `docs/np_hw_eegnet_001.md` |
| PBM optical resolution floor ("targets region X" claims) | `docs/np_opt_psf_001.md` |
| **Any shielding claim, or adding to / removing from the §4.3 stack** | `docs/np_bib_emf_001.md` |
| **Cavity resonance, the retired Layer 4 station** | `docs/np_emc_cav_001.md` |
| **T1 → T2 path, tier gating, cross-tier interfaces** | `docs/np_reg_upg_001.md` |
| Accessory / applicator hardware (audio cup, nasal probe, VNS clip, cVNS, TMS coil, tACS driver) — DRAFT, requirements-grade; §3 is not their spec | `docs/np_hw_{audio,nasal,vnsclip,cvns,tms,tacsdrv}_001.md` |
| Accessories + companion SW · durability · service network | `docs/reference/{accessories-roadmap,durability-maintenance,service-network}.md` |
| Competitive position · regulatory strategy · clinical evidence · marketing | `docs/reference/{competitive-position,regulatory-strategy,clinical,marketing-notes}.md` |
| Open items · locked-decision log · document + firmware register | `docs/status/{pending-decisions,completed-decisions,document-register}.md` |
| **Every choice with no derivation, measurement or checked source**, and where it was decided (§19) | `docs/status/unjustified-choices.md` |
| Formal DHF index: the **single maintained index**, gated by `scripts/check-dhf-index.ts`. `document-register.md` is frozen history (`OI-CONV-07`, closed) | `docs/np_dhf_001.md` |
| Manufactured artifacts + which spec / risk register / FAI exists | `docs/np_art_001.md` |
| FAI programme | `docs/np_fai_001.md` |
| Risk: ISO 14971 index · hex-tile · shell/socket/hub | `docs/np_risk_002.md` · `_003` · `_004` |
| Hex-tile mould tooling · shell interconnect review | `docs/np_tool_hextile_001.md` · `docs/np_rev_shell_001.md` |
| Is this figure / file the current one? | `docs/superseded/README.md` |
| Questions drafted for a human reviewer (clinical, regulatory, principal); not design inputs | `docs/human-input/README.md` |

The three `docs/status/` logs are large; each opens with grep recipes. Do not read them whole.

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
- 4-layer EMF shielding + active Helmholtz cancellation, **designed to be measured** (the dB figures are targets
  until `EMF-1` runs, and any "only" or "measured" claim waits on `OI-BIBEMF-01`, §4.3)
- Modular field-upgradeability via snap-in zone modules
- No mandatory subscription — all core functions offline-capable permanently
- UHDR/SHDR data separation (user health data never accessed by NeurOne)

**T1 → T2 is a new unit, never a conversion** (→ `docs/np_reg_upg_001.md` §7):
1. **A T1 unit never enables a T2 modality or unlocks a T2 feature, whatever is attached**
   (`REQ-UPG-01`). The gate is a signed tier identity written once at manufacture (`REQ-UPG-02`).
   It is **not in force**, so every unit is T1 until `OI-UPG-08`, and the software gate is not
   built (`OI-UPG-01`). `NP_PROTO_FLAG_T2_TIER` is app-computed and decides nothing.
2. **Modules carry over to the purchaser's T2** (`REQ-UPG-03`): a change to a tile socket,
   accessory port or lens mount lands on both tiers or neither.
3. **"Field-upgradeable" means within a tier.** Never present T1 as upgradeable to Pro.

---

## 2. CONFIGURATIONS + PRICING (🔓 retail unlocked; charger policy §2.2 still locked) → `docs/reference/commercial-model.md`

**Every §2 figure and table is in `docs/reference/commercial-model.md` §2.1–§2.3** — retail
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

Retail is unlocked but no price is set, and **break-even binds before margin does** (every T1
break-even already exceeds its price in force). The T1 and T2 ladders collide (`OI-COST-08`) and
every competitive price claim is live again (`OI-COST-09`). Ladder and reasoning:
`docs/reference/commercial-model.md` §2.1a.

### 2.2 Charger policy (locked) → `docs/reference/commercial-model.md` §2.2

**Keyed to peak draw (§4.5), not price — unaffected by the retail unlock**; auto-included at every
upgrade by serial-number tracking. **EU:** chargers are branded recommendations, never proprietary
requirements; **any PD-compliant charger must work, and the app informs ("power level: reduced"),
never blocks.**

### 2.3 Consumables + recurring revenue → `docs/reference/commercial-model.md` §2.3

Intranasal hygiene sleeves are the **only authenticated consumable** and the primary MRR driver.
**Every consumable replacement prompt is measurement-triggered (§5.2), never calendar-triggered**,
and a calendar trigger is unimplementable anyway (no RTC backup, §4.5). A consumable with no
measurement gets no prompt, and inventing a trigger is design work, not a documentation edit. **A
threshold back-derived from a calendar interval is a calendar prompt.** A new prompt needs a
Trigger-column row before it ships (`scripts/check-consumable-triggers.ts`). Service and calibration
visits are outside the rule. The admissible trigger kinds, the scope carve-out and every row:
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

**Hard limits that constrain any protocol or firmware work** (enforcement in §4.2):

| Modality | Ceiling |
|----------|---------|
| PBM scalp | **Time-averaged Σ Ēᵢ / (200 × C_A(λᵢ)) ≤ 1 over the channels on one tile** (one channel alone: 200 mW/cm² at 660 nm, ≈ 329 at 808, ≈ 364 at 830) · **400 mW/cm² peak** (a design figure; above ≈ 850 nm it binds the average too) · duty and mode are the protocol's · 42 °C limit (IEC 60601-1 states 43 °C, `OI-BIBPBM-03`) |
| PBM deep (T2) | ≤1,000 mW/cm² (1170 nm, TEC-stabilised) |
| BES / tACS | 0.5–40 Hz · ≤1 mA T1 / ≤4 mA T2 · charge-balanced biphasic · **40 µC/cm² per phase** |
| tDCS | 0.1–2 mA DC · **150 mC/cm² per session** hardware limit · 30 s ramp · ≤3 electrode pairs |
| VNS (auricular) | 1–25 Hz · ≤2 mA · biphasic charge-balanced · **40 µC/cm² per phase** |
| Visual | IEC 62471 MPE at 50% of exempt-group threshold · photoparoxysmal halt <200 ms |

**A PBM ceiling refuses a protocol. It never reshapes one**. Duty, mode and
irradiance are the protocol's, because a pulse train clamped to fit is a different stimulus from the
one authored. A protocol outside a ceiling is rejected before it is signed, and if the hardware cannot
deliver one inside them, the hardware is redesigned.

**Two controls that bind PBM work** (the rest of the derivation, the C_A figures and the build
status are in `docs/reference/modality-stack.md` §3, from `docs/np_bib_pbmirr_001.md`):
- **The weighted sum is the whole average term, at zero margin at every wavelength.** There is no
  separate per-channel 200, and each wavelength is commanded independently but summed for safety.
  The C_A constants are unverified (`OI-BIBPBM-01`), so they gate the pre-signing check
  (`OI-HEXTILE-31`). The check and the U4 clamp are not built (`OI-HEXTILE-30`).
- **The 400 peak is a hazard control at 1064 nm** (it alone holds 1064 nm to 400, not 1000) and may
  not be raised or retired while `OI-BIBPBM-03` is open (§18).

**The charge ceiling is two ceilings, one per waveform class.** DC channels (tDCS, HD-tDCS) have
**150 mC/cm² per session** per electrode. Charge-balanced channels (BES/tACS, VNS, cVNS, clinical
tACS) have **40 µC/cm² per phase** per electrode. Never compare a per-phase limit with a session
integral. Both are **commanded-dose** limits, enforced against the electrode area in the signed
descriptor. Derivation, and why the 150 is provisional: `docs/np_dt_001.md` §3.2.1.

**Do not answer a modality question from this roster alone.** Wavelengths, materials, consumables,
evidence and open items are in `docs/reference/modality-stack.md`.

---

## 4. HARDWARE SPECIFICATIONS (all locked)

### 4.1 Processor stack
- **Main:** NXP i.MX RT1062 (Cortex-M7, 600 MHz) · FreeRTOS · USB-HS OTG
- **Safety MCU:** STM32G071 (**NOT G031**, whose 8 KB SRAM is too small for the EMF firmware) ·
  bare-metal · **owns all stimulation GPIO enable lines**
- **Storage:** 8 GB industrial eMMC · LittleFS · firmware partition write-protected · **separate
  UHDR/SHDR partitions from the first firmware line**
- **Connectivity:** USB-C 3.2 Gen1 is the default (zero RF) · BT 5.3 LE Audio · Wi-Fi 6 · **antennas
  in the control hub, not the headset**

Versions, clocks, memory sizes, CPU headroom, BOM delta and cycle counts: `docs/reference/hardware-detail.md` §4.1.

### 4.2 Safety architecture
- Safety MCU physically owns all stimulation enable GPIO — app crash cannot cause unsafe stimulation
- SPI heartbeat from main processor every 200ms; 1.5s watchdog → all-stimulation cutoff <50ms
- Dual-processor isolation: IEC 62304 Class C (safety MCU, bare-metal; size: `wc -l firmware/safety_mcu/src/*.c`) + Class B (main processor) separately certified
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
| Cervical VNS (T2) | Cardiac rhythm interlock | Safety MCU owns enable GPIO; monitors R-peak GPIO; a heart-rate step cuts the GPIO; **detection and cutoff times are stated separately, and a cutoff time is never quoted without its detection time**; lockout, app confirm and repeat impedance before re-enable. All figures: `REQ-CVNS-09` (`docs/np_hw_cvns_001.md` §3), `docs/reference/hardware-detail.md` §4.2 |
| T2 lines (cVNS, TMS, 1170 nm, clinical stim) | Tier identity (`REQ-UPG-01`) | Safety MCU withholds unless its signed, UID-bound OTP record says T2; fail-closed to T1. Not in force until `OI-UPG-08` |
| All | Firmware anti-fragility | CSPRNG session protocol signing |

### 4.3 EMF shielding (4-layer passive + active) → `docs/reference/hardware-detail.md`

Four passive layers (**1, 2, 3 and 5**) plus active fluxgate + Helmholtz cancellation. The design
target is **35–45 dB ELF magnetic / 40–60 dB RF**. **These are design targets, never measurements.**
`EMF-1` has never run, so no dB figure may be published as measured. Rules that bind other work:

- Layer 3 is **palladium, not silver**. It is tarnish-immune, and fleet SHDR attenuation verifies
  it. That is what makes the claim *permanent*.
- The shell is bonded to the EEG DRL output. The TMS coil site needs a **non-conductive CFRP
  window**.
- **Layer 4 (absorber foam) is deleted** (`REQ-CAV-04`). **Never renumber:** `L5` stays
  `L5`, because every historical "Layer 4" means the absorber. *"Five-layer keying"* is a retired
  RISK-15 scheme, not this stack. The binding 3 mm outer-bowl re-loft went with the deletion. The
  only documented way anything returns to that station is an insulating, non-magnetic pad under
  `OI-EMCCAV-08`, and never an absorber.

Before quoting a shielding claim or changing the stack, read `docs/np_bib_emf_001.md` (what the
evidence supports: EEG signal quality, not therapeutic outcome) and `docs/np_emc_cav_001.md`
(cavity). Per-layer dB and the deletion's thermal and tolerance figures are in
`hardware-detail.md` §4.3.

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
- Storage: on-device eMMC UHDR partition, AES-256 with a user biometric-derived key (NeurOne does not hold it); nightly incremental backup to USB-C local or E2E-encrypted cloud (user-held key)

**SHDR — System Health Data Record**
- Owner: NeurOne
- Linked to: device ID + opaque TRNG warranty token **only** — never to user identity
- **Consent subject: warranty owner**, who may be a clinic, institution or purchaser and is NOT assumed to be the wearer (§6.0)
- Defining test: does this tell us about the **device's condition**, with nothing that reveals user biology? If yes → SHDR
- Storage: separate eMMC partition and encryption; uploaded to the fleet database on USB-C connect, with warranty-owner consent

**Two rules that decide most new fields:**

1. **When in doubt → UHDR.** Reclassification requires positive demonstration of no user biology
   content.
2. **A redaction applied conditionally on a sensitive predicate leaks that predicate.** It must be
   unconditional, or its pattern must not reveal the predicate. The fault-latch cardiac oracle is
   the worked case. `scripts/check-redaction-shape.ts` enforces this rule.

**The contents of each record, and the ruling on each field, are in
`docs/reference/data-architecture-detail.md` §5.1.** That file is authoritative per field. A field
on neither list is decided by the tests above and then added there.

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

## 6. CLINICAL CONSENT ENGINE (all locked) → `docs/reference/consent-engine.md`

### 6.0 Two consent subjects (locked)

| Subject | Who | Data | Consent granted at | Managed by |
|---------|-----|------|--------------------|------------|
| **Warranty owner** | Entity that registered the device (clinic, institution or individual), **not assumed to be the user** | SHDR only | Warranty registration | `SHDRUploader` |
| **User** | Person wearing the device | UHDR | Research consent flow (L1–L4) | `ConsentStore` (per user, on-device) |

- A clinic registering a warranty has **not** consented for any patient. Each patient on a clinic
  device has their own `ConsentStore`.
- The two gates are code-structurally independent: `SHDRUploader` has no reference to
  `ConsentStore`, and revoking either consent does not affect the other.
- Withdrawing from a study or category stops only that flow. **Withdrawing blanket consent (L3)
  stops all research flows and tears down research analytics.**
- **"Never asked" and "said stop" must stay distinguishable.** `blanketConsentWithdrawnAt` is set
  by the store, never by a screen, and it outranks stale L2 ticks at the §6.3 gate.

### 6.1 Use case subscription tiers → `docs/reference/commercial-model.md`

Four clinician tiers (prices are in the owning file). **Clinicians select *use cases*, never data
elements.** The system derives the minimum UHDR elements, and the user gets a plain-language
statement of what the clinician can and cannot learn. **Retroactive and prospective access are
always separate decisions**, so a grant's reach is scoped in time and never derived from its tier
alone.

### 6.2 A priori research consent (4 layers, 2 onboarding screens)

**Layers are not screens.** L1–L4 are the data-model units, and "L3" always means blanket consent.
Screen **S1** carries L4 + L1 ("what you get back"). Screen **S2** carries L2 + L3 ("what you
share").

L1 contact · L2 category (9 research areas) · L3 blanket · L4 results (including null results). The
per-layer table is in `consent-engine.md` §6.2.

Binding invariants (rationale in `consent-engine.md` §6.2.2–§6.2.5):
- **L2 is scope and L3 is posture.** All nine L2 categories is not blanket consent, and
  **select-all never auto-enables L3.**
- **L4-first must not become an inducement.** Use conditional framing, name null results, and state
  non-coercion on the screen.
- **Withdrawal stays at study, category and blanket granularity.** The analytics teardown fires on
  a **true→false transition** at `updateResearchConsent`. `scripts/check-consent-reachability.ts`
  guards it.
- **L3 carries an irreversibility notice whenever it is on.** Per-project consent repeats that
  notice for vulnerable populations (45 CFR 46).
- **An L3 engagement notification and a consent request are different objects.** Silence means
  opposite things in each. An L3 user's opt-out is a withdrawal, not a decline.

### 6.3 Research suggestion portal (three functions) → `docs/reference/consent-engine.md`

Research agenda voting · pre-identified subject pool · crowdfunding pledges (intent, not charges).
NeurOne builds the invitation list from device ID and contact preferences only (**no UHDR**). **The
device does not trust the server:** it verifies the signed study descriptor, and
`ConsentEngine.admit()` re-checks k≥10, the ≥1-week floor, L1 and the user's L2/L3 state before
showing anything. What a study cannot see is **computed as the complement** of what it asked for.
The only shipped `StudyDescriptorVerifier` refuses everything (`OI-CONSENT-07`).

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

## 17. LOCALIZED STRINGS — CODE GENERATION RULE (locked) → `docs/reference/localization.md`

**Non-firmware code carries only a key. User-facing text goes in `locales/<bcp47>.json`**, the only
committed copy. There are 11 locales, flat and sorted. The web, Apple and Android files are
**git-ignored build outputs** of `bun scripts/sync-locales.ts`, so never edit or commit them.

- **Add the key to all eleven locales, then reference it** (`t('KEY')` web · `String(localized:)`
  Apple · `stringResource(R.string.key)` Android). **Read `localization.md` §17.1–§17.3 before
  adding a key**, because a malformed placeholder or plural family fails the build.
- **Module-level tables hold keys, not text.** Resolve them with `t()` at render time.
- **Each modality has exactly one name and one description:** `MODALITY_<ID>_NAME` and
  `MODALITY_<ID>_DESC`, from the `.npps` token. §3's consumer names live in the `bes_tacs` and
  `tdcs` descriptions.
- **Firmware renders no text and references no locale file.** If one appears under `firmware/`,
  that is a design decision, not a detail.

Gates: `bun scripts/check-locale-strings.ts` · `bun scripts/sync-locales.ts --verify-untracked`.

## 18. REQUIREMENTS — A REQUIREMENT MUST BE REQUIRED (locked) → `docs/np_conv_001.md` §7.1

**Do not write a requirement unless something requires it.** Before a number, limit, tolerance or
"shall" enters a controlled document, its row must answer two questions. **What fails if it is not
met?** **Where is that traceable?** (a standard, a measurement, a derivation, or a hazard control).
A Notes cell that restates the requirement is not a derivation. Most documents here make every
figure MANDATORY, so a figure nothing requires rejects usable parts. **Retire such a row; do not
downgrade it. Mark it retired in place, and do not delete it.**

**This does not license stripping limits.** It never reaches an external limit (IEC 60601-1 42 °C,
IEC 62471, the §3 charge ceilings) or a hazard control. It also does not reach a requirement whose
derivation exists but is uncited. **"I could not find the derivation" means raise an open item,
never retire.** Removing a safety control is an ISO 14971 decision.

## 19. UNJUSTIFIED CHOICES — THE REGISTER IS KEPT CURRENT (locked) → `docs/status/unjustified-choices.md`

**Every choice that has no derivation, no measurement and no external citation has a row in
`docs/status/unjustified-choices.md`, and the row is kept current in the same change that alters it.**
A citation to an external source (a standard, a paper, a datasheet) is a justification, and it takes the
choice out of the register. An internal document that restates the figure is not one. Checking an external
citation against its source is tracked by its own open item, never by a row. The register is an index. The
reasoning stays in the owning open item (OI), so a row points at it and never replaces it.

- **Adding one adds a row.** A change that introduces a constant, limit, tolerance, threshold or
  assumption with no derivation behind it, or that marks one `PROVISIONAL`, `PLACEHOLDER`,
  `UNCALIBRATED` or `UNVALIDATED`, adds its row in that change. So does a change that finds an
  existing one. **A row cites every source that bears on it** (the OI, the document section, each
  GitHub issue or pull request that decided or discussed it), not the first one found.
- **Justifying one moves its row.** A change that derives, measures or cites an external source for a
  listed choice moves the row to the register's "Justified or retired" table in that change, with a link to the
  justification. **A row is never deleted and its ID is never reused.**
- **Retiring under §18 moves the row too.** "I could not find the derivation" still means a row and an
  open item, never a retirement (§18).
- **A marker outlives the row.** The gate keys on the marker, so a marked value whose choice is externally cited is
  named in the "Justified or retired" table, and the marker stays until the open item that tracks the check closes.
- **The gate is partial.** `scripts/check-unjustified-register.ts` fails a marked value the register
  does not name. It cannot see an unmarked one. Marking it, or adding the row, is the author's duty.

## 20. SHARED FILES — ONE HOME EACH: `app/NeurOneShared/` and `common/` (locked) → `scripts/check-shared-placement.ts`

**A file whose content is common to more than one app (iOS, watchOS, Android, Windows, web) lives in
`app/NeurOneShared/`, directly or in a subdirectory of it, and is never stored in one app's
directory for the others to reach into.** Subdirectories are made as needed: `TestData/` for
fixtures several apps' tests read (`hub-descriptor-golden.json`), `Resources/` for generated or
hand-written resources several apps bundle (`Localizable.xcstrings`), `Sources/` for the Swift package.

- **Decide at creation.** A new file that a second app will read, bundle or diff against is created in
  `app/NeurOneShared/` the first time, and every reference points there.
- **Moving is not copying.** Two copies of one file is a violation; keep one in `app/NeurOneShared/`.
  A thin re-export shim (`@_exported import NeurOneShared`) is the one sanctioned residue at an old path.
- **Scope.** Data and resource files (json, xcstrings, plist, xml, npps, csv and the like). An app's own
  source, project spec, manifest and docs stay in its tree. Files a toolchain demands per app
  (`Info.plist`, Gradle wrapper, lockfiles) are exempt by name in the checker.
- **Enforced** by `bun scripts/check-shared-placement.ts` (CI job `shared-placement` in
  `tooling-ci.yml`), which fails an `app/<A>/` file naming a data file under `app/<B>/` and any
  identical content stored under two apps.

**A file shared by an app and a build artifact that is not an app (the simulator bundle, firmware)
lives in `common/` at the repository root, never in an app's tree and never in `app/NeurOneShared/`.**
`app/NeurOneShared/` is for apps only. `scripts/` is not a build artifact: it makes them, so a
script reading a file does not make that file shared.

- **Layout.** `common/` mirrors the old tree it was lifted from, so relative imports between its
  files do not change (`lib/`, `types/`, `locales/`, `generated/`). Today it holds the TypeScript
  the web app and the simulator bundle (`simulator/js/vendor/npps-runtime.js`) both build from, the NPPS
  field table (`npps/fields.json`) and the Rust NPPS core (`npps-core/`) and its bindings (`npps-jni/` for Android, `npps-ffi/` the C ABI iOS links), `OI-NPPS-CORE-01`; Android's descriptor compiler runs on it, no other app yet.
- **Generated shared files are written into `common/`.** `bun scripts/sync-locales.ts` writes
  `common/generated/locales/` (git-ignored) and `bun scripts/sync-socket-map.ts` writes
  `common/lib/socketMap.generated.ts`. A generator never writes a shared file into one app's tree.
- **Tests of `common/` code live in `common/`**, beside the file (`common/lib/x.test.ts`), and run under the
  web app's vitest (`app/web/vite.config.ts` includes `../../common`). A test that needs an app's code
  (the web serializer, for example) is not a test of `common/`; it stays with the app.
- **`common/` is the lowest layer.** It imports nothing outside `common/` (packages aside), so it may
  not depend on an app. If it needs one, the shared piece was cut in the wrong place.
- **Nothing non-app reaches into `app/`.** An import, `require` or `#include` under `simulator/` or
  `firmware/` that names a path under `app/` is a violation. A comment naming one is prose.
- **Never a React dependency.** `common/` has no `node_modules` above it, so a module needing React
  or another package the web app installs belongs in the app, not here.

---

*When a locked decision changes, update the owning file, log it in
`docs/status/completed-decisions.md`, and add an entry to
`docs/reference/claude-md-revision-history.md`. Keep every section number (§1–§6, §16–§20) and every
subsection number in place, even when the content moves. `bun scripts/check-section-refs.ts` guards
the citations that point at them.*
