# ADS1299 EMI Rejection Ratio, 420 MHz – 3 GHz — Bench Test Procedure

**Project:** NeurOne
**Document:** NP-EMC-EMIRR-001
**Revision:** 2
**Date:** 2026-09-25
**Status:** DRAFT — procedure written, **never run. This document contains no measured data**, and nothing in it may be quoted as a measurement.
**Effective Date:** —
**Author:** NeurOne EMC / Systems Engineering
**Approved By:** — (pending EE Lead review; the procedure's owner is the owner of `OI-EMCCAV-01`)
**References:** `NP-EMC-CAV-001` §4.1, §4.2, §5.1, §5.2, §5.4, §9 item 3, §10 (`OI-EMCCAV-01`); `scripts/check-cavity-q.ts` (`fieldLimit()`, `emirrSensitivity()`); `NP-DRV-SHELL-002` §9.1 (`SH2-DRC-16`); `NP-FW-MMSOCK-001` §3.4 (EEG path PGA gain ×24); `NP-HW-EEGNET-001` §6.3 (`REQ-NET-17`, 1 kΩ series R, T1); CLAUDE.md §3 modality ③ (ADS1299, 500 Hz); `NP-CONV-001` §4, §7.1; TI SBOA128 *EMI Rejection Ratio of Operational Amplifiers* (EMIRR definition and 100 mV_P convention); IEC 61967-4 (conducted-emission 1 Ω / 150 Ω method — injection-network practice only, not a limit)
**Related Issues:** GitHub Issue #398 (`OI-EMCCAV-01`); GitHub Issue #391 (parent — `REQ-CAV-04`)
**Gate:** N/A — not gating; bounds `REQ-CAV-01` (`NP-EMC-CAV-001` §10)
**IEC 62304 Class:** N/A (component characterisation; no device software)
**Supersedes:** None — new document
**Parent Document:** `NP-EMC-CAV-001`

---

> **What this procedure is for, in one paragraph.** `NP-EMC-CAV-001` §5.1 derives `REQ-CAV-01`
> (**E ≤ 0.5 V/m peak, 420 MHz – 3 GHz**, at the electrode plane) from a chain of five inputs, and one
> of them — **the ADS1299's EMI rejection ratio, assumed 60 dB** — has no source: TI publishes no
> EMIRR for the part. `REQ-CAV-01` moves one decade per 20 dB of it, so the margin behind the Layer 4
> deletion (`REQ-CAV-04`) rests on a number nobody has measured. This procedure measures it on parts
> in hand and says exactly how the result re-derives `REQ-CAV-01`. **It has no device pass/fail: its
> output is an input.** What each candidate outcome would do to the requirement is computed in
> `NP-EMC-CAV-001` §5.4 by `scripts/check-cavity-q.ts`, not here.

---

## 1. Scope

**In scope.** Conducted RF injection into ADS1299 analog inputs, 420 MHz – 3 GHz, and measurement of
the in-band artifact that RF demodulation produces at the ADC output. Differential-mode (the figure
of record) and common-mode (a check on `NP-EMC-CAV-001` §5.1's model structure, §6.3).

**Out of scope.** Radiated immunity of any assembly; cavity Q (`EMF-1a`–`EMF-1c`); the field at the
electrode plane (`EMF-1d`); the fluxgates (the other §5.1 victim — separate item, not raised here);
any IEC 60601-1-2 immunity test. This is a component characterisation that feeds a derivation.

## 2. The quantity measured — and why it is not the datasheet convention

`NP-EMC-CAV-001` §5.1 and `fieldLimit()` in `scripts/check-cavity-q.ts` use EMIRR as

> `V_RF,max = ΔV_allowed · 10^(EMIRR/20)`, with ΔV_allowed = **1.0 µVpp** (20 % of `SH2-DRC-16`'s
> 5.0 µVpp) and V_RF the **peak** RF amplitude at the amplifier input.

So the number the model needs is

> **EMIRR_NP(f) = 20·log₁₀( V_RF,pk(f) / ΔV_art,pp )**, where ΔV_art,pp is the peak-to-peak
> **in-band** artifact at the ADC output, **referred to input**, when the RF is keyed on and off.

This differs from TI SBOA128's op-amp convention (CW at 100 mV_P, EMIRR from the **DC offset
shift**) in one respect that matters: the §3 source is **bursty** (I²C transactions, LED PWM), so what
reaches the EEG band is the demodulated offset *switching*, which is a peak-to-peak artifact. On/off
keying reproduces that directly and makes the result comparable to a µVpp budget without a
conversion factor. **The SBOA128 figure is also recorded (§7.3)**, so the result can be compared
with published EMIRR data for other parts; it is not the figure of record.

## 3. Test articles

| Item | Requirement | What fails without it · where it traces |
|---|---|---|
| DUT | ADS1299 (the part named in CLAUDE.md §3 modality ③), from stock | The requirement is about this part. Another part's EMIRR is not evidence |
| Sample size | **≥ 3 devices**, from ≥ 2 date codes where stock allows | One device cannot show part-to-part spread, and `REQ-CAV-01` is set by the worst part, not a typical one. Three is the smallest set that yields a spread at all — **this is a characterisation, not a population claim**; whether a statistical margin is added on top is §8's open question, not decided here |
| PGA gain | **×24** | The EEG path's gain (`NP-FW-MMSOCK-001` §3.4). EMIRR referred to input is gain-dependent; measuring at a gain the product does not use measures the wrong thing |
| Data rate | **500 SPS** | CLAUDE.md §3 modality ③. Sets the digital filter the artifact passes through |
| Channels | **All eight at the four §5 spot frequencies**; IN1 and IN5 across the full plan | Pin position changes package parasitics, and `REQ-CAV-01` is set by the worst pin. The spot frequencies find the worst pin; if it is neither IN1 nor IN5, it is swept across the full plan too (§6.4) |

## 4. Fixture

### 4.1 Injection board

- Four-layer FR-4 or better, solid ground on layer 2. **50 Ω grounded coplanar waveguide** from an
  edge-launch SMA to within ≤ 2 mm of each injected pin, terminated **at the pin** in a 50 Ω shunt
  (0402, to AGND through a DC block).
  *Why:* the ADS1299 input is not 50 Ω, so without a pin-side termination the RF voltage at the pin
  is set by an unknown mismatch and cannot be stated. The shunt defines V_RF,pk = √(2·P·50 Ω) at the
  pin to within the calibration of §4.3. 2 mm is ≈ λ/27 at 3 GHz (ε_eff ≈ 3.3, λ ≈ 55 mm), short
  enough that the unterminated stub between the shunt and the high-impedance pin raises V_pin over
  V_shunt by ≤ 0.25 dB at 3 GHz (V_pin/V_shunt = 1/cos βl). That is corrected analytically from the
  as-built stub length and recorded; a longer stub is not used.
- Pin DC bias from a mid-supply reference through **≥ 10 kΩ, resistive only** — ≥ 10 kΩ loads the
  50 Ω termination by < 0.05 dB; no inductor and no ferrite, because the fixture must not import a
  filter the product does not have.
- **No input RC network, no series resistor on the injected pins** (Configuration A, §4.4).
- ADS1299 supplies from **linear regulators** on the board; SPI, `DRDY#`, START and CLK leave through
  a bulkhead with **per-line RC filtering, sized so that §6.2 control 2 passes** (the control is the
  requirement; the filter is only the means).
  *Why:* the measured artifact must enter through the pin under test. RF reaching the digital or
  supply pins produces an artifact that §5.1's model does not describe; §6.2's control detects it.
- The board sits in a closed shielded box; only the SMA(s) and the filtered bulkhead penetrate it.

### 4.2 Differential and common-mode drive

| Mode | Network | Covers 420 MHz – 3 GHz? |
|---|---|---|
| **DM (figure of record)** | 180° hybrid on INxP / INxN | A single hybrid must cover the band; if none does, use two with an overlap of ≥ 1 point (§5) and record both |
| **CM** | In-phase power divider on INxP / INxN together | same |

Amplitude and phase imbalance of each hybrid are measured with a VNA across the band and recorded.
**A DM result is valid only where the hybrid's amplitude imbalance is ≤ 0.5 dB and its phase
imbalance ≤ 5°.** At those limits the residual CM drive is ≈ 31 dB below the DM drive, so the DM
figure is contaminated by ≤ 1 dB unless EMIRR_CM is more than ~13 dB below EMIRR_DM — which §6.3
measures directly; DM points where it is are flagged *CM-limited*. Where the hybrid is worse than
the limits, the point is recorded and flagged, not used.

### 4.3 Calibration of V_RF at the pin

1. Build a **calibration twin**: the same board and trace, the DUT replaced by an SMA to a
   calibrated power sensor at the pin location.
2. Sweep the §5 frequency plan at each §5 level; record the source setting that yields each target
   pin power. Those settings, not the generator's nominal output, are used in §6.
3. Repeat at the start and end of each measurement day. **The day's drift is carried as the
   uncertainty on every EMIRR taken that day** (a V_RF error of x dB is an EMIRR error of x dB).
   Where EMIRR_min ± that uncertainty straddles a §7.2 disposition boundary, the governing point is
   re-measured — a boundary is the only place a calibration error changes what happens next.

### 4.4 Configuration B (informational only)

Repeat §6.1 at three frequencies (422 MHz, 1 GHz, 3 GHz) with **1 kΩ in series** at each injected
pin — the puck resistor of `NP-HW-EEGNET-001` `REQ-NET-17` (T1). **This result is not credited to
`REQ-CAV-01`.** The in-tile pickup §5.1 models runs through the N4 path, which carries no such
resistor, and crediting it would be a change to §5.1's derivation — an EMC decision, not an outcome
of this procedure. It is recorded because it bounds how much the EEG-net path differs from the path
the requirement was derived for.

## 5. Frequency and level plan

**Frequencies.** Log-spaced, **step ≤ 5 %**, 420 MHz → 3 GHz inclusive — **41 points** — plus four
spot frequencies from `NP-EMC-CAV-001`:

| Spot | Why it is there |
|---|---|
| 422 MHz | lowest cavity mode, 62 cm head (§4.1) |
| 460 MHz | lowest mode, mid-range head — every single-number anchor in §5–§6 is evaluated here |
| 506 MHz | lowest mode, 52 cm head (§4.1) |
| 2.58 GHz | first radial (quarter-wave) mode (§4.2) |

*Why 5 %:* the wearer-loaded cavity has Q ≈ 1.3 (`NP-EMC-CAV-001` §6.3), so the field it presents is
broadband; narrow features in EMIRR can come only from the package and die (bond-wire and ESD-
structure resonances). A 5 % step resolves any feature whose −3 dB width is ≥ 10 %, i.e. Q ≤ 10. A
sharper feature found between points is chased with a local 1 % sweep (§6.4).

**Levels at the pin** (50 Ω): **−10, −20, −30, −40 dBm** (100, 31.6, 10.0, 3.16 mV_pk).

| Level | Why |
|---|---|
| −10 dBm | = 100 mV_P, the SBOA128 anchor, so the result compares with published EMIRR data |
| −20 … −40 dBm | walks toward §5.1's operating point (**V_RF,max ≈ 1 mV** at 60 dB). RF rectification is **square-law at small signals** — artifact ∝ V_RF² — so EMIRR is **not level-independent**, and a figure taken only at 100 mV_P would *understate* EMIRR at 1 mV. The descent measures the slope instead of assuming it |

The maximum level is within the ADS1299's input range at the §4.1 bias (100 mV_pk about mid-supply);
no level approaches the absolute-maximum input rating.

## 6. Method

### 6.1 Measurement, per device · channel · mode · frequency · level

1. RF source **100 % on/off keyed at 10.0 Hz**, 50 % duty.
   *Why 10 Hz:* inside the EEG band the artifact budget protects, and clear of 50/60 Hz and their
   harmonics, so mains pickup cannot alias into the detection bin.
2. Record the ADC stream at 500 SPS, gain ×24.
3. **Synchronous detection** at 10 Hz against the keying reference: fold the record at the keying
   period, average, and take ΔV_art,pp = (mean over the RF-on half) − (mean over the RF-off half),
   converted to µV **referred to input** (÷ gain).
4. Average until the detection noise floor (§6.2 step 1) is **≤ 0.1 µVpp RTI** — one tenth of §5.1's
   1.0 µVpp allocation, so a result at the allocation boundary is resolved with ≥ 20 dB SNR.
5. **A point is *resolved* when ΔV_art,pp ≥ 3 σ** of the floor, σ taken from the RF-off control.
   Otherwise it is recorded as *below floor*, with the floor as an upper bound on the artifact — **a
   lower bound on EMIRR, never a value**.

### 6.2 Controls — run at every frequency, at the highest level

| Control | Configuration | Must show | If it does not |
|---|---|---|---|
| 1. Floor | Keying running, RF into a 50 Ω load at the source (cable disconnected at the board) | The detection noise floor. Sets §6.1 step 5's σ | — (it defines the floor) |
| 2. Ingress | RF into a 50 Ω load **at the board SMA**, board powered and streaming | Artifact below floor | RF is entering through supply/digital lines or the box. **Invalidate the frequency** and fix the fixture |
| 3. Shorted input | INxP = INxN = bias, RF off | Offset and noise consistent with the ADS1299 datasheet at ×24 | DUT or fixture fault |

### 6.3 Common-mode figure, and what it tests

Repeat §6.1 with the CM network. §5.1's chain assumes the RF reaches the input **differentially**,
after a **20 dB** CM→DM conversion (the 110 dB CMRR does not hold at RF). If the part instead
rectifies CM drive directly — at the input ESD structures, before any conversion — the relevant
figure is EMIRR_CM without the 20 dB, and **the CM path sets `REQ-CAV-01` whenever**

> **EMIRR_CM < EMIRR_DM + 20 dB**

That comparison is a check on the **model's structure**, not a pass/fail. Its disposition is §7.2.

### 6.4 Chasing features

1. **Between points.** Where two adjacent §5 points differ by > 6 dB, insert a 1 % sweep across the
   interval. *Why 6 dB:* a single-pole feature with Q ≤ 10 changes by ≤ ~3 dB over one 5 % step, so
   a 6 dB step means a feature sharper than §5 is sized for.
2. **Between pins.** If the spot-frequency sweep (§3) finds the worst pin is neither IN1 nor IN5,
   sweep that pin across the full plan.

## 7. Result, and how it ties back to `REQ-CAV-01`

### 7.1 Figure of record

For each (device, frequency): take DM EMIRR_NP at the **lowest resolved level**. If a lower level was
below floor, the resolved-level figure stands — extrapolating downward with the measured slope would
credit square-law behaviour below where it was observed, and is not done. **Wherever the measured
slope exceeds 1 dB/dB (i.e. toward square-law), the extrapolation to 1 mV is reported alongside,
labelled *extrapolated*, and not used** — crediting it is a change to §5.1's model, and an EMC
decision.

> **EMIRR_min = min over devices, channels and all frequencies in 420 MHz – 3 GHz** of the figure of
> record. This is the number that replaces `EMIRR_DB = 60` in `scripts/check-cavity-q.ts`.

### 7.2 Disposition

`REQ-CAV-01` is re-derived as **E_max = 0.5 V/m × 10^((EMIRR_min − 60)/20)**, the §5.1 chain with
the measured value substituted (`fieldLimit(EMIRR_min)`), **or by the CM path of §6.3 if it is lower**.
The candidate outcomes are already computed in `NP-EMC-CAV-001` §5.4:

| EMIRR_min | `REQ-CAV-01` | Consequence | Who decides the next step |
|---|---|---|---|
| **≥ 60 dB** | ≥ 0.5 V/m | Assumption confirmed or conservative. Publish the derived value; update the anchor | EMC — documentation only |
| **36.4 – 60 dB** | 0.033 – 0.5 V/m | `REQ-CAV-01` tightens. With the source/layout share held fixed, the head's margin against the scaled Q ceiling shrinks from 23.6 dB but **stays positive** | EMC — re-derive, publish, update anchors |
| **< 36.4 dB** | < 0.033 V/m | Under the fixed allocation the wearer-loaded cavity no longer meets the scaled `REQ-CAV-02`. **An allocation decision is required**, and it is not this procedure's | **EMC / principal** — escalate. **Layer 4 is not a remedy:** it adds 0.001 dB with the head fitted, at any EMIRR (§5.4) |
| **CM path governs** (§6.3) | set by EMIRR_CM | §5.1's derivation chain is **structurally wrong**, not just mis-valued. Re-derive §5.1 with the CM path; new anchors | EMC |

If EMIRR_min varies with frequency by > 6 dB across the band, `REQ-CAV-01` could be published as a
frequency mask instead of a flat limit. **The default is flat at the minimum** — conservative, and
what the requirement is today. A mask is an EMC choice and is not made by running this procedure.

### 7.3 Also recorded (not figures of record)

- SBOA128-convention EMIRR at −10 dBm, CW, from DC offset shift — for comparison with published data.
- Artifact-vs-level slope per frequency.
- Configuration B (§4.4) at its three frequencies.
- All control results (§6.2), hybrid S-parameters (§4.2), calibration twin data (§4.3).

## 8. Open questions this procedure does not answer

1. **Statistical margin on three devices.** §3 takes the worst of ≥ 3 parts. Whether `REQ-CAV-01`
   should be derived from the worst measured part or from a tolerance bound on the population is an
   EMC decision; it needs a sample-size argument this procedure does not make.
2. **The fluxgates** are the other §5.1 victim and have no RF demodulation figure either. Not raised
   as an item here; noted so its absence is visible.

## 9. Equipment

Signal generator to ≥ 3 GHz with pulse/on-off modulation and a TTL reference output · power meter and
sensor, calibrated, to ≥ 3 GHz · VNA to ≥ 3 GHz (hybrid and fixture characterisation) · 180° hybrid(s)
and in-phase divider covering the band (§4.2) · DC blocks · shielded enclosure · linear bench supplies
· SPI capture host. **Every item's asset ID and calibration due date is recorded with the data.**

## 10. Record

The run produces a test record (raw ADC streams, keying reference, calibration twin data, control
results, equipment list) filed against `OI-EMCCAV-01`, and a revision of `NP-EMC-CAV-001` that
substitutes EMIRR_min into §5.1 and `scripts/check-cavity-q.ts`. **The record contains no user data
of any kind** — a bench component, no wearer — so neither UHDR nor SHDR applies.

## 11. Revision history

| Rev | Date | Author | Change |
|-----|------|--------|--------|
| 2 | 2026-09-25 | NeurOne Systems Engineering | **Naming only (`NP-CONV-001` §1.1, `OI-CONV-02` sweep, GitHub #394).** The ADS1299's data-ready output is active-low on the part, so it is `DRDY#`, not `DRDY`. No requirement, filter or test changes. |
| 1 | 2026-09-23 | NeurOne EMC / Systems Engineering | **Initial issue, for `OI-EMCCAV-01` (GitHub #398).** Specifies the bench measurement of ADS1299 EMIRR over 420 MHz – 3 GHz: the model-consistent definition (on/off-keyed, µVpp RTI, §2), a pin-terminated 50 Ω injection fixture with ingress control (§4, §6.2), 41 log-spaced points + the four `NP-EMC-CAV-001` mode frequencies, a four-level descent that measures rather than assumes the rectification slope (§5), a common-mode check on the §5.1 model's structure (§6.3), and the disposition of each outcome back to `REQ-CAV-01` (§7). **Contains no measured data; the measurement is pending.** |
