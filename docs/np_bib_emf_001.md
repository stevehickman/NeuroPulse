# EMF Shielding — Evidence Base and Claim Substantiation Record

**Project:** NeurOne
**Document:** NP-BIB-EMF-001
**Revision:** 5
**Date:** 2026-09-20
**Status:** ACTIVE — literature review and claim-substantiation record; asserts no measurement
**Effective Date:** 2026-09-15
**Author:** NeurOne Systems Engineering
**Approved By:** Pending — principal review required (§7 recommendations)
**References:** CLAUDE.md §1, §4.3, §4.2; `docs/reference/hardware-detail.md` §4.3; `NP-HEX-ZM-001` §5.2, §5.3, §5.3.1; `NP-DRV-SHELL-002` §9.1–§9.6; `NP-HELMET-GEOM-001` §2, §3.2; `NP-ENV-OPRANGE-001` §2; `NP-THERM-COOL-001` §6.2; `NP-HW-EEGNET-001` §5.4; `NP-PWR-THERM-001` §11–§12; `NP-THERM-SINK-001` `RISK-SINK-03`, `OI-SINK-07`; `NP-THERM-COOL-001` §2, §6.3; `NP-DT-001` `DI-PERF-22`, `DI-REG-05`; `docs/reference/competitive-position.md`; `docs/reference/regulatory-strategy.md`; **`NP-EMC-CAV-001`**; WHO EHC 238; ICNIRP 2010; IEC 60601-1-2; IEC 61000-4-8; FTC Health Products Compliance Guidance
**Related Issues:** OI-BIBEMF-01…10; EMF-1; EMF-3; RISK-20; OI-THCOOL-04; OI-THCOOL-06; OI-THCOOL-15; OI-PWRTH-05; OI-SINK-01; OI-SINK-07
**Gate:** —
**IEC 62304 Class:** — (evidence record, not device software)

---

## 1. Why this document exists

CLAUDE.md §1 lists *"5-layer EMF shielding + active Helmholtz cancellation (only consumer brain
wearable with measured shielding)"* as a founding design principle, and `NP-PWR-BUDGET-001` §484 calls
the shielding **"the product's primary technical claim."** Until this document, that claim had **no
evidence record anywhere in the document set**:

- `docs/reference/clinical.md` — zero mentions of EMF, shielding or electromagnetic. The evidence
  bibliography carries nothing.
- `docs/reference/regulatory-strategy.md` — zero mentions. No FTC substantiation plan, and
  **IEC 60601-1-2 is absent from its required-standards list** (§8), though `NP-REG-CVNS-001` §192
  names it for the cervical VNS accessory.
- `docs/reference/marketing-notes.md` — zero mentions, while `competitive-position.md` §176 carries a
  published-copy line asserting the claim.
- `docs/np_cost_001.md` — **no BOM line for the stack at all**: no mu-metal, palladium, absorber,
  fluxgate or Helmholtz entry.

A claim that is the product's primary differentiator, cited in shipping marketing copy, with no
bibliography, no substantiation plan and no cost line, is a gap of the same shape as `OI-DOC-01`.
This document closes the bibliography half. It does **not** close `EMF-1` — no measurement exists and
none is asserted here.

> **Scope boundary, stated once.** Everything below concerns whether shielding changes *outcomes* or
> *signal quality*. Nothing here bears on whether the stack meets its dB targets — that is `EMF-1`,
> `EMF-3`, `RISK-20` and `OI-THCOOL-06`, all open, all bench items, none of which literature can
> discharge.
>
> **⚠ STATUS CORRECTION (Rev 5, 2026-09-20): `OI-THCOOL-06` was already CLOSED when Rev 1 wrote this.**
> Its owning document closed it **2026-08-30** by D-2 — it was BLOCKING only on the pneumatic loop's
> penetration of the posterior boss, and `NP-THERM-COOL-001` §6.9 put that loop out of scope. **The
> owning document wins**, and this document propagated the error into `NP-EMC-CAV-001` and four of
> its revisions. `EMF-1`, `EMF-3` and `RISK-20` are unaffected and remain open.
>
> **♻ AND IT WAS REOPENED 2026-09-20 (principal direction), against the collar rather than the
> loop** — `NP-THERM-COOL-001` Rev 12. The closure note read *"reopen only if the loop is revived"*,
> which scoped the item to a **consumer** rather than to the thing measured. The measurement is
> *"bench-measure ELF magnetic leakage through a **mu-metal chimney collar at the posterior boss**"* —
> which applies to whatever passes through that collar, tubes or a 216-pin blind-mate harness. An
> outward-embossed boss needs exactly that number — and **`BOSS-1`** (principal, 2026-09-20) has since
> **decided on the outward emboss**, so the item is **BLOCKING on MECH-1 cutting the boss**
> (`NP-EMC-CAV-001` §8.6.3).

---

## 2. The three claims, separated

The word "shielding" is doing three jobs in the document set, and they have completely different
evidentiary standing. Conflating them is how a defensible engineering claim becomes an
unsubstantiated health claim.

| # | Claim form | Example wording | Evidence status |
|---|---|---|---|
| **C1** | **Performance** — the enclosure attenuates N dB | *"35–45 dB ELF magnetic, 40–60 dB RF"* | Supportable by bench measurement. **Not yet measured** (`EMF-1`). Literature is irrelevant to it. |
| **C2** | **Signal quality** — shielding improves EEG recording | implied by the DRL bond and the §4.3 architecture | **Well supported for the electric/RF layers.** Not supported for the ELF-magnetic layer. §3. |
| **C3** | **Outcome / protection** — shielding makes therapy work better, or protects the wearer from ambient EMF | not currently written anywhere | **No supporting evidence on any modality. The directly adjacent literature is strongly negative.** §5, §6. |

**C1 and C2 are the claims NeurOne can make. C3 is the one that must never be made.** §6 records why
that is a legal exposure and not only a scientific one.

---

## 3. EEG and conductive shielding — the one place the literature supports us

Shielded recording environments are standard clinical neurophysiology practice, and the documented
aggressor is **mains capacitive coupling**: 50/60 Hz from power lines, fluorescent and LED lighting,
and emissions from nearby electronics. Higher-order harmonics survive notch filtering and can saturate
the ADC.

| Finding | Source |
|---|---|
| Faraday-cage recording room + metal circuit enclosure + common-mode guarding are the efficient methods for reducing EEG electromagnetic interference | Usakli 2010, *Improvement of EEG Signal Acquisition: An Electrical Aspect for State of the Art of Front End*, Comput Intell Neurosci — PMC2817545 |
| Clinical EEG is recorded in shielded rooms; a Faraday cage is the principal fixture, to eliminate interfering **electric** fields | Springer, *Technical Requirements for Electroencephalography in the Operating Area* |
| Faraday cages cannot block static or slowly varying magnetic fields | ibid.; standard shielding theory |
| Consumer/research dry-EEG precedent: Wearable Sensing DSI-24 and DSI-VR300 ship a Faraday cage plus common-mode follower, marketed as "active/passive shielding technology" | *Evaluation of consumer-grade wireless EEG systems for BCI applications*, PMC11502727 |

**Consequence for our architecture.** Layers 1 (CFRP), 3 (Pd-polyester), 5 (port filters) and the
shell→DRL bond are doing work the literature recognises. This is the defensible technical core of the
claim, and it should be the claim.

**Consequence for the competitive table.** `competitive-position.md` §23 lists every competitor at
"None" for EMF shielding. That is accurate for the consumer wellness brands in that row and **not
accurate for the research-grade dry-EEG market**, which ships Faraday shielding today. *"Only consumer
brain wearable with measured shielding"* survives only on a specific reading of "consumer," and the
word "measured" is separately unearned until `EMF-1` runs. **`OI-BIBEMF-01`.**

---

## 4. The ELF-magnetic layer — where the evidence stops

### 4.1 Mu-metal is MEG technology

Multi-layer mu-metal enclosures exist for **magnetoencephalography**, which measures magnetic fields
directly. OPM-MEG requires residual fields **< 1 nT** and gradients **< 1 nT/m**, and operates within a
± 5 nT dynamic range around a zero-field resonance.

| Finding | Source |
|---|---|
| MSRs are built for high-precision magnetic field measurement such as MEG; two mu-metal layers for LF plus an eddy-current layer for HF | *A lightweight magnetically shielded room with active shielding*, PMC9363499 |
| OPM-MEG requires < 1 nT magnitude and < 1 nT/m gradients inside the enclosure | *On-scalp MEG system utilizing an actively shielded array of OPMs*, ScienceDirect S1053811919301958 |
| MSRs require periodic **degaussing** — mu-metal acquires remanence | *Fast Degaussing Procedure for a Magnetically Shielded Room*, PMC11643308 |
| EEG systems do not require external magnetic field suppression | ibid., by contrast throughout the MSR literature |

### 4.2 The ambient ELF term is three to four orders of magnitude below any reported effect

| Quantity | Value | Source |
|---|---|---|
| Residential ELF magnetic field, average | **0.07 µT** (Europe) · **0.11 µT** (North America) | WHO EHC 238; WHO Fact Sheet 322 (2007) |
| Urban outdoor public areas | 0.05–0.2 µT | WHO Fact Sheet 322 |
| ICNIRP general-public reference level | **100 µT** | ICNIRP 2010 |
| IARC 2B classification threshold (childhood leukaemia epidemiology) | time-averaged > 0.3–0.4 µT | WHO/IARC 2002 |
| Exposure at which alpha changes are *reported* | **± 200 µT** pulsed | Cook et al. 2008, *Changes in human EEG alpha activity following exposure to two different pulsed magnetic field sequences*, Bioelectromagnetics — PMID 18663700 |
| Exposure at which alpha modulation was **not** found | **1,800 µT**, 60 Hz | *Human exposure to a 60 Hz, 1800 µT magnetic field: a neuro-behavioral study* |

**The gap is the finding.** The exposure range in which anyone reports a neural effect sits
**2,000–18,000× above typical home ambient**, and the best-powered high-field study returned null.
Layer 2 attenuates a term that is orders of magnitude below where the literature locates any effect at
all — and it cannot reach the *internal* ELF sources, which sit inside the envelope with the sensors
(`NP-DRV-SHELL-002` §9.6).

### 4.3 A calculation the document set does not contain

> **This is an engineering estimate raised by this document, not a literature finding and not a
> measurement.** It is stated with its assumptions so it can be refuted rather than inherited.

Ambient ELF couples into an EEG lead loop by Faraday induction, `V = A · dB/dt`. At the WHO residential
average and 50 Hz:

```
B      = 0.1 µT                          (WHO residential average, §4.2)
dB/dt  = 2π · 50 · 1e-7      = 3.14e-5 T/s
A      = 10 cm² = 1e-3 m²                (deliberately generous — see below)
V      = A · dB/dt           = 3.14e-8 V ≈ 0.031 µV
```

Against `SH2-DRC-16`'s **5 µVpp** artifact budget that is **~150× below budget before any shielding is
applied**, and `REQ-NET-14`/`REQ-NET-15` (tree topology, no closed ring, co-routed single bundle)
drive the real loop area far below the 10 cm² assumed here. `REQ-NET-05` independently makes eddy
currents negligible at ELF by requiring ≥ 1 kΩ end-to-end on every net conductor.

If this holds, **Layer 2's contribution to EEG signal quality rounds to zero**, and §3's support for
conductive shielding does not extend to it. It should be checked by EE before it is relied on.
**`OI-BIBEMF-02`.**

---

## 5. The other seven modalities — nothing, and the literature points at our own internal problem

### 5.1 PBM (transcranial, intranasal, 1064 nm, 1170 nm deep)

No published work was found on ambient EMF affecting photobiomodulation outcomes, and no mechanism is
plausible: tPBM acts through NIR absorption at cytochrome c oxidase, with downstream ATP, ROS and NO
modulation (*Transcranial photobiomodulation: an emerging therapeutic method to enhance brain
bioenergetics*, Nat Neuropsychopharmacology 2024). A 0.1 µT ELF field has no coupling to that pathway.

**What the literature does document is the reverse direction, and it is our problem exactly.** In tPBM
work, *EEG could be recorded only outside stimulation periods because of electrical interference from
laser operation* (*Transcranial 1064-nm laser photobiomodulation modulates frequency-specific cortical
source dynamics*, PMC12823878). That is an emitter inside the measurement volume corrupting the
recording — `NP-DRV-SHELL-002` §9.6's finding, reached independently by the field.

### 5.2 BES/tACS, tDCS, HD-tDCS, VNS, cervical VNS

No literature on ambient EMF altering electrical-stimulation outcomes. The published EMI work is again
about the **device as aggressor**, and its remedies are ours:

- Combined tACS–rTMS interference is mitigated by **twisted lead geometry (20.06% reduction)** and
  **orthogonal coil alignment (up to 91.92% suppression)** — geometry and routing, not enclosure
  shielding (IOP, *Electromagnetic interference suppression in combined tACS-rTMS therapy*, 2025).
- tACS artifacts in EEG/MEG arise from the stimulation current itself and spectrally overlap the
  oscillation under study (*Transcranial alternating current stimulation (tACS)*, PMC3695369).

This is external corroboration for `REQ-EMI-01…07` and for `NP-DRV-SHELL-002` §9.2's conclusion that
the therapeutic band cannot be filtered off the bus.

### 5.3 TMS

Shielding is irrelevant to TMS efficacy — the coil delivers 0.1–0.5 T (`SPEC-TMS-01…05`), seven orders
of magnitude above ambient. Note the **claim** consequence already recorded as `OI-PWRTH-05`: §4.2's
interlock gates Helmholtz cancellation off 55 ms per pulse, so the active half is off for 55% of a
10 Hz train, continuously above 18.2 Hz, and for the whole of every 50 Hz iTBS burst.

### 5.4 Neural audio entrainment · visual stimulation

No literature found relating ambient EMF to outcomes for either. For visual work the documented
environmental confound is **ambient illumination**, not ambient EMF — ten minutes of α-tACS and ambient
illumination independently modulate EEG α-power (PMC5435819), which is a real confound for our
closed-loop EEG-adaptive visual protocols and is unrelated to shielding.

### 5.5 The null result, stated plainly

**No trial on any modality was found that compares shielded against unshielded delivery.** That arm
does not appear to exist in the literature. Any answer to *"how different will results be with and
without it"* is therefore unmeasured, by us and by everyone else. **`OI-BIBEMF-03`.**

---

## 6. The adjacent literature is negative, and the regulator has a history here

If any NeurOne claim drifts from C1/C2 toward C3 — protection of the wearer, or better outcomes
*because of* shielding — it lands in idiopathic environmental intolerance attributed to electromagnetic
fields (IEI-EMF), where the evidence is consistent and unfavourable.

| Finding | Source |
|---|---|
| **46 blind or double-blind provocation studies, 1,175 IEI-EMF volunteers**: no association between EMF presence and symptoms | Rubin, Nieto-Hernandez & Wessely 2010, updated systematic review |
| Symptoms closely associated with a **nocebo** effect; alarmist media coverage contributes | Rubin et al. 2011, *Bioelectromagnetics* 32:593 |
| No validated criteria for defining IEI-EMF, which limits study quality | BMC Public Health 2012;12:643 |

**FTC enforcement history in precisely this product space:**

- **WaveShield** cell-phone radiation patches — settled; order bars false or unsubstantiated
  radiation-blocking claims and requires substantiation by *competent and reliable scientific
  evidence* (FTC, December 2003).
- 2011 settlements with two "radiation-reducing" phone-sticker marketers; 2020 consumer warnings on
  "5G protection" products.
- [Health Products Compliance Guidance](https://www.ftc.gov/business-guidance/resources/health-products-compliance-guidance) (2022) sets the substantiation standard.

The DTC neurotechnology literature separately flags consumer EEG as the segment that has *largely
escaped scholarly and regulatory critique* while relying on neurofeedback evidence (Wexler & Thibault,
*J Cogn Enhanc* 2019; Coates McCall et al., *Neuron* 2019).

> **Binding conclusion.** A dB attenuation figure is a product-performance claim and is defensible once
> measured. It is **not** an efficacy claim and must never be presented adjacent to an outcome claim in
> a way that implies one. `competitive-position.md` §176's published line — *"Only consumer brain
> device with palladium-fabric EMF shielding verified by continuous fleet monitoring"* — is C1-shaped
> and acceptable in form, subject to `OI-BIBEMF-01` on "only consumer" and to `EMF-1` on "verified."

---
## 7. Per-layer value audit — does every layer earn its cost?

> **Standing principle applied here (principal direction, 2026-09-15):** *every cost must be justified
> by the value it provides.* This section applies that test layer by layer. It reaches one removal
> recommendation, one cost challenge, two keeps, and one finding that the test **cannot yet be run at
> all**.

### 7.0 The audit cannot be completed, and that is the first finding

**`docs/np_cost_001.md` contains no BOM line for any part of the shielding stack** — no mu-metal, no
palladium, no absorber foam, no port filters, no fluxgates, no Helmholtz coils. A repo-wide search of
the cost model returns nothing on any of them.

The only shielding figure anywhere in the cost record is `durability-maintenance.md` §30's **+$6/headset
for palladium over silver** — a *delta between two options*, not the cost of the layer, and not the cost
of the stack.

> **So for four of the five passive layers and the entire active subsystem, the cost side of
> "cost must be justified by value" is unknown.** CLAUDE.md §2.1 already warns that every cost figure
> is a floor excluding term **U**; the shielding stack is a *second* uncosted term, and unlike U it is
> not flagged anywhere. Against four gross-margin-negative T1 configurations this is the single most
> actionable gap in this document. **`OI-BIBEMF-07`, and it gates the honest version of this audit.**

### 7.1 Verdict table

| Layer | Stated benefit | Benefit evidence | Cost | Verdict |
|---|---|---|---|---|
| **L1** CFRP outer, 30–50 dB RF | structural shell + RF | §3 supports RF for EEG | **none attributable** — it is the chassis | **Keep.** No shielding-attributable cost to justify |
| **L2** mu-metal 0.2 mm, 15–25 dB ELF | ambient ELF magnetic | **§4 — not supported** for EEG or any modality | thermally ~0; architecturally high | **Keep, on other grounds.** §7.2 — its stated rationale is the wrong one |
| **L3** Pd-polyester, 40–60 dB RF | RF + permanent claim | §3 supports RF for EEG | layer uncosted; **+$6 Pd premium** | **Keep the layer. Challenge the premium** — §7.4 |
| **L4** carbon-loaded absorber foam | *"cavity-resonance suppression"* | **REFUTED** — 0.26 dB of the 26.2 dB `REQ-CAV-02` needs (`NP-EMC-CAV-001`) | 18 % of the outward path; **no tooling cut and nothing published**, so removal is a CAD edit | **DELETE — §7.3a.** With the 3 mm bowl re-loft **binding** (without it, 0.450 vs 0.410). The audit's one removal |
| **L5** USB-C + port filters, 30–50 dB | conducted emissions | FCC Part 15 (`DI-REG-05`) | small, standard parts | **Keep.** Compliance-required |
| **Active** fluxgates + Helmholtz | ELF trim | ambient benefit unsupported (§4); holds L2 in place | high — `REQ-EMI-10` | **The real question — §7.6** |

### 7.2 Layer 2 — cannot be removed, and not for the reason it is documented

§4 shows L2 buys little or nothing *for EEG against ambient ELF*. It does not follow that it is
removable. Three dependencies are load-bearing, one is a compliance unknown, one is claim arithmetic.

**(a) The Helmholtz coils are co-designed with the mu-metal as one magnetic circuit — decisive.**
`NP-HEX-ZM-001` §5.3.1 reason 2, verbatim:

> *"The coil is co-designed with the mu-metal as one magnetic circuit. The high-permeability mu-metal
> (outer bowl) shunts and reshapes any nearby coil's flux… co-locating passive shield + active trim on
> the outer bowl keeps them a single characterized subsystem."*

The calibrated quantity in the active loop is the **coil-drive → field transfer function**, defined
*with the mu-metal present*. Removing L2 does not subtract 15–25 dB from a total — it invalidates the
actuator characterisation on all three axes, and with it `REQ-EMI-05`'s feed-forward subtraction and
`REQ-EMI-11`'s per-configuration recalibration. The active half would need redesign, not re-measurement.

**(b) L2 is the degraded-mode fallback.** `NP-ENV-OPRANGE-001` §2 makes the active-cancellation envelope
**SOFT** *because* *"fluxgate temp drift degrades cancellation; **passive 5-layer shield always
present**"*. Remove L2 and a soft bound becomes a hard one, re-opening `NP-ENV-001`'s operating range.

**(c) L2 carries the shell's magnetic-continuity architecture.** `NP-HEX-ZM-001` §5.2/§5.3(d)
consolidates all shielding onto one unbroken outer bowl *specifically* because magnetic shields leak at
butt-joints. `NP-HELMET-GEOM-001` §89 routes L2 around the TMS window. `NP-THERM-COOL-001` §6.2 and
`OI-THCOOL-06` (~~BLOCKING~~ **CLOSED 2026-08-30** — see the §1 status correction; recommended for reopening against the collar by `OI-EMCCAV-10`) exist to protect its reluctance at the posterior boss.

**(d) IEC 61000-4-8 may require it.** IEC 60601-1-2 incorporates power-frequency magnetic immunity
(typically 30 A/m ≈ 37.7 µT — far above §4.2's 0.1 µT ambient). That is **device immunity**, not user
protection, and with fluxgates and a µV front end inside the envelope it is plausibly load-bearing.
`regulatory-strategy.md` §8 **omits IEC 60601-1-2 entirely**. L2 must not be deleted against a standards
list that does not yet name the standard that may require it. **`OI-BIBEMF-04` gates any L2 removal.**

**(e) The claim arithmetic moves.** L2 supplies 15–25 dB of the combined 35–45 dB ELF figure; removing
it leaves the ELF half resting on an active loop that `OI-PWRTH-05` already records as off for most of a
TMS train. CLAUDE.md §1 and §4.3 would both need re-derivation against 663 citations guarded by
`scripts/check-section-refs.ts`.

> **The finding worth keeping:** L2 is held in place by the active-cancellation architecture, the
> degraded-mode fallback and possibly EMC — **none of which is its documented rationale.**
> `hardware-detail.md` §4.3 justifies it by the one benefit §4 cannot support and omits the three that
> actually hold it. That is a documentation defect, and it is why the layer looked removable.

### 7.3 Layer 4 — the one line that fails the test today

> **⚠ UPDATED AT REV 2 (2026-09-20) — `OI-BIBEMF-08` IS DISCHARGED, AND NOT AS THIS SECTION
> EXPECTED.** `NP-EMC-CAV-001` answers the burden step 1 placed on EMC. Read §7.3a below before
> acting on anything in this subsection: **the premise corrected is this section's own.** The Rev 1
> text is retained unedited, per `NP-CONV-001` §7 — it was outweighed by a calculation, not
> refuted in its reasoning, and the burden it placed is what produced the answer.

**Layer 4 is the only layer in the stack whose benefit has never been stated in any measurable form,
anywhere.** Its entire documented justification is one phrase — *"cavity-resonance suppression"* —
repeated verbatim in `hardware-detail.md` §4.3, `NP-HELMET-GEOM-001` §70 and `NP-DT-001` `DI-PERF-22`.

In `DI-PERF-22` — the design-trace row that carries the whole claim — **every other layer has a dB
figure and Layer 4 has none:**

> *"CFRP 30–50 dB + mu-metal 15–25 dB ELF + palladium fabric 40–60 dB RF + **absorber foam** + port
> filters"*

It is explicitly **not** primary shielding. `docs/status/completed-decisions.md` §221: *"the absorber
suppresses cavity resonance rather than providing primary shielding — the mu-metal and Pd-polyester do
that."*

**And the source it suppresses is undocumented.** `NP-HEX-ZM-001` §5.3a bounds the RF concern to the
6 GHz Wi-Fi band and states *"the headset's own radios live in the hub, not here."* CLAUDE.md §1 and
§4.1 make the same commitment — *zero RF at scalp*, *antennas in control hub NOT headset*. No document
identifies what excites a cavity resonance inside the envelope, at what frequency, with what Q, or what
the consequence of not suppressing it would be.

> **This is not a claim that no source exists.** `NP-DRV-SHELL-002` puts **18 active cluster
> controllers** on L1 inside the cavity, and digital edges carry content into the hundreds of MHz where
> a helmet-scale cavity resonates. A real justification may well be available. **The finding is that
> nobody has ever written it down in any unit that can be tested against a cost.**

**Meanwhile the cost is precisely quantified, and it is large:**

| Cost | Figure | Source |
|---|---|---|
| Share of the outward thermal path | **18 %** (0.075 m²K/W, 3 mm at k ≈ 0.04) | `NP-THERM-COOL-001` §2 |
| Rank among attackable terms | **second-largest**, above external convection | ibid. |
| Sensitivity | *"the largest lever in §3.1's decomposition"* | `OI-SINK-07` |
| **Blocks thermal fix #1** | it is compressible, so a gap pad pressed against it *"never reaches rated conductivity"* | `OI-THCOOL-15` / completed-decisions §221 |
| **Blocks thermal fix #2** | `REQ-EMI-10` bars conductive additions protecting the magnetic subsystem it sits in | `RISK-SINK-03`, `OI-SINK-01` **BLOCKING** |

A layer with an unquantified benefit and a quantified cost that is blocking two separate fixes to a
**BLOCKING** thermal item is exactly the line the standing principle is aimed at.

**Recommended action — and removal is *not* the first instrument.** `OI-THCOOL-04` already proposes
replacing open-cell carbon foam with a **conductive-filled absorber elastomer (k ≈ 1–3 W/m·K against
~0.04)**, taking 0.075 → **~0.02 m²K/W** — *"a materials substitution with no architectural consequence
— the cheapest row in §5's table by a wide margin."* That recovers most of the thermal cost **while
keeping whatever RF benefit exists**, and a non-compressible elastomer also unblocks `OI-THCOOL-15`'s
gap pad. So:

1. **EMC states Layer 4's requirement in dB against a named source and frequency band** — the thing
   that has never been done. **`OI-BIBEMF-08`.**
2. If the requirement is real → adopt `OI-THCOOL-04`'s conductive-filled substitution. Both thermal
   blockers clear and the RF function is retained.
3. **If EMC cannot state a requirement → delete Layer 4.** The stack becomes 4-layer, CLAUDE.md §1's
   *"5-layer"* wording changes, and two thermal levers are recovered at negative BOM cost.

Either outcome is strictly better than today, and step 1 is the gate. **This is the audit's one live
removal candidate.**

### 7.3a What step 1 returned — `OI-BIBEMF-08` discharged (Rev 2, 2026-09-20)

`NP-EMC-CAV-001` ran the gate. **Neither branch of §7.3's step 2 is what came back**, and the two
corrections belong to this document rather than to that one.

**Correction 1 — §7.3's search for the source failed because it was looking for a radio.** §7.3 is
right that `NP-HEX-ZM-001` §5.3a bounds the RF concern to 6 GHz Wi-Fi and that the radios live in the
hub. That is the correct bound for *ingress* and the wrong one for this question. §7.3's own caveat
was the answer: `NP-DRV-SHELL-002` §3.2 puts **18 STM32G071 cluster controllers** on L1 inside the
envelope, plus a 400 kHz I2C tree, 80 PWM LED drivers and the ADS1299 SPI bank — and §9.6 of that
document already states the consequence in terms, *"A Faraday cage does not protect the EEG electrodes
and fluxgates that share the enclosure with the source."* **The exciting source was named in the
document set the whole time, one document away.** `REQ-EMI-04`'s prohibition on spread-spectrum PWM
sharpens it further: deterministic lines are what a cavity responds to.

**Correction 2 — the requirement is statable, so step 2b's stated reason is void; but the layer still
goes, for a better one.** `REQ-CAV-01` (E ≤ 0.5 V/m, derived from `SH2-DRC-16` via RF demodulation,
which `REQ-EMI-05` cannot subtract) and `REQ-CAV-02` (loaded Q ≤ 20 over **420 MHz – 3 GHz**, i.e.
**≥ 26.2 dB**) are stated and testable. Against them:

| | dB |
|---|---:|
| `REQ-CAV-02` needs | **26.2** |
| **Layer 4 supplies** | **0.26** |
| The wearer's head supplies | **49.8** |

The reason is structural rather than a property of our foam: for a lossy slab on a conductor
`Z_in = j·η·tan(kd)`, and in the thin limit `η·k = η₀·k₀` **exactly**, so `Z_in → j·η₀·k₀·d` — purely
reactive, **independent of the loading**. At 3 mm the foam is λ/217 at the lowest mode. This is why
thin commercial absorbers are iron- or ferrite-loaded, which `REQ-EMI-10` and §7.8 forbid here.

**So step 2a is void too: there is no RF function to preserve, and `OI-THCOOL-04`'s substitution was
additionally wrong in its material.** A *conductive* fill makes a reflector, not an absorber; the
filler must be ceramic (`REQ-CAV-03`). `NP-EMC-CAV-001` §8.1 carries the correction.

**Two things follow immediately, and one does not wait for anybody:**

1. **The Layer 4 station may now be specified on thermal grounds alone**, because there is no
   electrical requirement on it in band. `OI-THCOOL-04` and `OI-THCOOL-15` are unblocked **today**.
2. **Delete the station — and the 3 mm re-loft of the outer bowl is BINDING** (`NP-EMC-CAV-001`
   §8.2, Rev 3). Vacating 3 mm without re-lofting fills it with stagnant air at 0.115 m²K/W against
   the foam's 0.075 — **54 % worse per mm** — so the outward path would go 0.410 → **0.450**. With
   the re-loft it goes to **0.335**, the best figure available, against a ceramic-filled
   substitution's 0.355.

> **Two things this audit briefly got wrong about L4 are worth recording, because both are the kind
> of error it exists to catch.** An intermediate revision argued the layer should be *kept* and
> re-specified, on the grounds that deleting it meant a **shell tooling change** and a change to a
> **published claim**. **Neither cost exists.** `NP-REV-SHELL-001` is *"DRAFT — open review; no item
> signed"*, the programme is **pre-tooling**, and `OI-ART-01` already owes a re-scope of
> `NP-TOOL-SHELL-001`; nothing is externally published, and `competitive-position.md`'s own note says
> its copy *"should be re-verified before publication"*. It also claimed the foam was the **compliant
> member** — but `NP-HEX-ZM-001` §5.4a puts the preload on **over-center lever-throw cluster clamps
> with per-module spring plungers** (`MECH-2`). The foam is *incidentally* compressible, which is why
> it obstructs `OI-THCOOL-15`'s pad; that is a nuisance, not a function.
>
> **So L4 is not a second Layer 2.** L2 is held by three real, undocumented dependencies (§7.2). L4's
> substitutes for those turned out to be an unbuilt mould, an unpublished sentence and an inferred
> function — **which is what a layer looks like when nothing holds it up. It is the audit's one
> removal**, and `OI-EMCCAV-08` (`MECH-2`) is the single question left: if the clamps cannot take up
> the tolerance stack without 3 mm of incidental compliance, a **thin ceramic pad sized by that
> stack** returns — not the 3.0 mm an RF justification picked.

### 7.4 Layer 3 — keep the layer, challenge the palladium premium

The **layer** is well justified: §3's literature makes conductive RF shielding the load-bearing element
for EEG signal quality, and this is the layer that provides it.

The **premium** is a different question. `durability-maintenance.md` §30 justifies palladium over silver
at **+$6/headset** on these grounds:

> *"Silver tarnishes 12–18 months. Palladium tarnish-immune for device lifetime. Fleet SHDR verifies
> stable attenuation — **marketable, measurable claim**."*

That is a **marketing rationale for an engineering premium**, and the engineering question behind it has
not been asked anywhere: *does silver tarnish actually degrade RF shielding effectiveness at our
frequencies?* Tarnish is a thin surface sulfide; RF current flows in the skin depth. Whether a sub-µm
sulfide layer measurably moves shielding effectiveness at 0.8–6 GHz is an EE question with a real answer,
and nobody in the document set has posed it.

Under the standing principle, $6 × fleet volume against four gross-margin-negative T1 configurations
needs the engineering answer, not the marketing one. If tarnish does not measurably degrade SE, the
premium buys a *claim* rather than a *capability* — which is the same error
`competitive-position.md` §34 identified when it retired the LED-count claim. **`OI-BIBEMF-09`.**

### 7.5 Layers 1 and 5 — no case to answer

- **L1 (CFRP, 30–50 dB RF)** is the structural shell. `NP-HELMET-GEOM-001` §261 makes shell strength and
  weight depend on it. Its RF attenuation is a byproduct of a part the product needs anyway, so there is
  **no shielding-attributable cost to justify**.
- **L5 (USB-C + accessory port filters, 30–50 dB)** is ordinary, cheap EMC practice and is required for
  FCC Part 15 conducted emissions (`NP-DT-001` `DI-REG-05`, T1 **and** T2). Compliance-required cost.

### 7.6 The cross-cutting finding — the stack is aperture-limited, not layer-limited

Every open item against the shielding claim is about a **seam or a hole**, not about a layer:

| Item | What it is about | Status |
|---|---|---|
| `RISK-20` | CFRP rim roughness Ra ≤ 1.6 µm → parting-plane slot leakage | **OPEN, BLOCKING for tooling** |
| `EMF-3` | gasket line-pressure map at back-centre and both ear spans | unmeasured |
| `OI-THCOOL-06` | ELF leakage through the posterior-boss penetration — *"a hole in mu-metal is a hole"* | **BLOCKING** |
| `EMF-1` | two-layer attenuation ≥ single-shell baseline | unmeasured |
| `OI-PWR-12` | a 5 A switched inlet inside the envelope | gating |

This is standard shielding behaviour: **the weakest aperture sets the floor, and layers above that floor
are wasted.** Our own documents reached it independently — `NP-HEX-ZM-001` §5.2 consolidates onto one
unbroken bowl for exactly this reason.

> **The consequence for the standing principle is sharper than any per-layer verdict: marginal spend on
> *layer count* is poor value while the seam budget is unmeasured, and marginal spend on *seam control*
> is good value.** `EMF-1` is not merely a verification task — it is the measurement that tells us which
> layers are doing anything at all. Until it runs, no layer's value is knowable, and this audit cannot
> be closed for any layer other than L4, whose benefit is unstated in principle rather than merely
> unmeasured. **`OI-BIBEMF-10`.**

### 7.7 The question behind the question

L2, the fluxgates, the Helmholtz coils and `REQ-EMI-10` stand or fall **together** as one subsystem, and
together they are what gate `SPEC-SINK-04`'s spreader against a **BLOCKING** `OI-SINK-01` with 4 of 22
protocols waiting.

> **The decision worth putting to the principal is not "remove a layer" but "is the active-cancellation
> subsystem earning its architectural cost?"** — with `NP-THERM-SINK-001` and `NP-PWR-THERM-001` as the
> cost side and §4 of this document as the benefit side. That is a §4.3 locked-decision reopening.
> **`OI-BIBEMF-05`.**

### 7.8 One new risk this review surfaced

Mu-metal acquires remanence, which is why MSRs have degaussing procedures (§4.1). `NP-HW-EEGNET-001`
§5.4 justifies `REQ-NET-04`'s 1 nAm² limit on the grounds that *"a fixed ferromagnetic or conductive
mass is calibrated out"* — true at calibration time. **No degaussing or remanence-drift procedure exists
anywhere in the document set**, and the shell takes impact events that SHDR already logs. A shock that
re-magnetises the liner shifts the fluxgate DC offset with no recalibration trigger, because
`REQ-EMI-11` fires on `np_module_map` rebuild — an insertion change, not an impact. **`OI-BIBEMF-06`.**

---

## 8. Open items

| ID | Item | Owner | Blocking? |
|----|------|-------|-----------|
| **OI-BIBEMF-01** | *"Only consumer brain wearable with measured shielding"* — resolve against research-grade dry-EEG products that ship Faraday shielding (§3). Both *"only consumer"* and *"measured"* need substantiation before publication | Marketing + Regulatory | Gates the §176 copy line |
| **OI-BIBEMF-02** | EE to check §4.3's induction estimate. If ambient ELF coupling into the EEG loop is ~150× below `SH2-DRC-16`, record it — it is the quantitative basis for §7.2 | EE Lead | No |
| **OI-BIBEMF-03** | No shielded-vs-unshielded outcome comparison exists in the literature for any modality. Decide whether NeurOne intends to generate one, or whether the claim stays permanently C1-only | Clinical + Principal | No |
| **OI-BIBEMF-04** | **Add IEC 60601-1-2 to `regulatory-strategy.md` §8's required-standards list** and determine whether IEC 61000-4-8 magnetic immunity depends on Layer 2 | Regulatory + EE | **Gates any L2 removal** |
| **OI-BIBEMF-05** | Principal question: is the active-cancellation subsystem (L2 + fluxgates + Helmholtz + `REQ-EMI-10`) earning its architectural cost, given it gates `OI-SINK-01`? §7.7 | Principal | No — interacts with a BLOCKING item |
| **OI-BIBEMF-06** | No degaussing or remanence-drift procedure for the mu-metal liner; impact events do not trigger `REQ-EMI-11` recalibration. §7.8 | EE + ME | No |
| **OI-BIBEMF-07** | **The shielding stack has no BOM line in `NP-COST-001`** — not mu-metal, palladium, absorber, filters, fluxgates or Helmholtz. The cost side of the standing principle is unevaluable for the whole stack. §7.0 | Systems + Cost | **Gates the per-layer audit** |
| ~~**OI-BIBEMF-08**~~ | ~~State Layer 4's requirement in dB against a named source and frequency band~~ **✅ DISCHARGED 2026-09-20 by `NP-EMC-CAV-001`** (§7.3a). Source named (18 cluster controllers, not radios), band derived (**420 MHz – 3 GHz**, lower edge moving 84 MHz with head circumference), requirement stated (`REQ-CAV-01`/`REQ-CAV-02`, **≥ 26.2 dB**). **Layer 4 supplies 0.26 dB; the wearer's head supplies 49.8 dB.** Neither step-2 branch applies as written: there is no RF function to preserve (2a) and EMC was not silent (2b). **Rev 4: DELETE the station, with the 3 mm bowl re-loft BINDING** — without the re-loft stagnant air makes it a regression (0.410 → 0.450); with it, **0.335**. The costs an intermediate revision charged to deletion (shell tooling, a published claim) **do not exist** — no mould is cut and nothing is published. `OI-EMCCAV-08` (`MECH-2`) is the one open question. The thermal half is unblocked now | EMC + Thermal. **DISCHARGED** | — |
| **OI-BIBEMF-09** | Does silver tarnish measurably degrade RF shielding effectiveness at 0.8–6 GHz? The +$6/headset palladium premium is justified in the record by a marketing rationale only (§7.4) | EE + Materials | No |
| **OI-BIBEMF-10** | `EMF-1` is not only a verification task — it is the measurement that reveals which layers contribute at all. No layer's value is knowable until the seam budget is measured (§7.6) | EE + EMC | Tracks `EMF-1` |

---

## 9. What this document does not establish

Stated explicitly, because a bibliography is easy to over-read:

1. **It does not measure anything.** `EMF-1`, `EMF-3`, `RISK-20` and `OI-THCOOL-06` are unchanged and all
   still open. The 35–45 dB / 40–60 dB figures remain design targets whose only source is CLAUDE.md.
2. **It does not show ambient EMF is harmless** — it shows that at residential levels no effect on our
   modalities has been demonstrated, and that shielding's *benefit* is therefore unmeasured. Absence of
   demonstrated effect is not demonstrated absence, and the C1 claim does not need it to be.
3. **It does not show Layer 4 is worthless** — it shows Layer 4's value has never been stated in a
   testable form while its cost has, which is why the burden now sits with EMC (`OI-BIBEMF-08`).
   **Rev 2:** that burden has been discharged (§7.3a). The value is now stated in a testable form and
   is **0.26 dB against a 26.2 dB requirement** — but that is still a *calculation*, not a
   measurement, and `EMF-1a`/`EMF-1b` are what would make it one. No layer has been removed.
4. **It does not modify any locked decision.** §4.3 is untouched. §7 raises reopenings for the principal;
   it performs none.
5. **The §4.3 calculation is this document's own**, not a citation, and is offered for refutation.

---

## 10. Revision history

| Rev | Date | Author | Change |
|-----|------|--------|--------|
| 1 | 2026-09-15 | NeurOne Systems Engineering | Initial release. Establishes the first evidence record for the shielding claim, which had none in `clinical.md`, `regulatory-strategy.md`, `marketing-notes.md` or `np_cost_001.md`. **Separates the claim into C1 performance / C2 signal quality / C3 outcome (§2)**: the literature supports C2 for the electric/RF layers, supports neither for the ELF-magnetic layer, and supports C3 for nothing on any of the eleven modalities — no shielded-vs-unshielded outcome comparison exists anywhere. **§7 applies the standing "every cost must be justified by value" principle layer by layer.** Four results. **(1) The audit cannot be completed**: `NP-COST-001` carries no BOM line for any part of the stack, so the cost side is unknown for four of five passive layers and the whole active subsystem (`OI-BIBEMF-07`). **(2) Layer 2 cannot be removed, but not for the reason it is documented** — `NP-HEX-ZM-001` §5.3.1 co-designs the Helmholtz coils with the mu-metal as one magnetic circuit, `NP-ENV-OPRANGE-001` §2 makes it the degraded-mode fallback, and IEC 61000-4-8 immunity may depend on it, while its *stated* rationale (ambient ELF attenuation) is the one benefit §4 cannot support. **(3) Layer 4 is the audit's one live removal candidate** — the only layer with no dB figure in any document including `DI-PERF-22`, explicitly not primary shielding, suppressing a resonance whose exciting source is named nowhere (the radios live in the hub), against a precisely quantified 18 % of the outward thermal path that blocks *two* separate fixes to a BLOCKING `OI-SINK-01`. Burden placed on EMC to state a requirement in dB or lose the layer (`OI-BIBEMF-08`). **(4) The stack is aperture-limited, not layer-limited** — every open item is a seam or a hole, so marginal spend belongs on seam control, and `EMF-1` is what reveals which layers contribute at all (`OI-BIBEMF-10`). Also challenges the +$6 palladium premium as justified by a marketing rationale only (`OI-BIBEMF-09`), and raises an unhandled mu-metal remanence-drift path (`OI-BIBEMF-06`). Raises `OI-BIBEMF-01…10`, three of which gate other work. No locked section modified; no figure in a released document rewritten; no measurement asserted; no layer removed. |
| 2 | 2026-09-20 | NeurOne Systems Engineering | **`OI-BIBEMF-08` discharged by `NP-EMC-CAV-001`; new §7.3a records what came back, and it corrects this document twice.** Rev 1's §7.3 text is retained unedited per `NP-CONV-001` §7 — it was outweighed by a calculation, not refuted, and the burden it placed is what produced the answer. **(1) §7.3's search for the exciting source failed because it was looking for a radio.** `NP-HEX-ZM-001` §5.3a's 6 GHz Wi-Fi bound is the right bound for ingress and the wrong one here; §7.3's own caveat held the answer, and `NP-DRV-SHELL-002` §9.6 had already stated the consequence in terms — 18 STM32G071 cluster controllers, a 400 kHz I2C tree, 80 PWM LED drivers and the ADS1299 SPI bank all sit **inside** the envelope, and a Faraday cage does not protect a victim sharing it. The source was one document away the whole time. **(2) Neither step-2 branch applies as written.** The requirement IS statable — `REQ-CAV-01` (E ≤ 0.5 V/m, from `SH2-DRC-16` via RF demodulation, which `REQ-EMI-05` cannot subtract) and `REQ-CAV-02` (Q_L ≤ 20 over **420 MHz – 3 GHz**, i.e. **≥ 26.2 dB**) — so 2b's stated reason is void; and there is no RF function to preserve, so 2a is void. **Layer 4 supplies 0.26 dB of the 26.2 dB; the wearer's head supplies 49.8 dB.** The reason is structural, not a property of our foam: for a lossy slab on a conductor `Z_in = j·η·tan(kd)`, and in the thin limit `η·k = η₀·k₀` exactly, so `Z_in → j·η₀·k₀·d` is purely reactive **independent of the loading** — at 3 mm the foam is λ/217 at the lowest mode. That is why thin commercial absorbers are magnetically loaded, which `REQ-EMI-10` and §7.8 forbid here. Also finds that the enclosure's lowest resonance is **a property of the wearer**, sweeping 84 MHz across the 52–62 cm SKU range — recorded nowhere before. **Consequences:** the Layer 4 station may be specified on thermal grounds alone **today**, unblocking `OI-THCOOL-04`/`OI-THCOOL-15`; `OI-THCOOL-04`'s *conductive*-filled substitution is separately wrong (a conductive fill makes a reflector; the filler must be ceramic — `REQ-CAV-03`); and deletion of Layer 4 is **recommended and routed to the principal**, gated on `EMF-1a`/`EMF-1b`. §7.1's L4 verdict row and §9.3 updated. **No locked section modified; no layer removed; no measurement asserted** — `EMF-1`, `EMF-3`, `RISK-20` and `OI-THCOOL-06` all unchanged and open. |
| 3 | 2026-09-20 | NeurOne Systems Engineering | **`NP-EMC-CAV-001` Rev 2 reverses its own §8.2, and §7.3a plus §7.1's L4 row follow it.** Rev 2 of this document recorded that Layer 4 was the audit's one live removal candidate, now with a number attached. **It is not a removal candidate. It is a MIS-JUSTIFIED layer — the same shape as Layer 2 (§7.2), and the audit's second instance of it rather than its one deletion.** The RF finding is untouched: the layer still supplies **0.26 dB of the 26.2 dB** `REQ-CAV-02` needs, and its *stated* benefit stays refuted. What changed is the thermal half, which both this document's §7.3 and `OI-BIBEMF-08` took for granted: **vacating a 3 mm station does not delete its resistance, it fills it with stagnant air**, and air is **54 % worse per mm** than the foam (0.115 vs 0.075 m²K/W, both conductivities from `NP-THERM-COOL-001` §2's own table). The outward path goes 0.410 → **0.450** on deletion unless the outer bowl is **re-lofted 3 mm** — a shell tooling change `NP-REV-SHELL-001` gates — while a **ceramic-filled substitution reaches 0.355 with no tooling change**, within 0.020 of the best case. The foam is also the **compliant member** across a curved 5–7 mm gap with a ±0.5 tolerance stack, which is precisely why `OI-THCOOL-15` exists; a non-compressible ceramic part keeps that budget *and* unblocks the pad, an empty gap keeps neither. And nothing is priced — Rev 2's *"negative BOM cost"* inherited an assumption this document's own `OI-BIBEMF-07` forbids. **`REQ-CAV-04` therefore retains the station and changes its justification of record from "cavity-resonance suppression" to "tolerance compliance + thermal path".** §7.1's verdict row moves from **FAILS THE TEST** to **Keep, on other grounds**, with the stated/actual split made explicit — the standing cost-justification principle is satisfied by correcting the justification, not by deleting the part. §7.3a and §8's `OI-BIBEMF-08` row updated; Rev 1's §7.3 body still retained unedited per `NP-CONV-001` §7. **No locked section modified; no layer removed; no measurement asserted.** |
| 4 | 2026-09-20 | NeurOne Systems Engineering | **Rev 3's "keep and re-specify" is withdrawn; §7.1's L4 row returns to DELETE, and the two errors Rev 3 made are recorded because both are the kind this audit exists to catch.** Rev 3 argued L4 should be kept on three grounds. **Two were costs that do not exist.** It called the 3 mm bowl re-loft *"a shell tooling change `NP-REV-SHELL-001` gates"* — but that document is **"DRAFT — open review; no item signed"**, CLAUDE.md's header puts the programme in **pre-tooling design phase**, and `OI-ART-01` already owes a re-scope of `NP-TOOL-SHELL-001` whose F-01 still describes the retired 5-colour zone-slot scheme; the re-loft is a CAD edit riding a re-scope already due. And it called *"5-layer"* **a published claim** — but **nothing is externally published**, and `competitive-position.md`'s own source note says its copy *"should be re-verified before publication"*, with the shielding line already flagged ⚠ for two unearned words. **The third ground was an inference the record contradicts:** Rev 3 claimed the foam is the **compliant member** taking up the ±0.5 tolerance stack, but `NP-HEX-ZM-001` §5.4a specifies the preload path as **over-center lever-throw cluster clamps with per-module spring plungers** (`MECH-2`). The foam is *incidentally* compressible — which is why it obstructs `OI-THCOOL-15`'s pad — not a designed member. **What survives is the arithmetic, re-cast as a condition on HOW to delete:** vacating 3 mm fills it with stagnant air at 0.115 against the foam's 0.075, so **the 3 mm re-loft is BINDING** (0.450 without it, **0.335** with, against substitution's 0.355 — and that 0.020 gap is contact resistance, not bulk). **So L4 is NOT a second Layer 2, and Rev 3's framing of it as one was the substantive error.** L2 is held by three real undocumented dependencies (§7.2); L4's candidates turned out to be **an unbuilt mould, an unpublished sentence and an inferred function** — which is what a layer looks like when nothing holds it up. **L4 is this audit's one removal**, and it is also its cheapest moment: no tooling cut, nothing published. `OI-EMCCAV-08` (`MECH-2`) carries the single remaining question — if the clamps cannot take up the tolerance stack without 3 mm of incidental compliance, a **thin ceramic pad sized by that stack** returns, not the 3.0 mm an RF justification picked. §7.1's row, §7.3a and §8's `OI-BIBEMF-08` row updated; Rev 1's §7.3 body still retained unedited per `NP-CONV-001` §7. **No locked section modified; no layer removed by this document; no measurement asserted.** |
| 5 | 2026-09-20 | NeurOne Systems Engineering | **Status correction: `OI-THCOOL-06` was already CLOSED when Rev 1 called it open, and this document propagated that error.** Its owning document, `NP-THERM-COOL-001`, closed it **2026-08-30 by D-2** — it was BLOCKING only on the pneumatic loop's penetration of the posterior boss, and §6.9 put that loop out of scope. Rev 1's §1 scope note and §7.6's aperture table both carried it as open/BLOCKING, and `NP-EMC-CAV-001` inherited it through four revisions. **The owning document wins.** `EMF-1`, `EMF-3` and `RISK-20` are unaffected and remain open, so §7.6's aperture-limited finding stands on the other three. **The closure trigger is nonetheless too narrow, and reopening is recommended:** the measurement is *"bench-measure ELF magnetic leakage through a **mu-metal chimney collar at the posterior boss**"*, which is about a **formed collar**, not about what passes through it — and `NP-EMC-CAV-001` §8.6.3 now proposes embossing that boss **outward**, which needs exactly that number. Routed as `OI-EMCCAV-10`, time-boxed by `MECH-1`. **No locked section modified; no other status changed.** |
