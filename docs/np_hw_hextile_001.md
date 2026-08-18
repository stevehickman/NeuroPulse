# Hex-Tile Module — Electrical / FPC Specification (T1-A, T1-C)

**Project:** NeurOne
**Document:** NP-HW-HEXTILE-001
**Revision:** 4
**Date:** 2026-08-16
**Status:** DESIGN STUDY — not a tooling baseline. Every numeric value below is a proposed engineering commitment, not a measured or locked figure. See §10 (Decisions) and §11 (Open Items).
**Effective Date:** —
**Author:** NeurOne Hardware Engineering
**Approved By:** — (pending design review)
**References:** NP-HEX-ZM-001 (2026-07-15) §3 geometry, §4 addressing, §4a module-type taxonomy + SMART-1; NP-HW-FPC-001 Rev 5 (SUPERSEDED — reused for driver topology §6.2, InGaAs PD selection §5.1, PDMS bonding §7, TIA-saturation methodology §5.3); NP-HW-HUB-001 Rev 3 (§3.1 cluster-controller tier, §7.4 interface contract, §7.5 OI-HUB-C17 synthesis, §8.1/§8.3); NP-DRV-SHELL-002 Rev 2 (§3.3a OI-HUB-C17c resolution, §5.1 socket contact budget, REQ-SKT-01, REQ-EMI-03/07); NP-FW-PBM1064-001 Rev 2; NP-OPT-PSF-001 Rev 1; NP-THERM-CFD-R1-001 Rev 1; NP-THERM-BEZEL-001 Rev 1; CLAUDE.md §3 (modality stack), §4.2 (safety architecture), §4.5 (power)
**Related Issues:** —
**Gate:** GATE-2 (PBM coupling bench) — LED array must meet dose spec at the temporal worst case before this layout is tooled
**IEC 62304 Class:** — (hardware; the on-module driver firmware is Class B, see §6.5)
**Supersedes:** — (new document; fills the gap declared in NP-HW-FPC-001 Rev 5 supersession note: *"no document yet specifies the T1-A/T1-C hex-tile FPC pinout or electrical layout"*)
**Parent Document:** NP-HEX-ZM-001

---

> **Rev 4 (2026-08-16) — §8.1's forward-voltage budget stated as a rule (new §8.1.1). No decision changed; nothing in §4, §7 or §9 is affected.**
>
> Rev 3 gave the budget only as two worked instances — `11 × 2.10 V` and `14 × 1.60 V` — and those
> instances were being cited elsewhere **as a forward-voltage constraint on the architecture**, which
> they are not: §4.3's V_f figures are design targets (OI-HEXTILE-02), and the string bound is a
> property of the **rail**, not of the emitters. `docs/status/pending-decisions.md` §13.2e traces where
> that misreading landed — a "~1.6–2.2 V the existing 660 nm string/driver architecture assumes" claim
> that was the sole stated electrical ground for excluding the only in-window NIR candidate under
> **OI-LED-W1**. §8.1.1 states the rule generally so a candidate emitter can be tested against it, and
> separates its two bounds, which are **different kinds of constraint**: `N · V_f ≤ 24 V − V_dropout`
> is a hard functional ceiling; `N · V_f ≥ 22.4 V` is a thermal budget allocation.
>
> Two gaps the generalisation exposed, both raised rather than closed: the **≤7 % allocation is
> asserted, not derived** (**OI-HEXTILE-18**, which also notes no minimum driver dropout is specified
> anywhere), and **no tolerance is stated on the 24 V rail** (**OI-HEXTILE-19**), so every string
> figure here is a nominal-point calculation. Also records that fixed-N string construction depends on
> `NP-PROC-FPC-001` §2.1's ±0.10 V Vf bin — load-bearing beyond the RISK-08 current-hogging grounds
> §2.1 currently gives for it. **No emitter was selected; OI-HEXTILE-02 and OI-LED-W1 remain open.**
>
> *Revision label note: the banner below is written "Rev C", the letter scheme in use when it was
> issued. Per `NP-CONV-001` §4.1 that maps positionally to **Rev 3**; §1.1 keeps historical records as
> written, so it is not renamed.*

> **Rev C (2026-08-11) — socket interface re-cut to 19 positions (§7.1–7.2, §7.3, §8.1). Hardware interface change; no firmware change requested here.**
>
> Closes `NP-DRV-SHELL-002` Rev 2 **OI-SHELL2-09(i)**, which flagged this document's 16-position interface as **blocking socket tooling** — a tooled interface, not a paper inconsistency.
>
> | Quantity | Rev 2 | **Rev C** | Cause |
> |---|---|---|---|
> | Socket positions | 16 | **19** | Two independent causes, below |
> | `VLED` / `PGND` | 4 + 4 | **3 + 3** | Principal decision 2026-08-11; ≥2× degraded-case rule (§8.1) |
> | `PD1_K`, `PD2_K`, `NTC` | absent (D-4) | **restored** | **OI-HUB-C17c resolved against D-4** — N3 survives |
> | `SYNC`, `DGND` | absent | **added** | REQ-EMI-03 phase reference (also OI-HUB-C05); REQ-EMI-07 return separation |
> | Pad layout | single row, 32 mm | **two staggered rows, ~18 mm** | Forced — 19 at 2.00 mm pitch does not fit a 40 mm hex in one row |
> | Current per `VLED` contact | 0.26 A | **0.35 A** nominal / **0.52 A** degraded | §8.1 |
> | Peak contact force per module | 4.8–8.0 N | **5.7–9.5 N** (34.2–57.0 N per 6-tile plate) | §7.1 |
>
> **The two causes are independent and should not be conflated.** Three of the four added positions come from a *decision reversal elsewhere* — `NP-DRV-SHELL-002` §3.3a keeps the TIA + ADC on the cluster controller, so network N3 still crosses the socket. Two more (`SYNC`, `DGND`) come from *requirements this document had not accounted for*: REQ-EMI-03 needs a deterministic phase reference that I2C broadcast cannot provide, and REQ-EMI-07 needs `PGND` to be LED return only. Against those five, `VLED`/`PGND` dropping 4+4 → 3+3 returns two positions.
>
> **What is NOT changed, deliberately.** **D-4's reasoning is not withdrawn** — §5.3's TIA-saturation analysis and its case for on-module conversion remain correct, and were outweighed rather than refuted (the conservative ADC-drift argument, FAI-SM-06). **§5.3 and §6 still read as though D-4 holds**, because rewriting them is a larger change than this revision's scope and would touch the driver topology, the BOM, and OI-HEXTILE-06. Raised as **OI-HEXTILE-15**. Nothing in §4 (emitter lattice), §9 (concurrency ceiling) or §8.2 (I2C fan-out) is affected.
>
> **`NP-HW-HEXTILE-001` §7.2 and `NP-DRV-SHELL-002` §5.1.4 now agree pin for pin.** They specify one physical interface; **HT-DRC-23** exists so that stays true.

---

> **Rev B (2026-08-04) — cluster-count correction. Documentation only; no firmware change.**
>
> Rev 1 §8.2 stated that NP-HEX-ZM-001 §5.4a "partitions ~80 tiles into 4–10 clusters". **4–10 was that document's figure for the retired 30-socket lattice** and was carried over to ~80 sockets without rescaling. Several component counts were sized off it. This revision derives the count from the lattice and the two standing principal decisions (**CLUSTER-1**, 7-hex flower; **SYM-1**, mirror-symmetric partition, 2026-08-04) and propagates it.
>
> | Quantity | Rev 1 | Rev 2 | Where |
> |---|---|---|---|
> | Clusters at n = 80 | 4–10 | **18** (provably minimal) | §8.2.1 |
> | D-8 VLED high-side switches | 4–10 | **18** | §8.4 |
> | I2C pull-up resistors | 20 (at 10 segments) | **36** (18 segments) | §8.2 |
> | I2C segments used of 32 available | "a small fraction" | **18 / 32 (56 %)** | §8.2 |
> | Cluster-controller boards / tier BOM | 10 / $63.40 | **18 / $114.12** | §8.2.1, NP-HEX-ZM-001 §4a |
>
> **Neither CLUSTER-1 nor D-7's muxing architecture changes.** One tier of PCA9548A muxing over LPI2C1–4 still suffices (32 ≥ 18). Two new open items are raised rather than papered over: **OI-HEXTILE-13** (whether per-cluster safety *policy* is wanted at 18 clusters — the same question NP-HW-HUB-001 §7.4 routes to OI-HUB-C07; the safety-MCU GPIO budget does *not* demonstrably close at 18, and the package is unspecified. **A proposed resolution is recorded at §8.4.1** — split the enable by IEC 62304 class — for safety review to accept or falsify, plus **§8.4.2**, which establishes what the Class C enable word can and cannot absorb — the active 38-byte heartbeat has **zero spare bytes**, and enable-bit positions double as charge-monitor channel indices, so only the §7.2 collapse-with-holes is genuinely free) and **OI-HEXTILE-14** (peer documents still sized off 10 or 12, and 18 exceeds the 16 cluster-tail connectors NP-DRV-SHELL-002 §7.1 provisions — **options and a recommendation at §8.2.2**).

---

> **⚠ READ FIRST — what this document is and is not.**
>
> NP-HEX-ZM-001 specifies the hex-tile **mechanical, socket, and addressing** model. This document specifies the **electrical and FPC** design inside one 40 mm tile, for the two PBM-only types (**T1-A** base, **T1-C** 1064 smart). It is the hex-tile replacement for the electrical content of the retired NP-HW-FPC-001.
>
> **T1-B (EEG/electrode tile) is deliberately out of scope for Rev 1** — its layout is a masking derivation from the lattice defined here (§4.5), but the pod clearance diameter that drives it is not yet fixed. **T2-D (1170 nm laser tile) is out of scope entirely** (laser drive ≠ LED drive).
>
> **Nothing here is looked up.** The retired document's LED counts, pitch, connector, and pinout were all derived from a 66 × 78 mm module and a 5-slot hub, and none of them survives the change of form factor or the SMART-1 decision. Every number below is derived in-line from a stated requirement and a stated assumption, and every assumption that is not yet backed by a datasheet or a bench result is named as such in §11.

---

## 1. Scope

Specifies the electrical design of the universal 40 mm hex tile as populated for **T1-A** (base PBM) and **T1-C** (1064 nm smart PBM):

1. Emitter lattice geometry, count, and wavelength allocation within the 40 mm hex footprint (§4)
2. Photodiode placement and the dose-metering signal chain (§5)
3. The on-module driver, and the decision to fit one to **every** tile type rather than only T1-C (§6)
4. The socket interface — contact technology, pin count, and pinout (§7)
5. Per-socket power and I2C delivery at ~80 sockets under SMART-1 (§8)
6. The concurrency ceiling that the existing power envelope imposes on whole-vault tiling (§9)

**Invariant inherited from NP-HEX-ZM-001 §4a and not re-opened here:** all tile types share one mould, one outline, one socket interface, and one orientation-only mechanical key. A "type" is an FPC population difference and nothing else. Consequently **the socket pinout is the union of every type's needs**, including types not specified in this revision — any pin that T1-B or a future type requires must be present at every socket. This is what makes §7 a **19**-position interface rather than a 10-position one. *(Rev B said 16; see the §7.1 Rev 3 banner. The union rule is unchanged — what changed is which networks cross the socket, after OI-HUB-C17c restored N3.)*

---

## 2. Requirements this design is derived from

| # | Requirement | Source |
|---|---|---|
| R-1 | Tile is a regular hexagon, 40 mm flat-to-flat, module cap radius R_m = 87 mm | NP-HEX-ZM-001 §3 |
| R-2 | Any tile type inserts into any socket; identity by UID self-report, not mechanical keying | NP-HEX-ZM-001 §4a |
| R-3 | Every socket is I2C- and TIA-capable (SMART-1) | NP-HEX-ZM-001 §4a, §7 |
| R-4 | PBM ceiling 400 mW/cm² peak pulsed at ≤25 % duty; 200 mW/cm² CW | CLAUDE.md §3 modality 1 |
| R-5 | Three-channel aggregate ceiling 600 mW/cm² | NP-FW-PBM1064-001 Rev 2 (OI-PBM-05) |
| R-6 | Emitter drive 120–180 mA for L70 80,000–100,000 h | CLAUDE.md §3 modality 1 |
| R-7 | Session dose 60 J/cm² (660/808 nm), 36 J/cm² (1064 nm) | NP-FW-PBM1064-001 Rev 2 |
| R-8 | Dual-PD dose metering: PD1 forward (behind PDMS), PD2 scalp-facing backscatter; ratio separates fouling from ageing | CLAUDE.md §3 (RISK-14 Option B) |
| R-9 | Per-tile NTC, hardware current throttle at 62 °C junction; scalp face ≤42 °C | CLAUDE.md §4.2; NP-THERM-CFD-R1-001 (Path B1) |
| R-10 | Headset power envelope: T1 standard ~17–20 W, T1 peak ~45–50 W | CLAUDE.md §4.5 |
| R-11 | Safety MCU physically owns stimulation enable; app crash cannot cause unsafe output | CLAUDE.md §4.2 |
| R-12 | Module UID drives auto-inventory; re-inventory only on UID change | NP-HEX-ZM-001 §4 |

---

## 3. Available area — the budget everything else spends

| Quantity | Value | Derivation |
|---|---|---|
| Tile flat-to-flat, W | 40.00 mm | R-1 |
| Tile circumradius, a = W/√3 | 23.09 mm | |
| Tile area, (√3/2)·W² | 13.86 cm² | NP-HEX-ZM-001 §3.1 |
| Perimeter bezel (assumed) | 2.50 mm | NP-HEX-ZM-001 §3.1 coverage column — **conflicts with NP-THERM-BEZEL-001's 1.0 mm; see OI-HEXTILE-01** |
| **Active field** flat-to-flat, W_a = W − 2·bezel | **35.00 mm** | |
| Active field circumradius, a_a | 20.21 mm | |
| **Active field area, A_a** | **10.61 cm²** | (√3/2)·35² = 1061 mm² |
| Active fraction | 76.6 % | matches the 77 % in NP-HEX-ZM-001 §3.1 ✓ |

The 2.5 mm bezel is the **conservative** choice of the two conflicting figures in the existing document set: it yields the smaller active field and therefore the tighter irradiance and thermal budgets. If OI-HEXTILE-01 resolves to 1.0 mm, A_a rises to 12.15 cm² (+14.5 %) and every irradiance figure below improves; no conclusion in this document is invalidated by that direction of change.

---

## 4. Emitter lattice

### 4.1 The lattice — one geometry for every tile type

**Decision (D-1): a 5-ring centered-hexagonal lattice of 91 sites, at 3.80 mm pitch, identical on every tile type.**

A centered hexagonal array of n rings holds 3n² + 3n + 1 sites. For n = 5 that is **91**, and the array's own boundary is a hexagon — so it registers to the tile outline with no wasted corners, which a square grid cannot do inside a hexagon.

| n (rings) | Sites | Max pitch inside a_a = 20.21 mm | Areal density at that pitch |
|---|---|---|---|
| 4 | 61 | 5.05 mm | 5.7 /cm² |
| **5 ★** | **91** | **4.04 mm** | **8.6 /cm²** |
| 6 | 127 | 3.37 mm | 12.0 /cm² |

Pitch is set to **3.80 mm**, not the 4.04 mm ceiling, leaving 1.2 mm of clearance between the outermost emitter sites and the active-field boundary for placement tolerance and the PDMS window edge bead. Array circumradius = 5 × 3.80 = **19.00 mm**.

This independently reproduces the estimate NP-HEX-ZM-001 §3.1 carried without deriving — *"the densest tile (tri-wavelength PBM ~90 elements at ~3.5 mm pitch)"*. That the row-construction guess and this packing derivation agree at ~90 is the same two-independent-ways corroboration the parent document applied to the socket count.

**Site 0 (the array centre) is reserved and carries no emitter on any type.** On T1-A and T1-C it is the PD1 aperture (§5.1); on T1-B it is the electrode pod axis (§4.5). Reserving it once, for all types, is what lets a single lattice serve every population. **90 emitter sites remain.**

### 4.2 Wavelength allocation

A triangular lattice is exactly 3-colourable (each site's six nearest neighbours are three of one other colour and three of the third), which makes the 3-wavelength case the natural one and the 2-wavelength case a merge of two colours.

| Type | CH_A 660–670 nm | CH_B 808–830 nm | CH_C 1064 nm | Total emitters |
|---|---|---|---|---|
| **T1-A** | 45 | 45 | — | **90** |
| **T1-C** | 30 | 30 | 30 | **90** |
| *T1-B (out of scope, §4.5)* | *~22* | *~22* | *—* | *~44* |

T1-A interleaves CH_A and CH_B by alternating lattice rows. A triangular lattice contains odd cycles and therefore admits no perfect 2-colouring; row alternation leaves same-wavelength adjacency along one of the three lattice axes. This is accepted — at 3.8 mm pitch behind a diffusing PDMS window with several mm of standoff, per-wavelength granularity below the optical mixing length is not observable at the scalp, and NP-OPT-PSF-001 establishes that spatial structure well below the ~26 mm resolution floor buys nothing at depth.

### 4.3 Irradiance — does 90 emitters actually reach spec?

**Assumed emitter performance (OI-HEXTILE-02 — no base-tile emitter part is selected; these are design targets the eventual part must meet, not datasheet values):**

| Channel | V_f at 150 mA | Radiant flux at 150 mA | Implied WPE |
|---|---|---|---|
| CH_A 660–670 nm | 2.10 V | 95 mW | 30 % |
| CH_B 808–830 nm | 1.60 V | 95 mW | 40 % |
| CH_C 1064 nm | 1.40 V | 10 mW | 4.8 % |

1064 nm is an order of magnitude worse because it is far off the efficient direct-bandgap window; the retired document's part (EPITEX L1064-02AU, NP-PROC-FPC-1064-001) remains the reference and its low flux is the reason the retired design needed 150 emitters to do anything. That constraint does not go away in a smaller tile — it is why §4.3.2 concludes what it does.

**4.3.1 T1-A, per channel at full 150 mA drive:**

E = N · Φ / A_a = 45 × 95 mW / 10.61 cm² = **403 mW/cm²**

This lands, by construction, on the **400 mW/cm² pulsed peak ceiling** (R-4). That is the intended design point: **full drive at the top of the L70 current window equals the firmware-enforced peak ceiling**, so the array cannot be commanded past its own optical limit even before firmware intervenes, and CW operation at the 200 mW/cm² ceiling runs at roughly half current (≈75 mA), comfortably inside the L70 window and at better WPE.

**4.3.2 T1-C, all three channels:**

| Channel | N | E at 150 mA | Session time to reach dose (R-7) |
|---|---|---|---|
| CH_A 660–670 | 30 | 269 mW/cm² | 60 J/cm² ÷ 0.269 W/cm² = 3.7 min |
| CH_B 808–830 | 30 | 269 mW/cm² | 3.7 min |
| CH_C 1064 | 30 | **28 mW/cm²** | 36 J/cm² ÷ 0.028 W/cm² = **21 min** |
| **Aggregate** | 90 | **566 mW/cm²** | vs 600 mW/cm² ceiling (R-5) ✓ |

Two results worth stating plainly:

- **The aggregate ceiling is satisfied with 5.7 % margin at full drive on all three channels simultaneously** — not by firmware throttling, but by the emitter count itself. That is the desirable ordering: the hardware cannot exceed the limit that firmware also enforces.
- **1064 nm session length is set by emitter efficiency, not by protocol choice.** 21 minutes at full drive is a plausible session, but it is the floor — there is no headroom to shorten it, and any 1064 nm protocol shorter than ~21 min cannot reach the 36 J/cm² dose in a 40 mm tile. This is a real narrowing versus the retired 66 × 78 mm module and it should be checked against the clinical protocols in `protocols/predefined/clinical-03-pbm-cognitive-1064.npps` before this layout is tooled (**OI-HEXTILE-03**).

### 4.4 Uniformity

The retired 6 mm-pitch design claimed ±15–25 % irradiance variation. At 3.80 mm pitch the source spacing is 37 % smaller and the emitter count per unit area is 2.7× higher, so variation should improve materially — but **this document does not assert a number.** Uniformity depends on emitter beam angle, PDMS diffuser scattering coefficient, and window standoff, none of which is fixed yet. The claim is deferred to the illumination model (**OI-HEXTILE-04**); the GATE-2 coupling bench in NP-HEX-ZM-001 §7 is the measurement that settles it.

What can be said without a model: the **inter-tile** seam, not the intra-tile pitch, is now the dominant uniformity term. A 2.5 mm bezel on each of two adjacent tiles puts 5 mm of unpopulated width between the outermost emitters of neighbouring tiles — larger than the 3.8 mm intra-tile pitch. Whole-vault uniformity is therefore a bezel problem, which is a second and stronger reason to resolve OI-HEXTILE-01 toward the 1.0 mm figure if thermal analysis permits.

### 4.5 T1-B — why it is a masking derivation, not a separate layout

T1-B is out of scope for Rev 1, but the lattice was chosen so that T1-B is a depopulation of it rather than a new design. Depopulating whole rings around the reserved centre opens a circular clearance for the spring electrode pod:

| Rings depopulated | Sites removed | Emitters remaining | Clear diameter |
|---|---|---|---|
| 0–1 | 7 | 84 | 7.6 mm |
| 0–2 | 19 | 72 | 15.2 mm |
| 0–3 | 37 | 54 | 22.8 mm |

NP-HEX-ZM-001 §4a states T1-B has "~half the LED count", which corresponds to depopulating rings 0–3. The actual choice is set by the pod body diameter, which is not specified anywhere in the current document set (**OI-HEXTILE-05**). Whatever it resolves to, T1-B needs no new FPC outline, no new lattice, and no new socket interface — only a different placement file.

---

## 5. Photodiodes

### 5.1 PD1 — forward emission, at site 0

**Decision (D-2): PD1 occupies the reserved centre site, on the emitting face, behind the PDMS window.**

Reusing the array centre gives PD1 a position that is (a) identical across types, (b) already excluded from the emitter placement, and (c) at the point of maximum optical symmetry, so the PD1 reading is insensitive to which lattice axis an individual emitter batch is weak on. The centre reads above the field spatial average because of edge roll-off; this is a fixed multiplicative offset absorbed by the **existing** per-wavelength K coefficients written at factory calibration (NP-FW-PBM1064-001 Rev 2 §6.6, integrating-sphere procedure) — no new calibration mechanism is introduced.

- **Component:** InGaAs, Hamamatsu G12180-010A or qualified equivalent — inherited unchanged from NP-HW-FPC-001 Rev 5 §5.1, which its supersession note lists as still-reusable.
- **Fitted on both T1-A and T1-C.** InGaAs is broadband (600–1700 nm), so one part covers both the 2-channel and 3-channel populations and there is one PD SKU across the tile family. Per-wavelength dose separation is by firmware time-multiplexing and K coefficients, exactly as NP-FW-PBM1064-001 Rev 1 §6.2 already specifies.
- **Pad geometry:** 1.6 mm annular ring, hard gold ≥0.5 µm cobalt-alloyed — inherited unchanged.

Fitting InGaAs to T1-A (which has no 1064 nm channel and could use cheaper silicon) is a deliberate cost-for-uniformity trade: one PD part number across all tiles, no per-type TIA gain question, and no possibility of a silicon-PD tile being calibrated with InGaAs coefficients. See D-4 for why the gain question disappears entirely.

### 5.2 PD2 — scalp-facing backscatter

Co-located with PD1 in XY, on the opposite (scalp-facing) copper layer. The PD1/PD2 ratio is the fouling-versus-ageing discriminator (R-8), and that logic is only valid if both photodiodes sample the same optical path — co-location is load-bearing, not incidental. This is inherited directly from NP-HW-FPC-001 Rev 5 §5.2 and is the one geometric relationship that carries over from the retired design unchanged.

Same part, same pad geometry as PD1.

### 5.3 The TIA question — and why it stops being a hub problem

NP-HW-FPC-001 Rev 5 §5.3 established that InGaAs responsivity at 1064 nm (~0.90 A/W) is ~2× silicon at 808 nm (~0.47 A/W), saturating a 47 kΩ hub-side TIA, and resolved it with a per-slot DG2788A gain switch. SMART-1 turns that per-slot fix into a per-socket one, which NP-HEX-ZM-001 §4a and NP-HW-HUB-001's supersession note both flag as unscoped Hub PCB NRE at ~80 sockets.

**Decision (D-4): the TIA and its ADC move on-module. No PD analog signal crosses the socket interface.**

The saturation analysis in NP-HW-FPC-001 Rev 5 §5.3 remains correct physics; what changes is where it is solved. On-module:

- The gain is fixed at design time to the PD actually fitted, because the module knows what it is. There is no runtime gain-switch, no `GAIN_SEL` line, no DG2788A, and no ZONE_ID-to-gain sequencing hazard (the retired NP-HW-HUB-001 §5.1 ordering requirement disappears rather than scaling to 80).
- No high-impedance analog signal is routed across a spring contact and a metre of FPC through a shell that also carries LED switching currents and sits millimetres from µV EEG leads. This was already marginal at 5 slots; at 80 it is the dominant noise-injection path in the system.
- **The ~80× DG2788A + cascaded-analog-mux Hub PCB NRE that SMART-1 opened is not redesigned — it is deleted.**

Cost of the decision: the on-module MCU's ADC now sets dose-metering resolution. The ATtiny402 named in the retired design has a 10-bit ADC, which is thin for a dose claim that is a stated competitive differentiator. **A tinyAVR 2-series part (ATtiny426/427-class, 12-bit ADC with PGA) is specified instead** — same architecture, same UPDI programming, same package family, ~$0.10–0.15 more. See §6.2.

---

## 6. On-module driver

### 6.1 Every tile carries a driver — not just T1-C

**Decision (D-3): the on-module driver, which NP-HEX-ZM-001 §4a describes as a T1-C distinguishing feature, is fitted to every tile type.**

This is the largest single departure from the retired architecture and it is forced, not chosen. The retired 20-pin connector spent **16 of 20 pins on LED anode/cathode pairs** carrying drive current from hub-side drivers. Scaling that to the hex lattice:

- 80 sockets × 3 channels = **240 hub-side constant-current driver channels**, versus 15 in the retired design.
- 80 sockets × up to 16 current-carrying conductors = **~1,280 power conductors** to route from the hub, through the posterior blind-mate boss (NP-HEX-ZM-001 §5.3c), across the two-bowl parting plane, and out to the tile field.
- Every one of those conductors is a dI/dt source running alongside the EEG harness inside the Faraday envelope, in a product whose primary technical claim is measured EMF shielding.

None of that is buildable. Moving the driver on-module reduces the socket's power interface to a single DC rail pair and reduces the hub's job from *driving* 240 channels to *supplying* one bus and *commanding* over I2C. Combined with D-4, the socket interface becomes fully type-independent, which R-2 requires anyway.

**The cost is real and is not hidden:** see §6.4.

### 6.2 Driver topology

Inherited in concept from NP-HW-FPC-001 Rev 5 §6.2 — its supersession note lists the ATtiny + N-FET architecture as still-reusable — and re-validated here for the smaller tile.

| Ref | Function | Part class | Notes |
|---|---|---|---|
| U1 | I2C slave MCU, 3× PWM, ADC | tinyAVR 2-series, ATtiny426/427-class, SOIC-8/SOT-23-8 | 12-bit ADC + PGA for PD metering (§5.3); TWI address-match wake from standby (§8.3) |
| Q1–Q3 | Low-side N-MOSFET per channel | IRLML6344-class, SOT-23 | V_GS(th) 0.4–1.0 V, fully enhanced at 3.3 V gate drive |
| R1–R3 | Current sense per channel | 0.5 Ω / 1 Ω, 0402, 1 % | value set by string current, §6.3 |
| D1 | PD1 (InGaAs) | G12180-010A | §5.1 |
| D2 | PD2 (InGaAs) | G12180-010A | §5.2 |
| U2 | Transimpedance amplifier, dual | single-supply, rail-to-rail, SOT-23-8 | fixed gain per fitted PD (D-4) |
| RT1 | NTC thermistor | 10 kΩ, B25/85 3435 | on-module; read by U1 ADC, §6.5 |

**T1-A fits the identical assembly with Q3, R3, and the CH_C string omitted.** One rigidizer artwork, one pick-and-place program with a depopulation variant — the same "population differs, geometry does not" principle the mould already follows.

The FET thermal result from NP-HW-FPC-001 Rev 5 §6.2 carries over unchanged and with margin: at 180 mA and R_DS(on) = 27 mΩ, P = I²R = **0.87 mW** per FET. Dissipation is not a constraint on the driver; it is a constraint on the emitters (§9.3).

### 6.3 Physical fit — the 22 × 14 mm rigidizer in a 40 mm hex

The retired design placed the driver on a 22 × 14 × 0.8 mm FR4 rigidizer in a 24 × 16 × 3.5 mm cavity (NP-TOOL-ZM-SM-001 Rev 1 §4). The concern raised when this task was scoped — that this will not fit a much smaller tile — resolves favourably, for a geometric reason:

- A 22 × 14 mm rectangle has a half-diagonal of 13.0 mm. The 40 mm hex has an **inradius of 20.0 mm**. The rigidizer fits inside the tile outline with 7 mm of margin on every side. In-plane area was never the binding constraint; the retired module was simply large enough that nobody had to check.
- The binding constraint is **z**, and it is satisfied by the concave-shell geometry NP-HEX-ZM-001 §3.4 establishes: tiles tessellate at their **innermost** (emitting) faces and splay outward into the shell, "where gaps are harmless." The driver mounts on the **reverse (shell-facing) face of the tile FPC**, in exactly that roomy volume. It does not compete with the emitter field for area, because they are on opposite faces.

**Consequence for tooling:** because every type now carries a driver (D-3), the rigidizer cavity is a **standard feature of the universal hex-tile mould**, not a variant. This removes the last reason for a smart-module mould variant, complementing NP-HEX-ZM-001 §4a's removal of `OI-SM-SHELL-01` — the retired NP-TOOL-ZM-SM-001 has no successor, and none is needed.

### 6.4 BOM — stated, because it is a programme-level number

NP-HEX-ZM-001 §4a records the per-socket smart-capability cost as *"Known cost, not yet quantified."* Quantifying it is a deliverable of this document, and the answer is large enough to require a decision rather than an acknowledgement.

| Item | Unit | Per tile |
|---|---|---|
| U1 tinyAVR 2-series | $0.45–0.55 | $0.50 |
| Q1–Q3 N-FET (×3; ×2 on T1-A) | $0.12 | $0.36 |
| U2 dual TIA | $0.25–0.40 | $0.32 |
| D1/D2 InGaAs PD (×2) | $4.00–6.00 | **$10.00** |
| R/C passives, NTC, sense resistors | — | $0.20 |
| Rigidizer PCB (FR4, 2-layer) | $0.15 | $0.15 |
| **Driver + metering sub-total per tile** | | **~$11.53** |

**At 80 populated sockets this is ~$920 per headset** — against a Home Standard BOM of $405 (CLAUDE.md §2.1). The dominant term is not the driver at all; it is **two InGaAs photodiodes per tile, at ~$10 of the ~$11.50.**

This is a genuine programme-level finding and it is not resolvable inside a hardware layout document. Three directions, in rough order of attractiveness:

1. **Do not populate all sockets.** §9 shows the power envelope permits only ~5–6 tiles to run concurrently regardless; the lattice's value is *placement freedom*, not simultaneous activation. A build populating 20–30 tiles retains full protocol flexibility at a quarter of the cost. This is a configuration decision the product tiers already support.
2. **Silicon PD on T1-A.** Reverting §5.1's one-PD-SKU choice saves ~$9/tile on the majority type, at the cost of reintroducing a per-type calibration distinction (which D-4 makes safe, since gain is now set on-module per fitted part).
3. **One PD pair per cluster, not per tile.** Breaks the per-tile J/cm² metering claim, which NP-HEX-ZM-001 §4a explicitly protects ("each tile meters itself"). Listed for completeness; not recommended.

Routed to **OI-HEXTILE-06** for a cost/scope decision by the principal. **No option is selected here** — this document's job is to make the number visible, not to trade away a stated product claim.

### 6.5 Firmware boundary

The on-module MCU firmware is a new software item. It implements the register map already defined in NP-FW-PBM1064-001 Rev 1 §5.1 (registers 0x00–0x0D) extended with PD1/PD2 ADC readback registers made necessary by D-4, plus the on-module NTC and the local 62 °C throttle (R-9).

**The local throttle is a hardware-adjacent Class C-adjacent function and must be independent of hub commands** — a tile must throttle itself on over-temperature even if the I2C bus is silent. The safety MCU's independent backstop is the per-cluster VLED gate (§8.4), not per-socket, because an STM32G071 does not have 80 spare GPIOs. This two-level arrangement — fine-grained on-module, coarse hardware cut at the cluster — is proposed as the resolution to NP-HEX-ZM-001 §7 **OI-HUB-SOCKET-01**, which currently states that socket-addressed commands are dropped because `NP_SAFETY_EN_PBM_ZONE_0..4` is per-zone-slot.

Firmware specification is **not** in this document; it is **OI-HEXTILE-07** (successor to the retired NP-FW-ZM-TINY402-001 / OI-PBM-08).

---

## 7. Socket interface

### 7.1 Contact technology

The retired design used a Hirose FH34S 20-pin 0.5 mm ZIF with a back-flip lever. **That is not usable here.** A ZIF lever must be manually actuated per connector; NP-HEX-ZM-001 §5.4a's whole premise is that a user with Parkinson's H&Y II–III swaps tiles by throwing one cluster clamp and lifting the tile out on its ejector spring. A lever per tile contradicts the accessibility requirement the cluster clamp exists to satisfy.

**Decision (D-5, Rev 3): 19-position spring-contact (pogo) interface at 2.00 mm pitch, in two staggered rows. Spring pins on the socket, flat pads on the module.**

> **⚠ Rev 2 said 16 positions in a single row. Both figures are superseded, and the two changes have
> different causes — neither is a correction of an arithmetic slip.**
>
> | | Rev 2 | **Rev C** | Cause |
> |---|---|---|---|
> | Positions | 16 | **19** | §7.2 — three signals returned that **D-4** had removed, plus three the interconnect needs |
> | `VLED` / `PGND` | 4 + 4 | **3 + 3** | `NP-DRV-SHELL-002` Rev 2 §5.1.5, **principal decision 2026-08-11** |
> | Row layout | single row, 32 mm | **two staggered rows, ~18 mm** | forced by 19 at 2.00 mm pitch — see below |
> | Current per contact | 0.26 A | **0.35 A** nominal, **0.52 A** on loss of one | §8.1 |
>
> **What changed and why.** `NP-DRV-SHELL-002` Rev 2 §3.3a resolved **OI-HUB-C17c against D-4**
> (principal direction, 2026-08-11): the switched-gain TIA, PD mux, NTC mux and ADC stay on the
> cluster controller rather than moving on-module. **Network N3 therefore survives**, and with it
> `PD1_K`, `PD2_K` and `NTC` at the socket. That single decision accounts for three of the four added
> positions; `SYNC` and `DGND` are the fourth and fifth, and `SEAT#` — this document's own
> contribution — is retained. **Rev B's own §7.2 count was correct given D-4; D-4 no longer holds.**
>
> **`VLED`/`PGND` 4+4 → 3+3 is a separate decision with its own rule**, and the rule matters more
> than the number: *`VLED` is sized so the loss of any one contact still leaves ≥2× derating against
> the contact rating.* Rev 2's 4+4 gives ~3× degraded and was never wrong — it was more than the rule
> requires, at the cost of two extra contacts on an interface with a live RISK-22 one-handed-force
> constraint. See `NP-DRV-SHELL-002` §5.1.5 for the 2-vs-3-vs-4 comparison in full.

| Property | Value | Rationale |
|---|---|---|
| Positions | **19** | §7.2 |
| Pitch | 2.00 mm | unchanged |
| **Row layout** | **two staggered rows, nominally 9 + 10** | **Forced, not preferred.** A 19-position single row spans 36 mm pad-centre to pad-centre (38 mm including pads), which fits a 40 mm flat-to-flat hex only along the 46.19 mm vertex-to-vertex diagonal and tapers into the corners exactly where the ±0.4 mm lateral tolerance below is hardest to hold. Two rows span ~18 mm, inside the 20.0 mm **inradius**. This is `NP-DRV-SHELL-002` **REQ-SKT-01** |
| Springs on | socket (inner bowl) | keeps the moving, wearing, fatiguing element in the part that is never removed; the swappable tile is passive gold pad |
| Plating | hard gold ≥0.8 µm over nickel, both sides | fretting/oxidation resistance, per the §5.3b spring-finger precedent |
| Current per contact | ≥1.0 A continuous | pogo pins in this size class support 1–3 A; §8.1 needs **0.35 A/pin nominal, 0.52 A on loss of one contact** |
| Contact resistance | ≤50 mΩ | matched to the NP-HEX-ZM-001 §5.3b ground-bond target; binding for the electrode line (§7.2 note) **and now for `PD1_K`/`PD2_K`**, which carry 14–72 µA photocurrent (§7.2 note) |
| Mating cycles | ≥500 | service-event frequency, not daily; far below the Boa dial's 50,000 |
| Blind-mate tolerance | ±0.4 mm lateral, ±0.5 mm Z | spring travel absorbs cluster-clamp plate variation across a curved cluster. **The two-row layout is what keeps this holdable at 19 positions** |
| Contact force | 0.3–0.5 N per contact → **5.7–9.5 N per module** | **34.2–57.0 N per 6-tile clamp plate.** Note this is *below* the 18-contact/7-tile figure `NP-DRV-SHELL-002` Rev 1 carried, because no cluster reaches 7 tiles under the 18-cluster partition (§8.2.1) |

Pogo contacts also suit the **cluster clamp mechanics** directly: NP-HEX-ZM-001 §5.4a describes a clamp plate with a spring-loaded plunger per module, precisely because a rigid plate over a curved cluster cannot seat evenly. Spring contacts tolerate the residual Z variation that survives the plungers; a ZIF or board-edge connector would not.

Orientation is fixed by the tile's existing asymmetric mechanical key (R-2). The pad pattern is additionally asymmetric about the tile's long axis so a mis-keyed insertion cannot make contact — a fail-open, not a fail-wrong, geometry. **The two-row layout makes this easier, not harder:** a row-length difference (9 vs 10) is itself an asymmetry, where a single symmetric row needed a deliberate keying feature to carry it.

### 7.2 Pinout

**Nineteen positions.** The count is derived, not chosen: it is the union of every tile type's needs (§1), at the conductor width the power budget requires (§8.1), across the networks that actually cross the socket after **OI-HUB-C17c** (§7.1 banner). It is identical to `NP-DRV-SHELL-002` Rev 2 §5.1.4 — the two documents specify one physical interface and now agree pin for pin.

| Pin | Signal | Net | Direction | Rev 2 | Notes |
|---|---|---|---|---|---|
| 1 | VLED | N1 | socket → module | pins 1–4 | 24 V LED supply, §8.1 |
| 2 | VLED | N1 | socket → module | | paralleled ×3 for current sharing and single-contact redundancy |
| 3 | VLED | N1 | socket → module | | |
| 4 | PGND | N1 | — | pins 5–8 | **LED return only** — see the `DGND` note below; paralleled ×3 to match VLED |
| 5 | PGND | N1 | — | | |
| 6 | PGND | N1 | — | | |
| 7 | VCC_3V3 | N1 | socket → module | pin 9 | logic supply, ≤2 mA standby / ≤25 mA active (§8.3) |
| 8 | **DGND** | N1 | — | **NEW** | logic return, **separate from PGND** — see note |
| 9 | SDA | N2 | bidirectional | pin 10 | I2C data, 400 kHz fast mode |
| 10 | SCL | N2 | socket → module | pin 11 | I2C clock |
| 11 | **SYNC** | N2 | socket → module | **NEW** | broadcast sample/pulse-phase reference — see note |
| 12 | ALERT# | N2 | module → socket | pin 12 | open-drain, wired-OR per bus segment; thermal/fault/OCP assertion so the hub need not poll ~80 modules. **Renamed from Rev 2's `/ALERT`** — §7.4 |
| 13 | **PD1_K** | N3 | module → socket | **RESTORED** | PD1 forward-emission photocurrent, 14–72 µA |
| 14 | **PD2_K** | N3 | module → socket | **RESTORED** | PD2 scalp-facing backscatter photocurrent |
| 15 | **NTC** | N3 | module → socket | **RESTORED** | per-tile thermistor, 42 °C / 62 °C interlock chain (R-9) |
| 16 | AGND | N3 | — | pin 15 | analog/sense return, star-referenced **at the cluster controller**, **not** tied to PGND on the module |
| 17 | **ELEC** | N4 | bidirectional | pin 13 | dual-rated Ag/AgCl electrode — EEG µV signal **and** BES/tACS/tDCS current. Unused on T1-A/T1-C; **present at every socket** because R-2 permits a T1-B in any socket |
| 18 | **ELEC_SHLD** | N4 | socket → module | pin 14 | driven shield / DRL for pin 17, referenced to the EEG DRL output (CLAUDE.md §4.3). **`ELEC_SHLD` adopted over `NP-DRV-SHELL-002`'s `GUARD`** — §7.4 |
| 19 | SEAT# | — | module → socket | pin 16 | tied to PGND on the module through 1 kΩ; detects *partial* seating (§7.3) |

**Notes on the contentious pins:**

- **Pins 13–15 (`PD1_K`, `PD2_K`, `NTC`) are back, and this is the consequence of OI-HUB-C17c, not a reversal of anything this document argued.** **D-4** removed them by moving the TIA and ADC on-module, and §5.3's reasoning for that is unchanged physics that this revision does not withdraw. What changed is the *decision*: `NP-DRV-SHELL-002` Rev 2 §3.3a keeps the analog front end on the cluster controller, on the conservative ground that **a carrier-mounted ADC never faces the 25 → 62 °C drift question that a tile-mounted one must answer against the ±15 % dose claim (FAI-SM-06)**. The cost is exactly what §5.3 warned of and it is now carried openly: **three spring contacts sit in the photocurrent path**, so contact-resistance drift and fretting become dose-metering error terms rather than being designed out. The ≤50 mΩ / ≥500-cycle spec in §7.1 is therefore binding on pins 13–15 as well as on pin 17. **§5.3 and §6 are NOT rewritten by this revision** — see **OI-HEXTILE-15**.
- **Pin 8 (`DGND`) is not redundant with `PGND`.** The obvious argument for merging them — ground bounce is small, ~52 mV at ≤50 mΩ and 1.04 A against a 0.99 V I2C threshold — is true and is not the point. `NP-DRV-SHELL-002` **REQ-EMI-07** forbids the LED return from using *"any structure other than its paired `PGND` conductor"*, because that is what makes the §9.3 broadside supply/return pair cancel. If module logic current also flows in `PGND`, the current in the return is no longer the current in the supply and the cancellation is exact only for the LED term. **One pin keeps a requirement enforceable.**
- **Pin 11 (`SYNC`) closes a gap this document had left unowned.** Rev 2 had no phase-reference contact, but `NP-DRV-SHELL-002` **REQ-EMI-03** requires bus traffic and PBM pulse phase to be deterministic and phase-locked to the EEG/fluxgate sample frame, and **REQ-EMI-04** prohibits the usual EMC escape (dithered PWM) precisely so the artifact stays a subtractable known line. An I2C broadcast carries arbitration and segment-switch jitter and cannot serve that. This is also the pin **OI-HUB-C05** was asking for from the other end — *"T1-C PWM phase sync routing from the on-module ATtiny402 … is not yet defined"*. It now has a home.
- **Pins 17–18 and 16 are the price of R-2.** T1-A and T1-C do not use the electrode pair. They exist at every socket because "any type in any socket" means a T1-B may be inserted anywhere, and an electrode signal cannot be carried over I2C — it is a µV analog recording path to the ADS1299 *and* a stimulation current path from the tES driver. Removing them would silently re-impose the type-restricted placement model that SMART-1 was decided to eliminate.
- **Pin 12 (`ALERT#`) earns its position by arithmetic.** Polling 80 modules over segmented I2C for thermal status at the 10 ms cadence R-9 implies is not achievable; a wired-OR interrupt turns an 80-module poll into an exception path.
- **Pin 19 (SEAT#) is not redundant with the I2C presence poll — and the case for it is now stronger.** NP-HEX-ZM-001 §5.4a correctly notes an unseated tile fails its inventory poll. But a *partially* seated tile can answer I2C on two contacts while the PD, NTC, or electrode contacts are marginal. **With N3 restored that failure mode is live rather than hypothetical:** a tile answering I2C while `PD1_K` sits at elevated contact resistance returns an *under-read* photocurrent, which firmware interprets as low optical output and compensates for by driving harder — a silent dose-integrity failure against a stated competitive claim. SEAT# is positioned at the mechanical extreme of the pad pattern so it is the **last** contact to mate; if it reads low, every other contact is seated.
- **No ZONE_ID pin.** The retired resistor ladder (NP-HW-FPC-001 Rev 5 §3.2) is gone, replaced by UID self-report over I2C (R-12). Its ADC channel, its 1 %-tolerance resistor requirement, its threshold-margin analysis, and its debounce requirement (RISK-18, NP-SW-001 §5.2.1) do not apply to this interface. **NP-SW-001 §5.2.1 and the RISK-18 debounce requirement will need re-scoping** where they bind on module detection — flagged as **OI-HEXTILE-08**; they remain in force for any surviving non-tile accessory detection.
- **No UID EEPROM at any socket.** **D-3** fits a driver MCU to every tile type, so every tile self-reports its UID over I2C. `NP-DRV-SHELL-002` Rev 1's separate 24AA02UID line for T1-A/T1-B is deleted (~$8.50/headset).

### 7.3 Contact sequencing

Pad lengths are staggered so mating order is deterministic. **Rev C adds the four new pins to the sequence rather than leaving them at group 3 by default** — `DGND` moves up because it is a return, and the N3 sense lines stay last-but-one because nothing is harmed by their being late:

1. **PGND, `DGND`** (longest) — **every return established before any supply.** Rev 2 had only `PGND` here because logic shared it; with a separate logic return (pin 8) both must lead
2. **VLED, AGND, ELEC_SHLD**
3. **VCC_3V3, SDA, SCL, `SYNC`, `ALERT#`, ELEC, `PD1_K`, `PD2_K`, `NTC`**
4. **SEAT#** (shortest) — asserts only when the stack is fully home

Break order is the reverse. This prevents the module logic powering up against a floating return, and guarantees SEAT# cannot read seated during a partial insertion.

**Two-row consequence.** With the array in two staggered rows (§7.1), the length stagger must be applied **within each row and consistently across both**, and `SEAT#` must sit at the extreme of whichever row seats last under the worst-case tilt the ±0.5 mm Z tolerance permits. A stagger that is correct row-by-row but inconsistent between rows can let one row make full contact while the other is still partial — which is precisely the state SEAT# exists to detect. Verified by `NP-DRV-SHELL-002` **SH2-DRC-05a** and **SH2-DRC-10b**.

---

### 7.4 Signal naming — one name per conductor (decided 2026-08-11)

**Two conductors carried two names each between `NP-DRV-SHELL-002` and `NP-HW-HEXTILE-001`. Both
are settled here; neither choice has technical content, and both were chosen on a stated hazard
rather than on taste.**

| Conductor | Names in use | **Adopted** | Why |
|---|---|---|---|
| Socket pin 14 / 18 — DRL-driven shield for `ELEC` | `GUARD` (SHELL-002) · `ELEC_SHLD` (HEXTILE, HUB-001 §113) | **`ELEC_SHLD`** | **`GUARD` collides with a different conductor these same documents already name.** §3.5, §4.1 and REQ-EMI-02 all specify a *DRL-driven **guard plane*** on L1's scalp-facing face, under which the N4 shared electrode lanes run. The socket pin is driven **from** that plane but is not it — they are different nets with different extents. Naming the pin `GUARD` makes "the guard" ambiguous in the one document that specifies both. `ELEC_SHLD` names what it shields and pairs typographically with `ELEC` |
| Socket pin 12 / 8 — open-drain module fault | `/ALERT` (HEXTILE) · `ALERT#` (SHELL-002, HUB-001 §7.4/§7.5.1) | **`ALERT#`** | **A leading `/` is the hierarchical-path separator in EDA net naming** (KiCad, Altium): a net literally named `/ALERT` reads as a root-scope hierarchical path and can be silently re-scoped or duplicated on netlist import. That is a mechanism, not a preference. `ALERT#` was also already the majority usage across the document set |

**Why this was worth a decision at all.** A conductor with two names is harmless in prose and is not
harmless in a netlist, a test fixture, or a firmware pin table, where the failure mode is a silent
duplicate net or an unconnected pin — a defect that *reads* as agreement. Both collisions were found
by diffing the two pin tables mechanically rather than by reading them, which is why
**SH2-DRC-05b** / **HT-DRC-23** specify that check as a diff rather than a review.

**A third and fourth name were settled on the same criterion (OI-HEXTILE-17, closed 2026-08-11).**
Both were initially recorded as cosmetic residuals. Checking the codebase showed the first is not
cosmetic at all:

| Conductor | Names in use | **Adopted** | Why |
|---|---|---|---|
| Socket pin 19 — partial-seating detect | `SEAT_N` | **`SEAT#`** | **`_N` already means *cardinality* in this codebase, not active-low.** `firmware/hub_control/tests/np_module_map_tests.c` defines `PBM_TILE_N` and `EEG_TILE_N` as `sizeof(x)/sizeof(x[0])` — so `SEAT_N` reads as *"number of seats"* to anyone who has read the module map. That is a live ambiguity in the same class as the `GUARD` / guard-plane collision above, not a style difference |
| Socket pin 13 / 17 — dual-rated electrode | `ELEC` (HEXTILE §7.2, SHELL-002 §5.1.4) · `ELEC_SIG` (HUB-001 §113/§124) | **`ELEC`** | Two reasons. **Ownership:** the socket interface is defined by the two pin tables; `NP-HW-HUB-001` §7.4 explicitly *adopts* SHELL-002's contract, so it is the consumer — two normative pin tables do not change to match one banner's prose. **Accuracy:** this pin is dual-rated to carry **tES stimulation current** (≤2 mA T1 / ≤4 mA T2), not only a recording signal. `_SIG` biases the reader toward the recording role, which is the half that is *not* safety-relevant. `ELEC` / `ELEC_SHLD` is asymmetric and costs nothing |

> **Rule of record — now programme-wide, in `NP-CONV-001` §1.1: every active-low NeurOne signal name
> terminates with `#`.** Not `_N` (cardinality), not `_L`/`_R` (Left/Right — `NP-FW-CVNS-001` §5.1),
> not `_B` (channel B), not `_LOW` (threshold), and not a leading `/` (EDA path separator).
> `NP-CONV-001` §1.2 records each with the evidence that took it.
>
> Active-**high** signals take no suffix, so the absence of `#` is meaningful. Applying this beyond
> the socket found one further active-low signal: `NP-HW-HUB-001`'s tier-0 service-request line,
> now **`ATTN#`**. Indices use bracket notation — **`SAFE_EN[n]`**, not `SAFE_EN_n`, because a
> lowercase `_n` is indistinguishable from a polarity marker in a plain-text diff
> (`NP-CONV-001` §1.3).
>
> **⚠ The rule exposed a live safety-architecture conflict, which is raised and not resolved:**
> `SAFE_EN[n]` is written here as active-**high** (§6: LOW removes the rail), while the safety MCU
> specifies the opposite for its enable lines (*"LOW = stimulation enabled"*,
> `np_safety_config.h:7-8`). Both are internally fail-safe; together they are inverted, and
> SH2-DRC-13's "defaults LOW at reset" would flip from *safe* to *stimulation enabled* under the
> firmware's convention. **`NP-CONV-001` OI-CONV-01.**

**`#` is not a legal identifier character, so the doc→firmware mapping is stated rather than left to
whoever writes the driver. The full rule now lives in `NP-CONV-001` §2:**

> **`<SIGNAL>#` maps to `<SIGNAL>_ACTIVE_LOW` in firmware identifiers.**

> **⚠ Correction.** An earlier draft of this section specified **`_L`**. That was wrong: **`_L`
> already means *Left*** — `NP-FW-CVNS-001` §5.1 defines `CVNS_ENABLE_L` / `CVNS_ENABLE_R` as the
> left and right electrode drivers. Every other short candidate is taken as well: `_N` is
> cardinality, `_B` is channel B (`CH_B`, `LED_B`, `NP_BANK_B`), `_LOW` is a threshold
> (`NP_TIA_GAIN_LOW`). `NP-CONV-001` §1.2 records all of them with evidence.

**No firmware change is requested here, and none should be made to satisfy this.** The safety MCU's
ten stimulation enable lines are all active-LOW and carry polarity only in header comments
(`np_safety_config.h:7`, `np_gpio_mgr.c:5`); that is IEC 62304 **Class C** code already owned by
`NP-FMEA-001` **OI-FMEA-01**. §2 of `NP-CONV-001` exists so *new* names converge and so OI-FMEA-01
has a convention to adopt. See `NP-CONV-001` §3.

---

## 8. Per-socket power and I2C under SMART-1

### 8.1 VLED rail

**Decision (D-6): VLED = 24 V DC.**

Derived from conductor current, not from convenience. With the driver on-module (D-3), the socket carries DC bus power rather than per-string drive, so the only free variable is rail voltage, and it trades directly against contact current.

| Rail | T1-A peak current per tile (both channels, 150 mA) | Per VLED pin (**×3**, Rev 3) | *Per VLED pin (×4, Rev 2)* |
|---|---|---|---|
| 5 V | 5.0 A | 1.67 A — exceeds the contact rating outright | *1.25 A — exceeds comfortable pogo derating* |
| 12 V | 2.08 A | 0.69 A | *0.52 A* |
| **24 V ★** | **1.04 A** | **0.35 A** | *0.26 A* |

T1-A peak electrical: CH_A 45 × 0.315 W = 14.2 W, CH_B 45 × 0.24 W = 10.8 W → **25.0 W instantaneous**. At 24 V that is 1.04 A, or **0.35 A per contact across three paralleled VLED pins — ~3× derating against the ≥1.0 A contact rating (§7.1)**.

> **The nominal figure is not what set the count.** Three contacts were chosen on the **degraded**
> case, under the rule stated in `NP-DRV-SHELL-002` §5.1.5: *`VLED` is sized so the loss of any one
> contact still leaves ≥2× derating.* At 3 contacts a single-contact loss puts 0.52 A on each
> survivor (~2×); at 2 it puts **1.04 A — exactly the rating, zero margin**, into the fretting →
> resistance → local I²R heating → more fretting runaway that HT-DRC-08 and `NP-DRV-SHELL-002`
> SH2-DRC-09 exist to bound. Rev 2's 4 contacts gave ~3× degraded, which exceeds the rule at the
> cost of two contacts on an interface carrying a live RISK-22 one-handed-force constraint.
> **If the rail, the tile peak power or the contact rating changes, re-derive the count from the
> rule — 3 is a result, not a constant.**

The 5 V row is now excluded twice over: it exceeded comfortable derating at 4 contacts and exceeds the **rating itself** at 3.

24 V also suits series-string construction: at 11 series 660 nm emitters (11 × 2.10 = 23.1 V) or 14 series 808 nm (14 × 1.60 = 22.4 V), the residual dropped across the FET and sense resistor is ≤1.6 V, so linear-loss overhead stays under 7 %. A 12 V rail would halve the string length and double the number of parallel strings and sense resistors on a tile that has no room for them.

#### 8.1.1 The forward-voltage budget stated as a rule (Rev 4)

The two worked strings above are instances, not the constraint. Stated generally, so that a candidate
emitter can be tested against it without re-deriving it — and because the instances were being quoted
elsewhere *as* a Vf constraint on the architecture, which they are not (see
`docs/status/pending-decisions.md` §13.2e):

> **N · V_f ≥ 22.4 V** — the residual is a *thermal budget allocation*. Violating it produces heat in
> the linear stage, not a failure; the dissipation lands on the tile and is therefore a term in §9.3.
>
> **N · V_f ≤ 24 V − V_dropout(min)** — a *hard functional ceiling*. Below the stage's dropout the
> constant-current control falls out of regulation and the string is uncontrolled.

**The two bounds are different kinds of constraint and must not be quoted as one range.** Only the
upper is functional.

| V_f | Best N | N · V_f | Residual | Overhead | Meets the rule |
|---|---|---|---|---|---|
| 1.60 V (CH_B design target) | 14 | 22.40 V | 1.60 V | 6.7 % | ✓ at the ceiling |
| 2.10 V (CH_A design target) | 11 | 23.10 V | 0.90 V | 3.8 % | ✓ |
| 2.80 V | 8 | 22.40 V | 1.60 V | 6.7 % | ✓ |
| 3.00 < V_f < 3.20 V | — | — | — | — | ✗ **no integer N** (dead band; widens with `V_dropout`) |
| 3.30 V | 7 | 23.10 V | 0.90 V | 3.8 % | ✓ |

**Consequence for emitter selection (OI-HEXTILE-02).** A ~3 V AlGaAs NIR emitter is **not**
categorically incompatible with this rail — at 7–8 series it lands on residuals this section already
accepts. What it costs is *per-tile*: 7–8 emitters per string against CH_B's specified 14 roughly
doubles the parallel strings and sense resistors, which is the same objection raised against a 12 V
rail one paragraph above, on the same tile that has no room for them. That is a rigidizer-layout and
current-matching question, **not a rail or Hub PCB question.** There is also a dead band at
**3.00 < V_f < 3.20 V** where no integer N satisfies both bounds — N = 7 falls under the 22.4 V floor,
N = 8 exceeds the rail. It is ≥0.20 V wide and **widens as `V_dropout` grows** (OI-HEXTILE-18), since
the N = 8 ceiling is `(24 − V_dropout)/8`, not 3.00 V.

**Two caveats that bound every number in this section:**

1. **Fixed-N construction depends on Vf binning, and that dependency has not been stated before.**
   A commodity emitter's datasheet typ→max Vf spread is routinely ≥0.7 V per die; across 7–14 in
   series that is several times the entire 1.6 V residual budget, and no fixed N survives it. What
   makes fixed-N viable is `NP-PROC-FPC-001` §2.1's mandatory **±0.10 V within-order Vf bin**, which
   that document currently justifies only on RISK-08 current-hogging grounds. **It is load-bearing for
   string construction too.**
2. **The 7 % figure is asserted here, not derived.** It is not traceable to a tile power or
   temperature limit anywhere in this document or in `NP-HW-HUB-001` / `NP-DRV-SHELL-002`. Treat it as
   a chosen allocation open to re-derivation from §9.3, not as a bound with a physical basis —
   **OI-HEXTILE-18**.

**Consequence:** emitter count per channel must be an integer multiple of string length, which the 45/45 and 30/30/30 allocations of §4.2 do not exactly satisfy for every candidate V_f. The allocation carries **±2 sites of slack**, to be closed when emitters are selected (OI-HEXTILE-02). Worked example at the assumed V_f: CH_A 44 = 4 strings × 11; CH_B 42 = 3 × 14. The lattice geometry is unaffected — unallocated sites are simply left unpopulated.

### 8.2 I2C fan-out — the architecture SMART-1 requires

NP-HEX-ZM-001 §4a states the retired one-PCA9546A-plus-one-GPIO-mux design *"does not scale to ~30–80 sockets without a materially different I2C fan-out architecture (cascaded/multi-stage muxing)."* This is that architecture.

The retired design needed a mux per slot for one reason: every smart module shared the factory-burned address 0x30, so two modules on one bus collide. **The collision is what should be removed, not worked around** — muxing 80 sockets to preserve a fixed address is solving the wrong problem.

**Decision (D-7): dynamic address assignment from the module UID, over per-cluster bus segments.**

- **Segmentation follows the mechanical clusters.** NP-HEX-ZM-001 §5.4a partitions the lattice into clusters under **CLUSTER-1** (7-hex "flower", partial flower where the lattice edge cannot host a full one) and **SYM-1** (the partition is mirror-symmetric about the sagittal midline). Reusing that partition electrically means one bus segment = one cluster = one VLED switching domain (§8.4) — one physical grouping serving mechanics, power, safety, and addressing rather than four incompatible partitions.
- **The cluster count is 18 at the v1 80-socket lattice — derived, not carried over.** See §8.2.1. Earlier revisions of this section stated "4–10 clusters", which was NP-HEX-ZM-001 §5.4a's figure for the **retired 30-socket** lattice and does not survive rescaling; 4 clusters at 80 sockets would mean 20 tiles per cluster, which exceeds this section's own 10–19-modules-per-segment capacitance limit and is impossible under CLUSTER-1's 7-tile ceiling.
- **The i.MX RT1062 provides LPI2C1–LPI2C4.** One PCA9548A-class 8-channel switch per bus gives up to 32 segments; **18 clusters uses 56 % of that**, leaving 14 segments of headroom for a REG-1 lattice re-cut. Note this is *one tier* of muxing, not the cascade the parent document anticipated — because addresses no longer collide, muxing is only needed for bus capacitance, not arbitration. **The muxing architecture is unaffected by the count correction** (32 ≥ 18 with margin), but the two-level `8 branches × ≤2 clusters = 16` topology of NP-DRV-SHELL-002 §3.4 **is** — 18 > 16 (OI-HEXTILE-14; options at §8.2.2).
- **Address assignment uses the UID that already exists.** NP-HEX-ZM-001 §4 specifies power-on UID polling with re-inventory only on UID change. An SMBus-ARP-style assignment (address resolution from a unique device identifier) maps onto that directly: the hub enumerates a segment, assigns each module an address from its UID, and the resulting map feeds `np_module_map` unchanged. **No new identity concept is introduced** — the UID the addressing layer already depends on becomes the addressing key.
- **Capacitance:** at 400 kHz fast mode the 400 pF bus limit permits roughly 10–19 modules per segment with disciplined routing. Realised cluster sizes under CLUSTER-1 + SYM-1 are **3–6 tiles** (§8.2.1), comfortably inside that, with ≥1.7× margin even against a full 7-tile flower. Segment routing length is the real constraint and is a Hub PCB Rev C layout item. Note the earlier "3–7-tile cluster sizes" phrasing bracketed the **3-hex triad**, which CLUSTER-1 excludes; the surviving range is the partial flower (3–6) up to the full flower (7).
- **Pull-ups:** one 4.7 kΩ pair per segment (not per socket). At **18 segments, 36 resistors** — versus the 160 that a per-socket scheme would need.

### 8.2.1 Where 18 comes from

The count is fixed by the lattice, not chosen. Inputs: the v1 socket lattice `ROW_WIDTHS = [3,6,7,8,9,8,9,8,7,6,5,4]` (80 sockets, 12 coronal rows, `scripts/sync-socket-map.ts`), **CLUSTER-1** (flower or partial flower only), and **SYM-1** (mirror-symmetric partition).

1. **The six midline clusters are forced.** A cluster containing a midline socket must equal its own mirror image, so its centre must be self-mirror — i.e. *on* the midline. Only odd-width rows carry a midline socket (r0, r2, r4, r6, r8, r10 → sockets **{2, 13, 29, 46, 62, 74}**, NP-HEX-ZM-001 §3.2), and a flower spans only rows *r*−1…*r*+1, so each midline cluster holds **exactly one** midline socket. Hence exactly **6** midline-centred, self-symmetric clusters.
2. **They absorb exactly 30 sockets** — 6 centres + 12 in-row petals (x = ±1) + 10 contested petals at x = ±0.5 on r1…r9 + 2 on r11.
3. **The residual is 50 sockets: two mirror-image lateral bands of 25.** Exhaustive branch-and-bound over all flower/partial-flower covers of one band gives a minimum of **6** clusters per band (the naïve ceil(25/7)=4 is unreachable — the residual bands are only 2–3 sockets wide, so most flowers cannot fill).

**Total: 6 + 2 × 6 = 18 clusters, and this is provably minimal**, not a greedy result. Cluster sizes are 3–6 tiles. Diagram: `docs/diagrams/np_hextile_cluster_map.svg`.

**Contiguity is a binding shape rule, not an aesthetic one (CONTIG-1).** Minimising cluster *count* does not by itself constrain cluster *shape*: a partition can satisfy CLUSTER-1 and SYM-1 while still placing a petal whose only in-cluster contact is the centre — a **pendant petal**, with a foreign socket on both flanks. The first 18-cluster partition generated for this revision contained four (sockets 27, 31, 77, 80). That shape is mechanically inadmissible, because the cluster's structural member is the **clamp plate**, not the tile group (tiles are independent modules in independent sockets), and the plate cannot bridge the gap — the gap socket belongs to a different cluster whose plate actuates independently. The plate must therefore reach a pendant petal on a **cantilever arm**:

| Arm geometry | Value | Source |
|---|---|---|
| Length, centre plunger → pendant plunger | 40.0 mm | tile pitch, §4.1 |
| Maximum neck width crossing one tile boundary | **23.09 mm** (one hex edge, W/√3), less clearance for the two flanking plates | §3 |
| Dome the arm must follow over that span | 2.33 mm sagitta at R_m = 87 mm | R-1 |

**Rule: a cluster's petals must form a contiguous arc around its centre**, measured over ring positions that exist in the lattice. Dropping *outer* petals — CLUSTER-1's own wording — yields this automatically; only a count-minimising search violates it. A single missing petal in an otherwise complete ring (a horseshoe plate, e.g. C2/C4) is admissible: the petals still chain, so there is no cantilever. What is excluded is an **isolated** petal.

The four pendant petals were resolved at **zero cost in cluster count** by reassigning each to an adjacent cluster that already touches it — 73→C16, 75→C18, 27→C8, 31→C9 (principal direction, 2026-08-04). The partition remains 18 clusters, mirror-symmetric, max size 6, with **zero pendant petals and zero broken arcs**.

**The symmetry constraint costs clusters.** Without SYM-1 the minimum is **12** (ceil(80/7), the figure NP-HW-HUB-001 Rev 3 §4.4 and NP-DRV-SHELL-002 §7.1 both carry). SYM-1 raises it to 18 — a 50 % increase — because the midline forces a column of six clusters that are mostly partial flowers, and the residual lateral bands are too narrow to pack efficiently. **Every peer document currently sizes hardware off 12 or off `ceil(n/8)` = 10; all three counts are now in play and only 18 satisfies the standing decisions** (OI-HEXTILE-14).

### 8.2.2 Interconnect capacity at 18 clusters — options for OI-HEXTILE-14

**Status: PROPOSED, not decided.** Recorded so Hub PCB Rev C and NP-DRV-SHELL-002 can be coordinated before either is released.

**Two independent "16"s bind, and they belong to different documents.** They must not be conflated:

| # | Constraint | Source | Binds at |
|---|---|---|---|
| **C1** | Cluster-tail **connector positions** on the Hub PCB, 12 pins each | NP-DRV-SHELL-002 §7.1 | 16 (12 populated) |
| **C2** | I2C **tree capacity**, `8 branches × ≤2 clusters` | NP-DRV-SHELL-002 §3.4 | 16 |

**C2 does not exist under this document's own D-7.** D-7 is 4 × LPI2C, each with one 8-channel PCA9548A = **32 segments**, of which 18 uses 56 %. The two documents describe different trees, and that disagreement is already open as **OI-HUB-C17** — where NP-HW-HUB-001 §7's own comparison recommends D-4/D-7 prevailing. So C2 may resolve itself; **C1 will not**, and is the one that must be fixed before Rev 3 layout.

| # | Option | Effect | Assessment |
|---|---|---|---|
| **1** | **Provision 18 → 20 connector positions** | +24…+96 conductors through the §5.3c posterior boss (216–240 vs 192) | **Recommended.** The cheap axis: SHELL-002 §7.3 notes adding a *conductor* costs 16 hub pins, so widening the tail is expensive while adding tails is not. Still far below the ~880 that Rev 2's star implied. 20 rather than 18 absorbs a REG-1 re-cut without a second re-spin |
| **2** | Raise branch fan-out `8 × 2` → `8 × 3` = 24 | Fixes C2 with **no new silicon** — the tier-1 bus already addresses 32 controllers on a 5-bit strap | Only needed if SHELL-002's cluster-MCU tree survives OI-HUB-C17. Capacitance is not the obstacle either way: with a cluster MCU the branch sees 3 controllers; under D-7 the segment sees ≤6 modules, both inside the 10–19 limit (§8.2) |
| **3** | **Adopt D-7's topology wholesale for Rev 3** | C2 disappears; 18 of 32 segments, 14 spare | **Recommended.** Costs nothing new — already the standing requirement in OI-HEXTILE-10. Needs OI-HUB-C17 decided |
| **4** | **Multi-drop trunk instead of per-cluster star** | Connector count becomes largely **insensitive** to cluster count | **Recommended if §8.4.1 is accepted** — see the interaction below |
| 5 | Decouple electrical from mechanical clusters (populate fewer sockets, OI-HEXTILE-06) | Electrical clusters < 18 while mechanical stays 18; the capacity-8 board SKU already tolerates it | Legitimate but trades a stated principle — D-7's "one physical grouping serving mechanics, power, safety and addressing" — for connector count. Not the lead option |
| 6 | Re-cut the lattice to ≤16 clusters | — | **Not available without breaking a principal decision.** The 6 midline clusters are forced by SYM-1 given six odd-width rows, and 18 is provably minimal at n = 80 (§8.2.1); reaching 16 needs ~10 fewer sockets, which REG-1 registration is unlikely to permit |
| 7 | Pair clusters onto shared tails | 18 clusters over ≤16 tails | Breaks "board + clamp + sockets as a single FRU" (NP-HW-HUB-001 §4.4) and forces an asymmetric pairing under a symmetric partition. Rejected |

**Interaction with §8.4.1 — the reason option 4 is strategic rather than cosmetic.** N1 (power) is already a broadside tree and N2 (control) is already a two-level tree; **`SAFE_EN[n]` (N5) is the only star component of the tail** (NP-DRV-SHELL-002 §4 network table), and it is therefore the structural reason each cluster must terminate at the hub individually. If §8.4.1's single Class C `NP_SAFETY_EN_PBM_CRANIAL` is accepted, that line becomes a **broadcast**: the tail drops 12 → 11 conductors and clusters can tap a trunk instead of each running a dedicated star leg. The per-cluster high-side gate already sits on the cluster carrier (SHELL-002 §5.1 BOM), so local gating is unaffected. **This is what makes the interconnect robust to a future REG-1 re-cut** rather than merely sufficient at 18.

**Recommendation: options 1 + 3, with 4 if §8.4.1 survives safety review.** Adopt D-7's 32-segment tree, provision **20** connector positions, and — if the single cranial enable is accepted — remove `SAFE_EN[n]` from the tail and let a trunk absorb future count changes. **The failure mode to avoid is cutting Rev 3 against 16 and discovering the shortfall in layout**, which is precisely what OI-HEXTILE-14's "coordinate before either is released" exists to prevent.

### 8.3 3.3 V logic budget

The retired per-module figure was ≤50 mA. **At 80 modules that is 4.0 A / 13.2 W of pure logic overhead — roughly a third of the entire T1 peak envelope (R-10), spent before a single photon is emitted.** The retired number was sized for 5 modules and does not survive multiplication.

**Requirement (binding on OI-HEXTILE-07):**

| State | Per module | ×80 |
|---|---|---|
| Standby (MCU in power-down, TWI address-match wake armed) | ≤2 mA | 160 mA / 0.53 W |
| Active (PWM running, ADC sampling) | ≤25 mA | ~6 concurrent (§9) → 150 mA / 0.50 W |
| **Total** | | **~310 mA / ~1.0 W** |

This is achievable — tinyAVR 2-series parts wake from standby on TWI address match — but it is a **hard firmware requirement**, not an incidental property. A module that idles at the retired 50 mA makes the whole-vault lattice infeasible on the existing power envelope. Stated here because it is a hardware-driven constraint that only the module firmware can satisfy.

### 8.4 Safety gating

R-11 requires the safety MCU to physically own the enable path. The STM32G071 cannot present 80 GPIOs, so per-socket gating is not available at the safety layer.

**Decision (D-8): the safety MCU gates VLED per cluster** — **18 high-side switches** on the cluster segments defined in §8.2, replacing the retired `NP_SAFETY_EN_PBM_ZONE_0..4` five-zone scheme. Cutting VLED removes emitter drive regardless of on-module state, so a wedged module MCU cannot sustain output. Per-socket granularity is provided by the on-module driver (fine, fast, not safety-rated); the cluster gate is the coarse, hardware, safety-rated backstop.

This is proposed as the hardware half of the resolution to **OI-HUB-SOCKET-01**.

**The GPIO argument this decision rests on does not close at 18, and is now an open item.** D-8's premise is that "the STM32G071 cannot present 80 GPIOs", which is true and unaffected. But the inference that the cluster count is therefore comfortable was written against 4–10. At 18 it needs re-checking, and re-checking surfaces three problems:

| # | Finding | Evidence |
|---|---|---|
| 1 | **The safety MCU package is not specified anywhere in the document tree.** STM32G071 spans UFQFPN28 (~22 I/O) to LQFP64 (~52 I/O). The only package-qualified STM32G071 in the tree is NP-HW-HUB-001 §8.3's **UFQFPN32 cluster MCU** — a different part. | `docs/np_hw_hub_001.md:1173`; no package in `firmware/safety_mcu/` |
| 2 | **Demand at 18 enables is ~38–40 I/O.** SPI1 slave 4 (NSS is load-bearing — frames are delineated by NSS transfer length) + R-peak capture 1 + nine non-cranial modality enables + 6 NTC ADC channels + fault-indicator LED (FMEA-M08-04 requires it on a *different port* from the enables) + SWD 2 ≈ 22, **plus 18** = ~40. That excludes every ≤32-pin package and leaves LQFP48 with almost no margin. | `firmware/safety_mcu/include/np_safety_config.h`; `docs/np_fmea_001.md` FMEA-M08-04 |
| 3 | **The open question is per-cluster *policy*, not a conflict between the peer documents.** An earlier draft of this section called NP-HW-HUB-001 §7.2 and NP-DRV-SHELL-002 §6 *incompatible*; **that was wrong and is withdrawn.** NP-HW-HUB-001 **§7.4 already reconciles them**: *"These are compatible and were reached from different ends: 12–16 physical enable **lines** fanned out from **one** policy **bit**."* Physical per-cluster gates and a single policy bit are the same design. What is genuinely undecided is whether **per-cluster policy** — independently commanded cluster bits — is wanted, which §7.4 routes to **OI-HUB-C07**. | `docs/np_hw_hub_001.md` §7.2, **§7.4:928**; `docs/np_drv_shell_002.md:190,378,432,482` |
| 4 | **Per-cluster policy does not fit the current Class C enable word, but the word can now be widened cheaply.** The enable mask is `uint16_t` (`NP_SAFETY_EN_ALL_MASK = 0x3FFF`, 14 bits used, 2 spare). 18 cluster bits + 9 surviving modality bits = **27 > 16**; §7.4 found the same at 16 clusters (25 bits). Collapsing the five zone bits to one cranial bit yields 10 used / **6 spare** — still short, so recycling bits alone is insufficient. **However (principal, 2026-08-04): no SHDR fault records have been generated yet**, so §7.2's "bits 1–4 reserved, not reused" rule does not bind and widening to `uint32_t` is a pre-production change with no migration and no ambiguous historical logs. **The wire format is therefore a cost, not a ceiling.** | `firmware/safety_mcu/include/np_safety_protocol.h:46–60, 90–118`; `docs/np_hw_hub_001.md` §7.4 |

**The open question is finding 3: it is not decided whether the safety layer can cut one cluster or only the whole cranial lattice.** Raised as **OI-HEXTILE-13**, and the same question NP-HW-HUB-001 §7.4 routes to **OI-HUB-C07**. Until it closes, D-8's **switch** count is 18 while its **policy-bit** count is either 18 or 1. Note findings 1, 2 and 4 are all *costs* of per-cluster policy rather than blockers — none of them decides the question. The argument that does is in §8.4.1.

> **Note (firmware, no change requested) — the PA4 collision sits inside a macro set that is already slated for deletion.**
>
> `np_safety_config.h:24` declares **PA4** as SPI1 NSS (load-bearing — `np_safety_main.c` distinguishes frame types by NSS-delineated transfer length), while `:45` assigns `NP_EN_PBM_ZONE4_PIN = (1U << 4)` on GPIOA. Same pin, two owners. (`NP_EN_INTRANASAL_PIN` is also `1U << 4` but on GPIOB — not a collision.) The header calls bank assignments *"provisional pending PCB layout (G1 gate)"*, so this is a provisional-allocation artifact, not a live defect.
>
> **It should not be tracked as a standalone pin conflict.** `NP_SAFETY_EN_PBM_ZONE_0..4` / `NP_EN_PBM_ZONE0..4` encode the **retired 5-module-slot** meaning of "zone". Under the current architecture a zone is *"a named SET OF MODULES, defined as a list of socket addresses"*, authored in `protocols/predefined/00-zones.npps`, with **no fixed count and user-extensible** (CLAUDE.md §3). Crucially, **zones overlap** — that file's §"inclusive membership" rule puts every midline socket in BOTH the Left and the Right zone of its lobe, and requires firmware to dedup. **An overlapping, user-definable set can never be a hardware enable domain**: "cut zone *X*" is undefined when a socket belongs to two zones. Clusters *partition* the lattice; zones do not. This is why D-8 gates per **cluster**, and it makes the zone-enable macros structurally dead rather than merely miscounted.
>
> NP-HW-HUB-001 §7.2 already mandates their removal (five zone bits → one `NP_SAFETY_EN_PBM_CRANIAL`), which deletes PA4's second owner as a side effect. **The correct home is therefore that cleanup, not this open item** — cross-referenced from **NP-FMEA-001 FMEA-M08-04**, which already covers "stimulation enable GPIO shares a pin with another function" (S5 → ALARP, control = *"GPIO assignment verified against schematic in hardware design review"* — a control that has not yet executed, which is why the collision is still present).
>
> **Two siblings carry the same retired concept and should be cleaned up together:** `NP_NTC_CHANNEL_COUNT 6 /* 5 zones + 1 hub */` in `np_safety_config.h`, and NP-FMEA-001 FMEA-M04's *"reads the NTC thermistor ADC channel for each PBM zone (5 zones)"*.
>
> **No firmware change is made by this revision** — the deletion belongs to NP-HW-HUB-001 §7.2 and is gated on OI-HUB-C07 / OI-HUB-C17.

### 8.4.1 Proposed resolution to OI-HEXTILE-13 — keep per-cluster policy out of Class C

**Status: PROPOSED, not decided. Safety review (OI-HUB-C07) arbitrates.**

**This confirms NP-HW-HUB-001 §7.4 rather than proposing something new.** §7.4 already states the synthesis — *"12–16 physical enable lines fanned out from one policy bit"* — and already identifies the residual question as whether per-cluster *policy* is wanted. What this section adds is the arithmetic at **18** clusters and the argument that decides it.

**The trade is not safety-vs-safety.** Both peer documents concede that coarser cutting is never less safe: NP-HW-HUB-001 §7.2 ("over-cutting is a usability cost, never a hazard") and NP-DRV-SHELL-002 §6 ("an **availability** regression, not a safety one, and it is the conservative direction"). Per-cluster policy therefore buys **availability only**.

**Two arguments previously offered here are withdrawn.** They were costs, not blockers, and resting the case on them was wrong:

| Withdrawn argument | Why it does not decide the question |
|---|---|
| *"The peer documents specify incompatible architectures."* | They do not — NP-HW-HUB-001 §7.4 reconciles them explicitly. Per-cluster physical gates and a single policy bit are the same design, and the gates exist in **both** columns |
| *"27 enable bits do not fit the 16-bit Class C wire format."* | True today, but **no SHDR fault records have been generated yet** (principal, 2026-08-04), so §7.2's "bits 1–4 reserved, not reused" rule does not bind and the word can be widened to `uint32_t` as a pre-production change — no migration, no ambiguous historical logs. A cost, not a ceiling. See §8.4.2 |

**The argument that survives, and it is sufficient on its own:**

> **Per-cluster policy puts a *topological* map behind a Class C certification boundary, to buy an availability benefit that Class B can deliver instead.**

Eighteen independently-commanded cluster bits require the safety MCU to hold a socket→cluster mapping. That mapping is not identity — it changes whenever **MECH-2** revisits the clamp shape or **REG-1** re-cuts the lattice. NP-HW-HUB-001 §4.5.1 rejects encoding cluster identity in the logical address for exactly this reason. Behind a Class C boundary the consequence is worse than an awkward table: **a re-clustering becomes a Class C recertification** rather than a regenerated map. It also creates a failure mode that cannot otherwise exist — cutting the *wrong* cluster from a stale map, leaving the faulted one energised.

**Proposal — extend D-8's own two-level logic by one tier, and make only the top tier safety-rated:**

| Tier | Granularity | IEC 62304 class | Mechanism |
|---|---|---|---|
| Fine | per socket (~80) | B | on-module driver register (already D-8) |
| Coarse | **per cluster (18)** | **B** | per-cluster gate enables, for *availability* management |
| Hard | **whole cranial lattice** | **C** | single `NP_SAFETY_EN_PBM_CRANIAL`, in series with everything above |

This keeps NP-DRV-SHELL-002's per-cluster hardware gates and its availability benefit, keeps §7.2's single Class C bit and its 1-GPIO cost, and keeps the topological map outside the certified boundary. **R-11 is preserved** — the Class C bit is in series, so the safety MCU still physically owns the enable path and no Class B fault can re-energise a cut lattice. It also drops safety-MCU demand from ~40 to ~23 I/O, which closes on a mid-range package with margin.

**What would falsify this proposal.** It fails if safety review identifies a hazard where continuing to stimulate on the *other* clusters is safe, continuing on the faulted cluster is not, **and** a whole-lattice cut is itself unacceptable. Since a whole-lattice cut is always available and always safe, that requires the cut to be harmful in its own right — which nothing in the tree claims for PBM. (The cervical-VNS cardiac interlock has its own dedicated <100 ms enable path and is unaffected either way.) **Safety review should either produce such a hazard or close the item.**

### 8.4.2 The Class C enable word — what can and cannot be re-laid out

**Standing principal instruction (2026-08-04): no SHDR fault records have been generated, and none are to be assumed until the principal states otherwise.** NP-HW-HUB-001 §7.2 requires enable-word bits 1–4 to be *"reserved, not reused"* on the grounds that *"enable-bit positions appear in SHDR fault records; silently recycling a position would make historical logs misread."* With no records in existence, **that stated rationale does not currently bind.**

> **⚠ But the reservation rule survives the rationale it was given, and §7.2 does not say so.** Enable-bit positions have a **second consumer inside the firmware**: they are identical to charge-monitor channel indices. `NP_SAFETY_MAX_CHANNELS` (14) is specified as *"must match the `s_charge_nc[]` array size in `np_charge_monitor.c` **and the number of `NP_SAFETY_EN_* ` bits**"*; `NP_SAFETY_CH_CLIN_STIM` is defined as *"charge-monitor channel INDEX for CLIN_STIM (**= bit position of the enable bit**)"*; and the test suite shifts the index straight into the mask (`granted_mask & (1U << NP_SAFETY_CH_CLIN_STIM_IDX)`). **Enable-bit position ≡ `current_ua[]` slot ≡ charge accumulator index — a three-way identity, and it is Class C** (the 40 µC/cm² charge-density limit rests on it). A reader combining §7.2 with the no-SHDR-records finding would reasonably conclude that recycling bits 1–4 is now safe. **It is not.** NP-HW-HUB-001 should record this second rationale — raised as part of **OI-HEXTILE-14**.

**Frame capacity: there is none.** The active heartbeat `np_safety_rx_ext_frame_t` (`firmware/common/include/np_spi_wire_types.h`) allocates all 38 bytes:

| Offset | Field | Bytes |
|---|---|---|
| 0 | `magic[2]` | 2 |
| 2 | `session_status` | 1 |
| 3–4 | `enable_lo` / `enable_hi` | 2 |
| 5 | `channel_count` | 1 |
| 6 | `checksum` (over bytes 0–5) | 2 |
| 8 | `current_ua[NP_SAFETY_MAX_CHANNELS]` | 28 |
| 36 | `ext_checksum` (over bytes 8–35) | 2 |
| | **total** | **38 — no reserved field** |

*(The single reserved byte in `np_safety_rx_frame_t` is not available: that is the legacy 8-byte base retained only for the test suite and to document layout origin. The active heartbeat is the extended frame.)*

**Three changes, three different costs:**

| Change | Bit budget | Cost | Verdict |
|---|---|---|---|
| **Collapse `ZONE_0..4` → one `CRANIAL` bit, leaving bits 1–4 as holes** | 10 of 16 used | **Free.** Nothing above bit 4 moves; `current_ua[]` semantics, `NP_SAFETY_MAX_CHANNELS` and `s_charge_nc[]` all untouched | **Take it.** Worth doing regardless of how OI-HUB-C07 resolves — and it does not depend on the no-SHDR-records finding at all |
| **Compact the word — actually recycle bits 1–4** | 10 of 16, contiguous | **Not free.** Every modality bit shifts down 4, so `NP_SAFETY_CH_CLIN_STIM` 13 → 9 and every `current_ua[i]` slot changes meaning. Touches `s_charge_nc[]`, the hub-side packing in `np_hub_config.h`, and the charge-monitor tests | Only if a positive reason appears. Cosmetic tidiness is not one |
| **Widen for per-cluster policy (18 cluster + 9 modality = 27 bits)** | 27 — does not fit 16 | **Substantial.** `uint16_t` → `uint32_t`; frame must **grow** (zero spare bytes), moving `NP_SAFETY_RX_EXT_FRAME_LEN`, both checksum spans, the compile-time size assertion and eight `offsetof` assertions. If the bit ≡ channel identity is preserved, 27 bits implies `current_ua[27]` = 54 bytes, taking the 200 ms heartbeat from 38 to ~64 bytes | A real cost, on top of §8.4.1's Class C map objection |

**One escape, if per-cluster policy is ever adopted:** PBM clusters plausibly need no charge monitoring at all — charge density is a tES concept (BES/tDCS/CVNS), and `NP_SAFETY_CH_CLIN_STIM` is the only channel index the tree names explicitly. Breaking the bit ≡ channel identity would decouple enable width from `current_ua[]` width and remove most of the frame-growth cost. **That must be a deliberate Class C decision, not a side effect of a re-layout.**

**Net:** the enable word is **a cost, not a ceiling** — but the cost is larger than the GPIO count and larger than an earlier draft of this section stated. The only genuinely free move is the §7.2 collapse with bits 1–4 left as holes.

---

## 9. The concurrency ceiling — the finding that constrains protocol design

This section exists because the arithmetic falls out of §8.1 and materially changes what the hex lattice can be expected to do. It is stated here rather than left implicit.

### 9.1 Available optical power

| Quantity | Value | Source |
|---|---|---|
| T1 peak headset draw | 45–50 W | R-10 |
| Non-PBM overhead (processors, EEG, radios, module logic, fan) | ~6–8 W | CLAUDE.md §4.5 standby + §8.3 |
| **Available to PBM emitters** | **~38–42 W** | |
| T1-A tile at full drive, both channels | 25.0 W | §8.1 |
| T1-A tile at CW 200 mW/cm², both channels | 12.5 W | half current |

### 9.2 Concurrent-tile ceiling

| Mode | Per tile | Concurrent tiles within ~40 W |
|---|---|---|
| Dual-channel, 400 mW/cm² peak, **25 % duty** (R-4) | 6.25 W avg | **~6** |
| Dual-channel, 200 mW/cm² **CW** | 12.5 W | **~3** |
| Single-channel, 200 mW/cm² CW | ~6.3 W | ~6 |
| T1-C, three channels, 25 % duty | ~5.9 W avg | ~6 |

**Roughly six tiles can be active at once. Not eighty.**

### 9.3 What this means

The hex redesign does **not** increase deliverable dose, and was never going to — dose is bounded by the USB-C PD envelope and by the 42 °C scalp limit (R-9), neither of which the lattice changes. What it buys is **placement freedom**: any six-tile subset of ~80 positions, protocol-defined, rather than five fixed slots. That is a large gain for the research mission SMART-1 was decided to serve (NP-HEX-ZM-001 §4a: arbitrary montage/protocol design), and it is a different gain from the one "whole-vault active tiling" suggests.

Three consequences follow:

1. **A global concurrent-power governor is required in firmware.** Today nothing prevents a protocol from naming 40 sockets in a `NP_PROTO_TARGET_SOCKET_MASK` bitmap (NP-HEX-ZM-001 §4b) and commanding them all on. That protocol would brown out the rail or trip PD negotiation. The compiler and the session runner both need a power-budget check against the negotiated USB-C PD contract. **OI-HEXTILE-09** — this is a genuine safety-adjacent gap in the delivered v2 wire format, not a future nicety.
2. **NP-HEX-ZM-001 §6's aggregate thermal concern is bounded by the same arithmetic.** That document warns that "whole-vault active tiling raises aggregate scalp thermal load." It cannot, in the sense feared: the power envelope permits ~6 tiles ≈ 64 cm² of active area, which is comparable to the retired 5-slot design's footprint. The thermal risk is *local* (one tile at 42 °C) and is already owned by the per-tile NTC and Path B1 (NP-THERM-CFD-R1-001), not aggregate.
3. **§6.4's cost problem has a natural answer.** If only ~6 tiles can ever be live, populating all 80 with $11.50 of driver and metering hardware buys placement options, not capability. This is the strongest argument for option 1 in §6.4 — a partially-populated lattice — and the two open items should be decided together.

---

## 10. Decisions

Recorded so they can be challenged individually. None is locked; all are proposals for design review.

| ID | Decision | Rationale | Reversible? |
|---|---|---|---|
| **D-1** | 91-site 5-ring centered-hexagonal lattice at 3.80 mm pitch, site 0 reserved, identical on every tile type | Hexagonal array registers to a hexagonal tile with no corner waste; 3-colourable for the 3-wavelength case; independently reproduces NP-HEX-ZM-001 §3.1's ~90-element estimate; makes T1-B a masking derivation (§4.5) | Yes — FPC artwork only, pre-tooling |
| **D-2** | PD1 at the reserved centre site; PD2 co-located in XY on the scalp-facing layer | Type-independent position; maximum optical symmetry; centre-vs-average offset absorbed by existing factory K coefficients; co-location required for the PD1/PD2 fouling-vs-ageing ratio to be valid | Yes |
| **D-3** | On-module driver on **every** tile type, not only T1-C | 240 hub-side driver channels and ~1,280 power conductors are not buildable, and would put 80 dI/dt sources inside the Faraday envelope beside the EEG harness | **No** — sets the socket pinout and the Hub PCB Rev C architecture |
| ~~**D-4**~~ | ~~TIA + ADC on-module; no PD analog signal crosses the socket~~ **⚠ NOT ADOPTED — OI-HUB-C17c resolved against D-4, principal direction 2026-08-11.** The AFE stays on the cluster controller; **N3 survives** and `PD1_K`/`PD2_K`/`NTC` are back at the socket (§7.2, pins 13–15) | The rationale is **not withdrawn** — deleting the ~80× DG2788A NRE, design-time-fixed gain, and removing the longest high-impedance path were all real, and §5.3's physics is unchanged. It was **outweighed**, on the conservative ground that a controller-mounted ADC never faces the 25 → 62 °C drift question a tile-mounted one must answer against the ±15 % dose claim (FAI-SM-06). See `NP-DRV-SHELL-002` §3.3a for the full trade | **Superseded, not reversible-by-this-document** — §5.3 and §6 still read as if D-4 holds; **OI-HEXTILE-15** |
| **D-5** *(Rev C)* | **19-position** pogo interface, 2.00 mm pitch, **two staggered rows**, springs on socket. `VLED`/`PGND` = **3 + 3** | A per-tile ZIF lever contradicts the NP-HEX-ZM-001 §5.4a accessibility premise; spring contacts absorb the cluster-clamp Z variation the plungers do not. **Count is the union of the networks that actually cross the socket after D-4 was not adopted** (§7.2); `VLED` count follows the ≥2× degraded-case rule (§8.1); two rows are forced by 19 at 2.00 mm pitch inside a 40 mm hex (§7.1) | Partly — **pin count is load-bearing and now tooling-blocking**; contact style less so. *(Rev B: 16 positions, single row, 4 + 4)* |
| **D-6** | VLED = 24 V | Holds peak contact current to **0.35 A/pin (~3× derating nominal, ~2× on loss of one contact)** at Rev 3's 3 `VLED` pins — *Rev B stated 0.26 A/pin and 4× at 4 pins* — and keeps linear drive overhead ≤7 % at practical string lengths. **Adopted programme-wide as OI-HUB-C17b**, which closed `NP-DRV-SHELL-002` OI-SHELL2-01 against its 12 V estimate | Yes, with pin-count consequences |
| **D-7** | Per-cluster I2C segments with UID-derived dynamic addressing, **18 segments** at the v1 lattice (§8.2.1) | Removes the address collision instead of muxing around it; reuses the UID `np_module_map` already depends on; collapses cascaded muxing to one tier. 18 of 32 available segments — the one-tier conclusion survives the count correction | Yes |
| **D-8** | Safety MCU gates VLED per cluster, not per socket — **18 high-side switches** | STM32G071 has no 80-GPIO option; coarse hardware cut + fine on-module control is defence in depth, not a compromise. **Switch count 18; enable count unresolved (18 vs 1) — OI-HEXTILE-13, proposed resolution at §8.4.1 (single Class C bit + Class B per-cluster gates)** | Partly |

**Rejected, with reasons:**

- **Scaling the retired 20-pin pinout.** Sixteen of its twenty pins carry LED drive current from hub-side drivers; that model does not survive ×16 socket growth (§6.1).
- **Retaining the ZONE_ID resistor ladder for type detection.** Superseded by UID auto-inventory (R-12), and a ladder cannot encode ~80 positions with usable ADC margin regardless.
- **A per-socket DG2788A gain switch.** The problem it solves disappears under D-4; replicating it 80× would be paying full NRE to preserve an artefact of hub-side metering.
- **One PD pair per cluster** (§6.4 option 3) — would break the per-tile J/cm² metering claim NP-HEX-ZM-001 §4a explicitly protects.

**Values deliberately NOT asserted:** intra-tile irradiance uniformity percentage (§4.4), final emitter part numbers and their V_f/flux (§4.3), exact per-channel emitter counts to string-length divisibility (§8.1), T1-B pod clearance and emitter count (§4.5), bezel width (§3). Each is an open item below, not an omission.

---

## 11. Open Items

| ID | Description | Blocking |
|---|---|---|
| **OI-HEXTILE-01** | **Bezel width conflict:** NP-HEX-ZM-001 §3.1 assumes 2.5 mm; NP-THERM-BEZEL-001 Rev 1 sets 1.0 mm. This document uses the conservative 2.5 mm. Resolution changes A_a by ±14.5 % and every irradiance figure with it, and governs inter-tile uniformity (§4.4) | FPC artwork; **all §4 irradiance figures** |
| **OI-HEXTILE-02** | Select 660–670 nm and 808–830 nm emitters for the base tile. §4.3's V_f and radiant-flux figures are design targets, not datasheet values. V_f binning ≤±0.1 V per NP-PROC-FPC-001 §4.2 precedent. Selection closes the string-length divisibility slack in §8.1 | FPC artwork; emitter procurement; §4.3 validity |
| **OI-HEXTILE-03** | Verify the 21-minute 1064 nm minimum session (§4.3.2) against `protocols/predefined/clinical-03-pbm-cognitive-1064.npps` and the NP-BIB-1064-001 evidence base. A 40 mm tile cannot deliver 36 J/cm² faster | 1064 nm protocol authoring; interacts with REG-1 |
| **OI-HEXTILE-04** | Illumination model for intra-tile uniformity at 3.80 mm pitch — needs emitter beam angle (OI-HEXTILE-02), PDMS diffuser scattering, and window standoff (SCAN-1) | Uniformity claim; GATE-2 bench design |
| **OI-HEXTILE-05** | T1-B spring electrode pod body diameter → number of depopulated rings (§4.5) and T1-B emitter count | T1-B layout (deferred to Rev 2) |
| **OI-HEXTILE-06** | **Cost/scope decision: ~$11.50/tile driver+metering, ~$920 at 80 sockets (§6.4).** Options: partial socket population / silicon PD on T1-A / per-cluster PD. Decide jointly with OI-HEXTILE-09 and §9.3 | **Programme-level BOM; principal decision** |
| **OI-HEXTILE-07** | On-module driver firmware spec — register map extending NP-FW-PBM1064-001 §5.1 with PD ADC readback and local NTC throttle; binding ≤2 mA standby / ≤25 mA active budget (§8.3). Successor to the retired NP-FW-ZM-TINY402-001 (OI-PBM-08) | Module bring-up; IEC 62304 Class B item registration in NP-SW-001 |
| **OI-HEXTILE-08** | Re-scope NP-SW-001 §5.2.1 / RISK-18 ZONE_ID debounce: no ZONE_ID pin exists on this interface (§7.2). Requirement remains in force for surviving non-tile accessory detection; its module-detection scope needs restating under UID inventory | NP-SW-001 revision; traceability |
| **OI-HEXTILE-09** | **Global concurrent-power governor** in the protocol compiler and session runner: a `NP_PROTO_TARGET_SOCKET_MASK` naming more than ~6 tiles exceeds the PD contract (§9.3). Gap in the delivered v2 wire format | Session execution safety; **decide with OI-HEXTILE-06** |
| **OI-HEXTILE-10** | Hub PCB **Rev C** must adopt this interface: 4× LPI2C + one-tier PCA9548A segmentation, **18** per-cluster 24 V high-side switches with safety-MCU enable (count per §8.2.1 — **was stated as 4–10; that figure was the retired 30-socket lattice's**), 3.3 V budget per §8.3. **Deletes** Rev 2's `GAIN_SEL[0..4]`, its five DG2788A switches, and its ZONE_ID-to-gain sequencing (§5 of that document). **Rev C must not be released against the old count** — 18 exceeds the 16 cluster-tail connectors NP-DRV-SHELL-002 §7.1 provisions (OI-HEXTILE-14) | Hub PCB Rev C; **coordinate before either is released** |
| **OI-HEXTILE-13** | **Is per-cluster safety *policy* wanted at 18 clusters? (§8.4)** The same question NP-HW-HUB-001 §7.4 routes to **OI-HUB-C07**. **Not a conflict between peer documents** — an earlier draft called §7.2 and NP-DRV-SHELL-002 §6 incompatible and that is **withdrawn**; §7.4 reconciles them as *"12–16 physical enable lines fanned out from one policy bit"*, and per-cluster physical gates exist in both. The undecided part is whether independently-commanded **cluster bits** are wanted. Costs of saying yes, none of them decisive: (a) the STM32G071 **package is unspecified** anywhere in the tree and demand at 18 enables is ~40 I/O, excluding every ≤32-pin option; (b) 18 cluster + 9 modality = **27 bits against a 16-bit Class C enable word** — a cost rather than a ceiling, since no SHDR fault records exist so the word can be widened pre-production (§8.4.2); (c) `np_safety_config.h` double-assigns **PA4** to SPI1 NSS and `NP_EN_PBM_ZONE4_PIN` — **re-homed to the §7.2 dead-macro cleanup** (the zone-enable macros encode the retired 5-slot meaning of "zone"; zones are now overlapping authored socket sets in `00-zones.npps` and can never be enable domains), cross-referenced from NP-FMEA-001 FMEA-M08-04. **→ PROPOSED RESOLUTION at §8.4.1:** split the enable by IEC 62304 class — per-cluster gates retained but owned by **Class B** for availability, with a **single Class C** `NP_SAFETY_EN_PBM_CRANIAL` in series as the hard interlock. **The one sufficient argument:** per-cluster policy puts a *topological* socket→cluster map behind a Class C boundary, so a MECH-2 or REG-1 change becomes a recertification rather than a regenerated table, and a stale map can cut the wrong cluster. Preserves R-11 (Class C bit in series), keeps NP-DRV-SHELL-002's availability benefit, drops demand to ~23 I/O. **Falsifier stated:** a hazard where cutting only the faulted cluster is required *and* a whole-lattice cut is unacceptable | **Safety review (OI-HUB-C07) arbitrates; blocks D-8 closure.** Review should either produce the falsifying hazard or close the item. Package selection follows. **Sequence before first SHDR fault record** — §8.4.2 |
| **OI-HEXTILE-14** | **✅ LARGELY RESOLVED 2026-08-11 by `NP-DRV-SHELL-002` Rev 2**, which adopted the §8.2.2 recommendation: **20 connector positions** (options 1 + 3) and D-7's 32-segment tree, replacing its 12/16 provisioning and its `8 branches × ≤2 = 16` tree — so both C1 and C2 close, and 18 clusters now fit. Option 4 (broadcast `SAFE_EN[n]`, 11-conductor tail, multi-drop trunk) is recorded there as **conditional on OI-HUB-C07**, not adopted. `NP-HW-HUB-001` §7.4 and §6.3 still carry 12/16 and "1 per cluster (10 at n = 80)" and remain to be corrected at its next revision; `NP-HEX-ZM-001` §5.4a's MECH-2 table still prices the flower at 12 boards / $76.08 against an actual 18 / $114.12. The §7.2 enable-word second-rationale amendment landed 2026-08-05. **Residual: NP-HW-HUB-001 and NP-HEX-ZM-001 editorial passes only.** Original analysis retained below. — **Stale cluster counts in peer documents.** SYM-1 makes the count 18; peers are sized off 12 or 10: NP-DRV-SHELL-002 §7.1 provisions **12 cluster-tail connectors, 16 positions** (18 does not fit, and its §3.4 `8 branches × ≤2 clusters = 16` tree cannot reach 18 without a third branch tier or 3-deep branches); NP-HW-HUB-001 §6.3 sizes DG2788A at "**1 per cluster (10 at n = 80)**"; NP-HEX-ZM-001 §5.4a's MECH-2 table prices the flower at **12 boards / $76.08**, actual is **18 / $114.12**. Each needs an editorial pass on its own revision — **not corrected by this revision**, which owns only NP-HW-HEXTILE-001. **Additionally: NP-HW-HUB-001 §7.2 justifies its "bits 1–4 reserved, not reused" rule *solely* by SHDR fault records, but a second, unstated rationale also holds — enable-bit position is identical to the charge-monitor channel index (`NP_SAFETY_MAX_CHANNELS`, `NP_SAFETY_CH_CLIN_STIM`, `current_ua[]`), which is Class C.** A reader combining §7.2 with the standing no-SHDR-records instruction would wrongly conclude that recycling bits 1–4 is safe. §7.2 must record the second rationale (§8.4.2). **→ PROPOSED RESOLUTION at §8.2.2:** two independent limits bind — **C1** the 16 provisioned connector positions, and **C2** the `8 branches × ≤2` I2C tree. **C2 does not exist under this document's D-7** (4 × LPI2C × PCA9548A = 32 segments, 18 used), so it resolves with OI-HUB-C17; **C1 does not self-resolve** and must be fixed before Rev 3 layout. Recommended: **adopt D-7's tree + provision 20 connector positions** (the cheap axis — adding tails costs far less than widening them, per SHELL-002 §7.3), and **if §8.4.1 is accepted, remove `SAFE_EN[n]` from the tail** — it is the only star component of the 12-conductor tail, so a single broadcast cranial enable permits a multi-drop trunk and makes connector count insensitive to a future REG-1 re-cut. Options 5–7 (decouple electrical/mechanical clusters, re-cut the lattice, pair clusters onto shared tails) assessed and not recommended | NP-DRV-SHELL-002, NP-HW-HUB-001 Rev 3, NP-HEX-ZM-001 revisions; **coordinate with OI-HEXTILE-10 and OI-HUB-C17 before either is released** |
| **OI-HEXTILE-18** | **The ≤7 % linear-loss allocation in §8.1 is asserted, not derived — and no minimum driver dropout is specified anywhere.** §8.1/§8.1.1 bound the string at `N · V_f ≥ 22.4 V` on the strength of a 1.6 V residual and a "≤7 %" figure that is not traceable to any tile power or temperature limit in this document, `NP-HW-HUB-001`, or `NP-DRV-SHELL-002`. It is a chosen allocation. **Re-derive it from §9.3's tile thermal budget, or state plainly that it is a design choice** — as written it is the same species of unsourced-figure-read-as-a-rule that `docs/status/pending-decisions.md` §13.2e found in the "~1.6–2.2 V" claim, which was itself quoting §4.3. Separately, the *upper* bound needs `V_dropout(min)` for the constant-current stage, which no document states; without it §8.1.1's ceiling is written as `24 V` when the true ceiling is lower. **This is not an abstract gap — applying the ceiling as literally written selects an unbuildable string for the channel this document already specifies:** at CH_B's own 1.60 V design target, `N = 15` gives exactly `24.00 V`, satisfying `≥22.4 V` and `≤24 V` while leaving **zero** volts for the FET and sense resistor. §8.1 specifies 14, correctly — but only 14 *because a human applied a dropout the rule does not state.* Any mechanical check of this budget will pick 15 until `V_dropout(min)` is written down | **Emitter selection (OI-HEXTILE-02); string topology; sense-resistor values.** Not tooling-blocking — the socket interface is unaffected |
| **OI-HEXTILE-19** | **No tolerance is stated on the 24 V rail, so every string-length figure in §8.1.1 is a nominal-point calculation rather than a corner analysis.** D-6 fixes `VLED` = 24 V and `NP-HW-HUB-001` §7.4 sizes the boost, but neither states a regulation tolerance. At a nominal ±5 % the low corner is 22.8 V against §8.1.1's 22.4 V floor — ~0.4 V of window **before** subtracting driver dropout (OI-HEXTILE-18), plausibly empty. Emitter V_f tempco compounds it: an NIR junction under a scalp interface runs hot, and V_f falls with temperature (SFH 4703AS TC_V = −2 mV/K), moving the string voltage down exactly when the rail corner is already low. **State a rail tolerance, then re-run §8.1.1 at the corners** | **String topology; HUB-REQ-C04 / OI-HUB-C19 boost specification.** Assess jointly with OI-HEXTILE-18 |
| **OI-HEXTILE-15** | **§5.3 and §6 still read as though D-4 holds, and D-4 was not adopted.** `NP-DRV-SHELL-002` Rev 2 §3.3a resolved **OI-HUB-C17c against D-4** (principal, 2026-08-11): the switched-gain TIA, PD mux, NTC mux and ADC stay on the cluster controller. §7 is re-cut accordingly (Rev C), but **§5.3 ("The TIA question — and why it stops being a hub problem"), §6.2's U2 dual-TIA line, and §6.4's BOM still describe on-module conversion.** This revision deliberately did **not** rewrite them — the change reaches the driver topology, the per-tile BOM and OI-HEXTILE-06's PD-population options, and is larger than a socket-interface re-cut. **What is affected:** U2 (dual TIA, $0.32/tile ≈ $26 at 80 tiles) may be deleted from the module and its function returns to the controller; the 12-bit-ADC-with-PGA requirement on U1 (§5.3, §6.2) relaxes, since dose metering no longer depends on the on-module ADC — which may reopen the ATtiny402-vs-tinyAVR-2-series choice; §6.4's ~$11.53/tile figure moves. **What is NOT affected:** the InGaAs PD selection and co-location (D-2, §5.1–5.2), which are optical decisions independent of where the transimpedance stage sits, and therefore OI-HEXTILE-06's ~$10/tile PD-population question — still the dominant term, still orthogonal | **Module BOM; OI-HEXTILE-06; §6.2 part selection.** Not tooling-blocking — the socket interface (§7) is already correct |
| ~~**OI-HEXTILE-16**~~ | **✅ CLOSED 2026-08-11 — one name per conductor adopted (§7.4).** Pin 18 is **`ELEC_SHLD`** (over `GUARD`, which collides with the *DRL-driven guard plane* on L1 that `NP-DRV-SHELL-002` §3.5/§4.1/REQ-EMI-02 specify — the pin is driven from that plane but is not it). Pin 12 is **`ALERT#`** (over `/ALERT`: a leading `/` is the hierarchical-path separator in EDA net naming, so `/ALERT` reads as a root-scope path and can be silently re-scoped on netlist import). Propagated through this document, `NP-DRV-SHELL-002` and `NP-HW-HUB-001`. Residual naming items split out to **OI-HEXTILE-17** | — (closed) |
| ~~**OI-HEXTILE-17**~~ | **✅ CLOSED 2026-08-11 — both residuals settled on the same criterion that decided OI-HEXTILE-16 (§7.4).** **(a) `SEAT_N` → `SEAT#`.** The item was raised as "a third active-low convention", i.e. cosmetic. It is not: **`_N` already means *cardinality* in this codebase** — `firmware/hub_control/tests/np_module_map_tests.c` defines `PBM_TILE_N` / `EEG_TILE_N` as `sizeof(x)/sizeof(x[0])` — so `SEAT_N` reads as *"number of seats"*. That is a live ambiguity, not a style difference, and it settles the convention **toward `#`** rather than away from it: `ALERT#` is confirmed, not reversed, with SMBus precedent (`SMBALERT#`; the in-tree vendor header exposes `I2C_ISR_ALERT` for the same line). **Rule of record: active-low interface signals take `#` — never `_N`, never a leading `/`.** **(b) `ELEC_SIG` → `ELEC`.** Ownership: the interface is defined by this §7.2 and `NP-DRV-SHELL-002` §5.1.4, and `NP-HW-HUB-001` §7.4 explicitly *adopts* that contract, so two normative pin tables do not change to match one banner's prose. Accuracy: the pin is dual-rated to carry **tES stimulation current** (≤2 mA T1 / ≤4 mA T2), and `_SIG` biases the reader toward the recording role — the half that is *not* safety-relevant. **Doc→firmware mapping stated, since `#` is not a legal identifier character: `<SIGNAL>#` → `<SIGNAL>_L`, never `_N`.** Related but NOT closed by this: `NP-FMEA-001` **OI-FMEA-01** records that the firmware's ten active-LOW enable lines carry polarity only in comments; this decision adds no further unmarked names and gives that item a convention to converge on. **No firmware change requested** | — (closed) |
| **OI-HEXTILE-11** | Pogo contact qualification: ≤50 mΩ over ≥500 cycles **in the EEG signal path** (pin 13). Contact noise in a µV recording chain is not covered by the resistance spec alone | T1-B EEG performance; FAI |
| **OI-HEXTILE-12** | FPC stack-up, trace width/spacing, and copper weight for a 24 V / 1.04 A tile. PDMS bonding (SiO₂ 75 nm interlayer + O₂ plasma) and the 200-cycle IEC 60068-2-14 qualification inherit unchanged from NP-HW-FPC-001 Rev 5 §7 and remain BLOCKING | FPC artwork release |

---

## 12. Design Review Checklist

| Item | Description | Status |
|---|---|---|
| HT-DRC-01 | 91-site lattice fits active field with ≥1.0 mm boundary clearance | ✓ (1.21 mm at 3.80 mm pitch, §4.1) |
| HT-DRC-02 | T1-A per-channel irradiance reaches the 400 mW/cm² pulsed ceiling at ≤180 mA | ✓ by construction (403 mW/cm² at 150 mA) — **conditional on OI-HEXTILE-02** |
| HT-DRC-03 | T1-C three-channel aggregate ≤600 mW/cm² | ✓ (566 mW/cm², 5.7 % margin, §4.3.2) |
| HT-DRC-04 | Emitter drive current inside the 120–180 mA L70 window in all modes | ✓ (150 mA peak; ~75 mA CW) |
| HT-DRC-05 | 1064 nm session reaches 36 J/cm² in an acceptable session length | Open — 21 min minimum, OI-HEXTILE-03 |
| HT-DRC-06 | Intra-tile irradiance uniformity quantified | Open — OI-HEXTILE-04 |
| HT-DRC-07 | Rigidizer fits the tile outline | ✓ (13.0 mm half-diagonal vs 20.0 mm inradius, §6.3) |
| HT-DRC-08 | Peak contact current ≤50 % of pogo rating, **nominal and on loss of any one `VLED` contact** | ✓ (**0.35 A nominal / 0.52 A degraded** vs ≥1.0 A — ~3× and ~2×, §8.1). Degraded case verified on real contacts by `NP-DRV-SHELL-002` **SH2-DRC-10a** |
| HT-DRC-09 | Socket pinout covers every tile type including T1-B and future types | ✓ by union construction (§1, §7.2) at **19 positions** — re-verify on any new type, **and on any change to which networks cross the socket** (the Rev 2 → Rev 3 count change came from exactly that, not from a new tile type) |
| HT-DRC-22 | **19-contact pad array is two staggered rows** and fits inside the 20.0 mm tile inradius, with mis-key asymmetry and `SEAT#` at the extreme of the last-seating row | Open — CAD; `NP-DRV-SHELL-002` REQ-SKT-01 / SH2-DRC-05a |
| HT-DRC-23 | Socket pinout matches `NP-DRV-SHELL-002` §5.1.4 pin for pin **and signal name for signal name** | ✓ at Rev 3 (§7.2, §7.4) — **run as a mechanical diff, not a review**: both name collisions closed by OI-HEXTILE-16 read as agreement to a human reader. Re-verify on either document's next revision |
| HT-DRC-10 | Contact mating sequence prevents powered-floating-return and false-seated states | ✓ (§7.3) |
| HT-DRC-11 | I2C address collision structurally impossible across ~80 modules | ✓ (UID-derived assignment, D-7) — needs firmware confirmation, OI-HEXTILE-07 |
| HT-DRC-12 | 3.3 V logic budget ≤1 W across 80 modules | Open — depends on standby firmware, OI-HEXTILE-07 |
| HT-DRC-13 | Safety MCU retains physical ownership of emitter enable (R-11) | ✓ in principle (per-cluster VLED gate, D-8) — **enable count and MCU package unresolved, OI-HEXTILE-13** (proposed resolution §8.4.1 preserves R-11 by keeping the Class C bit in series); needs Hub PCB Rev C, OI-HEXTILE-10 |
| HT-DRC-17 | Cluster count derived from the lattice under CLUSTER-1 + SYM-1, not carried from another lattice generation | ✓ (18, exhaustively verified, §8.2.1) — re-derive on any REG-1 lattice re-cut |
| HT-DRC-21 | No cluster contains a pendant petal; every cluster's petals form a contiguous arc (CONTIG-1) | ✓ (0 pendants, 0 broken arcs, §8.2.1) — **re-verify on any re-clustering**, this is not implied by cluster count |
| HT-DRC-18 | I2C segment count within one-tier mux capacity | ✓ (18 of 32, 56 %, §8.2) |
| HT-DRC-19 | Safety-MCU free-GPIO count covers the cluster enables plus all existing modality enables | **Open — OI-HEXTILE-13.** Package unspecified; demand ~40 I/O at 18 enables, or **~23 I/O under the §8.4.1 proposal** |
| HT-DRC-20 | Peer documents (NP-DRV-SHELL-002, NP-HW-HUB-001, NP-HEX-ZM-001) agree on the cluster count | **Open — OI-HEXTILE-14.** Currently 12 / 10 / 12 against this document's 18; interconnect options at §8.2.2 |
| HT-DRC-14 | Concurrent-tile power ceiling enforced before a protocol can be signed | Open — OI-HEXTILE-09 **(safety-adjacent)** |
| HT-DRC-15 | Per-tile J/cm² dose metering claim preserved | ✓ as specified (PD1/PD2 per tile) — **at risk from OI-HEXTILE-06 option 3** |
| HT-DRC-16 | PDMS bond + 200-cycle thermal cycling qualification carried forward | Inherited — remains BLOCKING, OI-HEXTILE-12 |

---

## 13. Cross-references

- **Parent:** NP-HEX-ZM-001 (`docs/np_hex_zm_001.md`) — §3 geometry, §4/§4b addressing and wire format, §4a taxonomy + SMART-1, §5.4a cluster clamps, §7 gates
- **Predecessor (SUPERSEDED, reused in part):** NP-HW-FPC-001 Rev 5 (`docs/superseded/np_hw_fpc_001.md`) — §5.1 InGaAs PD selection, §5.3 TIA-saturation methodology, §6.2 driver topology, §7 PDMS bonding all carried forward; §2/§3 connector and pinout, §3.2 ZONE_ID ladder, §4 LED counts all retired
- **Must co-revise:** NP-HW-HUB-001 (`docs/np_hw_hub_001.md`) — Rev 3 per OI-HEXTILE-10; §6.3 DG2788A count and §7.2 enable architecture per OI-HEXTILE-13/14
- **Must co-revise:** NP-DRV-SHELL-002 (`docs/np_drv_shell_002.md`) — §3.4 branch tree, §6 `SAFE_EN[n]`, §7.1 cluster-tail connector count (12/16 provisioned vs 18) per OI-HEXTILE-13/14
- **Cluster partition diagram:** `docs/diagrams/np_hextile_cluster_map.svg` — the 18-cluster midline-symmetric partition of the 80-socket lattice, socket ids and cluster boundaries (§8.2.1)
- **Lattice source of truth:** `scripts/sync-socket-map.ts` (`ROW_WIDTHS`) → `hardware/np_socket_map.json`, `app/web/src/lib/socketMap.generated.ts`
- **Firmware:** NP-FW-PBM1064-001 Rev 2 (register map, §6.6 factory calibration); `firmware/hub_control/np_module_map.{h,c}` (UID inventory, `check_placement`)
- **Optics:** NP-OPT-PSF-001 (`docs/np_opt_psf_001.md`) — ~26 mm resolution floor at cortical depth; the basis for §4.2's acceptance of sub-millimetre wavelength interleave irregularity
- **Thermal:** NP-THERM-CFD-R1-001 (Path B1, scalp-facing NTC), NP-THERM-BEZEL-001 (bezel conflict, OI-HEXTILE-01)
- **Tooling:** the universal hex-tile mould gains a standard rigidizer cavity (§6.3); NP-TOOL-ZM-SM-001 (SUPERSEDED) needs no successor
