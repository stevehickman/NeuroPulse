---
task: Hexagonal standardized zone-module redesign (Option A rigid; Option B future)
project: NeurOne
slug: hex-zone-module
effort: E4
phase: plan
progress: 18/91
mode: design-study
started: 2026-07-15
updated: 2026-07-20
---

# Hexagonal Zone-Module Redesign — ISA (NP-HEX-ZM)

> Design-study ISA. This articulates the ideal state of the redesigned zone-module
> subsystem. It is NOT a locked NP-FW/NP-TOOL baseline — the go/no-go gate in
> `## Decisions` governs promotion to tooling. Engineering narrative + BOM/EMF
> detail live in the companion brief **NP-HEX-ZM-001** (`docs/np_hex_zm_001.md`).

## Problem

Today each snap-in zone module is fitted to the helmet with a position-unique
pin/key shape. That forces NeurOne to manufacture, inventory, and sell a distinct
module SKU per helmet location, and forces customers to stock position-specific
spares. It multiplies tooling (one mold family per unique module), inventory, and
service complexity — for a device whose whole value proposition includes field
upgradeability. The current design is otherwise mature and "locked", but the
per-position-uniqueness is a structural cost the tiling redesign removes.

## Vision

One universal module SKU (single size, single mold) that can be placed in ANY
socket, tiling the helmet interior like a honeycomb. A clinician or user restocks
from a single part number; NeurOne runs one production line for the mechanical
shell of every module; a module carries its own element inventory and announces
it to the helmet on insertion. The euphoric surprise: standardization does not
foreclose the better mechanical answer — the same socket interface that makes
modules universal is exactly what lets a rigid tile today be swapped for a
semi-flex tile tomorrow with zero shell change.

## Out of Scope

- The mechanical decision is NOT re-opened here: Option A (rigid, median-curved
  hexagon) is the committed baseline; Option B (semi-flex + shell-socket-defined
  curvature) is the future path, not this build.
- Goggle/visual micro-LED lenses, intranasal Y-probe, auricular VNS clip, and
  audio cups are NOT cranial-tiled and are out of scope — they keep their current
  form factors.
- Per-element intra-module physical offset mapping (sub-position within one
  module) is out of scope; protocol/group resolution operates at socket+type
  granularity.
- tFUS/LIFU and any non-existing modality are out of scope (see CLAUDE.md §13.2b).
- No change to the UHDR/SHDR data architecture or the safety-MCU interlock model.

## Principles

- **Standardize the sold part, not the geometry.** Variability the head imposes
  is absorbed by the shell (a made-once, non-inventoried part), never by the SKU.
- **Curvature fit is an optical-coupling budget, not an EEG one.** Spring-decoupled
  EEG pods self-accommodate ±12 mm; the module-curvature tolerance is set by PBM
  dose coupling and comfort.
- **A seam is defined by its worst aperture, not its average.** EMF integrity is
  governed by the longest slot in the conductive envelope, not total open area.
- **Fail closed on integrity.** No stimulation/EEG session may run with the
  shielding envelope open or a module un-inventoried.
- **Reliability outranks coverage.** A gain in tile size or coverage that raises
  warranty risk is rejected (principal's explicit ranking).
- **Interface stability is the upgrade path.** The socket contract (mechanical +
  electrical + addressing) is the invariant that lets Option A → Option B be a
  module swap, not a re-architecture.

## Constraints

- One module SKU: all modules identical external size and mount, even when
  element population differs. A module cannot be smaller than the space needed to
  embed its densest element set.
- Two-level HW element address: major = socket (fixed helmet location), minor =
  element on module. Socket field ≥7 bits so addressing never binds before
  geometry (41-tile ceiling; 30 at the 40 mm design point). Sockets
  asymmetrically keyed → single mount
  orientation.
- Rigid (Option A) module curvature is a single fixed radius = curvature-median
  of the vault (R_m ≈ 87 mm).
- Must preserve the measured EMF baseline: ≥35–45 dB ELF magnetic, ≥40–60 dB RF
  (single-shell spec). The redesign may not degrade below it.
- Levers to eject/insert modules may not sit on the tiled interior (they would
  break coverage) and may not pierce the passive EMF shield.
- IPX4 headset target preserved.
- Safety-MCU ownership of all stimulation enables is unchanged.

## Goal

Deliver a validated architecture for a helmet interior tiled with a single
universal rigid hexagonal module SKU (Option A), specified end-to-end —
geometry, 2-level addressing + NVRAM element map (firmware, done), a two-layer
shell whose EMF seam holds the existing shielding baseline, and a documented
Option-B (semi-flex) upgrade path through the unchanged socket interface — such
that the only remaining precondition to tooling is the curvature-scan go/no-go
bench.

## Criteria

### Geometry (Option A rigid)
- [ ] ISC-1: Recommended module flat-to-flat width is 40 mm (±0 design point), vertex span ≈ 46 mm — stated in the brief with the sizing equation.
- [ ] ISC-2: Workable width range 34–46 mm is documented with the floor driver (bezel + embedding; the lever-arm floor is removed by cluster clamps, ISC-33) and ceiling driver (rigid fit) named per bound.
- [ ] ISC-3: Module curvature fixed at R_m = 87 mm (curvature-median); dome depth at W=40 computed (~3.1 mm).
- [ ] ISC-4: Worst-case skull-mismatch sagitta at W=40 is ≤1.1 mm (Δs = (W²/6)·Δκ, Δκ=0.0039 mm⁻¹) and shown absorbed by PDMS window standoff + ≤0.8 mm gasket.
- [ ] ISC-5: Active-coverage fraction at W=40 with 2.5 mm bezel is ≥76%, tabulated across the workable range.
- [x] ISC-6: Module count for a ~420 cm² tileable vault is ⌊420/13.86⌋ = 30 at the 40 mm design point (27–33 for area uncertainty); geometry ceiling 41 at the smallest workable W (34 mm). Derived — not asserted — by `scripts/sync-socket-map.ts`, which fails the build if the shipped lattice disagrees.
- [x] ISC-6.1: The socket count has exactly one definition (`derivedSocketCount()` in `scripts/sync-socket-map.ts`); no doc, zone file, app module, test, or UI string carries an independent literal.
- [ ] ISC-7: Densest module element set (tri-wavelength PBM ~90 elements, or a spring EEG pod) fits within the 40 mm inner field — embedding floor cleared.
- [ ] ISC-8: Anti: no module width is specified small enough that bezel reduces skull element coverage below the single-zone baseline.

### Module-type taxonomy (see brief §4a)
- [ ] ISC-49: T1's cranial modalities are covered by exactly three tile types: T1-A base PBM (no EEG), T1-B EEG/electrode, T1-C 1064 smart PBM (no EEG).
- [ ] ISC-50: T1-A (base, non-EEG) includes 660–670 + 808–830 nm PBM + PD1/PD2 + NTC, no electrode.
- [ ] ISC-51: T1-B carries a dual-rated Ag/AgCl electrode that both records EEG AND delivers BES/tACS/tDCS — so no separate stim-electrode tile type exists.
- [ ] ISC-52: T1-B retains base 660/808 PBM (reduced LED count for pod clearance) so PBM coverage is continuous at electrode sites (no PBM dead spot under electrodes).
- [ ] ISC-53: T1-C (1064 smart) carries 660/808/1064 nm + on-module driver + InGaAs PD1/PD2 + NTC and no EEG; a combined EEG+1064 tile is a deferred grow-to-4 option, built only if 1064 zones and EEG sites overlap.
- [ ] ISC-54: T2 adds exactly one tile type (T2-D 1170 nm laser + TEC); qEEG-21 / HD-tDCS 4×1 / 16-ch clinical tACS reuse T1-B at higher density; TMS coil and cervical VNS are non-tile applicators.
- [ ] ISC-55: Anti: no cranial T1 modality requires a tile type outside {T1-A, T1-B, T1-C}; no scalp T2 modality except 1170 nm requires a type beyond the T1 set.
- [ ] ISC-56: T1-B is allocated ONLY at required electrode positions (~8 for the T1 EEG montage Fp1/2·F3/4·C3/4·P3/4, plus any tES montage sites); the majority of sockets are electrode-free T1-A. A representative build is ~8× T1-B + balance T1-A (+ T1-C at 1–5 depth zones).
- [ ] ISC-57: Anti: no build places an EEG electrode at every socket — EEG-in-every-module (electrode at every location) is rejected as over-provisioning (cost + lost LED area).

### Mixed insertion, placement + protocol maps, cluster clamps (see brief §4a, §5.4a)
- [ ] ISC-58: Tiles are type-agnostic — any type inserts in any socket (orientation-only key, no type keying); identity = socket (fixed position) + module (self-reported type via UID/inventory).
- [x] ISC-59: `np_module_map_check_placement()` validates the live inventory against per-socket type_mask requirements and reports the unmet sockets. — DELIVERED, host-tested.
- [x] ISC-60: Element type NP_ELEM_DUAL_ELECTRODE exists and satisfies BOTH an EEG and a tES placement requirement. — DELIVERED, host-tested.
- [ ] ISC-61: Every protocol carries a required module map — one specifier per socket, either DONT-CARE or a type-subset mask; a protocol is runnable only when the inserted modules match its map (check_placement passes for every non-DONT-CARE socket).
- [ ] ISC-62: NeurOne ships standard maps (All-T1-A; standard T1-A/T1-B mix; deep-PBM with T1-C) and supports user-defined maps; the app enables/disables protocols by matching maps against the current insertion.
- [ ] ISC-63: OTA updates can extend the set of valid module-type specifiers (and push new/updated standard maps) so existing protocols keep matching and new ones can require new types.
- [ ] ISC-64: Visual stimulation is blocked unless an EEG/dual electrode is present at the Oz socket (photoparoxysmal halt <200 ms); a visual-stim build carries ~9 electrode tiles (SW-1 wiring open).
- [ ] ISC-65: tES (BES/tACS/tDCS) is blocked unless electrodes are present at all montage sockets (HD-tDCS 4×1 = 5).
- [ ] ISC-66: Smart-socket coverage decided — all sockets I2C+TIA-capable vs a subset — governing where T1-C can seat (SMART-1).
- [ ] ISC-67: Socket lattice registers to the 10-20 system (8–9 T1, ~19 T2 scalp) within tolerance without violating the coverage/bezel budget (REG-1).
- [ ] ISC-68: PBM dose at electrode sites is lower (T1-B reduced LEDs) but per-tile PD metering stays accurate; firmware compensates within duty/thermal limits or accepts the ±15–25% non-uniformity.
- [ ] ISC-69: Modules are clamped in clusters (3–7) by one over-center lever-throw clamp each (push/pull, not a twist cam) with per-module spring plungers; swapping one module releases only its cluster; a loose/unseated tile is caught by the inventory/contact poll → the placement gate disables dependent protocols (MECH-2).
- [ ] ISC-70: Anti: no socket is type-keyed so a tile type can seat only in position-specific sockets (would reintroduce position-unique SKUs) — except the T1-C smart key per SMART-1.
- [ ] ISC-71: The cluster actuator carries the RISK-22 accessibility intent (Parkinson's H&Y II–III / post-stroke): large easy-grip control; a push/pull over-center lever throw (NOT a twist cam); low input force via mechanical advantage; one-handed; ejector springs self-present the module and the plate auto-reseats the whole cluster — so an impaired user makes ONE coarse low-force action, not N precise placements.
- [ ] ISC-72: HFE formative validates cluster-actuator accessibility with 5 Parkinson's H&Y II–III / post-stroke subjects (NP-TOOL-ZM-001 OI-4 eject-lever study re-pointed at the cluster actuator).
- [ ] ISC-73: Anti: the cluster actuator is not a small recessed twist cam (twisting defeats weak grip / limited forearm rotation / tremor — worse than the per-module lever it replaces).

### Addressing + NVRAM map (firmware — DONE)
- [x] ISC-9: 2-level packed address (7-bit socket : 7-bit element) with pack/unpack round-trip. — `np_hex_addr_pack/unpack`, tested.
- [x] ISC-10: Element type is a 6-bit enum (≤64 types); compile-time asserted. — `np_elem_type_t`, `_np_hexmap_elem_domain`.
- [x] ISC-11: Power-on poll caches per-socket UID+health; a module is re-inventoried ONLY when its UID differs from the one stored for that socket. — `np_module_map_apply_poll`, tests `same-uid → not re-inventoried`, `uid change → re-inventoried`.
- [x] ISC-12: Inventory is fail-closed: bad/oversized/absent inventory leaves the socket not-present. — tests `over-max → CMD_TOO_MANY`, `NULL-cb → NOT_PRESENT`.
- [x] ISC-13: Address→physical resolution returns lobe/side/x-y/type; out-of-range/empty → NOT_PRESENT. — `np_module_map_resolve`, tested.
- [x] ISC-14: Groups resolve from lobe+side (8 predefined L/R × 4 lobes), socket sets, and address sets, with type include/exclude filter (mask 0 = all). — `np_module_map_resolve_group/_predefined`, tests incl. mask-0 both modes.
- [ ] ISC-15: NVRAM blob is CRC-32 integrity-protected; bad magic/version/CRC rejected. — serialize/load done + tested; live Config-partition HAL (OI-HEXMAP-01) is the open integration seam.
- [ ] ISC-16: Real module inventory link (I2C/1-wire `inventory_fn`) is implemented and returns element-type list per minor address.
- [ ] ISC-17: Socket field width ≥7 bits verified so addressing never caps below the geometry ceiling.
- [ ] ISC-18: Anti: no user biology is written by the map — module UID is a component identifier (SHDR-class), nothing is UHDR or keyed to user identity.

### Two-layer shell + EMF seam
- [ ] ISC-19: Shell is two nested bowls: outer = complete EMF envelope; inner = module/socket/cluster-clamp carrier nesting inside it.
- [ ] ISC-20: The full passive stack (CFRP, mu-metal L2, palladium L3, absorber L4) lives UNBROKEN on the outer bowl; the inner bowl carries no passive shield.
- [ ] ISC-21: Module cluster clamps live in the inter-bowl gap (inner-bowl exterior face) and are reached only by unclamping the bowls — they never pierce the outer shield.
- [ ] ISC-22: Layer clamp is a symmetric four-corner pattern (anterior L/R, posterior L/R) at the rim between ear and neck attachment zones; it brackets the front-center and back-center spans with even L/R + front/back force and flanks the forehead bridge.
- [ ] ISC-23: Outer bowl overlaps the inner bowl rim with a labyrinth lip; residual continuous parting-plane slot ≤ λ/20 at 6 GHz (~2.5 mm).
- [ ] ISC-24: Outer-shield-to-system-ground bond crosses the parting plane at the 4 clamp latches via hard-gold BeCu spring fingers / conductive elastomer, ≤50 mΩ.
- [ ] ISC-25: The driven EEG shield (shell bonded to DRL) is preserved: the outer shield is referenced to hub ground through the clamp-point bond when closed.
- [ ] ISC-26: Fluxgate sensors (inner bowl, near scalp) and Helmholtz coils (outer bowl) route to the hub via a standalone posterior-center blind-mate boss (NOT a latch), seated by the flanking PL/PR latches.
- [ ] ISC-27: Layer-closed interlock: a Hall/contact sensor per latch; safety architecture refuses any modality enable unless all latches report closed.
- [ ] ISC-28: Measured attenuation with bowls clamped meets or exceeds the single-shell baseline (≥35–45 dB ELF magnetic, ≥40–60 dB RF) — verified on a prototype.
- [ ] ISC-29: Ground-bond contact resistance is trended in SHDR; a rise flags shield degradation (reuses fleet EMF-attenuation monitoring).
- [ ] ISC-30: The conductive parting-plane gasket is a replaceable/tethered service part (compression-set over clamp cycles).
- [ ] ISC-31: Anti: no session may run with the envelope open — an unclamped or single-latch-open state blocks session start.
- [ ] ISC-32: Anti: no clamp actuator, sensor wire, or fastener creates an un-gasketed aperture through the passive shield.
- [ ] ISC-48: Gasket line-pressure map (FEA or pressure-film bench) shows min seam compression ≥ seal threshold at the back-center (PL–PR) span AND both side (ear) spans under the four-corner AL/AR/PL/PR pattern; a marginal span is fixed by lip/gasket stiffening (or a lateral / restored posterior-center latch), not more corner latches. (Brief §7 EMF-3.)

### Reliability + manufacturing
- [ ] ISC-33: Modules are clamped in clusters (one over-center lever-throw clamp per 3–7-tile cluster, ~4–10 total, with per-module spring plungers) — NOT per-module levers; low one-handed input force (RISK-22 intent), 5–15× fewer moving parts, removing the per-module short-arm problem.
- [ ] ISC-34: Per-hex perimeter gasket seals IPX4 after field swap cycles (per-tile seam-length budget stated).
- [ ] ISC-35: Aggregate whole-vault thermal load with all tiles active stays within the 42 °C scalp limit; per-tile NTC + throttle retained.
- [ ] ISC-36: Single mold produces the universal module shell; element-population variants are FPC/loadings only.
- [ ] ISC-37: Anti: the redesign introduces no new high-cycle flex on rigid components (Option A tiles are rigid; flex reliability is deferred to Option B).

### Decision gate
- [ ] ISC-38: Curvature-scan bench: 5–95th percentile head curvature map validates the Δκ ≈ 0.0039 mm⁻¹ worst case used to size the tiles.
- [ ] ISC-39: PBM optical-coupling bench: a rigid 40 mm coupon at worst-case (temporal) mismatch meets the J/cm² dose-coupling spec.
- [ ] ISC-40: Go/no-go recorded: PASS → tool Option A; FAIL → Option A is DOA and Option B moves up (see Decisions).
- [ ] ISC-41: Option-B upgrade path is documented as a module swap into the unchanged socket interface (no shell re-tool).

### Cross-artifact
- [ ] ISC-42: Design brief NP-HEX-ZM-001 exists and carries geometry tables, EMF seam detail, BOM deltas, and open items.
- [x] ISC-43: Firmware addressing/NVRAM map builds `-Werror`-clean on Cortex-M7 and passes host tests. — 63 checks pass; full 16-binary host suite green.
- [x] ISC-44: The map test is wired into CMake + CI. — test #12 in `firmware/CMakeLists.txt`; CI triggers on `firmware/hub_control/**`.
- [ ] ISC-45: CLAUDE.md §13/§7 integration deferred until the go/no-go PASS promotes the study to a committed baseline (tracked, not done now).
- [ ] ISC-46: Antecedent: euphoric surprise lands only if a reviewer sees that A→B needs no shell change — the socket-interface invariant is stated explicitly and early.
- [ ] ISC-47: Anti: the ISA never claims the redesign is "locked" while the go/no-go gate is open.

### Socket addressing contract (app + protocol surface)
- [x] ISC-74: `.npps` zone parsing enforces the documented numbering base - a `sockets:` entry outside `[NP_SOCKET_ID_MIN, NP_SOCKET_ID_MAX]`, or non-integer, is a parse error naming the id and the valid range. Covered by `npps-zones-conditions.test.ts` (0, negative, fractional, past-end, both range ends).
- [x] ISC-75: Zone membership and zone unions are deduplicated everywhere app-side through one shared implementation (`app/web/src/lib/socketSet.ts`), reused by the parser, eligibility engine, inventory guard and config UI - no call site open-codes validation, dedup or union.
- [x] ISC-76: Anti: no doc, zone file, app module, test or UI string carries an independent socket-count or id-bound literal; all read the generated constants.
- [x] ISC-77: Every zone's socket membership is derived, not authored: the eight lobe zones from skull geography (central sulcus 50%, parieto-occipital 80%, temporal lateral 38-78%), the four aggregates as deduped unions of lobe zones, and "Motor / SMA" from the stated requirement of the only protocol that references it. `sync-socket-map.ts` diffs all thirteen against the lattice and additionally asserts "All" covers every socket.
- [x] ISC-78: The serializer enforces the parser's contract — `serializeZone` canonicalises and rejects out-of-lattice ids, so the app cannot emit a `.npps` file it would refuse to read back.
- [x] ISC-79: Socket-id coercion is strict decimal-integer: `true`, `[5]`, `'0x10'`, `'1e1'`, `''` are rejected rather than silently becoming 1, 5, 16, 10, 0. `socketIdProblem` and `toSocketSet` agree on every input, and the rejected token is preserved verbatim for the error message.
- [ ] ISC-80: REG-1 consequence recorded: a 40 mm tile cannot resolve adjacent 10-20 lines (34.6 mm row pitch vs ~33 mm line spacing), so the lattice registers to alternate lines. Resolve at the curvature/lattice gate: accept a reduced prefrontal montage or revisit W. (On the scanned surface, ISC-85, the wider prefrontal row restores per-hemisphere Fp coverage — reassess against shell CAD.)

### Scan-grounded geometry (2026-07-20 — supersedes the geometry BASIS of ISC-1..8, 6.1)
- [x] ISC-81: The tiling surface is measured, not idealized: a Scaniverse LiDAR scan of the reference helmet INTERIOR is the authoritative basis. Metric scale confirmed against the published 273×226 mm exterior (±1.5%); interior confirmed single-surface with radius 1–4 mm inside the exterior (= wall). See brief §3.4.
- [x] ISC-82: The published 207 mm height is packaging, not the helmet — the scan measures ~157 mm rim-to-crown. Any model input derived from 207 mm is wrong.
- [x] ISC-83: Measured over-vertex geodesics are nasion→inion ~465 mm and ear-to-ear peak ~419 mm — ~15% larger than the envelope-minus-wall ellipsoid (404/367), because that ellipsoid shrank a published EXTERIOR by a guessed wall.
- [x] ISC-84: The socket count on the measured surface is ~78–84 at 40 mm (row construction and area quotient agree ~80), exceeding the old 64 firmware bound — the collision is real, not a modelling artifact. REGEN-1 re-cut the artifacts to 80 sockets; `--check` reports "80 sockets".
- [x] ISC-85: The physical socket lattice is a full regular parity-alternating tessellation over the scalp vault (v1 widths 3 6 7 8 9 8 9 8 7 6 5 4 = 80); parity alternation is what makes it tessellate (ISC — no two sockets < 1 tile width apart). Realized in the generated artifacts by REGEN-1; the parity and no-overlap checks pass in both the generator and `protocolEligibility.test.ts`.
- [ ] ISC-86: The ACTIVE SURFACE is software-defined and distinct from the socket lattice: a boundary tile keeps its socket and the firmware disables the low-order element_ids outside the chosen surface, via the existing two-level (socket:element) addressing (same primitive as the zone element filter, extended to a spatial mask).
- [ ] ISC-87: The tiling surface is the INNERMOST emitting-face surface (the clear window), not an offset outboard of it: on a concave bowl, rigid hex prisms presenting a continuous emitting field tessellate at their scalp-facing faces. Module + clear-layer thickness set mechanical depth and dose distance, NOT the socket count (verified: outward offset increases area ~2%/mm).
- [ ] ISC-88: Over-ear audio is preserved: the lattice stops above the ears and the ear-adjacent boundary tiles are element-masked, leaving room for the over-ear planar-magnetic 40 mm drivers + mastoid bone-conduction (CLAUDE.md §3 modality 7). Anti: no tile is placed such that it forecloses the audio-cup footprint.
- [ ] ISC-89: The active PBM surface is ≥ the Neuronic LIGHT active vault (~500–650 cm² on the same shell); v1 is ~1040 cm². Anti: the active-surface boundary may not undercut the Neuronic active area.
- [x] ISC-90: `NP_HEXMAP_MAX_SOCKETS` raised 64 → 96 (fits the 7-bit socket field ≤127; NVRAM blob 6.3→9.5 KB) to match the measured ~80-socket surface with margin — the smaller honest fix vs re-opening the locked 40 mm module width. Firmware host suite 16/16 green after the change.
- [x] ISC-91: SUPERSEDED by explicit principal direction (REGEN-1, 2026-07-20). The scan-grounded 80-socket lattice HAS now been re-cut into the generated artifacts (`sync-socket-map.ts`, `00-zones.npps`, `socketMap.generated.ts`) as **v1, PROVISIONAL**. The original anti — "do not bake v1 numbers as FINAL before REG-1/ACT-1" — is preserved in spirit: the artifacts are stamped PROVISIONAL throughout (header banners, zone-file header, `_basis: scan-measured`, `NP_TILE_GEOMETRY` JSDoc), REG-1 and ACT-1 remain open, and the active-surface descriptor is deliberately NOT emitted (that is ACT-2). The failure mode this guards against is treating v1 as locked; it is not.

## Test Strategy

| ISC group | Type | Check | Threshold | Tool |
|-----------|------|-------|-----------|------|
| Geometry (1–8) | analysis | equations + tables present and internally consistent | exact | Read brief |
| Addressing (9–18) | unit | host tests pass; cross-compile clean | 100% | ctest / arm-none-eabi-gcc |
| EMF seam (19–32, 48) | design + bench | architecture specified; prototype attenuation ≥ baseline; gasket compression map ≥ seal threshold | ≥35 dB ELF / ≥40 dB RF | anechoic + fluxgate + pressure-film bench (future) |
| Reliability (33–37) | analysis + FAI | cluster-clamp force, IP, thermal within limits | per row | bench (future) |
| Decision gate (38–41) | bench | curvature scan + coupling coupon | PASS/FAIL | metrology + optical bench (future) |
| Cross (42–47) | inspection | files exist; claims honest | binary | Read / git |

## Features

| name | description | satisfies | depends_on | parallelizable |
|------|-------------|-----------|------------|----------------|
| geometry-spec | sizing tables + curvature math | ISC-1..8 | — | yes |
| fw-addressing-map | np_module_map (done) | ISC-9..18 | — | done |
| two-layer-emf | nested-bowl shell + seam engineering | ISC-19..32 | geometry-spec | yes |
| reliability | cluster clamps, IP, thermal, mold | ISC-33..37 | geometry-spec | yes |
| decision-gate | curvature scan + coupling bench | ISC-38..41 | geometry-spec | no (gates tooling) |
| design-brief | NP-HEX-ZM-001 narrative | ISC-42, 46 | all above | no |

## Decisions

- 2026-07-20 — **Geometry basis is now a 3D SCAN of the reference helmet
  interior; count is ~80, NOT 30/54 (principal-supplied scans).** Two Scaniverse
  LiDAR scans replaced every idealization. The exterior scan confirmed the metric
  scale (footprint 272×234 mm vs published 273×226) and falsified the 207 mm
  height (packaging; the helmet is ~157 mm rim-to-crown). The interior scan is
  the actual tiling surface (single-sided, radius 1–4 mm inside the exterior =
  wall; Boa dial at the back → front = −ex). Measured over-vertex geodesics
  (nasion→inion 465 mm, ear-to-ear peak 419 mm) are ~15% larger than the
  envelope-minus-guessed-wall ellipsoid, so 40 mm hexes give **~80 sockets**, not
  54. The earlier 30 (skull) and 54 (envelope ellipsoid) models are superseded as
  the BASIS; §3.1–3.3 keep the module-fit math. **Socket lattice and active
  surface are now separated**: a full regular parity-alternating lattice (~80
  sockets), and a software-defined active surface enforced per-element via the
  existing (socket:element) addressing — boundary tiles keep their socket and
  disable out-of-surface elements. This preserves the over-ear audio (lattice
  stops above the ears; ear-adjacent tiles element-masked) and keeps the active
  PBM area (~1040 cm²) above the Neuronic LIGHT baseline. The tiling surface is
  the innermost emitting-face (clear-window) surface, per the principal's
  clear-layer correction: on a concave bowl the count is set there, and module
  thickness projects outward (sets depth/dose-distance, not count). See ISC-81..91,
  brief §3.4. **The generated artifacts are deliberately NOT re-cut yet** —
  gated on REG-1 (10-20 registration vs shell CAD) and ACT-1 (active-surface
  boundary from the audio-cup footprint + coverage targets); baking v1
  LiDAR-derived numbers as final is the failure mode this work prevents (ISC-91).
- 2026-07-20 — **Firmware bound raised 64 → 96 (committed, principal decision
  "raise the bound").** The scan shows the surface holds ~80 tiles; 64 was a
  guessed ceiling, the same class of unchecked constant that once let a 78-socket
  lattice ship, only here the physics is real. 96 gives margin, fits the 7-bit
  socket field (≤127), NVRAM 6.3→9.5 KB. Chosen over widening the locked 40 mm
  module — let addressing match the physics rather than shrink the physics to a
  guess. Firmware 16/16 green after the change. See ISC-90.
- 2026-07-20 — **The lattice is derived from SKULL ANATOMY, and re-cut from 78
  to 30 (principal request).** [SUPERSEDED as basis by the interior scan above;
  retained for the derivation method.] The helmet is one adult SKU covering 52-62 cm
  (CLAUDE.md §4.4), so the shell is sized to the largest skull in that range:
  62 cm at cephalic index 0.78 gives a 221 x 172 mm skull, a 331 mm
  nasion->inion arc, and a 613 cm2 cranial vault of which ~429 cm2 is tileable.
  The lattice is then CONSTRUCTED: 9 coronal rows at the 34.6 mm hex pitch along
  that arc, each as wide as the ear-to-ear arc at its station, whole tiles only
  -> 1+3+4+5+5+5+4+2+1 = **30**. Corroborated independently by the area quotient
  floor(429/13.86) = 30, and the 429 cm2 reproduces the ~420 cm2 the brief
  carried before the skull model existed.
  Lobes come from real landmarks, not convenience: central sulcus at the C line
  (50% of the arc) divides frontal from parietal, parieto-occipital sulcus at
  the PO line (80%) divides parietal from occipital, and temporal is the LATERAL
  band below the Sylvian fissure (outermost socket of a row, rows >=5 wide,
  38-78% of the arc). Membership: frontal 11, temporal 6, parietal 10,
  occipital 3. Midline sockets 1, 3, 11, 16, 21, 30 belong to both hemispheres.
  All zone definitions were regenerated from this; the four aggregate zones are
  deduped unions of lobe zones and are now themselves diffed against the derived
  lattice, and "Motor / SMA" is derived from the requirements of the ONE
  protocol that uses it (clinical-08-pbm-parkinsons: "bilateral, midline over
  SMA/motor") = row r3 (precentral, 41.4%) minus its temporal sockets = 10, 11,
  12. See ISC-6, ISC-6.1, ISC-77, brief §3.1-3.3.
- 2026-07-20 — **Superseded intermediate: an area-only derivation.** The shipped socket map carried
  78 sockets. At the committed 40 mm module width a hexagon covers
  (sqrt(3)/2)*40^2 = 13.86 cm2, so a ~420 cm2 tileable vault holds
  floor(420/13.86) = **30** tiles. 78 tiles would need W = 24.9 mm - below the
  34 mm workable floor, below the ~90-element embedding floor, and above
  firmware's `NP_HEXMAP_MAX_SOCKETS = 64`. It was never a buildable helmet.
  **The brief's own "~54-64 tile ceiling" was also wrong**: 54-64 tiles implies
  W = 30.0-27.5 mm, likewise below its own 34 mm floor - it reads as "roughly
  double the nominal 27-30" rather than a computed figure. The real ceiling, at
  the smallest workable W (34 mm), is **41**. Both numbers are now derived, not
  asserted: `scripts/sync-socket-map.ts` owns `MODULE_WIDTH_MM`,
  `TILEABLE_VAULT_CM2` and `derivedSocketCount()`, asserts its hex row structure
  sums to the derived count, and emits `NP_SOCKET_COUNT` /
  `NP_SOCKET_NUMBERING_BASE` / `NP_SOCKET_ID_MIN` / `NP_SOCKET_ID_MAX` /
  `NP_TILE_GEOMETRY` to the app and `hardware/np_socket_map.json`.
  **Direction of authority inverted:** skull anatomy is now upstream of
  `00-zones.npps`, which was previously treated as the locked artefact with the
  geometry fitted to it - precisely the inversion that let an impossible lattice
  pass review. See ISC-6, ISC-6.1, brief section 3.
- 2026-07-20 — **Numbering base is enforced, not merely documented.**
  `nppsParser.ts` `parseZoneBlock` now rejects any `sockets:` entry that is not
  an integer in `[NP_SOCKET_ID_MIN, NP_SOCKET_ID_MAX]`, naming the offending id
  and the valid range. Previously the base was documented as 1-based and the
  parser filtered only on `Number.isFinite`, so `[0, ...]`, `[-1]`, `[1.5]` and
  ids past the end of the lattice all parsed cleanly and then addressed nothing
  - or, worse, reached firmware where the major address is 7 bits. See ISC-74.
- 2026-07-20 — **Zone membership and zone unions are sets, enforced by shared
  code.** `app/web/src/lib/socketSet.ts` is the single implementation of
  socket-id validation, canonicalisation (dedup + sort) and union; `nppsParser`,
  `protocolEligibility` (`zoneCoverageFor`, `coverageForSockets`,
  `targetSocketsFor`, shortfall aggregation), `helmetInventory` and the
  `HelmetConfig` zone editor all route through it. Midline sockets belong to BOTH
  hemisphere zones of their lobe, so a bilateral protocol referencing a lobe pair
  must address the shared midline socket once; a concatenation double-counted it,
  inflating coverage denominators and double-dosing the site. Fixing
  `zoneCoverageFor` also closed a real defect surfaced by the test suite:
  out-of-lattice ids were being dropped instead of reported as invalid.
  See ISC-75.
- 2026-07-15 — **Mechanical option sequencing (principal decision):** Option A
  (rigid, median-curved 40 mm hexagon) is the committed baseline for the first
  build. Option B (semi-flex tile with rigid center island, curvature imposed by
  the molded socket floor) is the documented FUTURE upgrade path, deliverable as
  a module swap into the unchanged socket interface. Rationale: A honors the
  reliability-first ranking and de-risks the shared/expensive architecture
  (two-layer shell, EMF seam, sockets, addressing) with the simplest, most
  reliable module; the standardized interface keeps B open at no shell cost.
- 2026-07-15 — **Go/no-go still governs A's viability:** the curvature-scan +
  PBM-coupling bench (ISC-38..40) remains the technical gate. PASS → tool A.
  FAIL (real astigmatism worse than Δκ≈0.0039 so rigid can't hit dose coupling
  near the temples) → A is DOA and B is FORCED earlier. The sequencing decision
  does not waive this gate.
- 2026-07-15 — **EMF split placement:** all passive shielding stays on the outer
  bowl (unbroken); the inner bowl carries modules/cluster-clamps/sensors and nests
  inside the envelope. This makes the layer-parting plane a mechanical seam, not
  an aperture in the passive shield. Module cluster clamps are reached by unclamping,
  never by piercing the shield.
- 2026-07-15 — **Firmware addressing/NVRAM map delivered ahead of the mechanical
  gate** (`firmware/hub_control/np_module_map.*`): the addressing contract is
  identical for Option A and Option B, so it is safe to build now. Delegation:
  authored inline (self-contained, fully host+cross verified) rather than via
  Forge — soft delegation floor, show-your-math.
- 2026-07-15 — **Module cluster clamps (principal decision):** modules are clamped
  in clusters (one over-center lever-throw clamp per 3–7-tile cluster with per-module
  spring plungers), NOT per-module levers — a per-module lever does not scale to
  30–41 tiles (interference, short arms, 30–41 failure points). ~4–10 cluster
  actuators, 5–15× fewer moving parts; removes the per-module lever-arm floor on
  hex size. A loose tile is self-detected by the inventory/contact poll → placement
  gate. Cluster size (3–7; 7-flower vs 3-triad) finalized with the lattice work.
  **Accessibility (RISK-22 intent) is carried onto the actuator and likely
  improved**: ejector springs self-present the module and the plate auto-reseats
  the whole cluster, so an impaired user (Parkinson's/post-stroke) makes ONE coarse
  low-force action instead of N precise placements — PROVIDED the actuator is a
  large easy-grip lever-throw, not a small twist cam. Validate by HFE formative.
  See ISC-69/71/72/73, brief §5.4a, gate MECH-2.
- 2026-07-15 — **Protocol ↔ module-map matching (principal design, from user):**
  every protocol carries a required module map (per socket: DONT-CARE or a
  type-subset mask); a protocol is runnable only if the inserted modules match it.
  Standard maps shipped (All-T1-A; standard T1-A/T1-B mix; deep-PBM T1-C) plus
  user-defined; OTA extends valid type specifiers as new types appear. The
  delivered firmware primitive `np_module_map_check_placement()` (per-socket
  type_mask requirements) is exactly this matcher; the safety presence-gates (Oz
  for visual, montage for tES) are mandatory sub-cases. Added NP_ELEM_DUAL_ELECTRODE
  so the T1-B electrode satisfies both EEG and tES requirements. See ISC-58..70,
  brief §4a, gates REG-1/SMART-1/SW-1.
- 2026-07-15 — **T1 module-type taxonomy (principal decision):** three tile
  types — T1-A base PBM (660/808, no EEG), T1-B EEG/electrode (dual-rated Ag/AgCl
  electrode covering EEG + BES/tACS/tDCS, plus reduced 660/808 PBM), T1-C 1064
  smart PBM (no EEG). tES needs no separate type because T1-B's electrode is
  dual-rated; T1-B keeps base PBM for continuous coverage; an EEG+1064 4th type is
  a deferred grow-to-4. T2 adds only T2-D (1170 nm laser); qEEG-21/HD-tDCS/clin-
  tACS reuse T1-B; TMS coil + cervical VNS are non-tile. Enabling decision to
  confirm: adopt dual-rated Ag/AgCl for T1 (currently semi-dry hydrogel). See
  ISC-49..55 and brief §4a.
- 2026-07-15 — **Clamp-latch pattern (supersedes the earlier 4 = ant-C + PL/PR
  + PC):** symmetric **four-corner AL/AR/PL/PR**. Same latch count, but it
  brackets front-center and back-center spans with even force and flanks the
  forehead bridge, vs. the front-center layout that triple-covered the
  PL/PR-bracketed back and under-served the front. The blind-mate sensor/coil
  connector becomes a **standalone posterior-center boss** (decoupled from the
  clamp pattern — a connector need only mate on closure). Residual weak spots (two
  side spans over the ears; back-center PL–PR span) are verified by the new gasket
  line-pressure gate (ISC-48 / brief EMF-3), not asserted. Supersedes ISC-22/24/26
  and brief §5.3c/§5.4/§5.7.
- 2026-07-15 — **Active-cancellation placement (supports ISC-26):** Helmholtz
  coils on the outer bowl, fluxgate sensors on the inner bowl. Coils need
  enclosing uniformity, co-design with the mu-metal, separation from the µV EEG
  leads they protect, and fixed (never-opened) geometry — all favoring the outer
  bowl; sensors sample near the scalp. Full rationale in NP-HEX-ZM-001 §5.3.1.
- 2026-07-15 — **CLAUDE.md integration deferred:** the redesign is a study; the
  big locked CLAUDE.md §13/§7 tables are not edited until the go/no-go PASS
  promotes it. Prevents recording an un-gated decision as "locked".

## Changelog

- conjectured: a careful geometric model of the tiling surface (skull anatomy,
  then a helmet-interior ellipsoid from the published envelope) would give the
  right socket count.
  refuted_by: a 3D scan of the actual helmet interior. Every idealization was
  wrong in a specific way — the skull ignored the standoff, the ellipsoid shrank
  a published EXTERIOR by a guessed 3 mm wall, and the published 207 mm height
  was packaging (real ~157 mm). The measured surface is ~15% larger in every
  arc, giving ~80 sockets, not 30 or 54.
  learned: for a physical quantity, a measurement beats any model built on
  published spec sheets and reasonable assumptions — and it retires the
  assumptions wholesale (SHELL_WALL_MM, the ellipsoid, the envelope height, the
  cephalic index). The scan also settled two things no model could: that the
  count exceeds the firmware bound for real (raise the bound, don't shrink the
  physics), and that socket lattice and active surface are DIFFERENT objects —
  the lattice is a full regular tessellation, the active surface a software mask
  over it, so boundaries (rim, ear, coverage) are enforced per-element rather
  than by omitting whole tiles.
  criterion_now: ISC-81..91.

- conjectured: correcting the count was an arithmetic problem — compute
  floor(vault area / hex area) and re-cut the lattice to it.
  refuted_by: an area quotient assumes 100% packing of whole hexes inside an
  arbitrary boundary on a doubly-curved cap. It is a legitimate FALSIFIER (it
  correctly kills 78) but not a generator, and asserting `===` against it shipped
  the theoretical maximum as if it were a design output. An intermediate fix
  papered over this with a hand-picked PACKING_EFFICIENCY constant, which merely
  moved the unexamined number.
  learned: derive the lattice by CONSTRUCTION, not by quotient — place rows at
  the real hex pitch along the real nasion->inion arc and take whole tiles. The
  floor per row is what accounts for boundary waste. Then keep the area quotient
  as an independent bound: two methods that share inputs but not machinery
  agreeing at 30 is evidence; one method asserted as equality is not.
  criterion_now: ISC-6, ISC-6.1.

- conjectured: lobe zones could keep their existing shape (outer columns of the
  widest rows are temporal, bands split by row index) once renumbered.
  refuted_by: that rule is positional convenience, not anatomy — it made
  temporal membership a function of row width alone, so a re-cut lattice
  silently moved the lobe boundaries. Skull geography fixes them: the central
  sulcus is the C line at 50% of the nasion->inion arc, the parieto-occipital
  sulcus is the PO line at 80%, and temporal is a LATERAL band below the
  Sylvian fissure with its own front-to-back extent.
  learned: expressing row position as a fraction of the nasion->inion arc makes
  the lattice register to the 10-20 system by construction, which turns the lobe
  assignment into a lookup against real landmarks AND surfaces a constraint
  nobody had stated: at 34.6 mm row pitch a 40 mm tile cannot resolve adjacent
  10-20 lines (~33 mm apart), so Fp1/Fp2 collapse onto one socket. That is a
  montage constraint the module size imposes, and it belongs in REG-1.
  criterion_now: ISC-77, ISC-80.

- conjectured: the shipped 78-socket lattice was consistent with the geometry
  study, whose stated ceiling of ~54-64 tiles merely needed the count nudged
  down to fit.
  refuted_by: multiplying it out. Hex area (sqrt(3)/2)*W^2 against a ~420 cm2
  vault puts 78 tiles at W = 24.9 mm and 54-64 tiles at W = 30.0-27.5 mm - both
  below the study's OWN 34 mm workable floor. The ceiling was not a bound the
  lattice had exceeded; it was itself unbuildable, and appears to have been
  "double the nominal 27-30" rather than a computed number.
  learned: a derived quantity written down as a literal stops being checkable,
  and every downstream copy makes the error more expensive to find. The count
  had four independent literals (brief, sync script, zone file, tests) and not
  one of them was the arithmetic. The fix is not a better literal but an
  executable derivation with an assertion between it and the artefact - and
  inverting the direction of authority, so geometry constrains the zone file
  rather than the zone file constraining geometry.
  criterion_now: ISC-6, ISC-6.1, ISC-76.

- conjectured: documenting the socket numbering base and the set semantics of
  zones was sufficient for consumers to honour them.
  refuted_by: `parseZoneBlock` filtered on `Number.isFinite` alone, so socket 0
  and out-of-lattice ids parsed cleanly; and zone unions were open-coded per call
  site, so midline sockets - members of both hemisphere zones of their lobe -
  were double-counted. Routing coverage through shared code immediately exposed a
  further defect the docs had not prevented: invalid ids were being dropped
  rather than reported.
  learned: a documented invariant with no executable enforcement is a comment.
  Shared, tested primitives are the enforcement; prose is the explanation of why
  they exist.
  criterion_now: ISC-74, ISC-75.

- conjectured: a single fixed-curvature rigid hexagon cannot fit an astigmatic
  vault within PBM coupling tolerance at a useful size.
  refuted_by: the sagitta math — curving to the curvature-median doubles the
  allowable width vs flat, putting the ≤1.1 mm worst-case mismatch at W≈40 mm,
  inside the PDMS-standoff + gasket budget.
  learned: the fit ceiling and the bezel/lever/embedding floor are set by
  DIFFERENT constraints, so a genuine size sweet spot (38–42 mm) exists.
  criterion_now: ISC-1..5.

## Verification

- **Artifact state (2026-07-20, REGEN-1):** the generated artifacts now hold the
  scan-grounded **80-socket** v1 lattice (widths 3 6 7 8 9 8 9 8 7 6 5 4). `bun
  scripts/sync-socket-map.ts --check` -> "80 sockets - all 8 lobe zones reproduced
  exactly from 00-zones.npps"; web `vitest run` 197/197; firmware `ctest` 16/16
  (firmware untouched). The one-model gap between the ISCs (ISC-81..91) and the
  shipped code is **closed on v1**: on explicit principal direction REGEN-1 re-cut
  `sync-socket-map.ts`, `00-zones.npps`, and `socketMap.generated.ts` from the
  scan-grounded lattice. The artifacts remain **PROVISIONAL** — REG-1 (10-20
  registration vs shell CAD) and ACT-1 (active-surface boundary) are still open,
  and the row/lobe boundaries and active surface must be confirmed there before
  any clinical placement/dosing claim.
- ISC-81, ISC-82, ISC-83, ISC-90: derived from the scan analysis — axial ray
  test (interior single-surface), PCA de-tilt, footprint 272×234 vs published
  273×226 (scale), 157 mm measured height vs 207 mm published, geodesic arcs
  465/419 mm; firmware bound 64→96 committed with `ctest` 16/16 green.
- ISC-6, ISC-6.1, ISC-76, ISC-77 (interim model): the validator re-derives all
  four aggregate unions and asserts "All" covers every socket - both were
  verified to FAIL loudly against a stale zone file, evidence they are
  load-bearing. "stays within the firmware major-address ceiling" now checks
  NP_SOCKET_ID_MAX <= 96 (raised from 64, ISC-90).
- ISC-78, ISC-79: `npps-zones-conditions.test.ts` - serializer refuses a zone
  with ids [0, 4, 999] and canonicalises [5,3,5,1] -> [1,3,5] with the output
  re-parsing; `[true, 4]`, `[0x10]`, `[1e1]`, `[[5], 4]` all rejected;
  `socketSet.test.ts` "agrees with toSocketSet on every input" pins the two
  validators together across 12 adversarial values.
- ISC-74: `npps-zones-conditions.test.ts` - socket 0, negative, fractional,
  past-end all throw /is not on this helmet/; the error names both the offending
  id and the range; both range ends accepted. Verified against the derived
  bounds, not literals.
- ISC-75: `socketSet.test.ts` (18 checks) - dedup, sort, invalid separation,
  Set/array/string input, idempotent self-union, union never exceeds the socket
  count; `protocolEligibility.test.ts` asserts each aggregate zone equals
  `unionSockets` of its lobe zones and that a lobe pair's union is strictly
  smaller than the concatenation. Regression found and fixed en route:
  `zoneCoverageFor` dropped out-of-lattice ids instead of reporting them.
- Suite state 2026-07-20: web `vitest run` 184/184 pass across 8 files;
  `tsc --noEmit` clean; firmware `ctest` 16/16 pass.
- ISC-9,10,13,14,43,44: `ctest --test-dir build -R np_module_map_tests` → Passed;
  full host suite 16/16; `arm-none-eabi-gcc -Werror` object build clean.
- ISC-11,12: host tests `same UID → not re-inventoried`, `uid change →
  re-inventoried`, `over-max → CMD_TOO_MANY`, `NULL-cb → NOT_PRESENT` all PASS.
- ISC-15 (partial): serialize/load + CRC reject tests PASS; live Config HAL open.
- ISC-59,60: `np_module_map_check_placement()` + NP_ELEM_DUAL_ELECTRODE — 10
  placement host tests PASS (Oz-present OK, missing-electrode NOT_PRESENT + reported
  socket, dual electrode satisfies EEG and tES, incomplete montage, empty/NULL).
  73/73 map checks; full host suite 16/16; Cortex-M7 `-Werror` clean.
- Remaining ISCs: pending brief authoring (geometry/EMF/reliability/gate) and
  future hardware benches — tracked, not yet verified.
