# NP-ENV-OPRANGE-001 — Per-Modality Operating-Range Working Spec

**Program:** NeurOne systems / thermal
**Status:** DRAFT working spec (OI-ENV-04). All bounds are **first-pass provisional** — this doc exists
to give the team concrete numbers to refine, not blanks. Legend: **†** = high-temp bound is a THERM-1a
CFD output (pending run); **‡** = from component datasheet (pending part/grade selection); **§** =
first-principles estimate in this spec.
**Parent:** NP-ENV-001 (§3 framework, the intersection rule, humidity = survival-only).
**Sources:** CLAUDE.md §3 (modalities), §4.1/§4.2 (electronics, interlocks); NP-THERM-CFD-001 (THERM-1a
cases C3/C4 → thermal high bounds); NP-REQ-FANHEALTH-001 (SR-FAN Class C gate); NP-ENV-001 §4 (hybrid gating).
**Date:** 2026-07-21

---

## 1. What "operating range" means here

- **Envelope variable = ambient temperature** (°C). Humidity is **survival-only** (NP-ENV-001 §5); the
  humidity-sensitive operating items are covered by electrode-impedance monitoring + the power-up
  anti-condensation warm-up hold, not an ambient RH gate.
- **Enforcement follows the hybrid gate (NP-ENV-001 §4):** **Safety**-class bounds → **HARD** (on-device,
  non-dismissible, Class C); **Efficacy/Comfort**-class bounds → **SOFT** (range shown + dismissible warning).
- **Thermal modalities are a derate ramp, not one number:** full-dose ≤ T_f; linear duty derate T_f→T_max;
  blocked > T_max (where even minimum useful dose cannot hold scalp ≤ 42 °C). All operating assumes the
  **fan running** — a fan fault drops to the SR-FAN natural-convection ceiling regardless of ambient.
- All operating bounds are **⊂ the −20/+60 °C survival envelope** (NP-ENV-001 §2).

## 2. Per-modality operating ranges (provisional)

| Modality | Low bound (°C) | High bound (°C) | Limiting mechanism | Class | Enforce | Sensed by |
|----------|---------------|-----------------|--------------------|-------|---------|-----------|
| **PBM 660/808 (T1-A)** | −10 §(condensation/warm-up) | full ≤ **+35** · **block > +35** ‖ *(no derate band)* | scalp headroom to 42 °C shrinks with ambient | **Safety** | **HARD** | ambient NTC + THERM-1a envelope |
| **PBM 1064 smart (T1-C)** | **0 ‡**(ATtiny402/FET grade) | same as T1-A † | above + on-module driver IC range | Safety+comp | **HARD** | ambient NTC; module detection (`np_module_map`) |
| **PBM 1170 laser+TEC (T2-D)** | −10 § | full ≤ **+30†** · derate +30→**+35†** · block > +35† | **tightest** — TEC can't hold laser setpoint at high ambient; higher power → less scalp headroom | Safety+eff | **HARD** | ambient NTC; TEC setpoint error |
| **EEG hydrogel (ADS1299)** | +5 §(gel conductivity) | +40 §(gel dry-out) | hydrogel state, not electronics (ADS −40/+85‡) | Efficacy | **SOFT** | **electrode impedance** (existing) |
| **BES/tACS, tDCS** | −10 § | +45 § | delivery bounded by charge-density + impedance interlocks, not ambient | Safety*(interlocked) | interlock + SOFT | charge-density MCU, impedance |
| **VNS/HRV (PPG clip)** | 0 §(peripheral perfusion) | +45 § | cold → weak PPG; electronics wide | Efficacy | SOFT | PPG signal quality |
| **Neural audio** | −10 § | +50 § | none limiting | Comfort | none | — |
| **Visual stim (goggle µLED)** | −10 § | +45 § | safety is IEC 62471 **MPE** (temp-independent), not ambient | Safety(optical)/eff | MPE hard; ambient SOFT | IR proximity/Hall; MPE limit |
| **EC lens** | **+5 §**(cold switching stalls) | +50 § | bistable EC ion mobility falls cold; failsafe = clears to 75 % (safe) | Efficacy/fn | SOFT | EC transition-time monitor |
| **Active EMF cancel (fluxgate)** | −10 § | +50 § | fluxgate temp drift degrades cancellation; passive 5-layer shield always present | Efficacy(shield) | SOFT | fluxgate/SHDR attenuation |

\* tES safety is delivered by the charge-density/impedance interlocks (CLAUDE.md §4.2), not an ambient gate,
so its ambient envelope is wide and soft.

**‖ PBM block threshold = +35 °C — DECIDED (principal). Was +43† until 2026-08-30, then +38, now +35.**

**Why it left +43 (2026-08-30).** **(i) Use case, the operative reason:** there is no non-emergency
reason to run this device in a room above +35 °C, so an envelope reaching +43 bought availability
nobody wants. **(ii) It did not match the physics:** `NP-THERM-COOL-001` §7 fits
`NP-THERM-CFD-R1-001` §5.1's two published ambients and finds T1-std holds the scalp-facing face
≤ 42 °C at full dose only to **37.9 °C**, so +43 sat ~5 °C beyond that and left the derate ramp
carrying a band it was never validated across. **(iii) Supporting:** near the top of the old band the
derated duty approached the `NP-PWR-BUDGET-001` §3.4 efficacy floor (0.02–0.3 W/cm², 10–120 J/cm²), so
the device could complete a session the user believed was a treatment while delivering a sub-threshold
dose.

**Why it then went to +35 (2026-08-31, principal): alignment with T2.** T2-D already blocks at +35 (its
TEC cannot hold laser setpoint above it). A customer upgrading T1 → T2 should not meet a *tighter*
usage limit on the more expensive tier, so the two block thresholds are now the same number.

**The consequence, stated plainly: T1-A has no derate band.** Full dose is held to ≤ +35 and blocking
begins above it, so the reduced-duty region between them has zero width. This is a simplification
rather than a loss — reason (iii) above records that the derate band was exactly where a "completed"
sub-threshold session could occur — and **`OI-OPR-01`'s T1-A derate curve is moot, because there is no
curve left to specify.** T2-D keeps its own +30 → +35 derate band, so the *shapes* still differ even
though the block thresholds now match; only the block was asked to align.

**This threshold is decided, not provisional — the `‖` rows do not carry the `†` "pending THERM-1a"
caveat.** New: a hard block at a single temperature needs **hysteresis** so an ambient NTC resting on
+35 cannot chatter — `NP-THERM-COOL-001` `OI-THCOOL-16`.

## 3. Shared-electronics base envelope (inherited by every protocol)

Set by the hub electronics, **⊂** which every modality operates: STM32G071 −40/+85‡, i.MX RT1062
industrial‡, eMMC −40/+85‡, **22 F supercap −40/+65‡ (high temp accelerates the 5-yr aging budget)**.
Working base envelope **−10 to +45 °C §** (conservative; parts capable wider, supercap +65 is the ceiling,
self-heating + fan set the practical top). Any protocol's envelope is the base ∩ its modality envelopes (§4).

## 4. Module roll-up (module = ∩ its modalities ∩ construction limit)

| Module | Operating envelope (provisional) | Set by |
|--------|----------------------------------|--------|
| **T1-A** base PBM | −10 → **+35 ‖** (no derate band) | PBM thermal |
| **T1-B** EEG/electrode + reduced PBM + tES | +5 → **+35 ‖** (no derate band) | PBM thermal (high, HARD) ∩ gel +5 low (SOFT) |
| **T1-C** 1064 smart PBM | **0‡** → **+35 ‖** | PBM thermal ∩ **on-module driver IC low bound** (construction) |
| **T2-D** 1170 laser | −10 → **+35†** | TEC + laser (tightest high bound) |
| Intranasal PBM probe | −10 → **+35 ‖** | PBM thermal (lower power; likely wider) |
| VNS/HRV clip | 0 → +45 | PPG perfusion |
| Audio / goggles | −10 → +45–50 | non-limiting / MPE |

**Construction matters independently of modality (the design point):** T1-C's low bound is set by its
on-module ATtiny/FETs, not by PBM; T2-D's high bound by its TEC, not by "PBM" generically.

## 5. Protocol roll-up examples (envelope = ∩ activated modules)

| Protocol | Modules | Operating envelope | Binding limit |
|----------|---------|--------------------|---------------|
| Gamma clarity (40 Hz PBM) | T1-A | −10 → **+35 ‖** | PBM scalp thermal (HARD) |
| **EEG neurofeedback only** | T1-B (EEG, no PBM active) | **+5 → +45** | gel (SOFT) — *runs in a hot room a PBM protocol won't* |
| tDCS priming | T1-B (tES) | −10 → +45 | interlocks (not ambient) |
| Deep-PBM cognition (1064) | T1-A + T1-C | 0‡ → **+35 ‖** | 1064 module low bound + PBM thermal |
| **Full multi-modal** (PBM+1170+EEG+tES+audio) | T1-A/B/C + T2-D | **+5 → +35†** | **1170 TEC (high) + gel (low)** — tightest of all |

This is the user's principle made concrete: **the protocol's envelope is set by its most-limiting
included modality** — an EEG-only session is usable across a far wider ambient band than a 1170 nm laser session.

## 6. How the numbers firm up

- **Thermal high bounds (†):** outputs of **THERM-1a** — case **C4** (fan-nominal peak, ambient sweep) gives
  the full-dose ceiling; **C3** (safe-duty ceiling) gives the derate curve; per config (T1-std/peak, T2-peak).
- **Component bounds (‡):** from datasheets once parts/grades lock — the **ATtiny402/FET grade** (commercial
  0 °C vs industrial −40 °C) directly sets T1-C's low bound and is the one decision that visibly moves a
  module envelope. The **TEC ΔT capacity** sets T2-D's high bound.
- **Gel/EC/PPG bounds (§):** from consumable + EC + PPG characterization; gel is *enforced by impedance*, so
  its ambient number is advisory.

## 7. Open items

| ID | Description | Owner |
|----|-------------|-------|
| OI-OPR-01 | Replace † bounds with THERM-1a C3/C4 outputs (full-dose ceiling + derate curve per config) | Thermal |
| OI-OPR-02 | ATtiny402/FET grade decision (commercial vs industrial) → fixes T1-C low bound | EE |
| OI-OPR-03 | TEC ΔT-capacity spec → fixes T2-D high bound; confirm laser setpoint-error sensing feeds the gate | EE + Thermal |
| OI-OPR-04 | Confirm gel operating band + that impedance monitoring is the enforcement (no ambient gel gate) | Consumables + FW |
| OI-OPR-05 | **Designed → NP-FW-POE-001** (POE block in the signed descriptor; MCU-table-authoritative min() enforcement so it can't widen safety). Residual: OI-POE-01…05 there | FW |

## 8. Cross-references

NP-ENV-001 (framework, gating, humidity) · NP-THERM-CFD-001 (THERM-1a C3/C4) · NP-REQ-FANHEALTH-001
(fan-fault ceiling overrides ambient) · CLAUDE.md §3/§4.1/§4.2.
