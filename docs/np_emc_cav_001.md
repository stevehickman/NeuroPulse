# Enclosure Cavity Resonance — Source, Band, and the Layer 4 Requirement

**Project:** NeurOne
**Document:** NP-EMC-CAV-001
**Revision:** 4
**Date:** 2026-09-20
**Status:** ACTIVE — EMC analysis discharging `OI-BIBEMF-08` step 1; asserts no measurement
**Effective Date:** 2026-09-20
**Author:** NeurOne EMC / Systems Engineering
**Approved By:** Pending — principal decision required (§8)
**References:** CLAUDE.md §1, §3, §4.1, §4.3, §4.4; `docs/reference/hardware-detail.md` §4.3; `NP-BIB-EMF-001` §7.3, §7.6, §7.8; `NP-HELMET-GEOM-001` §2, §3.2; `NP-DRV-SHELL-002` §3.2, §3.4, §3.5, §5.4, §9.1–§9.6; `NP-HEX-ZM-001` §5.2, §5.3a, §5.3.1; `NP-THERM-COOL-001` §2, §6.3, §6.9.1; `NP-THERM-SINK-001` `RISK-SINK-03`, `OI-SINK-01`, `OI-SINK-07`; `NP-DT-001` `DI-PERF-22`, `DI-REG-05`; `NP-HW-HUB-001` §8.1; FCC Part 15 Subpart B §15.109; CISPR 11 Group 1 Class B; IEC 60601-1-2; Gabriel et al. 1996 (tissue dielectric properties)
**Related Issues:** GitHub Issue #362; `OI-BIBEMF-08`; `OI-THCOOL-04`; `OI-THCOOL-15`; `OI-SINK-01`; `OI-SINK-07`; `RISK-SINK-03`; `EMF-1`; `EMF-3`; `RISK-20`; `OI-HUB-C19`
**Gate:** —
**IEC 62304 Class:** — (analysis record, not device software)
**Jurisdiction Scope:** N/A
**Change Summary:** Rev 4 adds §8.3 (the clamps lose no space to the re-loft, and the tolerance stack *shrinks* 38 %) and §8.4 (the "5-layer" touch-list, with the RISK-15 homonym trap). `OI-EMCCAV-08` narrowed and no longer gating. See §11.

---

## 1. What this document is for

`NP-BIB-EMF-001` §7.3 found that **Layer 4 of the CLAUDE.md §4.3 stack — carbon-loaded EMI absorber
foam — is the only layer whose benefit has never been stated in any measurable form, anywhere.** Its
entire documented justification is one phrase, *"cavity-resonance suppression"*, repeated verbatim in
`docs/reference/hardware-detail.md` §4.3, `NP-HELMET-GEOM-001` §70 and `NP-DT-001` `DI-PERF-22` — the
last of which gives every other layer a dB figure and Layer 4 none.

Its cost, by contrast, is known to three significant figures: **18 % of the outward thermal path**
(`NP-THERM-COOL-001` §2), the second-largest attackable term, and it blocks *two* separate fixes to a
**BLOCKING** `OI-SINK-01`.

`OI-BIBEMF-08` therefore placed a burden on EMC:

> **State Layer 4's requirement in dB against a named source and frequency band. If EMC cannot,
> delete Layer 4 and recover two thermal levers. If it can, adopt `OI-THCOOL-04`'s conductive-filled
> substitution.**

**This document discharges that burden, and the answer is neither of the two §7.3 anticipated.**

| | §7.3's expectation | What this document finds |
|---|---|---|
| Is there a source? | *"the radios live in the hub"* — probably not | **Yes, and it was always there.** Not radios — 18 cluster controllers' digital edges (§3) |
| Can EMC state a requirement? | maybe not | **Yes.** Band, victim, mechanism and dB are all derivable from the existing record (§4, §5) |
| Then keep the layer and substitute? | §7.3 step 2a | **No.** There is no RF function to preserve — the layer supplies **1 %** of its own requirement (§6) |
| Or delete it because EMC was silent? | §7.3 step 2b | **Delete it — but not for that reason.** EMC is not silent; the layer simply is not what meets the requirement. **And the 3 mm re-loft of the outer bowl is binding**, or the thermal case inverts (§8.2) |

Every figure below is reproduced by `scripts/check-cavity-q.ts`, which carries the model and fails on
drift (`bun scripts/check-cavity-q.ts --validate`). §9 states what this document does **not**
establish, which is more than usual.

---

## 2. The cavity — what it actually is

`NP-HELMET-GEOM-001` §2's radial stack, scalp datum outward. The cavity is bounded **inside** by the
scalp and **outside** by the first continuous conductor, which is the Pd-polyester liner (EMF L3).
Layer 4 is the last 3.0 mm before that conductor, laid directly against it.

| Station | Element | Radial (mm) |
|---|---|---:|
| L1 | module body + socket wall + cluster-clamp features | 18–22 |
| Gap | inter-bowl clamp travel + blind-mate boss + labyrinth lip | 5–7 |
| | **→ air/dielectric region** | **23–29** |
| L2 | **carbon-loaded absorber foam (EMF L4)** — the layer under audit | 3.0 |
| | Pd-polyester (EMF L3) — **the conductive boundary** | 0.1 |
| | **→ scalp to conductor** | **26–32** |

Two consequences the document set has never recorded:

1. **The cavity is a thin curved shell, not a box.** At 26–32 mm over a 83–99 mm head radius, it is
   a shell whose thickness is a small fraction of its circumference. That decides which mode family
   is lowest (§4.1).
2. **A wearer's head is ~37 % of its boundary area and ~44 % of the volume it encloses** — and it is
   the lossiest surface in the enclosure by a factor of 539 (§6.3). That turns out to be the whole
   answer.

> **`NP-DRV-SHELL-002` §9.6 is the governing statement and it is already in the record:**
> *"What is new is an **internal** emitter inside the envelope. A Faraday cage does not protect the
> EEG electrodes and fluxgates that share the enclosure with the source."* Everything below is that
> sentence, quantified.

---

## 3. The named source — and it is not a radio

`NP-BIB-EMF-001` §7.3's stated difficulty was that *"no document identifies what excites a cavity
resonance inside the envelope"*, given `NP-HEX-ZM-001` §5.3a bounds the RF concern to the 6 GHz Wi-Fi
band and states *"the headset's own radios live in the hub, not here."* §7.3 then flagged its own
caveat — that `NP-DRV-SHELL-002` puts 18 active cluster controllers inside the cavity — and asked for
it to be written down.

**It is written down here.** Everything in this table is inside the Faraday envelope, on L1, by
`NP-DRV-SHELL-002` §9.6's own finding:

| Source | Quantity | Spectral character | Cite |
|---|---|---|---|
| **STM32G071 cluster controllers** | **18** | 64 MHz core; GPIO edges 4–8 ns → emission knee **40–80 MHz**, −40 dB/dec above it | §3.2; CLAUDE.md §4.1 |
| **I2C control tree** | 32 segments | 400 kHz, ~18 ms per 100 ms tick (18 % bus duty); open-drain edges, uncontrolled | §3.4; `NP-HW-HUB-001` |
| **PWM LED drivers** | up to **80** | ≈20 kHz carrier on a 24 V rail; the *edges*, not the carrier, are the RF source | §9.2, §5.4 |
| **ADS1299 bank + SPI** | 1 | at the posterior aggregation node, inside the envelope | §3.5 |

**The relevant emission is the edge rate, not the clock rate.** A trapezoidal edge's envelope is flat,
then falls at −20 dB/dec past `1/(πτ)`, then at −40 dB/dec past the knee `1/(π·t_r)`. For 4–8 ns edges
the knee is 40–80 MHz, so there is real spectral content through the whole band of §4 — and it falls
**31.1 dB between the lowest cavity mode and 3 GHz**, which is what sets the band's upper edge.

> **The premise that had to be corrected.** §7.3 is right that the radios are in the hub, and that is
> why `NP-HEX-ZM-001` §5.3a's *external* bound (λ/20 at 6 GHz) is the right bound for *ingress*. It is
> the wrong bound for this question. The cavity is excited from the inside, by parts the same document
> set put there, and `REQ-EMI-04` **prohibits spread-spectrum PWM** — deliberately, to keep the
> artifact subtractable — which concentrates that energy into discrete lines rather than smearing it.
> A deterministic line is exactly what a resonant cavity responds to.

**One conditional worth flagging now.** `OI-HUB-C19` provisionally sites the 15–20 V → 24 V boost
stage (~35 W) on the Hub PCB, **outside** the helmet. A switching converter is the classic broadband
cavity driver, and it is an order of magnitude above anything in the table. **If `OI-HUB-C19` ever
moves that stage inside the envelope, §4's band and §6's verdict must both be re-derived.** That
decision is now an EMC decision and not only a packaging one — recorded as `OI-EMCCAV-03`.

---

## 4. The named band

### 4.1 Lowest supported mode — and it is a property of the wearer

The air region is 23–29 mm thick. A shell cannot support a *radial* mode until its thickness reaches
a quarter wavelength, which does not happen until 2.6 GHz (§4.2). The lowest family is therefore
**circumferential** — one guided wavelength fitted around the mean circumference of the shell:

| Head circumference (CLAUDE.md §4.4) | mean radius | mean circumference | **lowest mode** |
|---|---:|---:|---:|
| 52 cm | 94.3 mm | 592 mm | **506 MHz** |
| 62 cm | 113.2 mm | 711 mm | **422 MHz** |

> **One adult SKU covers 52–62 cm, so the enclosure's lowest resonance sweeps 84 MHz across the
> population.** No document in the set records that the resonant frequency of the shield is a property
> of the person wearing it. It is the reason a requirement written at a single frequency would have
> been wrong even if one had ever been written — and it is a second, independent argument against the
> single-frequency habit `NP-BIB-EMF-001` §7.6 already attacks from the aperture side.

### 4.2 Upper edge

- **First radial mode:** the outer boundary is a conductor (short), the inner is high-permittivity
  tissue, which lies between a short (t = λ/2) and an open (t = λ/4). The quarter-wave case binds:
  **2.58–3.26 GHz**. The half-wave case is 5.2–6.5 GHz.
- **Source roll-off:** 31.1 dB between 500 MHz and 3 GHz on the −40 dB/dec asymptote, which exceeds
  any plausible resonant-enhancement differential across the same span.

> **`REQ-CAV-00` — band of concern: 420 MHz – 3 GHz.** Lower edge set by the largest head in the SKU
> range; upper edge set by the source's own roll-off, not by the cavity, which continues to support
> modes above it. **Re-derive on any change to the radial stack, the head-size range, or the fastest
> edge inside the envelope** (§3's conditional).

> **⚠ The upper edge is derived against the INTERNAL source only, and that is a real limit of this
> document.** The 31.1 dB roll-off argument is a property of §3's digital edges. It says nothing
> about **external 6 GHz Wi-Fi ingress through the parting-plane seam** — a different excitation,
> with a source that does not roll off, and the one `NP-HEX-ZM-001` §5.3a and `RISK-20` are actually
> written against (λ/20 at 6 GHz ≈ 2.5 mm). **Above 3 GHz the foam is no longer electrically thin**
> and §6.2 shows it reaching 11–41 dB there, so the §6 verdict does **not** transfer to that case.
> §6.3's mechanism still should — tissue is *lossier* at 6 GHz than at 460 MHz, so the head should
> damp it harder — but **that case is not worked here.** **`OI-EMCCAV-06`.**

---

## 5. The requirement, in dB

### 5.1 The victim, and why the enclosure has to hold it

The victim is **inside** the shield with the source: the ADS1299 front end (µV) and the fluxgates.
The mechanism is **RF demodulation** — cavity RF rectified in the PGA input structures to a DC/LF
offset that lands directly in the EEG band.

It matters that this contributor is different in kind from the one `NP-DRV-SHELL-002` §9.1–§9.5
already handles:

| | Therapeutic-band self-field (`NP-DRV-SHELL-002` §9.3) | **RF demodulation (this document)** |
|---|---|---|
| Frequency | 0.5–100 Hz, in-band by construction | 420 MHz – 3 GHz, demodulated into band |
| Deterministic? | Yes — `REQ-EMI-03` phase-locks it | **No** — moves with posture, hand proximity, configuration |
| Subtractable? | Yes — `REQ-EMI-05` feed-forward | **No.** `REQ-EMI-05` predicts commanded current; it cannot predict a demodulated offset |
| Held by | layout (`REQ-EMI-06`), firmware | **the enclosure.** Nothing else is left |

Deriving the limit from the budget that already exists:

| Step | Value | Source |
|---|---:|---|
| EEG artifact budget, all LEDs at full PWM | 5.0 µVpp | **SH2-DRC-16** (`NP-DRV-SHELL-002` §9.1) |
| Allocated to RF demodulation | 20 % → **1.0 µVpp** | this document — it is the non-subtractable share |
| EMIRR at the amplifier input | 60 dB | **design assumption** — the softest input here; see §9 item 3 |
| → V_RF permitted at the input | **1.0 mV** | |
| In-tile electrode pickup (N4 beyond it is guarded, `REQ-EMI-02`) | 40 mm → h_e 20 mm | electrically short at 500 MHz (λ/15) |
| Common-mode → differential conversion at RF | 20 dB | the 110 dB CMRR spec is a 50/60 Hz figure and does not survive to hundreds of MHz |
| → coupling | 2.0 mV per (V/m) | |

> **`REQ-CAV-01` — E ≤ 0.5 V/m peak, 420 MHz – 3 GHz**, at the electrode plane, session-active, both
> bowls closed, head phantom fitted.

### 5.2 The enclosure's share, as a Q ceiling

`REQ-CAV-01` is a field limit, and field inside a cavity is set by source strength, coupling and **Q**.
Source and layout are already owned by `REQ-EMI-01`…`REQ-EMI-07`. **Q is the only term the enclosure
controls**, so it is the enclosure's allocation.

> **`REQ-CAV-02` — loaded Q ≤ 20 at every cavity mode in 420 MHz – 3 GHz**, measured with a head
> phantom fitted. Equivalently: the enclosure treatment supplies **≥ 26.2 dB** of peak-field reduction
> at resonance, against the bare-wall figure derived below.

**This is an allocation and is labelled one** — that is what an EMC requirement is, and unlike the
phrase it replaces, it is testable (§7). **The finding in §6 does not depend on the number chosen:**
at Q ≤ 5 or Q ≤ 50, the head still meets it and the foam still does not move it.

### 5.3 The external anchor

`NP-DT-001` `DI-REG-05` already commits T1 and T2 to **FCC Part 15 Subpart B Class B** radiated
emissions: 46.0 dBµV/m at 3 m over 216–960 MHz, 54.0 dBµV/m above 960 MHz; CISPR 11 Group 1 Class B
is comparable within ~1.5 dB once distance-corrected. Standard practice designs to ≥6 dB below.

Leakage through the parting-plane seam (`RISK-20`) and the posterior boss scales with the internal
field driving them, so **`REQ-CAV-02` is simultaneously the emissions allocation** — which is the same
observation `NP-BIB-EMF-001` §7.6 makes from the aperture side, arriving from the other direction.

---

## 6. What Layer 4 actually contributes — and the result is structural

### 6.1 The one-line reason a thin non-magnetic absorber does nothing

A lossy slab of thickness *d* laid against a conductor presents surface impedance

> `Z_in = j · η · tan(k·d)`

In the thin limit `tan(kd) → kd`, and **`η·k = η₀·k₀` exactly** — because `η ∝ 1/√ε_r` while
`k ∝ √ε_r`, so the permittivity cancels. Therefore

> `Z_in → j · η₀ · k₀ · d`

**— purely reactive, and independent of the loading.** Absorption is second order in (d/λ). At the
lowest mode, 3 mm is **λ/217**.

This is not a property of our particular foam. It is why **every thin commercial RF absorber is iron-
or ferrite-loaded**: a *magnetic* absorber works in the H-field maximum at a conductor wall, where a
*dielectric* one sits in the E-field null. And magnetic loading is precisely what `REQ-EMI-10` (no
conductive or magnetic addition without fluxgate re-qualification) and `NP-BIB-EMF-001` §7.8 (mu-metal
remanence) forbid in this enclosure.

### 6.2 The numbers

Mid-range head (57 cm), lowest mode 460 MHz. Cavity volume 4.06 L; shield boundary 1801 cm²; head
boundary 1034 cm² (36.5 % of the total).

**Bare-wall Q**, Pd-polyester liner only, head treated as lossless:

| Fabric surface resistance | Q |
|---|---:|
| 0.02 Ω/sq (best commercial) | 2047 |
| **0.1 Ω/sq (design assumption)** | **409** |
| 0.5 Ω/sq (worst commercial) | 82 |

**Layer 4's contribution**, swept across loadings far beyond anything a real open-cell carbon foam
reaches:

| Foam ε_r | R_s presented | Wall Q | **Q reduction** |
|---|---:|---:|---:|
| 1.5 − j0.3 (light) | 0.9 mΩ/sq | 409 → 406 | **0.08 dB** |
| 2.0 − j1.0 (design) | 3.0 mΩ/sq | 409 → 397 | **0.26 dB** |
| 3.0 − j3.0 (heavy) | 9.1 mΩ/sq | 409 → 375 | **0.76 dB** |
| 6.0 − j12 (**absurd** — included to show the conclusion is loading-independent) | 36.6 mΩ/sq | 409 → 300 | **2.71 dB** |

**Against a requirement of 26.2 dB.** Even at an implausible loading Layer 4 delivers a tenth of it.

> **Where the foam *does* work, honestly stated.** At 3 GHz the same calculation gives 11–41 dB,
> because 3 mm is no longer λ/217 there. That is a real result and it is the strongest available
> counter-argument to this document. It does not rescue the layer: by 3 GHz the source is 31.1 dB
> down (§4.2), and §6.3 removes the mode entirely at every frequency in band.

### 6.3 The wearer's head is the absorber, by ~50 dB

Dry skin at 460 MHz — ε_r 44, σ 0.44 S/m, **the least lossy tissue that could bound this cavity**
(muscle and grey matter are both lossier, so this understates the effect):

> η = 53.9 + j10.1 Ω → **R_s = 53.9 Ω/sq**, i.e. **539× the fabric wall**, over 36.5 % of the boundary.

| | Q |
|---|---:|
| Q_wall (Pd fabric) | 409 |
| Q_head (skin) | **1.32** |
| **Q_loaded** | **1.32** |

**`REQ-CAV-02` needs ≤ 20. The head delivers it with 23.6 dB to spare.** At Q ≈ 1.3 the mode is
over-damped: it is not a suppressed resonance, it is an **absent** one.

And the timing is not a coincidence — **every state in which §3's sources are energised is a state in
which the head is inside the cavity.** The cluster controllers and LED drivers run during a session;
a session requires tiles against the scalp. Mode 3 autonomous operation (CLAUDE.md §4.6) is still
worn. The only high-Q state is *bowls closed, powered, off the head* — a bench and FAI state, which
§7 covers.

### 6.4 The verdict

| | dB |
|---|---:|
| **`REQ-CAV-02` needs** (bare Q 409 → Q_L 20) | **26.2** |
| Layer 4 supplies, design loading | **0.26** |
| The wearer's head supplies | **49.8** |

**Layer 4 delivers 1.0 % of its own stated job**, in the band where that job exists, at a cost of 18 %
of the outward thermal path and two blocked fixes to a BLOCKING `OI-SINK-01`.

---

## 7. What would settle it on the bench

The calculation above is first-order (§9). **The measurement that closes this has never been
specified anywhere, and it is cheap** — it is one addition to `EMF-1`'s fixture and one extra sweep.

| ID | Method | What it decides |
|---|---|---|
| **`EMF-1a`** | S₂₁ between two small probes inside the closed assembly, 300 MHz – 6 GHz, **absorber fitted vs. absorber removed**, empty cavity | The only direct measurement of Layer 4's contribution ever proposed. §6.2 predicts < 1 dB below 1 GHz |
| **`EMF-1b`** | Same sweep, **head phantom fitted** (IEC 60601-1-2 / IEEE 1528 class tissue-equivalent) | §6.3 predicts the modes do not resolve at all. Confirms or refutes the whole finding |
| **`EMF-1c`** | Q extracted from the −3 dB width of each resolvable mode, across three head sizes spanning 52–62 cm | Confirms §4.1's population sweep, and produces the `REQ-CAV-02` compliance number |
| **`EMF-1d`** | `REQ-CAV-01` directly: E-field probe at the electrode plane, session-active, all 18 controllers and all LED drivers running | The requirement itself, rather than its Q proxy |

**All four run on the `EMF-1` fixture that does not yet exist.** This document adds no new bench
programme; it adds four sweeps to one that was already committed and has never run.

> **The deletion in §8 should not be executed before `EMF-1a`/`EMF-1b` run.** A first-order
> calculation is entitled to place a burden and to recommend; it is not entitled to remove a locked
> §4.3 layer on its own authority. That is the same limit `NP-BIB-EMF-001` §9.4 observed.

---

## 8. Recommendation, and what changes if it is taken

### 8.1 The thermal side is unblocked **now**, regardless of the layer decision

This is the part that does not wait for the principal or for the bench, and it is the immediately
useful result:

> **There is no electrical requirement on the Layer 4 station in 420 MHz – 3 GHz.** The station may
> therefore be specified on **thermal grounds alone**.

That closes the premise `OI-THCOOL-04` and `OI-THCOOL-15` have been waiting on. `NP-THERM-COOL-001`
§2 says the absorber is *"chosen for cavity-resonance suppression in dB"* and `OI-THCOOL-04` says it
is *"currently specified in dB only"* — **both are mistaken; there was never a dB figure to hold**,
which is the defect `OI-BIBEMF-08` was raised about. Thermal may proceed against 0.075 → ~0.02 m²K/W
without an EMC clearance, and `OI-THCOOL-15`'s gap pad may land on that station without one.

**But `NP-THERM-COOL-001` §6.3's proposed material is wrong on its own terms, and that correction is
this document's second contribution.** §6.3 proposes a *"conductive-filled absorber elastomer"*:

1. **"Conductive-filled absorber" is a contradiction.** Raising bulk *electrical* conductivity turns
   an absorber into a **reflector** — the wave no longer penetrates to be dissipated.
2. **It collides with the pad's own constraints.** `NP-THERM-COOL-001` §6.9.1 already requires the
   gap pad to be *"electrically insulating and non-magnetic"*, because a conductive or ferrous bridge
   between the inner-bowl fluxgates and the outer-bowl Helmholtz coils perturbs the cancellation.
   `REQ-EMI-10` says the same thing as a prohibition.
3. **The fix is routine, and `NP-THERM-COOL-001` §6.9.1 already names it.** Thermal conductivity comes from an
   **electrically insulating** filler — **BN, AlN or Al₂O₃** at k ≈ 1–3 W/m·K, i.e. §6.9.1's
   "ceramic-filled silicone" — not from a conductive one. Nothing lossy needs to be added at all,
   because §6 establishes the RF function does not exist.

> **`REQ-CAV-03` — any material occupying the Layer 4 station shall be electrically insulating
> (volume resistivity ≥ 10¹² Ω·cm) and non-magnetic (µ_r ≤ 1.01).** This is not a new constraint; it
> restates `REQ-EMI-10` and `NP-THERM-COOL-001` §6.9.1 at the station where the substitution happens,
> because §6.3's wording would have violated both.

### 8.2 The layer decision — delete the station and re-loft (Rev 3)

> **⚠ This section has been rewritten twice. Both earlier positions are stated, because neither was
> wrong about nothing.** Rev 1: *delete*, on the RF finding alone — right conclusion, but it had not
> checked what would occupy the vacated station. Rev 2: *substitute, do not delete* — it found the air
> regression below, which stands, but it loaded two costs onto deletion that **do not exist**, and
> rested the remainder on a function the foam does not have. Rev 3 keeps Rev 2's arithmetic and
> discards its conclusion.

**The two costs Rev 2 charged to deletion are not real.**

| Rev 2 claimed | The record says |
|---|---|
| Re-lofting the bowl is *"a shell tooling change `NP-REV-SHELL-001` gates"* | **No tooling exists.** `NP-REV-SHELL-001` is *"DRAFT — open review; no item signed"*, and CLAUDE.md's header puts the whole programme in **pre-tooling design phase**. Worse for the argument: `OI-ART-01` already requires `NP-TOOL-SHELL-001` to be **re-scoped or superseded**, because its F-01 still describes the retired 5-colour zone-slot scheme. A 3 mm re-loft is a CAD edit riding along with a re-scope that must happen anyway |
| *"5-layer"* is **a published claim** | **Nothing is externally published.** `competitive-position.md`'s own source note says the copy *"should be re-verified before publication"*, and the shielding line already carries a ⚠ that two of its words are unearned. *"5-layer"* is an internal string in a design document |

**And the third reason — compliance — was an inference, not a documented function.** Rev 2 argued the
foam is the compliant member taking up the ±0.5 tolerance stack. The record does not say that.
`NP-HEX-ZM-001` §5.4a specifies the preload path explicitly: **over-center lever-throw cluster clamps
with per-module spring plungers**, one actuator per cluster (`MECH-2`). That is the designed
compliance, and it is not the foam. The foam blocks `OI-THCOOL-15`'s gap pad because it is
*incidentally* compressible, which is a nuisance, not a function.

**What survives from Rev 2 is the arithmetic, and it is a condition on *how* to delete, not an
argument against deleting.** Vacating a 3 mm station does not delete its resistance — it fills it with
stagnant air at **0.115 m²K/W against the foam's 0.075, 54 % worse per mm**:

| Option | Outward path (m²K/W) |
|---|---:|
| today | 0.410 |
| **delete, gap not closed** | **0.450** — still a regression |
| **delete, outer bowl re-lofted 3 mm** | **0.335** ← **best available, and now nearly free** |
| substitute — ceramic-filled elastomer | 0.355 |

> **`REQ-CAV-04` (Rev 3) — delete the Layer 4 station, and the 3 mm re-loft of the outer bowl is
> BINDING, not optional.** Deletion without it inverts the thermal case. The two must move as one
> change.

**Why the remaining 0.020 m²K/W is not worth buying back.** Once the station is ceramic-filled its
*bulk* resistance is ~0.002 (3 mm at k ≈ 1.5) — the 0.020 in the substitution row is almost entirely
**contact resistance**, the price of pressing a compliant pad against two curved surfaces. So
substitution does not lose to deletion on the material; it loses on the interface it necessarily
creates. Deleting the station removes both.

**This is the cheapest moment this decision will ever be available.** No mould is cut, the shell
tooling spec is already open for re-scoping on unrelated grounds, and no external copy depends on the
number. Every month of tooling progress makes the same change more expensive, and nothing about the
RF finding is going to improve with age.

**What the deletion touches** — now correctly priced:

| File | Change | Real cost |
|---|---|---|
| `NP-HELMET-GEOM-001` §2, §70 | Remove the 3.0 mm station; radial total 30–35 → **27–32 mm** | CAD |
| `NP-TOOL-SHELL-001` | Carry the re-loft into the re-scope `OI-ART-01` already requires | **none — the re-scope is already owed** |
| `CLAUDE.md` §1, §4.3 · `hardware-detail.md` §4.3 · `DI-PERF-22` | *"5-layer"* → 4-layer | internal string edit |
| `competitive-position.md` §176 | copy edit **before** first publication | none — it is pre-publication and already flagged |
| `NP-THERM-COOL-001` §2 | the 18 % term leaves the outward path | it is the point |
| `scripts/check-section-refs.ts` | inbound citations re-verified | one CI run |

**The one thing that could bring a thin ceramic pad back, and it is a mechanical question, not an EMC
one.** If `MECH-2` finds the over-center clamps and spring plungers do **not** fully take up the
tolerance stack across the curved span once 3 mm of compliance leaves the gap, the station returns —
as a **thin ceramic-filled pad sized by the tolerance stack**, not by the 3.0 mm an RF justification
happened to pick. That lands at ~0.355 and is the documented fallback. **`OI-EMCCAV-08`** carries it,
and it is the only open question between here and executing the deletion.

**A renumbering caution.** `L5` must **not** silently become `L4`. Every citation of "Layer 4" in the
set currently means the absorber; renumbering makes each of them silently mean port filters instead.
Retain the gap, or renumber mechanically with a checked script — the same rule CLAUDE.md's footer
applies to section numbers.

**What still gates.** `EMF-1a`/`EMF-1b` (§7) confirm §6 and should run, but they gate *confidence*,
not the decision: they can only show the layer does less than 0.26 dB, never more, because §6.1's
result is structural. `OI-EMCCAV-06`'s 6 GHz ingress case is the one place a measurement could
change the answer, and it needs `EMF-1a` swept to 6 GHz **with the absorber fitted and removed** —
that is the sweep to run before the re-loft is cut into CAD.

---

### 8.3 How much space do the clamps need — and does the re-loft take any of it?

`OI-EMCCAV-08` was raised as *"the one question between here and executing the deletion."* Worked
against the geometry, **it largely answers itself, and in the opposite direction to the way §8.2
first framed it.**

**What is allocated.** `NP-HELMET-GEOM-001` §2 gives the clamp two radial budgets:

| Where | Element | Radial (mm) | Tol |
|---|---|---:|---|
| **inside L1** | cluster-clamp + lever features, on L1's outer face | **3.0–4.0** | ±0.3 |
| **the Gap** | inter-bowl clamp **travel** + blind-mate boss + labyrinth lip | **5–7** | ±0.5 |
| | **total radial budget available to the clamp** | **8–11** | |

**What is specified.** Nothing dimensional. `NP-HEX-ZM-001` §5.4a specifies the actuator
*functionally* — a large easy-grip control, a **push/pull over-center lever throw (not a twist cam)**,
low input force by mechanical advantage, one-handed, clear open/closed state, validated by HFE
formative against `RISK-22` — and `MECH-2` is still open on exactly this: *"Remaining: over-center
lever-throw actuator … + per-module spring plungers, curvature span at 122 mm, low one-handed input
force."*

> **So the 5–7 mm is an allocation, not a derived requirement.** No throw, stroke or plate-lift figure
> exists anywhere in the document set. **That is the same defect shape as Layer 4's, one level down**
> — a dimension in the radial stack that no stated requirement produced — and it is worth naming as
> such even though it is `MECH-2`'s to close, not this document's.

**Does the re-loft take any of it? No — and this is the part that decides the item.** The absorber
sits at station **L2, on the outer bowl's inner face, outboard of the Gap**. The clamps live in the
Gap. Deleting the absorber moves the Pd-polyester liner and everything outboard of it **3 mm inward
together**, so:

- **the 5–7 mm Gap is preserved unchanged** — the clamp loses no travel;
- **the 3.0–4.0 mm of clamp features inside L1 is untouched** — L1 does not move at all;
- what changes is only **what the clamp's far end reacts against**.

**And that change is an improvement, not a cost.** Today the counterface is compressible carbon foam.
An over-center mechanism reacting against a compliant counterface has **degraded throw authority and
a drifting over-center point** — foam compression set moves it over service life, across repeated
bowl separations for module replacement. After deletion the counterface is **rigid Pd-polyester on
mu-metal on CFRP**, which is what an over-center lever wants.

**The tolerance argument also runs backwards from how §8.2 Rev 2 put it.** The clamp load path runs
from L1's outer face to the first rigid counterface on the outer bowl. Summing `NP-HELMET-GEOM-001`
§2's tolerances along it:

| | contributors | worst case | RSS |
|---|---|---:|---:|
| today | clamp features ±0.3 · Gap ±0.5 · **absorber ±0.5** | **±1.30** | ±0.77 |
| after deletion | clamp features ±0.3 · Gap ±0.5 | **±0.80** | ±0.58 |

**Deleting the absorber shrinks the stack the clamp must take up by 38 % worst-case (24 % RSS)**,
because the foam is not only a compliance *provider*, it is a ±0.5 tolerance *contributor*. Rev 2
counted the first and missed the second.

> **`OI-EMCCAV-08` is therefore narrowed to a residual, and it is `MECH-2`'s anyway:** the per-module
> spring plungers must cover a **±0.80 stack instead of ±1.30**, against a plunger stroke nobody has
> specified yet. **That is a strictly easier requirement than the one they face today**, so it cannot
> block the re-loft — it can only be confirmed when `MECH-2` selects the actuator.

**One more consequence, and it closes a BLOCKING-adjacent item outright.** `OI-THCOOL-15`'s central
question is *"what the pad compresses against"* — the answer today being the compressible foam, so
*"a pad pressed against foam compresses the foam and never reaches rated conductivity"*
(`completed-decisions.md` §221). **Delete the station and the pad presses against the rigid bowl.**
The question does not get answered; it **ceases to exist.**

### 8.4 Every "5-layer" string the deletion touches — and the homonym trap

A find-and-replace here would corrupt four unrelated lines. **There are two different "five-layers"
in this document set**, and only one of them is the EMF stack.

| Carries the **EMF** stack — update | |
|---|---|
| `CLAUDE.md` §1 (founding principle), §4.3 heading | `docs/reference/hardware-detail.md` §4.3 heading |
| `NP-DT-001` `DI-PERF-22` + its trace row | `NP-ART-001` A6 |
| **`NP-ENV-OPRANGE-001` §2** — *"passive 5-layer shield always present"* | `NP-HELMET-GEOM-001` §134, §268 |
| `NP-HELMET-GEOM-ISA` `ISC-4` | `NP-HEX-ZM-001` §773 |
| `NP-HW-FITOVER-001` §145 | `NP-PWR-THERM-001` §498 |
| `NP-RM-001` §184 | `NP-THERM-COOL-001` §549 |
| `NP-THERM-SINK-001` §42 (*"five-layer outer bowl"*) | `NP-TOOL-HUB-001` §40 |
| `competitive-position.md` §23 | `regulatory-strategy.md` §11 |

> **⚠ DO NOT TOUCH these — they are the retired RISK-15 zone-module KEYING scheme, a different
> five-layer entirely:** `NP-DT-001` `DI-USE-05` and `DO-HW-01`, `NP-RISK-003` §41,
> `durability-maintenance.md` §18. All four say *"five-layer keying"*, and all four describe a scheme
> **retired 2026-07-28**.

**`NP-ENV-OPRANGE-001` §2 is the one that is not cosmetic.** Its wording is the evidence for **D2**,
Layer 2's degraded-mode dependency (`hardware-detail.md` §4.3). The sentence must become *"passive
4-layer shield always present"* — the substance is unaffected, because Layer 4 contributes nothing to
the ELF figure that sentence is about, but **editing it without understanding that it is load-bearing
for a different layer is how a real dependency gets deleted by a search-and-replace.**

---

## 9. What this document does **not** establish

Stated explicitly, because an analysis with a clean answer is easy to over-read.

1. **It measures nothing.** `EMF-1`, `EMF-1a`–`EMF-1d`, `EMF-3`, `RISK-20` and `OI-THCOOL-06` are all
   open and unchanged. The 35–45 dB / 40–60 dB figures remain design targets.
2. **The models are first-order.** The head is a homogeneous sphere with a skin surface impedance; the
   enclosure is a spherical shell; the mode estimate is a circumference count, not a full-wave
   solution. A real head is stratified and aspherical, and real modes are not degenerate.
   **Every simplification was taken in the conservative direction** — least lossy tissue, lossless
   head for the bare-wall figure, no credit for the L1 module bodies that also sit in the cavity — so
   the 23.6 dB margin is a floor. But a floor computed from a first-order model is still first-order.
3. **The 60 dB EMIRR is the softest input, and it is the one worth measuring first.** The ADS1299
   datasheet carries no EMIRR specification. It is a bench measurement on a part already in hand, and
   `REQ-CAV-01`'s 0.5 V/m moves decade-for-decade with it. **`OI-EMCCAV-01`.**
4. **It does not clear `RISK-SINK-03`.** The `SPEC-SINK-04` graphite spreader sits on the **exterior**
   of the CFRP, outside the shield stack entirely, so nothing here bears on it — except to narrow the
   question: an exterior conductor cannot change the internal cavity Q, so what `OI-SINK-01` needs
   cleared is **ELF eddy-current loading of the Helmholtz actuator** (`REQ-EMI-11`'s transfer
   function) and external RF, not cavity behaviour. That narrowing is offered to `OI-SINK-01`, which
   stays **BLOCKING**. **`OI-EMCCAV-02`.**
5. **It does not modify any locked decision.** CLAUDE.md §4.3 and §1 are untouched. §8.2 raises a
   reopening; it performs none.
6. **It does not budget the heat that moves when the station changes.** Lowering outward resistance
   is good for the scalp but puts more heat into the **outer bowl**, which carries the mu-metal and
   the Helmholtz coils. `NP-ENV-OPRANGE-001` §2 already makes the cancellation envelope **SOFT** on
   temperature drift, and `REQ-EMI-11` calibrates a coil-drive→field transfer function that moves
   with coil resistance. That applies to the substitution as much as to the deletion, and nobody has
   budgeted it. **`OI-EMCCAV-07`.**
7. **It does not price anything.** §8.2's Rev 1 phrase *"negative BOM cost"* was an assumption, not a
   figure: `OI-BIBEMF-07` records that the shielding stack has **no BOM line at all**, so what
   deleting the foam would save is unknown, and so is what a ceramic-filled elastomer would cost.
8. **The empty-cavity state is real and is not dismissed.** Bowls closed, powered, off the head, Q is
   409 and Layer 4 still supplies 0.26 dB of the 26.2 dB needed — so the layer is not the answer
   there either. What *would* be, if `EMF-1a` finds a problem, is aperture and seam resistive control
   (`NP-BIB-EMF-001` §7.6), not layer count.

---

## 10. Open items

| ID | Item | Owner | Blocking? |
|----|------|-------|-----------|
| **`OI-EMCCAV-01`** | **Measure the ADS1299's EMIRR at 420 MHz – 3 GHz.** §5.1's 60 dB is the softest input in this document and `REQ-CAV-01` moves decade-for-decade with it. Bench measurement on a part in hand | EE Lead | No — bounds `REQ-CAV-01` |
| **`OI-EMCCAV-02`** | `RISK-SINK-03` narrowed, not cleared: the exterior spreader cannot affect cavity Q, so `OI-SINK-01`'s EMF clearance reduces to **ELF eddy loading of the Helmholtz actuator** + external RF. Re-scope the item to those two mechanisms | EMC + Thermal | No — narrows a **BLOCKING** item |
| **`OI-EMCCAV-03`** | **`OI-HUB-C19` is now an EMC decision.** If the 15–20 V → 24 V boost is ever sited inside the helmet envelope rather than on the Hub PCB, §4's band and §6's verdict must both be re-derived — a switching converter is an order of magnitude above anything in §3's table | EE Lead + EMC | Gates any re-siting of the boost |
| **`OI-EMCCAV-04`** | Add `EMF-1a`–`EMF-1d` (§7) to the `EMF-1` fixture's test plan. Four sweeps on a fixture already committed. ~~Gates §8.2's deletion~~ — **Rev 2: no longer gating**, because §8.2 now recommends substitution, which does not depend on them. They are what would let `REQ-CAV-04`'s justification be stated as *measured*; **sweep `EMF-1a` to 6 GHz** for `OI-EMCCAV-06` | EMC | No — **re-scoped at Rev 2** |
| **`OI-EMCCAV-06`** | **The band's upper edge is derived against the INTERNAL source only (§4.2).** The 31.1 dB roll-off is a property of §3's digital edges and says nothing about **external 6 GHz Wi-Fi ingress through the parting-plane seam** — the case `NP-HEX-ZM-001` §5.3a and `RISK-20` are actually written against. **Above 3 GHz the foam is no longer electrically thin** (§6.2 shows 11–41 dB), so §6's verdict does **not** transfer. Tissue is lossier at 6 GHz so §6.3's mechanism should hold harder, but **the case is not worked**. Sweep `EMF-1a` to 6 GHz | EMC | **Bounds §6's scope** |
| **`OI-EMCCAV-08`** | ~~The only question between here and executing the deletion~~ **NARROWED 2026-09-20 by §8.3, and it no longer gates.** The absorber sits **outboard of the Gap**, so the re-loft takes **no clamp space at all**: the 5–7 mm Gap and the 3.0–4.0 mm of clamp features inside L1 are both preserved, and only the clamp's *counterface* changes — from compressible foam to rigid Pd/mu-metal/CFRP, which is **better** for an over-center mechanism whose over-center point would otherwise drift with foam compression set. The tolerance argument also inverts: the foam is a **±0.5 contributor**, so deleting it **shrinks the clamp's stack 38 % worst-case (±1.30 → ±0.80)**. **Residual, and it is `MECH-2`'s regardless:** confirm the per-module spring-plunger stroke covers ±0.80 — *a strictly easier requirement than today's ±1.30* | ME Lead (**`MECH-2`**) | **No — narrowed, no longer gating** |
| **`OI-EMCCAV-09`** | **The 5–7 mm inter-bowl Gap is an allocation, not a derived requirement.** `NP-HEX-ZM-001` §5.4a specifies the cluster-clamp actuator only *functionally* (over-center lever throw, one-handed, low input force, `RISK-22`); **no throw, stroke or plate-lift figure exists anywhere in the document set**, and `MECH-2` is open on exactly this. Same defect shape as Layer 4's, one level down — a dimension in the radial stack that no stated requirement produced | ME Lead (**`MECH-2`**) | No — but it is the next one of these |
| **`OI-EMCCAV-07`** | **Nobody has budgeted the heat that moves when this station is re-specified.** Lowering outward resistance puts more heat into the **outer bowl**, which carries the mu-metal and the Helmholtz coils. `NP-ENV-OPRANGE-001` §2 already makes the cancellation envelope **SOFT** on temperature drift, and `REQ-EMI-11` calibrates a coil-drive→field transfer that moves with coil resistance. Applies to the substitution as much as to a deletion | Thermal + EE Lead | No — but it is a **new** coupling |
| **`OI-EMCCAV-05`** | **`IEC 60601-1-2` is still absent from `regulatory-strategy.md` §8.** §5.3 leans on Part 15B, which *is* in the record via `DI-REG-05`, but the immunity half of this analysis has no standards home. Same gap `OI-BIBEMF-04` raises from the Layer 2 side | Regulatory | No — but two documents now depend on it |

---

## 11. Revision history

| Rev | Date | Author | Change |
|-----|------|--------|--------|
| 1 | 2026-09-20 | NeurOne EMC / Systems Engineering | **Initial release, discharging `OI-BIBEMF-08` step 1 — the gate `NP-BIB-EMF-001` §7.3 placed on EMC.** States Layer 4's requirement against a named source and band for the first time, and reaches an answer §7.3 did not anticipate. **(1) The source is named and it is not a radio** — `NP-DRV-SHELL-002` puts 18 STM32G071 cluster controllers, a 400 kHz I2C tree, 80 PWM LED drivers and the ADS1299 SPI bank *inside* the envelope; §9.6 already states that a Faraday cage does not protect victims sharing it. §7.3's "the radios live in the hub" is the right bound for ingress and the wrong one for this question. **(2) The band is 420 MHz – 3 GHz, and its lower edge is a property of the wearer** — the lowest circumferential mode runs 506 MHz at a 52 cm head to 422 MHz at 62 cm, so the enclosure's resonance sweeps 84 MHz across one SKU; recorded nowhere before. **(3) The requirement is stated in dB** — `REQ-CAV-01` E ≤ 0.5 V/m derived from SH2-DRC-16 via RF demodulation, which unlike the therapeutic-band self-field is *not* subtractable by `REQ-EMI-05`; `REQ-CAV-02` allocates the enclosure's share as Q_L ≤ 20, i.e. ≥ 26.2 dB. **(4) Layer 4 supplies 0.26 dB of it, and that result is structural** — for a lossy slab on a conductor `Z_in = j·η·tan(kd)`, and in the thin limit `η·k = η₀·k₀` exactly, so `Z_in → j·η₀·k₀·d` is purely reactive **independent of the loading**; at 3 mm the foam is λ/217 at the lowest mode. Swept to an absurd ε_r of 6 − j12 it still reaches only 2.71 dB. This is why thin commercial absorbers are magnetically loaded — which `REQ-EMI-10` and `NP-BIB-EMF-001` §7.8 forbid here. **(5) The wearer's head is the absorber, by 49.8 dB** — dry skin (the least lossy candidate) presents 53.9 Ω/sq over 36.5 % of the boundary, taking Q from 409 to 1.32, and every state in which the §3 sources are energised is a state in which the head is inside the cavity. **Outcome: neither §7.3 branch.** Not 2a (there is no RF function to preserve) and not 2b's stated reason (EMC is not silent) — the requirement exists and Layer 4 is not what meets it. **Two results that do not wait for that decision:** the Layer 4 station may now be specified on thermal grounds alone, closing the premise `OI-THCOOL-04`/`OI-THCOOL-15` were waiting on; and `NP-THERM-COOL-001` §6.3's proposed *"conductive-filled absorber elastomer"* is corrected — a conductive fill makes a reflector, not an absorber, and collides with §6.9.1's own insulating/non-magnetic constraint and with `REQ-EMI-10`; the filler must be ceramic (BN/AlN/Al₂O₃), and nothing lossy need be added at all (`REQ-CAV-03`). Specifies `EMF-1a`–`EMF-1d`, four sweeps on the already-committed `EMF-1` fixture, as what would settle it (§7). Raises `OI-EMCCAV-01…05`, two gating. Adds `scripts/check-cavity-q.ts` (`CI-Kind: report`), which reproduces all 14 published anchors and fails on drift. **No locked section modified; no layer removed; no measurement asserted; `EMF-1`, `EMF-3`, `RISK-20` and `OI-THCOOL-06` all unchanged and open.** |
| 2 | 2026-09-20 | NeurOne EMC / Systems Engineering | **Rev 1's §8.2 recommendation is REVERSED: substitute the Layer 4 station, do not delete it.** The RF finding is unchanged and unchallenged — the layer still supplies **0.26 dB of a 26.2 dB requirement**, and §3–§6 stand as written. What Rev 1 got wrong is the **thermal** half. It inherited `OI-BIBEMF-08`'s assumption that removing the layer recovers its 18 % of the outward path, and never asked what would occupy the station afterwards. **Vacating a 3 mm station does not delete its resistance — it fills it with stagnant air, and air is a worse insulator per mm than the foam:** 0.115 against 0.075 m²K/W, a **54 % regression**, both conductivities taken from `NP-THERM-COOL-001` §2's own table. Through to the outward path: today **0.410**; delete without closing the gap **0.450** (*a regression, the opposite of the intent*); delete with a 3 mm re-loft of the outer bowl **0.335**, but that is a shell tooling change `NP-REV-SHELL-001` gates; **ceramic-filled substitution 0.355 with no tooling change at all.** Substitution therefore lands within **0.020 m²K/W** of the best case for free, and deletion wins only if the re-loft actually happens — which reverses Rev 1's ordering. **Three further reasons Rev 1 did not weigh:** the foam is the **compliant member** taking up a ±0.5 tolerance stack across a curved 5–7 mm gap and preloading the clamps and latches — its compressibility is exactly why `OI-THCOOL-15` exists, and a non-compressible ceramic-filled part *keeps* that budget while unblocking the pad, whereas an empty gap keeps neither; **nothing is priced** (Rev 1's *"negative BOM cost"* was an assumption, and `OI-BIBEMF-07` records that the stack has no BOM line at all); and substitution **leaves the published claim and the renumbering hazard untouched**. New **`REQ-CAV-04`**: the station is retained and re-specified, and **its justification of record changes from "cavity-resonance suppression" — which §6 refutes — to tolerance compliance and a thermal path**. Keeping a layer for a reason it does not serve is the defect `OI-BIBEMF-08` was raised about; keeping it under a corrected justification is not. The deletion option is retained, costed, in §8.2, with the re-loft marked **not optional**. **Two limits of Rev 1 also recorded.** **`OI-EMCCAV-06`** — §4.2's upper band edge is derived against the **internal** source only; the 31.1 dB roll-off says nothing about **external 6 GHz Wi-Fi ingress through the parting-plane seam**, the case `NP-HEX-ZM-001` §5.3a and `RISK-20` are actually written against, and **above 3 GHz the foam is no longer electrically thin** (§6.2 gives it 11–41 dB there), so §6's verdict does not transfer; tissue is lossier at 6 GHz so §6.3's mechanism should hold harder, but the case is **not worked**. **`OI-EMCCAV-07`** — nobody has budgeted the heat that moves into the **outer bowl** (mu-metal + Helmholtz) when outward resistance falls, against `NP-ENV-OPRANGE-001` §2's SOFT temperature envelope and `REQ-EMI-11`'s transfer function; it applies to the substitution too. `OI-EMCCAV-04` re-scoped: `EMF-1a`/`EMF-1b` no longer gate anything, since substitution does not depend on them — they are what would make `REQ-CAV-04`'s justification *measured*, and `EMF-1a` should now sweep to 6 GHz. §1's summary table, §4.2, §8.2 and §9 (items 6 and 7 added) updated; `scripts/check-cavity-q.ts` gains §7's thermal comparison and six anchors, 16 → 22. **Still no locked section modified, no layer removed, no measurement asserted.** |
| 3 | 2026-09-20 | NeurOne EMC / Systems Engineering | **§8.2 rewritten a second time, and it lands on Rev 1's conclusion by Rev 2's arithmetic: delete the station, and the 3 mm re-loft is BINDING.** Rev 2 argued *substitute, do not delete* on three grounds. **Two were phantom costs.** (1) It called the re-loft *"a shell tooling change `NP-REV-SHELL-001` gates"* — but **no tooling exists**: that document is *"DRAFT — open review; no item signed"*, CLAUDE.md's header puts the programme in **pre-tooling design phase**, and `OI-ART-01` already requires `NP-TOOL-SHELL-001` to be re-scoped or superseded because its F-01 still describes the retired 5-colour zone-slot scheme. The re-loft is a CAD edit riding along with a re-scope already owed. (2) It called *"5-layer"* **a published claim** — but **nothing is externally published**; `competitive-position.md`'s own source note says the copy *"should be re-verified before publication"*, and its shielding line already carries a ⚠ that two of its words are unearned. **The third ground was an inference, and the record contradicts it.** Rev 2 claimed the foam is the compliant member taking up the ±0.5 tolerance stack; `NP-HEX-ZM-001` §5.4a specifies the preload path as **over-center lever-throw cluster clamps with per-module spring plungers** (`MECH-2`). The foam is *incidentally* compressible — which is why it blocks `OI-THCOOL-15`'s pad — not a designed compliant member. **What survives from Rev 2 is its arithmetic, re-cast as a condition on HOW to delete rather than an argument against deleting:** vacating 3 mm fills it with stagnant air at 0.115 against the foam's 0.075, so deletion without the re-loft is still a regression (0.410 → 0.450), while **delete + re-loft reaches 0.335** — the best figure available — against substitution's 0.355. **`REQ-CAV-04` restated: delete the station, and the 3 mm re-loft is binding, not optional; the two move as one change.** Also records why the residual 0.020 is not worth buying back: once ceramic-filled, the station's *bulk* resistance is ~0.002, so that 0.020 is almost entirely **contact resistance** — the price of the interface substitution necessarily creates, which deletion removes. And notes that **this is the cheapest moment the decision will ever be available**: no mould cut, the shell tooling spec already open for re-scoping on unrelated grounds, no external copy depending on the number. New **`OI-EMCCAV-08`** carries the one remaining question, and it is mechanical rather than EMC: once 3 mm of incidental compliance leaves the gap, do the clamps and spring plungers still take up the tolerance stack? If not, the station returns as a **thin ceramic-filled pad sized by the tolerance stack** — not by the 3.0 mm an RF justification picked — at ~0.355, the documented fallback. `EMF-1a`/`EMF-1b` gate **confidence, not the decision**: §6.1's result is structural, so they can only show the layer does less than 0.26 dB, never more. `OI-EMCCAV-06`'s **6 GHz ingress case is the one place a measurement could change the answer**, and that sweep — absorber fitted vs removed, to 6 GHz — should run before the re-loft is cut into CAD. §1's summary table corrected (both outcome rows were stale). **Still no locked section modified, no layer removed in this document, no measurement asserted.** |
| 4 | 2026-09-20 | NeurOne EMC / Systems Engineering | **§8.3 answers `OI-EMCCAV-08` and narrows it out of the gating position; §8.4 enumerates every *"5-layer"* string the deletion touches, with a homonym trap flagged.** **The clamps lose no space to the re-loft.** `NP-HELMET-GEOM-001` §2 allocates the cluster clamp **3.0–4.0 mm** of lever features inside L1 plus the **5–7 mm** inter-bowl Gap for travel — **8–11 mm** total. The absorber sits at station **L2, outboard of the Gap**, so deleting it moves the Pd liner and everything outboard **3 mm inward together**: the Gap is preserved unchanged, L1 does not move, and **only the clamp's counterface changes** — from compressible foam to rigid Pd/mu-metal/CFRP. **That is an improvement**: an over-center mechanism reacting against a compliant counterface has degraded throw authority and an over-center point that drifts with foam compression set across repeated bowl separations. **The tolerance argument also runs backwards from Rev 2's**: the foam is not only a compliance provider, it is a **±0.5 tolerance CONTRIBUTOR**, so the clamp load path (features ±0.3 · Gap ±0.5 · absorber ±0.5) **shrinks from ±1.30 to ±0.80 worst-case — 38 % — on deletion**. `OI-EMCCAV-08` is therefore **narrowed and no longer gating**: the residual is confirming the per-module spring-plunger stroke covers ±0.80, *a strictly easier requirement than today's ±1.30*, and it is `MECH-2`'s regardless. **One item closes outright as a side effect:** `OI-THCOOL-15`'s central question — *"what the pad compresses against"* — **ceases to exist** once the pad presses on the rigid bowl. **New `OI-EMCCAV-09`:** the 5–7 mm Gap is itself **an allocation, not a derived requirement** — `NP-HEX-ZM-001` §5.4a specifies the actuator only functionally (over-center lever throw, one-handed, `RISK-22`) and **no throw, stroke or plate-lift figure exists anywhere in the document set**, which is the same defect shape as Layer 4's, one level down. **§8.4** lists the 16 documents carrying an EMF *"5-layer"* string, and flags four that must **not** be touched — `DI-USE-05`, `DO-HW-01`, `NP-RISK-003` §41 and `durability-maintenance.md` §18 all say *"five-layer keying"*, the **retired RISK-15 zone-module scheme**, a different five-layer entirely. It also singles out **`NP-ENV-OPRANGE-001` §2** as the one edit that is not cosmetic: its *"passive 5-layer shield always present"* is the evidence for **D2**, Layer 2's degraded-mode dependency, so it must be updated knowingly rather than by search-and-replace. `scripts/check-cavity-q.ts` gains the clamp-stack arithmetic and three anchors, 22 → 25. **No locked section modified; no layer removed; no measurement asserted.** |
