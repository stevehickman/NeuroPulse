# Head-Presence Gate and Bench / Service Mode

**Project:** NeurOne
**Document:** NP-FW-BENCH-001
**Revision:** 1
**Date:** 2026-08-26
**Status:** DESIGN STUDY — not a release baseline. Every behaviour below is a proposed engineering commitment traced to a cited source; **no threshold in this document is measured, and the two the gate needs do not exist anywhere in the document set** (§4.4, `OI-BENCH-01`). See §11 (Decisions) and §12 (Open Items).
**Effective Date:** —
**Author:** NeurOne Firmware + Safety Engineering
**Approved By:** — (pending design review)
**References:** CLAUDE.md §3 (modality stack), §4 (hardware, safety architecture, status indicators), §5 (UHDR/SHDR), §6 (consent subjects); `NP-SW-001` Rev 3 §3 (SW-01/SW-02/SW-03 classification); `NP-HW-HEXTILE-001` Rev 8 §7.2–7.3 (19-position socket, `SEAT#`, contact sequencing); `NP-HW-HUB-001` Rev 6 (cluster-controller fan-out; `ALERT#`/`SEAT#` aggregation); `NP-DRV-SHELL-002` Rev 4 §3.3a, §5.1.3a (analog front end on the cluster controller); `NP-RISK-003` Rev 1 (RISK-14 dual photodiode); `NP-RISK-004` Rev 2 (RISK-SHELL-01, RISK-18); `NP-FAI-001` Rev 1 §3, §5; `NP-FW-EMMC-002` Rev 2 §G, §H (record-denominated windows; SHDR aggregation limit); `NP-FW-PBM1064-001` Rev 4 (PD1/PD2 dose metering); `NP-NPPS-REF-001` Rev 14 §4.1 (`zones`); `NP-HEX-ZM-001` Rev 3 §4a (`check_placement`, gate SW-1); `NP-HFE-002` Rev 2 §7.5; `NP-CONV-001` Rev 6 §4, §6, §8; `firmware/safety_mcu/` (SW-01); `firmware/hub_control/` (SW-02); `ci/shdr/shdr_fleet_schema.sql`
**Related Issues:** —
**Gate:** — (no programme gate; the gate this document specifies is a *session* precondition, not an `NP-COORD-001` milestone)
**IEC 62304 Class:** **SW-02 Class B** — both the head-presence gate and its bypass. The classification is argued in §5, and the argument turns on the gate never being the sole barrier for any hazard in the tree. **No SW-01 (Class C) source file changes, and no bit is added to any Class C wire format.**
**Supersedes:** None — new document.
**Parent Document:** `NP-SW-001`

---

> **⚠ READ FIRST — what this document is, and the three claims in its own brief that it had to correct.**
>
> A principal decision of 2026-08-26 adds a head-presence gate: *a protocol must not run when the
> helmet is not on a head.* The moment that ships, every bench activity — first-article inspection,
> service, hardware bring-up — is blocked. A bypass will be improvised unless it is designed. This
> document designs the bypass, and because the bypass cannot be specified against an unspecified
> gate, it specifies the gate too.
>
> **Three things asserted as established context did not survive checking, and the design changes
> because of them.**
>
> 1. **The safety MCU cannot read either candidate head-presence sensor.** Its impedance analog
>    front end has exactly four channels — `VNS_HRV`, `tDCS`, `BES_TACS`, `CVNS`
>    (`firmware/safety_mcu/src/np_impedance_check.c`, `k_imp_slot` / `k_imp_en_bit`;
>    `firmware/safety_mcu/include/np_safety_config.h`, `NP_IMP0_ADC_CH`…`NP_IMP3_ADC_CH`) — and none
>    of them is a cranial EEG electrode. EEG electrode impedance is read by the **hub** through the
>    ADS1299 (`firmware/hub_control/modules/np_mod_eeg.c:57`,
>    `np_mod_eeg_hal_read_impedance()`), and PD2 photocurrent leaves the socket on pin 14 (`PD2_K`)
>    into the **cluster controller's** TIA/mux/ADC (`NP-HW-HEXTILE-001` §7.2;
>    `NP-DRV-SHELL-002` §3.3a). **Both sensors are on the Class B side of the boundary.** That fact,
>    not a preference, is what decides §5.
> 2. **`SEAT#` has no firmware consumer, and the safety MCU has no `SEAT#` input.**
>    `grep -rn "SEAT" firmware/` returns nothing but unrelated prose about goggle seating and module
>    seating; `np_safety_config.h` allocates no pin to it; and `NP-HW-HUB-001` places `ALERT#` and
>    `SEAT#` **aggregation at the cluster controller** — the Class B tier. The module-swap stop is
>    therefore specified today as a Class B function, not, as the brief for this study stated, a
>    safety-MCU enable-line drop. §8 designs against what is on file and raises the discrepancy as
>    `OI-BENCH-06` rather than assuming it away.
> 3. **Neither `clinical-09` nor `07-vascular-baseline` is a bench workflow.** `clinical-09`'s
>    `zones: clinician_selected` is *targeting* — the operator picks sockets over the lesion margin
>    before the run, on a patient who is wearing the helmet (`NP-NPPS-REF-001` §4.1;
>    `protocols/predefined/clinical-09-pbm-stroke-rehab.npps`). `07-vascular-baseline` is a
>    *therapeutic* maintenance session — *"Use as a maintenance or recovery session"*, 30 min CW PBM
>    plus VNS and EEG, worn. Neither needs bench mode; §9 says what each does need instead.
>
> **One finding falsifies the obvious first guess at the gate's threshold.** `NP-FAI-001` §5
> FAI-IPX-01 accepts an assembled headset, **not on a head**, at *"All EEG channels < 10 kΩ after
> test"*. Whatever hydration state that item assumes, it asserts on the record that an off-head
> headset can read below 10 kΩ. **10 kΩ therefore cannot be reused as the head-present threshold**,
> and `NP_IMPEDANCE_MAX_OHM` (also 10 kΩ) is a tES contact limit for 25 cm² pads read through an
> explicitly **UNCALIBRATED** reference leg (`np_safety_config.h`, `NP_IMP_SENSE_R_OHM`,
> `OI-SWCI-34`). The gate's two numbers are not derivable from anything in this repository. They are
> `OI-BENCH-01`, and they are blocking.

---

## 1. Scope

**In scope:** the head-presence gate (sensor selection, decision rule, disagreement handling, failure
direction); the bench/service bypass (ownership, entry, scope, annunciation, audit, exit); the list
of interlocks the bypass may never touch and the structural means by which it cannot; the
interaction with the first-article programme, with operator-targeted protocols, and with `SEAT#`.

**Out of scope:** the sensor hardware itself (no new part is proposed — §4.1); the `NP-CFG-UI-001`
socket-selection UI, which is cited by `NP-NPPS-REF-001` §4.1 and §12 and **exists nowhere in the
document set or in `NP-DHF-001`** (`OI-BENCH-07`); the commercial service network
(`docs/reference/service-network.md`); and any change to SW-01 source.

**Nothing in this document is implemented.** No code changed with its release.

---

## 2. Why a bypass has to be designed rather than left to arise

Two properties of the programme make an undesigned bypass the worse outcome, not merely an untidy
one.

- **The bench population is not marginal.** `NP-FAI-001` §3's checklist structure puts *"Functional
  test — the artifact doing its job in a complete system"* at item 6 of every artifact checklist,
  and `NP-ART-001` records fifteen manufactured artifacts. Bring-up, service (`Tier B` partner
  visits) and FAI all put a complete, powered, unworn helmet in front of an operator who needs it to
  emit.
- **The improvised bypass is always the wrong shape.** The improvisations available without a design
  are a build flag, a commented-out check, or a debug protocol — all three of which are invisible to
  annunciation and to audit, and none of which can be revoked after it leaves the building. §6
  compares them explicitly rather than dismissing them.

The adversary this design is built against is **not** a malicious attacker. It is a well-meaning
user following a forum post. That choice of adversary is load-bearing and it decides §6: a mechanism
whose *description* is sufficient to defeat it fails, and a mechanism that survives full publication
passes.

---

## 3. What the gate is for, stated narrowly

The gate answers exactly one question — *is tissue in contact with the applied parts?* — and it
answers it to prevent one class of outcome: therapeutic energy delivered into something that is not
the intended target. It is **not** a proxy for correct fit (that is the Boa dial, the forehead bridge
and `NP-HW-EEGNET-001`'s registration model), **not** a proxy for the right person, and **not** a
proxy for consent.

Stating it narrowly matters because a gate that is believed to mean more than it measures is how a
Class B availability control gets treated as a Class C safety control by the next reader.

---

## 4. The gate

### 4.1 Sensor selection — no new part

Four sensors in the current design can be argued to bear on head presence. Only one survives as a
pre-start gate.

| Candidate | Where it is read | What it actually measures | Verdict |
|---|---|---|---|
| **EEG electrode contact impedance** | Hub, ADS1299 — `np_mod_eeg_hal_read_impedance(ch)` (`np_mod_eeg.c:57`, `OI-EEG-05`) | AC impedance between a semi-dry hydrogel tip and whatever it touches, at ≤8 cranial sites (Fp1/2, F3/4, C3/4, P3/4 — CLAUDE.md §3 modality 3) | **PRIMARY.** Active measurement; present in every shipped configuration including Core; already taken at session start, since the ADS1299 internal-reference self-calibration runs *"at every session start"* (CLAUDE.md §3) |
| **PD2 scalp-facing photodiode** | Cluster-controller TIA/ADC via socket pin 14 `PD2_K` (`NP-HW-HEXTILE-001` §7.2); `NP_ELEM_PD_BACK` in `np_module_map.h:230` | Backscattered **tissue** optical power — RISK-14 Option B, `NP-RISK-003` | **IN-SESSION CORROBORATOR ONLY** — see §4.2 for why it cannot be a pre-start gate |
| **tES contact impedance** | **Safety MCU**, `np_impedance_check.c`, 1 kHz / 50 ms, reject above `NP_IMPEDANCE_MAX_OHM` | Contact at the VNS clip, tDCS/BES pads, cervical pads | **ALREADY A CLASS C GATE, UNCHANGED.** Not part of the head-presence gate — it is upstream of it and stricter (§7 item 3) |
| **IR proximity (940 nm) + Hall, at the lens** | Hub — `np_mod_visual_hal_ir_eye_open()`, `np_mod_visual_hal_hall_lifted()` (`np_mod_visual.c`, `OI-VIS-03`/`-04`) | Eye-open, and goggle seating | **ALREADY A PER-MODALITY GATE, UNCHANGED.** Lens-only, and semantically ambiguous in-tree — see the note below |

> **A naming defect in the visual interlock, recorded because the head-presence gate must not repeat
> it.** `np_mod_visual.c` uses `np_mod_visual_hal_hall_lifted()` for three different propositions
> within one file: its HAL comment at `OI-VIS-04` reads *"goggles off head"*, its detect path reads
> *"magnet present → goggles seated"* (line 63), and its control path comments *"goggles must be on
> head"* (line 106). *Goggles seated on the headset* and *headset on a head* are different physical
> facts, and one predicate cannot be both. Not fixed here — no code changes with this document — but
> the head-presence predicate specified below is named for what it measures, not for what it is
> hoped to imply. `OI-BENCH-08`.

**No new sensor is proposed.** That is a finding, not an economy: the pre-hex cost work already
records the InGaAs photodiode pair as the dominant recoverable BOM term (CLAUDE.md §2.1 note 3,
`NP-COST-001` §6), so a design that needed a *further* presence sensor would be arguing against a
live cost decision, `OI-HEXTILE-06`.

### 4.2 The PD2 circularity, resolved

PD2 measures light that came back out of tissue. Seeing anything at all requires the emitters to be
on. As a **pre-start** gate that is circular: the gate would authorise the emission it needs in order
to decide.

Three ways out were considered:

| Option | Description | Disposition |
|---|---|---|
| **(a) Sub-therapeutic probe pulse** | Drive emitters briefly at a level below any dose-relevant irradiance, read PD2, then decide | **Rejected as the general answer.** It still emits into an unverified target, it needs an irradiance floor nobody has set, and it is *unavailable in the Core configuration*, which has no emitters at all (CLAUDE.md §2.1: Core is EEG only). A gate that does not exist in one shipped configuration is not the gate |
| **(b) Borrow the 940 nm IR proximity emitters** | Already fitted, already an active emitter/detector pair, no circularity | **Rejected for the cranial case.** They are in the lens (`np_mod_visual.c`), not on the scalp-facing lattice. Retained where they already are |
| **(c) Do not use PD2 as a pre-start gate at all** | Impedance decides at start; PD2 corroborates during the session, sampled from the therapeutic emission already running | **ADOPTED — D-2** |

Option (c) costs nothing because impedance already covers session start, and it removes the
circularity by removing the requirement rather than engineering around it.

### 4.3 The decision rule

> **`HEAD_PRESENT` is true iff at least `K` of the `N` electrode channels enabled for the session
> report a contact impedance at or below `Z_HP`, and the reading is fresh.**

Six properties, each with its reason:

1. **`Z_HP` is a distinct, higher threshold than the EEG signal-quality threshold.** A helmet on a
   desk reads open; a helmet on a head with a tired hydrogel tip reads high but finite. Refusing to
   start because the gel is dry is a *consumable* decision — CLAUDE.md §2.3 already routes it to an
   impedance-trend prompt and an 8-pack reorder — and collapsing it into the presence decision turns
   a reminder into a hard block. **One sensor, two thresholds, two meanings.**
2. **`K` of `N`, not all of `N`.** A single lifted electrode over thick hair is the ordinary case, not
   an absent head. Requiring unanimity manufactures `RISK-18`'s hazard shape — *presence-detect
   false negative blocks a session* (`NP-RISK-004` §2) — at ~8× the rate of any single contact.
3. **Freshness.** A reading older than one debounce window is not a reading. Stale-value reuse is
   what `np_impedance_check.c` guards against for CVNS by invalidating `s_cvns_report_valid` at each
   request (`OI-CVNS-HUB-11`); the same discipline applies here.
4. **Fail-closed.** No reading, HAL error, non-finite value, ADS1299 ID mismatch
   (`np_mod_eeg.c:78` already refuses on `(id & 0x1F) != 0x1E`) → `HEAD_PRESENT` is **false** → the
   session is refused. This follows the direction `np_safety_config.h` already states for the tES
   AFE: *"errors report an impedance above `NP_IMPEDANCE_MAX_OHM`, refusing the enable"*, a direction
   that holds *"regardless of calibration"*.
5. **Continuous, not one-shot.** Evaluated at start and throughout. See §10 for what loss does.
6. **The gate is never switched off.** Bench mode suspends the gate's **veto on session start**; it
   never stops the gate being evaluated. This is what makes exit-on-reacquisition (§10) possible at
   all, and it is the difference between *suspending a decision* and *blinding a sensor*. **D-4.**

### 4.4 The two numbers this rule needs, and why neither is invented here

`Z_HP`, `K` and the debounce depth `T_HP` are **not set by this document.**

- `NP_IMPEDANCE_MAX_OHM = 10000` is a tES limit for a 25 cm² pad, measured through
  `NP_IMP_SENSE_R_OHM`, which `np_safety_config.h` marks *"an UNCALIBRATED placeholder: the ohms
  `np_hal_impedance_read_ohm()` returns do not rest on any bench measurement"* (`OI-SWCI-34`).
- `NP-FAI-001` FAI-IPX-01 accepts *"All EEG channels < 10 kΩ"* on an assembled headset **not on a
  head**. Reusing 10 kΩ as `Z_HP` would therefore, on the face of the FAI's own accept criterion,
  classify a bench headset as worn.
- `ci/shdr/shdr_fleet_schema.sql`'s `eeg_impedance_trend` holds `trend_slope_ohm_per_session` — a
  *"derived metric only"*. There is no absolute-impedance population anywhere in the fleet schema to
  fit a threshold against, by design.

Setting these numbers requires a measured off-head and on-head impedance distribution across head
sizes, hair types and tip age. **`OI-BENCH-01`, blocking on the gate shipping.** This is the same
defect class `NP-FW-EMMC-002` §G.2 records for `NP_ACCEL_DROP_THRESHOLD_G` and
`NP_ACCEL_MAINT_THRESHOLD` — *"chosen before any hardware existed and … never compared against a
device that failed"* — and it is named here at specification time rather than discovered later in a
shipped constant.

### 4.5 Sensor disagreement

The brief asks what happens when the two sensors disagree. The honest first answer is that
**"disagreement" is largely a category error**: impedance measures galvanic contact at ≤8 discrete
sites; PD2 measures optical backscatter under one tile. A helmet correctly worn by a person with
dense hair can hold good frontal electrode contact and poor parietal backscatter, and neither reading
is wrong.

| Impedance | PD2 | Action | Reason |
|---|---|---|---|
| present | present | run | — |
| **absent** | present | **refuse / stop** | Fail-closed. The union of absences wins; a corroborator cannot overrule the gate |
| present | **absent** | **run**, and raise a device-health signal | PD2-low with contact-good is the **fouling** signature the PD1/PD2 ratio already exists to discriminate (`np_pbm1064_dose_evaluate_ratio()`, `firmware/pbm_1064nm/src/np_pbm1064_dose.c:157`; `NP_PBM1064_FOULING_RATIO_THRESH`). Treating a fouled window as an absent head would mis-attribute a maintenance condition to the user |
| absent | absent | refuse / stop | — |

**Therefore PD2 is never a veto — D-3.** It corroborates, and where it dissents it feeds the existing
fouling/aging discriminator rather than the gate.

---

## 5. Ownership and IEC 62304 class

> **The head-presence gate and its bypass are both SW-02, IEC 62304 Class B. Neither is Class C.**

The argument has three legs; the first alone is close to dispositive.

**(i) The Class C tier physically cannot see the sensors.** Enumerated in the banner above: the
safety MCU's four impedance channels are the tES channels, the ADS1299 is on the hub, and `PD2_K`
terminates at the cluster controller. Making the gate Class C means routing a new sensor into the
Class C boundary and allocating a new input on an MCU whose GPIO and ADC map is already fully spoken
for and itself provisional (`np_safety_config.h`: PA0 cranial enable, PA1–PA3/PB10–PB12 NTC,
PA4–PA7 SPI1, PA8 R-peak, PB0–PB8 the nine remaining enables; `OI-SWCI-28`). That cost has to be
earned by the hazard.

**(ii) The gate is the sole barrier for nothing.** With head presence bypassed, every one of these
still holds, unchanged and independently:

| Barrier | Tier | Evidence |
|---|---|---|
| 62 °C junction thermal cutoff, 5 cranial sense domains + hub | **C** | `np_thermal_interlock.c`; `NP_NTC_CUTOFF_DEG_C`. Reads junction temperature; indifferent to whether a head is present |
| 40 µC/cm² charge-density limit, per electrode per session | **C** | `np_charge_monitor.c`; `NP_CHARGE_LIMIT_UC_CM2`; plus the `OI-CHARGE-03` fail-safe geometry gate |
| tES contact confirmation | **C** | `np_impedance_check.c` — and see §7 item 3: it makes tES *unrunnable* on a bare bench |
| IEC 62471 MPE ceiling, hardware current limit | **C** | CLAUDE.md §4.2 interlock table |
| Session-descriptor Ed25519 signature | **C** | `np_session_sig.c`; `NP_SAFETY_STATUS_SIG_PENDING` blocks `granted_mask` |
| 200 ms heartbeat / 1.5 s watchdog, cutoff < 50 ms | **C** | `np_spi_watchdog.c` |
| Cervical VNS cardiac interlock | **C** | `np_cardiac_interlock.c` |
| Fault latch with warm-reset persistence | **C** | `np_fault_latch.c` |
| Hall goggle-lift cutoff, IR eye-open | **B** | `np_mod_visual.c` |
| Photoparoxysmal halt at Oz, < 200 ms | **B** | `np_mod_visual_ppx_halt()` |

The residual hazard of running unworn is optical and thermal exposure of *whatever is in front of
the emitters*, bounded by the 62 °C and 62471 limits above — with **one exception**, the lens, whose
ocular protections are themselves presence-conditioned and therefore degrade off-head. §7 removes the
visual modality from bench mode for exactly that reason, which converts the one Class-C-arguable case
into a scope restriction rather than a class escalation.

**(iii) The Class C tier must not learn that a bypass exists.** `NP-SW-001` §3.2's Class B
justification for SW-02 rests on SW-01 being an *independent* backstop. A bypass bit crossing the
SPI link would make SW-01's behaviour a function of a SW-02 policy decision, which is precisely the
independence the classification is built on. So the rule is stated as a prohibition with a
mechanical check behind it (§7.2), not as a convention.

**Residual, stated rather than buried:** if a future configuration ever makes head presence the sole
barrier for a hazard — a modality with no thermal, charge or ocular backstop — this classification
must be re-derived. Nothing in the current modality stack (CLAUDE.md §3) has that shape.

---

## 6. Entry — and why three of the four candidate mechanisms fail

| Mechanism | Cost | Defeated by publication? | Leaves an event to annunciate and audit? | Verdict |
|---|---|---|---|---|
| **Build-time flag** | zero | n/a — it is not a secret, it is a *build* | **No** | **REJECTED for any shipped device** |
| **Physical action no wearer would perform** | near zero | **Yes** — a forum post describes it perfectly, and once described it is free | Yes | **REJECTED as sole factor; RETAINED as second factor** |
| **Signed service credential** | reuses existing Ed25519 + OTP root | **No** — publish the whole scheme and a user still cannot mint one | Yes | **ADOPTED as primary — D-5** |
| **Time-boxed session** | — | — | — | Not an entry mechanism at all; it is *scope*. Combined with the above |

**Why a build-time flag is not acceptable for a shipped device, plainly.** It produces two firmware
images that the fleet cannot distinguish: `ci/shdr/shdr_fleet_schema.sql`'s `firmware_history`
records a version, not a build configuration, so a device shipped with the bypass compiled in is
indistinguishable in telemetry from one without. It generates no entry event, so §8's annunciation
and audit requirements have nothing to fire on. And it cannot be revoked after the unit leaves the
building. The repository does contain a compile-time gate — `NP_MODE_F_REGULATORY_CLEARED` in
`np_mod_visual.c` — and it is the *inverse* case: it **removes** capability from the shipped build
pending clearance. A flag that **adds** a bypass in the field is not the same pattern wearing a
different sign. A build-time flag remains legitimate for an engineering image that is never shipped,
and even there it must annunciate (§8).

**The adopted mechanism.**

> **Bench mode is entered by redeeming an Ed25519-signed service credential, bound to this device,
> while a physical service assertion is held; it grants a window denominated in monotonic ticks and
> session count; it is held in RAM only and does not survive a power cycle.**

Each clause earns its place:

- **Ed25519, because the primitive is already here.** `np_session_sig.c` verifies Ed25519 against a
  manufacturing root public key read from OTP (`np_hal_otp_read_pubkey`), with an
  all-zero-means-unprovisioned sentinel and `NP_FAULT_SLOT_UNPROV`. Crypto is Monocypher 4.0.2, RFC
  8032 §5.1.7 (`firmware/crypto`, `np_crypto`). No new primitive, no new key format.
- **Verified on the hub, not the safety MCU.** Per §5(iii). The hub therefore needs its own
  service-authority public key, and **where that key lives and how it is provisioned is not
  answerable from anything on file** — the OTP that holds the existing root is on the STM32G071.
  `OI-BENCH-03`, blocking.
- **Device-bound, so it is non-transferable.** The credential covers the device's identifier, so a
  credential shared on a forum authorises somebody else's helmet and nothing of the reader's.
  Publication of the *scheme* costs nothing; publication of a *credential* costs one device that
  the credential's holder already had physical access to.
- **The window is denominated in ticks and sessions, never in wall-clock time.** This is not a
  preference; it is forced by the same fact `NP-FW-EMMC-002` §H had to design around. The device has
  no battery, no coin cell and no `VBAT` rail — CLAUDE.md §4.5 is USB-C powered throughout, with
  Mode 3 running from a power bank — so the RT1062's SNVS RTC has no backup domain and wall time is
  re-supplied by the phone. A calendar expiry would be **both losable and settable backwards**
  (completed-decisions, 2026-08-12). The window is therefore `np_hal_get_tick_ms()` elapsed since
  redemption, plus a session budget, plus a per-session duration cap.
- **RAM only.** Bench state is never written to the eMMC and never survives reset. This makes "does
  not persist" a structural property rather than a policy, and it means the recovery from any
  confusion about a device's state is *unplug it*.
- **Plus a held physical assertion**, as a second factor. It cannot be performed accidentally by a
  wearer and it cannot be performed remotely at all. It does not have to be secret — it is not
  carrying the security, it is carrying the *accident* case.

**What this defends and what it does not.** Against the stated adversary — a well-meaning user
following a forum post — it holds completely, because the post cannot supply a credential. It does
**not** defend against a service technician who chooses to misuse a valid credential, and it is not
intended to; that is a process control (`NP-QMS-001` records), and the audit trail in §8 exists to
make it visible after the fact.

---

## 7. What bench mode must never bypass

### 7.1 The list

Every item below is **never bypassable, in service or otherwise**, and each row states why the answer
is *never* rather than *not yet*.

| # | Interlock | Tier | Why never |
|---|---|---|---|
| 1 | **62 °C junction thermal cutoff** | C | The hazard is thermal injury to whatever is in contact — on a bench that is an operator's hand. Bench conditions are also the *less* favourable ones: `RISK-26` already records a scalp-face-over-limit path with the junction NTC nominal, in an assembly whose outward thermal resistance is dominated by a stagnant inter-bowl gap (`NP-RISK-004`; `NP-THERM-CFD-C2-001` §7) |
| 2 | **40 µC/cm² charge-density limit** | C | Charge accumulates on commanded current, not on a load hypothesis. The `OI-CHARGE-03` geometry gate stays armed too, or a bench HD-tDCS run accrues at the permissive 25 cm² default |
| 3 | **tES contact confirmation** (`NP_IMPEDANCE_MAX_OHM`) | C | **Consequence stated deliberately: tES cannot run on a bare bench, and that is correct.** A bench rig has no tissue load; bypassing this would drive 2–4 mA into an unknown impedance. Bench verification of tES uses a resistive phantom, which passes the *real* gate and therefore needs no bypass at all |
| 4 | **IEC 62471 MPE ceiling, IR eye-open, Hall goggle-lift** | C (current limit) + B | The ocular case is the one hazard whose remaining protections are themselves presence-conditioned, so they degrade exactly where bench mode operates. Hence item 5 |
| 5 | **The visual modality is excluded from bench mode entirely** | — | Bench mode never enables `NP_SAFETY_EN_VISUAL`. Firmware cannot verify that a lens is in a light-tight fixture, so it must not assume one. **D-6** |
| 6 | **Photoparoxysmal halt at Oz** | B | Not bypassable — and independently, it is **vacuous off-head**: it is computed from an EEG channel that has no signal without a scalp. A protection that cannot fire is a second reason item 5 holds |
| 7 | **Session-descriptor Ed25519 signature** | C | If bench mode became an unsigned-protocol path it would be a larger hole than the one it was opened for. A bench protocol is signed like any other |
| 8 | **200 ms heartbeat / 1.5 s watchdog** | C | Nothing about a bench makes a hung main processor safe |
| 9 | **Cervical VNS cardiac interlock** | C | Follows from item 3 — CVNS cannot pass its own impedance gate on a bench |
| 10 | **Module-swap stop (`SEAT#`)** | B (as specified) | §8 |
| 11 | **`np_module_map_check_placement()`** | B | §8 |

### 7.2 How this is structural, not asserted

Item 4 of the brief asks that the restriction be enforced structurally. It is, and the enforcement is
an **absence of a wire**:

- Bench mode is a hub-side predicate. It appears in **no** field of `np_safety_rx_ext_frame_t`, sets
  **no** bit in `session_status` (`NP_SESSION_STATUS_ACTIVE`, `_CVNS_REENABLE`, `_GEOM_REQUIRED` are
  the three allocated), and is **never** transmitted over the SPI link.
- The safety MCU computes `granted_mask` solely as `requested_mask & NP_SAFETY_EN_ALL_MASK` when
  `active_faults == 0`, and `0` otherwise (`np_spi_watchdog.c`, `np_spi_watchdog_tick`). With no
  bench input, **SW-01's behaviour is bit-identical in and out of bench mode.** Every Class C row in
  §7.1 is therefore unreachable from the bypass — not by policy, but because there is no path.
- Bench mode can only ever *widen the set of preconditions SW-02 itself evaluates*. It cannot relax
  a precondition it does not own.

**This is checkable mechanically, and per `NP-CONV-001` §8 it should be.** The required check is:
*no bench-mode identifier appears anywhere under `firmware/safety_mcu/`, and no new bit is allocated
in `session_status`.* Per §8 of that document the probe must be **falsified before it is trusted** —
introduce the symbol into a safety-MCU translation unit and confirm the check fails. Specified here,
not written: `OI-BENCH-04`.

---

## 8. `SEAT#`, the module-swap stop, and modules legitimately absent

### 8.1 What is actually on file

`SEAT#` is socket **pin 19** of the 19-position array, tied to `PGND` on the module through 1 kΩ,
positioned at the mechanical extreme so it is the **last to mate and first to break**
(`NP-HW-HEXTILE-001` §7.2, §7.3; `NP-DRV-SHELL-002` §5.1.3a; the *"pin 16"* in HEXTILE's table is
the Rev 2 column, not the current position). It exists to catch a **partially** seated tile that
answers I2C while `PD1_K` sits at elevated contact resistance and returns a silent dose under-read —
`RISK-SHELL-01`, HIGH, `NP-RISK-004` §2.

Two things the brief stated as settled are not on file:

- **No firmware anywhere consumes `SEAT#`.** `grep -rn "SEAT" firmware/` returns only unrelated
  comments about goggle seating and module seating.
- **The safety MCU has no `SEAT#` input**, and `NP-HW-HUB-001` places `ALERT#` and `SEAT#`
  aggregation at the **cluster controller** — Class B. So the module-swap stop as specified is a
  Class B function. Whether it should be Class C is a real question and is **`OI-BENCH-06`**; this
  document does not answer it, and does not assume the answer.

### 8.2 The design consequence

If the module-swap stop and the head-presence gate both live in SW-02, a single bench predicate could
switch off both. It must not, and the reason is that **bench testing is the activity `SEAT#` exists
to serve**: suppressing a partial-seating detector on a bench suppresses it precisely where
partial seating is being introduced, ten times per tile, by `NP-FAI-001` FAI-IPX-02.

> **Bench mode does not relax `SEAT#`, and `SEAT#` deassert during a bench session stops
> stimulation exactly as it would in a worn session. D-7.**

### 8.3 The accommodation a bench rig actually needs, which is different in kind

A bench rig legitimately has modules out. The correct answer is **not** a presence bypass, because
one already exists and it is authored, not switched: `np_module_map_check_placement()` validates the
live UID-derived inventory against *the running protocol's* required module map, and a mismatch
blocks the session (`NP-HEX-ZM-001` §4a, gate SW-1; `NP-RM-001` §7 usability hazards;
`firmware/hub_control/include/np_module_map.h:532`). A rig with three tiles runs a protocol that
requires three tiles.

**So: missing modules are a protocol-authoring question; partially-seated modules are a defect.** The
two look alike at the socket and are opposite in disposition, and `SEAT#` is exactly the signal that
tells them apart. That is why bench mode touches neither.

The `SEAT#` debounce requirement re-scoped from the retired `ZONE_ID` (`OI-HEXTILE-08`,
`RISK-18` CARRIED) governs both cases and is unchanged by this document.

---

## 9. Real workflows

### 9.1 First-article inspection — `NP-FAI-001`

Walking the programme item by item rather than assuming "FAI is bench work, therefore FAI needs bench
mode":

| Item | Needs bench mode? | Why |
|---|---|---|
| FAI-M01…M03, FAI-TC01…TC04, TC04a, TC06 | **No** | Coupon peel, thermal chamber, spectrophotometer, document review. No device is powered |
| FAI-TC05 | **No** | Method is explicitly *"Bench supply + tile"* — 3 complete tiles, 500 LED cycles, PD1 monitored. A tile on a bench supply is not a helmet running a protocol; nothing in the session path is involved |
| FAI-IPX-01 / IPX-02 | **No — and it constrains the gate.** | The accept criterion is a *measurement*, *"All EEG channels < 10 kΩ"*, not a stimulation run. As §4.4 records, that same criterion is what falsifies reusing 10 kΩ as `Z_HP` |
| FAI-IPX-03 / IPX-04 / IPX-05 | **No** | Visual, dimensional, damp-heat, calculation |
| **Checklist §6 — functional test** (`NP-FAI-001` §3: *"the artifact doing its job in a complete system"*) | **YES** | This is the whole bench-mode population inside the FAI programme, and it is one section of each per-artifact checklist |

**Finding: the FAI programme's need for bench mode is narrower than it looks — one section per
checklist — and the FAI's own ingress criterion is a live constraint on the gate's threshold.** Both
are reasons to specify the bypass tightly rather than broadly.

### 9.2 `clinical-09-pbm-stroke-rehab` — not a bench case

`zones: clinician_selected` means *the operator picks the sockets over the lesion margin plus midline
before this protocol can run*, because perilesional cortex is patient-specific
(`protocols/predefined/clinical-09-pbm-stroke-rehab.npps`; `NP-NPPS-REF-001` §4.1 and §12 — one of
exactly two legal `zones` forms). The patient is wearing the helmet. **Head presence holds; bench
mode is the wrong answer and would substitute a safety bypass for a missing UI.**

What it actually needs is `NP-CFG-UI-001`, cited twice by `NP-NPPS-REF-001` as the owner of operator
socket selection and **absent from the document set and from `NP-DHF-001`** — a findable absence in
`NP-CONV-001` §4.0's sense. `OI-BENCH-07`.

One genuine interaction with the gate: socket selection happens *before* the run. If selection is
only possible with the helmet off, and running requires it on, the workflow contains a doff/don
between the two, and the selection must survive it. That is a UI persistence requirement, recorded
here so `NP-CFG-UI-001` inherits it.

### 9.3 `07-vascular-baseline` — not a bench case either

It is a *therapeutic* maintenance session — *"Continuous-wave PBM for cerebrovascular support …
Use as a maintenance or recovery session"*, 30 min, `zones: ["All"]`, plus 1 Hz VNS and closed-loop
alpha EEG. It is worn by a person. The device-maintenance concept in this programme is the
measurement-triggered reminder engine and the predictive-maintenance telemetry (CLAUDE.md §5.2), not
a protocol.

The genuine device-maintenance activity that *does* approach the gate is a **self-test that drives
emitters** — for example exercising the PD1/PD2 fouling-versus-aging discriminator
(`np_pbm1064_dose_evaluate_ratio()`). Whether such a self-test is a "protocol" for gating purposes,
and therefore whether it needs bench mode or an exemption of its own, is undecided:
**`OI-BENCH-09`**.

### 9.4 Service and development

- **Service** (`docs/reference/service-network.md` partner tiers): the population bench mode is
  built for, together with FAI §6 functional test and hardware bring-up.
- **Development**: mostly out of scope. The repository ships a simulator (`simulator/`), and most
  firmware work does not require a real helmet emitting. Saying so keeps the credential population
  small, which is the only thing that keeps a credential scheme meaningful.

---

## 10. Annunciation, audit, and exit

### 10.1 Annunciation

CLAUDE.md §4.7 gives three device states — green power LED breathing at idle, amber in-use LED whose
*"pulse rate mirrors session frequency"*, and a red blink for fault — and firmware implements exactly
those three (`np_hub_control_main.c:55`, `NP_LED_IDLE` / `NP_LED_SESSION` / `NP_LED_FAULT`).

> **Bench mode gets a fourth, visually distinct state, and stealth mode does not suppress it. D-8.**

The stealth-mode precedent decides this. CLAUDE.md §4.7 makes safety faults override stealth, and the
reason that rule is right is that the annunciation protects **someone other than the person who set
the preference**. Bench mode has exactly that shape: it protects whoever picks the helmet up next.

The pattern must not be confusable with a session pulse, which mirrors session frequency across
0.5–100 Hz (CLAUDE.md §3 modality 8, §4.7) — so it must be structurally different (for example a
grouped blink with a long gap) rather than merely a different rate. The specific pattern is a human
factors question and belongs to `NP-HFE-002`: `OI-BENCH-05`.

The app shows a persistent, non-dismissible banner, **including when the app did not initiate bench
mode** — the failure to design for is a second phone, or a technician's session inherited by a user's
app.

### 10.2 The audit line — SHDR versus UHDR

This is the part of the design most likely to be got wrong, so it is stated as three rules with their
reasons.

**(a) A bench-mode entry is SHDR-eligible.** It is a device event with no wearer *by construction* —
the state asserts that no head is present. It falls under the locked *"safety interlock log → SHDR"*
boundary resolution (CLAUDE.md §5.1), and its consent subject is the **warranty owner**, which for a
serviced device is the correct party (CLAUDE.md §6.0).

**(b) A session run in bench mode still produces a UHDR session record.** CLAUDE.md §5.1 places
session timestamps, protocol parameters, closed-loop adaptation events and PBM dose in UHDR. The
runtime circumstance does not reclassify them, and it must not, because a classification that
*depends on* a runtime predicate is a bypass for the boundary itself: *"run it in bench mode and the
record becomes fleet telemetry"*. That is the failure shape CLAUDE.md Rev 35 already ruled on — a
redaction conditioned on a sensitive predicate leaks the predicate — in its inverted and worse form,
where the condition would *remove* protection rather than signal it. **D-9.**

**(c) A worn/not-worn time series is UHDR and is never journalled anywhere.** The head-presence
boolean is evaluated continuously; its history is a record of when a person put a device on and took
it off, which passes CLAUDE.md §5.1's defining test for UHDR — *does this record tell us something
about the person?* — without argument. It lives in RAM and is never written to either partition.
This is consistent with the existing boundary resolutions, which permit only the *derived* form of
every analogous signal: raw EEG impedance is UHDR and only the trend slope is SHDR; raw VNS impedance
is UHDR and only the contact-resistance trend is SHDR; per-electrode CVNS impedance is *"transferred
device-internally only, NEVER written to SHDR"*.

**And a fourth rule, taken from a defect this programme has already had.**
`ci/shdr/shdr_fleet_schema.sql` records, at `shdr_accel_records`, that an append-per-event table with
a **non-unique** `warranty_token` and an index on it reconstructs by aggregation the very quantities
its column list bans — *"a column-level control cannot express a row-set-level hazard"*
(`OI-EMMC2-11`). A `bench_mode_entries` table with one row per entry would do the same thing here:
`count(*)` per token plus the `session_index` values is a **service-event timeline** for that device,
which for a clinic-owned unit is a record of clinic operations. Nothing in the device-health question
needs that.

> **So bench-mode entry is recorded as an upserted per-device counter plus the most recent
> `session_index` — one row per device, not one row per entry.** The remedy `OI-EMMC2-11` says is
> unavailable for accelerometer data — collapsing to a single upserted row — *is* available here,
> because nothing downstream needs bench-mode sequencing. This is the lesson applied where it fits
> rather than merely cited.

### 10.3 Exit

| Trigger | Effect on bench mode | Effect on a running session |
|---|---|---|
| **Head presence re-acquired** | **Immediate exit** | **ABORT**, stimulation off | 
| USB-C disconnect, power cycle, reset | Exit (state is RAM-only — structural) | Ends with the power |
| Any Class C fault or watchdog cutoff | Exit | Already stopped by SW-01 |
| Credential window expiry (ticks or session budget) | Exit at the **next** session start | Running session completes, bounded by the credential's per-session cap |
| Operator command | Exit | Stops |

**The asymmetry between the two presence transitions is deliberate and is the most important rule
here.**

- **Losing** presence during an ordinary session **pauses** — debounced over `T_HP`, stimulation off,
  session `PAUSED` (a state the hub already carries, `np_safety_spi.c:128`), auto-resume if presence
  returns inside a window, abort after it. Pausing is the availability-friendly direction and it is
  the direct answer to `RISK-18`'s hazard shape, where a presence-detect false negative costs a user
  a session with functional hardware.
- **Gaining** presence during a bench session **aborts**. The alternative — continuing because bench
  mode is "more permissive" — means a person is being stimulated by a protocol authored for a bench,
  with the per-modality gates in their bench configuration. There is no reading of that which is
  safe, so the debounce runs in the *opposite* direction: a single confident presence reading is
  enough to abort, where several consecutive absences are required to pause.

**Why expiry does not kill a running session.** Expiry is an *authorisation* bound; head presence is a
*hazard* bound. Terminating an FAI functional test mid-measurement to enforce an authorisation
deadline destroys a measurement and prevents nothing. The credential therefore carries a per-session
duration cap so "runs to completion" cannot be unbounded, and the window is checked at session
*start*.

---

## 11. Decisions

| ID | Decision |
|---|---|
| **D-1** | **EEG electrode contact impedance is the primary head-presence sensor**, read by SW-02 through the ADS1299. No new sensor is added |
| **D-2** | **PD2 is not a pre-start gate.** Its dependence on the emitters being on is circular for a start decision, and it does not exist at all in the Core configuration. It corroborates in-session only |
| **D-3** | **PD2 is never a veto.** Contact-good with backscatter-low is the fouling signature the PD1/PD2 ratio already discriminates, not an absent head |
| **D-4** | **Bench mode suspends the gate's veto on session start; it never stops the gate being evaluated.** Suspending a decision, not blinding a sensor |
| **D-5** | **Entry is an Ed25519-signed, device-bound service credential, redeemed while a physical service assertion is held**, granting a window denominated in monotonic ticks and session count, held in RAM only. A build-time flag is rejected for any shipped device |
| **D-6** | **The visual modality is excluded from bench mode.** `NP_SAFETY_EN_VISUAL` is never enabled in bench mode; the ocular protections are themselves presence-conditioned and the photoparoxysmal halt is vacuous off-head |
| **D-7** | **Bench mode does not relax `SEAT#` or `check_placement()`.** Missing modules are a protocol-authoring question; partially-seated modules are a defect, and bench testing is where they are introduced |
| **D-8** | **Bench mode has a fourth LED state that stealth mode does not suppress**, on the same reasoning that makes safety faults override stealth |
| **D-9** | **A session run in bench mode is UHDR exactly as any other session is.** Only the *entry event* is SHDR, and it is an upserted per-device counter, not an append-per-event row |

---

## 12. Risk register

| ID | Sev | Hazard | Cause | Consequence | Control | Owner | Status |
|---|---|---|---|---|---|---|---|
| **RISK-BENCH-01** | **HIGH** | A user enters bench mode from published instructions and runs a protocol while wearing the helmet | Bypass mechanisms that are secrets-by-obscurity are defeated by description | The head-presence gate is void for the population least able to judge the risk | D-5 credential is unforgeable from its own description; D-4 keeps the sensor live; exit-on-reacquisition aborts (§10.3); D-8 annunciation | FW + Security | **MITIGATED — unverified** |
| **RISK-BENCH-02** | **HIGH** | A future change routes a bench predicate into SW-01 | A `session_status` bit is cheap to add and the link already carries three | SW-01 ceases to be an independent backstop, voiding `NP-SW-001` §3.2's Class B basis for SW-02 | §7.2 structural absence + the CI check of `OI-BENCH-04`, falsified in both directions per `NP-CONV-001` §8 | FW + Quality | **OPEN — check not written** |
| **RISK-BENCH-03** | MEDIUM | Head-presence false negative blocks a legitimate session | `RISK-18`'s shape on a new sensor; `Z_HP` is not derivable today | User cannot start a session with functional hardware | Two thresholds not one (§4.3); `K` of `N`; debounce; pause-not-abort | FW | **OPEN — thresholds not derivable (`OI-BENCH-01`)** |
| **RISK-BENCH-04** | MEDIUM | Head-presence false positive on a bench (wet fixture, phantom, operator's hand) | A conductive load is not a head, and impedance cannot tell them apart | Benign for the gate; **hazardous for exit** — a false positive aborts a legitimate bench run | Accepted in the safe direction. FAI functional tests must use a phantom whose impedance is characterised, or accept the abort | FW + Quality | **ACCEPTED — direction is safe** |
| **RISK-BENCH-05** | MEDIUM | Ocular exposure to a bench operator from the lens | IR eye-open and the photoparoxysmal halt are presence-conditioned and degrade off-head | Retinal exposure with two of three layers weakened | **D-6** — the visual modality is not available in bench mode at all | FW + Safety | **MITIGATED by exclusion** |
| **RISK-BENCH-06** | MEDIUM | A device's service-event timeline is reconstructable from fleet telemetry | Append-per-event rows on a non-unique `warranty_token` — the `OI-EMMC2-11` shape | More is exposed than the device-health question needs; for a clinic device, a record of clinic operations | **D-9** — one upserted row per device, not one per entry | FW + Privacy | **MITIGATED by schema shape** |
| **RISK-BENCH-07** | MEDIUM | A worn/not-worn series reaches SHDR | The gate produces a continuous boolean, and continuous booleans invite logging | A behavioural record of the wearer in NeurOne-owned telemetry — a UHDR/SHDR boundary breach | §10.2(c) — RAM only, never a column in either partition; no derived form is proposed | FW + Privacy | **OPEN — no CI column guard yet** |
| **RISK-BENCH-08** | MEDIUM | Bench mode becomes an unsigned-protocol path | The most convenient way to run an ad-hoc bench protocol is to skip signing it | Loses the cryptographic session gate for the whole bench population | §7.1 item 7; structurally, the signature gate is Class C and unreachable from SW-02 (§7.2) | FW | **MITIGATED — structural** |
| **RISK-BENCH-09** | LOW | An engineering image with a compile-time bypass is shipped | `firmware_history` records a version, not a build configuration | An un-annunciated, unrevocable bypass in the field | §6 — build-time flags rejected for shipped devices; engineering images must still annunciate | FW + Quality | **OPEN — no build-provenance field exists** |

---

## 13. Open items

| ID | Description | Owner | Blocking |
|---|---|---|---|
| **OI-BENCH-01** | **`Z_HP`, `K` and the debounce depth `T_HP` are not derivable from anything in this repository.** `NP_IMPEDANCE_MAX_OHM` is a tES pad limit read through an uncalibrated reference leg (`OI-SWCI-34`); `NP-FAI-001` FAI-IPX-01 accepts an **off-head** headset at *"< 10 kΩ"*, which falsifies reusing that figure; `eeg_impedance_trend` stores only a slope, by design. Needs a measured off-head/on-head distribution across head sizes, hair types and tip age | FW + Clinical | **The gate shipping** |
| **OI-BENCH-02** | Debounce for the head-presence gate must be decided **jointly with `OI-HEXTILE-08`**, which holds the `SEAT#` debounce requirement re-scoped from `ZONE_ID`. Two presence-like signals debounced by different rules on the same session-start path is how `RISK-18` recurs | FW | The gate shipping |
| **OI-BENCH-03** | **Where does the hub's service-authority public key live, and how is it provisioned?** The existing Ed25519 root is in OTP on the STM32G071 (`np_hal_otp_read_pubkey`), which is the wrong side of the boundary — §5(iii) forbids relaying the bypass decision to SW-01. No hub-side key store is specified anywhere | FW + Security | **Bench mode shipping** |
| **OI-BENCH-04** | **Write and falsify the CI check** that no bench-mode identifier appears under `firmware/safety_mcu/` and that `session_status` gains no bit. Per `NP-CONV-001` §8 the probe must be falsified in both directions before it is trusted | FW + CI | `RISK-BENCH-02` |
| **OI-BENCH-05** | Bench-mode LED pattern — must be structurally distinct from a session pulse (which mirrors session frequency across 0.5–100 Hz), not merely a different rate, and must survive stealth mode. Belongs with `NP-HFE-002` | HFE | Annunciation |
| **OI-BENCH-06** | **The module-swap stop's owning tier is unstated.** `SEAT#` has no firmware consumer, the safety MCU has no input for it, and `NP-HW-HUB-001` aggregates it at the cluster controller (Class B). Whether the stop should be Class C is a real question this document does not answer. Contradicts the framing under which bench mode was scoped | Safety + EE | Module-swap stop shipping |
| **OI-BENCH-07** | **`NP-CFG-UI-001` does not exist.** It is cited twice by `NP-NPPS-REF-001` as the owner of operator socket selection, and `clinical-09` is unrunnable without it. It must also carry the selection-persists-across-doff/don requirement from §9.2 | App + Clinical | `clinical-09` operability (not this study) |
| **OI-BENCH-08** | `np_mod_visual_hal_hall_lifted()` is documented as *"goggles off head"* at `OI-VIS-04`, used as *"magnet present → goggles seated"* at `np_mod_visual.c:63`, and commented *"goggles must be on head"* at line 106. Three propositions, one predicate. Decide which it measures and rename it | FW | Ocular interlock clarity |
| **OI-BENCH-09** | Does an emitter-driving **self-test** — e.g. exercising the PD1/PD2 fouling-vs-aging discriminator — count as a protocol for gating purposes? If it does, every such self-test needs bench mode; if it does not, it needs its own bounded exemption | FW | Predictive maintenance on the bench |
| **OI-BENCH-10** | **Configuration assumption, stated rather than assumed:** every shipped configuration includes EEG electrodes (CLAUDE.md §2.1 — Core is 4-ch EEG, every other row includes 8-ch or 21-ch). If a PBM-only build is ever configured it has **no pre-start presence sensor at all**, and D-2 would have to be reopened | Systems | Any PBM-only configuration |
| **OI-BENCH-11** | No field anywhere distinguishes an engineering firmware image from a shipped one — `firmware_history` records a version string only. `RISK-BENCH-09` has no detection, only a prohibition | FW + Quality | Fleet build provenance |

---

## 14. Deliverable summary

**The gate.** EEG electrode contact impedance, read by SW-02 through the ADS1299, is the primary
sensor; PD2 corroborates in-session and never vetoes; the tES and ocular gates are unchanged and
upstream. `HEAD_PRESENT` is `K` of `N` enabled channels at or below a **head-present threshold
distinct from the signal-quality threshold**, with a freshness requirement, debounced, **fail-closed**
on every error path. The thresholds are not set here and are not derivable from anything on file.

**The bypass.** An Ed25519-signed, device-bound service credential redeemed while a physical service
assertion is held, granting a window denominated in monotonic ticks and session count, held in RAM
only, annunciated on a dedicated LED state that stealth mode cannot suppress, and exiting immediately
on head-presence reacquisition, disconnect, or any Class C fault. It suspends the gate's veto on
session start; it never disables the gate.

**Never bypassable.** The 62 °C junction thermal cutoff; the 40 µC/cm² charge-density limit and the
`OI-CHARGE-03` geometry gate; tES contact confirmation — which means **tES does not run on a bare
bench, deliberately**; the IEC 62471 MPE ceiling, IR eye-open and Hall cutoff, together with the
**exclusion of the visual modality from bench mode entirely**; the photoparoxysmal halt; the
session-descriptor signature; the heartbeat watchdog; the cervical VNS cardiac interlock; the fault
latch; `SEAT#`; and `check_placement()`.

**IEC 62304 class.** **SW-02 Class B**, for both the gate and the bypass, because (i) the Class C tier
physically cannot read either candidate sensor, (ii) head presence is the sole barrier for no hazard
in the tree, and (iii) the bypass never crosses the SPI link, so SW-01's behaviour is bit-identical in
and out of bench mode. That last property is what makes the never-bypassable list structural rather
than asserted: the restriction is an absence of a wire, and it is mechanically checkable.

**Open items.** Eleven, of which three are blocking: `OI-BENCH-01` (the gate's thresholds are not
derivable), `OI-BENCH-03` (no hub-side service-authority key store exists), and `OI-BENCH-04` (the CI
check that keeps the bypass out of the Class C tier is specified but not written).

---

## 15. Revision history

| Rev | Date | Author | Description |
|---|---|---|---|
| 1 | 2026-08-26 | NeurOne Firmware + Safety Engineering | Initial release. Specifies the head-presence gate (D-1…D-4) and its bench/service bypass (D-5…D-9) against the principal decision of 2026-08-26. **Corrects three claims in its own scoping brief:** the safety MCU can read neither candidate head-presence sensor (its impedance AFE covers only the four tES channels; the ADS1299 and the PD2 TIA are both hub-side), which is what decides the Class B classification; `SEAT#` has no firmware consumer and no safety-MCU input, so the module-swap stop is specified today as Class B and not as an enable-line drop (`OI-BENCH-06`); and neither `clinical-09` nor `07-vascular-baseline` is a bench workflow — the first needs the absent `NP-CFG-UI-001` (`OI-BENCH-07`), the second is a therapeutic session. **Falsifies the obvious threshold:** `NP-FAI-001` FAI-IPX-01 accepts an off-head headset at *"All EEG channels < 10 kΩ"*, so 10 kΩ cannot be reused as `Z_HP`; the gate's thresholds are not derivable from this repository (`OI-BENCH-01`, blocking). Applies `OI-EMMC2-11`'s row-set-aggregation lesson to the bench-mode audit record, and `NP-FW-EMMC-002` §H's record-denominated-window precedent to the credential's expiry. No code changed. |
