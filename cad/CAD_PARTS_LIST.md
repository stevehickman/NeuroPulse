# NeurOne — Mechanical CAD Parts List

**Source basis:** `CLAUDE.md` §1–§6 (core, locked) + `docs/np_helmet_geom_001.md` (shell layer
stack) + `docs/np_hex_zm_001.md` (hex socket/module architecture, current) + `docs/np_therm_bezel_001.md`
+ `docs/reference/durability-maintenance.md` + `docs/np_tool_hub_001.md` + `docs/neurone_tool_shell_001.docx`
+ `docs/neurone_tool_lens_001.docx` + `docs/np_hw_fpc_001.md` / `docs/np_hw_hub_001.md` / `docs/np_tool_zm_sm_001.md`
(smart-module electrical/mould detail — mechanically still-reusable pieces only, per their own
superseded-banners) + `docs/reference/accessories-roadmap.md`.
**Date:** 2026-07-29
**Status:** Working parts inventory for CAD tooling planning. Several source documents are explicitly
**PROVISIONAL / DESIGN STUDY** (not a locked tooling baseline) — those parts are flagged **[PROVISIONAL]**
below. Two source documents (`np_tool_zm_sm_001.md`, `np_hw_fpc_001.md`) describe a **retired**
five-position zone-module architecture; only the mechanically-reusable sub-elements they still name
(rigidizer cavity concept, InGaAs PD footprint, PDMS process) are carried into this list, flagged
**[ARCH-TRANSITION]** where the current hex-tile architecture hasn't yet re-specified them at the new
form factor. The shell-tooling doc (`neurone_tool_shell_001.docx`) still describes 5 zone-specific
plug/anchor features (ZM-01…05) that predate the ~80-socket hex-lattice redesign; flagged the same way.

**Numbering:** Part IDs are mnemonic (`NP-CAD-<AREA>-<NN>`) for this document and the companion
dependency graph (`cad-parts-dependency-graph.dot`). They are not yet official drawing numbers.

---

## A. Headset shell / chassis (two-nested-bowl architecture)

| ID | Part | Qty | Key CAD constraints |
|----|------|----|----------------------|
| SHELL-OUTER | Outer bowl (structural + EMF shell, L2+L3) | 1 | CFRP structural shell, laminated 0.2mm mu-metal (PETG-encapsulated) + palladium-polyester (0.1mm) + carbon-loaded absorber foam (3.0mm) bonded to inner face, unbroken. Total L2+L3 ≈6mm. Never opened in service. Must carry ≥35–45dB ELF / ≥40–60dB RF combined (prototype acceptance: meet/exceed single-shell baseline). Houses Helmholtz coil formers (fixed geometry, calibration-critical) and the non-conductive CFRP TMS window (T2). Crown exterior ≈189mm (157mm rim→crown datum + ~32mm stack). |
| SHELL-INNER | Inner bowl (module carrier / socket layer, L1) | 1 | Glass-filled PBT (lead) or PA66-GF30 (cost-down fallback) — **non-magnetic, low-eddy** (fluxgate mounts live here). Radial depth ~18–22mm (module body dominates). Holds ~80 keyed hex sockets on parity-alternating lattice, FPC addressing channels, cluster-clamp bosses, fluxgate mounts. Dimensional stability to REG-1 10-20 registration tolerance across 60–110°F. |
| SOCKET-BODY | Hex socket body (×~80, one moulded feature repeated) | ~80 | 40mm flat-to-flat hexagon, circumradius a=W/√3, moulded to curvature-median Rm≈87mm. Asymmetric keying (orientation-only — **no per-type mechanical key**, SMART-1 decision 2026-07-28). Every socket I2C/TIA-capable (full coverage, not a subset). Row widths (parity-alternating) 3-6-7-8-9-8-9-8-7-6-5-4. **[PROVISIONAL — REG-1 gate: 10-20 registration vs. shell CAD not yet fixed.]** |
| BLANK-PLUG | Blanking plug (empty/unpopulated socket cover) | up to ~80 | Overmoulded PC core + LSR seal face, opaque, identical hex footprint to a live module — seals exactly like a filled socket. |
| SOCKET-GASKET | Per-socket co-molded LSR gasket land | ~80 | Shore 40–50A medical silicone, D-section 2.5×2.0mm (legacy 5-slot dimension, carried forward pending hex-lattice FAI), 20% compression when seated. IPX4 target after 10 field swap cycles (FAI-IPX-02, BLOCKING). Isolates a leak to one socket by design (rejected the continuous-grommet-sheet alternative). |
| ELEC-HYDRO-CAP | Electrode hydration/fouling cap (tethered) | per electrode socket | Silicone, WVTR <0.5 g/m²/day, tethered (loss-prevention). Protects sintered Ag/AgCl face when module uninstalled; extends factory-sealed storage to 24+ months. |
| BEZEL | Sacrificial bezel land (optical modules only) | per optical socket | Standing **1.0mm** proud of face (raised from 0.6mm — `np_therm_bezel_001` analysis: thermal decoupling of scalp from module face is the binding constraint, not impact or optics). Electrode-pod aperture is **bezel-free** (s=0 by design). Impact + thermal-decoupling dual function. |
| CLUSTER-CLAMP | Module cluster-clamp assembly (over-center lever-throw + clamp plate + per-module spring plungers) | 4–10 (one per 7-hex "flower" or 3-hex triad) | Push/pull over-center toggle latch — **not a twist cam** (RISK-22 accessibility: Parkinson's H&Y II–III / post-stroke grip). One-handed, ≤ specified low input force via mechanical advantage. Clamp plate carries one spring-loaded plunger per module in the cluster. Validate via HFE formative study (n=5). |
| RIM-LATCH | Rim layer-latch, four-corner (AL/AR/PL/PR) | 4 | Symmetric four-corner clamp at the outer-bowl/inner-bowl parting plane, seated between ear and neck-attachment zones. Integrates **hard-gold-plated BeCu spring fingers** (shield-to-ground bond, ≤50mΩ target) and a **Hall/contact closed-sensor** (safety interlock: no modality enable unless all 4 report closed). |
| RIM-LABYRINTH | Labyrinth rim lip + conductive elastomer bead | 1 (continuous, both bowls) | ≥2× overlap fold; target residual slot ≤ λ/20 at 6GHz ≈ 2.5mm. Conductive elastomer bead closes residual RF gap. Replaceable/tethered service part (elastomer takes compression set over clamp cycles). |
| BLIND-MATE-BOSS | Posterior-center blind-mate sensor/coil connector boss | 1 | NOT a latch — standalone boss at occiput centerline; mates automatically as bowls close, seated by flanking PL/PR latches. Carries fluxgate + Helmholtz harness across the parting plane. |
| FLUXGATE-MOUNT | Fluxgate magnetometer mount (3-axis) | 1 set | Mounts on inner bowl (near scalp — samples the field the wearer experiences). Non-magnetic local structure required. |
| HELMHOLTZ-FORMER | Helmholtz coil former (3-axis pairs) | 1 set | Fixed geometry on outer bowl (co-designed as one magnetic circuit with the mu-metal layer) — geometry is calibration-critical and must not vary clamp-to-clamp. |
| TMS-WINDOW | Non-conductive CFRP TMS window (T2 only) | 1 | Local break in CFRP L3 at coil site; local L2 mu-metal routed around it to prevent eddy-current field loss. |
| EEG-ROUTE-CHANNEL | EEG cable routing channel (molded, outer-CFRP inner surface) | 1 | 8×5mm cross-section, on the **opposite CFRP inner surface** from the zone-module FPC bundle (which routes on the scalp-facing inner surface) — ≥2mm wall separation at any common cross-section. DRC-18: ≥15mm point-to-point separation from any zone-module FPC conductor; <15mm requires a grounded 35µm Al foil barrier tied to chassis GND. 3 snap-in cable retention clips (~80mm pitch, ≤3N opening force). Branch feed-throughs at each electrode pod (5mm dia., 0.5×45° chamfer). |
| ZONE-PLUG-ANCHOR | Zone-slot plug anchor post | 5 legacy / **needs re-spec to socket count** | **[ARCH-TRANSITION]** Ø4.0×3.5mm boss, through-slot for Shore 30A silicone tether loop, ≥500 tug cycles @5N, color-coded per zone (legacy ZM-01…05 5-color scheme). Written against the retired 5-slot architecture — needs re-derivation against the ~80-socket hex lattice (or superseded entirely by the tethered `ELEC-HYDRO-CAP` / `BLANK-PLUG` model, which already has its own retention approach). |
| PORT-COVER-ANCHOR (shell) | Accessory port cover anchor post (USB-C, hub L/R accessory ports) | 3 | Same boss geometry as ZONE-PLUG-ANCHOR; Shore 40A TPE tether, neutral (non-zone) color. ≥15mm tether clearance from port centreline (full-size USB-C plug insertion with cover hanging clear). |
| TEMPORAL-WING-BOSS | Temporal wing anchor boss (mastoid pad mount) | 2 (bilateral) **[PROVISIONAL]** | Recessed snap-fit receiver, 34×34×5mm envelope, 30mm diameter opening (mates 40Hz mastoid LRA pad accessory). Snap release 3–6N axial. Withstands 15N axial / 10N lateral shear (FEA before first cut). Flush/recessed when pad not attached; tethered dust cover when empty. Non-labelled in Rev A tooling pending HOPE Phase 3 clinical result. |

## B. Fit system

| ID | Part | Qty | Key CAD constraints |
|----|------|----|----------------------|
| BOA-DIAL | Boa occipital dial + enclosed PTFE-lined cable channel | 1 | 10cm range, 0.5mm/click, 50,000-cycle rated. Channel fully enclosed (prevents hair entanglement) — must be molded in from first shell tooling cut. |
| BOA-CABLE | Boa replacement lace/cable + hook tool | 1 (+ spare in box) | Matches Boa-supplier lace OD (datasheet pending — governs downstream hub-channel bend radius, see HUB-CHANNEL). |
| FOREHEAD-BRIDGE | 5-position forehead bridge | 1 | 5mm adjustment steps. |
| TEMPORAL-WING | Temporal stability wing (snap-on) | 2 | Stored in hub dock when detached. Carries TEMPORAL-WING-BOSS as an integral molded feature. |
| ELECTRODE-POD | Spring-decoupled electrode pod (housing + spring + plunger) | per EEG/electrode site (8–9 T1, ~19 T2 scalp) | 80–120g contact force, ±12mm travel, Shore 30A silicone, independent of Boa dial tension. Bezel (optical sockets) consumes 5–8% of one-sided travel where co-located; electrode-pod aperture itself is bezel-free. |

## C. Zone modules — universal hex-tile family (current architecture, `np_hex_zm_001`)

One mechanical mould, 40mm flat-to-flat hexagon, orientation-keyed only; "type" is an element-population/FPC difference, not a separate mechanical part family.

| ID | Part | Qty | Key CAD constraints |
|----|------|----|----------------------|
| TILE-SHELL | Universal hex-tile shell (shared mould, all T1/T2 types) | 1 mould, many instances | 40mm flat-to-flat, curvature-median Rm=87mm cap. Worst-case module-to-scalp mismatch Δs≈1.04mm at 40mm, absorbed by PDMS window standoff + ≤0.8mm compliant gasket. **[PROVISIONAL — GATE-1/GATE-2 curvature-scan + PBM-coupling bench not yet passed.]** |
| TILE-T1A | T1-A base-PBM element population (660–670+808–830nm LEDs, PD1/PD2, NTC) | majority of sockets | No EEG. Bulk scalp PBM coverage. |
| TILE-T1B | T1-B EEG/electrode element population (dual-rated Ag/AgCl electrode + reduced-count 660/808 PBM + PD + NTC) | ~8–9 (T1 montage sites) | Electrode is dual-rated: records EEG **and** delivers BES/tACS/tDCS — no separate stim-only electrode part. Reduced LED count for pod clearance (PBM dose island, accepted). |
| TILE-T1C | T1-C smart 1064nm element population (660/808/1064nm LEDs + on-module driver + InGaAs PD1/PD2 + NTC) | protocol-defined depth zones | Carries the rigidizer sub-board (RIGIDIZER-BOARD) and InGaAs photodiodes (see below). No EEG (grow-to-4-type option deferred). |
| TILE-T2D | T2-D 1170nm deep-PBM laser element population (laser diode + TEC + laser driver) | T2 depth zones | Non-tile-shared internals (laser ≠ LED driver); still occupies the universal TILE-SHELL footprint. |
| RIGIDIZER-BOARD | On-module driver rigidizer sub-board (ATtiny402 I2C slave + 3× N-FET driver, FR4) | per TILE-T1C / TILE-T2D-class smart tile | **[ARCH-TRANSITION]** Legacy dimension (66×78mm module cavity, 24×16×3.5mm cavity clear, 22×14×0.8mm board) is **not yet re-validated** against the 40mm hex-tile footprint — this is an open mechanical gap, not merely a superseded spec. 1.0mm thermal vent to lateral face (ATtiny402 self-heat <50mW). 2-point 1.0mm alignment boss to board mounting holes. |
| PDMS-WINDOW (tile) | PDMS optical window + 75nm SiO₂ interlayer, O₂-plasma activated | per optical tile | >90% T at 660–1170nm. 200-cycle IEC 60068-2-14 thermal-cycle qualification **BLOCKING** for production. Anti-fouling (hydrophilic, repels sebum). |
| AG-AGCL-CONTACT | Sintered Ag/AgCl electrode contact (dual-rated: EEG record + tES deliver) | per TILE-T1B | Low/stable half-cell potential, chloride-corrosion resistant, ISO 10993. Seal at pod-perimeter gasket, not at the contact itself. |
| PD-PAIR (tile) | PD1 (forward-emission) + PD2 (scalp-facing backscatter) photodiode pair | per optical tile | PD1/PD2 ratio separates PDMS fouling (PD1↓, PD2 stable) from LED aging (both↓) — eliminates the 3-year service calibration visit. Silicon PDs on base tiles; InGaAs (Hamamatsu G12180-010A class) on 1064nm smart tiles — **different responsivity drives a TIA gain-switch requirement on the hub/socket electronics side, not a mechanical CAD difference at the tile.** |

## D. Control hub enclosure

| ID | Part | Qty | Key CAD constraints |
|----|------|----|----------------------|
| HUB-HOUSING | Hub enclosure housing (upper/lower shell) | 1 | Externally mounted at headset occipital arch, outside the shielded shell envelope (working placement assumption — hub-to-shell mechanical interface **not yet CAD-locked**, OI-HTOOL-01). CFRP-filled polymer, matches probe-dock/anchor-boss/fan-door features below as a single mould (no separate variant tool). |
| PROBE-DOCK | Intranasal probe dock (bilateral probe-tip receptacles + central Y-junction saddle) | 1 | Saddle radius sized to Y-junction body OD (not lead OD) so the junction bears its own weight in storage. Retention ≤2N insertion/extraction (passive holster, no keying — probe tips are identical). |
| PROBE-DOCK-PAD | Probe-tip receptacle silicone insert pad | 2 | Shore 30–40A, protects probe-tip PD window + optical-code/pogo-pin authentication contacts from abrasion. |
| PORT-COVER (hub) | Hub port cover (USB-C, DFU/service) | 2 | Tethered to PORT-COVER-ANCHOR (shell, §A) via molded boss (Ø1.0mm, 0.5mm protrusion), ≤20mm free tether length — **must be geometrically incapable of reaching the fan-intake grille** (FAI-HTOOL-02 BLOCKING). |
| HUB-CHANNEL | Boa cable channel continuation through hub housing | 1 | Continues the shell's enclosed PTFE-lined channel with no cross-section discontinuity. Min bend radius ≥12× Boa lace OD at every turn (OI-HTOOL-02: exact lace OD pending supplier datasheet). No turns <90°. Thermally/mechanically isolated from the fan/heatsink cavity. |
| FAN-DOOR | Fan/heatsink tool-free access door | 1 | Two diagonally-opposed quarter-turn captive fasteners (door cannot open with only one released). Behind-door intake/exhaust louvre slot width ≤6mm (IEC 60529/60601-1 finger-probe safety). Gasket/ingress class **not yet specified** (OI-HTOOL-03 — hub environmental rating undetermined). |
| HEATSINK-ASSY | Fan-cooled heatsink assembly (BN-boss thermal export terminus) | 1 | External terminus of the BN-filled conductive thermal-export path from module junctions; kept outside the sealed/shielded interior to preserve the shield/IP moat. Cavity geometry provisional pending verification-grade CFD (OI-HTOOL-04). |
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
| EARCUP-HOUSING | Over-ear cup housing (planar magnetic 40mm driver + bone-conduction mount) | 2 | Separate rim-mounted subassembly (per §A datum decision — vault stack terminates at the ear cutout already excluded from the lattice); mounts to the outer-bowl (L3) rim. Own environmental seal, separate from the L1/L2/L3 helmet seam. |
| BAYONET-MOUNT | Aluminium bayonet mount (mesh frame retention) | 2 | Replaces plastic detent (plastic flattens at 500–1,000 cycles); metal-to-metal rated for device lifetime. |
| MESH-FRAME | Snap-in mesh frame (silver-coated nylon, 40dB RF) | 2 | User-replaceable, no tools; driver-impedance monitoring detects both acoustic fouling and RF-shielding loss via the same signal. |
| BONECONDUCT-ISOLATOR | Bone conduction piezo silicone isolator | 2 | Shore 20–30A silicone mount; piezo element is brittle, isolator absorbs impact. |

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
| TMS-COIL-HOUSING | TMS focal figure-8 coil housing | 1 | Mates against TMS-WINDOW (§A) at the coil site; requires the non-conductive CFRP window underneath to avoid eddy-current field loss. Too large for the tile lattice — a dedicated applicator, not a socket-mounted module. |
| CVNS-NECKBAND | Cervical VNS neckband body | 1 | Neck-worn, off-scalp. Houses gel electrode assembly over the carotid sheath (bilateral or unilateral); safety-MCU cardiac-interlock wiring is electronic, not mechanical, but electrode placement geometry is safety-relevant (carotid proximity). |
| CVNS-GEL-ELECTRODE-HOUSING | Cervical VNS gel electrode housing | 2 (bilateral) | Consumable gel pad (5-pack) mates here. |
| QEEG21-CAP-ELECTRODE | qEEG-21 wet-gel cap electrode housing (T2) | 21 sites (reuses TILE-T1B mechanical family at higher density) | Dual-rated Ag/AgCl per §C; wet-gel vs. semi-dry is a consumable difference, not a new mechanical part. Includes FC3/FC4 (TMS targeting) and Oz (photoparoxysmal detection) and A1/A2 (shared with CLIP-A1A2-CONTACT function, on VNS clips). |

## J. Packaging / service

| ID | Part | Qty | Key CAD constraints |
|----|------|----|----------------------|
| CASE-CLAMSHELL | Hard clamshell case (incl. probe dock) | 1 (in box, Home Standard+) | Doubles as shipping container; includes its own probe dock cavity (distinct from the hub's PROBE-DOCK — this one is for transport, not active storage). |
| CABLE-USBC | Braided aramid USB-C cable + dual silicone strain relief | 1 (+ spare in box) | 50,000+ flex-cycle rating at the strain-relief boundary — this is the commodity-cable failure mode being designed out. |
| BOA-REGREASE-KIT | Boa regrease kit applicator | 1 (accessory) | Services BOA-DIAL; no CAD beyond a simple applicator nozzle, listed for completeness. |

---

## Notes on architecture currency

1. **Current (2026-07-20/28) locked mechanical basis** for the helmet is the **hex-tile universal-socket
   architecture** (`np_hex_zm_001.md`, `np_helmet_geom_001.md`): ~80 sockets, one 40mm hex module mould,
   orientation-only keying, full I2C/TIA coverage at every socket (SMART-1). This supersedes the
   five-position zone-module architecture (`ZM-01`…`ZM-05`) described in `np_hw_fpc_001.md`,
   `np_hw_hub_001.md`, and `np_tool_zm_sm_001.md`, and **not yet fully reconciled** in
   `neurone_tool_shell_001.docx` (still written against 5 zone slots).
2. Where a part is flagged **[PROVISIONAL]**, its parent document is explicitly a design study, not a
   locked tooling baseline — do not release tooling drawings from these without closing the named gate
   (REG-1, GATE-1/GATE-2, THERM-1, BEZEL-1a/b, EMF-1/2/3, etc.).
3. Where a part is flagged **[ARCH-TRANSITION]**, the mechanical concept is believed still valid but its
   *dimensions* were derived against the retired form factor and have not been re-validated at 40mm hex
   scale — this is open engineering work, not merely a documentation update.
4. Electronic components (resistors, ICs, PCB copper features) are omitted except where they set a
   mechanical envelope a CAD drawing must capture (e.g., the rigidizer sub-board outline, PD footprint).
