# Hex-Tile Module — Electrical / FPC Specification (T1-A, T1-C)

**Project:** NeurOne
**Document:** NP-HW-HEXTILE-001
**Revision:** A
**Date:** 2026-07-30
**Status:** DESIGN STUDY — not a tooling baseline. Every numeric value below is a proposed engineering commitment, not a measured or locked figure. See §10 (Decisions) and §11 (Open Items).
**Effective Date:** —
**Author:** NeurOne Hardware Engineering
**Approved By:** — (pending design review)
**References:** NP-HEX-ZM-001 (2026-07-15) §3 geometry, §4 addressing, §4a module-type taxonomy + SMART-1; NP-HW-FPC-001 Rev E (SUPERSEDED — reused for driver topology §6.2, InGaAs PD selection §5.1, PDMS bonding §7, TIA-saturation methodology §5.3); NP-HW-HUB-001 Rev B (SUPERSEDED — needs Rev C); NP-FW-PBM1064-001 Rev B; NP-OPT-PSF-001 Rev A; NP-THERM-CFD-R1-001 Rev A; NP-THERM-BEZEL-001 Rev A; CLAUDE.md §3 (modality stack), §4.2 (safety architecture), §4.5 (power)
**Related Issues:** —
**Gate:** GATE-2 (PBM coupling bench) — LED array must meet dose spec at the temporal worst case before this layout is tooled
**IEC 62304 Class:** — (hardware; the on-module driver firmware is Class B, see §6.5)
**Supersedes:** — (new document; fills the gap declared in NP-HW-FPC-001 Rev E supersession note: *"no document yet specifies the T1-A/T1-C hex-tile FPC pinout or electrical layout"*)
**Parent Document:** NP-HEX-ZM-001

---

> **⚠ READ FIRST — what this document is and is not.**
>
> NP-HEX-ZM-001 specifies the hex-tile **mechanical, socket, and addressing** model. This document specifies the **electrical and FPC** design inside one 40 mm tile, for the two PBM-only types (**T1-A** base, **T1-C** 1064 smart). It is the hex-tile replacement for the electrical content of the retired NP-HW-FPC-001.
>
> **T1-B (EEG/electrode tile) is deliberately out of scope for Rev A** — its layout is a masking derivation from the lattice defined here (§4.5), but the pod clearance diameter that drives it is not yet fixed. **T2-D (1170 nm laser tile) is out of scope entirely** (laser drive ≠ LED drive).
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

**Invariant inherited from NP-HEX-ZM-001 §4a and not re-opened here:** all tile types share one mould, one outline, one socket interface, and one orientation-only mechanical key. A "type" is an FPC population difference and nothing else. Consequently **the socket pinout is the union of every type's needs**, including types not specified in this revision — any pin that T1-B or a future type requires must be present at every socket. This is what makes §7 a 16-position interface rather than a 10-position one.

---

## 2. Requirements this design is derived from

| # | Requirement | Source |
|---|---|---|
| R-1 | Tile is a regular hexagon, 40 mm flat-to-flat, module cap radius R_m = 87 mm | NP-HEX-ZM-001 §3 |
| R-2 | Any tile type inserts into any socket; identity by UID self-report, not mechanical keying | NP-HEX-ZM-001 §4a |
| R-3 | Every socket is I2C- and TIA-capable (SMART-1) | NP-HEX-ZM-001 §4a, §7 |
| R-4 | PBM ceiling 400 mW/cm² peak pulsed at ≤25 % duty; 200 mW/cm² CW | CLAUDE.md §3 modality 1 |
| R-5 | Three-channel aggregate ceiling 600 mW/cm² | NP-FW-PBM1064-001 Rev B (OI-PBM-05) |
| R-6 | Emitter drive 120–180 mA for L70 80,000–100,000 h | CLAUDE.md §3 modality 1 |
| R-7 | Session dose 60 J/cm² (660/808 nm), 36 J/cm² (1064 nm) | NP-FW-PBM1064-001 Rev B |
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

T1-B is out of scope for Rev A, but the lattice was chosen so that T1-B is a depopulation of it rather than a new design. Depopulating whole rings around the reserved centre opens a circular clearance for the spring electrode pod:

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

Reusing the array centre gives PD1 a position that is (a) identical across types, (b) already excluded from the emitter placement, and (c) at the point of maximum optical symmetry, so the PD1 reading is insensitive to which lattice axis an individual emitter batch is weak on. The centre reads above the field spatial average because of edge roll-off; this is a fixed multiplicative offset absorbed by the **existing** per-wavelength K coefficients written at factory calibration (NP-FW-PBM1064-001 Rev B §6.6, integrating-sphere procedure) — no new calibration mechanism is introduced.

- **Component:** InGaAs, Hamamatsu G12180-010A or qualified equivalent — inherited unchanged from NP-HW-FPC-001 Rev E §5.1, which its supersession note lists as still-reusable.
- **Fitted on both T1-A and T1-C.** InGaAs is broadband (600–1700 nm), so one part covers both the 2-channel and 3-channel populations and there is one PD SKU across the tile family. Per-wavelength dose separation is by firmware time-multiplexing and K coefficients, exactly as NP-FW-PBM1064-001 Rev A §6.2 already specifies.
- **Pad geometry:** 1.6 mm annular ring, hard gold ≥0.5 µm cobalt-alloyed — inherited unchanged.

Fitting InGaAs to T1-A (which has no 1064 nm channel and could use cheaper silicon) is a deliberate cost-for-uniformity trade: one PD part number across all tiles, no per-type TIA gain question, and no possibility of a silicon-PD tile being calibrated with InGaAs coefficients. See D-4 for why the gain question disappears entirely.

### 5.2 PD2 — scalp-facing backscatter

Co-located with PD1 in XY, on the opposite (scalp-facing) copper layer. The PD1/PD2 ratio is the fouling-versus-ageing discriminator (R-8), and that logic is only valid if both photodiodes sample the same optical path — co-location is load-bearing, not incidental. This is inherited directly from NP-HW-FPC-001 Rev E §5.2 and is the one geometric relationship that carries over from the retired design unchanged.

Same part, same pad geometry as PD1.

### 5.3 The TIA question — and why it stops being a hub problem

NP-HW-FPC-001 Rev E §5.3 established that InGaAs responsivity at 1064 nm (~0.90 A/W) is ~2× silicon at 808 nm (~0.47 A/W), saturating a 47 kΩ hub-side TIA, and resolved it with a per-slot DG2788A gain switch. SMART-1 turns that per-slot fix into a per-socket one, which NP-HEX-ZM-001 §4a and NP-HW-HUB-001's supersession note both flag as unscoped Hub PCB NRE at ~80 sockets.

**Decision (D-4): the TIA and its ADC move on-module. No PD analog signal crosses the socket interface.**

The saturation analysis in NP-HW-FPC-001 Rev E §5.3 remains correct physics; what changes is where it is solved. On-module:

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

Inherited in concept from NP-HW-FPC-001 Rev E §6.2 — its supersession note lists the ATtiny + N-FET architecture as still-reusable — and re-validated here for the smaller tile.

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

The FET thermal result from NP-HW-FPC-001 Rev E §6.2 carries over unchanged and with margin: at 180 mA and R_DS(on) = 27 mΩ, P = I²R = **0.87 mW** per FET. Dissipation is not a constraint on the driver; it is a constraint on the emitters (§9.3).

### 6.3 Physical fit — the 22 × 14 mm rigidizer in a 40 mm hex

The retired design placed the driver on a 22 × 14 × 0.8 mm FR4 rigidizer in a 24 × 16 × 3.5 mm cavity (NP-TOOL-ZM-SM-001 Rev A §4). The concern raised when this task was scoped — that this will not fit a much smaller tile — resolves favourably, for a geometric reason:

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

The on-module MCU firmware is a new software item. It implements the register map already defined in NP-FW-PBM1064-001 Rev A §5.1 (registers 0x00–0x0D) extended with PD1/PD2 ADC readback registers made necessary by D-4, plus the on-module NTC and the local 62 °C throttle (R-9).

**The local throttle is a hardware-adjacent Class C-adjacent function and must be independent of hub commands** — a tile must throttle itself on over-temperature even if the I2C bus is silent. The safety MCU's independent backstop is the per-cluster VLED gate (§8.4), not per-socket, because an STM32G071 does not have 80 spare GPIOs. This two-level arrangement — fine-grained on-module, coarse hardware cut at the cluster — is proposed as the resolution to NP-HEX-ZM-001 §7 **OI-HUB-SOCKET-01**, which currently states that socket-addressed commands are dropped because `NP_SAFETY_EN_PBM_ZONE_0..4` is per-zone-slot.

Firmware specification is **not** in this document; it is **OI-HEXTILE-07** (successor to the retired NP-FW-ZM-TINY402-001 / OI-PBM-08).

---

## 7. Socket interface

### 7.1 Contact technology

The retired design used a Hirose FH34S 20-pin 0.5 mm ZIF with a back-flip lever. **That is not usable here.** A ZIF lever must be manually actuated per connector; NP-HEX-ZM-001 §5.4a's whole premise is that a user with Parkinson's H&Y II–III swaps tiles by throwing one cluster clamp and lifting the tile out on its ejector spring. A lever per tile contradicts the accessibility requirement the cluster clamp exists to satisfy.

**Decision (D-5): 16-position spring-contact (pogo) interface at 2.00 mm pitch. Spring pins on the socket, flat pads on the module.**

| Property | Value | Rationale |
|---|---|---|
| Positions | 16 | §7.2 |
| Pitch | 2.00 mm | 16 × 2.0 = 32 mm across a 40 mm tile; fits with margin |
| Springs on | socket (inner bowl) | keeps the moving, wearing, fatiguing element in the part that is never removed; the swappable tile is passive gold pad |
| Plating | hard gold ≥0.8 µm over nickel, both sides | fretting/oxidation resistance, per the §5.3b spring-finger precedent |
| Current per contact | ≥1.0 A continuous | pogo pins in this size class support 1–3 A; §8.1 needs 0.26 A/pin |
| Contact resistance | ≤50 mΩ | matched to the NP-HEX-ZM-001 §5.3b ground-bond target; binding for the electrode line (§7.2 note) |
| Mating cycles | ≥500 | service-event frequency, not daily; far below the Boa dial's 50,000 |
| Blind-mate tolerance | ±0.4 mm lateral, ±0.5 mm Z | spring travel absorbs cluster-clamp plate variation across a curved cluster |

Pogo contacts also suit the **cluster clamp mechanics** directly: NP-HEX-ZM-001 §5.4a describes a clamp plate with a spring-loaded plunger per module, precisely because a rigid plate over a curved cluster cannot seat evenly. Spring contacts tolerate the residual Z variation that survives the plungers; a ZIF or board-edge connector would not.

Orientation is fixed by the tile's existing asymmetric mechanical key (R-2). The pad pattern is additionally asymmetric about the tile's long axis so a mis-keyed insertion cannot make contact — a fail-open, not a fail-wrong, geometry.

### 7.2 Pinout

Sixteen positions. The count is derived, not chosen: it is the union of every tile type's needs (§1), at the conductor width the power budget requires (§8.1).

| Pin | Signal | Direction | Notes |
|---|---|---|---|
| 1 | VLED | socket → module | 24 V LED supply, §8.1 |
| 2 | VLED | socket → module | paralleled for current sharing |
| 3 | VLED | socket → module | |
| 4 | VLED | socket → module | |
| 5 | PGND | — | LED return; paralleled ×4 to match VLED |
| 6 | PGND | — | |
| 7 | PGND | — | |
| 8 | PGND | — | |
| 9 | VCC_3V3 | socket → module | logic supply, ≤2 mA standby / ≤25 mA active (§8.3) |
| 10 | SDA | bidirectional | I2C data, 400 kHz fast mode |
| 11 | SCL | socket → module | I2C clock |
| 12 | /ALERT | module → socket | open-drain, wired-OR per bus segment; thermal/fault/OCP assertion so the hub need not poll ~80 modules |
| 13 | **ELEC** | bidirectional | dual-rated Ag/AgCl electrode — EEG µV signal **and** BES/tACS/tDCS current. Unused on T1-A/T1-C; **present at every socket** because R-2 permits a T1-B in any socket |
| 14 | **ELEC_SHLD** | socket → module | driven shield / DRL for pin 13, referenced to the EEG DRL output (CLAUDE.md §4.3) |
| 15 | AGND | — | analog/electrode return, star-referenced at the hub, **not** tied to PGND on the module |
| 16 | SEAT_N | module → socket | tied to PGND on the module through 1 kΩ; detects *partial* seating (§7.3) |

**Notes on the contentious pins:**

- **Pins 13–15 are the price of R-2.** T1-A and T1-C do not use them. They exist at every socket because "any type in any socket" means a T1-B may be inserted anywhere, and an electrode signal cannot be carried over I2C — it is a µV analog recording path to the ADS1299 *and* a stimulation current path from the tES driver. Removing them would silently re-impose the type-restricted placement model that SMART-1 was decided to eliminate. Contact resistance on pin 13 is in the EEG signal path, which is why the ≤50 mΩ target in §7.1 is binding rather than nominal.
- **Pin 12 (/ALERT) earns its position by arithmetic.** Polling 80 modules over segmented I2C for thermal status at the 10 ms cadence R-9 implies is not achievable; a wired-OR interrupt turns an 80-module poll into an exception path.
- **Pin 16 (SEAT_N) is not redundant with the I2C presence poll.** NP-HEX-ZM-001 §5.4a correctly notes an unseated tile fails its inventory poll. But a *partially* seated tile can answer I2C on two contacts while the PD, NTC, or electrode contacts are marginal — the failure mode that produces a plausible-looking but wrong dose reading. SEAT_N is positioned at the mechanical extreme of the pad pattern so it is the **last** contact to mate; if it reads low, every other contact is seated.
- **No ZONE_ID pin.** The retired resistor ladder (NP-HW-FPC-001 Rev E §3.2) is gone, replaced by UID self-report over I2C (R-12). Its ADC channel, its 1 %-tolerance resistor requirement, its threshold-margin analysis, and its debounce requirement (RISK-18, NP-SW-001 §5.2.1) do not apply to this interface. **NP-SW-001 §5.2.1 and the RISK-18 debounce requirement will need re-scoping** where they bind on module detection — flagged as **OI-HEXTILE-08**; they remain in force for any surviving non-tile accessory detection.

### 7.3 Contact sequencing

Pad lengths are staggered so mating order is deterministic:

1. **PGND** (longest) — return established before any supply
2. **VLED, AGND, ELEC_SHLD**
3. **VCC_3V3, SDA, SCL, /ALERT, ELEC**
4. **SEAT_N** (shortest) — asserts only when the stack is fully home

Break order is the reverse. This prevents the module logic powering up against a floating return, and guarantees SEAT_N cannot read seated during a partial insertion.

---

## 8. Per-socket power and I2C under SMART-1

### 8.1 VLED rail

**Decision (D-6): VLED = 24 V DC.**

Derived from conductor current, not from convenience. With the driver on-module (D-3), the socket carries DC bus power rather than per-string drive, so the only free variable is rail voltage, and it trades directly against contact current.

| Rail | T1-A peak current per tile (both channels, 150 mA) | Per VLED pin (×4) |
|---|---|---|
| 5 V | 5.0 A | 1.25 A — exceeds comfortable pogo derating |
| 12 V | 2.08 A | 0.52 A |
| **24 V ★** | **1.04 A** | **0.26 A** |

T1-A peak electrical: CH_A 45 × 0.315 W = 14.2 W, CH_B 45 × 0.24 W = 10.8 W → **25.0 W instantaneous**. At 24 V that is 1.04 A, or 0.26 A per contact across four paralleled VLED pins — a 4× derating margin against the ≥1.0 A contact rating (§7.1).

24 V also suits series-string construction: at 11 series 660 nm emitters (11 × 2.10 = 23.1 V) or 14 series 808 nm (14 × 1.60 = 22.4 V), the residual dropped across the FET and sense resistor is ≤1.6 V, so linear-loss overhead stays under 7 %. A 12 V rail would halve the string length and double the number of parallel strings and sense resistors on a tile that has no room for them.

**Consequence:** emitter count per channel must be an integer multiple of string length, which the 45/45 and 30/30/30 allocations of §4.2 do not exactly satisfy for every candidate V_f. The allocation carries **±2 sites of slack**, to be closed when emitters are selected (OI-HEXTILE-02). Worked example at the assumed V_f: CH_A 44 = 4 strings × 11; CH_B 42 = 3 × 14. The lattice geometry is unaffected — unallocated sites are simply left unpopulated.

### 8.2 I2C fan-out — the architecture SMART-1 requires

NP-HEX-ZM-001 §4a states the retired one-PCA9546A-plus-one-GPIO-mux design *"does not scale to ~30–80 sockets without a materially different I2C fan-out architecture (cascaded/multi-stage muxing)."* This is that architecture.

The retired design needed a mux per slot for one reason: every smart module shared the factory-burned address 0x30, so two modules on one bus collide. **The collision is what should be removed, not worked around** — muxing 80 sockets to preserve a fixed address is solving the wrong problem.

**Decision (D-7): dynamic address assignment from the module UID, over per-cluster bus segments.**

- **Segmentation follows the mechanical clusters.** NP-HEX-ZM-001 §5.4a already partitions ~80 tiles into 4–10 clusters (7-hex "flower" or 3-hex triad super-cells). Reusing that partition electrically means one bus segment ≈ one cluster ≈ one VLED switching domain (§8.4) — one physical grouping serving mechanics, power, safety, and addressing rather than four incompatible partitions.
- **The i.MX RT1062 provides LPI2C1–LPI2C4.** One PCA9548A-class 8-channel switch per bus gives up to 32 segments; 4–10 clusters uses a small fraction of that, leaving margin for a lattice re-cut. Note this is *one tier* of muxing, not the cascade the parent document anticipated — because addresses no longer collide, muxing is only needed for bus capacitance, not arbitration.
- **Address assignment uses the UID that already exists.** NP-HEX-ZM-001 §4 specifies power-on UID polling with re-inventory only on UID change. An SMBus-ARP-style assignment (address resolution from a unique device identifier) maps onto that directly: the hub enumerates a segment, assigns each module an address from its UID, and the resulting map feeds `np_module_map` unchanged. **No new identity concept is introduced** — the UID the addressing layer already depends on becomes the addressing key.
- **Capacitance:** at 400 kHz fast mode the 400 pF bus limit permits roughly 10–19 modules per segment with disciplined routing, which brackets the 3–7-tile cluster sizes and the 7-hex flower's 7. Segment routing length is the real constraint and is a Hub PCB Rev C layout item.
- **Pull-ups:** one 4.7 kΩ pair per segment (not per socket). At 10 segments, 20 resistors — versus the 160 that a per-socket scheme would need.

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

**Decision (D-8): the safety MCU gates VLED per cluster** — 4–10 high-side switches on the cluster segments defined in §8.2, replacing the retired `NP_SAFETY_EN_PBM_ZONE_0..4` five-zone scheme. Cutting VLED removes emitter drive regardless of on-module state, so a wedged module MCU cannot sustain output. Per-socket granularity is provided by the on-module driver (fine, fast, not safety-rated); the cluster gate is the coarse, hardware, safety-rated backstop.

This is proposed as the hardware half of the resolution to **OI-HUB-SOCKET-01**.

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
| **D-4** | TIA + ADC on-module; no PD analog signal crosses the socket | Deletes the ~80× DG2788A gain-switch NRE rather than redesigning it; gain fixed to the PD actually fitted; removes the longest high-impedance analog path in the system | **No** — same coupling as D-3 |
| **D-5** | 16-position pogo interface, 2.00 mm pitch, springs on socket | A per-tile ZIF lever contradicts the NP-HEX-ZM-001 §5.4a accessibility premise; spring contacts absorb the cluster-clamp Z variation the plungers do not | Partly — pin count is load-bearing, contact style less so |
| **D-6** | VLED = 24 V | Holds peak contact current to 0.26 A/pin (4× derating) and keeps linear drive overhead ≤7 % at practical string lengths | Yes, with pin-count consequences |
| **D-7** | Per-cluster I2C segments with UID-derived dynamic addressing | Removes the address collision instead of muxing around it; reuses the UID `np_module_map` already depends on; collapses cascaded muxing to one tier | Yes |
| **D-8** | Safety MCU gates VLED per cluster, not per socket | STM32G071 has no 80-GPIO option; coarse hardware cut + fine on-module control is defence in depth, not a compromise | Partly |

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
| **OI-HEXTILE-01** | **Bezel width conflict:** NP-HEX-ZM-001 §3.1 assumes 2.5 mm; NP-THERM-BEZEL-001 Rev A sets 1.0 mm. This document uses the conservative 2.5 mm. Resolution changes A_a by ±14.5 % and every irradiance figure with it, and governs inter-tile uniformity (§4.4) | FPC artwork; **all §4 irradiance figures** |
| **OI-HEXTILE-02** | Select 660–670 nm and 808–830 nm emitters for the base tile. §4.3's V_f and radiant-flux figures are design targets, not datasheet values. V_f binning ≤±0.1 V per NP-PROC-FPC-001 §4.2 precedent. Selection closes the string-length divisibility slack in §8.1 | FPC artwork; emitter procurement; §4.3 validity |
| **OI-HEXTILE-03** | Verify the 21-minute 1064 nm minimum session (§4.3.2) against `protocols/predefined/clinical-03-pbm-cognitive-1064.npps` and the NP-BIB-1064-001 evidence base. A 40 mm tile cannot deliver 36 J/cm² faster | 1064 nm protocol authoring; interacts with REG-1 |
| **OI-HEXTILE-04** | Illumination model for intra-tile uniformity at 3.80 mm pitch — needs emitter beam angle (OI-HEXTILE-02), PDMS diffuser scattering, and window standoff (SCAN-1) | Uniformity claim; GATE-2 bench design |
| **OI-HEXTILE-05** | T1-B spring electrode pod body diameter → number of depopulated rings (§4.5) and T1-B emitter count | T1-B layout (deferred to Rev B) |
| **OI-HEXTILE-06** | **Cost/scope decision: ~$11.50/tile driver+metering, ~$920 at 80 sockets (§6.4).** Options: partial socket population / silicon PD on T1-A / per-cluster PD. Decide jointly with OI-HEXTILE-09 and §9.3 | **Programme-level BOM; principal decision** |
| **OI-HEXTILE-07** | On-module driver firmware spec — register map extending NP-FW-PBM1064-001 §5.1 with PD ADC readback and local NTC throttle; binding ≤2 mA standby / ≤25 mA active budget (§8.3). Successor to the retired NP-FW-ZM-TINY402-001 (OI-PBM-08) | Module bring-up; IEC 62304 Class B item registration in NP-SW-001 |
| **OI-HEXTILE-08** | Re-scope NP-SW-001 §5.2.1 / RISK-18 ZONE_ID debounce: no ZONE_ID pin exists on this interface (§7.2). Requirement remains in force for surviving non-tile accessory detection; its module-detection scope needs restating under UID inventory | NP-SW-001 revision; traceability |
| **OI-HEXTILE-09** | **Global concurrent-power governor** in the protocol compiler and session runner: a `NP_PROTO_TARGET_SOCKET_MASK` naming more than ~6 tiles exceeds the PD contract (§9.3). Gap in the delivered v2 wire format | Session execution safety; **decide with OI-HEXTILE-06** |
| **OI-HEXTILE-10** | Hub PCB **Rev C** must adopt this interface: 4× LPI2C + one-tier PCA9548A segmentation, per-cluster 24 V high-side switches with safety-MCU enable, 3.3 V budget per §8.3. **Deletes** Rev B's `GAIN_SEL[0..4]`, its five DG2788A switches, and its ZONE_ID-to-gain sequencing (§5 of that document) | Hub PCB Rev C; **coordinate before either is released** |
| **OI-HEXTILE-11** | Pogo contact qualification: ≤50 mΩ over ≥500 cycles **in the EEG signal path** (pin 13). Contact noise in a µV recording chain is not covered by the resistance spec alone | T1-B EEG performance; FAI |
| **OI-HEXTILE-12** | FPC stack-up, trace width/spacing, and copper weight for a 24 V / 1.04 A tile. PDMS bonding (SiO₂ 75 nm interlayer + O₂ plasma) and the 200-cycle IEC 60068-2-14 qualification inherit unchanged from NP-HW-FPC-001 Rev E §7 and remain BLOCKING | FPC artwork release |

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
| HT-DRC-08 | Peak contact current ≤50 % of pogo rating | ✓ (0.26 A vs ≥1.0 A, 4× margin, §8.1) |
| HT-DRC-09 | Socket pinout covers every tile type including T1-B and future types | ✓ by union construction (§1, §7.2) — re-verify on any new type |
| HT-DRC-10 | Contact mating sequence prevents powered-floating-return and false-seated states | ✓ (§7.3) |
| HT-DRC-11 | I2C address collision structurally impossible across ~80 modules | ✓ (UID-derived assignment, D-7) — needs firmware confirmation, OI-HEXTILE-07 |
| HT-DRC-12 | 3.3 V logic budget ≤1 W across 80 modules | Open — depends on standby firmware, OI-HEXTILE-07 |
| HT-DRC-13 | Safety MCU retains physical ownership of emitter enable (R-11) | ✓ (per-cluster VLED gate, D-8) — needs Hub PCB Rev C, OI-HEXTILE-10 |
| HT-DRC-14 | Concurrent-tile power ceiling enforced before a protocol can be signed | Open — OI-HEXTILE-09 **(safety-adjacent)** |
| HT-DRC-15 | Per-tile J/cm² dose metering claim preserved | ✓ as specified (PD1/PD2 per tile) — **at risk from OI-HEXTILE-06 option 3** |
| HT-DRC-16 | PDMS bond + 200-cycle thermal cycling qualification carried forward | Inherited — remains BLOCKING, OI-HEXTILE-12 |

---

## 13. Cross-references

- **Parent:** NP-HEX-ZM-001 (`docs/np_hex_zm_001.md`) — §3 geometry, §4/§4b addressing and wire format, §4a taxonomy + SMART-1, §5.4a cluster clamps, §7 gates
- **Predecessor (SUPERSEDED, reused in part):** NP-HW-FPC-001 Rev E (`docs/np_hw_fpc_001.md`) — §5.1 InGaAs PD selection, §5.3 TIA-saturation methodology, §6.2 driver topology, §7 PDMS bonding all carried forward; §2/§3 connector and pinout, §3.2 ZONE_ID ladder, §4 LED counts all retired
- **Must co-revise:** NP-HW-HUB-001 (`docs/np_hw_hub_001.md`) — Rev C per OI-HEXTILE-10
- **Firmware:** NP-FW-PBM1064-001 Rev B (register map, §6.6 factory calibration); `firmware/hub_control/np_module_map.{h,c}` (UID inventory, `check_placement`)
- **Optics:** NP-OPT-PSF-001 (`docs/np_opt_psf_001.md`) — ~26 mm resolution floor at cortical depth; the basis for §4.2's acceptance of sub-millimetre wavelength interleave irregularity
- **Thermal:** NP-THERM-CFD-R1-001 (Path B1, scalp-facing NTC), NP-THERM-BEZEL-001 (bezel conflict, OI-HEXTILE-01)
- **Tooling:** the universal hex-tile mould gains a standard rigidizer cavity (§6.3); NP-TOOL-ZM-SM-001 (SUPERSEDED) needs no successor
