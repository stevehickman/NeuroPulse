# NP-FW-POE-001 — Protocol Operating-Envelope Encoding

**Program:** NeurOne firmware / thermal-safety
**Status:** DRAFT design (OI-OPR-05). Defines how a protocol's operating envelope + derate curve is
encoded in the signed session descriptor so the Class C Safety MCU (SW-01) and the app (SW-03) read the
**same** numbers — while guaranteeing a bad/absent envelope cannot widen safety.
**Sources:** NP-ENV-OPRANGE-001 (the envelope numbers), NP-ENV-001 §4 (hybrid gate), NP-HEX-ZM-001 §4/§4b
(NP Hub Protocol v2, required module map, `(socket:element)` addressing), NP-FMEA-001 §3.4/§3.7 (SW01-M04
thermal, SW01-M07 signature verify), NP-REQ-FANHEALTH-001 (SR-FAN ceiling), NP-SW-001 §6.2.
**IEC 62304 Class:** SW-01 enforcement = **C**; SW-02 derate + SW-03 compute/display = **B**.
**Date:** 2026-07-21

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
        POE_clamp, SR-FAN ceiling )                              SW-02 session runner derates duty via POE
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
| `poe_version` | u8 | envelope-schema version = 0x01 |
| `table_version` | u16 | operating-envelope **table** version the app computed against |
| `flags` | u8 | bit0 LOW_HARD · bit1 HIGH_HARD · bit2 FAN_REQUIRED · bit3 HUMIDITY_NA(=1) · bit4 HAS_SOFT_BOUNDS |
| `binding_module` | u8 | module/modality id that set the binding bound (display: "limited by 1170 nm laser") |
| `t_low_block_dC` | i16 | cold hard block, °C ×10 (e.g. +50 = +5.0 °C) |
| `t_low_soft_dC` | i16 | cold soft-warn threshold, °C ×10 (efficacy) |
| `n_points` | u8 | high-side derate breakpoints, ≥2 |
| `points[n]` | {i16 temp_dC, u8 duty_pct} | monotonic ↑ temp, monotonic ↓ duty; [0]=full-dose knee (duty 100), [n-1]=block (duty 0) |

**Derate semantics:** `max_duty(ambient)` = piecewise-linear over `points` (100 % below `points[0].temp`,
0 % above `points[n-1].temp` = hard block); `ambient < t_low_block_dC` → block. `t_low_soft_dC` and any
SOFT-flagged bound are **app-only** (MCU ignores them).

**Worked example — full multi-modal protocol (binding = 1170 TEC):**
`table_version=7`, `flags=LOW_HARD|HIGH_HARD|FAN_REQUIRED|HUMIDITY_NA`, `binding_module=T2-D`,
`t_low_block=+50 (+5.0 °C)`, `points=[(+300,100),(+350,0)]` → ≤ +30 °C full dose, +30→+35 linear derate,
> +35 blocked, < +5 blocked. ~14 bytes payload.

## 4. Enforcement flow

1. **App (SW-03, Class B):** compute protocol envelope = ∩ of activated modules' envelopes from the table;
   emit the POE block; Ed25519-sign the descriptor.
2. **SW-01 verify (M07):** signature + length check (existing). Reject unsigned/truncated.
3. **SW-01 admission (thermal gate, extends M04):** read ambient (§6); compute
   `clamp = min( table_clamp(ambient, active_modalities, MCU_table_version), POE_clamp(ambient), SR-FAN ceiling )`.
   If `clamp == 0` (blocked) → deny enable, reason `NP_POE_OUT_OF_RANGE`.
4. **SW-02 session runner (Class B):** scale commanded duty by `POE_clamp(ambient)` so the device derates
   smoothly; the MCU clamp is the backstop, rarely reached.
5. **SW-01 real-time backstop (existing):** junction NTC 62 °C throttle / 65 °C cutoff + SR-FAN face-temp
   protection run continuously — the reactive guarantee beneath the predictive POE admission/derate.

**Layering:** POE = *predictive, ambient-based admission + derate* (coarse, evaluated at session start
baseline + coarse re-checks); junction NTC + SR-FAN = *reactive, real-time* thermal guarantee. Same MCU
owns both; the POE just keeps the reactive layer from having to fire.

## 5. The min() rule, spelled out (why nothing can widen safety)

`effective_duty = min( TABLE_clamp, POE_clamp, SR-FAN_ceiling )`, evaluated by the Safety MCU:
- **TABLE_clamp** — authoritative, from the MCU's own table × active modalities × sensed ambient.
- **POE_clamp** — from the descriptor; can only pull the result *down*.
- **SR-FAN_ceiling** — fan-fault natural-convection ceiling (NP-REQ-FANHEALTH-001), composes independently.
- POE absent → term dropped, TABLE_clamp governs (defense in depth: safety intact without a POE).
- POE claims a *wider* range than TABLE at any ambient → TABLE wins by the min(); a `NP_POE_WIDER_THAN_TABLE`
  SHDR flag is raised and the app is told to refresh.

## 6. Ambient sensing

- Source per OI-ENV-05: NTC read at **session start before self-heating**, and/or a dedicated ambient NTC.
- The POE admission/derate uses the **start-of-session ambient baseline** + coarse periodic re-reads
  (self-heating contaminates a live ambient read; the *dynamic* thermal safety is the junction NTC + SR-FAN).
- **No ambient reading → fail-safe:** assume worst-case ambient (survival-high) → most restrictive clamp,
  exactly like the open-NTC rule (FMEA-M04-02).

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

## 9. New failure modes (FMEA-style; feed NP-FMEA-001 / NP-FMEA-GEOM-001)

| FM | Effect | S | Mitigation | Res |
|----|--------|---|------------|-----|
| POE claims wider than table | would over-run thermal envelope | S3 | min() with authoritative TABLE_clamp; skew/wider flags | S2×P1 |
| Ambient sensor fault | no admission basis | S3 | fail-safe worst-case ambient → most restrictive (FMEA-M04-02 pattern) | S2×P1 |
| table_version skew (OTA drift) | app/MCU disagree | S2 | MCU authoritative + refresh prompt | S1×P1 |
| POE truncated/absent | — | S2 | length+signature reject (truncated) / TABLE governs (absent) | S1×P1 |
| App displays stale range | user confusion | S1 | version tag forces refresh before display | S1×P1 |

## 10. Allocation & verification

| Item | Owner / Class | Verification |
|------|---------------|--------------|
| POE block compute + sign | SW-03 (B) | app==MCU table parity test for identical inputs |
| MCU table + `min()` clamp + fail-safe | SW-01 (**C**), extends SW01-M04 (or new SW01-M09) | unit: interpolation, min-logic, skew fallback, absent-POE fallback, no-ambient fail-safe; 100 % branch |
| Session-runner derate | SW-02 (B) | integration: ambient sweep → smooth derate → MCU backstop only at edge |
| System | — | run protocol as ambient rises → derate → block; inject skew → MCU authoritative; Mode 3 (no app) → HARD holds |

## 11. Open items

| ID | Description | Owner |
|----|-------------|-------|
| OI-POE-01 | Assign the envelope-table format + version scheme; provisioning path into SW-01 config (mirror Steinhart-Hart coeff provisioning) | FW |
| OI-POE-02 | **Settled → NP-FW-M09-ARCH-001: new module SW01-M09** (matches the interlock-checker pattern, freezes the certified M04 junction interlock). Residual: OI-M09-01…04 there | FW + Quality |
| OI-POE-03 | Wire the app POE compute to the *same* table artifact the MCU is provisioned from (single source, versioned) | FW + App |
| OI-POE-04 | Fill table values once THERM-1a (C3/C4) + datasheet bounds land (NP-ENV-OPRANGE OI-OPR-01…03) | Thermal + EE |
| OI-POE-05 | Add these failure modes to NP-FMEA-001 (or a hardware/firmware sibling) under change control | Quality |

## 12. Cross-references

NP-ENV-OPRANGE-001 (envelope numbers) · NP-ENV-001 §4 (hybrid gate) · NP-HEX-ZM-001 §4/§4b (protocol v2,
module map) · NP-REQ-FANHEALTH-001 (SR-FAN ceiling) · NP-FMEA-001 §3.4/§3.7 · NP-SW-001 §6.2/§7.2.
