# NP-FW-POE-001 — Protocol Operating-Envelope Encoding

**Program:** NeurOne firmware / thermal-safety
**Status:** DRAFT design (OI-OPR-05). Defines how a protocol's operating envelope + derate curve is
encoded in the signed session descriptor so the Class C Safety MCU (SW-01) and the app (SW-03) read the
**same** numbers — while guaranteeing a bad/absent envelope cannot widen safety.
**Sources:** NP-ENV-OPRANGE-001 (the envelope numbers, and §1's derate semantics), NP-THERM-COOL-001
§7.4 / D-4 (the efficacy-floor clamp this revision encodes), NP-THERM-COOL-001 §7.5 (the hysteresis
sizing §6.1 encodes), NP-PWR-BUDGET-001 §3.4 (the floor itself), NP-ENV-001 §4 (hybrid gate),
NP-HEX-ZM-001 §4/§4b
(NP Hub Protocol v2, required module map, `(socket:element)` addressing), NP-FMEA-001 §3.4/§3.7 (SW01-M04
thermal, SW01-M07 signature verify), NP-REQ-FANHEALTH-001 (SR-FAN ceiling), NP-SW-001 §6.2.
**IEC 62304 Class:** SW-01 enforcement = **C**; SW-02 derate + SW-03 compute/display = **B**. The
efficacy-floor refusal added at `poe_version` 0x02 is **B**, deliberately — see §1.1.
**Date:** 2026-09-03 (§1.1 + `poe_version` 0x02 — the efficacy-floor clamp, closing `OI-THCOOL-17`;
§6.1 added 2026-09-02 — hysteresis on the hard edges, closing `OI-THCOOL-16`; was 2026-07-21)

---

## 1. The load-bearing safety property (state it first)

> **Safety does not depend on the descriptor's envelope being present or correct.** The Safety MCU holds
> its **own** authoritative per-modality envelope table (Class C config, sourced from THERM-1a) and knows
> which heat-generating modalities are active (it owns their enable GPIO and reads their NTCs). It computes
> the HARD duty clamp itself. The descriptor's encoded envelope is used to (a) drive the app's SOFT
> range/warnings, (b) let the session runner derate *smoothly ahead* of the MCU clamp, and (c) provide a
> signed, audited record — **but it can only ever make the session more restrictive, never less.**

So the encoding is for **consistency and UX**, not for delegating safety. This mirrors the existing rule
that a signed descriptor may request *less* than the MCU's hardware limits (40 µC/cm², 42 °C/62 °C) but
never more.

## 1.1 The second bound, and why it is Class B rather than Class C

`NP-THERM-COOL-001` D-4 (2026-09-02) added an **efficacy-floor clamp** beneath the thermal block: a PBM
session is **refused** once the derated duty would put delivered dose below `NP-PWR-BUDGET-001` §3.4's
**10 J/cm²**, rather than being derated further toward zero. Because the floor binds on delivered dose,
the refusal temperature is **per-protocol** — 33.8 / 34.2 / 34.6 °C for a 40 / 60 / 120 J/cm² protocol —
which is why the descriptor now carries the protocol's dose (§3).

> **The Safety MCU does not compute this bound, and must not.** Three reasons, in order of weight:
>
> 1. **It is not a safety bound.** A sub-threshold PBM session is *ineffective*, not hazardous. By
>    `NP-ENV-001` §4's own class rule that makes it an **Efficacy**-class bound, and Efficacy-class
>    bounds are not enforced in SW-01.
> 2. **It would enlarge the Class C boundary for nothing.** The floor is a clinical input that will
>    move — `NP-ENV-OPRANGE-001` `OI-OPR-07` already questions whether it is one number or a
>    per-modality table. Siting it in SW-01 makes every revision of a literature figure a change to
>    certified Class C software.
> 3. **§1's load-bearing property is preserved exactly.** The MCU's authoritative table still
>    terminates at 0 % above +35 °C. The floor clamp can only ever refuse **earlier** than that, so a
>    descriptor that overstates `dose_full_dJ` degrades **its own efficacy guarantee and nothing
>    else** — it cannot widen a thermal bound, because +35 was never its to move. (`OI-POE-07`
>    specifies the integrity check that this argument does *not* excuse.)
>
> **Efficacy-class does not make it dismissible.** `NP-ENV-001` §4 pairs SOFT with a dismissible
> warning, and that default is wrong for this one bound: the user cannot distinguish a null session
> from a real one, so "run it anyway" would be a choice made without the information that would inform
> it. It is a **non-dismissible Class B refusal**, enforced in **SW-02** (session runner) with **SW-03**
> presenting it. SW-02 runs on the main processor, so **Mode 3 autonomy is covered with no app
> present** (§8).

## 2. Source of truth and the two consumers

```
                 per-modality/module OPERATING-ENVELOPE TABLE   (versioned; from THERM-1a + datasheets)
                 ┌───────────────────────────────┴───────────────────────────────┐
   provisioned into SW-01 as Class C config                       bundled with / read by SW-03 (app)
   (authoritative for HARD bounds)                                (computes identical POE for display)
                 │                                                                 │
                 │                          app computes protocol envelope = ∩ activated modules
                 │                          → writes the POE block into the descriptor, Ed25519-signed
                 ▼                                                                 ▼
   SW-01 enforces min( table_clamp(ambient,active,ver),          SW-03 shows range + soft warnings;
        POE_clamp, SR-FAN ceiling )   <- Class C, thermal        SW-02 session runner derates duty via POE
                                                                 and REFUSES below duty_floor_pct
                                                                 <- Class B, efficacy (§1.1)
```

Because both the MCU table and the app compute from the **same versioned table**, the numbers match by
construction; version skew is handled fail-safe (§7).

## 3. The POE block (binary, in the signed descriptor)

Appended as a new length-prefixed block in the NP Hub Protocol v2 descriptor (same convention as the v2
target block, NP-HEX-ZM-001 §4b); the whole descriptor is Ed25519-signed, so truncation/tamper is caught by
the length field + signature (FMEA-M07-04 pattern). Little-endian.

| Field | Type | Notes |
|-------|------|-------|
| `block_tag` | u8 | `NP_DESC_BLOCK_POE` (e.g. 0x03) |
| `block_len` | u16 | payload byte count |
| `poe_version` | u8 | envelope-schema version = **0x02** (0x01 had no efficacy-floor fields) |
| `table_version` | u16 | operating-envelope **table** version the app computed against |
| `flags` | u8 | bit0 LOW_HARD · bit1 HIGH_HARD · bit2 FAN_REQUIRED · bit3 HUMIDITY_NA(=1) · bit4 HAS_SOFT_BOUNDS · **bit5 HAS_EFFICACY_FLOOR** |
| `binding_module` | u8 | module/modality id that set the binding bound (display: "limited by 1170 nm laser") |
| `t_low_block_dC` | i16 | cold hard block, °C ×10 (e.g. +50 = +5.0 °C) |
| `t_low_soft_dC` | i16 | cold soft-warn threshold, °C ×10 (efficacy) |
| **`dose_full_dJ`** | u16 | **NEW 0x02.** The protocol's nominal full-dose PBM delivery, J/cm² ×10, at 100 % duty over its own declared length. **0 = not dose-bearing** (no PBM module activated — an EEG-only or tES protocol), which clears bit5 and disables the floor clamp entirely |
| **`duty_floor_pct`** | u8 | **NEW 0x02.** The duty at which `dose_full_dJ` reaches the efficacy floor: `ceil(100 × floor_dJ / dose_full_dJ)`, clamped to 1…100; **0 when `dose_full_dJ` = 0**. Carried rather than left implicit so the descriptor is self-contained and auditable when the floor itself is revised (`OI-OPR-07`) — but **SW-02 recomputes it and takes the larger of the two**, so a descriptor cannot loosen its own floor |
| `n_points` | u8 | high-side derate breakpoints, ≥2 |
| `points[n]` | {i16 temp_dC, u8 duty_pct} | monotonic ↑ temp, monotonic ↓ duty; [0]=full-dose knee (duty 100), [n-1]=block (duty 0) |

**Derate semantics:** `max_duty(ambient)` = piecewise-linear over `points` (100 % below `points[0].temp`,
0 % above `points[n-1].temp` = hard block); `ambient < t_low_block_dC` → block. `t_low_soft_dC` and any
SOFT-flagged bound are **app-only** (MCU ignores them). **Session length is FIXED** — duty scales and
delivered dose scales with it; the session never extends to hold dose (`NP-ENV-OPRANGE-001` §1,
`NP-THERM-COOL-001` D-4). The `points` curve is **the same for every helmet module and the intranasal
probe**; only the point at which a given protocol stops descending it is protocol-specific.
**Both hard edges — `points[n-1].temp` and `t_low_block_dC` — carry the §6.1
hysteresis band, and so does the §3 efficacy-floor edge; the band itself is not encoded here** (it is
Class C table config, for the reason §6.1 gives).

**Efficacy-floor semantics (0x02, Class B, SW-02):** while `max_duty(ambient) ≥ duty_floor_pct` the
session runs at that duty. Once `max_duty(ambient) < duty_floor_pct` the session is **refused** — not
derated further — with reason `NP_POE_BELOW_EFFICACY_FLOOR`, distinct from the thermal
`NP_POE_OUT_OF_RANGE`. The two reasons must stay distinct because they need different words in front of
the user and different `OI-THCOOL-16` hysteresis. **The floor never widens anything:** it can only
refuse at or below the temperature at which `points` already reaches 0 %. With bit5 clear the clamp is
absent and behaviour is exactly `poe_version` 0x01.

**Worked example — full multi-modal protocol (binding = 1170 TEC), 60 J/cm² over 20 min:**
`table_version=7`, `flags=LOW_HARD|HIGH_HARD|FAN_REQUIRED|HUMIDITY_NA|HAS_EFFICACY_FLOOR`,
`binding_module=T2-D`, `t_low_block=+50 (+5.0 °C)`, `dose_full_dJ=600`, `duty_floor_pct=17`,
`points=[(+300,100),(+350,0)]` → ≤ +30 °C full dose, +30→+35 linear derate, **refused above +34.2 °C on
the efficacy floor**, > +35 blocked on the thermal table, < +5 blocked. +3 bytes over the 0x01 layout.

**And the same descriptor for an EEG-only protocol:** `dose_full_dJ=0`, `duty_floor_pct=0`, bit5 clear —
no PBM dose to fall below a floor, so the clamp does not apply and `NP-ENV-OPRANGE-001` §5's +5 → +45
envelope is unaffected. **This is the field that keeps the clamp confined to PBM.**

## 4. Enforcement flow

1. **App (SW-03, Class B):** compute protocol envelope = ∩ of activated modules' envelopes from the table;
   emit the POE block; Ed25519-sign the descriptor.
2. **SW-01 verify (M07):** signature + length check (existing). Reject unsigned/truncated.
3. **SW-01 admission (thermal gate, extends M04):** read ambient (§6); compute
   `clamp = min( table_clamp(ambient, active_modalities, MCU_table_version), POE_clamp(ambient), SR-FAN ceiling )`.
   If `clamp == 0` (blocked) → deny enable, reason `NP_POE_OUT_OF_RANGE`, and **set the §6.1 hysteresis
   latch**; while that latch is set the admission test is `ambient ≤ T_block_eff − Δ`, not
   `ambient < T_block_eff`.
4. **SW-02 efficacy-floor admission (Class B, new at 0x02):** recompute `duty_floor_pct` from
   `dose_full_dJ` and take `max(recomputed, descriptor)` — a **margin composes by `max()`**, per §5's rule, which is why a
   descriptor cannot loosen its own floor. If `POE_clamp(ambient) < duty_floor` → refuse
   to start, reason `NP_POE_BELOW_EFFICACY_FLOOR`, **non-dismissible** (§1.1). Skipped entirely when
   bit5 is clear. A refusal here **sets the §6.1 latch on the floor edge** exactly as a thermal denial
   does on the block edge; §6.1's anchor is `T_block_eff`, which §3's floor makes per-protocol.
   **This runs after SW-01 admission, not before** — a thermally-blocked session must
   report the thermal reason, since it is the one the user can act on by cooling the room.
5. **SW-02 session runner (Class B):** scale commanded duty by `POE_clamp(ambient)` so the device derates
   smoothly; the MCU clamp is the backstop, rarely reached. **Session length is unchanged by the derate**
   — the runner must not extend a session to recover dose (`NP-THERM-COOL-001` §7.4.2: extension puts a
   20-minute protocol past `NP-PWRSRC-001` §5.5's 40 CEM43 line at 34.5 °C).
6. **SW-01 real-time backstop (existing):** junction NTC 62 °C throttle / 65 °C cutoff + SR-FAN face-temp
   protection run continuously — the reactive guarantee beneath the predictive POE admission/derate.

**Layering:** POE = *predictive, ambient-based admission + derate* (coarse, evaluated at session start
baseline + coarse re-checks); junction NTC + SR-FAN = *reactive, real-time* thermal guarantee. Same MCU
owns both; the POE just keeps the reactive layer from having to fire.

## 5. The min() rule, spelled out (why nothing can widen safety)

`effective_duty = min( TABLE_clamp, POE_clamp, SR-FAN_ceiling, HYST_clamp )`, evaluated by the Safety MCU:
- **TABLE_clamp** — authoritative, from the MCU's own table × active modalities × sensed ambient.
- **POE_clamp** — from the descriptor; can only pull the result *down*.
- **SR-FAN_ceiling** — fan-fault natural-convection ceiling (NP-REQ-FANHEALTH-001), composes independently.
- POE absent → term dropped, TABLE_clamp governs (defense in depth: safety intact without a POE).
- **HYST_clamp** — the §6.1 hard-edge hysteresis, `{0 %, 100 %}`. It can only pull the result down, so it
  composes here without changing anything else about the rule.
- POE claims a *wider* range than TABLE at any ambient → TABLE wins by the min(); a `NP_POE_WIDER_THAN_TABLE`
  SHDR flag is raised and the app is told to refresh.
- **The `min()` is for clamps, not for margins.** Δ and `t_dwell` are *margins*: larger is stricter, so
  they compose by `max()` and are kept out of the descriptor entirely (§6.1). Putting a margin through
  this `min()` would invert its guarantee.

**The efficacy floor is deliberately NOT a term in this min(), and that is the point (§1.1).** It is a
Class B refusal sitting *above* the Class C computation, gating whether a session starts at all rather
than how much duty it gets. Three properties follow, and together they are why the floor could be added
without touching SW-01: it can only make the effective envelope **narrower** than `effective_duty`
already allows; **removing it entirely restores exactly the 0x01 behaviour**, so it is defence in depth
in the same sense an absent POE is; and **a descriptor lying about `dose_full_dJ` cannot reach any
thermal bound** — the worst it achieves is failing to refuse a session it should have refused, which
costs the user an ineffective session and nobody any safety margin. `OI-POE-07` still specifies the
integrity check, because "cannot cause harm" is not "need not be correct".

## 6. Ambient sensing

- Source per OI-ENV-05: NTC read at **session start before self-heating**, and/or a dedicated ambient NTC.
- The POE admission/derate uses the **start-of-session ambient baseline** + coarse periodic re-reads
  (self-heating contaminates a live ambient read; the *dynamic* thermal safety is the junction NTC + SR-FAN).
- **No ambient reading → fail-safe:** assume worst-case ambient (survival-high) → most restrictive clamp,
  exactly like the open-NTC rule (FMEA-M04-02).

### 6.1 Hysteresis on the hard edges (normative; `OI-THCOOL-16`)

The high block, the low block and — since `OI-THCOOL-17` closed, the efficacy-floor clamp's — the
floor-clamp edge are **discrete** transitions on a noisy, drifting input. Each carries a band. The derate ramp does
not: it is continuous, and ambient noise moves duty by a few percent, which is not a transition.

**Sizing: Δ = 1.0 °C.** Derived in `NP-THERM-COOL-001` §7.5.1 from three bounds that meet at one number
— `adc_to_celsius()` returns whole degrees so 1 °C is the smallest band the sense path can express; a
room's own thermostat differential is 0.5–1.0 °C so anything finer sits inside the room's oscillation;
and the band is charged to the user as a 20–60 min wait, which caps it. **The junction interlock's 7 °C
band (`NP_NTC_CUTOFF_DEG_C` 62 / `NP_NTC_REARM_DEG_C` 55) is not the precedent to copy** — that is a
reactive fault latch where over-cutting is free, this is an admission gate where the band is a lock-out.

**The rule.** Let `T_block_eff = min(TABLE_block, POE_block)` for the protocol being admitted — the same
`min()` as §5, so the anchor is already the restrictive one, and it **is** per-protocol: `OI-THCOOL-17`
closed on 2026-09-03 and §3's efficacy floor makes `POE_block` per-protocol (33.8 / 34.2 / 34.6 °C at
40 / 60 / 120 J/cm²). Writing the rule against the anchor rather than against 35.0 is what let §6.1
land first and need no revision when it did.

> **While the latch is set, admission tests `ambient ≤ T_block_eff − Δ` in place of
> `ambient < T_block_eff`** (and `ambient ≥ t_low_block + Δ` in place of `ambient > t_low_block`).
> The latch is **set** whenever an admission is denied on ambient or a §6 re-read crosses an edge
> mid-session; it is **cleared** by any evaluation that passes the margined test continuously for
> `t_dwell`.

**The latch raises the bar; it does not hold the device off.** That shape is deliberate, and three
properties follow:

1. **It cannot widen safety, so §5's `min()` proof is untouched.** The margined test is strictly more
   restrictive than the plain one at every ambient and every protocol. Formally it is a fourth `min()`
   term with values `{0 %, 100 %}`; nothing else about the algebra changes.
2. **It needs one boolean and one timer** — no stored anchor, no per-envelope map. The anchor is
   recomputed by whatever request is being evaluated.
3. **It is per-protocol-correct.** A higher-dose protocol tolerates a higher ambient under
   `OI-THCOOL-17` (its derated dose reaches the efficacy floor later — 34.6 °C at 120 J/cm² against
   33.8 at 40), so a blanket hold-off would deny sessions that are genuinely admissible.

**Mid-session crossing terminates the session; it never pauses it** (`NP-THERM-COOL-001` §7.5.3). A
resumed session is a split, partially-dosed session whose completion report would overstate delivered
dose; suspend/resume cycles add time-at-ceiling, which drives CEM43; and a resume path is an automatic
re-entry loop, which is the thing being removed. **With no automatic re-entry anywhere, chatter is
impossible by construction** and Δ governs only the next user-initiated start. The session record
reports the dose actually delivered.

**`t_dwell` inherits `OI-ENV-05`** — 60 s with a dedicated ambient NTC outside the thermal path; ≥ 5τ_hub
if the hub NTC is the ambient proxy, since it must outlast the device's own self-heat decay. **Fail-safe
either way**: a self-heat-contaminated proxy reads high, making the gate more restrictive, never less.

**Where the parameters live, and the trap that decides it.** Δ and `t_dwell` are fields of the **Class C
versioned envelope table**, alongside the bounds they modify. **They are deliberately not in the POE
block, and the descriptor format is unchanged**, because:

> **A margin parameter composes by `max()`, not `min()`.** Folding a descriptor-supplied Δ into §5's
> `min()` would let a stale or hostile descriptor supply Δ = 0 and erase the hysteresis while appearing
> to obey the rule that a descriptor may only restrict. **The direction of "restrictive" inverts for a
> parameter that is itself a margin** — larger is stricter. If Δ is ever moved into the POE block, it
> must enter as `max(Δ_TABLE, Δ_POE)`, and `t_dwell` likewise.

**Three things this deliberately does not do.**

- **No persistence across a power cycle.** The latch is RAM state and unplugging clears it. The dodge is
  acceptable because **it cannot cross the safety bound**: it buys re-entry only between
  `T_block_eff − Δ` and `T_block_eff`, where the plain admission test already passes. Persisting it
  would mean `NP-FW-NVRAM-001` writes for a usability latch with no safety content.
- **No new SHDR field.** The existing `NP_POE_OUT_OF_RANGE` reason covers the denial; distinguishing the
  margined test from the plain one is diagnostic only, and a new field would need a
  `docs/reference/data-architecture-detail.md` §5.1 boundary resolution first.
- **No new indicator, and Mode 3 is unaffected.** The latch is SW-01 state, so §8's guarantee holds
  unchanged with no app present.

**App display (SW-03, Class B).** While the latch is set the app shows the **re-arm** temperature, not
the block temperature — "available again at 34.0 °C", not "blocked above 35.0 °C". Showing the block
number when re-entry needs a degree less is what makes a correctly-working device look broken.

## 7. Versioning & table skew (keeps app and MCU consistent)

- `table_version` in the POE is checked against the MCU's provisioned table version.
- **Match →** POE_clamp participates in the min() as a smooth-derate hint; numbers already agree.
- **Skew (app newer or older) →** MCU **ignores POE numbers**, uses TABLE_clamp only (authoritative,
  stricter-or-equal), raises `NP_POE_TABLE_SKEW`; app prompts a table refresh so it can recompute/display
  correctly. Safety is never affected by skew.
- Table updates ride the **controlled SW-01 update path** (explicit user confirm, not automatic OTA —
  NP-SW-001 §7.2), because the table is Class C config. THERM-1a refinements bump the version.

## 8. Autonomous Mode 3 (no phone)

Pre-loaded signed protocol carries its POE; the MCU enforces from its own table + POE + SR-FAN with no app
present. Soft warnings simply aren't displayed (no app), but every HARD bound holds. ✓

**The efficacy-floor refusal holds in Mode 3 too**, and this is the reason it was sited in SW-02 rather
than SW-03: the session runner is on the main processor and runs with no phone attached, so an
autonomous session in a 34.5 °C room is refused exactly as a connected one is. What Mode 3 loses is only
the *explanation* — `OI-THCOOL-18`'s wording problem becomes an LED and a stored reason code, which is
a worse place to deliver "a higher-dose protocol may still run" than a screen. Worth resolving with
`OI-THCOOL-18` rather than after it.

## 9. New failure modes (FMEA-style; feed NP-FMEA-001 / NP-FMEA-GEOM-001)

| FM | Effect | S | Mitigation | Res |
|----|--------|---|------------|-----|
| POE claims wider than table | would over-run thermal envelope | S3 | min() with authoritative TABLE_clamp; skew/wider flags | S2×P1 |
| Ambient sensor fault | no admission basis | S3 | fail-safe worst-case ambient → most restrictive (FMEA-M04-02 pattern) | S2×P1 |
| table_version skew (OTA drift) | app/MCU disagree | S2 | MCU authoritative + refresh prompt | S1×P1 |
| POE truncated/absent | — | S2 | length+signature reject (truncated) / TABLE governs (absent) | S1×P1 |
| App displays stale range | user confusion | S1 | version tag forces refresh before display | S1×P1 |
| Ambient dithers on a hard edge | start/deny/start chatter; a device that looks broken | S1 | §6.1 latch: Δ = 1.0 °C margined re-admission + `t_dwell`; **no automatic re-entry path exists** (mid-session crossing terminates, never pauses) | S1×P1 |
| Latch cleared by power cycle | user re-enters within the band | S1 | bounded by construction — re-entry is only possible between `T_block_eff − Δ` and `T_block_eff`, where the plain test already passes; **the safety bound is never crossed** | S1×P1 |
| Hysteresis erased by a supplied Δ = 0 | edge chatters again | S1 | Δ is Class C table config, not descriptor-carried; if ever moved, it enters as `max()` (§6.1) | S1×P1 |
| `dose_full_dJ` overstated (bad tooling or tamper) | floor set too low; a sub-threshold session is admitted — the D-4 failure mode re-entering through the descriptor | S1 (efficacy, not safety — thermal bounds unreachable, §1.1) | SW-02 recomputes `duty_floor_pct` and takes the larger; `OI-POE-07` consistency check against commanded irradiance × duty × length | S1×P2 |
| `dose_full_dJ` understated | session refused earlier than necessary | S1 | same check; fails toward refusal, which is the safe direction for this bound | S1×P1 |
| bit5 clear on a PBM protocol (0x01 descriptor, or a stale app) | no floor clamp; behaviour reverts to `poe_version` 0x01 | S1 | accepted by design — thermal safety unaffected; app refresh prompt on version skew (§7) | S1×P1 |
| Ambient crosses the floor edge **mid-session** | session already admitted; refusal condition now true | S1 | **Answered by §6.1** — a §6 re-read that crosses an edge **terminates** the session and sets the latch; it never pauses, because a resumed session would be a split, partially-dosed one whose report overstates delivered dose. The session record reports the dose actually delivered, which for this bound is the whole point | S1×P1 |

## 10. Allocation & verification

| Item | Owner / Class | Verification |
|------|---------------|--------------|
| POE block compute + sign | SW-03 (B) | app==MCU table parity test for identical inputs |
| MCU table + `min()` clamp + fail-safe | SW-01 (**C**), extends SW01-M04 (or new SW01-M09) | unit: interpolation, min-logic, skew fallback, absent-POE fallback, no-ambient fail-safe; 100 % branch |
| Session-runner derate | SW-02 (B) | integration: ambient sweep → smooth derate → MCU backstop only at edge |
| §6.1 hysteresis latch | SW-01 (**C**) | unit: set on ambient denial; margined test while set; cleared only after `t_dwell` below `T_block_eff − Δ`; latch never admits above `T_block_eff`. Integration: dither ambient ±0.5 °C across the edge → exactly one transition; mid-session crossing **terminates** and does not resume |
| Efficacy-floor refusal | SW-02 (**B**), SW-03 presents | unit: `duty_floor_pct` recompute + max(); refusal at the per-protocol edge for 40/60/120 J/cm²; bit5-clear and `dose_full_dJ`=0 both bypass; **negative test that no floor path can raise `effective_duty`**; Mode 3 (no app) still refuses |
| System | — | run protocol as ambient rises → derate → block; inject skew → MCU authoritative; Mode 3 (no app) → HARD holds |

## 11. Open items

| ID | Description | Owner |
|----|-------------|-------|
| OI-POE-01 | Assign the envelope-table format + version scheme; provisioning path into SW-01 config (mirror Steinhart-Hart coeff provisioning) | FW |
| OI-POE-02 | **Settled → NP-FW-M09-ARCH-001: new module SW01-M09** (matches the interlock-checker pattern, freezes the certified M04 junction interlock). Residual: OI-M09-01…04 there | FW + Quality |
| OI-POE-03 | Wire the app POE compute to the *same* table artifact the MCU is provisioned from (single source, versioned) | FW + App |
| OI-POE-04 | Fill table values once THERM-1a (C3/C4) + datasheet bounds land (NP-ENV-OPRANGE OI-OPR-01…03) | Thermal + EE |
| OI-POE-05 | Add these failure modes to NP-FMEA-001 (or a hardware/firmware sibling) under change control | Quality |
| OI-POE-06 | **Ambient sense path resolution and `t_dwell` (§6.1), inherited from `OI-ENV-05`.** The POE block encodes temperatures in 0.1 °C (`i16` dC) but the shipped `adc_to_celsius()` returns whole degrees, so **1.0 °C is the finest band the sense path can currently express**. Fix the ambient source (dedicated NTC vs hub NTC proxy — the MCU config has no ambient channel today), then set `t_dwell` (60 s dedicated; ≥ 5τ_hub as proxy) and decide whether a sub-1 °C band is wanted enough to specify the path at 0.1 °C. Not blocking: the rule holds at Δ = 1.0 °C and the proxy error is fail-safe | FW + Thermal |
| OI-POE-07 | **Nothing checks `dose_full_dJ` against what the protocol actually commands.** The field is signed, so it is not forgeable by a third party, but it is *asserted* by the authoring tool rather than derived from the command stream — and an overstated value silently disables the clamp for the protocol that most needs it. Specify the consistency check against commanded irradiance × duty × declared length, **where it runs** (app sign-time, SW-02 admission, or both), and what an inconsistency does. Paired with `NP-THERM-COOL-001` `OI-THCOOL-19` | FW + App |
| OI-POE-08 | **The floor value itself has no home in this encoding.** `duty_floor_pct` is carried and SW-02 recomputes it — but from *what*? 10 J/cm² is inherited from `NP-PWR-BUDGET-001` §3.4 and is currently a constant with no version, no owner and no provisioning path, while the operating-envelope table it sits beside has all three. If `OI-OPR-07` makes the floor per-modality it needs to ride the same versioned artifact as the table (`OI-POE-03`), or app and runner will disagree about a number neither of them owns | FW + Thermal |

## 12. Cross-references

NP-ENV-OPRANGE-001 (envelope numbers; §1 derate semantics + efficacy-floor clamp) · NP-THERM-COOL-001
§7.4 / D-4 (why the clamp exists and why it is Class B) · NP-THERM-COOL-001 §7.5 (hysteresis sizing,
`OI-THCOOL-16`) · NP-PWR-BUDGET-001 §3.4 (the 10 J/cm² floor) · NP-PWRSRC-001 §5.5 (the CEM43 finding
that ruled out session extension) · `firmware/safety_mcu/src/np_thermal_interlock.c` +
`np_safety_config.h` (the 62/55 °C junction re-arm precedent §6.1 declines to copy) · NP-ENV-001 §4
(hybrid gate) · NP-HEX-ZM-001 §4/§4b (protocol v2,
module map) · NP-REQ-FANHEALTH-001 (SR-FAN ceiling) · NP-FMEA-001 §3.4/§3.7 · NP-SW-001 §6.2/§7.2.
