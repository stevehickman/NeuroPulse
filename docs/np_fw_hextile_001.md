# Hex-Tile On-Module Driver Firmware Specification

**Project:** NeurOne
**Document:** NP-FW-HEXTILE-001
**Revision:** 2
**Date:** 2026-09-28
**Status:** DRAFT
**Effective Date:** —
**Author:** NeurOne Firmware Engineering
**Approved By:** — (pending design review)
**References:** NP-HW-HEXTILE-001 Rev 24 (§6.2 driver topology, §6.5 firmware boundary, §7.2 pinout, §7.3 mating order, §8.3 3.3 V budget, §8.4 safety gating, D-3, D-4 not adopted, D-7, D-8, D-9); NP-FW-PBM1064-001 Rev 5 §5 (register map 0x00–0x0D, frequency codes, duty ceiling, hub startup and poll); NP-HEX-ZM-001 Rev 9 §4.1 (tile identity block); NP-MOD-ID-001 §5 (on-module odometer, MODID-4a mate count); NP-SOUP-LFS-001 §13.14 (`REQ-TDRV-01`/`-02`); NP-FEAS-PBMCH-001 (`OI-PBMCH-01`); NP-DRV-SHELL-002 §3.3a (front end on the cluster controller), REQ-EMI-03/-04; NP-SES-PWR-001 (`OI-SESPWR-03`); NP-SW-001 Rev 11; CLAUDE.md Rev 59 §3, §4.2, §18
**Related Issues:** — (closes `NP-HW-HEXTILE-001` `OI-HEXTILE-07`; successor to the never-written NP-FW-ZM-TINY402-001, `OI-PBM-08`)
**Gate:** GATE-2 (PBM coupling bench) — module bring-up
**IEC 62304 Class:** SW-04 Class B (hex-tile on-module firmware; registered NP-SW-001 Rev 11)
**Supersedes:** None
**Parent Document:** NP-HW-HEXTILE-001

---

> **Rev 2 (2026-09-28): the tile delivers the duty and mode the hub commands. It never clamps
> them, and CW is the gate held on (principal direction; CLAUDE.md Rev 59).** Rev 1 clamped every
> DUTY write to 25 %, CW included, and held a CW channel off until a "carrier" was chosen. So a CW
> protocol produced either a 25 % pulse train or nothing, and neither is the protocol that was
> authored. **Both are withdrawn.** R-4 is now 400 mW/cm² peak, with the time average held to 200 mW/cm² per channel plus a weighted
> sum across wavelengths, at
> whatever duty the protocol specifies. A protocol outside that is refused before it is signed, never
> reshaped (`NP-HW-HEXTILE-001` `OI-HEXTILE-31`). U1 has no emitter data, so it cannot tell a legal
> duty from an illegal one, and it enforces neither. The bounds that hold against U1's own failure are
> U3 (peak) and a U4 redesigned to bound the average (`OI-HEXTILE-30`). §5.2, §5.3, §5.5, §5.6, §13,
> §14, §15 and §16 change. `OI-FWTILE-06` is retired: its premise that CW needs a carrier was wrong.

> **Rev 1 (2026-09-28): the tile firmware is specified. This closes `OI-HEXTILE-07` as a
> specification item. No tile firmware exists, no part is selected, and nothing is measured.**
>
> Three things changed after `OI-HEXTILE-07` was worded, and this document is written against the
> record as it stands now, not against the item's wording:
>
> 1. **D-4 was not adopted** (`NP-DRV-SHELL-002` §3.3a, 2026-08-11). The PD front end and the NTC
>    readout stay on the cluster controller, and `PD1_K`, `PD2_K` and `NTC` cross the socket. The
>    item's "PD ADC readback registers" therefore have nothing to read, and **none are defined**
>    (§3). Its "local NTC throttle" is still required, by R-9, but the tile has no temperature
>    input of its own on the current pinout. That is **`OI-FWTILE-01`**.
> 2. **D-9 bounds optical output in hardware** (U3 peak, U4 duty; `REQ-TDRV-01`/`-02`). The firmware
>    is the *fine* control, not the ceiling. It therefore neither knows nor enforces `I_cap` (§5.4).
> 3. **Two binding inputs were added:** the identity block at `0x40` (`NP-HEX-ZM-001` §4.1) and the
>    odometer with its once-per-power-up mate command (`NP-MOD-ID-001` §5, MODID-4a).
>
> **Answered here, from the items that routed a question to this specification:**
>
> - **`OI-PBMCH-01`, the firmware half.** `CUR = 0` means the channel's gate is never driven, not
>   "regulate to zero" (§5.3). Whether an undriven gate emits nothing is still a part property and
>   still waits on `OI-HEXTILE-02`.
> - **`OI-HEXTILE-25`(c): which limit governs CW.** The 25 % firmware ceiling governs every mode, CW
>   included, on the tile as on the hub. **⚠ WITHDRAWN Rev 2: it made CW undeliverable as CW.** R-4's 200 mW/cm² CW figure is a ceiling nothing requires the
>   device to reach (§5.5).
> - **`OI-HEXTILE-25`(d).** The register map now says that `CUR` codes above `I_cap` saturate in
>   hardware (§5.4).
>
> **Found while writing it, and raised rather than decided:** §6.2's U1 package (SOIC-8/SOT-23-8)
> cannot carry the pins this firmware needs (§12, `OI-FWTILE-03`). §6.2 gives U3 a fixed reference
> and no adjustable setpoint, so `CUR` has no circuit to act on (`OI-FWTILE-02`). And `VLED` mates
> before `VCC_3V3` (§7.3), so every gate spends each insertion with U1 unpowered (`OI-FWTILE-04`).

---

## 1. Scope

This document specifies the firmware of **U1**, the tinyAVR 2-series MCU that `NP-HW-HEXTILE-001`
**D-3** fits to every hex tile (T1-A, T1-B, T1-C). It covers:

- the I2C register map the hub drives (§5), extending `NP-FW-PBM1064-001` §5.1;
- when a channel's gate may be driven (§5.3), and what each register means under D-9 (§5.4–§5.6);
- faults, the latch and `ALERT#` (§5.7), and the local over-temperature response (§6);
- the identity block (§7) and the odometer (§8), both specified elsewhere and implemented here;
- addressing (§9), power states (§10), and reset and programming behaviour (§11);
- what U1 needs from the circuit (§12), and what this firmware deliberately does not do (§13).

**Out of scope:** the hub side of every exchange (`NP-FW-PBM1064-001`, `NP-FW-HUB-001`,
`np_hexmap_inventory.c`); the drive-stage circuit and its values (`OI-HEXTILE-24`); PD metering,
which is the cluster controller's (`NP-DRV-SHELL-002` §3.3a); the electrode path on T1-B, which
does not pass through U1 (`NP-HW-HEXTILE-001` §7.2 pins 17–18).

**No code exists.** No `firmware/` directory holds tile firmware, and nothing here is verified. §15
lists the tests this specification needs.

---

## 2. Inputs this specification is bound by

| Source | What it binds here |
|---|---|
| `NP-FW-PBM1064-001` §5.1–§5.5 | Register addresses `0x00`–`0x0D`, their encodings, the frequency codes, ~~and `NP_PBM_DUTY_MAX_REG` = `0x32`~~ (Rev 2: not binding on the tile, §5.5). **The hub code is written against this map** (`firmware/pbm/src/np_pbm_drive.c`), so this document keeps every address and encoding and defines only what was left undefined |
| `NP-HEX-ZM-001` §4.1 | The identity block at `0x40`, byte for byte (§7) |
| `NP-MOD-ID-001` §5.2, §5.3, §5.3.1 | The odometer record, its four-slot rotation, its CRC over UID ‖ payload, and MODID-4a (§8) |
| `NP-HW-HEXTILE-001` §8.3 | ≤ 2 mA standby, ≤ 25 mA active per module on `VCC_3V3` (§10) |
| `NP-HW-HEXTILE-001` R-9, CLAUDE.md §4.2 | A per-tile response at 62 °C that does not depend on the bus (§6) |
| `NP-HW-HEXTILE-001` D-9 | U3 caps the on-current and U4 caps the conducting fraction, in hardware. The firmware is inside those bounds and is not one of them (§4, §5.4) |
| `NP-HW-HEXTILE-001` §7.2 | The pins U1 sees: `SDA`, `SCL`, `SYNC`, `ALERT#`, `VCC_3V3`, `DGND`. `PD1_K`, `PD2_K` and `NTC` go to the controller and not to U1 |
| `NP-DRV-SHELL-002` REQ-EMI-03/-04 | Pulse phase is deterministic and locked to `SYNC`, and never dithered (§5.6) |
| CLAUDE.md §3 (Rev 59) | 400 mW/cm² peak; the time average at ≤ 200 mW/cm² per channel plus a weighted sum across the tile's wavelengths; duty and mode are the protocol's. A ceiling refuses a protocol and never reshapes it (§5.5) |
| CLAUDE.md §18 | Every requirement below states what fails without it and where it traces (§14) |

---

## 3. What the item asked for and what exists to build

`OI-HEXTILE-07` asked for a register map *"extending NP-FW-PBM1064-001 §5.1 with PD ADC readback
and local NTC throttle"*. That wording assumed **D-4**, under which U1 digitised both photodiodes
and the thermistor. D-4 was not adopted.

| Asked for | Status | Where |
|---|---|---|
| PD1/PD2 ADC readback registers | **Not defined.** The photocurrents leave the tile on pins 13–14 and are digitised on the cluster controller. A register would have nothing behind it. The range `0x0E`–`0x3F` that `np_hexmap_inventory.h` left clear "for the PD readback registers D-4 adds" is used for other things (§5.2) | `NP-DRV-SHELL-002` §3.3a |
| Local NTC throttle | **Specified (§6), not buildable yet.** The thermistor's node goes to pin 15 and is biased and read on the controller, so U1 has no temperature input on the current circuit. §6.2's RT1 row says *"read by U1 ADC"* and §7.2 routes it off the tile. Both cannot hold without a circuit decision | **`OI-FWTILE-01`** |
| ≤ 2 mA / ≤ 25 mA budget | Specified | §10 |
| Identity block at `0x40` | Specified | §7 |
| IEC 62304 Class B item registration | Done: **SW-04**, NP-SW-001 Rev 11 | §4 |

---

## 4. Classification and what the firmware is for

**SW-04, Class B.** The argument is the one that makes SW-02 Class B, applied one level down. No
failure of U1 can take a tile outside what these independent bounds allow:

| Tier | Mechanism | Holds when U1 is wedged or wrong? | Class |
|---|---|---|---|
| Fine | U1 gate control, this document | — | B (SW-04) |
| Per-channel hardware | U3 caps on-current at `I_cap` (`REQ-TDRV-01`); U4 caps the conducting fraction at 50 % over any `T_w` ≥ 250 ms (`REQ-TDRV-02`). **Rev 2: that form forbids CW and is to be redesigned to bound the 200 mW/cm² average instead (`OI-HEXTILE-30`)** | **Yes.** U4 acts on the gate path, not in U1 | hardware risk control |
| Coarse | 18 per-cluster `VLED` gates, hub-commanded (`NP-HW-HEXTILE-001` D-8) | Yes | hardware, commanded by SW-02 (B) |
| Hard | `NP_SAFETY_EN_PBM_CRANIAL`, in series with all of the above; cut on heartbeat loss in < 50 ms | Yes | SW-01 (C) |

> **⚠ Residual, stated rather than assumed.** D-9 is **decided, not built** (`OI-HEXTILE-24`). Until
> U3 and U4 exist and pass their bench test, a wedged U1 holding a gate on is bounded by the cluster
> gate, the Class C cut and the 62 °C thermal cut, not by an optical limit. That is the residual
> already carried by `RISK-FWHUB-10` and `RISK-NVRAM-04`. This document does not add to it and does
> not discharge it.

**What U1 is not.** It is not the peak-irradiance limit (U3), the duty limit that holds against its
own failure (U4), the dose meter (the cluster controller), or the safety enable (SW-01). It **is**
the only control that is per channel, per tile and fast. Every fine-grained behaviour the hub
commands, and the local over-temperature response of §6, runs here.

---

## 5. Register map

### 5.1 Access rules

- **7-bit I2C target, 400 kHz fast mode** (`NP-HW-HEXTILE-001` §7.2). Address: §9.
- **Register pointer.** The first byte of a write sets the pointer. Following bytes are written to
  consecutive registers, and a read starts at the pointer. The pointer increments after each byte
  and stops at `0xFF`. Reading past `0xFF` repeats `0xFF`'s value, and writing past it is ignored.
- **A write to a read-only or reserved register is acknowledged and has no effect.** Reserved
  registers read `0x00`. The hub never needs to distinguish a refused write from an accepted one
  on the bus, because every register that clamps or refuses reads back what it holds (§5.2).
- **Multi-byte registers** are little-endian, as the identity block is.

### 5.2 The map

`0x00`–`0x0D` keep `NP-FW-PBM1064-001` §5.1's addresses and encodings. The **Tile behaviour**
column is what this document adds.

| Addr | Register | Access | Reset | Tile behaviour |
|---|---|---|---|---|
| `0x00` | STATUS | R | per §5.7 | bit 0 `FAULT` = any of bits 1–4 set. Bits 1–4 are the **latched** `THERMAL`, `OCP_A`, `OCP_B`, `OCP_C` (§5.7), so a fault that has passed is still reported. Bit 5 `RESET_SEEN` (§5.7). Bits 6–7 read 0 |
| `0x01` | CH_ENABLE | R/W | `0x00` | bit 0 CH_A (660 nm), bit 1 CH_B (808 nm), bit 2 CH_C (1064 nm). **A bit for a channel the tile does not populate is held 0** (§5.3). Cleared per channel on fault entry (§5.7) |
| `0x02`–`0x04` | CUR_A/B/C | R/W | `0x00` | Commanded current, 0–255 → 0–180 mA (0.706 mA per code). **0 means the gate is never driven** (§5.3). Codes above `I_cap` saturate in hardware (§5.4) |
| `0x05`–`0x07` | PWM_FREQ_A/B/C | R/W | `0x28` (40 Hz) | Codes of `NP-FW-PBM1064-001` §5.2 only. An unlisted code is refused, and the register keeps its previous value. **`0x00` is CW: the gate is held on**, and `DUTY` does not apply (§5.5) |
| `0x08`–`0x0A` | DUTY_A/B/C | R/W | `0x00` | 0.5 % per code, `0x00`–`0xC8` (0–100 %). **Stored as written, never clamped.** A write above `0xC8` is refused, and the register keeps its previous value (§5.5) |
| `0x0B` | THERMAL | R/W | `62` | Local over-temperature threshold, °C. **A write above 62 is stored as 62** (§6) |
| `0x0C` | FAULT_LATCH | R/W | per §5.7 | Same bit layout as STATUS bits 1–5. Writing `0xFF` clears every latched bit whose condition no longer holds (§5.7) |
| `0x0D` | CONFIG | R/W | `0x00` | bit 0 `SOFT_RESET`, self-clearing (§11). bit 1 **retired**: *"PWM_mode"* was never given a meaning. The hub still writes it (`np_pbm_drive_startup()`), and the tile ignores it (CLAUDE.md §18). bit 2 `SYNC_EN` (§5.6) |
| `0x0E`–`0x0F` | FW_VERSION | R | build | major, minor (§14 `REQ-FWTILE-13`) |
| `0x10` | CMD | W | — | `0x01` = `MATE_INCREMENT` (§8.2). Any other value is ignored |
| `0x11` | CMD_STATUS | R | `0x00` | `0x00` none issued this power-up · `0x01` mate recorded · `0x02` mate already recorded this power-up, not written · `0x80` odometer write failed |
| `0x12`–`0x3F` | — | — | `0x00` | reserved |
| `0x40`–`0xCD` | IDENTITY | R | factory | `NP-HEX-ZM-001` §4.1, at most 142 bytes (§7). Bytes past the block's own end read `0xFF` |
| `0xCE`–`0xDF` | — | — | `0x00` | reserved |
| `0xE0`–`0xFF` | ODOMETER | R/W | EEPROM | The newest valid 32-byte record, `NP-MOD-ID-001` §5.2 layout (§8) |

The identity block's worst case ends at `0x40` + 142 − 1 = `0xCD`, which is the bound
`np_hexmap_inventory.h` asserts at compile time. The odometer window is exactly one 32-byte record,
so one transaction reads or writes it.

### 5.3 When a gate is driven, and what setpoint 0 means

**U1 drives channel *x*'s gate only while all six conditions hold:**

1. `CH_ENABLE` bit *x* is 1;
2. `CUR_x` ≠ 0;
3. `DUTY_x` ≠ 0, for a pulsed code (CW ignores `DUTY`, §5.5);
4. the tile populates channel *x*, as read from its own identity block at boot;
5. no fault latched against channel *x* (§5.7);
6. the channel's PWM is in its on-phase (always, for CW).

Otherwise the gate is **held** at its non-conducting level. It is driven there, not left
unconfigured. The same holds for any other gate-path output U1 owns under `OI-FWTILE-02`, such as a
setpoint output or a regulator enable.

**This answers the firmware half of `OI-PBMCH-01`.** On a base tile `np_mod_pbm_base_params_t` has
no `ch_mask`, so `cur_a = 0` is the hub's only way to say "off". Under condition 2 it is a firmware
"off" that means the same as clearing `CH_ENABLE`: the gate is not driven. It is **not** a request
to regulate a string to 0 mA, which on a real regulator is an offset-limited current rather than
zero. So on a base tile, *"660 nm off"* and *"660 nm at 0 %"* are the same firmware state, which is
what NP-FEAS-PBMCH-001 §6.2 option (b) needs this document to say.

**What is still open, and is not firmware.** With the gate held off, the string still sees the FET's
off-state leakage from `VLED`. Whether that emits measurably is a property of the FET and the
emitter, and no emitter is selected (`OI-HEXTILE-02`). So `OI-PBMCH-01` keeps its hardware half.
That half decides whether "off" is *provably* zero emission, and so whether the base struct needs
a `ch_mask` byte after all.

**Condition 4, and why an unreadable identity turns every channel off.** U1 learns which channels
exist from the element types in its own identity block. If that block fails its own CRC at boot,
U1 treats **every** channel as unpopulated. The hub rejects that tile anyway (`NP-HEX-ZM-001`
§4.1: an invalid block applies the socket as empty), so the two sides agree that nothing there may
be driven. On a T1-A, CH_C has no FET (Q3 is not fitted), and holding bit 2 at 0 makes `CH_ENABLE`
read back what can actually conduct.

### 5.4 `CUR` under D-9

`CUR` is the **commanded** current, and the only way the protocol library sets irradiance. The
compiler writes `intensity` linearly into it, and duty is already spent on the pulse pattern
(`NP-HW-HEXTILE-001` Rev 21 banner). Two statements the register map owes (`OI-HEXTILE-25`(d)):

- **Codes above `I_cap` saturate at `I_cap`, in U3.** The 0–180 mA span covers about 480 mW/cm² at
  the design-target flux, and `I_cap` is sized to ≤ 400 mW/cm² at worst-case tolerance
  (`REQ-TDRV-01`). A code above it is not an error and is not refused. It delivers `I_cap`.
- **The firmware does not clamp `CUR` to `I_cap`, and holds no value of it.** `I_cap` depends on the
  emitter (`OI-HEXTILE-02`) and the regulation tolerance (`OI-HEXTILE-24`), and D-9 puts the bound
  where no firmware, register or stored value can move it. A firmware copy would be a second,
  movable value of a hardware constant. It could only disagree with U3, and never protect beyond it.

**How `CUR` reaches the current is not yet a circuit.** §6.2's U3 has a **fixed** reference that
sets `I_cap`. Nothing in §6.2 lets U1 command a current below it, but the library needs 5–90 mA on
CH_A (`OI-HEXTILE-29`). U1 needs a setpoint output that U3 clamps. On a part with no DAC that is a
filtered PWM, one pin per channel. That is **`OI-FWTILE-02`**, which must close with `OI-HEXTILE-24`.
Until it does, this document fixes what `CUR` *means*, not how U1 produces it.

### 5.5 Duty and CW: delivered as commanded

**U1 stores each DUTY write as written, 0–100 %, and drives the gate to it.** It never clamps and
never substitutes a nearby value. A write above `0xC8` (100 %) has no meaning, so it is refused. The
register keeps its previous value and reads it back, so the hub can see that the write did not land.

**Frequency code `0x00` is CW, and CW is the gate held on.** While §5.3's conditions hold, the gate
is on continuously. There is no carrier, and `DUTY` does not apply. Irradiance is set by `CUR` alone.
A channel pulsed at any rate, however fast, is a different stimulus from CW, and so a different
protocol (principal, 2026-09-28).

**Why U1 enforces no duty ceiling.** CLAUDE.md §3 (Rev 59) bounds PBM at 400 mW/cm² peak, and bounds
the time average at 200 mW/cm² per channel plus a weighted sum across the tile's wavelengths. It leaves
duty to the protocol. Whether a duty is legal therefore depends on the irradiance at the commanded
current, `DUTY` × *E*(`CUR`), and on the other channels. *E* is emitter data U1 does
not hold (§5.4). A tile-side check would need a firmware copy of the emitter's flux curve, which can
disagree with the part fitted. A tile-side clamp would do worse: it would deliver a stimulus nobody
authored. The rule is **refuse, never reshape**, and refusal happens where the irradiance is known:

| Where | What bounds it | State |
|---|---|---|
| Before signing | A protocol whose peak exceeds 400 or whose time average exceeds 200 mW/cm² is refused | **not built** (`OI-HEXTILE-31`) |
| Hub | Today `NP_PBM_DUTY_MAX_REG` clamps to 25 %. It is the only firmware bound in force, so it is replaced by the pre-signing check, not deleted ahead of it | **still clamps** (`OI-HEXTILE-31`) |
| Tile hardware, holding against U1's own failure | U3: peak ≤ 400 (`REQ-TDRV-01`). U4: today ≤ 50 % conduction, which forbids CW. To be redesigned to hold the average ≤ 200 and allow any duty, CW included, below it | **U4 redesign open** (`OI-HEXTILE-30`) |

**Consequence to know until those close.** A pulsed duty above 25 % cannot reach the tile today:
the language refuses it, and the compiler and the hub clamp it. **A CW command can.** The language
allows `frequency: 0`, and nothing checks a CW protocol's `CUR` against R-4's average terms. So
under this specification, a CW protocol at high intensity would be bounded only by U4, and U4 is
neither built nor yet in its redesigned form. No tile exists, so nothing is exposed today. **This is
why the pre-signing check is the first thing to build, before any tile firmware is written and
before any clamp is removed** (`OI-HEXTILE-31`).

**`OI-HEXTILE-25`(c) is re-answered.** No duty limit governs CW. CW is bounded by its irradiance,
≤ 200 mW/cm², through `CUR`. Rev 1's answer is withdrawn.

### 5.6 Frequency, phase and `SYNC`

- **Period and on-time.** For a code of *f* Hz the period is 1/*f*, and the on-phase is
  `DUTY` × 0.5 % of it, at any duty from 0 to 100 %. `REQ-TDRV-02`'s 250 ms window was sized around a
  25 % library and forbids any duty above 50 %, so it is part of U4's redesign (`OI-HEXTILE-30`).
- **A change to `PWM_FREQ_x`, `DUTY_x` or `CUR_x` takes effect at the next period boundary.**
  Applied mid-period, a shorter period with the old on-time could run one pulse above the duty
  ceiling. U4 would bound it, but only at 50 %.
- **No dithering.** Periods are derived from U1's timer clock and are not spread or randomised
  (REQ-EMI-04), so the artifact stays at a known, subtractable line.
- **`SYNC_EN` = 1:** each channel's period starts on the `SYNC` edge (REQ-EMI-03). The `SYNC`
  waveform, its edge and its rate are the hub's (`OI-HUB-C05`). If `SYNC` stops, U1 free-runs at the
  commanded period. A missing reference costs phase lock, not emission control.

### 5.7 Faults, the latch and `ALERT#`

| Bit | Name | Set when | Action on entry | ALERT# |
|---|---|---|---|---|
| 1 | `THERMAL` | the local temperature reaches `THERMAL` (§6) | every gate held off; `CH_ENABLE` ← 0 | asserted |
| 2–4 | `OCP_A/B/C` | channel *x*'s sense current is outside its window (`OI-FWTILE-05`) | channel *x*'s gate held off; `CH_ENABLE` bit *x* ← 0 | asserted |
| 5 | `RESET_SEEN` | every power-up, brown-out, watchdog or soft reset | none (the reset already set every register to off) | **not** asserted |

- **Entry clears only what the fault reaches.** A `THERMAL` latch clears all of `CH_ENABLE`, and an
  `OCP_x` latch clears only bit *x*. That matches the hub's poll, which disables the affected
  channels and rewrites the rest (`np_pbm_drive_poll_status()`).
- **Clearing a latch never re-enables anything.** Writing `0xFF` to `FAULT_LATCH` clears each bit
  whose condition no longer holds. `THERMAL` stays set while the temperature is at or above the
  threshold. `CH_ENABLE` stays as the fault left it, so the hub has to rewrite it to resume.
- **`ALERT#` is driven low while any of bits 1–4 is latched**, and released when they clear. It is
  open-drain and wire-ORed per cluster, so a tile fault reaches the hub, and through it the safety
  MCU, without a poll (`NP-HW-HEXTILE-001` §7.2 pin 12).
- **`RESET_SEEN` is how the hub learns that a tile restarted mid-session.** Every register returns
  to off on a reset, so emission has stopped. A hub that did not notice would keep integrating a
  dose the tile is not delivering (R-7). It is kept out of `FAULT` and `ALERT#`, because it is set at
  every power-up. Otherwise the hub's startup check (`NP-FW-PBM1064-001` §5.3 step 6) would refuse
  the first session after every boot.

---

## 6. Local over-temperature response (R-9)

**Requirement.** When the tile's local temperature reaches `THERMAL`, U1 holds every gate off,
latches `THERMAL`, clears `CH_ENABLE` and asserts `ALERT#`. It does this **without a bus
transaction and without waiting for the hub**, and it does not resume on its own.

- **Threshold.** `THERMAL` resets to 62 °C, and a write can only lower it. 62 °C is the junction
  limit of CLAUDE.md §4.2 and R-9. A hub that could raise a tile above it would turn an IEC
  60601-derived limit into a register value.
- **No hysteresis figure.** Resuming needs the temperature below the threshold, a latch clear and a
  fresh `CH_ENABLE` from the hub (§5.7). The hub decides when to resume, and a firmware hysteresis
  would be a number with nothing requiring it (CLAUDE.md §18).
- **Sampling.** The temperature is sampled at least once per PWM period while any channel is
  enabled. This is the tile's only temperature consumer, and it needs no faster sampling than the
  thermal time constants the scalp side already takes (`NP-THERM-CFD-R1-001`).

**What this does not replace.** The hub still reads every tile's NTC through the controller, and
the safety MCU's cut is still the Class C response (§4). This is the per-tile, bus-independent
response that RISK-26 lists as *"on-module NTC throttle, duty → 0"* (`NP-HW-HUB-001` §7.2.1).

**⚠ It cannot be built on the current circuit (`OI-FWTILE-01`).** U1 has no temperature input. The
thermistor's only node is pin 15, biased and read on the controller, and it floats whenever the
controller's mux selects another socket. So U1 cannot read it. Three ways out, for EE:

| Option | What it costs | What it leaves |
|---|---|---|
| A second thermistor on the tile, on its own U1 ADC input and bias | ~1 part, 1 pin, a few µA of bias in standby (§10) | Two sensors that must agree. The controller's stays the dose-model input |
| One thermistor with a fixed on-tile bias, read by U1 and sensed high-impedance by the controller | 1 pin | The controller's front end and calibration change (`NP-DRV-SHELL-002` §6.3) |
| U1's on-die temperature sensor | Nothing | Measures the rigidizer on the shell-facing face, not the emitter junction. **Not equivalent, and not proposed** |

**The hub's staged throttle is pre-empted (`OI-FWTILE-08`).** `NP-FW-PBM1064-001` §7.4 drops CH_C
first at 62 °C, then CH_B after 5 s, then everything at 65 °C. A tile that goes fully dark at 62 °C
leaves that staging nothing to act on. The conservative order is kept: the tile cuts at the limit.
The hub's staging is only meaningful if it starts below 62 °C, and that is the hub's to restate.

---

## 7. Identity block (`NP-HEX-ZM-001` §4.1)

The block is implemented **byte for byte** as `NP-HEX-ZM-001` §4.1 defines it: format `0x01`, 8-byte
UID, `elem_count`, `elem_type[n]` in `element_id` order, then a CRC-32 (IEEE 802.3 reflected,
little-endian) over bytes 0…9+n. Its layout is owned there and not restated here.

| Rule | Why |
|---|---|
| **The block is written at manufacture, whole, CRC included, and U1 serves those bytes.** U1 never computes the served CRC and never rewrites any byte of the block | A CRC U1 computed at boot would certify whatever its memory held, and a corrupted UID or element list would then pass the hub's check. The seal is only worth something if it was computed by the tool that wrote the data |
| **Storage is flash, outside the application image**, write-protected after manufacture | EEPROM is fully taken by the odometer (4 × 32 = 128 bytes), and the user row is too small for a T1-A block (10 + ~93 + 4 bytes). A firmware update must not be able to change a tile's identity |
| **UID never all-`0x00` or all-`0xFF`** | The hub treats those as "empty" and "floating bus" (`NP-HEX-ZM-001` §4.1). The factory tool refuses them |
| **Reads past the block's end return `0xFF`; writes to the region are ignored** | The hub reads the header, then the whole block, and never past `10 + n + 4` |
| **U1 checks its own block's CRC at boot** and treats every channel as unpopulated if it fails (§5.3) | U1 reads its channel population from the block. A tile whose identity is corrupt must not emit, and the hub will already have applied its socket as empty |

---

## 8. Odometer (`NP-MOD-ID-001` §5)

### 8.1 The record

U1 keeps the 32-byte record of `NP-MOD-ID-001` §5.2 in its 128-byte EEPROM, as four slots written
round-robin. **The newest slot whose CRC is valid wins.** The CRC covers the tile's own UID followed
by the payload, so a record transplanted between tiles fails.

| Operation | Behaviour |
|---|---|
| **Read** `0xE0`–`0xFF` | The newest valid record, CRC included, so the hub can check it itself. With no valid slot, all 32 bytes read `0x00`. The zero `magic` makes that detectable |
| **Write** 32 bytes starting at `0xE0` in one transaction | U1 checks `magic` and `version`. It **replaces** the hub's `mate_cycles_observed` with its own value (§8.2), computes the CRC over its UID ‖ payload itself, and commits to the next slot at the STOP. Any other write shape (a different start, a short write, a length past 32) is discarded |
| **Regression refused** | A write whose `session_count`, `emitter_on_seconds`, `thermal_seconds_over_threshold`, `peak_ntc_celsius_ever` or `throttle_events` is below the stored value is discarded, and `CMD_STATUS` reads `0x80` |

**Why the hub never supplies the CRC.** The CRC's job is to bind a record to one tile's UID. A hub
that computed it could bind any record to any tile, which is the transplant the CRC exists to
catch.

**Why regressions are refused.** These counters travel with the part (`NP-MOD-ID-001` §6) and feed
exposure-count maintenance prompts (CLAUDE.md §2.3). A hub writing a stale baseline, for example a
cache from another device or a restored record, would silently erase wear. Refusing lower values
makes the odometer's own record the floor.

**Which fields U1 computes.** Only the CRC and the mate count. Every other field is composed by the
hub, which alone knows the session count, the commanded on-time and the controller's NTC and PD
readings. `distinct_socket_count` has no specified computation (`NP-MOD-ID-001` `OI-MODID-09`). U1
stores whatever the hub writes there, and the refusal rule above does not cover it.

### 8.2 The mate command (MODID-4a)

`CMD` = `0x01` (`MATE_INCREMENT`):

- **First time this power-up:** U1 increments `mate_cycles_observed`, writes the whole record to the
  next slot at once, and sets `CMD_STATUS` to `0x01`. It does not wait for session end, because that
  wait is the gap MODID-4a closes.
- **Any repeat this power-up:** no write, and `CMD_STATUS` = `0x02`. This is success, as MODID-4a
  rule 4 requires.
- **The latch is armed only by a power-up**, a real loss of `VCC_3V3`. A watchdog or `SOFT_RESET`
  does **not** re-arm it: neither is a mate, and the hub's startup sequence issues `SOFT_RESET`
  before every first session (`NP-FW-PBM1064-001` §5.3 step 1). U1 tells the reset causes apart from
  its reset-flag register at boot.
- **A failed EEPROM write** sets `CMD_STATUS` = `0x80` and leaves the previous slot valid. The rotation
  exists for exactly that.

---

## 9. Addressing and transport

**Mode S, implemented now: one tile per electrical segment.** U1 answers at the fixed 7-bit address
**`0x30`** (`NP_PBM_I2C_ADDR`), which is what the hub's probe and inventory already use. It works
whenever the hub reaches each socket on its own segment, as the cluster controller's per-socket
switch does (`NP-DRV-SHELL-002` §3.4). It needs nothing from the tile beyond §5–§8.

**Mode D, D-7's shared cluster segment: requirements only, waiting on `OI-HUB-C15` (`OI-FWTILE-07`).**
If `OI-HUB-C15` puts 3–6 tiles on one segment, a fixed address collides. The hub must also read each
UID before any tile has a unique address (`NP-HEX-ZM-001` §4.1, *Topology*). Whatever mechanism
`OI-HUB-C15` picks, the tile side must meet all of these:

1. **The UID is readable before assignment, collision-safely.** An SMBus-ARP-style discovery works:
   every unassigned tile transmits its UID at one shared discovery address, bitwise arbitration
   leaves exactly one winner per pass, and the others drop out silently.
2. **Assignment is bound to the UID.** A tile adopts an address only from a write that names its own
   UID.
3. **An assigned tile stops answering the shared addresses.**
4. **Assignment is volatile.** Any reset returns the tile to unassigned, so a re-seated tile is
   re-discovered, which the power-on poll relies on.
5. **Standby wake (§10) survives the scheme.** The wake must match both the discovery address and
   the tile's own.

U1 can meet 1–5 if its TWI peripheral reports a lost data bit while transmitting and matches a
second address in hardware. Both are to be confirmed against the datasheet (`OI-FWTILE-03`).
**HT-DRC-11 stays open until `OI-HUB-C15` settles**, because under Mode S the collision is
impossible by topology and under Mode D it rests on 1–4.

---

## 10. Power states (`NP-HW-HEXTILE-001` §8.3)

| State | Entered when | U1 obligations | Budget (whole module, `VCC_3V3`) |
|---|---|---|---|
| **Standby** | `CH_ENABLE` = 0, or no enabled channel has non-zero `CUR` and `DUTY`, and no transaction is in progress | CPU in power-down. Timers, ADC and every analog reference off. Every gate-path output driven to off. TWI address match armed as the wake source. No pin left floating that could bias another part | **≤ 2 mA** |
| **Active** | any channel meets §5.3 conditions 1–4 | PWM running; temperature sampled per §6 | **≤ 25 mA** |

The budget is per module, not per U1, so it includes U3's quiescent current, any thermistor bias
under `OI-FWTILE-01` and anything else on `VCC_3V3`. U1's firmware obligation is its own share and
the pin states that keep the rest quiet. **The 80-tile figure (~1.0 W) holds only if standby is the
default**: an idle tile that stays active, even at the retired 50 mA, makes the whole-vault lattice
infeasible (§8.3). **HT-DRC-12** closes on a bench measurement of a populated module in each state,
not on this table.

---

## 11. Reset, brown-out, watchdog, programming

| Rule | What fails without it |
|---|---|
| **Every register resets to off** (§5.2 column *Reset*) on power-up, brown-out, watchdog and `SOFT_RESET` | A tile that restarted with its last setpoints would resume emission the hub did not command |
| **Brown-out detection is enabled by fuse, at a level inside U1's rated range at its clock** | `VLED` mates before `VCC_3V3` (§7.3 groups 2 and 3), and extraction reverses that. So every insertion and removal takes U1 through a slow supply edge with the emitter rail live. Execution below the rated voltage can drive a gate or write a torn odometer slot |
| **The watchdog is enabled by fuse, not by firmware**, and is serviced only from the main loop, never from an interrupt | A wedged U1 would otherwise hold its last gate state indefinitely. U4 bounds it at 50 % duty, but only once it is built (§4) |
| **`SOFT_RESET` is equivalent to a watchdog reset** for registers and `RESET_SEEN`. It does not re-arm the mate latch (§8.2) | See §8.2 |
| **No field update path.** U1 is programmed over UPDI at manufacture, then locked | The tile firmware is fixed for the life of the tile unless one is specified (`OI-FWTILE-09`). `FW_VERSION` lets the hub record which image each tile carries |

**The gate state while U1 is unpowered or in reset is a circuit property, not a firmware one
(`OI-FWTILE-04`).** During a reset U1's pins are high-impedance, and during insertion U1 has no
supply while `VLED` is already present (§7.3). Each gate, and U4's input, needs a defined
non-conducting state that holds without U1, such as a pull-down. §6.2 shows none. No firmware rule
can cover this interval, because no firmware is running in it.

---

## 12. What U1 needs from the circuit

**Pins.** §6.2 names U1 as *"ATtiny426/427-class, SOIC-8/SOT-23-8"*. That is inconsistent: the
8-pin tinyAVRs are the 0/1-series (ATtiny402/412), and the ATtiny426 is a 20-pin part. This
firmware needs:

| Function | Pins | Condition |
|---|---|---|
| `SDA`, `SCL` | 2 | always |
| `SYNC` in | 1 | always (REQ-EMI-03) |
| `ALERT#` out, open-drain | 1 | always |
| Gate drive CH_A, CH_B, CH_C | 3 | always (CH_C unused on T1-A) |
| Current setpoint CH_A, CH_B, CH_C | 3 | if `OI-FWTILE-02` puts the setpoint in U1 |
| Sense-current ADC inputs | 3 | if `OI-FWTILE-05` routes them to U1 |
| Local temperature ADC input | 1 | if `OI-FWTILE-01` gives U1 one |
| UPDI | 1 | always |
| **Total I/O** | **8 minimum, 15 with every condition met** | |

An 8-pin part has 6 I/O. So **U1 is a 20-pin-class tinyAVR 2-series (ATtiny426/826-class), and
§6.2's package column must change** (`OI-FWTILE-03`). A 14-pin ATtiny424 carries the minimum set and
fails the full one. The rigidizer has room: 22 × 14 mm (§6.3).

**Memory.** EEPROM is 128 bytes, all of it the odometer (`NP-MOD-ID-001` §5.2). The identity block
needs up to 142 bytes of protected flash. Whether the application fits a 4 KB part, or needs the
8 KB variant, is unknown until it is written. Either is pin-compatible.

**Part facts this document relies on and has not re-read** (confirm against DS40002311A under
`OI-FWTILE-03`):
- TWI target wake from power-down on address match;
- a second hardware address match;
- a collision flag while transmitting;
- no DAC on the 2-series (hence a PWM setpoint);
- the user-row size;
- flash and EEPROM sizes;
- the brown-out levels.

---

## 13. Deliberate absences

Each of these would be easy to add. None is added, because nothing requires it (CLAUDE.md §18).

| Not specified | Why not |
|---|---|
| **A bus-silence timeout that stops emission** | A hub that stops talking is covered twice, in hardware. SW-01 cuts `VLED` on heartbeat loss in < 50 ms, and session end drops the cranial enable. A tile-side timeout would need a figure that nothing derives, and it would stop tiles during a legitimate quiet window (REQ-EMI-03) |
| **PD readback registers** | D-4 was not adopted (§3) |
| **Per-string control** | Per-string switching is safe only if each string is regulated separately (`OI-HEXTILE-27` reading (ii)). Until that is decided, a register for it would be an exposed hazard |
| **A firmware copy of `I_cap`, or a `CUR` clamp** | §5.4 |
| **Odometer fields U1 computes from its own gate time** | The hub owns every non-CRC field (§8.1). Two writers of one counter would disagree |
| **A duty floor or a `THERMAL` floor** | A low value costs availability, not safety |
| **A duty clamp, or any duty ceiling** | A clamp delivers a stimulus nobody authored. A ceiling needs emitter data U1 does not hold. Refusal happens before signing (§5.5) |
| **Per-channel slope scaling on the tile** | CH_A and CH_B already have separate `CUR` registers. That they receive one value is the compiler's (`hubCompiler.ts` writes `intensity` into both). Matching unequal mW-per-mA slopes belongs there, not in U1 (`OI-PBMCH-06`) |

---

## 14. Requirements

| ID | Requirement | What fails without it | Traces to |
|---|---|---|---|
| **REQ-FWTILE-01** | Serve the identity block of `NP-HEX-ZM-001` §4.1 at `0x40`, byte for byte, from a factory-written image including its CRC. Never compute or rewrite it | The hub's `inventory_fn` rejects the tile, or accepts a corrupted one whose CRC was recomputed | `NP-HEX-ZM-001` §4.1; R-12 |
| **REQ-FWTILE-02** | Drive a gate only under all six conditions of §5.3. `CUR` = 0 holds the gate off | "Off" on a base tile is undefined (`OI-PBMCH-01`); a corrupt tile or an unpopulated channel could be driven | NP-FEAS-PBMCH-001 §3.4, §6.2; `NP-HEX-ZM-001` §4.1 |
| **REQ-FWTILE-03** | Store every DUTY write as written (0–100 %), never clamped. Refuse a value above `0xC8`, keeping the previous value | A clamp delivers a different stimulus from the protocol, silently; an undefined code drives the gate to an undefined duty | CLAUDE.md §3 (Rev 59: duty is the protocol's; a ceiling refuses and never reshapes) |
| **REQ-FWTILE-04** | `THERMAL` resets to 62 and is never stored above 62 | A register write raises a tile past the 62 °C junction limit | CLAUDE.md §4.2; R-9 |
| **REQ-FWTILE-05** | At local temperature ≥ `THERMAL`: all gates off, latch, clear `CH_ENABLE`, assert `ALERT#`, with no bus transaction and no automatic resume. **Blocked on `OI-FWTILE-01`** | A tile heats past the limit while the bus is silent or the hub is slow | R-9; RISK-26 (on-module throttle); `NP-HW-HEXTILE-001` §6.5 |
| **REQ-FWTILE-06** | `ALERT#` low while any of `THERMAL`, `OCP_A/B/C` is latched | A tile fault waits for an 80-tile poll to be seen | `NP-HW-HEXTILE-001` §7.2 pin 12; `NP-DRV-SHELL-002` (ALERT# reaches the safety MCU without a transaction) |
| **REQ-FWTILE-07** | A fault clears the `CH_ENABLE` bits it reaches, and clearing a latch re-enables nothing | A cleared transient re-energises a channel the hub believes it disabled | `NP-FW-PBM1064-001` §5.4 (the hub re-enables explicitly) |
| **REQ-FWTILE-08** | Standby ≤ 2 mA and active ≤ 25 mA per module, with standby the default state | The 80-tile lattice exceeds its logic budget (~1.0 W) | `NP-HW-HEXTILE-001` §8.3 |
| **REQ-FWTILE-09** | Apply frequency, duty and current changes at a period boundary | A mid-period change runs one pulse at neither the old duty nor the new one | REQ-FWTILE-03 (the commanded duty is what is delivered) |
| **REQ-FWTILE-10** | No dithering. With `SYNC_EN` set, each period starts on `SYNC` | The PBM artifact is not a fixed, subtractable line | `NP-DRV-SHELL-002` REQ-EMI-03, REQ-EMI-04 |
| **REQ-FWTILE-11** | The odometer per §8: newest valid slot wins; U1 computes the CRC over UID ‖ payload and owns the mate count; a mate is written at most once per power-up; lower counters are refused | Part history is lost on a torn write, transplanted between tiles, overcounted, or rolled back | `NP-MOD-ID-001` §5.2, §5.3, §5.3.1, §6 |
| **REQ-FWTILE-12** | Brown-out detection and the watchdog are enabled by fuse, and every reset returns every register to off and sets `RESET_SEEN` | U1 runs erratically on the slow supply edge of every mate with `VLED` live; the hub integrates a dose a restarted tile is not delivering | `NP-HW-HEXTILE-001` §7.3 mating order; R-7 |
| **REQ-FWTILE-13** | Report `FW_VERSION` at `0x0E` | The software configuration of a fielded device cannot be established per tile | IEC 62304 §8.1.1 (identification of configuration items) |
| **REQ-FWTILE-14** | Mode S: answer at `0x30`. Mode D: per §9 items 1–5 when `OI-HUB-C15` adopts it | The hub's probe and inventory cannot reach the tile | `NP-HW-HEXTILE-001` D-7; `NP-HEX-ZM-001` §4.1 |
| **REQ-FWTILE-15** | Refuse an unlisted `PWM_FREQ` code. On `0x00` (CW), hold the gate on while §5.3 holds, with no carrier and `DUTY` not applied | A protocol runs at an undefined frequency; a CW protocol is delivered as a pulse train, which is a different stimulus | `NP-FW-PBM1064-001` §5.2; CLAUDE.md §3 (Rev 59) |

---

## 15. Verification

Nothing below has been run, because no tile firmware exists.

| Check | Method |
|---|---|
| REQ-FWTILE-01 | Host test of the register layer against `np_hexmap_inventory_tests`' reader: a factory image is accepted; a flipped byte fails the hub's CRC; a write to the region changes nothing |
| REQ-FWTILE-02 | Host test over every combination of the six conditions: the gate output is on in exactly one |
| REQ-FWTILE-03/-04 | Write `0x64` (50 %) and `0xC8` to each DUTY and read them back unchanged; write `0xC9` and read back the previous value; write `0xFF` to `THERMAL` and read back 62 |
| REQ-FWTILE-15 | Code `0x00` with `DUTY` = 0: the gate is continuously on for the whole capture, with no edges |
| REQ-FWTILE-05/-06/-07 | Host test with a simulated temperature input, then the GATE-2 bench with a heated tile and the bus disconnected: the gates go off, `ALERT#` is asserted, and clearing the latch leaves `CH_ENABLE` = 0 |
| REQ-FWTILE-08 | Bench: module current in each state, 3.3 V, over 25–62 °C (HT-DRC-12) |
| REQ-FWTILE-09 | Logic-analyser capture of a frequency change: no on-phase exceeds the duty of either setting |
| REQ-FWTILE-10 | Capture against `SYNC`: period starts are edge-locked; FFT of the gate shows no spreading |
| REQ-FWTILE-11 | `NP-MOD-ID-001` §10's two module tests, plus: power-fail at each byte of a commit (newest-valid recovery); a lower counter is refused; `SOFT_RESET` does not re-arm the mate latch |
| REQ-FWTILE-12 | Slow-ramp `VCC_3V3` with `VLED` live: no gate edge and no torn slot. Watchdog starvation → registers off, `RESET_SEEN` set |
| Falsification | Each guard above is removed in turn, and at least one check must fail. `NP-HEX-ZM-001` §4.1 set this bar for the hub side (10 mutants, each caught) |

---

## 16. Open items

| ID | Description | Owner | Blocking |
|---|---|---|---|
| **OI-FWTILE-01** | **U1 has no temperature input.** §6.2 RT1 says *"read by U1 ADC"*, and §7.2 routes the thermistor to pin 15, where the controller biases it and reads it through a mux. U1 cannot read a node that floats whenever the mux is elsewhere. Choose between a second on-tile thermistor on its own bias and one shared thermistor with a fixed on-tile bias (§6). The on-die sensor is not equivalent | EE + FW | **REQ-FWTILE-05**; RISK-26's on-module control; `NP-RISK-003` RISK-05 |
| **OI-FWTILE-02** | **`CUR` has no circuit.** U3 (§6.2, D-9) has a fixed reference that sets `I_cap` and nothing below it, but the library needs ~5–90 mA on CH_A (`OI-HEXTILE-29`). Specify a U1 setpoint path that U3 clamps. On a part with no DAC that is a filtered PWM per channel. The setpoint is held steady while the gate pulses, so two figures must be derived: its ripple, which becomes current ripple within a pulse, and its settling after a `CUR` change, which must finish before the next period boundary (§5.6; 25 ms at 40 Hz). Close with `OI-HEXTILE-24` | EE + FW | analog intensity on every protocol; `OI-HEXTILE-24` |
| **OI-FWTILE-03** | **U1's package and part facts.** §6.2's SOIC-8/SOT-23-8 carries 6 I/O, and this firmware needs 8 to 15 (§12). Move §6.2 to a 20-pin-class part, and confirm the §12 part facts against DS40002311A. Size flash (4 KB or 8 KB) once the image exists | EE + FW | §6.2; rigidizer layout; §6.4 BOM |
| **OI-FWTILE-04** | **Gate state without U1.** During every insertion `VLED` is live before `VCC_3V3` (§7.3), and during a reset U1's pins are high-impedance. Each gate and U4's input need a non-conducting default that holds with U1 unpowered | EE | rigidizer schematic; `OI-HEXTILE-24` |
| **OI-FWTILE-05** | **What `OCP_x` detects under D-9.** U3 makes over-current a regulator fault, so the useful detections are *current while the gate is off* (a shorted FET, i.e. a stuck-on channel) and *current above `I_cap`'s tolerance* (U3 failed). Both need the sense nodes on U1's ADC and thresholds from `OI-HEXTILE-24`. If they are not routed, `OCP_x` reads 0 permanently, and a stuck-on channel is detected only by the hub from the PD reading of an undriven tile. Say which | EE + FW | STATUS bits 2–4; `np_pbm_drive_poll_status()` |
| ~~**OI-FWTILE-06**~~ | **RETIRED Rev 2, wrong premise.** It assumed CW needed a carrier because of the 25 % clamp. CW is the gate held on (§5.5), so there is no carrier to choose. The CW + `duty_cycle` ambiguity in the language stays with `OI-SESPWR-03` and `OI-HEXTILE-31`. *Original:* **CW carrier.** Code `0x00` holds the channel off until this sets a carrier frequency. The carrier places a spectral line that REQ-EMI-03/-04 must account for, and it waits on `OI-SESPWR-03` (what `frequency: 0` with a duty means) | — | — (retired) |
| **OI-FWTILE-07** | **Mode D addressing.** Specify the discovery and assignment exchange if `OI-HUB-C15` adopts D-7's shared segments, against §9 items 1–5 | FW (hub + tile) | `OI-HUB-C15`; HT-DRC-11 |
| **OI-FWTILE-08** | **The hub's staged throttle sits at or above the tile's cut.** `NP-FW-PBM1064-001` §7.4 stages CH_C, then CH_B, from 62 °C, and the tile turns everything off at 62 °C. Restate the staging below 62 °C, or retire it | FW (hub) | `NP-FW-PBM1064-001` §7.4 |
| **OI-FWTILE-09** | **No field update path for tile firmware.** A defect in SW-04 is fixed only by replacing or bench-reflashing each tile. Decide whether that is acceptable for a Class B item fitted up to 80 times per device, or whether a hub-driven update is needed (IEC 62304 §6) | FW + Quality | SW-04 maintenance plan |

---

## 17. Revision history

| Rev | Date | Author | Description |
|---|---|---|---|
| 1 | 2026-09-28 | NeurOne Firmware Engineering | First issue; closes `NP-HW-HEXTILE-001` `OI-HEXTILE-07` as a specification item. It extends the `NP-FW-PBM1064-001` §5.1 map without moving an address. Added: FW_VERSION, CMD, CMD_STATUS, the identity window at `0x40` and the odometer window at `0xE0`. No PD readback registers are defined, because D-4 was not adopted. It defines the six-condition gate rule, under which `CUR` = 0 holds the gate off (`OI-PBMCH-01`, firmware half). The tile clamps duty to 25 % in every mode, CW included (`OI-HEXTILE-25`(c) decided for the tile). `CUR` above `I_cap` saturates in U3 (`OI-HEXTILE-25`(d)). Also specified: the local over-temperature response, latch and `ALERT#` semantics, identity storage and the boot self-check, the odometer write rules, and the MODID-4a mate latch. Modes S and D addressing, the power states and the reset rules round it out. The firmware is registered as SW-04, Class B. Nine open items are raised, `OI-FWTILE-01`…`-09`. No code, no part, nothing measured |
| 2 | 2026-09-28 | NeurOne Firmware Engineering | **Duty and CW are delivered as commanded (principal direction, CLAUDE.md Rev 59).** Rev 1's 25 % clamp is withdrawn: it applied to CW, so CW protocols were delivered as a pulse train or not at all. DUTY is stored as written, 0–100 %, and a value above 100 % is refused. Code `0x00` is the gate held on, with `DUTY` not applied. U1 enforces no duty ceiling, because legality depends on emitter data it does not hold, and a clamp reshapes the stimulus. §5.5 records where refusal happens instead, and what is not yet built (`OI-HEXTILE-30`, `-31`). `OI-HEXTILE-25`(c) re-answered. REQ-FWTILE-03, -09 and -15 rewritten. `OI-FWTILE-06` retired. No code |
