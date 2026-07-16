---
task: Hexagonal standardized zone-module redesign (Option A rigid; Option B future)
project: NeurOne
slug: hex-zone-module
effort: E4
phase: plan
progress: 6/48
mode: design-study
started: 2026-07-15
updated: 2026-07-15
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
  geometry (~54–64 tile ceiling). Sockets asymmetrically keyed → single mount
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
- [ ] ISC-2: Workable width range 34–46 mm is documented with the floor driver (bezel/lever/embedding) and ceiling driver (rigid fit) named per bound.
- [ ] ISC-3: Module curvature fixed at R_m = 87 mm (curvature-median); dome depth at W=40 computed (~3.1 mm).
- [ ] ISC-4: Worst-case skull-mismatch sagitta at W=40 is ≤1.1 mm (Δs = (W²/6)·Δκ, Δκ=0.0039 mm⁻¹) and shown absorbed by PDMS window standoff + ≤0.8 mm gasket.
- [ ] ISC-5: Active-coverage fraction at W=40 with 2.5 mm bezel is ≥76%, tabulated across the workable range.
- [ ] ISC-6: Module count for a ~420 cm² tileable vault is 27–30 (range 24–33 for area uncertainty); max geometry ceiling ~54–64 at smallest workable W.
- [ ] ISC-7: Densest module element set (tri-wavelength PBM ~90 elements, or a spring EEG pod) fits within the 40 mm inner field — embedding floor cleared.
- [ ] ISC-8: Anti: no module width is specified small enough that bezel reduces skull element coverage below the single-zone baseline.

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
- [ ] ISC-19: Shell is two nested bowls: outer = complete EMF envelope; inner = module/socket/lever carrier nesting inside it.
- [ ] ISC-20: The full passive stack (CFRP, mu-metal L2, palladium L3, absorber L4) lives UNBROKEN on the outer bowl; the inner bowl carries no passive shield.
- [ ] ISC-21: Module levers live in the inter-bowl gap (inner-bowl exterior face) and are reached only by unclamping the bowls — they never pierce the outer shield.
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
- [ ] ISC-32: Anti: no lever, sensor wire, or fastener creates an un-gasketed aperture through the passive shield.
- [ ] ISC-48: Gasket line-pressure map (FEA or pressure-film bench) shows min seam compression ≥ seal threshold at the back-center (PL–PR) span AND both side (ear) spans under the four-corner AL/AR/PL/PR pattern; a marginal span is fixed by lip/gasket stiffening (or a lateral / restored posterior-center latch), not more corner latches. (Brief §7 EMF-3.)

### Reliability + manufacturing
- [ ] ISC-33: Module levers give ≤1 N extraction with a lever arm ≥13 mm at W=40 (accessibility: Parkinson's H&Y II–III), per RISK-22 precedent.
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

## Test Strategy

| ISC group | Type | Check | Threshold | Tool |
|-----------|------|-------|-----------|------|
| Geometry (1–8) | analysis | equations + tables present and internally consistent | exact | Read brief |
| Addressing (9–18) | unit | host tests pass; cross-compile clean | 100% | ctest / arm-none-eabi-gcc |
| EMF seam (19–32, 48) | design + bench | architecture specified; prototype attenuation ≥ baseline; gasket compression map ≥ seal threshold | ≥35 dB ELF / ≥40 dB RF | anechoic + fluxgate + pressure-film bench (future) |
| Reliability (33–37) | analysis + FAI | lever force, IP, thermal within limits | per row | bench (future) |
| Decision gate (38–41) | bench | curvature scan + coupling coupon | PASS/FAIL | metrology + optical bench (future) |
| Cross (42–47) | inspection | files exist; claims honest | binary | Read / git |

## Features

| name | description | satisfies | depends_on | parallelizable |
|------|-------------|-----------|------------|----------------|
| geometry-spec | sizing tables + curvature math | ISC-1..8 | — | yes |
| fw-addressing-map | np_module_map (done) | ISC-9..18 | — | done |
| two-layer-emf | nested-bowl shell + seam engineering | ISC-19..32 | geometry-spec | yes |
| reliability | levers, IP, thermal, mold | ISC-33..37 | geometry-spec | yes |
| decision-gate | curvature scan + coupling bench | ISC-38..41 | geometry-spec | no (gates tooling) |
| design-brief | NP-HEX-ZM-001 narrative | ISC-42, 46 | all above | no |

## Decisions

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
  bowl (unbroken); the inner bowl carries modules/levers/sensors and nests
  inside the envelope. This makes the layer-parting plane a mechanical seam, not
  an aperture in the passive shield. Module levers are reached by unclamping,
  never by piercing the shield.
- 2026-07-15 — **Firmware addressing/NVRAM map delivered ahead of the mechanical
  gate** (`firmware/hub_control/np_module_map.*`): the addressing contract is
  identical for Option A and Option B, so it is safe to build now. Delegation:
  authored inline (self-contained, fully host+cross verified) rather than via
  Forge — soft delegation floor, show-your-math.
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

- conjectured: a single fixed-curvature rigid hexagon cannot fit an astigmatic
  vault within PBM coupling tolerance at a useful size.
  refuted_by: the sagitta math — curving to the curvature-median doubles the
  allowable width vs flat, putting the ≤1.1 mm worst-case mismatch at W≈40 mm,
  inside the PDMS-standoff + gasket budget.
  learned: the fit ceiling and the bezel/lever/embedding floor are set by
  DIFFERENT constraints, so a genuine size sweet spot (38–42 mm) exists.
  criterion_now: ISC-1..5.

## Verification

- ISC-9,10,13,14,43,44: `ctest --test-dir build -R np_module_map_tests` → Passed;
  full host suite 16/16; `arm-none-eabi-gcc -Werror` object build clean.
- ISC-11,12: host tests `same UID → not re-inventoried`, `uid change →
  re-inventoried`, `over-max → CMD_TOO_MANY`, `NULL-cb → NOT_PRESENT` all PASS.
- ISC-15 (partial): serialize/load + CRC reject tests PASS; live Config HAL open.
- Remaining ISCs: pending brief authoring (geometry/EMF/reliability/gate) and
  future hardware benches — tracked, not yet verified.
