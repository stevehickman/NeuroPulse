# NeurOne — Mechanical CAD Parts List

**Source basis:** `CLAUDE.md` §1–§6 (core, locked) + `docs/np_helmet_geom_001.md` (shell layer
stack) + `docs/np_hex_zm_001.md` (hex socket/module architecture + CLUSTER-1 / SYM-1 / CONTIG-1,
current) + **`docs/np_hw_hextile_001.md` Rev B (tile electrical/FPC, emitter lattice, socket pogo
interface, cluster count — current)** + **`docs/np_drv_shell_002.md` Rev A (L1 socket interconnect,
cluster carriers, parting-plane crossing — current, DRAFT)** + **`docs/np_hw_hub_001.md` Rev C (hub
interface contract, 24 V rail, safety gating — current, DRAFT)** + `docs/np_therm_bezel_001.md`
+ `docs/reference/durability-maintenance.md` + `docs/np_tool_hub_001.md` + **`docs/np_hfe_002.md`
(retires the module braille / tactile-dot keying features)** + `docs/neurone_tool_shell_001.docx`
+ `docs/neurone_tool_lens_001.docx` + `docs/np_hw_fpc_001.md` / `docs/np_tool_zm_sm_001.md`
(SUPERSEDED — retained only for the sub-elements their own supersession banners still name as
reusable) + `docs/np_pwr_budget_001.md` + `docs/reference/accessories-roadmap.md`.

**Date:** 2026-08-11 (rev 2 — re-baselined against `origin/main` @ `e1f8a51`)
**Previous:** 2026-07-29 (rev 1, commit `79e2ccc`)

> **★ Principal direction 2026-08-11 — bezel height decided at `h_b` = 1.0 mm** (the calculated
> NP-THERM-BEZEL-001 value), conditional on it allowing forced cooling airflow. **Condition tested and
> met**: the bezel gap is a still-air conduction decoupler and is not in the forced-airflow path at
> all — forced convection lives on the outward junction → BN-boss → shell → fan path, and the shielded
> interior is deliberately unventilated. At 1.0 mm the outward path is the preferred sink across the
> whole forced-convection range; at 0.6 mm it is marginal. The competing 2.5 mm was a table-header
> assumption in `NP-HEX-ZM-001` §3.1, never a derivation. **Full reasoning and the six consequences to
> propagate are at §L-2.**

**Status:** Working parts inventory for CAD tooling planning. Several source documents are explicitly
**PROVISIONAL / DESIGN STUDY / DRAFT** (not a locked tooling baseline) — those parts are flagged
**[PROVISIONAL]** below.

> ### ⚠ What changed since rev 1 — read this before using any row
>
> Rev 1 was written the same day `NP-DRV-SHELL-002` Rev A landed and before `NP-HW-HEXTILE-001`
> existed. Those two documents, plus `NP-HW-HUB-001` Rev C, **replace the mechanical basis of the
> inner bowl and the socket interface**. Four rev-1 statements were not merely stale — they were the
> opposite of the current position:
>
> | Rev 1 said | Current position | Owner |
> |---|---|---|
> | Cluster clamps: **4–10** | **18** clusters, provably minimal under CLUSTER-1 + SYM-1 + CONTIG-1 | NP-HEX-ZM-001 §5.4a; NP-HW-HEXTILE-001 §8.2.1 |
> | Zone-module FPC bundle routes on the scalp-facing inner surface; EEG channel keeps ≥15 mm from it | **Tiles have no FPC tail at all.** The molded per-zone FPC channels (REQ-ST-01..07) are **deleted from shell tooling**; ≥15 mm is **shown unachievable** and replaced by four mechanisms retaining the <5 µVpp threshold | NP-DRV-SHELL-002 §1.1, §9.1, §10.2 |
> | Rigidizer board is `[ARCH-TRANSITION]` — dimensions not re-validated at 40 mm hex | **Resolved and inverted**: 22 × 14 mm has a 13.0 mm half-diagonal against the hex's 20.0 mm inradius. Because every tile type now carries a driver, the rigidizer cavity is a **standard feature of the universal mould**, not a variant | NP-HW-HEXTILE-001 §6.3, D-3 |
> | Silicon PDs on base tiles, InGaAs on smart tiles → drives a hub-side TIA gain-switch requirement | **InGaAs on every tile** (one PD SKU); TIA + ADC move **on-module**, so the gain switch is **deleted, not relocated** | NP-HW-HEXTILE-001 D-2, D-4 |
>
> **Three numbers are now contested between current documents and this list does not pick between
> them** — see §L. They are the socket contact count, the perimeter bezel width, and whether the
> cluster carrier holds a local MCU.

**Numbering:** Part IDs are mnemonic (`NP-CAD-<AREA>-<NN>`) for this document and the companion
dependency graph (`cad-parts-dependency-graph.dot`). They are not yet official drawing numbers.
Rev-1 IDs are preserved verbatim — the graph references them.

---

## A. Headset shell / chassis (two-nested-bowl architecture)

| ID | Part | Qty | Key CAD constraints |
|----|------|----|----------------------|
| SHELL-OUTER | Outer bowl (structural + EMF shell, L2+L3) | 1 | CFRP structural shell, laminated 0.2mm mu-metal (PETG-encapsulated) + palladium-polyester (0.1mm) + carbon-loaded absorber foam (3.0mm) bonded to inner face, unbroken. Total L2+L3 ≈6mm. Never opened in service. Must carry ≥35–45dB ELF / ≥40–60dB RF combined (prototype acceptance: meet/exceed single-shell baseline). Houses Helmholtz coil formers (fixed geometry, calibration-critical) and the non-conductive CFRP TMS window (T2). Crown exterior ≈189mm (157mm rim→crown datum + ~32mm stack). |
| SHELL-INNER | Inner bowl (module carrier / socket layer, L1) | 1 | Glass-filled PBT (lead) or PA66-GF30 (cost-down fallback) — **non-magnetic, low-eddy** (fluxgate mounts live here; REQ-EMI-10 forbids *any* conductive addition to L1 without fluxgate re-qualification). Radial depth ~18–22mm. Holds ~80 keyed hex sockets on a parity-alternating lattice, cluster-clamp bosses, fluxgate mounts. **No longer a molded part: L1 is now an electro-mechanical assembly** — cluster carrier boards and their tails are *laminated into* the bowl, not harnessed to it. New supplier category (rigid-flex lamination into a molded carrier) required, OI-SHELL2-04. Dimensional stability to REG-1 10-20 registration tolerance across 60–110°F. |
| SOCKET-BODY | Hex socket body (×~80, one moulded feature repeated) | ~80 | 40mm flat-to-flat hexagon, circumradius a=W/√3, moulded to curvature-median Rm≈87mm. Asymmetric keying (orientation-only — **no per-type mechanical key**, SMART-1 decision 2026-07-28); pad pattern additionally asymmetric about the long axis so a mis-keyed insertion **fails open, not wrong** (NP-HW-HEXTILE-001 §7.1). Every socket I2C-capable (SMART-1 full coverage). *"TIA-capable" no longer means a TIA at the socket* — under NP-HW-HEXTILE-001 D-4 the TIA sits on the tile; D-4 is deferred pending the module heat-sink design (OI-HUB-C17c). Row widths (parity-alternating) 3-6-7-8-9-8-9-8-7-6-5-4. **[PROVISIONAL — REG-1 gate: 10-20 registration vs. shell CAD not yet fixed.]** |
| SOCKET-CONTACT | Per-socket spring-contact (pogo) array | ~80 arrays | **Springs on the socket, flat gold pads on the module** — the wearing element stays in the part that is never removed. 2.00mm pitch; hard gold ≥0.8µm over nickel both halves; ≥1.0A continuous per contact; ≤50mΩ contact resistance (binding, not nominal, because the `ELEC` contact sits in the µV EEG path); ≥500 mating cycles; blind-mate tolerance ±0.4mm lateral / ±0.5mm Z (absorbs residual clamp-plate variation the plungers do not). Pad lengths **staggered in 4 tiers** for deterministic mate order: PGND → VLED/AGND/ELEC_SHLD → VCC/SDA/SCL/ALERT/ELEC → SEAT_N last. **Contact count is UNRESOLVED — 18 / 16 / 14–15 are all live. See §L-1.** |
| BLANK-PLUG | Blanking plug (empty/unpopulated socket cover) | up to ~80 | Overmoulded PC core + LSR seal face, opaque, identical hex footprint to a live module — seals exactly like a filled socket. **Quantity is now a first-order question, not a rounding:** the power envelope permits only ~6 concurrent tiles (NP-HW-HEXTILE-001 §9.2) and full population costs ~$920/headset in driver+metering, so a 20–30-tile build with 50–60 blanking plugs is an explicitly-tabled option (OI-HEXTILE-06). Tool for the high count. |
| SOCKET-GASKET | Per-socket co-molded LSR gasket land | ~80 | Shore 40–50A medical silicone, D-section 2.5×2.0mm (legacy 5-slot dimension, carried forward pending hex-lattice FAI), 20% compression when seated. IPX4 target after 10 field swap cycles (FAI-IPX-02, BLOCKING). Isolates a leak to one socket by design (rejected the continuous-grommet-sheet alternative). **New ingress path to qualify:** the compression contact array itself is a seam the rev-1 ZIF-tail model did not have — SH2-DRC-11. |
| ELEC-HYDRO-CAP | Electrode hydration/fouling cap (tethered) | per electrode socket | Silicone, WVTR <0.5 g/m²/day, tethered (loss-prevention). Protects sintered Ag/AgCl face when module uninstalled; extends factory-sealed storage to 24+ months. |
| BEZEL | Sacrificial bezel land (optical modules only) | per optical socket | **★ DECIDED 2026-08-11 (principal direction) — `h_b` = 1.0mm, the NP-THERM-BEZEL-001 Rev A §4.5 value.** Standing 1.0mm proud of face; impact + thermal-decoupling dual function. Electrode-pod aperture is **bezel-free** (s=0 by design). The competing 2.5mm was **never derived** — it appears in NP-HEX-ZM-001 §3.1 only as a table column header (*"Active coverage (2.5 mm bezel)"*), an input to the coverage arithmetic, and in NP-HW-HEXTILE-001 §3 only as the conservative carry-forward of that input. 1.0mm is the calculated value: `R_gap` = `s`/`k_air` = 0.0385 m²K/W, raising the module-face ceiling to ~45.5 °C. **Condition tested and met — see §L-2: the bezel gap is not in the forced-airflow path at all, and 1.0mm makes the forced-convection outward path the preferred sink where 0.6mm leaves it marginal.** Consumes 8% of one-sided electrode-pod travel (was 5% at 0.6mm) — accepted. Residual gates BEZEL-1a (comfort/fit over 52–62cm heads) and BEZEL-1b (pod still seats at 80–120 g) remain open; they can reject 1.0mm on comfort, in which case 0.6mm is the fallback and the face ceiling tightens to ~44 °C. |
| CLUSTER-CLAMP | Module cluster-clamp assembly (over-center lever-throw + clamp plate + per-module spring plungers) | **18** (was 4–10 in rev 1) | **CLUSTER-1** (principal, 2026-07-30): the 7-hex flower is the cluster unit wherever the lattice allows one, partial flower at the boundary; the 3-hex triad is **excluded**. Longest span **122.2mm**, arc 89.2°, stored dome depth **25.1mm**. An 8th tile would raise plate bending stress ×1.75 and plate-mode deflection ×3.07 — plate compliance is the very failure the per-module plungers exist to guard, so ≤7 is load-bearing. **SYM-1** (principal, 2026-08-04): the partition is mirror-symmetric about the sagittal midline, which **forces the count to 18** (6 midline-centred + 6 mirror pairs, provably minimal; cluster sizes 3–6). **CONTIG-1** (principal, 2026-08-04): a cluster's petals must form a **contiguous arc** — no pendant petal, because the clamp plate cannot cantilever 40mm across a foreign socket on a 23.09mm-wide neck over a 2.33mm dome. Push/pull over-center toggle latch — **not a twist cam** (RISK-22: Parkinson's H&Y II–III / post-stroke grip). Plate carries one spring plunger per module through clearance features and **carries no conductors** (it must actuate no electrical disconnection except at the module pads). Validate via HFE formative (n=5), MECH-2. |
| RIM-LATCH | Rim layer-latch, four-corner (AL/AR/PL/PR) | 4 | Symmetric four-corner clamp at the outer-bowl/inner-bowl parting plane, seated between ear and neck-attachment zones. Integrates **hard-gold-plated BeCu spring fingers** (shield-to-ground bond, ≤50mΩ target) and a **Hall/contact closed-sensor** (safety interlock: no modality enable unless all 4 report closed). |
| RIM-LABYRINTH | Labyrinth rim lip + conductive elastomer bead | 1 (continuous, both bowls) | ≥2× overlap fold; target residual slot ≤ λ/20 at 6GHz ≈ 2.5mm. Conductive elastomer bead closes residual RF gap. Replaceable/tethered service part (elastomer takes compression set over clamp cycles). |
| BLIND-MATE-BOSS | Posterior-center blind-mate sensor/coil/module connector boss | 1 | NOT a latch — standalone boss at occiput centerline; mates automatically as bowls close, seated by flanking PL/PR latches. **Now the single shared crossing for the module interconnect as well** (a second aperture would be a second RF slot to defend). **Must present four segregated contact groups with independent returns** — {N1 power}, {N2/N5 digital+safety}, {N4 post-ADC digital}, {fluxgate/coil harness} — star-returned at the Hub PCB, with **no shared return between the power group and the fluxgate/coil group**: a shared impedance lets the Helmholtz actuator modulate its own sensor's reference, nulling at the fluxgate while the field at the brain diverges (same shape as FMEA-G07-01). No bend permitted within 5mm of the boss. **TIME-BOXED — this constrains the boss contact layout, which MECH-1 tools. OI-SHELL2-02, REQ-EMI-05.** Position count also unresolved: 16 provisioned vs 18 clusters vs 20 recommended (§L-3). |
| FLUXGATE-MOUNT | Fluxgate magnetometer mount (3-axis) | 1 set | Mounts on inner bowl (near scalp — samples the field the wearer experiences). Non-magnetic local structure required. **New load on this mount:** the N1 LED bus is now an ELF dB/dt source on the same layer, so cancellation calibration is **configuration-dependent** (any tile type in any socket, plus blanking plugs, changes the self-field) and must re-run on `np_module_map` rebuild — REQ-EMI-11, OI-SHELL2-07. |
| HELMHOLTZ-FORMER | Helmholtz coil former (3-axis pairs) | 1 set | Fixed geometry on outer bowl (co-designed as one magnetic circuit with the mu-metal layer) — geometry is calibration-critical and must not vary clamp-to-clamp. |
| TMS-WINDOW | Non-conductive CFRP TMS window (T2 only) | 1 | Local break in CFRP L3 at coil site; local L2 mu-metal routed around it to prevent eddy-current field loss. |
| ~~EEG-ROUTE-CHANNEL~~ | ~~EEG cable routing channel (molded, outer-CFRP inner surface)~~ | **RETIRED** | **[SUPERSEDED — NP-DRV-SHELL-002 §1.1, §9.1, §10.2.]** The rev-1 entry described the retired 5-slot architecture: a molded 8×5mm channel with retention clips, keeping the EEG harness ≥15mm from the zone-module FPC bundle by putting them on opposite sides of the shell wall. **There is no zone-module FPC bundle and no separate EEG harness.** EEG electrodes now live inside T1-B tiles on the same L1 carrier as PBM drive current; the retired REQ-ST-01..07 shell channel features are **deleted from shell tooling** (a net simplification of the shell tool — the complexity moves into L1 lamination). ≥15mm is unachievable and **not claimed**; the <5 µVpp artifact threshold it served is **retained verbatim** as SH2-DRC-16 and met by four other mechanisms (§K, §L-6). Kept as a tombstone row so the dependency graph and any drawing referencing it resolve to an explicit retirement rather than a missing part. |
| ~~ZONE-PLUG-ANCHOR~~ | ~~Zone-slot plug anchor post~~ | **RETIRED** | **[SUPERSEDED.]** Rev 1 flagged this `[ARCH-TRANSITION]` and asked whether it needed re-derivation at ~80 sockets or was superseded by the tethered `ELEC-HYDRO-CAP` / `BLANK-PLUG` model. **It is superseded.** Its color-coded ZM-01…05 5-zone scheme encodes the retired five-slot architecture; the blanking plug now seals exactly like a filled socket and needs no separate tether post, and NP-HFE-002 retired the tactile/colour keying layer this scheme served (see below). No successor part. |
| PORT-COVER-ANCHOR (shell) | Accessory port cover anchor post (USB-C, hub L/R accessory ports) | 3 | Same boss geometry as the retired ZONE-PLUG-ANCHOR (Ø4.0×3.5mm boss, through-slot for a tether loop, ≥500 tug cycles @5N); Shore 40A TPE tether, neutral (non-zone) color. ≥15mm tether clearance from port centreline (full-size USB-C plug insertion with cover hanging clear). |
| TEMPORAL-WING-BOSS | Temporal wing anchor boss (mastoid pad mount) | 2 (bilateral) **[PROVISIONAL]** | Recessed snap-fit receiver, 34×34×5mm envelope, 30mm diameter opening (mates 40Hz mastoid LRA pad accessory). Snap release 3–6N axial. Withstands 15N axial / 10N lateral shear (FEA before first cut). Flush/recessed when pad not attached; tethered dust cover when empty. Non-labelled in Rev A tooling pending HOPE Phase 3 clinical result. |

> **Retired module-keying features (NP-HFE-002 Rev A, 2026-07-31).** The five-layer RISK-15 keying
> scheme is superseded, and **layers 3–5 are retired outright, not re-scaled**: braille + raised
> numeral on the module body, N tactile dots on the shell, and the bone-conduction insertion
> confirmation tone. `NP-TOOL-ZM-SM-001` F-05/F-06 (braille + tactile dots) therefore have **no
> successor features in the universal hex-tile mould.** Blind identification is now app-guided and
> per-cluster: a tile is reliably seated only once its cluster clamp is thrown, so readback is
> per-cluster (3–7 tiles), not per-module. **Mechanical dependency: that loop assumes cheap
> re-opening of a cluster — a high-force or fiddly MECH-2 actuator degrades it badly (OI-HFE2-05).**

## B. Fit system

| ID | Part | Qty | Key CAD constraints |
|----|------|----|----------------------|
| BOA-DIAL | Boa occipital dial + enclosed PTFE-lined cable channel | 1 | 10cm range, 0.5mm/click, 50,000-cycle rated. Channel fully enclosed (prevents hair entanglement) — must be molded in from first shell tooling cut. |
| BOA-CABLE | Boa replacement lace/cable + hook tool | 1 (+ spare in box) | Matches Boa-supplier lace OD (datasheet still pending, OI-HTOOL-02 — governs downstream hub-channel bend radius, see HUB-CHANNEL). |
| FOREHEAD-BRIDGE | 5-position forehead bridge | 1 | 5mm adjustment steps. |
| TEMPORAL-WING | Temporal stability wing (snap-on) | 2 | Stored in hub dock when detached. Carries TEMPORAL-WING-BOSS as an integral molded feature. |
| ELECTRODE-POD | Spring-decoupled electrode pod (housing + spring + plunger) | per EEG/electrode site (8–9 T1, ~19 T2 scalp) | 80–120g contact force, ±12mm travel, Shore 30A silicone, independent of Boa dial tension. Bezel (optical sockets) consumes 5–8% of one-sided travel where co-located; electrode-pod aperture itself is bezel-free. **The pod body diameter is not specified anywhere in the document set (OI-HEXTILE-05), and it is the input that decides the T1-B tile layout** — depopulating emitter rings 0–1 / 0–2 / 0–3 around the reserved centre opens a 7.6 / 15.2 / 22.8mm clearance respectively. Until it is fixed, T1-B emitter count is undetermined. |

## C. Zone modules — universal hex-tile family (current architecture, `np_hex_zm_001` + `np_hw_hextile_001`)

One mechanical mould, 40mm flat-to-flat hexagon, orientation-keyed only; "type" is an
element-population/FPC difference, not a separate mechanical part family. **Every type now carries an
on-module driver (D-3) and its TIA/ADC (D-4, deferred on thermal), so the rigidizer cavity is a
standard mould feature and there is no smart-module mould variant — `NP-TOOL-ZM-SM-001` needs no
successor.**

| ID | Part | Qty | Key CAD constraints |
|----|------|----|----------------------|
| TILE-SHELL | Universal hex-tile shell (shared mould, all T1/T2 types) | 1 mould, many instances | 40mm flat-to-flat, curvature-median Rm=87mm cap. **The tile has no FPC tail** — it is a sealed body presenting a flat gold pad array on its back face, seated by the cluster clamp's per-module spring plunger. This deletes the dominant rev-1 failure mode (fatigue cracking at a stiffener edge over 1,000 swap cycles) and leaves **zero dynamic-flex paths in the module interconnect** (REQ-BR2-02 set is empty). Worst-case module-to-scalp mismatch Δs≈1.04mm at 40mm, absorbed by PDMS window standoff + ≤0.8mm compliant gasket. Emitter lattice occupies the emitting face; the driver mounts on the **reverse (shell-facing) face**, in the splay volume the concave tessellation opens — the two never compete for area. **[PROVISIONAL — GATE-1/GATE-2 curvature-scan + PBM-coupling bench not yet passed.]** |
| TILE-T1A | T1-A base-PBM element population (660–670+808–830nm, 45+45 of a 91-site lattice) | majority of sockets | 5-ring centered-hexagonal lattice, **91 sites at 3.80mm pitch, array circumradius 19.00mm**, site 0 reserved (no emitter on any type), leaving 90 emitters. No EEG. **Now carries the same on-module driver + TIA + NTC as T1-C** — assembly is identical with Q3/R3 and the CH_C string depopulated. Emitter parts not yet selected (OI-HEXTILE-02); per-channel counts carry ±2 sites of slack pending string-length divisibility at 24 V. |
| TILE-T1B | T1-B EEG/electrode element population (dual-rated Ag/AgCl electrode + depopulated PBM + PD + NTC) | ~8–9 (T1 montage sites) | Electrode is dual-rated: records EEG **and** delivers BES/tACS/tDCS — no separate stim-only electrode part. **A masking derivation of the T1-A lattice, not a separate layout** — no new FPC outline, no new socket interface, only a different placement file. Ring depopulation set by ELECTRODE-POD body diameter (OI-HEXTILE-05, open). Layout deferred — out of scope of NP-HW-HEXTILE-001 Rev B. |
| TILE-T1C | T1-C smart 1064nm element population (660/808/1064nm, 30+30+30) | protocol-defined depth zones | Carries the same rigidizer sub-board as every other type. Aggregate 566 mW/cm² at full drive vs the 600 mW/cm² three-channel ceiling — **satisfied by emitter count, not by firmware throttling.** Mechanically relevant consequence: 1064nm emitter efficiency (~4.8% WPE) sets a **21-minute floor** on any 1064nm session in a 40mm tile — a real narrowing vs the retired 66×78mm module (OI-HEXTILE-03). |
| TILE-T2D | T2-D 1170nm deep-PBM laser element population (laser diode + TEC + laser driver) | T2 depth zones | Non-tile-shared internals (laser ≠ LED driver); still occupies the universal TILE-SHELL footprint. Electrically **out of scope** of NP-HW-HEXTILE-001 — no successor spec yet. |
| RIGIDIZER-BOARD | On-module driver rigidizer sub-board (tinyAVR 2-series I2C slave + 3× N-FET + dual TIA, FR4) | **per tile, all types** | **`[ARCH-TRANSITION]` flag REMOVED — the fit question is resolved favourably.** A 22×14mm rectangle has a 13.0mm half-diagonal against the hex's **20.0mm inradius** → 7mm margin on every side; in-plane area was never the binding constraint. The binding constraint is **z**, satisfied by mounting on the shell-facing face where the concave tessellation splays open. MCU upgraded from ATtiny402 (10-bit ADC, thin for a dose claim) to **ATtiny426/427-class, 12-bit ADC with PGA**. 1.0mm thermal vent to lateral face; 2-point 1.0mm alignment boss. **Cavity is now a standard mould feature.** Open: FPC stack-up, trace width and copper weight for a 24 V / 1.04 A tile (OI-HEXTILE-12). |
| PDMS-WINDOW (tile) | PDMS optical window + 75nm SiO₂ interlayer, O₂-plasma activated | per optical tile | >90% T at 660–1170nm. 200-cycle IEC 60068-2-14 thermal-cycle qualification **BLOCKING** for production (inherits unchanged, OI-HEXTILE-12). Anti-fouling (hydrophilic, repels sebum). Window edge bead is why the emitter array stops 1.2mm inside the active-field boundary. |
| AG-AGCL-CONTACT | Sintered Ag/AgCl electrode contact (dual-rated: EEG record + tES deliver) | per TILE-T1B | Low/stable half-cell potential, chloride-corrosion resistant, ISO 10993. Seal at pod-perimeter gasket, not at the contact itself. Its socket contact is the **one pair** on which the ≤50mΩ / fretting spec is genuinely binding (OI-HEXTILE-11: contact noise in a µV chain is not covered by a resistance spec alone). |
| PD-PAIR (tile) | PD1 (forward-emission) + PD2 (scalp-facing backscatter) photodiode pair | per optical tile | **PD1 sits at the reserved lattice centre (site 0)** — identical position on every type, already excluded from emitter placement, at maximum optical symmetry. **PD2 co-located with PD1 in XY on the opposite (scalp-facing) copper layer** — co-location is load-bearing, not incidental: the PD1/PD2 ratio is only a valid fouling-vs-ageing discriminator if both sample the same optical path. **InGaAs (Hamamatsu G12180-010A class) on ALL tile types**, not silicon-on-base-tiles — one PD SKU, no per-type calibration distinction. 1.6mm annular pad, hard gold ≥0.5µm cobalt-alloyed. **The hub-side TIA gain-switch requirement rev 1 recorded is deleted, not relocated** — gain is fixed on-module to the PD actually fitted. **Cost consequence is programme-level: ~$10 of the ~$11.53/tile driver+metering BOM is these two PDs, ~$920 at 80 sockets against a $405 Home Standard BOM (OI-HEXTILE-06).** |

## D. Control hub enclosure

| ID | Part | Qty | Key CAD constraints |
|----|------|----|----------------------|
| HUB-HOUSING | Hub enclosure housing (upper/lower shell) | 1 | Externally mounted at headset occipital arch, outside the shielded shell envelope (working placement assumption — hub-to-shell mechanical interface **still not CAD-locked**, OI-HTOOL-01). CFRP-filled polymer, matches probe-dock/anchor-boss/fan-door features below as a single mould (no separate variant tool). **New thermal load:** the 24 V vault boost (15–20 V → 24 V, ~35 W, ~1.46 A) is sited **on the Hub PCB** (OI-HUB-C19, provisional) — magnetics kept outside the shielded envelope away from the fluxgates, at a cost of ~1.8 W conversion loss on the fan-served side. Confirm hub thermal headroom for that 1.8 W against the F-04 fan/heatsink path. |
| PROBE-DOCK | Intranasal probe dock (bilateral probe-tip receptacles + central Y-junction saddle) | 1 | Saddle radius sized to Y-junction body OD (not lead OD) so the junction bears its own weight in storage. Retention ≤2N insertion/extraction (passive holster, no keying — probe tips are identical). |
| PROBE-DOCK-PAD | Probe-tip receptacle silicone insert pad | 2 | Shore 30–40A, protects probe-tip PD window + optical-code/pogo-pin authentication contacts from abrasion. |
| PORT-COVER (hub) | Hub port cover (USB-C, DFU/service) | 2 | Tethered to PORT-COVER-ANCHOR (shell, §A) via molded boss (Ø1.0mm, 0.5mm protrusion), ≤20mm free tether length — **must be geometrically incapable of reaching the fan-intake grille** (FAI-HTOOL-02 BLOCKING; needs the full 3D reachable-envelope sweep in every hub orientation, OI-HTOOL-05). |
| HUB-CHANNEL | Boa cable channel continuation through hub housing | 1 | Continues the shell's enclosed PTFE-lined channel with no cross-section discontinuity. Min bend radius ≥12× Boa lace OD at every turn (OI-HTOOL-02: exact lace OD still pending supplier datasheet). No turns <90°. Thermally/mechanically isolated from the fan/heatsink cavity. |
| FAN-DOOR | Fan/heatsink tool-free access door | 1 | Two diagonally-opposed quarter-turn captive fasteners (door cannot open with only one released). Behind-door intake/exhaust louvre slot width ≤6mm (IEC 60529/60601-1 finger-probe safety). Gasket/ingress class **still not specified** (OI-HTOOL-03 — hub environmental rating undetermined). |
| HEATSINK-ASSY | Fan-cooled heatsink assembly (BN-boss thermal export terminus) | 1 | External terminus of the BN-filled conductive thermal-export path from module junctions; kept outside the sealed/shielded interior to preserve the shield/IP moat. Cavity geometry provisional pending verification-grade CFD (OI-HTOOL-04). **Now on the critical path for an electrical decision:** NP-HW-HEXTILE-001 D-4 (TIA + ADC on-module) is deferred *pending the module heat-sink / helmet-cooling design* — the gate is continuous on-tile dissipation inside the 42 °C face / 62 °C junction envelope, plus ADC drift 25→62 °C against the ±15% dose claim (OI-HUB-C17c). |
| SUPERCAP-MOUNT | Supercapacitor retention/mount (22F) | 1 | Absorbs LED duty-cycle transients; hub NTC thermistor co-located for aging estimation. |

## E. Visual stimulation (goggle) assembly

| ID | Part | Qty | Key CAD constraints |
|----|------|----|----------------------|
| LENS-RIM | Lens rim body (standard + EC variants, shared mould) | 2 (L/R) or 1 bridged | Carries the sliding-rail T-slot groove (F-01), 6× N42 magnet pockets (F-03, ≥1mm wall all faces — ME sign-off BLOCKING), and (EC config only) EC driver contact pads (F-05). |
| GOGGLE-ARM | Goggle arm body | 2 (L/R) | Carries the mating rail tongue (F-02), lens-rim-guard tether anchor hook (F-07, ≥4mm clearance from shade body, ≤45mm tether free length, routed clear of the wearer's visual field), and (EC config only) spring-pin bores (F-06). Identical tooling for standard and EC configs — EC pins populated only for EC orders. |
| LENS-SUBSTRATE | Lens substrate (standard: hard-coated PC or COC; EC: bistable electrochromic film + carrier) | per lens | AgNW outer conductive coating (replaces ITO — 5–10% strain tolerance vs ITO's 0.5%), 3–5µm hard coat over AgNW (and over EC film on EC inner face), inner PDMS diffuser (plasma-activated, same process family as tile PDMS but requalified on PC substrate — not transferable from PI-substrate qualification without a coupon test). 108 micro-LEDs/lens, 6 zones/eye, silicone potting (1,800+ thermal cycles). |
| SHADE-S1 | S1 opaque shade | 1 (in box) | <0.5% VLT. 6 mating magnet pockets (F-04), reversed polarity vs. LENS-RIM. |
| SHADE-S2 | S2 polarizing shade | 1 (accessory) | ~12% VLT. Standard lens only. |
| SHADE-S3-CARRIER | S3 prescription clip carrier | 1 (accessory) | Compatible with both standard and EC lens mounts. |
| S3-RX-INSERT | S3 Rx insert (optician-fitted) | per prescription | 12–24 month renewal item; optician partner network fits into SHADE-S3-CARRIER. |
| LENS-RIM-GUARD | Lens rim guard (tethered) | 1 per lens | Shore 85A UV-stable TPU, clear, snap-fit over rim profile; tether lanyard 1.5mm braided polyester to GOGGLE-ARM anchor hook. |
| GOGGLE-PROXIMITY | IR proximity sensor mount (940nm) + Hall sensor (goggle-lift cutoff) | per goggle | Eye-open detection; Hall sensor triggers instant LED cutoff on lift — one of three independent IEC 62471 MPE-limit layers. |

## F. Neural audio entrainment assembly

| ID | Part | Qty | Key CAD constraints |
|----|------|----|----------------------|
| EARCUP-HOUSING | Over-ear cup housing (planar magnetic 40mm driver + bone-conduction mount) | 2 | Separate rim-mounted subassembly (per §A datum decision — vault stack terminates at the ear cutout already excluded from the lattice); mounts to the outer-bowl (L3) rim. Own environmental seal, separate from the L1/L2/L3 helmet seam. **The ear-cup footprint is one of the two inputs that sets the active-surface boundary (ACT-1, open), which decides which boundary tiles are element-masked.** |
| BAYONET-MOUNT | Aluminium bayonet mount (mesh frame retention) | 2 | Replaces plastic detent (plastic flattens at 500–1,000 cycles); metal-to-metal rated for device lifetime. |
| MESH-FRAME | Snap-in mesh frame (silver-coated nylon, 40dB RF) | 2 | User-replaceable, no tools; driver-impedance monitoring detects both acoustic fouling and RF-shielding loss via the same signal. |
| BONECONDUCT-ISOLATOR | Bone conduction piezo silicone isolator | 2 | Shore 20–30A silicone mount; piezo element is brittle, isolator absorbs impact. Note the bone-conduction **insertion-confirmation tone** was retired by NP-HFE-002 §7.2 — the transducer remains, that one use of it does not. |

## G. PBM intranasal probe

| ID | Part | Qty | Key CAD constraints |
|----|------|----|----------------------|
| PROBE-BODY | Bilateral Y-probe body | 1 | Silicone over-mould at Y-junction (flex without fracture); minimum 20mm bend radius marked on probe shaft. Mates to PROBE-DOCK on the hub when stored (§D). |
| PROBE-DEPTH-RING | Depth-stop ring set (15/20/25mm, silicone) | 1 set per probe | Wear-resistant, over-molded. |
| PROBE-TIP-SENSOR | Probe-tip photodiode + reference LED + optical-code/pogo-pin auth sleeve housing | 2 (bilateral) | Optical code + resistive sleeve authentication (no NFC, no EMF) — mates against PROBE-DOCK-PAD in storage. |
| PROBE-SLEEVE | Hygiene sleeve (consumable) | 30-pack | Single-use; molded to fit over PROBE-TIP-SENSOR housing. |

## H. VNS + HRV auricular clip

| ID | Part | Qty | Key CAD constraints |
|----|------|----|----------------------|
| CLIP-BODY | Auricular clip body (force-contact spring mechanism) | 1 | Force contact confirmation (safety interlock reads impedance before enable). Auricular branch CN X placement. |
| CLIP-HYDROGEL-INTERFACE | PDMS hydrogel pad interface geometry (consumable-facing) | 1 | Mates the 2-pack consumable pad; electrochemical degradation is the expected wear mode (20–40 sessions). |
| CLIP-A1A2-CONTACT | A1/A2 EEG reference contact pad (on clip) | 2 | Uses 2 spare conductors already in the existing 6-pin clip cable — no new cable, only new contact geometry. |

## I. T2-only accessories (non-tile, off-scalp applicators)

| ID | Part | Qty | Key CAD constraints |
|----|------|----|----------------------|
| TMS-COIL-HOUSING | TMS focal figure-8 coil housing | 1 | Mates against TMS-WINDOW (§A) at the coil site; requires the non-conductive CFRP window underneath to avoid eddy-current field loss. Too large for the tile lattice — a dedicated applicator, not a socket-mounted module. **Envelope is genuinely undetermined: TMS coil drive has ZERO power/energy specification anywhere in the document tree** and was never included in the T2-peak figure (which sizes the 1170nm laser zone only). A first-order estimate puts average power during a 10Hz train at ~15–400 W depending where in the stated 0.1–0.5 T range the coil runs — at the top of that range TMS alone exceeds the entire T2-peak envelope. Whether it shares the main rail (capacitor bank + >100 W EPR contract) or runs mains-tethered (excluding TMS from Mode 3) is undecided — **OI-PWR-04. Do not size a coil housing, cable, or thermal path until it closes.** |
| CVNS-NECKBAND | Cervical VNS neckband body | 1 | Neck-worn, off-scalp. Houses gel electrode assembly over the carotid sheath (bilateral or unilateral); safety-MCU cardiac-interlock wiring is electronic, not mechanical, but electrode placement geometry is safety-relevant (carotid proximity). |
| CVNS-GEL-ELECTRODE-HOUSING | Cervical VNS gel electrode housing | 2 (bilateral) | Consumable gel pad (5-pack) mates here. |
| QEEG21-CAP-ELECTRODE | qEEG-21 wet-gel cap electrode housing (T2) | 21 sites (reuses TILE-T1B mechanical family at higher density) | Dual-rated Ag/AgCl per §C; wet-gel vs. semi-dry is a consumable difference, not a new mechanical part. Includes FC3/FC4 (TMS targeting), Oz (photoparoxysmal detection) and A1/A2 (shared with CLIP-A1A2-CONTACT function, on VNS clips). Note HD-tDCS uses 3.5mm electrodes, which matches neither of the two conflicting tDCS electrode-area constants in the codebase (OI-CHARGE-04) — the pad geometry a drawing should carry is not settled. |

## J. Packaging / service

| ID | Part | Qty | Key CAD constraints |
|----|------|----|----------------------|
| CASE-CLAMSHELL | Hard clamshell case (incl. probe dock) | 1 (in box, Home Standard+) | Doubles as shipping container; includes its own probe dock cavity (distinct from the hub's PROBE-DOCK — this one is for transport, not active storage). |
| CABLE-USBC | Braided aramid USB-C cable + dual silicone strain relief | 1 (+ spare in box) | 50,000+ flex-cycle rating at the strain-relief boundary — this is the commodity-cable failure mode being designed out. |
| BOA-REGREASE-KIT | Boa regrease kit applicator | 1 (accessory) | Services BOA-DIAL; no CAD beyond a simple applicator nozzle, listed for completeness. |

## K. L1 socket interconnect — NEW (`np_drv_shell_002`, `np_hw_hub_001` Rev C)

This section did not exist in rev 1. It exists because the retired architecture gave every module a
**tail**; the replacement gives every module a **footprint**, and the wiring that used to be a
harness is now laminated structure inside the inner bowl. These are mechanical parts even though
their content is electrical.

| ID | Part | Qty | Key CAD constraints |
|----|------|----|----------------------|
| CLUSTER-CARRIER | Cluster carrier board (rigid-flex, laminated into L1) | **18** (see §L-3) | Hosts its cluster's ≤8 socket contact arrays plus local conditioning; **fixed — laminated into L1, never moves.** Must stay a different part from the clamp plate on the opposite side of the same cluster footprint: if the carrier were the plate, every module swap would cycle the electrical connection of an entire cluster. **Every socket must sit ≤50mm from its own carrier** (SH2-DRC-03) — that is the whole reason the analog front end moved here. Components mount on the gap-facing (outboard) side, out of the scalp thermal path. Field-replaceable at cluster level: a damaged carrier replaces 7 sockets, not the bowl (OI-SHELL2-08 — service tiering not yet reflected). **Whether it carries a local MCU is unresolved — see §L-4.** |
| CLUSTER-TAIL | Cluster tail (laminated flex, carrier → PAN) | 18 | **Static-only flex — formed once at assembly and never handled again.** ≥**12.5mm** static bend radius at every formed bend (REQ-BR2-01, deliberately conservative against IPC-2223D's ~1.2mm dynamic floor). **No bend within 5mm** of a rigid-flex transition, stiffener edge, boss or carrier edge — bending there delaminates rather than merely fatiguing (REQ-BR2-03). **L1 lamination geometry must ENFORCE the radius by construction** — a tail must not be *capable* of a tighter radius during assembly (REQ-BR2-04). **No formed bend may fall under a clamp-plate footprint or under a socket** (REQ-BR2-05). 12 conductors as drawn (11 if the single cranial enable is adopted, §L-3). |
| PAN | Posterior aggregation node (L1, occiput centreline) | 1 | All 18 cluster tails converge here, co-sited with the BLIND-MATE-BOSS. Hosts the hub-level I2C branch switch, the ADS1299 bank, the tES driver interface and the PDN feed point. Whether the ADS1299 bank actually sits here or moves to the Hub PCB is open (OI-SHELL2-10) — it moves an SPI interface across the boss either way. |
| N1-BUS-PAIR | VLED/PGND broadside-coupled supply pair (laminate core of L1) | per cluster feed | **Every `VLED+`/`PGND` pair broadside-coupled with full overlap across its entire run, dielectric ≤0.2mm, loop area ≤25mm² per cluster feed** (REQ-EMI-06). Same-layer 5mm spacing gives ~1,000mm² and ~1.9µT at 30mm; broadside at 0.1mm gives ~20mm² and ~37nT — fifty-fold, and still the same order as the ambient ELF the cancellation loop nulls. **Two absolute prohibitions:** the shell, shield stack, mu-metal and any DRL-driven structure may **NOT** carry LED or tES return current (REQ-EMI-08 — the shell is bonded to the EEG DRL output, so returning therapeutic-band current through it injects that current into the EEG reference); and N1 may **NOT** form a closed ring around L1 — topology is a tree from the PAN (REQ-EMI-09). |
| N4-GUARD-LANE | Electrode lanes + DRL guard plane (L1 scalp-facing face) | 8 (T1) / ~21 (T2) | Sized by **channel count, not socket count** — any socket may host a T1-B, but only the montage is ever simultaneously electrode-active. Runs under a continuous DRL-driven guard on the quiet face, sandwiched away from N1 by the laminate core. This is the layer assignment that replaces the retired "opposite sides of an 18–22mm shell wall" trick with ~2–3mm of laminate (see §L-6). |

## L. Unresolved issues that constrain mechanical design

Every item below is open on `origin/main` today and would change a drawing. Grouped by how badly
they bite. **None of these is resolved by this document.**

> **How to use a multi-valued row without resolving it.** A cell reading "18 / 16 / 14–15" is inert
> if it leaves the reader to pick silently. Each conflict below is therefore typed:
> **[ENVELOPE]** — safe to *reserve* the worst case and proceed with soft tooling and space claim;
> **[BLOCKING]** — nothing downstream proceeds until it closes. Reserving an envelope is a
> reservation, not a resolution, and does not commit the programme to a value.
>
> | Conflict | Type | Reservation that is safe today |
> |---|---|---|
> | Socket contact count (L-1) | **[ENVELOPE]** for space claim, **[BLOCKING]** for the pattern | Reserve area and clamp-plate load for **18**; do **not** cut the pad/spring pattern |
> | ~~Bezel height (L-2)~~ | **RESOLVED 2026-08-11** | `h_b` = **1.0mm** by principal direction. Residual: BEZEL-1a comfort/fit may still reject it |
> | Cluster-tail connector positions (L-3) | **[ENVELOPE]** | Reserve boss real estate for **20**; do not fix the contact layout (MECH-1 time-box) |
> | Cluster-carrier MCU (L-4) | **[BLOCKING]** | None — board thickness and component height both depend on it |

### L-0. Blocking before any steel is cut

| # | Issue | What it constrains | Owner |
|---|---|---|---|
| L-0a | **REG-1** — the ~80-socket lattice has never been registered to 10-20 against shell CAD. Row boundaries (`ROW_WIDTHS`, `ROW_PITCH_MM_MEASURED = 34.6`) are an unverified scan observation. | Socket positions, therefore cluster partition, therefore clamp-plate shapes, therefore carrier outlines. A re-cut re-tools the inner bowl. | NP-HEX-ZM-001 §7 |
| L-0b | **GATE-1 / GATE-2** — curvature-scan bench and PBM coupling bench not run. | TILE-SHELL cap radius and the Δs≈1.04mm mismatch budget; go/no-go on the whole tile geometry. | NP-HEX-ZM-001 §7 |
| L-0c | **ACT-1 / ACT-2** — the active-surface boundary has never been set deliberately from the ear-cup footprint + clinical coverage targets. | Which boundary tiles are element-masked; interacts with EARCUP-HOUSING placement. | NP-HEX-ZM-001 §7 |
| L-0d | **PDMS 200-cycle IEC 60068-2-14 qualification** — still BLOCKING for production, inherited unchanged. | PDMS-WINDOW release. | OI-HEXTILE-12 |
| L-0e | **CFRP shell slot rim Ra ≤ 1.6 µm supplier confirmation** — still open (RISK-20, BLOCKING). | SHELL-OUTER supplier qualification. | pending-decisions.md |

### L-1. Socket contact count — three live numbers

| Source | Count | Force/module @0.3–0.5N | Per 7-tile plate |
|---|---|---|---|
| NP-DRV-SHELL-002 §5.1 (as written) | **18** | 5.4–9.0 N | 37.8–63.0 N |
| NP-HW-HEXTILE-001 D-5 §7.2 | **16** | 4.8–8.0 N | ~34–56 N |
| NP-HW-HUB-001 §7.5.2 synthesis (if D-4 deletes network N3) | **15** (AGND kept) / **14** (dropped) | 4.2–7.5 N | 29.4–52.5 N |

This is not bookkeeping. **Contact count is an accessibility variable**: OI-SHELL2-03 asks for 18→12
because that would cut clamp-plate load by a third, against RISK-22's one-handed-input-force intent
for Parkinson's H&Y II–III and post-stroke users. The synthesis buys 17–22%, not 33% — **the
remaining gap stays with MECH-2 and the HFE formative.** It also decides the pad-array footprint on
the tile's back face and the socket's spring-array tooling. **Blocked behind OI-HUB-C17c**, which is
itself blocked behind the module heat-sink design.

### L-2. Bezel height — **RESOLVED 2026-08-11 (principal direction): `h_b` = 1.0 mm** (closes OI-HEXTILE-01 on the value)

**Why 2.5 mm was never a real competitor.** It is not a calculation. It appears in NP-HEX-ZM-001 §3.1
only as a **table column header** — *"Active coverage (2.5 mm bezel)"* — i.e. an assumed input to the
coverage arithmetic, with no derivation anywhere in that section. NP-HW-HEXTILE-001 §3 then carried it
forward explicitly as *"the conservative choice of the two conflicting figures."* A conservative
carry-forward of an undefended assumption is not a second opinion. **1.0 mm is the only value in the
tree that was derived**, from a resistance network and an areal flux budget (NP-THERM-BEZEL-001 §4.2–4.5).

**The airflow condition — tested, and it resolves in favour of 1.0 mm.** The bezel gap is *not* a
forced-airflow channel and was never modelled as one: NP-THERM-BEZEL-001 §4.2 treats it as still air
(`R_gap` = `s`/`k_air`), a **conduction decoupler**. Forced convection lives entirely on the **outward**
path — junction → BN-filled boss → socket → inter-bowl → shell → fan/vent — and NP-THERM-CFD-R1-001 §1
states the design *"keeps the junction throttle-free **without ventilating the shielded interior**"*;
the inter-bowl gap is explicitly **stagnant** (0.231 m²K/W). So the bezel obstructs no airflow at any
height. **Affirmatively, 1.0 mm is what makes forced cooling work:**

| `h_b` | `R_gap` (still air) | Forced-convection `R_conv` (h = 25–100 W/m²K) | Which sink does heat prefer? |
|---|---|---|---|
| 0.6 mm | 0.0231 | 0.010–0.040 | **Straddles** — the scalp is competitive as a sink |
| **1.0 mm ★** | **0.0385** | 0.010–0.040 | **Outward path is at-or-better across the whole forced range** |

That is precisely the condition NP-THERM-BEZEL-001 §4.4 names as load-bearing (*"without airflow the
scalp is the lower-resistance sink and heat prefers the patient"*). **The larger bezel is the
patient-protective direction, and it is the direction that lets the fan do its job.**

**Third independent confirmation:** the thermal CFD case matrix (NP-THERM-CFD-001 §6) sweeps bezel
**{0.6, 1.0 mm}** only. 2.5 mm appears in no thermal analysis in the tree.

**Consequences to propagate — flagged for the owning documents, not corrected here:**

| # | Consequence | Direction |
|---|---|---|
| a | Tile active field `A_a` rises 10.61 → **12.51 cm²** (W_a = 38.0 mm) | +17.9% |
| b | **NP-HW-HEXTILE-001 §3 states "+14.5%, A_a rises to 12.15 cm²" for the 1.0 mm case. That back-solves to a 1.27 mm bezel, not 1.0 mm** — a small arithmetic slip in that document. Raise against its owner | correction needed |
| c | T1-A per-channel irradiance at 150 mA falls **403 → ~342 mW/cm²**. HEXTILE §4.3.1's design property — *"full drive at the top of the L70 current window equals the firmware-enforced peak ceiling"* — now needs **~176 mA**, not 150 mA, to reach 400 mW/cm². Still inside the 120–180 mA L70 window, but at 98% of its top. **The property survives with almost no headroom; this should be re-checked when emitters are actually selected (OI-HEXTILE-02)** | tighter |
| d | T1-C three-channel aggregate falls **566 → ~480 mW/cm²** against the 600 mW/cm² ceiling — margin **5.7% → 20%** | better |
| e | Inter-tile seam falls from 5.0 mm to **2.0 mm** of unpopulated width between neighbouring tiles — now **below** the 3.80 mm intra-tile pitch instead of above it. HEXTILE §4.4's *"whole-vault uniformity is a bezel problem"* resolves in the good direction | better |
| f | Electrode-pod travel consumed rises 5% → **8%** of one-sided travel | accepted |

**Still open, and not closed by this decision:** BEZEL-1a (comfort/fit test 0.6 vs 1.0 mm across
52–62 cm heads — it can still reject 1.0 mm on point-load or fit-gap grounds, with 0.6 mm as the
fallback at a tighter ~44 °C face ceiling) and BEZEL-1b (electrode-pod aperture stays bezel-free and
seats at 80–120 g with the offset).

> **Scope note on NP-THERM-BEZEL-001's credibility.** Its §2 optical argument carries a stale
> parenthetical — *"600 LEDs"*, the retired 66 × 78 mm module's count, against the current 90 emitters.
> It does **not** enter the calculation: the spreading term uses the uniform-disk radius a = 20 mm,
> which is the current 40 mm tile. The bezel conclusion is areal (mW/cm², m²K/W) and therefore
> module-size independent. Do not discount the document for that line — but see L-10a for the part of
> it that genuinely is superseded.

### L-3. Cluster count propagation — 18 vs 12 vs 10 vs 16 (OI-HEXTILE-14, OI-HEXTILE-10)

The count under the standing decisions is **18**. Peer documents are still sized off older figures:

- NP-DRV-SHELL-002 §7.1 provisions **12 cluster-tail connectors, 16 positions** — 18 does not fit;
  and its §3.4 `8 branches × ≤2 clusters = 16` I2C tree cannot reach 18.
- NP-HW-HUB-001 §6.3 sizes the (now-deleted) gain switches at **10**.
- NP-HEX-ZM-001 §5.4a's MECH-2 table prices the flower at **12 boards / $76.08**; actual is
  **18 / $114.12**.

Recommended but **not decided**: provision **20** connector positions at the boss (adding tails is
the cheap axis — widening one costs 16 hub pins), adopt the 32-segment I2C tree, and — if the single
Class C `NP_SAFETY_EN_PBM_CRANIAL` enable survives safety review — drop `SAFE_EN_n` from the tail
(12 → 11 conductors), which is what makes a multi-drop trunk possible and the connector count
**insensitive to a future REG-1 re-cut**. Until it closes, the boss contact layout is unfixed — and
the boss is tooled by **MECH-1**, which is why OI-SHELL2-02 is explicitly time-boxed.

### L-4. Does the cluster carrier hold an MCU? (OI-HUB-C15, OI-HUB-C17)

NP-HW-HUB-001 §3/§8.1–8.3 describes a distributed **cluster-controller** tier with an STM32G071
(UFQFPN32) per board. Its own §7.5.3 — written after C17a adopted NP-DRV-SHELL-002's architecture —
says the carrier stays *"passive-plus-switches with no MCU."* OI-HUB-C15 flags the §3/§5.2/§8 rewrite
as pending. **Mechanical consequence: board thickness, component height on the gap-facing side, and
local dissipation inside the inter-bowl gap all differ.** Do not fix the carrier envelope yet.

### L-5. The tile cost/population decision (OI-HEXTILE-06 + OI-HEXTILE-09)

The power envelope permits **~6 concurrent tiles, not 80** — the lattice buys *placement freedom*,
not deliverable dose. Populating all 80 sockets with ~$11.53/tile of driver and metering hardware
costs **~$920/headset** against a $405 Home Standard BOM, and ~$10 of that $11.53 is the two InGaAs
photodiodes. A 20–30-tile build retains full protocol flexibility at a quarter of the cost.
**Mechanically this decides BLANK-PLUG volume, mould cavitation for TILE-SHELL, and packaging.**
It must be decided jointly with OI-HEXTILE-09 (there is today no global concurrent-power governor —
a protocol may name 40 sockets and brown out the rail).

### L-6. EEG artifact — the mechanism is gone, the threshold is not

The retired ≥15mm PBM-to-EEG separation was met geometrically, by putting module FPCs and EEG cables
on opposite sides of an 18–22mm shell wall. **That trick is unavailable** — EEG electrodes now live
inside T1-B tiles on the same L1 carrier as PBM drive current, and the best geometric separation
available within L1 is the ~2–3mm laminate wall. **NP-DRV-SHELL-002 §9.1 states it in its own words:
*"≥15 mm is not achievable and this document does not claim it."*** (This list reports that finding;
it did not make it.) The
<5 µVpp threshold is retained verbatim as SH2-DRC-16 and is now *harder to pass and more important,
because no geometric margin backs it up*. On-tile decoupling cannot help: holding a 40Hz/25%-duty
pulse within 100mV droop needs **15.6 mF per tile**, which is not placeable on a 40mm tile — the
0.5–100Hz modulation *is* the therapy, in exactly the EEG and ELF bands. It must be handled
geometrically (L-K N1-BUS-PAIR) and computationally, never by filtering. Unresolved: the fluxgate
self-field budget the N1 bus must stay within (OI-SHELL2-07) — SH2-DRC-17 has no pass criterion yet.

### L-7. Hub enclosure — three specification gaps still open

- **OI-HTOOL-01** — the hub-to-shell mechanical interface (mounting, cable entry, strain relief, EMI
  bonding at the shell boundary) is **not CAD-locked**. The whole §D placement is a working assumption.
- **OI-HTOOL-02** — the Boa lace OD datasheet is still not on file, so HUB-CHANNEL's minimum bend
  radius remains a 12×-OD placeholder.
- **OI-HTOOL-03** — the hub's environmental (ingress) rating has **never been set by any document**,
  so FAN-DOOR's gasket class is undetermined. (CLAUDE.md's IPX4 references are scoped to the socket
  swap seam, not the hub enclosure.)
- **OI-HTOOL-04** — fan/heatsink cavity geometry awaits verification-grade CFD; now also gating
  OI-HUB-C17c (see HEATSINK-ASSY).

### L-8. Power/thermal envelopes that a mechanical designer cannot yet size against

- **OI-PWR-04 — TMS has no electrical specification at all**, and the T2-peak 70–74 W figure never
  included it. Estimated average power spans ~15–400 W. Capacitor bank vs mains tether is undecided.
  **TMS-COIL-HOUSING cannot be sized.**
- **OI-PWR-01** — the "~6 concurrent tiles" rule is *power*-derived, not thermally derived. An
  independent first-order estimate brackets 4–8; clustered montages (e.g. bilateral DLPFC)
  concentrate residual cavity heat more than the flat rule assumes. Real multi-tile CFD is not done.
- **OI-HUB-C19 residual** — the 24 V boost must be sized and its ~1.8 W confirmed against the hub
  thermal budget; HUB-REQ-C04 requires control-loop bandwidth ≫40 Hz so LED duty modulation never
  becomes 2–40 Hz rail ripple that could masquerade as an EEG entrainment response.

### L-9. Smaller, but they still reach a drawing

| # | Issue | Constrains | Owner |
|---|---|---|---|
| L-9a | T1-B electrode-pod body diameter unspecified | ELECTRODE-POD envelope; T1-B ring depopulation and emitter count | OI-HEXTILE-05 |
| L-9b | Emitter parts not selected; V_f/flux are design targets, not datasheet values. The NIR window is itself contested — no high-power part sits inside 808–830 nm | Tile string topology, sense-resistor values, rigidizer layout | OI-HEXTILE-02, OI-LED-W1, OI-LED-01 |
| L-9c | Intra-tile irradiance uniformity deliberately **not asserted** — needs beam angle, PDMS scattering, window standoff (SCAN-1) | GATE-2 bench design; PDMS diffuser spec | OI-HEXTILE-04 |
| L-9d | Pogo contact qualification in the µV EEG path: ≤50 mΩ over ≥500 cycles is necessary but does not cover contact *noise* | SOCKET-CONTACT / AG-AGCL-CONTACT acceptance | OI-HEXTILE-11 |
| L-9e | Port-cover tether reachable-envelope 3D sweep not done in every hub orientation | PORT-COVER (hub) tether length — BLOCKING for FAI-HTOOL-02 | OI-HTOOL-05 |
| L-9f | tDCS electrode area diverges 25 cm² (safety MCU) vs 35 cm² (app), and HD-tDCS uses 3.5 mm electrodes matching neither | QEEG21-CAP-ELECTRODE / electrode pad geometry on a drawing | OI-CHARGE-04 |
| L-9g | No FMEA entry yet exists for the §4.3 shared-return failure at the boss, alongside FMEA-G07-01 | Design-control completeness for BLIND-MATE-BOSS | OI-SHELL2-09 |
| L-9h | Rigid-flex-into-molded-carrier supplier category not yet in NP-PROC-SUP-001 | SHELL-INNER sourcing — a new CAT alongside moulding / CFRP / PDMS | OI-SHELL2-04 |
| L-9i | Cluster-level repair not yet reflected in service-network tiering | Serviceability documentation for CLUSTER-CARRIER | OI-SHELL2-08 |

### L-10. Gaps with **no owning open item** — raised here, not resolved here

These are not in any document's open-items table. They are recorded because they fall out of crossing
two current documents, and a derived list is where that crossing first becomes visible. **Each needs
an owner assigned; none is decided here.**

| # | Gap | Why it bites mechanically |
|---|---|---|
| L-10a | **The bezel HEIGHT is decided (L-2); the face-temperature CEILING it implies is not.** `NP-THERM-BEZEL-001` Rev A (2026-07-21, untouched since) computes a ~45.5 °C module-face ceiling at `h_b` = 1.0 mm for a tile whose electronics sat on the *hub*. `NP-HW-HEXTILE-001` D-3 has since put a driver on **every** tile and D-4 adds a TIA + ADC, and `NP-HW-HUB-001` OI-HUB-C17c names *"continuous on-tile dissipation inside the 42 °C face / 62 °C junction envelope"* as an open gate. **Choosing 1.0 mm does not close this — it makes the ceiling more attainable (45.5 vs 44 °C) but does not verify it against dissipation the analysis never saw.** THERM-1a (CFD) and THERM-1b (bench) are the gates that close it. | TILE-SHELL face temperature; HEATSINK-ASSY sizing; the D-4 on-module-TIA decision |
| L-10f | **`NP-HEX-ZM-001` §3.1 should be treated as a stale source generally, not just on the bezel.** Two of its figures have already been caught propagating into peer documents as if derived — the *"4–10 clusters"* count (a 30-socket-lattice figure that sized safety-MCU switches, I2C pull-ups and cluster-board BOM at ~80 sockets before it was caught) and the *"2.5 mm bezel"* column header (L-2). The section carries a 30-socket-generation residue that its own in-place warnings only partly fence off. **Anything sourced from §3.1 should be re-derived before it reaches a drawing**, not carried forward on the strength of appearing in a current document. | Any dimension traced to §3.1 |
| L-10b | **No document owns a Z tolerance stack-up for the new seating chain**: tile back-face pads → pogo array → cluster carrier laminated into L1 → molded shell datum. `NP-HW-HEXTILE-001` §7.1 gives a per-contact blind-mate allowance (±0.4 mm lateral, ±0.5 mm Z) and `NP-HELMET-GEOM-001` §2 gives the L0–L3 radial stack, but nothing budgets the chain **between** them across a compliant laminated-in carrier on a doubly-curved bowl. The chain spans NP-DRV-SHELL-002 and NP-HW-HEXTILE-001, which is exactly why neither states it. | Whether pogo working travel is actually consumed by tolerance before it reaches contact force — a first-article risk for SOCKET-CONTACT / CLUSTER-CARRIER / CLUSTER-CLAMP together |
| L-10c | **`NP-HELMET-GEOM-001` §2's L1 module-depth budget assumes a module with a 20-pin FPC tail.** Tiles now have no tail, and L1 has gained 18 carriers, their tails and the PAN. NP-DRV-SHELL-002 OI-SHELL2-09 lists this as a controlled-document update *"this architecture implies but does not make"* — it has not been made. The 18–22 mm L1 subtotal is therefore carried forward unverified. | SHELL-INNER radial depth; whether the carriers fit the inter-bowl gap alongside the clamp plates |
| L-10d | **No owning document was identified in this pass for head-borne mass or centre of gravity.** `NP-PWR-BUDGET-001` owns watts; nothing found owns grams. Eighteen carriers, a PAN and a hub-sited 24 V boost all landed after the last revision of the geometry documents. Recorded as *not found*, not as *does not exist* — if an owner exists, point this row at it. | Fit-system loading (BOA-DIAL, FOREHEAD-BRIDGE, TEMPORAL-WING), comfort, and the 52–62 cm single-SKU claim |
| L-10e | **The socket contact array is a new cleaning and ingress surface** that the rev-1 sealed-ZIF model did not present. SH2-DRC-11 covers IPX4 after 10 swap cycles, but wipe-down chemical compatibility against an exposed gold pogo array is not covered by the existing cleaning guidance (written for PDMS windows and mesh frames). | SOCKET-CONTACT plating and SOCKET-GASKET chemistry |

---

## Notes on architecture currency

1. **Current mechanical basis** for the helmet is the **hex-tile universal-socket architecture**
   (`np_hex_zm_001.md`, `np_helmet_geom_001.md`, `np_hw_hextile_001.md`, `np_drv_shell_002.md`,
   `np_hw_hub_001.md` Rev C): ~80 sockets, one 40mm hex module mould, orientation-only keying, full
   I2C coverage at every socket (SMART-1), **18 mirror-symmetric clusters**, and a cluster-carrier
   interconnect laminated into L1. This supersedes the five-position zone-module architecture
   (`ZM-01`…`ZM-05`) described in `np_hw_fpc_001.md`, `np_tool_zm_sm_001.md` and
   `neurone_shell_fpc_routing_review.docx` (NP-DRV-SHELL-001), and is **still not reconciled** in
   `neurone_tool_shell_001.docx` (written against 5 zone slots).
2. `NP-DRV-SHELL-002` and `NP-HW-HUB-001` Rev C are **DRAFT**; `NP-HW-HEXTILE-001` is a **DESIGN
   STUDY**. They are releasable as design inputs to shell tooling scoping and Hub PCB Rev C; they are
   **not** tooling baselines. An interconnect cannot be more certain than its lattice, and the
   lattice is PROVISIONAL pending REG-1/ACT-1.
3. Where a part is flagged **[PROVISIONAL]**, its parent document is explicitly a design study — do
   not release tooling drawings from these without closing the named gate (REG-1, GATE-1/GATE-2,
   THERM-1, BEZEL-1a/b, EMF-1/2/3, MECH-1, MECH-2).
4. **The `[ARCH-TRANSITION]` flag class from rev 1 is retired.** Both parts that carried it are
   resolved: RIGIDIZER-BOARD favourably (§C), ZONE-PLUG-ANCHOR by supersession (§A).
5. Electronic components are omitted except where they set a mechanical envelope a CAD drawing must
   capture — the rigidizer sub-board outline, the PD footprint, the cluster carrier, and the socket
   contact array.
6. **This document is derived, not authoritative.** Where two current documents disagree, it records
   both and names the open item; it never picks. If a row here contradicts `docs/`, `docs/` wins and
   this file is stale.
