# Independent 660 nm / 808 nm PBM Channel Control — Costed Design Study

**Project:** NeurOne
**Document:** NP-FEAS-PBMCH-001
**Revision:** 1
**Date:** 2026-08-26
**Status:** EXPLORATORY — costed feasibility study. Creates no locked decision and no design input. Feeds a go/no-go on making CH_A (660 nm) and CH_B (808 nm) independently commandable end to end.
**Effective Date:** —
**Author:** NeurOne Systems Engineering
**Approved By:** — (pending design review)
**References:** NP-HW-HEXTILE-001 Rev 8 (§4.2 wavelength allocation, §4.3 irradiance, §6.2 driver topology, §6.4 driver BOM, §6.5 firmware boundary, §7.2 pinout, §8.1 VLED rail, §9 concurrency, D-3/D-5/D-6, OI-HEXTILE-02/06/07/09/20/21); NP-DRV-SHELL-002 Rev 4 (§5.1.4 pin table, §5.1.5 `VLED+` count, REQ-SKT-01, REQ-EMI-06/07); NP-HW-HUB-001 Rev 6; NP-FW-PBM1064-001 Rev 4 (§5.1 register map, §6.2 dose computation, §6.4 aggregate ceiling, §6.5 per-wavelength dose limits); NP-NPPS-REF-001 Rev 14 (§1.6, §4.1, Revs 5/8/12 divergence record); NP-SES-PWR-001 Rev 1; NP-PWR-BUDGET-001 Rev 3; NP-COST-001 Rev 2 (§6, §8, OI-COST-10); NP-OPT-PSF-001 Rev 1; NP-CONV-001 Rev 6; `docs/pbm_neuro_protocols.md`; `docs/status/pending-decisions.md`; CLAUDE.md §3 (modalities 1–2), §4.2, §4.5
**Related Issues:** RISK-03 (GitHub Issue #5) — the irradiance-ceiling regulatory opinion this study's §8 depends on and does not have
**Gate:** — (no gate of its own; §4 finds this study does **not** touch REG-1, MECH-2 or socket tooling)
**IEC 62304 Class:** — (study document). The change it costs would land **SW-02 Class B**; §8 states the one construction that would move it to Class C and recommends against it.
**Supersedes:** None — first issue
**Parent Document:** NP-HW-HEXTILE-001

---

> **⚠ READ FIRST — the headline result, and the premise correction that produces it.**
>
> **The question was expected to be a hardware question. It is not.** Every layer below the
> language is already per-channel: the tile carries a separate FET and a separate sense resistor
> per channel (`NP-HW-HEXTILE-001` §6.2), the two channels sit on **series strings of different
> length** so a shared current sink was never architecturally available (§8.1), the on-module
> register map has independent `CUR_A`/`CUR_B`, `DUTY_A`/`DUTY_B` and `PWM_FREQ_A`/`PWM_FREQ_B`
> (`NP-FW-PBM1064-001` §5.1), and the hub wire struct has carried separate `cur_a` and `cur_b`
> since it was written (`firmware/hub_control/include/np_hub_types.h:207-208`).
>
> **The collapse is one line of TypeScript.** `app/web/src/lib/hubCompiler.ts:557` writes
> `new Uint8Array([fc, duty, cur, cur])` — the *same* register value into both channels — because
> NPPS has one `intensity` field per PBM block and no per-channel form.
>
> **BOM delta: $0.00 per tile. Contact delta: 0 positions. Wire-format delta: 0 bytes.** The cost
> is language, five runtimes, per-channel dose accounting, and a per-channel restatement of R-5.
> **Estimated 12–19 engineering days for the recommended option**, with two caveats that are not
> engineering days: §8's R-5 restatement is blocked on an opinion nobody has commissioned, and
> §3.4's "what does a zero current setpoint mean" question cannot be answered until
> `OI-HEXTILE-02` selects an emitter.
>
> **What it buys, measured not asserted (§7).** Eight shipped clinical protocols are authored
> against trials that used a **single NIR wavelength** and currently deliver 660 nm alongside at
> the same commanded intensity. That unwanted channel is **exactly 50.0 % of emitted optical
> power** and **56.8 % of tile electrical power**. In two of the eight it alone delivers
> **≥60 J/cm²** — at or over R-7's entire per-session 660 nm dose limit. Turning it off returns
> **two over-budget protocols to single-pass operation** and roughly **halves session wall-clock**
> for the rest.

---

## 1. Scope and the question

This study answers one question: **what would it cost to make the 660 nm and 808 nm PBM channels
independently controllable, end to end?** "End to end" means from the `.npps` file an author writes
to the current that flows in an emitter string, including everything in between that would have to
change or be re-verified.

It costs six layers, in §3–§8. Each states what changes, what it costs, and what it depends on.
§7 quantifies the benefit. §9 assesses three interactions. §10 is the costed options table and the
recommendation.

**Out of scope, stated so the boundary is visible:**

- **CH_C (1064 nm) is not re-opened.** It already has independent control on both the wire
  (`ch_mask`, `cur_c`) and in the language (`wavelength: "1064nm"`). Its problem is a 9× emitter
  irradiance wall, `OI-HEXTILE-21`, which no control change touches (§9.3).
- **No number here sets a price, a contact count or a tooling feature.** §4 finds this change is
  contact-neutral, which is the one result that could have made it tooling-blocking.
- **This document changes no code, no protocol and no other specification.** Its outputs are the
  options table in §10, the decisions in §11 and the open items in §12.

---

## 2. The premise, verified against primary sources

An earlier statement in this repository said the T1-A tile *"drives 660 nm and 808 nm from one
current register."* **That is wrong.** It has been corrected in the protocol files that carried it
(they now say the constraint is *"in the LANGUAGE AND COMPILER, not the wire"*), and this section
re-establishes the real picture from primary sources rather than inheriting either version.

### 2.1 What is already per-channel — five independent places

| # | Layer | Evidence | Per-channel? |
|---|---|---|---|
| 1 | **Emitter strings** | `NP-HW-HEXTILE-001` §8.1: *"11 series 660 nm emitters (11 × 2.10 = 23.1 V) or 14 series 808 nm (14 × 1.60 = 22.4 V)"*; worked example *"CH_A 44 = 4 strings × 11; CH_B 42 = 3 × 14"* | **Yes — necessarily.** Different V_f, different string length, different string count. One sink cannot regulate both |
| 2 | **Driver topology** | §6.2 table: **Q1–Q3 low-side N-MOSFET *per channel*; R1–R3 current sense *per channel*;** U1 is an *"I2C slave MCU, **3× PWM**, ADC"*. *"T1-A fits the identical assembly with Q3, R3, and the CH_C string omitted"* → T1-A has Q1/R1 and Q2/R2 | **Yes** |
| 3 | **On-module register map** | `NP-FW-PBM1064-001` §5.1: `0x02 CUR_A`, `0x03 CUR_B`, `0x05 PWM_FREQ_A`, `0x06 PWM_FREQ_B`, `0x08 DUTY_A`, `0x09 DUTY_B`, `0x01 CH_ENABLE` bit0/bit1 | **Yes** — and on *three* axes, not one: amplitude, duty and PWM frequency are all independently addressable per channel |
| 4 | **Hub wire structs** | `np_hub_types.h:207-208` `uint8_t cur_a; /* 660nm */` `uint8_t cur_b; /* 808nm */`; `:216-219` smart adds `cur_c` + `ch_mask`; `:228-229` intranasal `cur_660`/`cur_808` | **Yes** |
| 5 | **Documented architecture** | `docs/ABBREVIATIONS.md` (NP-FW-PBM1064-001 entry): *"ATtiny402 I2C slave + Infineon IRLML6344 N-FETs for **independent channel drive** (660nm CH_A, 808nm CH_B, 1064nm CH_C)"* | **Yes** |

**Item 1 is the decisive one and it is worth stating alone.** The two channels are on series
strings of *different length* — 11 × 660 nm against 14 × 808 nm at the assumed V_f — because
`NP-HW-HEXTILE-001` §8.1.1's rule `N · V_f ≥ 22.4 V` resolves to a different N for each channel.
Two strings of different length cannot share one constant-current sink: the sink would have to hold
one string in regulation while the other sits below its dropout. **A shared sink was never an
available design, so "make them independent" is not a change to the hardware — it is a description
of what the hardware already is.**

### 2.2 Where the collapse actually is — one line

```
app/web/src/lib/hubCompiler.ts:539-560
  const cur = intensityReg(p.intensityPercent);      // :546  ONE value
  ...
  params = new Uint8Array([fc, duty, cur, cur, cur, chMask]);   // :554  smart
  params = new Uint8Array([fc, duty, cur, cur]);                // :557  base
```

`intensityReg()` (`:470-472`) maps 0–100 % to a 0–255 register. **One percentage in, the same byte
written into `cur_a` and `cur_b`.** `encodePBMIntranasal` (`:562-573`) does the same into `cur_660`
and `cur_808`.

The upstream cause is the type: `PBMTranscranialParams` (`app/web/src/types/protocol.ts:156-175`)
carries a single `intensityPercent: number` and a `wavelength` enum with exactly three members —
`'660_808nm' | '1064nm' | '660_808_1064nm'` — **none of which names one of the two base channels
alone.**

### 2.3 The genuinely open part, stated as uncertainty

The picture above is established as *architecture*, not as a released specification. Three
qualifications, none of which changes the answer but all of which bound it:

1. **`NP-HW-HEXTILE-001` §10 says of its own decisions: *"None is locked; all are proposals for
   design review."*** D-3 (on-module driver on every tile) is the exception it marks
   **irreversible**, and D-3 is what puts Q1–Q3 and R1–R3 on the tile. So the per-channel topology
   rests on the one decision in that table that cannot be taken back.
2. **`OI-HEXTILE-07` — the on-module driver firmware specification — has not been written.** §6.5
   says it *"implements the register map already defined in `NP-FW-PBM1064-001` Rev 1 §5.1
   (registers 0x00–0x0D)"*, and that document's own supersession banner lists *"the three-channel
   driver register map **concept** (§5.1 …) … addressing-independent"* as **likely still-reusable**.
   *Likely still-reusable* is not *specified*. **The T1-A register map is inherited by intent, not
   by publication**, and `OI-HEXTILE-07` is the item that publishes it.
3. **`OI-HEXTILE-02` has selected no emitter.** §4.3's V_f (2.10 V / 1.60 V) and radiant flux
   (95 mW each) are *"design targets the eventual part must meet, not datasheet values."* Every
   power, irradiance and split figure in this study inherits that. **What does not depend on it:**
   that the two channels have *different* V_f, which is a physics fact about 660 nm versus 808 nm
   junctions and is what forces separate strings regardless of which parts are chosen.

**Finding (F-1): the hardware question the brief expected to be open is closed by D-3 and §8.1's
string arithmetic. What is open is the firmware *specification* of a capability the hardware
already has.**

---

## 3. Layer 1 — Hardware / tile

### 3.1 What changes

**Nothing.** Separate constant-current sinks per channel are already the topology (§2.1 items 1–2).
There is no "shared sink" variant to migrate from.

### 3.2 BOM delta

| Item | Per tile | At 20 tiles | At 30 tiles | At 80 tiles |
|---|---|---|---|---|
| Additional FET | $0.00 — Q1/Q2 already fitted per `NP-HW-HEXTILE-001` §6.2 | $0.00 | $0.00 | $0.00 |
| Additional sense resistor | $0.00 — R1/R2 already fitted | $0.00 | $0.00 | $0.00 |
| Additional MCU PWM channel | $0.00 — U1 is specified with 3× PWM and T1-A uses 2 | $0.00 | $0.00 | $0.00 |
| **Total** | **$0.00** | **$0.00** | **$0.00** | **$0.00** |

For scale, the line this delta is *not*: §6.4 puts driver + metering at **$11.53/tile**, of which
**~$10.00 is the two InGaAs photodiodes** — *"at 80 populated sockets this is ~$920 per headset."*

### 3.3 Why a zero BOM delta still matters commercially

`NP-COST-001` finds **every T1 configuration is gross-margin negative** — Home Standard at
$897–959 BOM against $849 retail, **−41 % to −51 %** — and §6 of that document runs
`OI-HEXTILE-06`'s three options and concludes **none of them, alone or combined, restores a
positive T1 margin.** `OI-COST-10` makes `OI-HEXTILE-06` a precondition on setting any retail
price.

**Any BOM delta proposed today lands in a live cost argument.** That this one is zero is not a
convenience; it is what keeps this change outside `OI-HEXTILE-06`'s decision entirely. Independent
channel control neither helps nor hurts the margin case, and can therefore be sequenced
independently of a principal decision that everything else in the PBM stack is waiting on.

**One second-order interaction, in the *favourable* direction.** `OI-HEXTILE-06` option 1 (populate
20–30 tiles rather than 80) is opposed by `NP-PWR-BUDGET-001` §3.6's whole-vault argument, which
wants *more* sockets populated. §7.3 shows single-channel operation **more than doubles** the tile
count that fits a given power budget. It does not resolve the opposition — the two pressures in
`OI-HEXTILE-06` are about BOM against capability, not about watts — but it makes the capability
side cheaper in watts than the cost model assumes.

### 3.4 The one hardware-adjacent thing that is genuinely not established

**What does a current setpoint of zero mean?** `NP-FW-PBM1064-001` §5.1 defines `CUR_A` as *"8-bit,
0–255 → 0–180 mA"*, so 0 encodes 0 mA. But:

- **`np_mod_pbm_base_params_t` has no `ch_mask`.** The smart struct has one; the base struct is
  four bytes and carries no channel-enable at all (`np_hub_types.h:205-209`). On a base tile,
  **`cur_a = 0` is the only available expression of "off"**, and the hub path
  `np_mod_pbm_hal_pwm_set(slot, cur_a, cur_b, freq_code, duty)`
  (`firmware/hub_control/modules/np_mod_pbm.c:37,222`) has nowhere to put an enable bit.
- **Whether a zero setpoint produces zero emission is a part property nobody can check yet.**
  Leakage through a low-side FET into a series string, and the *minimum usable* drive below which
  a string does not conduct at all, are both emitter- and FET-dependent. `OI-HEXTILE-02` has
  selected no emitter. This is the same unknown the absolute-irradiance work records as the `min`
  term of its module-type map.

**Two consequences.** (i) If "off" must be *provably* off rather than *nominally* zero, the base
struct needs a `ch_mask` byte and that is a **wire-format change** — the only one this study
identifies. §5.1 costs both readings. (ii) `OI-HEXTILE-07` must state the semantics of setpoint 0
explicitly rather than leaving it to the encoding table. Recorded as **OI-PBMCH-01**.

---

## 4. Layer 2 — Socket / FPC / contacts

**This is the layer the brief correctly identified as highest-risk, and it is clean. The answer is
zero additional contacts, and the reason is structural rather than lucky.**

### 4.1 Why independent drive needs no contact

`NP-HW-HEXTILE-001` D-3 moved the driver on-module precisely so that **drive current stops crossing
the socket**. §6.1 states the arithmetic that forced it: hub-side drivers would have meant
*"80 sockets × 3 channels = 240 hub-side constant-current driver channels"* and
*"~1,280 power conductors"*, and *"none of that is buildable."*

What crosses the socket after D-3 is a **DC bus**, not a drive:

| Pin | Signal | What it carries | Sensitive to per-channel control? |
|---|---|---|---|
| 1–3 | `VLED` ×3 | 24 V bus, **shared by both channels** (D-6, §8.1) | **No** — one rail feeds both strings whatever the per-channel setpoints are |
| 4–6 | `PGND` ×3 | LED return, paired 1:1 with `VLED` for REQ-EMI-06 broadside cancellation | **No** |
| 9–10 | `SDA`/`SCL` | I2C, 400 kHz — **carries the commands** | **No** — `CUR_A` and `CUR_B` are two register writes on a bus that already exists |
| 11 | `SYNC` | broadcast pulse-phase reference (REQ-EMI-03) | **No** — see §4.3 |
| 12 | `ALERT#` | wired-OR fault/thermal | No |
| 13–15 | `PD1_K`, `PD2_K`, `NTC` | analog sense to the cluster controller | **See §5.2** — not a *count* question |

**The `VLED` contact count is the load-bearing number** — D-5 calls it *"load-bearing and now
tooling-blocking"*, and `NP-DRV-SHELL-002` §5.1.5 derives 3 from the rule *"`VLED+` is sized so the
loss of any one contact still leaves ≥2× derating."* That rule is driven by **peak per-tile
current**, 1.04 A at 24 V for 25.0 W.

**Per-channel control cannot increase peak current.** Both channels at full drive is already the
worst case the count is sized against; independent control only makes *lower* combinations
expressible. **The count derivation is untouched in the direction that matters.**

### 4.2 Finding: the change is contact-neutral, therefore not tooling-blocking

**Finding (F-2): independent per-channel control adds zero socket positions and does not perturb
the D-5 / D-6 / §5.1.5 derivation chain. It does not gate socket tooling and need not be sequenced
against MECH-2 or REG-1.**

This is worth stating explicitly because the *opposite* is true of the thing this change is often
confused with. `OI-HEXTILE-20` **does** move the contact count: whether R-5's 600 mW/cm² aggregate
ceiling binds T1-A sets the per-tile peak at **25.0 W or ~18.6 W**, which sets rail current
(1.04 A → 0.78 A), which sets per-pin derating, which set `VLED` at 3. §8.2 shows per-channel
control changes *how R-5 is stated* — but the peak it is applied to is unchanged, so `OI-HEXTILE-20`
resolves the same way with or without this study's change.

### 4.3 One thing to verify rather than assume — `SYNC` and per-channel phase

`NP-DRV-SHELL-002` **REQ-EMI-03** requires PBM pulse phase to be deterministic and phase-locked to
the EEG/fluxgate sample frame, and **REQ-EMI-04** prohibits dithered PWM *"precisely so the artifact
stays a subtractable known line."* Pin 11 `SYNC` is one broadcast reference.

The register map already allows `PWM_FREQ_A ≠ PWM_FREQ_B` and `DUTY_A ≠ DUTY_B`, so **two
independently-phased PBM lines inside the Faraday envelope is a configuration the hardware can
already be commanded into today** — it is not created by this change. But this change makes it
*expressible from a protocol file*, which turns a latent configuration into a reachable one.

**This study does not assert that per-channel phase is EMI-safe.** The subtractable-artifact
argument is written for one line; whether it survives two at different frequencies is a question for
the EMI owner. **The recommended language (§6) deliberately does not expose per-channel frequency
or duty** — only per-channel *intensity* — which keeps both channels on one `freq_code` and one
`duty` byte and leaves REQ-EMI-03/04 exactly as they are. Recorded as **OI-PBMCH-02**.

---

## 5. Layer 3 — Firmware

### 5.1 Wire format — probably none, and the fork is §3.4's

| Reading | Change | Cost |
|---|---|---|
| **"Off" = `cur_a = 0`** | **None.** `np_mod_pbm_base_params_t` already carries both registers; the compiler writes two different bytes instead of two identical ones | **0 bytes, 0 firmware LOC** |
| **"Off" must be enable-gated** | Add `ch_mask` to the base struct (4 → 5 bytes), and a `CH_ENABLE` write to `np_mod_pbm_hal_pwm_set()` | Struct + HAL signature + one hub call site + the smart/base length discriminator at `np_mod_pbm.c:181,215`. **~1–2 days.** Note `np_mod_pbm_control()` selects the struct **by length** (`len >= sizeof(...)`), so growing the base struct to 5 bytes is safe only while the smart struct stays 6 — verify, do not assume |

**Recommendation: take the no-change reading in phase 1**, and route the semantics question to
`OI-HEXTILE-07` (**OI-PBMCH-01**) rather than pre-emptively growing a Class B wire struct against an
unselected part.

**One tidy-up the change makes visible, not a defect it creates.** `hubCompiler.ts:554` writes
`cur_a = cur_b = cur_c = cur` even when `chMask = 0x04` (1064-only), so current setpoints are
written for channels the mask disables. Harmless today; under per-channel intent it is a
contradiction in the emitted frame and should be made consistent.

### 5.2 Dose metering — this is the real firmware cost, and it is a pre-existing defect

The brief asks whether per-channel dose accounting exists or is assumed. **It is assumed, and the
assumption is unsound today.**

`np_pbm1064_dose_tick()` (`firmware/pbm_1064nm/src/np_pbm1064_dose.c:95-140`) loops over wavelength
index `w ∈ {660, 808, 1064}` and, **inside the loop**, reads:

```c
uint16_t pd1_raw = 0, pd2_raw = 0;
if (!np_pbm1064_hal_adc_read_pd(socket_id, 0, &pd1_raw)) { continue; }
if (!np_pbm1064_hal_adc_read_pd(socket_id, 1, &pd2_raw)) { continue; }
dose->pd1_counts[w] = (float)pd1_raw;
...
float irr = cal[w].K_PD1 * dose->pd1_counts[w] * (1.0f + ratio_adj);
dose->dose_J_cm2[w] += irr * 0.001f * tick_duration_s;
```

`np_pbm1064_hal_adc_read_pd(slot, pd_ch, out)` has **no wavelength argument**
(`np_pbm1064_hal.h:50`). The same broadband photocurrent is read once per wavelength and scaled by
three different `K_PD1` constants. §6.1 of `NP-FW-PBM1064-001` states the physical reason plainly:
*"PD1 and PD2 thus meter all three wavelengths simultaneously (InGaAs is broadband; per-wavelength
dose requires calibrated K coefficients)."*

**What that means when both channels run:** PD1 sees the **sum** of 660 nm and 808 nm emission, and
the code attributes that whole sum, independently, to each wavelength. Per-wavelength dose is a
*model*, not a measurement.

**Why per-channel control makes this consequential rather than merely inelegant.** Today, with
`cur_a == cur_b` always, the two channels' contributions are in a fixed ratio and the model's error
is a systematic scale factor a calibration coefficient can partly absorb. **Under independent
control that stops being true**: command 808 nm at 74 % and 660 nm at 0 %, and the loop will still
report a 660 nm irradiance of `K_PD1[660] × (the 808-only photocurrent)` — a **non-zero dose for a
channel that is off**, accumulating against R-7's 60 J/cm² limit, and feeding
`np_pbm1064_dose_aggregate_irradiance()` (`:146-155`), which sums all three.

**Three ways to fix it, costed:**

| Option | Method | Cost | Fidelity |
|---|---|---|---|
| **D-a — command-gated attribution** | Attribute PD1 to channels in proportion to their *commanded* drive; a channel commanded to 0 gets 0 | **~2–3 days.** Pure firmware; no hardware, no calibration change | Restores the invariant *"off means zero dose"*. Does not make per-wavelength dose a measurement — it remains a model, correctly zeroed |
| **D-b — time-multiplexed sampling** | Sequence the channels within the 100 ms dose tick so each PD read is attributable to one channel | **~5–8 days** + an EMI review, because it modulates emission at the tick rate against REQ-EMI-03/04 | Genuine per-channel measurement |
| **D-c — wavelength-selective detection** | Filtered or paired photodiodes | **Hardware.** Reopens `OI-HEXTILE-06`'s PD BOM — the ~$10/tile term | Genuine, and expensive |

**Recommendation: D-a, in the same change as the language.** It is small, it is the difference
between a dose figure that is wrong-but-modelled and one that is *self-evidently* wrong, and it is
what keeps CLAUDE.md §3's real-time J/cm² dose claim honest under per-channel operation.

**Do not lose sight of what the claim is worth.** `NP-HW-HEXTILE-001` §6.4 (Rev 7 note) records
that independent testing of a marketed 1070 nm helmet found a **−79 % gap between declared and
measured scalp power density**, and that dose metering is *"the structural answer to the failure
mode this product category is known for."* Shipping per-channel control without D-a would put a
per-channel dose number on screen that no measurement supports.

### 5.3 Per-wavelength dose limits and the throttle cascade

Both already exist per channel and both improve under this change:

- **`NP-FW-PBM1064-001` §6.5** — 660 nm 60 J/cm², 808 nm 60 J/cm², 1064 nm 36 J/cm²; on limit,
  *"channel disabled via I2C `CH_ENABLE` write … Session continues for remaining channels."* Already
  per-channel. **Note this is a smart-module path** — the base tile has no `CH_ENABLE` (§3.4), so
  the per-channel disable that §6.5 describes has **no implementation on T1-A**. That is a
  pre-existing gap this study surfaces, not one it creates (**OI-PBMCH-03**).
- **§6.4** — aggregate throttle cascade CH_C → CH_B → CH_A, *"a proportional reduction in duty."*
  Under per-channel control the cascade becomes both more meaningful and more contestable: see §8.2.

### 5.4 Firmware cost summary

| Item | Days | Class |
|---|---|---|
| Wire format (no-change reading) | 0 | — |
| D-a command-gated dose attribution + host tests | 2–3 | B |
| `np_mod_pbm.c` consistency tidy (currents for masked-off channels) | 0.5 | B |
| `OI-HEXTILE-07` register-map statement of setpoint-0 semantics | *documentation, not code* | — |
| **Subtotal** | **~3** | **B** |

---

## 6. Layer 4 — NPPS language

### 6.1 The three candidate forms

**(a) Per-channel block or sub-fields**

```
pbm_transcranial {
    channel 660nm { intensity: 0% }
    channel 808nm { intensity: 74.4% }
    frequency: 40Hz
    duty_cycle: 25%
    zones: ["Vault (excl. Occipital)"]
}
```

| | |
|---|---|
| **For** | Maximally expressive; extends naturally to CH_C; every channel's value is stated rather than implied |
| **Against** | A **new block form** inside a modality block — the grammar has no precedent for nesting a repeated named sub-block inside `pbm_transcranial`, so this is a real production change in `npps/grammar/npps.peggy` and a new node in four hand-written parsers. **`wavelength` becomes redundant and ambiguous**: is a block with only an `808nm` channel `wavelength: "660_808nm"` or not? Two ways to say the same thing is the defect class `NP-NPPS-REF-001` Rev 11 documents (*"the iOS and Android enums used one string for two different vocabularies"*) |
| **Migration** | Every one of the 22 shipped `pbm_transcranial` blocks either changes or acquires a defaulting rule |

**(b) Extended `wavelength` enum**

```
wavelength: "808nm"      # new
wavelength: "660nm"      # new
wavelength: "660_808nm"  # unchanged — both, at one intensity
```

| | |
|---|---|
| **For** | **No grammar change at all.** `wavelength` is already a quoted string field parsed by a `switch` in all four runtimes (`nppsParser.ts:1078`, `NPProtocolScripting.swift:944-949`, `NPPSParser.kt:737-742`). Two new enum members. The 22 shipped protocols are **untouched** — `"660_808nm"` keeps its exact meaning. Selection stays where selection already lives: `wavelength` is *already* the field that turns CH_C off, via `"660_808nm"` versus `"660_808_1064nm"` |
| **Against** | Cannot express *"660 at 20 %, 808 at 74 %"* — only on/off per channel. The enum grows combinatorially if CH_C is folded in (`"808_1064nm"`, `"660_1064nm"` …) |
| **Migration** | None. Additive |

**(c) Array-valued intensity**

```
intensity: [0%, 74.4%]
```

| | |
|---|---|
| **For** | Small grammar delta; expresses independent magnitudes |
| **Against** | **Positional.** `[0%, 74.4%]` means nothing without knowing the channel order, and the order differs by wavelength value — under `"660_808_1064nm"` it is three elements, under `"1064nm"` one. This is precisely the failure `NP-NPPS-REF-001` Rev 12 catalogues on the mobile lexers: a value that parses into something the author did not mean, with **no error**. A two-element list where a scalar was intended, or the elements transposed, is a **wrong-wavelength dosing** error that nothing catches |
| **Migration** | Every shipped protocol either changes or relies on a scalar-vs-array polymorphism |

### 6.2 Recommendation — (b), with (a) named as the successor if magnitudes are ever needed

**Adopt (b): extend the `wavelength` enum with `"660nm"` and `"808nm"`.**

Five reasons, in order of weight:

1. **It answers the actual need.** §7 shows the eight affected protocols want *one channel off*, not
   *two channels at different levels*. Every trial in `docs/pbm_neuro_protocols.md` §1, §2, §4, §5
   and §7 that states parameters states them for **a single wavelength**. No shipped protocol, and
   no trial in the evidence base, asks for an unequal two-channel mix.
2. **It is the cheapest change that is also the safest.** No grammar production changes, so
   `NP-NPPS-GRAM-001` needs no revision; no shipped protocol changes, so the 26 authored intensity
   values stay exactly as authored; nothing becomes positional, so nothing can be silently
   transposed.
3. **It reuses the mechanism that already exists for exactly this purpose.** `wavelength` already
   selects *which channels run* — that is what `"1064nm"` versus `"660_808_1064nm"` does, and
   `hubCompiler.ts:553` already turns it into a `ch_mask`. Options (a) and (c) add a **second**
   channel-selection mechanism beside it.
4. **It fails loudly on the mobile runtimes rather than quietly.** All four `wavelength` parsers
   currently fall through to `BASE_660_808NM` on an unrecognised value (`nppsParser` via the cast at
   `:1078`; `NPProtocolScripting.swift:949` `default:`; `NPPSParser.kt:742` `else ->`). **That
   default is itself a defect** and this change must fix it in the same commit — otherwise an
   `"808nm"` protocol opened on a runtime that has not shipped the enum yet **silently turns 660 nm
   back on**, which is the exact failure this whole exercise exists to remove. §7.1 costs it.
5. **It leaves (a) available.** If per-channel *magnitudes* are ever wanted, (a) can be added later
   as the general form with the enum values as shorthand. (c) forecloses nothing but earns nothing.

**One consequence to accept openly:** under (b), *"660 nm off"* and *"660 nm at 0 %"* are the same
statement, and §3.4 records that the hardware meaning of the second is not yet established. The
language is honest about this only if `OI-HEXTILE-07` states it. **This is the one place where
option (b) is weaker than option (a)** — (a) would let an author write `intensity: 0%` and mean it
literally. It is not weak enough to change the recommendation, because (a) has the same dependency
in the end: something still has to define what zero drive does at the emitter.

### 6.3 What else the language change touches

| Item | Change |
|---|---|
| `NP-NPPS-REF-001` §4.1 | Two enum members, and a statement of what each does and does not command. **Rev 14 → Rev 15** |
| §11 grammar summary, §12 dictionary | Enum list in two places |
| `protocolEligibility.ts:53` | `pbm_transcranial: { requires: [['led_660'], ['led_808']] }` — a **static per-modality** requirement that both wavelengths be present at every socket, with the comment *"a PBM zone needs both wavelengths at the same site, not 660 here and 808 there."* Under `"808nm"` the requirement becomes **wavelength-dependent**, i.e. a function of the protocol rather than a constant of the modality. This is the largest structural change in the language layer and it is in **web only** — no other runtime has an equivalent table |
| Zone `types:` filters | `NP-NPPS-REF-001` §8 lets a zone filter on element types including `led_660`; unaffected, but should be re-read against the new enum |

---

## 7. Layer 5 — four runtimes + Windows

### 7.1 Cost it as one change, because history says so

`NP-NPPS-REF-001`'s own revision history is the argument, and it is not anecdotal:

- **Rev 12(iv):** *"Android never lost the Rev 6 compound-identifier rule — that removal reached the
  web and Swift lexers and the shipped library, but not Kotlin, so Android alone still accepted bare
  `660_808nm`."*
- **Rev 12(v):** *"The Kotlin serializer wrote `wavelength` unquoted, i.e. NPPS text its own parser
  cannot read back."*
- **Rev 11:** *"both modes serialized to text neither parser could read back, silently resetting to
  the default mode on a round-trip."*
- **Rev 8, Rev 12(vi):** five retired field names survived in one serializer after being removed
  from its parser, so *"every one of those values was lost on reload."*

**Every one of those is the same defect: a field that exists on some runtimes and not others,
failing silently rather than loudly.** `wavelength` specifically has been the subject of three of
them. The change below must land as one commit across all five targets.

### 7.2 The five targets

| Target | Files | Change | Days |
|---|---|---|---|
| **Web** | `types/protocol.ts:171`, `lib/nppsParser.ts:1078`, `lib/nppsSerializer.ts:81`, `lib/hubCompiler.ts:539-560`, `lib/protocolEligibility.ts:53` | Enum + **the actual compiler fix** (`cur` → `curA`/`curB`) + wavelength-dependent eligibility + reject-unknown-enum | **3–4** |
| **iOS** | `Protocol/NPProtocolDefinition.swift:105-126`, `Protocol/NPProtocolScripting.swift:944-949` (read), `:1610` (write) | Enum cases + `default:` becomes an error, not a fallback | **1–2** |
| **Android** | `protocol/NPProtocolModels.kt:83-89`, `protocol/NPPSParser.kt:737-742`, `protocol/NPPSSerializer.kt:243` | Same; `else ->` becomes an error | **1–2** |
| **PEG grammar** | `npps/grammar/npps.peggy` | **No production change** under option (b) — `wavelength` is a quoted string. Comment only. `NP-NPPS-GRAM-001` needs no revision | **0** |
| **Windows** | `Protocol/NPProtocolDefinition.cs:25,29`, `Protocol/SessionProtocolCompiler.cs:42-53` | Enum + **see §7.3** | **1–2** |
| **Fixtures + goldens** | `npps/fixtures/all_t1_modalities.{npps,expected.json}`, per-runtime goldens, plus **one `error_*` negative fixture per runtime** asserting an unknown wavelength is refused | | **1–2** |
| | | **Total** | **7–12** |

### 7.3 A divergence this study found that the brief does not name

**`wavelength` never reaches the wire on iOS or Windows.** Both build a `PBMTranscranialConfig` /
`PbmTranscranialConfig` carrying `socketMask` / `Zones`, `frequencyHz`, `dutyCyclePercent`,
`durationSeconds` and `targetDoseJoules` — **and no wavelength field at all**
(`app/ios/NeurOne/Session/NPSessionProtocol+FromDefinition.swift:26-32`;
`app/windows/NeurOne/Protocol/SessionProtocolCompiler.cs:42-53`;
`app/ios/NeurOne/Session/SessionProtocol.swift:44-52`).

Only `hubCompiler.ts` compiles to the byte-level `np_mod_pbm_*_params_t` frames. **On iOS and
Windows a `wavelength: "1064nm"` protocol compiles today to the same session config as a
`"660_808nm"` one** — the wavelength selection that already exists is already being dropped on two
of five runtimes.

**Consequence for this change: adding `"808nm"` to the enum does nothing on iOS or Windows unless
the field is carried into the session protocol.** That is a pre-existing divergence of exactly the
Rev 11/12 class, it is larger than the change this study costs, and it is **not** included in the
7–12 days above. Recorded as **OI-PBMCH-04**. Whether it is fixed here or separately is a
sequencing decision; what must not happen is shipping the enum on web only and believing the
platform agrees.

**Related, and already recorded elsewhere:** `SessionProtocolCompiler.cs:50-52` computes
`TargetDoseJoules` as `duration × intensity/100 × 0.4` and **never multiplies by duty**, where iOS's
identical-looking line does the same and the Android/iOS *validators* and the simulator apply duty.
That is one of five mutually-disagreeing reference constants, and it is the absolute-irradiance
work's finding, not this study's; it is named here only because §7.2 touches the same file.

### 7.4 Simulator

`NP-NPPS-REF-001` Rev 14 §1.6 forbids any build-time cache of protocol content, and the simulator
now parses `.npps` at load through a bundle of the real web parser
(`simulator/js/vendor/npps-runtime.js`, rebuilt by `scripts/build-simulator-runtime.ts`). **So the
simulator inherits the web enum for free**, and the only work is a bundle rebuild. Its own dose
transform hardcodes a peak-irradiance constant and would show an 808-only protocol at the same
irradiance as a dual-channel one until that constant is made channel-aware — a small edit, folded
into the web line above.

---

## 8. Layer 6 — safety and IEC 62304

### 8.1 Which side of the Class B / Class C line

**Class B, and it should stay there.** The reasoning is the same one the absolute-irradiance work
records and it holds identically here:

- **The safety MCU owns the stimulation enable GPIO** (CLAUDE.md §4.2) and gates all cranial PBM
  with **one Class C policy bit**, `NP_SAFETY_EN_PBM_CRANIAL`
  (`firmware/safety_mcu/include/np_safety_protocol.h:97`), fanned out to 18 per-cluster Class B
  availability gates (`NP-HW-HEXTILE-001` D-8).
- **The safety MCU has no optical-irradiance limit of any kind.** Its only PBM barrier is thermal —
  `NP_NTC_CUTOFF_DEG_C 62` (`np_safety_config.h:111`). R-4's 400/200 mW/cm² is enforced **nowhere in
  Class C code**. R-5's 600 mW/cm² lives in Class B, as
  `NP_PBM1064_AGGREGATE_IRRADIANCE_MW_CM2` in `firmware/pbm_1064nm/`.
- **The credible harm from a per-channel command error is thermal**, and the mitigation is the
  Class C 62 °C junction throttle, which **measures the outcome** rather than trusting the commanded
  input and does not consult any of the software this change touches. IEC 62304 §4.3 permits
  classification at the lower level exactly on that structure.
- **A per-channel change can only reduce commanded optical power**, never increase it: both channels
  at full drive is already expressible today.

**Where it would become Class C:** if an *absolute per-channel irradiance ceiling* were added as the
only barrier between a protocol and an unsafe irradiance, it would be a safety function and would
belong on the safety MCU — new code in a ~1,600-line certified module. **Recommendation: do not.**
That is the same recommendation the absolute-irradiance work reaches, for the same reason, and doing
both at once mixes a Class B language change with a Class C addition in one change.

**One Class B item that must be right:** §5.2's D-a. A dose model that reports non-zero dose for a
disabled channel is a Class B correctness defect with a UHDR-visible output (dose per zone is a
CLAUDE.md §5.1 UHDR element). It is not a safety function — the 62 °C interlock still backstops —
but it is the item design review should look at hardest.

### 8.2 How R-5 would need restating — and the ground under it

**R-5 is *"three-channel aggregate ceiling 600 mW/cm²"* (`NP-HW-HEXTILE-001` §2), sourced to
`NP-FW-PBM1064-001` Rev 2 (OI-PBM-05).** Per-channel control changes what "aggregate" ranges over,
so the statement has to say what it sums.

| Reading | Statement | Consequence for T1-A |
|---|---|---|
| **R-5a — sum over *enabled* channels** | *"The sum of irradiance over the channels a protocol enables shall not exceed 600 mW/cm²"* | 808-only at full drive = **403 mW/cm²**, compliant with 33 % headroom. Both channels = **806**, non-compliant — which is `OI-HEXTILE-20` |
| **R-5b — sum over *populated* channels** | *"…over the channels the fitted tile carries"* | 808-only at full drive is **still 806** on a T1-A tile, because the 660 nm string exists whether or not it is driven. **Absurd**, but it is what a naive reading of "three-channel aggregate" gives once channels can be off |

**R-5a is the only defensible reading and it must be written down**, because today the text says
neither and the code implements a third thing again:
`np_pbm1064_dose_aggregate_irradiance()` (`np_pbm1064_dose.c:146-155`) sums
`irradiance_mW_cm2[w]` over every `w` whose dose limit has not been hit — i.e. over **modelled**
irradiances derived from one broadband PD read (§5.2). **Under D-a that sum becomes correct for a
disabled channel; without D-a, R-5a cannot be enforced even if it is stated**, because the runtime
cannot tell a disabled channel from an enabled one.

**The throttle cascade needs a companion sentence.** §6.4 throttles CH_C → CH_B → CH_A. Under
per-channel authoring, throttling CH_B on an 808-only protocol is throttling **the only therapeutic
channel**, while CH_A — off, contributing nothing — is protected by being last. The cascade order
was written when every channel was always on; it should be restated as *throttle in the given order
**among enabled channels***, which is a one-line change with no behavioural difference in the
all-on case.

### 8.3 The ceilings are not regulatory limits, and that is load-bearing here

**Neither R-4 nor R-5 is a regulatory ceiling. Both are NeurOne-chosen firmware governors awaiting
a RISK-03 counsel opinion that has never been commissioned.** The chain is traced end to end in
`docs/status/pending-decisions.md` §13.1a — **which is on branch
`claude/risk-03-counsel-engagement` and is not merged to `main` as of `b69d28b`**, so this study
cites it as an unmerged finding, not as published record:

| Step | What it says |
|---|---|
| `NP-HW-HEXTILE-001` §2 R-5 | 600 mW/cm², source `NP-FW-PBM1064-001` Rev 2 (OI-PBM-05) |
| `NP-FW-PBM1064-001` §6.4 | `PBM_AGGREGATE_IRRADIANCE_LIMIT_MW_CM2 = 600` — *"pending confirmation from RISK-03 regulatory opinion"* |
| `firmware/pbm_1064nm/include/np_pbm1064_config.h` | the constant, carrying the same *"pending RISK-03"* comment |
| `NP-REG-PBM1064-001` §4 | the brief **to** counsel. **Asks** about 600; does not answer it |
| `NP-RM-001` RISK-03 | *"not yet obtained"* |

**The chain terminates in a request that was never sent.** No IEC 62471 derivation, no FDA position,
no ICNIRP limit appears anywhere in it; 600 is exactly half of 3 × 400 with no derivation recorded,
and `NP-REG-PBM1064-001` Q4 asks counsel whether it should stay, **rise to 1,200**, or **fall** to
secure IEC 62471 Exempt Group classification.

**Two consequences for this study, and they point in opposite directions:**

- **Against sequencing R-5's restatement first:** rewriting a governor whose *value* is open in both
  directions is work that may have to be redone. §8.2's restatement is about **what the sum ranges
  over**, which is stable under any value — so it can proceed — but it should not be presented as
  settling R-5.
- **For doing the language change now:** per-channel control is one of the few things that makes
  R-5 *easier* to satisfy at any value it lands on. An 808-only protocol at 403 mW/cm² is compliant
  even if counsel moves the ceiling **down** to secure Exempt Group; a dual-channel one at 806 is
  not compliant at 600 and is barely compliant at 1,200.

---

## 9. What it buys — quantified, not assumed

### 9.1 The affected protocols

The brief names six. **This study finds eight**, and the discrepancy is a finding rather than a
quibble. Source-trial wavelengths are from `docs/pbm_neuro_protocols.md` §1, §2, §4, §5, §7;
protocol parameters are read from branch `claude/anxiety-split-by-trial` (which splits several
files by trial and is **not merged**), and are unmodified by this study.

| Protocol | Source trial | Trial λ | Single NIR? | Carries the fidelity-gap note? |
|---|---|---|---|---|
| `clinical-01-pbm-alzheimers-chun` | Chun 2026 | 808 nm | **Yes** | Yes |
| `clinical-01-pbm-alzheimers-wozniak` | Woźniak-Mitał 2026 | 850 nm | **Yes** | **No — gap not annotated** |
| `clinical-02-pbm-mci` | Papi 2022 | 850 nm | **Yes** | **No — gap not annotated** |
| `clinical-04-pbm-depression-cassano` | Cassano 2018 | 823 nm | **Yes** | Yes |
| `clinical-04-pbm-depression-schiffer` | Schiffer 2009 | 810 nm | **Yes** | Yes |
| `clinical-05-pbm-anxiety-maiello` | Maiello 2019 | 830 nm | **Yes** | Yes |
| `clinical-05-pbm-anxiety-wang` | Wang 2023 | 820 nm | **Yes** | Yes |
| `clinical-07-pbm-autism` | Fradkin 2024 | 850 nm | **Yes** | **No — gap not annotated, and this is the pediatric protocol** |
| `clinical-06-pbm-tbi` | Naeser 2011/2014 | **633 + 870 nm** | No — **counter-example** | n/a |
| `clinical-08-pbm-parkinsons` | §8 protocol | **810 + 634/660 nm** | No — **second counter-example** | n/a |
| `clinical-09-pbm-stroke-rehab` | §9 protocol | **810 (+633) nm** | No — **third counter-example** | n/a |

**Two corrections of record.** (i) The affected set is **eight, not six** — `clinical-02`,
`clinical-01-wozniak` and `clinical-07` are equally affected and are not annotated, and
`clinical-07` is the **pediatric** protocol, where `docs/pbm_neuro_protocols.md` §7 says
*"conservative dosing and tolerability titration essential."* (ii) There are **three**
counter-examples, not one: `clinical-08` and `clinical-09` also target protocols whose own stated
parameters are dual-wavelength (810 + 634/660 nm and 810 + 633 nm). **The change must therefore be
additive — the existing `"660_808nm"` value must keep working exactly as it does — which is a point
in favour of option (b) and against (a) and (c), both of which would have migrated these three too.**

### 9.2 What the unwanted channel actually costs

Derived from `NP-HW-HEXTILE-001` §4.2 (45 + 45 emitters on T1-A), §4.3 (V_f 2.10 V / 1.60 V, radiant
flux 95 mW each at 150 mA) and §4.3.1 (403 mW/cm² per channel at full drive), using the same
linear-in-intensity power model as `scripts/check-pbm-power.ts:130-131`.

**The split, at any drive level:**

| Quantity | CH_A 660 nm | CH_B 808 nm | CH_A share |
|---|---|---|---|
| Emitters | 45 | 45 | 50 % |
| Radiant flux at 150 mA | 4.275 W | 4.275 W | **exactly 50.0 %** |
| Electrical power at 150 mA | 45 × 2.10 V × 0.150 A = **14.175 W** | 45 × 1.60 V × 0.150 A = **10.800 W** | **56.8 %** |
| Irradiance at full drive | 403 mW/cm² | 403 mW/cm² | 50.0 % |

**Finding (F-3): the "roughly half the emitted optical power" claim is verified and is exactly half
by construction — the 45/45 allocation and the equal 95 mW flux target make it so. Electrically it
is worse than half: 56.8 %, because the 660 nm junction's forward voltage is 31 % higher.** Both
figures inherit `OI-HEXTILE-02` — no emitter is selected, and the equal-flux assumption in
particular is a design target, not a datasheet value. **What does not inherit it:** that 660 nm
costs *more* electrical power per emitted watt than 808 nm, which follows from bandgap physics and
holds for any parts that get chosen.

**Per protocol, as authored:**

| Protocol | Trial λ | I % | Mode | 660 W/tile | 660 W total | **660 nm J/cm² delivered** | vs R-7's 60 J/cm² |
|---|---|---|---|---|---|---|---|
| clinical-01 Alzheimer's (Chun) | 808 | 74.4 | 40 Hz | 2.64 | 187.2 | 49.5 | 82 % |
| clinical-01 Alzheimer's (Woźniak) | 850 | 30.0 | 40 Hz | 1.06 | 75.5 | 36.3 | 60 % |
| clinical-02 MCI (Papi) | 850 | 70.7 | 40 Hz | 2.51 | 92.7 | 42.7 | 71 % |
| clinical-04 Depression (Cassano) | 823 | 8.9 | CW | 1.26 | 46.7 | **64.6** | **108 %** |
| clinical-04 Depression (Schiffer) | 810 | 62.0 | 10 Hz | 2.20 | 81.3 | **60.0** | **100 %** |
| clinical-05 Anxiety (Maiello) | 830 | 7.4 | CW | 1.05 | 38.8 | 35.8 | 60 % |
| clinical-05 Anxiety (Wang) | 820 | 77.0 | 10 Hz | 2.73 | 101.0 | 32.6 | 54 % |
| clinical-07 Autism (Fradkin) | 850 | 20.0 | 40 Hz | 0.71 | 7.1 | 7.3 | 12 % |

**Finding (F-4): in two of the eight, the unwanted 660 nm channel alone reaches or exceeds R-7's
entire per-session 660 nm dose limit** — Cassano at 64.6 J/cm² (108 %) and Schiffer at exactly
60.0 J/cm² (100 %). Per `NP-FW-PBM1064-001` §6.5 that should disable CH_A mid-session; §5.3 notes
the base tile has no `CH_ENABLE` to disable it with. **A protocol reproducing an 823 nm trial is
delivering a full clinical dose of a wavelength the trial did not use, and tripping a firmware limit
to do it.**

Both CW rows carry `OI-SESPWR-03`'s CW-plus-duty ambiguity and are reported at the CW reading (the
higher figure), exactly as `scripts/check-pbm-power.ts` does.

### 9.3 What turning it off returns

Power model as above; **40 W available to emitters** (`NP-HW-HEXTILE-001` §9.1: R-10's 45–50 W peak
less ~6–8 W non-PBM overhead). "Groups" is the cascade count `NP-SES-PWR-001` uses; "wall" is
authored duration × groups.

| Protocol | Sockets | W/tile both → 808 | Total W both → 808 | Groups both → 808 | Wall-clock both → 808 |
|---|---|---|---|---|---|
| clinical-01 Alzheimer's (Chun) | 71 | 4.65 → 2.01 | 330 → 143 | 9 → 4 | 99m → **44m** |
| clinical-01 Alzheimer's (Woźniak) | 71 | 1.87 → 0.81 | 133 → 58 | 4 → 2 | 80m → **40m** |
| clinical-02 MCI (Papi) | 37 | 4.41 → 1.91 | 163 → 71 | 5 → 2 | 50m → **20m** |
| clinical-04 Depression (Cassano) | 37 | 2.22 → 0.96 | 82 → 36 | 3 → **1** | 90m → **30m** |
| clinical-04 Depression (Schiffer) | 37 | 3.87 → 1.67 | 143 → 62 | 4 → 2 | 64m → **32m** |
| clinical-05 Anxiety (Maiello) | 37 | 1.85 → 0.80 | 68 → 30 | 2 → **1** | 40m → **20m** |
| clinical-05 Anxiety (Wang) | 37 | 4.81 → 2.08 | 178 → 77 | 5 → 2 | 35m → **14m** |
| clinical-07 Autism (Fradkin) | 10 | 1.25 → 0.54 | 12 → 5 | 1 → 1 | 6m → 6m |

**Finding (F-5): two protocols move from over-budget to inside the 40 W envelope in a single pass**
— Cassano (82 W → 36 W) and Maiello (68 W → 30 W). Both are CW trials at low irradiance whose
authored fidelity depends on running the whole target area at once; cascading them across three or
two groups changes what the session *is*. **For the other six, session wall-clock roughly halves.**

**This does not fix `OI-HEXTILE-09` and must not be presented as fixing it.** `NP-SES-PWR-001`
audits **20** protocols and finds 17 over budget; this change moves **two** of them, and the audit's
own sequencing note says most of the over-budget condition is caused by lobe-scale zone targeting
(`OI-SESPWR-01`), *"so a governor built against the current library would reject 17 protocols when
the correct response to 15 of those is to fix the protocol, not the governor."* Per-channel control
is a **second, independent** ~2.3× on the same axis, not a substitute for the governor. The governor
is still required, still must be **in watts** (`NP-PWR-BUDGET-001` D-4), and is still blocked on
`OI-SESPWR-03`.

### 9.4 One benefit that is not about power at all

`docs/pbm_neuro_protocols.md`'s dosimetry lesson 2 states the mechanism: *"NIR (800–1080 nm)
penetrates; deep red (630–670 nm) mostly acts at scalp/systemically or intranasally. Transcranial
cortical targets use 810/1064 nm; **660 nm is an adjunct or intranasal/vascular channel**."*

**660 nm's proper home in this architecture is the intranasal probe**, where CLAUDE.md §3 modality 2
places it deliberately and where §1's Alzheimer's protocol calls for it explicitly (*"add 660 nm
intranasal"*). The transcranial collapse spends it on a path where the same document says it mostly
does not reach cortex. **The benefit is therefore evidence fidelity first and watts second**: eight
protocols currently deliver a wavelength their source trials did not use, to a depth that document
says it does not reach, at half the emitted optical power.

---

## 10. Interactions

### 10.1 `OI-HEXTILE-09` — the concurrency governor

Assessed in §9.3. **Helps by roughly 2.3× on per-tile draw; does not close the item.** The two are
independent and should not be sequenced against each other, with one exception: if a per-channel
form ships, the governor's per-tile draw input must be **per-channel**, or it will bill 808-only
protocols at the dual-channel rate and reject sessions that fit. Recorded as **OI-PBMCH-05**.

### 10.2 `OI-HEXTILE-21` — the 1064 nm irradiance shortfall

**No help, and the study should say so plainly.** `OI-HEXTILE-21` is an *emitter-efficiency wall*:
CH_C delivers 28 mW/cm² against the 250 mW/cm² its flagship protocol specifies, η_wp ≈ 4.8 % at
1064 nm, and R-6 caps drive at 120–180 mA for L70. *"Neither more watts nor more sites closes it."*
Turning off CH_A frees electrical power, and on a T1-C tile that is real headroom — but headroom is
not the binding constraint. **A wall that more watts cannot close is not closed by making watts
available.** The three responses `OI-HEXTILE-21` lists (better emitter, accept CW and long sessions,
or re-target the claim at the 1060–1080 nm Alzheimer's band) are unchanged by this study.

### 10.3 The move from `intensity_percent` to absolute `irradiance_mw_cm2`

This is a **live, principal-directed change**, planned but not landed. The interaction is
substantial and runs both ways.

**They are compatible and should be sequenced deliberately.**

| Aspect | Interaction |
|---|---|
| **Same files, same five runtimes** | Both change `PBMTranscranialParams`, the four parsers, the two serializers, the web compiler and the Windows/iOS session builders. **Doing them separately pays the five-runtime coordination cost twice**, and that cost is the single largest line in §7 |
| **The absolute change makes per-channel *more* natural** | Under percent, *"which percent, of what?"* has no answer — §6's five mutually-disagreeing reference constants exist because of it. Under `irradiance_mw_cm2`, *"808 at 300, 660 at 0"* is a statement with a meaning, and option (a)'s per-channel magnitudes become worth having |
| **Per-channel makes the absolute change *more correct*** | The reference-emitter constant differs per channel — 660 nm and 808 nm have different V_f and (in principle) different flux. A single per-tile conversion constant is already an averaging assumption that per-channel authoring exposes |
| **R-4/R-5 become checkable** | Both are written in mW/cm². Under absolute units an 808-only protocol at 403 states its own compliance, and the aggregate becomes a sum of stated numbers rather than of modelled ones |
| **The 26 authored values** | The absolute change's stated blocker is that *"every one of these 26 numbers has to be decided, not converted."* **Per-channel control does not add to that count** — the eight affected protocols set 660 nm to *off*, which is a decision already made by their source trials, not a new number to invent |

**Recommendation: land per-channel control (option b) *first*, as a small additive change, then the
absolute-unit change.** Three reasons: per-channel is additive and reversible while the absolute
change is neither; per-channel needs no principal decision while the absolute change is blocked on
26 of them; and the absolute change's own plan recommends its Class B conversion stay fail-closed
and free of an irradiance interlock, which is easier to hold if the channel selection is already
settled. **If they are combined instead, they must be one commit across five runtimes, and the
combined change is 20–30 days, not the sum of the two estimates.**

---

## 11. Costed options

Days are engineering days for one experienced engineer including tests, and exclude design review,
regulatory work and the RISK-03 opinion, which is not an engineering task and has its own
$8,000–15,000 / 3–5 week estimate in `docs/status/pending-decisions.md` §13.1.

| | **Option 0 — do nothing** | **Option 1 — language + compiler ★** | **Option 2 — full per-channel hardware** |
|---|---|---|---|
| **What it is** | Keep `wavelength` at three values; keep writing one register value into both channels | `wavelength` gains `"660nm"` and `"808nm"`; compiler writes `cur_a` and `cur_b` independently; dose attribution gated on commanded drive (D-a) | Option 1 **plus** per-channel magnitudes, per-channel duty/frequency, `ch_mask` on the base struct, and a hardware path that makes per-channel dose a **measurement** |
| **Tile BOM delta** | $0.00 | **$0.00** | **$0.00** for control; **+$0 to +$10/tile** if per-channel dose needs wavelength-selective detection (§5.2 D-c) — reopens `OI-HEXTILE-06`'s dominant term |
| **Socket contacts** | 19 | **19 — unchanged** | **19 — unchanged** |
| **Wire format** | unchanged | **unchanged** (0 bytes) | +1 byte on `np_mod_pbm_base_params_t` for `ch_mask` |
| **NPPS** | — | 2 enum members; **no grammar production change**; **no shipped protocol changes** | New nested block form; grammar change; `NP-NPPS-GRAM-001` revision; migration of 22 blocks |
| **Runtimes** | — | 7–12 days across web / iOS / Android / grammar / Windows + fixtures | 15–25 days, same five, larger surface |
| **Firmware** | — | ~3 days (D-a + tidy), Class B | 8–15 days; D-b's time-multiplexing needs an EMI review against REQ-EMI-03/04 |
| **Docs** | — | `NP-NPPS-REF-001` Rev 14 → 15; `NP-HW-HEXTILE-001` §2 R-5 restatement; `NP-FW-PBM1064-001` §6.4 cascade wording | the above plus `OI-HEXTILE-07` register map, `NP-DRV-SHELL-002` EMI re-review |
| **Total** | **0 days** | **≈12–19 days** | **≈30–50 days**, plus an EMI review and possibly an `OI-HEXTILE-06` reopening |
| **IEC 62304** | — | **Class B** | Class B, unless D-b's EMI answer or an irradiance interlock pulls part of it to Class C |
| **Delivers** | nothing | Eight protocols stop delivering an unevidenced wavelength; 2 return to single-pass; wall-clock ~halves for 6; R-5 headroom doubles | the above, plus unequal two-channel mixes and per-channel dose as measurement — **neither of which any shipped protocol or cited trial currently asks for** |
| **Costs** | Eight protocols keep delivering 49.5 J/cm² (Chun) to 64.6 J/cm² (Cassano) of an unevidenced wavelength; two trip R-7's 660 nm limit; ~57 % of tile electrical power spent on it | `"660 nm off"` and `"660 nm at 0 %"` become the same statement, and §3.4 says the hardware meaning of the second is not yet established | Everything in Option 1's column, plus a grammar change and a second channel-selection mechanism beside `wavelength` |

### 11.1 Recommendation

**Option 1, sequenced before the absolute-irradiance change, with three conditions.**

1. **One commit across all five runtimes**, including the negative fixtures. `NP-NPPS-REF-001`
   Revs 5, 8, 11 and 12 record four separate instances of this exact field diverging; three of them
   were `wavelength`.
2. **The unknown-wavelength fallback must become an error in the same commit.** All four parsers
   currently default an unrecognised value to `660_808nm`. Ship the enum without fixing that, and an
   `"808nm"` protocol on a stale runtime **silently re-enables the channel this whole change exists
   to disable** — a wrong-wavelength dosing path that produces no error and looks identical to
   correct operation.
3. **D-a (command-gated dose attribution) lands with it, not after.** Without it the device reports
   a non-zero 660 nm dose for a disabled 660 nm channel, accumulating against R-7's limit and
   feeding the R-5 aggregate. A per-channel dose number that no measurement supports is worse than
   the collapsed one it replaces, and dose metering is the claim this product category is judged on.

**Do not adopt Option 2 now.** Its extra capability — unequal two-channel magnitudes — is asked for
by **no shipped protocol and no trial in `docs/pbm_neuro_protocols.md`**. Its per-channel dose
measurement is worth having but is the expensive half, and D-a delivers the correctness property
(off means zero) without it. **Option 2's language half remains available as a strict superset of
Option 1's**, which is the strongest argument for taking Option 1 first: nothing in it has to be
undone.

**Option 0 is not neutral.** Every session run under it delivers a wavelength the evidence did not
use, at half the emitted optical power, and in two protocols at a full clinical dose. That is a
standing cost, not a deferral.

### 11.2 Which numbers here are real, and which cannot be known today

| Real, and derivable from the tree today | Unknowable until `OI-HEXTILE-02` selects an emitter |
|---|---|
| BOM delta **$0.00** — no part is added | The **absolute** irradiance of either channel. §4.3's 95 mW flux and 403 mW/cm² are **design targets, not datasheet values** |
| Socket contact delta **0** — the socket carries a bus, not a drive (§4) | Whether the 660 nm/808 nm optical split is *exactly* 50 % — it is 50 % **by the equal-flux design target**, and real parts will differ |
| Wire-format delta **0 bytes** under the recommended reading | Whether a zero current setpoint produces zero emission (§3.4) — leakage and minimum usable drive are part properties |
| The **56.8 %** electrical split — follows from the V_f *targets*, and its **direction** (660 costs more per emitted watt) follows from physics regardless of parts | The **absolute watts** in §9.2–9.3. The *ratios* survive; the watts inherit `OI-HEXTILE-02` and `OI-HEXTILE-20` |
| Engineering days — counted from real files and real line counts | Whether R-5 is 600, 1,200 or lower. **RISK-03 has never been commissioned** (§8.3) |
| That eight protocols are affected and three are counter-examples — read from the files and the evidence document | Whether R-4's 400/200 mW/cm² survives counsel review, which changes every intensity figure downstream |

**Every irradiance, watt and dose figure in this study is a floor-or-target derived from unselected
parts.** What does **not** depend on part selection: the *architecture* is already per-channel (§2),
the socket is contact-neutral (§4), the dose model does not measure per channel (§5.2), and the
language collapses two registers into one (§2.2). **Those four are the findings; the numbers are the
scale.**

---

## 12. Decisions

Recorded so each can be challenged individually. **None is locked**; all are proposals for design
review.

| ID | Decision | Rationale | Reversible? |
|---|---|---|---|
| **D-1** | The hardware question is **closed**: per-channel current is already the T1-A architecture, at zero BOM delta | Separate FET + sense resistor per channel (`NP-HW-HEXTILE-001` §6.2); different string lengths per channel make a shared sink impossible (§8.1); per-channel register map (`NP-FW-PBM1064-001` §5.1) | n/a — a finding, not a choice |
| **D-2** | The change is **contact-neutral** and does not gate socket tooling | D-3 moved drive off the socket; `VLED` count is sized on peak current, which per-channel control cannot raise | n/a — a finding |
| **D-3** | Adopt NPPS option **(b)** — extend the `wavelength` enum with `"660nm"` and `"808nm"` | No grammar change; no shipped protocol changes; reuses the existing channel-selection mechanism; nothing positional; (a) stays available as a superset | **Yes** — additive; (a) can supersede it later |
| **D-4** | The unknown-`wavelength` fallback becomes an **error** in the same commit | A stale runtime would otherwise silently re-enable 660 nm — the precise failure the change exists to remove, with no error to notice | Yes, but doing so re-creates the hazard |
| **D-5** | Dose attribution is **gated on commanded drive** (§5.2 D-a) in the same change | Otherwise a disabled channel reports non-zero dose against R-7's limit and into R-5's aggregate | Yes |
| **D-6** | **No irradiance interlock is added to the safety MCU** by this change; it stays Class B | The 62 °C thermal interlock is the independent Class C mitigation and measures the outcome; an absolute ceiling would be a new Class C safety function and a separate decision | Yes — and it is the decision that would move the class |
| **D-7** | R-5 is restated as *sum over **enabled** channels* (§8.2 R-5a); the §6.4 throttle cascade is restated as *among enabled channels* | R-5b is absurd once channels can be off; the code already sums over a per-wavelength array, so R-5a matches the implementation once D-5 lands | Yes |
| **D-8** | Sequence **before** the absolute-irradiance change | Additive and reversible; needs no principal decision; the 26-value blocker does not apply | Yes — combining them is viable at 20–30 days as one commit |

**Rejected, with reasons:**

- **Option (c), array-valued intensity** — positional, and a transposed or mis-lengthed list is a
  wrong-wavelength dosing error with no error message. Same defect class as
  `NP-NPPS-REF-001` Rev 12's silently-dropped tokens.
- **Option (a) now** — a second channel-selection mechanism beside `wavelength`, a grammar change,
  and migration of 22 blocks, to express magnitudes no shipped protocol and no cited trial asks for.
- **Growing `np_mod_pbm_base_params_t` pre-emptively** — a Class B wire-format change made against
  an unselected part, to fix a semantics question that `OI-HEXTILE-07` should answer in prose first.

**Values deliberately NOT asserted:** the absolute irradiance of either channel; whether a zero
setpoint emits nothing; whether R-4 or R-5 survives counsel review; whether two independently-phased
PBM lines satisfy REQ-EMI-03/04. Each is an open item below, not an omission.

---

## 13. Open Items

Append-only per `NP-CONV-001` §6.

| ID | Description | Owner | Blocking |
|---|---|---|---|
| **OI-PBMCH-01** | **What does a current setpoint of zero mean at the emitter?** `np_mod_pbm_base_params_t` has no `ch_mask`, so on a base tile `cur_a = 0` is the only expression of "off". Whether that produces zero emission depends on FET leakage and the string's minimum usable drive, both of which are part properties. `OI-HEXTILE-07` must state the semantics explicitly rather than leaving them to §5.1's encoding table. **If "off" must be provably off, the base struct needs a `ch_mask` byte — the only wire-format change this study identifies** | Firmware + EE | `OI-HEXTILE-07`; **sequence after `OI-HEXTILE-02`** |
| **OI-PBMCH-02** | **Is per-channel PWM phase EMI-safe?** The register map already permits `PWM_FREQ_A ≠ PWM_FREQ_B` and `DUTY_A ≠ DUTY_B`, so two independently-phased PBM lines inside the Faraday envelope is already commandable — but `NP-DRV-SHELL-002` REQ-EMI-03/04's subtractable-artifact argument is written for **one** line, and pin 11 `SYNC` is **one** broadcast reference. The recommended language (option b) exposes only per-channel *intensity* and keeps one `freq_code` and one `duty` byte, so this is **not** opened by D-3 — but it must be answered before any per-channel duty or frequency form is added (Option 2) | EMI + EE | Option 2 only; not blocking Option 1 |
| **OI-PBMCH-03** | **`NP-FW-PBM1064-001` §6.5's per-channel dose shutdown has no implementation on T1-A.** §6.5 says *"channel disabled via I2C `CH_ENABLE` write … Session continues for remaining channels"*, but the base-tile path (`np_mod_pbm.c:215-225` → `np_mod_pbm_hal_pwm_set`) has no `CH_ENABLE` to write. **Pre-existing, surfaced not created by this study**, and §9.2 shows it is live: two protocols already deliver ≥60 J/cm² on the channel that cannot be individually shut down. Overlaps `OI-PBMCH-01` | Firmware | Independent of this change; **should be fixed regardless of the option chosen** |
| **OI-PBMCH-04** | **`wavelength` never reaches the wire on iOS or Windows.** Both session builders drop the field entirely (`NPSessionProtocol+FromDefinition.swift:26-32`, `SessionProtocolCompiler.cs:42-53`), so a `"1064nm"` protocol compiles to the same config as a `"660_808nm"` one **today**. Adding enum members changes nothing on those two platforms until the field is carried through. Same defect class as `NP-NPPS-REF-001` Rev 11's round-trip failures. **Not included in §7.2's 7–12 days** | App (iOS + Windows) | **Must be resolved for the change to have any effect on two of five runtimes.** Sequence with, or explicitly before, D-3 |
| **OI-PBMCH-05** | **A power governor must take per-channel drive as an input.** `OI-HEXTILE-09` requires the governor be denominated in watts with per-tile drive as an input. Once channels are independently commandable, "per-tile drive" is per-channel, and a governor billing 808-only protocols at the dual-channel rate would reject sessions that fit (§9.3: Cassano 82 W → 36 W). Not blocking, but the governor must be written knowing this | Firmware + app | `OI-HEXTILE-09`; sequence after `OI-SESPWR-03` |
| **OI-PBMCH-06** | **Three shipped protocols carry the fidelity gap without the annotation, and one is pediatric.** `clinical-01-pbm-alzheimers-wozniak` (850 nm), `clinical-02-pbm-mci` (850 nm) and `clinical-07-pbm-autism` (850 nm, pediatric) are as affected as the five that carry the note, and are not marked. §9.1 finds the affected set is **eight, not six**. **This study modifies no protocol**; the annotation gap is recorded here so the set is correct wherever it is next acted on. Note `clinical-07`'s §7 evidence calls for *"conservative dosing and tolerability titration"* | Protocol authoring | Protocol library; not tooling-blocking |
| **OI-PBMCH-07** | **R-5's restatement (D-7) is stable, but R-5's *value* is not.** §8.3 traces both R-4 and R-5 to firmware constants marked *"pending RISK-03 regulatory opinion"*, and RISK-03 has never been commissioned; `NP-REG-PBM1064-001` Q4 asks counsel whether 600 should stay, rise to 1,200, or fall for IEC 62471 Exempt Group. D-7 changes **what the sum ranges over**, which holds at any value — but it must not be presented as settling R-5. **The fullest statement of this chain is `docs/status/pending-decisions.md` §13.1a, which is on branch `claude/risk-03-counsel-engagement` and is NOT merged to `main` as of `b69d28b`** — and which cites R-5 to `NP-HW-HEXTILE-001` §5; the requirements table R-1…R-12 is **§2**, and §5 is the photodiode section. A one-character correction, noted here rather than made, because that document is not this study's to edit | CEO / Regulatory Counsel | RISK-03 (Issue #5). **Extend the existing engagement — do not open a second one** (`NP-REG-PBM1064-001` §2) |

---

## 14. Cross-references

| Document | Relationship |
|---|---|
| `NP-HW-HEXTILE-001` Rev 8 | **Parent.** Supplies the driver topology (§6.2), string arithmetic (§8.1), pinout (§7.2), irradiance (§4.3) and concurrency (§9) that every finding here rests on. This study adds no requirement to it and proposes one wording change (D-7, R-5) |
| `NP-FW-PBM1064-001` Rev 4 | Owns the register map (§5.1), the dose model (§6.2), the aggregate ceiling (§6.4) and the per-wavelength limits (§6.5). §5.2's D-a and §8.2's cascade restatement land here |
| `NP-NPPS-REF-001` Rev 14 | Owns the language. D-3 takes it to Rev 15. Its Revs 5/8/11/12 are the evidence for §7.1's one-commit rule |
| `NP-SES-PWR-001` Rev 1 | Supplies the power-audit method and the 20-protocol baseline §9.3 extends. `scripts/check-pbm-power.ts` is the shared arithmetic |
| `NP-DRV-SHELL-002` Rev 4 | Peer on the socket interface; §4's contact-neutrality is verified against its §5.1.4 pin table and §5.1.5 count rule. REQ-EMI-03/04 own `OI-PBMCH-02` |
| `NP-COST-001` Rev 2 | §3.3's cost context. This study's zero BOM delta keeps it **outside** `OI-HEXTILE-06` and therefore outside `OI-COST-10`'s pricing precondition |
| `NP-OPT-PSF-001` Rev 1 | Not exercised here — this change alters *which wavelength*, never *where*, so the ~26 mm resolution floor is untouched |
| `docs/pbm_neuro_protocols.md` | The evidence base. §9.1's trial wavelengths and §9.4's penetration argument come from §1–§9 and dosimetry lesson 2 |
| `docs/status/pending-decisions.md` | §13.1 RISK-03. **§13.1a is unmerged** — see `OI-PBMCH-07` |

---

## 15. Revision History

| Rev | Date | Author | Change |
|---|---|---|---|
| 1 | 2026-08-26 | NeurOne Systems Engineering | First issue. Costed six-layer study of independent 660/808 nm channel control. **Corrects the premise it was given:** the T1-A tile does not drive both channels from one current register — separate FETs, separate sense resistors, separate series strings of different length and a per-channel register map are all already specified, so the BOM delta is **$0.00** and the socket delta is **0 contacts**; the collapse is one line of `hubCompiler.ts` writing one register value into two registers. Recommends **NPPS option (b)** — extend the `wavelength` enum — at **≈12–19 days**, sequenced before the absolute-irradiance change, with three binding conditions: one commit across five runtimes, unknown-enum fallback becomes an error, and command-gated dose attribution lands with it. Finds **eight** affected protocols (not six) and **three** dual-wavelength counter-examples (not one); verifies the unwanted 660 nm channel at **exactly 50.0 % of optical** and **56.8 % of electrical** tile power, delivering **49.5–64.6 J/cm²** in the worst cases — **at or over R-7's 60 J/cm² limit in two protocols**. Finds `np_pbm1064_dose_tick()` reads one broadband photodiode per wavelength, so per-wavelength dose is **modelled, not measured**, and would report non-zero dose for a disabled channel. Raises **OI-PBMCH-01…07** |
