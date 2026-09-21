# Enclosure Cavity Resonance — Source, Band, and the Layer 4 Requirement

**Project:** NeurOne
**Document:** NP-EMC-CAV-001
**Revision:** 12
**Date:** 2026-09-21
**Status:** ACTIVE — EMC analysis discharging `OI-BIBEMF-08` step 1; asserts no measurement
**Effective Date:** 2026-09-20
**Author:** NeurOne EMC / Systems Engineering
**Approved By:** Pending — principal decision required (§8)
**References:** CLAUDE.md §1, §3, §4.1, §4.3, §4.4; `docs/reference/hardware-detail.md` §4.3; `NP-BIB-EMF-001` §7.3, §7.6, §7.8; `NP-HELMET-GEOM-001` §2, §3.2; `NP-DRV-SHELL-002` §3.2, §3.4, §3.5, §5.4, §9.1–§9.6; `NP-HEX-ZM-001` §5.2, §5.3a, §5.3.1; `NP-THERM-COOL-001` §2, §6.3, §6.9.1; `NP-THERM-SINK-001` `RISK-SINK-03`, `OI-SINK-01`, `OI-SINK-07`; `NP-DT-001` `DI-PERF-22`, `DI-REG-05`; `NP-HW-HUB-001` §8.1; FCC Part 15 Subpart B §15.109; CISPR 11 Group 1 Class B; IEC 60601-1-2; Gabriel et al. 1996 (tissue dielectric properties)
**Related Issues:** GitHub Issue #362; `OI-BIBEMF-08`; `OI-THCOOL-04`; `OI-THCOOL-15`; `OI-SINK-01`; `OI-SINK-07`; `RISK-SINK-03`; `EMF-1`; `EMF-3`; `RISK-20`; `OI-HUB-C19`
**Gate:** —
**IEC 62304 Class:** — (analysis record, not device software)
**Jurisdiction Scope:** N/A
**Change Summary:** Rev 12 **withdraws Rev 11's "lattice compresses at the rim" caveat** — tiles are one universal mould, so `PACK-1`'s 23.09 mm is **constant at every cluster**. Rev 11 measured 3-space chords on a curved surface. See §11.

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

### 8.5 FLUSH-1 re-opens the Gap — and it is a bigger lever than Layer 4 ever was

**Principal direction, 2026-09-20 — `FLUSH-1` (`NP-HEX-ZM-001` §5.4a):** *the cluster lever is flush
when closed, and throws only with the bowls separated.*

**The second half is a fact this document set already contained without drawing its consequence.**
`NP-HEX-ZM-001` §5.2: *"The module cluster clamps sit in the inter-bowl gap and are **reached by
unclamping the bowls**."* §5.5: *"During a module swap the bowls are open."* **So the lever's swept
volume is a bowls-OPEN volume.**

> **`NP-HELMET-GEOM-001` §2's Gap line item is therefore mis-named.** It reads *"**inter-bowl clamp
> travel** + blind-mate boss + labyrinth lip"* at **5–7 mm**, but **travel is not an assembled-state
> requirement** — the throw happens with the bowls apart, and the gap closes only after the lever is
> home. §8.3 called the 5–7 mm *"an allocation, not a derived requirement"* (`OI-EMCCAV-09`).
> **`FLUSH-1` says what it was mis-allocated for.**

**What actually sets the assembled gap**, once travel is off the list:

| Contributor | Distributed or local? |
|---|---|
| **closed** footprint of the lever (18 clusters) | distributed — and `FLUSH-1` minimises it by construction |
| labyrinth lip | at the rim |
| ~~fluxgate sensors~~ | **NOT A GAP CONTRIBUTOR — see §8.6.** They are *in* the inner-bowl wall, not in the gap |
| blind-mate sensor/coil boss | **a standalone posterior-centre feature** (§5.3c) — **local to the occiput, not across the vault** |

**So a locally-relieved gap is available — deep at the boss, shallow over the lattice — and it was not
previously on the table**, because "travel" implied a uniform swept clearance everywhere.

**The prize, and it dwarfs the layer this document was written about.** The inter-bowl gap is
**stagnant air at 0.231 m²K/W — 56 % of the entire outward path and its single largest term**
(`NP-THERM-COOL-001` §2). At k = 0.026:

| Gap | R_gap | Outward total | Recovered |
|---:|---:|---:|---:|
| 7 mm | 0.269 | 0.448 | −0.038 |
| **6 mm (today)** | **0.231** | **0.410** | — |
| 5 mm | 0.192 | 0.372 | +0.038 |
| 4 mm | 0.154 | 0.333 | +0.077 |
| 3 mm | 0.115 | 0.295 | +0.115 |

> **Each millimetre of gap is worth 0.0385 m²K/W. Narrowing it 2 mm recovers more than deleting the
> entire Layer 4 absorber (0.077 vs 0.075); 3 mm recovers 0.115 — half again as much.**

**Against `NP-THERM-COOL-001` §6.1's sealed-recirculation proposal** (0.231 → 0.067, recovery 0.164,
requiring a motor inside the sealed cavity), **narrowing to 3 mm reaches ~70 % of the prize with no
motor, no power draw, no moving part and no BOM line.** The two **interact rather than compose** — a
narrower gap is a smaller volume at higher flow resistance to stir — so they must be **traded, not
stacked**, and §6.1's 0.067 is not additive with anything here.

**This is outside this document's scope and is stated only because `FLUSH-1` surfaced it here.**
Dimensioning the assembled gap is `MECH-2`'s and the thermal case is `NP-THERM-COOL-001`'s;
`OI-EMCCAV-09` is re-scoped to carry the hand-off. **The EMC position is narrow and unchanged:** the
gap is inside the Faraday envelope either way (`NP-HEX-ZM-001` §5.2), narrowing it moves no aperture,
and §4.1's cavity modes shift only with the *total* scalp-to-conductor dimension — across which the
420 MHz lower edge was already computed over a head-size span far larger than any gap change
contemplated here.

---

### 8.6 What the boss and the fluxgates actually need — and which way the boss should project

Two questions put to this analysis after §8.5. The first corrects §8.5's own table.

#### 8.6.1 The fluxgates need **zero** gap — §8.5 was wrong to list them

§8.5 named the fluxgate sensors as a Gap contributor. **They are not one.** Three places in the
record put them *inside the inner bowl*, not in the gap:

| Source | What it says |
|---|---|
| `NP-HELMET-GEOM-001` §2 | they live in the **"Inner-bowl socket wall + FPC channel"** station — **2.0–2.5 mm**, inboard of the Gap entirely |
| `NP-HEX-ZM-001` §5.3(c) | *"fluxgate magnetometers on the inner bowl (**near the scalp**, where they must sense the field the wearer experiences)"* |
| `NP-HEX-ZM-001` §5.1 | lists them among the **inner bowl's** contents, alongside the sockets |

*"Near the scalp"* is decisive: the Gap is on the **far** side of L1 from the scalp. Putting the
sensors there would defeat the reason §5.3.1 split them from the coils in the first place.

> **A discrepancy this exposes, and it is not mine.** `completed-decisions.md` §221 states that
> *"§5.3c's **fluxgates** sit there"* — in the inter-bowl gap — while listing keep-outs that forbid a
> continuous gap-pad sheet. **That contradicts both owning documents.** The likely reconciliation is
> that the fluxgate *keep-out* (a magnetic clearance volume) reaches into the gap while the sensor
> *body* does not — which would still forbid a pad over it, so §221's conclusion survives even though
> its stated reason does not. **This matters for `OI-EMCCAV-09`**: a keep-out is a *planform*
> exclusion, not a *radial* one, and only a radial constraint sets the gap. **`OI-EMCCAV-11`.**

#### 8.6.2 The boss is the only real gap constraint, it is undimensioned, and it is already owed

No throw, mating depth or projection figure for the blind-mate boss exists anywhere in the document
set — the same finding as `OI-EMCCAV-09`, one component further in. What *is* known is what has to
cross it, and it is not small:

- **216 interface pins in 20 tail groups** (`NP-DRV-SHELL-002` §5.3, §10.1), plus the fluxgate/coil
  harness;
- in **four segregated contact groups with independent returns** — `{N1 power}`, `{N2/N5
  digital+safety}`, `{N4 post-ADC digital}`, `{fluxgate/coil harness}` — star-returned at the Hub PCB
  with **no shared return** between the power group and the fluxgate/coil group (`§4.3`, `REQ-EMI-05`,
  `OI-SHELL2-02`);
- **mated automatically as the bowls draw closed** (`NP-HEX-ZM-001` §5.3(c)), so it needs real
  blind-mate depth: contact wipe + lead-in chamfer + the ±0.4 lateral / ±0.5 Z blind-mate tolerance.

**This document does not put a number on it** — that is `MECH-1`'s, and `NP-DRV-SHELL-002` §4.3
already says so in terms: *"it constrains the boss contact layout, which **MECH-1** tools. It must be
settled before MECH-1 cuts the posterior boss."* **The dimension is already owed; FLUSH-1 only makes
it urgent, because it is now the one thing left setting the Gap.**

#### 8.6.3 Does the boss need to project inward? **No — and it is now decided: outward**

> **★ `BOSS-1` — DECIDED 2026-09-20 (principal direction), recorded in `NP-HEX-ZM-001` §5.3(c):
> the posterior blind-mate boss projects OUTWARD, and only as far as it needs to** — a local emboss
> at the occiput centreline, planform the boss footprint, depth the **minimum** the blind-mate stack
> requires. **The analysis below is what the decision was taken against; it is retained as the
> rationale of record.**
>
> **Two consequences land immediately.** **(1) The Gap's last local constraint is gone.** `FLUSH-1`
> removed *travel*; §8.6.1 removed the fluxgates (they were never in it); **`BOSS-1` removes the
> boss.** What remains setting the assembled Gap is the **closed lever footprint and the labyrinth
> lip** — which is `FLUSH-1`'s full prize unlocked at 0.0385 m²K/W per millimetre, and re-scopes
> `OI-EMCCAV-09` from *"which contributor binds?"* to *"how thin can the lever close?"*
> **(2) `OI-THCOOL-06` becomes needed rather than advisable.** `BOSS-1` commits to the geometry whose
> magnetic cost that measurement bounds, so it is **BLOCKING on MECH-1 cutting the boss**, not a
> recommendation. `OI-EMCCAV-10`'s direction half is **closed by this decision**; its measurement half
> **is** `OI-THCOOL-06`.

**Nothing requires an inward projection.** The boss is a **standalone posterior-centre feature** on
the occiput centreline (§5.3(c)), sited *"where the internal harness gathers near the occiput (Boa
arch / neck attach)"* — **the bulkiest part of the assembly, and the one place a local outward emboss
disappears into hardware that is already there.**

| | Boss projects **inward** | Boss embosses **outward** |
|---|---|---|
| Vault Gap over the lattice | **set by the boss** — the deepest local feature dictates the global allocation | **decoupled entirely** — Gap falls to the closed lever + labyrinth lip, which is FLUSH-1's whole prize |
| Exterior profile | unchanged | a local bump at the occiput, inside the Boa arch / neck attach volume |
| Exterior area | unchanged | marginally **increased** — mildly helpful, given `NP-THERM-SINK-001` §6 finds the exterior already sits 6.1 K above ambient with every tile idle |
| Harness routing | the connector competes with the harness in the same gap | the connector sits in its own recess |

**The cost, and it is real.** The outer bowl carries the mu-metal, which §5.3(d) requires **unbroken**
and which `hardware-detail.md` §4.3's **D3** makes load-bearing. **Mu-metal is annealed for
permeability and loses it when work-hardened** — drawing it over a local dome is exactly that
operation, and it cannot be re-annealed after lamination to PETG and CFRP.

**But that cost is largely already sunk, and the geometry argument survives it.** The boss is
**already a penetration** — `NP-DRV-SHELL-002` §4.3 calls it *"one aperture"* and routes the whole
module interconnect through it precisely to avoid opening a second. Magnetic continuity is therefore
**already interrupted at that exact spot**, and the accepted treatment is a **mu-metal chimney
collar**. *A collar is itself a projection.* So the question was never *whether* something projects
at the boss — only **which way**:

> **Inward, the collar consumes Gap everywhere its shadow falls and competes with the harness.
> Outward, it consumes exterior profile at the one location already thick with hardware. Outward is
> strictly better, and the marginal magnetic cost is paid where continuity is already broken.**

**And outward is not a special case on this bowl.** `NP-HELMET-GEOM-001` §2 already records that
**"coil formers add local thickness only"** — local outward thickness variation is an **existing
feature class** here, not a new one. `BOSS-1` adds one more instance of it.

**Two keep-outs bind the emboss, and neither is a blocker:**

1. **It must stay inside the existing Boa-arch / neck-attach exterior volume.** Outward is free only
   while it hides in hardware already there; break that envelope and it becomes a new exterior
   feature with industrial-design and `NP-HW-FITOVER-001` consequences.
2. **It must not intersect a Helmholtz coil former.** `NP-HELMET-GEOM-001` §174 makes former geometry
   **fixed *because calibration depends on it*** — and that calibration is the coil-drive → field
   transfer function that `hardware-detail.md` §4.3's **D1** identifies as Layer 2's *decisive*
   dependency, re-run per configuration by `REQ-EMI-11`. A former is a **keep-out to route around**,
   not an obstacle; intersecting one means re-fixing and re-calibrating it deliberately.

**It is not zero, though, and there is a measurement for it that already exists.** `OI-THCOOL-06`'s
original text was *"bench-measure ELF magnetic leakage through a **mu-metal chimney collar at the
posterior boss**"* — retained struck-through under `NP-CONV-001` §4's append-only rule. It was closed
on 2026-08-30 **only because the pneumatic loop that needed it went out of scope**, with the note
*"reopen only if the loop is revived."*

> **That reopen trigger is too narrow.** The measurement is about **a formed mu-metal collar at the
> posterior boss**, and it applies to whatever passes through it — pneumatic tubes or a 216-pin
> blind-mate harness. **An outward-embossed boss needs exactly that bench number.** Recommend
> reopening `OI-THCOOL-06` against the collar rather than against the loop. **`OI-EMCCAV-10`**, and
> it is **time-boxed by `MECH-1`** for the same reason §4.3 gives.

---

### 8.7 Dimensioning the Gap — and the closed lever is **not** what sets it

Asked to dimension the Gap to the closed-lever footprint. **The lever is not the binding
contributor, and the thing that is has never been on the list.**

Under `FLUSH-1` the lever is flush with L1's outer face, inside the **3.0–4.0 mm** of L1 outer-face
features `NP-HELMET-GEOM-001` §2 already allocates. **Flush means it contributes zero Gap.** So
dimensioning "to the closed-lever footprint" would give a Gap of ~0 — and that is wrong, because
something else is in there.

> **`NP-DRV-SHELL-002` §4.1 assigns L1's *gap-facing* face to** *"**Cluster controller components,
> incl. the STM32G071**; clamp plates."* **The cluster controller boards sit in the Gap.** They are
> not on §8.5's contributor table, not on §8.6's, and not on `NP-HELMET-GEOM-001` §2's Gap row. This
> is the **third** correction to that list in three revisions, and the first one that *adds* rather
> than removes.

#### 8.7.1 The floor, from the package types the record actually names

`NP-DRV-SHELL-002` §10.1 and `NP-HW-HUB-001` §8.1 name the parts by package, so their heights are
JEDEC maxima rather than guesses:

| Part | Package | Max height |
|---|---|---:|
| STM32G071 cluster MCU | UFQFPN32 | 0.60 mm |
| PCA9548A I2C mux | TSSOP-24 | 1.20 mm |
| 16:1 PD current mux (TMUX1308-class) | TSSOP | 1.20 mm |
| Zero-drift TIA op-amp (MCP6V51 / OPA378-class) | SOT-23-5 | **1.45 mm ← tallest** |
| ~400 pull-ups, straps, decoupling, ESD | 0402 / 0603 | 0.55 mm |

**Closed-state stack. Two of the three terms are now decisions rather than assumptions** —
`PCB-1` and `PLATE-1`, both principal direction 2026-09-21:

| Term | Value | Status |
|---|---|---|
| Cluster PCB, 4-layer rigid-flex | **0.80 mm ±0.08** | **★ `PCB-1` — DECIDED** (`NP-DRV-SHELL-002` §3.2) |
| + tallest component | 1.45 mm | sourced (package maximum) |
| **= component plane** | **2.25 mm** | |
| + assembly / tolerance clearance | 0.5–1.0 mm | **STILL ASSUMED — not stated anywhere** |
| **= Gap floor** | **2.75–3.25 mm** | against **5–7 mm** today |

> **★ `PLATE-1` — DECIDED: the clamp plate POCKETS over the controller components**
> (`NP-HEX-ZM-001` §5.4a). Its seated plane is set by its **land areas**, so the stack is
> **max(component plane, plate plane)** and **not their sum**. A 0.8 mm plate sitting *over* the
> parts would give **3.55 mm** instead — **pocketing is worth 0.031 m²K/W**, on top of `PCB-1`'s
> **0.0077**. The plate's own thickness now only governs if it exceeds the 1.45 mm component plane.

> **✅ `BOARD-1` — DECIDED 2026-09-21 (`NP-DRV-SHELL-002` §3.2): the controller board sits at the
> cluster's PAN-FACING EDGE, on the cluster's own mirror axis — not at its centroid.** `PLATE-1`
> removes plate section, and §5.4a caps the cluster at 7 tiles because an 8th raises **plate-mode
> deflection ×3.07** — *"the one that matters"* — against a plate already carrying **34.2–57.0 N**
> across 19 contacts. **At the plate edge the bending moment is minimal, so the pocket costs nearly
> nothing; where the board falls into the inter-tile gap, no pocket is needed at all.** Ribbing
> remains the fallback rather than the expected case.
>
> **Four constraints select that position independently, which is why it is a rule and not a
> preference:** plate structure (above) · **shortest tail** — all 18 converge on the PAN at the
> occiput centreline (§4.2) · **`REQ-EMI-06` loop area**, which §9.3 computes over a 200 mm cluster
> feed and which is **linear in feed length**, so a shorter feed is direct margin on the ≤25 mm²
> limit §9.5 calls mandatory · and **`SYM-1` preserved without an exception clause**, because the
> cluster's own mirror axis keeps midline clusters self-symmetric and lets lateral pairs mirror.
>
> **`OI-EMCCAV-13` is closed on the structural question.** The residual is a **planform packing
> conflict**: `NP-HELMET-GEOM-001` §3's cluster-clamp bosses also want the inter-tile gaps, and for
> a midline cluster the mirror axis meets the PAN-facing edge **on an inter-tile boundary**. That is
> `MECH-2`'s layout question, and it is not solved here.

#### 8.7.2 What that is worth, and what it costs to find out

| Gap | Outward total | Recovered |
|---:|---:|---:|
| 6 mm (today) | 0.410 | — |
| 3.5 mm | 0.314 | +0.096 |
| **3.0 mm** | **0.295** | **+0.115** |
| 2.75 mm | 0.284 | +0.125 |

> **So the answer to "dimension the Gap" is ~3 mm, not ~0 — and the number is set by a 1.45 mm
> op-amp package, a PCB thickness nobody has written down, and a clearance nobody has derived.**

**Two of the four missing inputs are now closed; two remain:**

| # | Input | Status |
|---|---|---|
| 1 | Cluster PCB thickness | **✅ `PCB-1` — 0.80 mm ±0.08** |
| 2 | Whether the plate pockets or sits above — *the largest swing* | **✅ `PLATE-1` — it pockets** |
| 3 | **Clamp plate structural thickness** | **OPEN** — but it now only matters if it exceeds 1.45 mm |
| 4 | **Outer bowl inner-surface PROFILE tolerance** | **OPEN, and it is nowhere in the record** — §2's stack gives thicknesses (CFRP 2.5 ±0.3) but no **form** tolerance over a ~190 mm dome, and that is what sets the clearance term |

**`OI-EMCCAV-09` carries the two that remain**, both `MECH-2`'s. **Item 4 is the one that actually
binds now** — the clearance term is the whole of the remaining 0.5 mm uncertainty, worth
**0.019 m²K/W** across its range.

#### 8.7.3 A consequence for the gap pad that partly restores §221's cap

`completed-decisions.md` §221 capped gap-pad coverage at **22.7 %** on three keep-outs: cluster-clamp
actuators, fluxgates, and bowl separation for service. **Two of those three are now wrong** — the
actuators are flush (`FLUSH-1`) and the fluxgates were never in the Gap (§8.6.1). But **§8.7's
finding supplies a keep-out §221 never named, and it is bigger than either: the 18 cluster controller
boards themselves.**

**§221's conclusion therefore survives its reasoning being twice wrong**, which is worth stating
plainly rather than quietly: coverage is still capped, just by a different obstruction. **How much
is now computable and is not computed here** — it needs the cluster board footprint, which §3.2 does
not give. **`OI-EMCCAV-12`.**

---

### 8.8 Resolving board vs. boss — `PACK-1`

§8.7 and `BOARD-1` left one residual: `NP-HELMET-GEOM-001` §3's **cluster-clamp bosses also sit in
the inter-tile gaps**, and for a midline cluster `BOARD-1`'s mirror axis meets the PAN-facing edge on
an inter-tile boundary. **The conflict dissolves once the two are asked what they actually want,
because they want different *radii* on the same axis.**

| | Wants | Why |
|---|---|---|
| **Cluster-clamp boss** | an **internal** vertex | §3: the bosses *"add **mid-span** stiffness to the scalp-facing plane"* — internal is the point |
| **Controller board** | the **perimeter** | `PLATE-1`/§5.4a: minimum bending moment, and outside the plate footprint it needs no pocket at all |

> **★ `PACK-1` — the boss takes the PAN-facing INTERNAL vertex; the board takes the outer end of the
> SAME seam, on the cluster perimeter. Both sit on the cluster's mirror axis, one hex edge apart.**

**The geometry is exact, not approximate.** For a flower centred at C with two petals P_a, P_b
straddling the PAN direction, the vertex shared by C, P_a and P_b lies **on the axis at one
circumradius**, and the P_a/P_b seam runs outward from it by **one edge length** to the cluster
perimeter. On a regular hexagon circumradius = edge length = `W/√3`, so:

| Point on the mirror axis | Distance from cluster centre |
|---|---:|
| **Boss** — internal vertex (C, P_a, P_b) | **a = 23.09 mm** |
| **Board** — outer end of the P_a/P_b seam | **2a = 46.19 mm** |
| **Clear separation** | **one hex edge = 23.09 mm** |

`NP-HEX-ZM-001` §5.4a independently calls **23.09 mm** *"one hex edge, W/√3"*, and
`hardware/np_socket_map.json` carries `moduleWidthMm: 40` with `rowPitchMm: 34.64` = 40·cos 30° —
a consistent 40 mm hex lattice.

**Three things fall out, and the third was got WRONG at Rev 11:**

1. **Neither party moves off the mirror axis, so `SYM-1` survives for both** — midline clusters stay
   self-symmetric and lateral pairs mirror by construction. No exception clause.
2. **The seam they share is the board's tail route.** The tail exits along the same seam it sits on,
   toward the PAN — which is what `BOARD-1`'s loop-area and tail-length arguments already wanted.
3. **The 23.09 mm is CONSTANT across all 18 clusters — it is the tile's own edge, and the tile is one
   part.**

> **⚠ Rev 11 said the opposite, and it was wrong. Correction (Rev 12).** Rev 11 published a caveat
> that *"the lattice compresses at crown and rim"*, with one hex edge falling to **13.9 mm** at
> row 11, and told the reader to run the packing check at the tightest cluster. **Withdrawn.**
>
> **The governing constraint is the PART, not a measurement.** The tile is **one universal 40 mm
> mould**, **type-agnostic**, **identical shape** — `NP-ART-001` A1, `NP-HEX-ZM-001` §627 and
> `NP-DT-001` `DI-USE-05` all say so, because **modules are interchangeable**. Every hexagon is
> therefore the same size **by construction**, so adjacent socket centres are **40 mm apart on the
> surface everywhere**. They cannot be closer — the parts would overlap. **`PACK-1`'s 23.09 mm is
> the tile's own edge length, so the boss-to-board clearance is identical at every cluster.**
>
> **What Rev 11 actually measured was a 3-space chord, not an on-surface distance.** The socket map
> gives positions on a **doubly-curved scanned surface**; at row 11 the z-coordinate swings
> **21.6 mm across four sockets** — the rim is turning over — so the chord badly understates the
> spacing. A 25 mm chord for a 40 mm geodesic implies a local radius of **~13 mm**, which is the rim
> fold. Over the mid-vault, where curvature is gentle, the same chords read **38.6–41.0 mm** and the
> error is invisible; that is exactly why it slipped through.
>
> **And the real variation runs the other way.** Congruent flat hexagons **cannot tile positive
> Gaussian curvature without opening gaps** — which is *why* there are inter-tile gaps at all, and
> why `NP-HELMET-GEOM-001` §3 can put the clamp bosses in them *"made free by the lattice gaps."*
> **The gaps widen where curvature is highest, and both the boss and the board live in those gaps —
> so `PACK-1` gets MORE room at crown and rim, not less.** Quantifying that needs the surface model
> (`scripts/extract-helmet-surface.ts`), not the socket list. **`OI-EMCCAV-15`.**


**What is still owed, and it is the same gap `OI-EMCCAV-12` names:** the **cluster board footprint**
is not given in `NP-DRV-SHELL-002` §3.2. **23.09 mm (or 13.9 at the rim) is a centre-to-centre budget,
not a verified fit** — a board carrying an STM32G071, a PCA9548A, two mux banks, a TIA op-amp, a gain
switch and ~400 passives has a real footprint that nobody has stated. **`OI-EMCCAV-14`.**

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
| **`OI-EMCCAV-09`** | **TWO OF FOUR INPUTS CLOSED 2026-09-21 by `PCB-1` and `PLATE-1`.** The Gap floor is set by the **cluster controller boards** on L1's gap-facing face (`NP-DRV-SHELL-002` §4.1), not by the lever — which is flush and contributes zero. With **`PCB-1`** (0.80 mm ±0.08, 4-layer rigid-flex) and the tallest named package (**1.45 mm**, SOT-23-5 op-amp) the component plane is **2.25 mm**; with **`PLATE-1`** the plate **pockets**, so the stack is **max(...)** not **sum(...)**. **Floor: 2.75–3.25 mm against 5–7 mm today.** **Two remain, both `MECH-2`'s:** the plate's structural thickness (now only binding if it exceeds 1.45 mm), and — **the one that actually binds** — the **outer bowl's inner-surface PROFILE tolerance**, which is **nowhere in the record** and is the whole of the remaining 0.5 mm clearance uncertainty, worth **0.019 m²K/W** | ME (**`MECH-2`**) + Thermal | **No — two inputs left** |
| ~~**`OI-EMCCAV-13`**~~ | **✅ CLOSED ON THE STRUCTURAL QUESTION 2026-09-21 by `BOARD-1`** (principal; `NP-DRV-SHELL-002` §3.2) — **the controller board sits at the cluster's PAN-facing edge, on the cluster's own mirror axis, not at its centroid.** At the plate **edge** the bending moment is minimal, so `PLATE-1`'s pocket costs nearly nothing, and **where the board falls into the inter-tile gap no pocket is needed at all**. Ribbing remains the fallback and is no longer the expected case. **Four constraints select the position independently:** plate structure, shortest tail to the PAN, `REQ-EMI-06` loop area (linear in feed length), and `SYM-1` preserved without an exception clause. **Residual, and it is a layout question not an architectural one:** `NP-HELMET-GEOM-001` §3's **cluster-clamp bosses also want the inter-tile gaps**, and for a midline cluster the mirror axis meets the PAN-facing edge **on an inter-tile boundary** — so board and boss compete for that gap | ME (**`MECH-2`**). **STRUCTURAL HALF CLOSED** | ~~Residual: planform packing~~ **✅ FULLY CLOSED 2026-09-21 by `PACK-1` (§8.8)** |
| **`OI-EMCCAV-14`** | **`PACK-1`'s clearance is a centre-to-centre budget, not a verified fit — the cluster board FOOTPRINT is not stated anywhere.** §8.8 puts the boss and the board **one hex edge = 23.09 mm** apart on the cluster mirror axis, and because the tile is **one universal mould** that figure is **identical at all 18 clusters** (Rev 12 withdrew Rev 11's "compresses at the rim" caveat). But a board carrying an STM32G071, a PCA9548A, two mux banks, a TIA op-amp, a gain switch and ~400 passives has a real footprint that `NP-DRV-SHELL-002` §3.2 does not give. Same missing input as `OI-EMCCAV-12` | EE + ME (**`MECH-2`**) | No — but it is the fit check `PACK-1` rests on |
| **`OI-EMCCAV-15`** | **Quantify how the inter-tile gaps widen with curvature — it is free clearance nobody has counted.** Congruent flat hexagons cannot tile positive Gaussian curvature without opening gaps, which is why the gaps exist and why `NP-HELMET-GEOM-001` §3 sites the clamp bosses in them *"made free by the lattice gaps"*. **The gaps open where curvature is highest, and both the boss (`PACK-1`) and the board (`BOARD-1`) live in them**, so crown and rim have **more** room than the mid-vault, not less. Needs the surface model (`scripts/extract-helmet-surface.ts`), not the socket list — **3-space chords between socket centres are not on-surface distances**, the error Rev 11 made and Rev 12 withdrew | ME + Systems | No — upside, not risk |
| **`OI-EMCCAV-12`** | **Gap-pad coverage is still capped — by an obstruction `completed-decisions.md` §221 never named.** §221 capped coverage at **22.7 %** on three keep-outs: cluster-clamp actuators, fluxgates, and bowl separation. **Two are now wrong** — the actuators are flush (`FLUSH-1`) and the fluxgates were never in the Gap (§8.6.1) — but §8.7 supplies a bigger one §221 missed: **the 18 cluster controller boards**. **§221's conclusion survives its reasoning being twice wrong**, which is worth saying plainly. Recomputing the cap needs the **cluster board footprint**, which `NP-DRV-SHELL-002` §3.2 does not give | ME + Thermal | No |
| ~~**`OI-EMCCAV-10`**~~ | **✅ DIRECTION DECIDED 2026-09-20 by `BOSS-1`** (principal; `NP-HEX-ZM-001` §5.3(c)) — **the boss projects OUTWARD, as a local emboss at the occiput centreline, only as deep as the blind-mate stack requires.** §8.6.3 is retained as the rationale of record. **The Gap's last local constraint is gone** (`FLUSH-1` removed travel, §8.6.1 removed the fluxgates, `BOSS-1` removes the boss), leaving the closed lever footprint and the labyrinth lip — which re-scopes `OI-EMCCAV-09`. **The measurement half is not closed and is now `OI-THCOOL-06`**, reopened 2026-09-20 against the collar and **BLOCKING on MECH-1 cutting the boss** — `BOSS-1` is what turns it from advisable into needed. Two keep-outs ride with the decision: the emboss stays inside the existing Boa-arch / neck-attach volume, and it does not intersect a Helmholtz coil former (`NP-HELMET-GEOM-001` §174 — fixed geometry, calibration depends on it, and that calibration is **D1**) | ME (**`MECH-1`**) + EMC. **DIRECTION CLOSED** | Residual → `OI-THCOOL-06` |
| **`OI-EMCCAV-11`** | **The record contradicts itself on where the fluxgates are, and only one reading constrains the Gap.** `completed-decisions.md` §221 says *"§5.3c's fluxgates sit there"* — in the inter-bowl gap — while **`NP-HELMET-GEOM-001` §2 puts them in the "Inner-bowl socket wall + FPC channel" station (2.0–2.5 mm)** and **`NP-HEX-ZM-001` §5.3(c) puts them "on the inner bowl, **near the scalp**"**, which is the far side of L1 from the Gap. Likely reconciliation: the magnetic **keep-out** reaches into the gap while the sensor **body** does not — which preserves §221's no-continuous-pad conclusion but not its stated reason. **Material to `OI-EMCCAV-09`:** a keep-out is a **planform** exclusion, not a **radial** one, and only a radial constraint sets the Gap | EE + ME | No — but it changes the Gap's contributor list |
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
| 5 | 2026-09-20 | NeurOne EMC / Systems Engineering | **§8.5 added: `FLUSH-1` (principal direction) removes *travel* from the assembled-state inter-bowl Gap, and the lever it re-opens is larger than the one this whole document was written about.** `FLUSH-1` — *the cluster lever is flush when closed and throws only with the bowls separated* — has a second half that was **already a fact in the document set, with its consequence never drawn**: `NP-HEX-ZM-001` §5.2 reaches the levers *"by unclamping the bowls"* and §5.5 confirms *"during a module swap the bowls are open"*, so **the lever's swept volume is a bowls-OPEN volume**. `NP-HELMET-GEOM-001` §2's Gap line item — *"inter-bowl clamp **travel** + blind-mate boss + labyrinth lip"*, 5–7 mm — is therefore **mis-named**: travel is not an assembled-state requirement. §8.3 had already found that 5–7 mm was *"an allocation, not a derived requirement"*; **`FLUSH-1` says what it was mis-allocated for.** What remains is the **closed** lever footprint, the labyrinth lip, the fluxgates and the blind-mate boss — and **the boss is a standalone posterior-centre feature** (§5.3c), so **a locally-relieved gap is available** (deep at the occiput, shallow over the lattice), which "travel" had foreclosed by implying uniform swept clearance. **The prize is the largest in the outward path:** the gap is stagnant air at **0.231 m²K/W — 56 % of the total and its single biggest term**, so at k = 0.026 **each millimetre is worth 0.0385 m²K/W**. **Two millimetres of gap beats deleting the entire 3 mm Layer 4 absorber (0.077 vs 0.075); three reaches 0.115.** Against `NP-THERM-COOL-001` §6.1's sealed recirculation (0.231 → 0.067, recovery 0.164, needing a **motor inside the sealed cavity**), narrowing to 3 mm reaches **~70 % of the prize with no motor, no power draw, no moving part and no BOM line** — though the two **interact rather than compose** (less volume at higher flow resistance to stir) and must be traded, not stacked. **Stated here only because `FLUSH-1` surfaced it; dimensioning the gap is `MECH-2`'s and the thermal case is `NP-THERM-COOL-001`'s.** `OI-EMCCAV-09` **re-scoped** to carry the hand-off. **The EMC position is unchanged and narrow:** the gap is inside the Faraday envelope either way, narrowing it moves no aperture, and §4.1's modes were already computed across a head-size span far larger than any contemplated gap change. `scripts/check-cavity-q.ts` gains §8's gap model and six anchors, 25 → 31. **No locked section modified; no layer removed; no measurement asserted.** |
| 6 | 2026-09-20 | NeurOne EMC / Systems Engineering | **§8.6 answers the two questions Rev 5 left open, corrects Rev 5's own Gap table, and fixes a status error this document had been propagating.** **(1) The fluxgates need ZERO gap.** Rev 5's §8.5 listed them as a Gap contributor; they are not one. `NP-HELMET-GEOM-001` §2 places them in the **"Inner-bowl socket wall + FPC channel"** station (2.0–2.5 mm), `NP-HEX-ZM-001` §5.3(c) puts them *"on the inner bowl, **near the scalp**"* — the far side of L1 from the Gap — and §5.1 lists them among the inner bowl's contents. Table corrected. **This exposes a contradiction that is not this document's:** `completed-decisions.md` §221 says the fluxgates *"sit there"*, in the gap. Likely reconciliation is that the magnetic **keep-out** reaches into the gap while the sensor **body** does not — which preserves §221's no-continuous-pad conclusion but not its reason, **and matters to `OI-EMCCAV-09` because a keep-out is a planform exclusion, not a radial one, and only a radial constraint sets the Gap.** **`OI-EMCCAV-11`.** **(2) The boss is the only real Gap constraint, it is undimensioned, and the dimension is already owed.** What crosses it: **216 interface pins in 20 tail groups** plus the fluxgate/coil harness, in **four segregated contact groups with independent returns**, **blind-mated as the bowls draw closed** — so it needs real mating depth (wipe + lead-in + the ±0.4 lateral / ±0.5 Z blind-mate tolerance). No figure exists anywhere. `NP-DRV-SHELL-002` §4.3 already says the layout *"must be settled before MECH-1 cuts the posterior boss"*; **FLUSH-1 only makes it urgent, because it is now the one thing left setting the Gap.** **(3) The boss should project OUTWARD, not inward.** Nothing requires inward. Outward **decouples the vault Gap from the boss entirely** — FLUSH-1's whole prize — for a local emboss at the occiput centreline, *"where the internal harness gathers (Boa arch / neck attach)"*, the bulkiest part of the assembly; it also marginally **increases** exterior area, mildly helpful against `NP-THERM-SINK-001` §6's 6.1 K idle rise. **The magnetic cost is real but largely sunk:** the outer bowl carries the mu-metal that §5.3(d) requires unbroken and `hardware-detail.md` §4.3's **D3** makes load-bearing, and mu-metal loses permeability when work-hardened and cannot be re-annealed after lamination — **but the boss is already "one aperture"** (`NP-DRV-SHELL-002` §4.3), continuity is already interrupted there, and the accepted treatment is a **mu-metal chimney collar**, which is itself a projection. **The question was never whether something projects at the boss, only which way** — inward it consumes Gap everywhere its shadow falls and competes with the harness; outward it consumes profile where there is already hardware. **`OI-EMCCAV-10`**, time-boxed by `MECH-1`. **(4) A status error this document propagated is corrected.** Rev 1–Rev 5 repeatedly stated *"`OI-THCOOL-06` unchanged and open"*, inherited from `NP-BIB-EMF-001` §1/§7.6. **Its owning document closed it on 2026-08-30** (`NP-THERM-COOL-001`, by D-2 — it was BLOCKING only on the pneumatic loop, which §6.9 put out of scope). The owning document wins. **But its closure note — *"reopen only if the loop is revived"* — is too narrow a trigger**, because the measurement is about a **formed mu-metal collar at the posterior boss** and applies to whatever passes through it. An outward-embossed boss needs exactly that bench number, so `OI-EMCCAV-10` recommends reopening it **against the collar rather than against the loop**. **No locked section modified; no layer removed; no measurement asserted.** |
| 7 | 2026-09-20 | NeurOne EMC / Systems Engineering | **`BOSS-1` decided (principal direction): the posterior blind-mate boss projects OUTWARD, as a local emboss at the occiput centreline, only as deep as the blind-mate stack requires.** Recorded in its owning section, `NP-HEX-ZM-001` §5.3(c); §8.6.3 here is retained as the rationale of record. **The decision completes a three-step removal of every local constraint on the inter-bowl Gap:** `FLUSH-1` (Rev 5) took out *travel* — the lever throws only with the bowls separated; §8.6.1 (Rev 6) took out the **fluxgates**, which were never in the Gap at all; **`BOSS-1` takes out the boss.** What remains setting the assembled Gap is the **closed lever footprint and the labyrinth lip**, so `OI-EMCCAV-09` is re-scoped from *"which contributor binds?"* to *"how thin can the lever close?"* At **0.0385 m²K/W per millimetre** on the largest single outward thermal term, that is `FLUSH-1`'s full prize unlocked. **Outward is not a special case on this bowl:** `NP-HELMET-GEOM-001` §2 already records that *"coil formers add local thickness only"*, so local outward thickness variation is an **existing feature class** and `BOSS-1` adds one more instance. **Two keep-outs bind it, neither a blocker:** the emboss must stay inside the existing **Boa-arch / neck-attach exterior volume** (outward is free only while it hides in hardware already there), and it must **not intersect a Helmholtz coil former** — §174 of that document makes former geometry *fixed because calibration depends on it*, and that calibration is the coil-drive → field transfer function which `hardware-detail.md` §4.3's **D1** identifies as Layer 2's **decisive** dependency, re-run per configuration by `REQ-EMI-11`. A former is a keep-out to route around, not an obstacle. **`OI-EMCCAV-10`'s direction half is closed by this decision; its measurement half is `OI-THCOOL-06`**, reopened 2026-09-20 against the collar (`NP-THERM-COOL-001` Rev 12) and **BLOCKING on MECH-1 cutting the boss** — `BOSS-1` is precisely what turns that measurement from advisable into needed, because it commits to the geometry whose magnetic cost the measurement bounds. **`NP-HELMET-GEOM-001` §2's TOTAL row now carries two pending local exceptions that do not conflict:** the Layer 4 deletion re-lofts the bowl **3 mm inward globally**, `BOSS-1` embosses it **outward locally at the occiput**. **No locked section modified; no layer removed; no measurement asserted.** |
| 8 | 2026-09-20 | NeurOne EMC / Systems Engineering | **§8.7 dimensions the inter-bowl Gap, and finds that the closed lever is NOT what sets it.** Under `FLUSH-1` the lever is flush with L1's outer face, inside the 3.0–4.0 mm of L1 features `NP-HELMET-GEOM-001` §2 already allocates — **flush means zero Gap contribution**, so dimensioning *"to the closed-lever footprint"* would give ~0, which is wrong because something else is in there. **`NP-DRV-SHELL-002` §4.1 assigns L1's *gap-facing* face to "cluster controller components, incl. the STM32G071; clamp plates" — the 18 controller boards sit in the Gap, and they appear on no contributor list anywhere**, not §8.5's, not §8.6's, not §2's Gap row. **This is the third correction to that list in three revisions and the first that ADDS rather than removes.** **The floor, from package types the record names** (so JEDEC maxima, not guesses): UFQFPN32 0.60 · TSSOP-24 1.20 · TMUX1308-class TSSOP 1.20 · **zero-drift op-amp SOT-23-5 1.45 (tallest)** · 0402/0603 passives 0.55. With the clamp plate pocketing over the parts: PCB 0.8–1.0 (**assumed — not stated**) + 1.45 + clearance 0.5–1.0 (**assumed — not stated**) = **2.75–3.45 mm**, against **5–7 mm** today, worth **0.096–0.125 m²K/W**. If the plate sits *over* the parts instead, add its thickness — also unstated. **So the answer is ~3 mm, not ~0, and it is set by a 1.45 mm op-amp package, a PCB thickness nobody has written down, and a clearance nobody has derived.** **`OI-EMCCAV-09` re-scoped a third time** to carry the four missing inputs: cluster PCB thickness, clamp plate thickness, **whether the plate pockets or sits above** (the largest swing), and the **outer bowl's inner-surface PROFILE tolerance** — §2's stack gives thicknesses (CFRP 2.5 ±0.3) but **no form tolerance over a ~190 mm dome**, and that is what sets the clearance term. **New `OI-EMCCAV-12`:** `completed-decisions.md` §221 capped gap-pad coverage at **22.7 %** on three keep-outs — clamp actuators, fluxgates, bowl separation. **Two are now wrong** (the actuators are flush, the fluxgates were never there) **but §8.7 supplies a bigger one §221 never named: the 18 controller boards.** §221's *conclusion* survives its *reasoning* being twice wrong — stated plainly rather than quietly — and recomputing the cap needs a cluster board footprint §3.2 does not give. `scripts/check-cavity-q.ts` gains the floor model and five anchors, 31 → 36. **No locked section modified; no layer removed; no measurement asserted.** |
| 9 | 2026-09-21 | NeurOne EMC / Systems Engineering | **`PCB-1` and `PLATE-1` decided (principal direction), closing two of §8.7's four missing inputs and firming the Gap floor.** **`PCB-1`** (`NP-DRV-SHELL-002` §3.2): the cluster controller board is **0.80 mm nominal, ±0.08 (±10 %), 4-layer rigid-flex**. Rev 2 of that document costed it (*"small 4-layer + rigid-flex tail, $1.80"*) and gave it **no thickness**, which turned out to matter outside it entirely. **0.80 rather than the default 1.6** because the usual reason for 1.6 is self-support and this board does not need it — §8.1 makes the carrier *"fixed — laminated into L1, never moves"*, so the bowl provides stiffness — and 0.80 is a **stock rigid-section thickness** for 4-layer rigid-flex, carrying no fabrication premium. **Not 0.60**, which constrains dielectric and copper-weight options on a 4-layer stack-up carrying a 24 V rail at up to the whole vault feed; the residual 0.20 mm is worth ~0.008 m²K/W and is not worth buying without a fab DFM review. **`PLATE-1`** (`NP-HEX-ZM-001` §5.4a): the clamp plate **pockets** over the controller components, so its seated plane is set by its **land areas** and the closed-state stack is **max(component plane, plate plane)**, not their **sum**. **Result: component plane 0.80 + 1.45 = 2.25 mm, Gap floor 2.75–3.25 mm** against 5–7 today. A 0.8 mm plate sitting *over* the parts would give 3.55 mm, so **pocketing is worth 0.031 m²K/W** on top of `PCB-1`'s **0.0077**; the plate's own thickness now only governs if it exceeds 1.45 mm. **One cost recorded rather than glossed:** `PLATE-1` removes plate section, and §5.4a caps the cluster at 7 tiles precisely because an 8th raises **plate-mode deflection ×3.07** — *"the one that matters"* — against a plate already carrying **34.2–57.0 N** across 19 contacts. **The mitigation is free and nobody has used it: the controller board's POSITION within the cluster footprint is unspecified in both owning documents.** Off mid-span the pocket lands at low bending moment; at the centre, at maximum moment under a distributed load. **`OI-EMCCAV-13`**, gating the pocket geometry, with ribbing as the fallback rather than the first move. **Two inputs remain in `OI-EMCCAV-09`**, both `MECH-2`'s: the plate's structural thickness, and — **the one that actually binds** — the **outer bowl's inner-surface PROFILE tolerance**, which is **nowhere in the record** (§2 gives thicknesses, CFRP 2.5 ±0.3, but no **form** tolerance over a ~190 mm dome) and is the whole of the remaining 0.5 mm clearance uncertainty, worth **0.019 m²K/W**. `scripts/check-cavity-q.ts` gains three anchors, 36 → 39. **No locked section modified; no layer removed; no measurement asserted.** |
| 10 | 2026-09-21 | NeurOne EMC / Systems Engineering | **`BOARD-1` decided (principal direction), closing the structural half of `OI-EMCCAV-13`: the cluster controller board sits at the cluster's PAN-FACING EDGE, on the cluster's own mirror axis — not at its centroid.** Rev 9 recorded that `PLATE-1` removes plate section exactly where `NP-HEX-ZM-001` §5.4a says not to — that section caps the cluster at 7 tiles because an 8th raises **plate-mode deflection ×3.07**, *"the one that matters"*, against a plate already carrying **34.2–57.0 N** across 19 contacts — and flagged that the board's **position** was unspecified in both owning documents. **It is now specified, and the mitigation is as free as Rev 9 predicted:** at the plate **edge** the bending moment is minimal, so the pocket costs nearly nothing, and **where the board falls into the inter-tile gap no pocket is needed at all**. Ribbing remains the fallback rather than the expected case. **What makes this a rule and not a preference is that four independent constraints select the same position.** **(1) Plate structure**, above. **(2) Tail length** — all **18** tails converge on the PAN at the occiput centreline (`NP-DRV-SHELL-002` §4.2), so PAN-facing siting is the shortest run for every cluster: less rigid-flex, lower cost, fewer bend-radius constraints. **(3) `REQ-EMI-06` loop area** — N1 is a **tree from the PAN**, never a ring (`REQ-EMI-09`), and §9.3 computes loop area **over a 200 mm cluster feed**; loop area is **linear in feed length**, so a shorter feed is direct margin on the **≤25 mm²** limit §9.5 calls mandatory, and margin on the self-field §9.5 must subtract. **(4) `SYM-1` preserved without an exception clause** — siting on the **cluster's own** mirror axis keeps the six self-symmetric midline clusters self-symmetric and lets the six lateral mirror pairs mirror by construction. **`REQ-EMI-01` is unaffected**: the board stays inside its cluster footprint, so no electrode-mux or TIA lane lengthens. **One packing conflict is recorded rather than glossed, and it is `MECH-2`'s:** `NP-HELMET-GEOM-001` §3's **cluster-clamp bosses also sit in the inter-tile gaps**, where they *"add mid-span stiffness to the scalp-facing plane without costing any module coverage"* — and for a **midline** cluster the mirror axis meets the PAN-facing edge **on an inter-tile boundary**, exactly where a boss wants to be. **The board and the boss compete for that gap; the planform packing is a layout question and is not solved here.** `OI-EMCCAV-13` closed on structure, residual on packing. **No locked section modified; no layer removed; no measurement asserted.** |
| 11 | 2026-09-21 | NeurOne EMC / Systems Engineering | **§8.8 adds `PACK-1`, resolving the board-vs-boss packing residual `BOARD-1` left open — and it dissolves rather than trades, because the two want different RADII on the same axis.** The cluster-clamp boss wants an **internal** vertex, because `NP-HELMET-GEOM-001` §3 has the bosses *"add **mid-span** stiffness to the scalp-facing plane"*; the controller board wants the **perimeter**, because that is where `PLATE-1`'s pocket costs least and where, outside the plate footprint, it costs nothing. **`PACK-1`: the boss takes the PAN-facing internal vertex, the board takes the outer end of the same seam, and they sit one hex edge apart on the cluster's mirror axis.** **The geometry is exact:** for a flower centred at C with petals P_a, P_b straddling the PAN direction, the vertex shared by C, P_a and P_b lies on the axis at **one circumradius**, and the P_a/P_b seam runs outward by **one edge length** to the perimeter — and on a regular hexagon circumradius = edge = `W/√3`. So boss at **a = 23.09 mm**, board at **2a = 46.19 mm**, clear separation **23.09 mm**. `NP-HEX-ZM-001` §5.4a independently calls 23.09 mm *"one hex edge, W/√3"*, and `hardware/np_socket_map.json` carries `moduleWidthMm: 40` with `rowPitchMm: 34.64` = 40·cos30°, a consistent 40 mm lattice. **Three consequences.** **(1)** Neither party leaves the mirror axis, so **`SYM-1` survives for both** with no exception clause. **(2)** The shared seam **is** the board's tail route to the PAN, which is what `BOARD-1`'s loop-area and tail-length arguments already wanted. **(3) — and this is the one that matters — 23.09 mm is NOMINAL, and the scan-grounded lattice does not hold nominal everywhere.** In-row spacing runs **38.6–41.0 mm over the mid-vault (rows 5–8)** but falls to **27.5–28.3 mm at rows 1, 3 and 10** and **24.0 mm at row 11, the rim** — so one hex edge falls from 23.09 to **~13.9 mm** at the tightest cluster. **`PACK-1` is comfortable over the mid-vault and tight at crown and rim, and the packing check must be run at the tightest cluster rather than at nominal.** Where the intra-cluster run is too short the board **continues outward into the inter-cluster seam**, which is free because the neighbouring cluster's boss is at *its* internal vertex, not its perimeter — keeping the board on-axis and `SYM-1` intact, rather than moving the boss off-axis, which is the move that would cost the symmetry. **Still owed, and it is `OI-EMCCAV-12`'s gap too: the cluster board FOOTPRINT is not given in `NP-DRV-SHELL-002` §3.2**, so 23.09 mm (13.9 at the rim) is a **centre-to-centre budget, not a verified fit**. **`OI-EMCCAV-14`.** `scripts/check-cavity-q.ts` gains four anchors, 39 → 43. **No locked section modified; no layer removed; no measurement asserted.** |
| 12 | 2026-09-21 | NeurOne EMC / Systems Engineering | **Rev 11's §8.8 caveat is WITHDRAWN: the lattice does not compress, and the governing constraint was never a measurement.** Rev 11 published *"the lattice compresses at crown and rim"*, with one hex edge falling to **13.9 mm** at row 11, and directed the packing check to the tightest cluster. **That is wrong.** **The tile is one universal 40 mm mould, type-agnostic, identical shape** — `NP-ART-001` A1, `NP-HEX-ZM-001` §627 and `NP-DT-001` `DI-USE-05` all state it, **because the modules are interchangeable**. Every hexagon is therefore the same size **by construction**, adjacent socket centres are **40 mm apart on the surface everywhere** (they cannot be closer — the parts would overlap), and **`PACK-1`'s 23.09 mm is the tile's own edge length, so the boss-to-board clearance is IDENTICAL at all 18 clusters.** There is no per-cluster sweep to run, which is the point of interchangeability. **What Rev 11 measured was a 3-space CHORD, not an on-surface distance.** The socket map gives positions on a **doubly-curved scanned surface**; at row 11 the z-coordinate swings **21.6 mm across four sockets** as the rim turns over, so the chord badly understates spacing — a 25 mm chord for a 40 mm geodesic implies a local radius of **~13 mm**, the rim fold. Over the mid-vault the same chords read **38.6–41.0 mm** and the error is invisible, which is exactly why it passed. **This is the SECOND time this lattice was mis-measured the same way**: Rev 11's own log entry already recorded a naive nearest-neighbour pass that read a 19–39 mm spread as a pitch discrepancy. **Same root cause both times — treating 3-space distances between socket centres as on-surface distances** — so a guard is now written into `scripts/check-cavity-q.ts` at the point of use rather than left as a lesson. **And the real variation runs the other way.** Congruent flat hexagons **cannot tile positive Gaussian curvature without opening gaps**, which is *why* inter-tile gaps exist at all and why `NP-HELMET-GEOM-001` §3 can site the clamp bosses in them *"made free by the lattice gaps"*. **The gaps widen where curvature is highest, and both the boss and the board live in them — so `PACK-1` gets MORE clearance at crown and rim, not less.** Quantifying that needs the surface model (`scripts/extract-helmet-surface.ts`), not the socket list: **`OI-EMCCAV-15`**. `PACK-1`'s conclusion is unchanged and strengthened — boss at the internal vertex, board at the perimeter, one hex edge apart, on every cluster. **No locked section modified; no layer removed; no measurement asserted.** |
