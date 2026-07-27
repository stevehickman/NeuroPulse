# NP-HEX-ZM-001 Rev A — Hexagonal Standardized Zone-Module Design Brief

**Program:** NeurOne zone-module redesign
**Status:** DESIGN STUDY — Option A committed as baseline, Option B documented as future path. NOT a locked tooling baseline; gated by the curvature-scan go/no-go (§7).
**Companion ISA:** `docs/np_hex_zm_isa.md` (NP-HEX-ZM ISA)
**Firmware delivered:** `firmware/hub_control/np_module_map.{h,c}` + `tests/np_module_map_tests.c`
**Date:** 2026-07-15

---

## 1. Purpose

Replace position-unique zone modules (a distinct SKU per helmet pin location) with
a **single universal hexagonal module SKU** that tiles the helmet interior. One
mold, one part number, one inventory line for NeurOne and for customers. This
brief specifies the Option-A (rigid) baseline end-to-end and records the Option-B
(semi-flex) future path.

## 2. Decision record (2026-07-15)

| Item | Decision |
|------|----------|
| **Mechanical option** | **Option A (rigid, median-curved 40 mm hexagon) NOW.** |
| **Future path** | **Option B (semi-flex tile + shell-socket-defined curvature) LATER**, as a module swap into the unchanged socket interface — no shell re-tool. |
| **Rejected** | Pure Option C (fixed curvature + accepted gaps) for PBM tiles — gaps attack the J/cm² dose claim. |
| **Gate** | Curvature-scan + PBM-coupling bench (§7) still governs whether A ships or B is forced earlier. Sequencing does not waive it. |

The strategic point: the **socket interface is the invariant**. Rigid tiles today
and semi-flex tiles tomorrow drop into the same sockets, shell, addressing, and
firmware — so committing to A costs only the (cheap) module mold, not the
expensive shared architecture.

## 3. Geometry (Option A)

> **⚠ Read §3.4 first (2026-07-20).** The tile-count derivations in §3.1–3.3
> below evolved through a skull-area model (30 sockets) and then a helmet-interior
> ellipsoid derived from the Neuronic LIGHT published envelope (54 sockets). Both
> are now **superseded as the geometry basis** by a direct 3D scan of the
> reference helmet interior (§3.4), which measures the actual tiling surface and
> gives a ~80-socket lattice. §3.1–3.3 are retained for the derivation method and
> the module-fit math (curvature, dose coupling, bezel), which are unchanged; the
> **socket count and lattice come from §3.4**. The shipped generated artifacts
> (`00-zones.npps`, `socketMap.generated.ts`) now hold the scan-grounded
> **80-socket v1** lattice (re-cut by REGEN-1, 2026-07-20, on explicit principal
> direction) — still **PROVISIONAL** pending REG-1 / ACT-1; see §3.4.

Governing relation — for a hexagon of flat-to-flat width **W**, circumradius
a = W/√3, on a spherical module cap R_m against a skull region R_s:

> **Δs ≈ (W²/6)·|1/R_s − 1/R_m|** (peak module-to-scalp mismatch)

- Module curvature fixed at the **curvature-median R_m = 87 mm** (κ = 0.0115 mm⁻¹),
  which symmetrizes the worst-case mismatch across temples (tight, ~65 mm) and
  crown (flat, ~130 mm). Worst-case astigmatic **Δκ ≈ 0.0039 mm⁻¹**.

### 3.1 Tile count — derived from the largest skull, two ways

The helmet is **one adult SKU covering 52–62 cm** head circumference (CLAUDE.md
§4.4), so the shell — which carries the sockets — is sized to the **largest**
skull in that range. Everything below follows from that skull and the 40 mm tile.

**Skull model (62 cm circumference, cephalic index 0.78, vault height 100 mm):**

| Quantity | Value |
|---|---|
| Skull length × breadth | 221 × 172 mm |
| Nasion→inion arc over the vertex | 331 mm — *the 10-20 system's own longitudinal ruler* |
| Ear-to-ear arc over the vertex | 293 mm |
| Cranial vault (upper half-ellipsoid) | 613 cm² |
| **Tileable vault** (×0.70 for rim, forehead bridge, Boa arch, ear drop) | **429 cm²** |

**Tile geometry.** A regular hexagon of flat-to-flat width W has area
**A = (√3/2)·W²**; offset-packed rows advance by **¾ · 2W/√3**.

| W (flat-to-flat) | Vertex span | Dome depth | Worst-case mismatch | Active coverage (2.5 mm bezel) | Hex area | Row pitch | **Tiles** |
|---|---|---|---|---|---|---|---|
| 34 mm (floor) | 39 mm | 2.2 mm | 0.75 mm | 73% | 10.01 cm² | 29.4 mm | **42** |
| **40 mm ★** | **46 mm** | **3.1 mm** | **1.04 mm** | **77%** | **13.86 cm²** | **34.6 mm** | **30** |
| 42 mm | 48 mm | 3.4 mm | 1.15 mm | 77% | 15.28 cm² | 36.4 mm | 28 |
| 46 mm (ceiling) | 53 mm | 4.0 mm | 1.38 mm | 79% | 18.33 cm² | 39.8 mm | 23 |

**The count is corroborated two independent ways at the 40 mm design point:**

1. **Area quotient** — ⌊429 / 13.86⌋ = **30**. A pure upper bound; it assumes
   perfect packing and is only useful as a falsifier.
2. **Row-by-row construction** — rows spaced 34.6 mm along the 331 mm
   nasion→inion arc, each as wide as the ear-to-ear arc at that station, taking
   only whole tiles: **1 + 3 + 4 + 5 + 5 + 5 + 4 + 2 + 1 = 30**. This one
   accounts for the boundary waste the area quotient ignores.

Both give 30, and the 429 cm² tileable area independently reproduces the
~420 cm² this brief carried before the skull model existed.

- **Workable 34–46 mm.** Floor = bezel + element embedding (cluster clamps
  remove the per-module lever-arm floor, §5.4a); ceiling = rigid fit. Ideal
  **38–42 mm**; design point **40 mm**.
- Worst-case 1.04 mm at 40 mm is absorbed by the PDMS window standoff + a
  ≤0.8 mm compliant gasket; most of the vault sees ~0.25 mm.
- **Geometry ceiling = 42 tiles**, at the smallest workable W (34 mm).
- The densest tile (tri-wavelength PBM ~90 elements at ~3.5 mm pitch, or a
  single spring EEG pod) fits inside the 40 mm inner field.

> **Correction (2026-07-20) — the old "~54–64 tile ceiling" was wrong, and so was
> the 78-socket lattice built against it.** 54–64 tiles requires W = 30.0–27.5 mm,
> *below this table's own 34 mm workable floor*; the figure reads as "roughly
> double the nominal 27–30" rather than a computed one. Worse, the shipped socket
> map carried **78** sockets, which needs W = 24.9 mm — below the workable floor,
> below the ~90-element embedding floor, and above firmware's
> `NP_HEXMAP_MAX_SOCKETS = 64`. No W in the workable range yields 78 tiles, so
> that lattice was never buildable. Note the brief's *nominal* line (~27–30) was
> right all along: three separate guardrails disagreed with 78 and none of them
> was ever checked against it.

### 3.2 The lattice, in 10-20 coordinates

Rows are coronal bands, positioned as a fraction of the nasion→inion arc — which
*is* the 10-20 coordinate, so the lattice registers to anatomy by construction.

| Row | Arc | ~10-20 line | Tiles | Sockets | Lobe band |
|---|---|---|---|---|---|
| r0 | 10.0% | Fp | 1 | 1 | frontal |
| r1 | 20.5% | AF | 3 | 2–4 | frontal |
| r2 | 30.9% | F | 4 | 5–8 | frontal |
| r3 | 41.4% | FC | 5 | 9–13 | frontal — **precentral (motor/SMA)** |
| r4 | 51.9% | C | 5 | 14–18 | parietal — postcentral |
| r5 | 62.4% | CP | 5 | 19–23 | parietal |
| r6 | 72.8% | P | 4 | 24–27 | parietal |
| r7 | 83.3% | PO | 2 | 28–29 | occipital |
| r8 | 93.8% | O | 1 | 30 | occipital — **Oz** |

**Lobe assignment uses skull geography, not convenience:**

- **Central sulcus ≈ the C line (50% of the arc)** → frontal | parietal.
- **Parieto-occipital sulcus ≈ the PO line (80%)** → parietal | occipital.
- **Temporal is a LATERAL band**, below the Sylvian fissure — the outermost
  socket of a row, only where the row reaches the temporal line (≥5 wide) and
  only across the lobe's own front-to-back extent (F7/T3 at 38% to T5/P5 at 78%).
- **Midline sockets (1, 3, 11, 16, 21, 30) belong to BOTH hemisphere zones** of
  their lobe, so a lobe's two hemispheric zones together cover the whole lobe.

Resulting membership: frontal 11, temporal 6, parietal 10, occipital 3 = 30.

> **⚠ REG-1 consequence — a 40 mm tile cannot resolve adjacent 10-20 lines.**
> The row pitch is 34.6 mm; the 10-20 lines are 10% of the arc apart ≈ 33 mm. The
> lattice registers to **alternate** lines at best (Fp, F, C, P, O). Two specific
> impacts, both of which must be confirmed against shell CAD before any clinical
> placement claim:
>
> - The Fp row is only 72 mm of arc wide, so it holds **one** tile. **Fp1 and Fp2
>   cannot each have their own socket** — socket 1 straddles the midline and
>   covers the prefrontal montage as a single site. This narrows the 8-channel
>   T1 neurofeedback montage and is an open item against REG-1.
> - **Oz does get its own address (socket 30)**, so the photoparoxysmal halt has
>   the electrode site it requires (§4a safety presence-gates).

### 3.3 Single point of truth

`scripts/sync-socket-map.ts` is the only place the inputs appear. It derives the
lattice, asserts the row construction never exceeds the area bound, and emits
`NP_SOCKET_COUNT`, `NP_SOCKET_NUMBERING_BASE`, `NP_SOCKET_ID_MIN/MAX` and
`NP_TILE_GEOMETRY` to `app/web/src/lib/socketMap.generated.ts` and
`hardware/np_socket_map.json`.

| Constant | Value | Meaning |
|---|---|---|
| `HEAD_CIRCUMFERENCE_MM` | 620 | largest head the single adult SKU covers |
| `CEPHALIC_INDEX` | 0.78 | breadth / length |
| `VAULT_HEIGHT_MM` | 100 | vault above the circumference plane |
| `TILEABLE_FRACTION` | 0.70 | minus rim, bridge, Boa arch, ear drop |
| `MODULE_WIDTH_MM` | 40 | the design point above |
| `CENTRAL_SULCUS_ARC` / `PARIETO_OCCIPITAL_ARC` | 0.50 / 0.80 | lobe boundaries |

It also re-derives the eight lobe zones **and every aggregate union** from the
lattice and diffs them against `protocols/predefined/00-zones.npps`, with an
extra check that `"All"` covers every socket. **Direction of authority: skull
anatomy is upstream of the zone file** — when they disagree the zone file is
re-cut. (The previous rule was the reverse, treating `00-zones.npps` as the
locked artefact, which is how an impossible lattice passed review.)

### 3.4 Scan-grounded geometry — the authoritative basis (2026-07-20)

§3.1–3.3 idealized the tiling surface (a skull, then an ellipsoid derived from
the Neuronic LIGHT *exterior* envelope minus a **guessed** 3 mm wall). Two 3D
scans of a physical reference helmet (Scaniverse LiDAR, metric) replaced the
idealization with measurement.

**The scans, and what each settled:**

1. **Exterior scan** (`HelmetNoPad`). A single-sided outer shell — verified by
   casting rays along the helmet axis (each crossed exactly one surface) and by
   mesh-boundary detection (open edges only at the crown, where it rested on the
   table). PCA recovered the helmet's own axes (it sat inverted, tilted 5.1°).
   Footprint 272 × 234 mm matched the published 273 × 226 to ~1.5%, **confirming
   the metric scale**. It also falsified the published 207 mm height: the helmet
   is **~157 mm rim-to-crown** — 207 mm was the packaging.

2. **Interior scan** (`HelmetInterior`). Captures the actual tiling surface —
   the clear inner window. Confirmed interior (single surface, radius 1–4 mm
   *smaller* than the exterior everywhere = the wall thickness). The Boa dial
   sits at the **back**, so **front = −ex** in the scan frame.

**Measured over-vertex geodesics on the real interior surface:**

| Quantity | Ellipsoid model (§3.1) | **Measured (scan)** |
|---|---|---|
| Nasion→inion arc | 404 mm | **465 mm** |
| Ear-to-ear arc, peak | 367 mm | **419 mm** |
| Rim→crown height | ~157 mm implied | **157 mm** ✓ |
| Vault surface area | 939 cm² | **~1370 cm²** (full mesh) |

The real surface is **~15% larger in every arc** than the envelope-minus-wall
ellipsoid — because that ellipsoid shrank a published *exterior* by a guessed
wall. Laying 40 mm hexes on the measured surface gives **~78–84 sockets**, not
54. Both the row construction and the area quotient agree at ~80.

**The tiling surface is the innermost (emitting-face) surface — the user's
clear-layer correction.** The reference helmet has a continuous clear inner
window; its radiating elements are embedded in a layer *behind* it (further from
the scalp), shining through. NeurOne mirrors this: a module's scalp-facing face
sits **no closer to the head than the clear window**, and the socket/body
projects outward into the shell. On a **concave** bowl, rigid hex prisms that
must present a continuous emitting field tessellate at their **innermost** faces
(they touch at the scalp side and splay into the roomy shell side, where gaps
are harmless). So the socket count is set by the scanned inner surface; module
and clear-layer thickness set the mechanical depth and the face-to-scalp dose
distance, **not** the count. (Verified: offsetting the tessellation surface
outward *increases* area ~2%/mm — thickness does not reduce the count on a
concave interior.)

**Socket lattice ≠ active surface (architecture, principal decision).** These
are now separated:

- The **physical socket lattice** is a full, regular, parity-alternating
  tessellation over the scalp vault (~80 sockets). Row widths must alternate
  parity or the tiles overlap (§3.2 rule); the v1 widths are
  `3 6 7 8 9 8 9 8 7 6 5 4`.
- The **active surface** is **software-defined**, enforced per-element through
  the existing two-level `(socket:element)` addressing. A boundary tile keeps
  its socket and the firmware **disables the low-order element_ids outside the
  chosen active surface**. Because alternate rows are offset half a module, any
  smooth boundary cuts through tiles; masking sub-module elements lets the active
  edge be smooth rather than stair-stepped by whole hexes. This is the same
  primitive as the zone element-type filter (§4a) — extended to a spatial mask.
- **Audio is preserved.** The lattice stops above the ears; the ear-adjacent
  boundary tiles are element-masked so the **over-ear planar-magnetic 40 mm
  drivers + mastoid bone-conduction** (CLAUDE.md §3 modality 7) keep their space.
- **Active area ≥ Neuronic LIGHT.** The v1 active PBM surface is ~1040 cm²,
  above the Neuronic active vault (~500–650 cm² on the same shell) — a stated
  floor the active-surface boundary may not undercut.

**Firmware bound set to the full 7-bit major domain, 128 (committed).** The old
`NP_HEXMAP_MAX_SOCKETS = 64` was a guessed "physical ceiling" the scan disproved
(the surface holds ~80). It was briefly raised to 96 — but 96 is just a *different*
guessed margin, and it broke the firmware invariant that the socket bound spans
the entire addressable field (`MAX == 1 << SOCKET_BITS`), which the max-address
round-trip test relies on. Set instead to **128** = the full 7-bit major domain:
no arbitrary sub-ceiling to re-justify, the whole field is usable, the max socket
id is `0x7F`, and ~50 spare addresses are left for hardware extensions (audio
cups, HRV/VNS clip, intranasal probe, goggles) to share the same two-level
(socket:element) space. Runtime `n_sockets` (~80) is the actual count; 128 is the
addressing ceiling (NVRAM blob ~17.4 KB on the RT1062 eMMC Config partition,
~0.1% of it). Both invariants are committed host tests. See
`firmware/hub_control/include/np_module_map.h`.

**Status — v1 re-cut into the generated artifacts (REGEN-1, 2026-07-20), still
PROVISIONAL.** On explicit principal direction, `scripts/sync-socket-map.ts`,
`00-zones.npps`, and `socketMap.generated.ts` were re-cut from the scan-grounded
80-socket lattice (widths 3 6 7 8 9 8 9 8 7 6 5 4). The generator now derives the
lattice from the measured constants (nasion→inion 465 mm, ear-to-ear 419 mm,
rim→crown 157 mm, row pitch 34.6 mm, 40 mm module) — the ellipsoid/skull-axes
machinery is deleted. The artifacts are stamped **PROVISIONAL** throughout
(header banners, `_basis: scan-measured`, `NP_TILE_GEOMETRY` JSDoc): the
80-socket lattice, the lobe assignment, and the active/ear/rim boundary come from
a consumer-LiDAR scan plus tiling-margin choices, and the socket-to-10-20
registration is the open **REG-1** gate. These v1 figures are NOT locked. Still
open before this can be treated as final: (a) REG-1 10-20 registration fixing the
row/lobe boundaries against shell CAD; (b) the active-surface boundary set
deliberately from the audio-cup footprint and clinical coverage targets; (c) the
`active_surface` descriptor + firmware element-mask API (new work, ACT-2 — NOT
part of REGEN-1). The physical 80-socket lattice is what REGEN-1 delivered; the
active surface follows at ACT-1/ACT-2.

## 4. Two-level addressing + NVRAM element map (firmware — DELIVERED)

Implemented and verified in `firmware/hub_control/np_module_map.*` (63 host
checks; Cortex-M7 `-Werror` clean; CI test #12). Summary:

- **HW address = (socket_id : element_id)** packed 7 + 7 bits. Socket ≥7 bits so
  addressing never binds before the 42-tile geometry ceiling (§3.1); element 7 bits covers
  the densest ~90-element tile. Sockets asymmetrically keyed → one mount orientation.
- **Power-on poll:** the hub polls every socket for module **UID + health**; a
  module is re-inventoried (element-type list per minor address streamed into
  NVRAM) **only when its UID differs** from the one stored for that socket.
  Unchanged modules are never re-inventoried. Fail-closed on bad/oversized/absent
  inventory.
- **Resolution:** `(socket:element)` → lobe/side/x-y/type. **Groups:** 8 predefined
  (L/R × frontal/temporal/parietal/occipital) + user socket-sets + address-sets,
  each with an element-type include/exclude filter. Protocol authors manipulate
  individual elements or whole groups.
- **NVRAM:** CRC-32-protected serialize/load behind an injected HAL (bad
  magic/version/CRC rejected). Two integration seams remain: the module I2C
  `inventory_fn` and the Config-partition NVRAM HAL (OI-HEXMAP-01).
- **Privacy:** module UID is a component identifier (SHDR-class); nothing is UHDR.

### 4b. Protocol wire format carries sockets (NP Hub Protocol v2 — DELIVERED)

The addressing layer above landed ahead of the wire format that feeds it, so
until now `app/web/src/lib/hubCompiler.ts` still compiled every cranial command
down to the retired five-bit zone-module slot mask (`0x1F` / `0x07` / `0x18`).
The lattice, the named zones in `00-zones.npps`, and the `(socket:element)`
scheme all stopped at the app boundary. **Protocol v2 closes that gap.**

- **Command header 12 → 14 bytes**, adding `target_kind` + `target_len`. Each
  command carries an optional variable-length TARGET BLOCK between the header
  and `params[]`. `NP_HUB_PROTO_VERSION` is `0x0002`; v1 blobs are rejected
  outright (no shipped fleet to stay compatible with).
- **`NP_PROTO_TARGET_SOCKET_MASK`** — a 16-byte socket bitmap, one bit per
  socket, LSB-first, 0-based to match `np_hex_addr_t`. Sized to
  `NP_HEXMAP_MAX_SOCKETS` (128, the full 7-bit domain), *not* to the count of
  sockets this shell wires (80 on the current scan-grounded lattice, and it has
  already moved once from 78), so a lattice re-cut needs no wire revision.
- **A bitmap, not an address list — this is the load-bearing choice.** Under the
  inclusive membership rule a midline socket is in BOTH hemisphere zones of its
  lobe, so a protocol naming "Frontal Left" and "Frontal Right" names sockets 1,
  5 and 13 twice. A list carries the duplicate to the driver: double J/cm² on the
  same module. In a bitmap the duplicate cannot be expressed, so the dedup
  guarantee stops depending on every producer remembering it.
- **`slot_mask` → `slot_id`.** v1 addressed slots with a `uint8_t` mask and
  dispatched by `(slot_mask >> slot) & 1`, so **slots 8–16 could not be named at
  all** — the VNS clip, intranasal probe, cervical VNS and every T2 unit were
  unreachable. That is why the compiler sent every non-PBM modality to `0x01`,
  i.e. zone slot 0, i.e. the PBM driver. Each remaining slot is one device, so
  the header now carries a plain slot index (or `NP_HUB_SLOT_NONE`), and the
  parser rejects a retired zone slot, an out-of-domain slot, an unknown target
  kind, a target length that disagrees with its kind, and a slot id on a
  socket-addressed command.
- **`np_mod_stim` (BES/tACS + tDCS) got slots — and had to be fixed to use them.**
  Its externs were declared in the registry but it had no probe-table entry, so
  `np_mod_reg_get()` never returned it and nothing ever called it. Added as slots
  17/18 (appended, so no existing slot number, safety-enable bit, or probe index
  shifts). Registering it exposed two latent defects in the never-executed
  driver: `state_for_slot()` ignored its `slot` argument and returned the BES
  state for both slots, and `detect()` discriminated on `NP_HUB_SLOT_EEG + 1`, a
  slot the driver was never assigned. Left alone, a tDCS command would have taken
  the BES branch — DC priming parameters reinterpreted as an AC waveform at the
  same offsets. Both now switch on the slot; parameter lengths are checked for
  exact struct size rather than "at least".
- **Parser hardening beyond the target block.** The body must now be EXACTLY
  consumed (a `cmd_count` that under-counts silently dropped the tail, and the
  tail of an interval protocol is its STOP commands), and `start_ms +
  duration_ms` may not wrap uint32 (a wrapped auto-stop deadline can land on the
  runner's "no stop pending" sentinel 0, discarding the duration).
- **UHDR no longer records a dropped command as delivered.** `dispatch_command()`
  reports whether it dispatched, and `mods_active_mask` is set only when it did —
  otherwise a session in which every socket-addressed command was dropped would
  have written a dose record asserting PBM ran. Module faults now record on
  `s_ctx.abort_reason`, which the session-end block previously overwrote.
- **Handoff to the map:** `np_protocol_socket_expand()` turns a parsed bitmap
  into the ascending `uint16_t` socket array `np_group_query_t` wants for
  `NP_GROUP_KIND_SOCKET_SET`, so a compiled target feeds
  `np_module_map_resolve_group()` directly.
- **Retired selectors do not migrate silently.** `zones: all|front|rear` now
  refuse to compile, naming their `00-zones.npps` migration target in the error.
  Two documented meanings conflict and the consequence is wrong-site dosing:
  v1's `front` mask was `0x07` = frontal L/R **plus parietal L**, while
  `00-zones.npps` says `front` migrates to "Frontal" (frontal only); and
  `nppsParser.ts` documents numeric `custom_zones` as 0-based while
  `00-zones.npps` documents them as 1-based. No shipped protocol uses either
  form. See OI-HUB-SOCKET-02.

Verified: `np_protocol_tests` (56 host checks) + `np_mod_stim_tests` (21 checks),
full firmware host suite **18/18**; `np_protocol.c`, `np_session_runner.c`,
`np_module_registry.c` and `np_mod_stim.c` Cortex-M7 `-Werror` clean.
`hubCompiler.test.ts` (41 checks against the real `00-zones.npps`, including a
per-modality params-length table pinned to the packed `sizeof()` of every
firmware param struct), app suite **199/199**, `tsc --noEmit` clean. The stim
tests were regression-checked against the pre-fix driver: 11 of 21 fail.

## 4a. Module-type taxonomy

All tiles share **one size and one mechanical mold**; a "type" differs only by
**element population** on the identical footprint. The firmware auto-inventories
whatever is plugged (`np_module_map` UID-change poll), so type count is a
cost/inventory decision, not a firmware constraint. Only the four cranial-scalp
modalities are tiles; intranasal, auricular VNS/HRV, audio, and visual goggles
are separate accessories (not tiles).

### T1 — three tile types

| ID | Type | Elements | EEG | Covers |
|----|------|----------|-----|--------|
| **T1-A** | Base PBM | 660–670 + 808–830 nm LEDs + PD1/PD2 + NTC | no | PBM transcranial (bulk scalp coverage) |
| **T1-B** | EEG / electrode | **dual-rated Ag/AgCl electrode** + 660/808 PBM (reduced count for pod clearance) + PD + NTC | yes | EEG **and** BES/tACS/tDCS (one electrode records + stimulates); PBM at electrode sites |
| **T1-C** | 1064 smart PBM | 660/808/**1064 nm** LEDs + on-module driver (ATtiny402 + FETs) + InGaAs PD1/PD2 + NTC | no | premium deep PBM (three-tier depth stack) |

**Decisions baked in (flag to change):**
- **T1-B electrode is dual-rated** — records EEG *and* delivers BES/tACS/tDCS, so
  no separate stim-electrode type is needed. (T1 EEG is currently semi-dry
  hydrogel; adopting dual-rated Ag/AgCl for T1 is the enabling decision.)
- **T1-B keeps base 660/808 PBM** (reduced LED count around the pod) so PBM
  coverage stays continuous at electrode sites, rather than an electrode-only tile
  that leaves PBM dead spots.
- **T1-C carries no EEG.** Deep 1064 PBM *at* an electrode site would need a **4th
  type (EEG + 1064)** — deferred as a "grow-to-4" option, built only if the 1064
  zones and EEG sites actually overlap.

**Allocation — EEG only where it's needed.** The type split exists *precisely so
EEG is not in every module.* Putting EEG in every module would put an electrode
(and its spring pod + Ag/AgCl) at every one of the 30 sockets — unnecessary
cost, and it eats LED area everywhere. Instead:

- The **majority of sockets take T1-A** (base PBM, no EEG) for bulk scalp coverage.
- **T1-B is placed only at electrode positions** — the T1 EEG montage
  (Fp1/2, F3/4, C3/4, P3/4 ≈ 8 sites) plus any tES montage positions. NOTE
  §3.2: at 40 mm the Fp row holds a single tile, so Fp1/Fp2 share socket 1 —
  an open item against REG-1.
- **T1-C** goes only at the 1–5 zones chosen for 1064 depth.

Representative T1 build: **~8–9 × T1-B** (8 neurofeedback sites + Oz when visual
stim is used) **+ the balance in T1-A**, with T1-C substituted at the configured
depth zones. (High-density EEG or extra tES sites are just more T1-B placements —
a configuration choice, no new type.)

### T2 — one additional tile type

| ID | Type | Elements | Reuses / notes |
|----|------|----------|----------------|
| **T2-D** | 1170 nm deep-PBM laser | 1170 nm laser diode + TEC + laser driver | new type (laser ≠ LED) |
| — | qEEG-21 / HD-tDCS 4×1 / 16-ch clinical tACS | dual-rated Ag/AgCl electrodes | **reuse T1-B** at higher density; wet-gel vs semi-dry is a consumable, not a type |
| — | TMS focal coil; cervical VNS | — | **non-tile** special applicators (too large / off-scalp) |

**Net: T1 = 3 tile types (grow-to-4); T2 = +1 (T2-D).** Cost-optimal working set
≈ 3 (T1) and 4 (T2). Accessories (nose/ear/audio/goggles + TMS coil + neck VNS)
are separate hardware in every case.

### Mixed insertion + placement validation

Tiles are **type-agnostic**: any type (T1-A/B/C) inserts into any socket — same
size, same mount, orientation-only key (no type keying). Identity is split —
**socket = position** (fixed major address from the geometry map) and
**module = type** (self-reported on insertion via UID → element inventory). So the
helmet always knows what is where with no manual setup; `np_module_map` rebuilds
the map on any change and the bone-conduction announce states position + type
("Frontal-left, EEG connected"). Seal, power, and clamp are identical across
types, so mixing does not affect IP or power.

**Software placement check (DELIVERED).** Because sockets don't enforce type,
safety-critical and montage requirements are checked in software against the live
inventory — `np_module_map_check_placement()` (firmware, host-tested). Each
requirement is "socket S must hold an element in `type_mask`"; the call returns the
list of unmet sockets so the app can guide the user to fix the placement.

**Protocol ↔ module-map matching.** The check generalizes into the run-gating
model: every protocol carries a **required module map** — one specifier per socket,
either **DONT-CARE** or a **subset of allowed module types** (a `type_mask`;
DONT-CARE = all-types mask, i.e. that socket is simply omitted from the check). A
protocol is **runnable only if the inserted modules match its map** (`check_placement`
passes for every non-DONT-CARE socket). NeurOne ships **standard maps** (All-T1-A;
the standard T1-A/T1-B mix; deep-PBM with T1-C zones; …) and users may **define
their own**. The app enables/disables protocols by matching each protocol's map
against the current insertion — you can only run protocols whose map matches what
is currently inserted. The safety presence-gates below (Oz, tES montage) are
mandatory sub-cases folded into every relevant protocol's map. **OTA-extensible:**
as new module types are introduced, OTA updates extend the set of valid type
specifiers (and can push new/updated standard maps), so existing protocols keep
matching and new ones can require the new types. The firmware primitive already
expresses a map as an array of per-socket `type_mask` requirements.

**Safety presence-gates (firmware enforces before modality enable — SW-1):**
- **Visual stim → EEG element at Oz.** The photoparoxysmal halt (<200 ms, §4.2)
  needs an Oz electrode, and **Oz is NOT in the 8-ch neurofeedback montage** — so a
  visual-stim build needs a T1-B at Oz — socket 30 in the derived lattice (§3.2), and visual
  stim is blocked unless `check_placement({Oz, EEG|DUAL})` passes.
- **tES (BES/tACS/tDCS) → electrodes at all montage sockets** (anode + returns;
  HD-tDCS 4×1 = 5) before enable.
- The T1-B electrode is typed `NP_ELEM_DUAL_ELECTRODE`, so it satisfies both EEG
  and tES requirements.

**Smart-socket coverage (open decision — SMART-1).** T1-C (1064) needs the I2C bus
+ switchable-gain TIA and carries a distinct mechanical key. Decide: **every socket
smart-capable** (T1-C anywhere; higher per-socket cost) **or a subset** (cheaper;
T1-C key matches only those sockets). This is the only real limit on "any type,
any socket."

**PBM dose islands (accepted).** T1-B has ~half the LED count (pod clearance), so
electrode sites deliver less PBM. Per-tile PD metering stays accurate (the J/cm²
claim is intact — each tile meters itself); firmware may compensate within the 25%
duty / 42 °C limits, or accept the mild non-uniformity (PBM is already ±15–25%).

**10-20 registration (gate — REG-1).** T1-B inserts anywhere but is only *useful*
at 10-20 positions, so the socket lattice must place sockets at (or within
tolerance of) each required site — 8–9 for T1, ~19 scalp for T2. The ±12 mm pod
travel + EEG placement tolerance give margin, but the lattice can't be a naïve
uniform grid and the registration must be verified against the coverage/bezel
budget (§3).

## 5. Two-layer shell + the EMF seam (detailed)

### 5.1 Why two layers

Tiling the interior removes the interior real estate the module clamp mechanism
used to occupy — a mechanism on the tiled face would leave coverage gaps. So the
clamp moves OFF the interior. But a mechanism that pierces the 5-layer EMF stack
punches an aperture in the shield. The resolution is a **two-nested-bowl shell**:

- **Outer bowl = the complete EMF envelope.** The full passive stack lives on its
  INNER face, unbroken: CFRP outer (30–50 dB RF) · 0.2 mm mu-metal L2 (15–25 dB
  ELF magnetic) · palladium-polyester L3 (40–60 dB RF) · carbon-loaded absorber
  L4. The Helmholtz cancellation coils mount here too. This bowl is NEVER opened
  for a module swap.
- **Inner bowl = the module carrier.** It holds the keyed sockets, the module
  **cluster clamps** (§5.4a — one actuator per cluster of tiles, on its OUTER,
  gap-facing face), the fluxgate sensors, and the tiled module field on its
  interior (scalp-facing) face. It **nests inside** the outer bowl.
- The two bowls **clamp together for use** and **separate for module replacement.**

### 5.2 The key insight — the split is not a cut in the shield

Because the passive shield is entirely on the outer bowl and the inner bowl nests
**inside** it, the module carrier lives within the Faraday envelope. The
layer-parting plane is therefore a **mechanical** seam between the shielded outer
bowl and the unshielded inner bowl — **not an aperture through the passive
shield.** The module cluster clamps sit in the inter-bowl gap and are reached by
unclamping the bowls; **they never pierce the shield.** Module swaps never touch
shielding.

### 5.3 The seams that still need real engineering

Four electromagnetic details survive and must be designed:

**(a) Parting-plane / rim slot.** The two bowls meet around the helmet mouth. To
keep this from acting as a slot antenna, the **outer bowl overlaps the inner
bowl's rim with a labyrinth lip** (a ≥2× overlap fold), so there is no
line-of-sight aperture from outside to the modules. Target: any continuous
residual slot ≤ **λ/20 at 6 GHz ≈ 2.5 mm** (the upper Wi-Fi 6 band bounds the
external RF concern; the headset's own radios live in the hub, not here). A
conductive elastomer bead along the lip closes the residual gap.

**(b) Shield-to-ground bond across the parting plane.** The outer shield must stay
referenced to hub/system ground — this is what makes it a **driven EEG shield**
(the shell is bonded to the DRL output today). With the shield on the outer bowl
and the DRL electronics on the inner-bowl/hub side, that reference has to cross
the parting plane. It is carried by **hard-gold-plated beryllium-copper spring
fingers (or conductive elastomer) at the four clamp latches**, target contact
resistance **≤50 mΩ**. Hard gold resists the fretting/oxidation that would
otherwise raise the bond impedance over clamp cycles.

**(c) Sensor / coil harness crossing.** The active cancellation splits across the
seam by design: **fluxgate magnetometers on the inner bowl** (near the scalp,
where they must sense the field the wearer experiences) and **Helmholtz coils on
the outer bowl** (with the passive shield). Both route to the hub controller via a
**standalone blind-mate boss at the posterior-center** (occiput centerline, NOT a
latch), mated automatically as the bowls draw closed and seated by the flanking
PL/PR latches. Decoupling the connector from the clamp pattern is deliberate — a
blind-mate feature only has to mate on closure; it should not dictate latch count.

**(d) Magnetic (mu-metal) continuity.** Magnetic shields leak at butt-joints. The
mu-metal L2 stays **entirely on the outer bowl, unbroken**; the inner bowl carries
**no** magnetic layer, so there is no mu-metal seam to leak. (This is the reason
all shielding is consolidated on one bowl rather than split between them.)

### 5.3.1 Rationale: coils on the outer bowl, fluxgate sensors on the inner bowl

The obvious objection is that a cancellation coil placed on the inner carrier
would sit closer to the brain (more field per amp). That is true and it loses —
the split assigns each element to the layer its physics wants. Sensors sample
where the brain is (inner); the actuator wants to be a stable, uniform, separated,
enclosing structure (outer). Five reasons the coils go outer:

1. **Uniformity, not proximity, is the requirement.** Cancellation must null the
   field roughly uniformly across the whole brain volume — a large *enclosing*
   coil (outer) puts the brain in its uniform-field region; a near coil (inner)
   cancels locally and steeply, cleaning one electrode while worsening another.
2. **The coil is co-designed with the mu-metal as one magnetic circuit.** The
   high-permeability mu-metal (outer bowl) shunts and reshapes any nearby coil's
   flux. A coil *inside* that shell (inner bowl) has its field distorted before it
   reaches the brain and becomes hard to calibrate; co-locating passive shield +
   active trim on the outer bowl keeps them a single characterized subsystem.
3. **On the inner bowl the coil sits on top of what it cleans.** The inner carrier
   holds the EEG electrodes, tES drivers, and their µV-sensitive FPC leads. A
   dB/dt source millimeters away injects its drive current and switching harmonics
   (plus notch/TMS-gated transients) straight into the recording it exists to
   protect. Separation decouples the field generator from the pickup.
4. **The inner bowl is the handled/opened/serviced layer.** Cancellation is a
   calibrated loop assuming a fixed coil-drive→field transfer. The outer bowl is
   never opened; its coil geometry stays fixed. The inner bowl gets unclamped and
   levered, so its geometry drifts clamp-to-clamp — a poor home for a precision
   actuator.
5. **Three-axis coils are bulky and compete for tiling real estate.** Three
   orthogonal coil pairs weaving through the serviceable inner bowl would either
   steal element coverage (the redesign's whole point) or add thickness and a heat
   source at the scalp (42 °C limit). They build cleanly into the fixed outer shell.

Reconsider an inner-side trim coil only if *module-generated* fields (stim
currents, LED drivers) prove to dominate over external ELF — but those are killed
at the source by the existing adaptive stim-frequency notch and TMS-gated
cancellation, not by relocating the Helmholtz pair.

### 5.4 The clamp mechanism

- **Four layer-clamp latches, symmetric:** anterior-left, anterior-right,
  posterior-left, posterior-right — a four-corner clamp. All seated at the rim
  **between** the ear (audio) and neck attachment zones so they don't collide
  with those accessories. They reuse the recessed-lever concept (flush when
  closed, no snag).
- **Why this pattern and not a kinematic three-point, or a front-center layout:**
  the parting-plane conductive gasket must stay uniformly compressed all the way
  around the rim for the RF/ground seal to hold — a flange-seal problem, not a
  rigid-body location problem. The symmetric four-corner pattern **brackets both
  the front-center and back-center spans** with even L/R and front/back force
  (least bowl distortion), and AL/AR flank the 5-position forehead bridge instead
  of colliding with it on the centerline. It is strictly better placement than an
  anterior-center + posterior-center layout, which triple-covers the (already
  PL/PR-bracketed) back-center while under-serving the front. The mild
  over-constraint of four points is absorbed by the compliant bowls + gasket.
- The **blind-mate sensor/coil connector is a standalone posterior-center boss**
  (§5.3c), NOT a latch — sited where the internal harness gathers near the occiput
  (Boa arch / neck attach) and seated by the flanking PL/PR latches.
- Each latch integrates the ground-bond spring fingers (§5.3b) so clamping the
  bowls simultaneously closes the shield-to-ground bond.
- **Residual weak points (verified, not asserted — see EMF-3, §7):** the two
  **side spans over the ears** get no rim latch (ear zones forbid it) and the
  **back-center span between PL/PR** has no dedicated latch. Both must be shown
  above the gasket seal-compression threshold by the line-pressure map; if
  marginal, the fix is lip/gasket stiffening (or a temporal-wing lateral latch for
  the sides / restoring a posterior-center latch for the back), not more corner
  latches.

### 5.4a Module cluster clamps (not per-module levers)

A lever *per module* does not scale: at 30 tiles (up to 42 at the small-hex
ceiling) you get 30–42 mechanisms in the inter-bowl gap — mutual interference,
short lever arms, and
a huge moving-part count (each lever = spring + pin + detent = a failure point).
Instead the modules are clamped in **clusters**, one actuator per cluster.

- **Cluster unit from the hex lattice:** the natural super-cell is the **7-hex
  "flower"** (1 center + 6 neighbors) → 30 tiles ≈ **4 clusters**; the smaller
  **3-hex triad** → ~9–10 clusters. (So total actuators ≈ 4 corner layer-latches +
  4–10 cluster clamps, vs 4 + 30–42 with per-module levers — a 4–10× reduction.)
- **Mechanism:** one **over-center lever-throw clamp per cluster** — a push/pull
  toggle latch (NOT a twist cam; see accessibility below) — drives a **clamp plate
  carrying a spring-loaded plunger per module.** Throwing it closed compresses every
  plunger → all N modules seated with *individual, controlled force* despite the
  dome curvature; the over-center geometry gives high mechanical advantage near
  close (low one-handed input force) and a positive latched state; releasing it
  lifts the plate → modules pop on their own ejector springs. The per-module
  plungers are the key — a rigid plate over a curved cluster could not seat evenly.
- **This removes the lever-arm floor on hex size** (that was a per-module
  small-lever artifact): the size floor is now bezel + element embedding only (§3).
- **Tradeoff (small):** swapping one module releases its whole cluster (3–7 tiles
  loosen). They don't fall out — the inner bowl faces up when the helmet is open and
  gasket friction holds them; lift out the one you want and re-clamp.
- **A loose module is self-detected:** an unseated tile fails its contact/inventory
  poll → shows "not present" in `np_module_map` → the placement/protocol-map gate
  (§4a) disables any protocol that needs it. A cluster-closed sensor is cheap
  insurance but not strictly required.
- **Accessibility — carries the RISK-22 intent, and likely improves on it.** The
  eject lever existed to serve Parkinson's H&Y II–III / post-stroke hand weakness
  (≤1 N, tool-free). Clustering *can* be a net gain — fewer/larger targets than
  many small recessed levers; **ejector springs make extraction near-zero-force**
  (the module self-presents); and re-throwing the one actuator **auto-reseats the
  whole cluster via the plate + plungers**, replacing N precise placements with one
  coarse action; swaps happen with the helmet off on a surface. But it is a
  requirement, not automatic — the actuator MUST be: a **large easy-grip control**
  (palm/hook, not a fingertip pinch); a **push/pull over-center lever throw — NOT a
  twist cam** (twisting defeats weak grip / limited forearm rotation / tremor);
  **low input force via mechanical advantage** (the RISK-22 low-force intent
  restated as input force, not per-module extraction); **one-handed**, large
  forgiving target, clear open/closed state. **Validate by HFE formative** (5 subjects, Parkinson's
  H&Y II–III / post-stroke) — the NP-TOOL-ZM-001 OI-4 eject-lever study re-pointed
  at the cluster actuator.
- **Optional alignment:** cluster boundaries may align to the lobe groups (a
  frontal-left cluster ≈ the frontal-left group), but are ultimately set by
  geometry/curvature — final cluster size is decided with the lattice work (§7
  REG-1 / MECH-2).

### 5.5 Layer-closed interlock (safety + EMF integrity)

A **Hall/contact sensor on each of the four latches** reports closed/open. The
safety architecture **refuses to enable any modality unless all four report
closed** — analogous to the existing goggle-lift Hall cutoff. Consequences:

- Every active session runs with the full passive shield intact AND the active
  cancellation available (fluxgates + coils connected).
- During a module swap the bowls are open and shielding is degraded — but no
  session can run in that state, so the degraded-open condition is never live.

### 5.6 Monitoring + service

- The **ground-bond contact resistance is trended in SHDR**; a rising trend flags
  shield/bond degradation, reusing the existing fleet EMF-attenuation monitoring
  (§5.1/§4.3 of CLAUDE.md). This is device-condition data — no user biology.
- The **conductive parting-plane gasket is a replaceable/tethered service part**
  (conductive elastomer takes compression set over clamp cycles).
- Prototype acceptance: measured attenuation with bowls clamped must **meet or
  exceed the single-shell baseline (≥35–45 dB ELF magnetic, ≥40–60 dB RF)** — the
  redesign may not regress the shielding claim.

### 5.7 Cross-section (schematic)

```
   outside
 ══════════════  ← outer bowl: CFRP / mu-metal / Pd / absorber (unbroken)  +Helmholtz coils
        )        ← labyrinth overlap lip + conductive bead  (parting-plane seam)
   ┌─cluster─┐   ← inter-bowl gap: module cluster clamp (reached only when unclamped)
 ──────────────  ← inner bowl: sockets + fluxgate sensors
  ▢ ▢ ▢ ▢ ▢ ▢   ← tiled hexagonal module field
 ~~~~~~~~~~~~~~  ← scalp
        ▲
   clamp latch (×4: ant L/R, post L/R — four-corner) → Hall + BeCu ground-bond
   posterior-center BOSS (no latch) = blind-mate sensor/coil connector
```

## 6. Reliability + manufacturing

- **Cluster clamps (not per-module levers, §5.4a):** one over-center lever-throw
  clamp per 3–7-tile cluster (~4–10 total) with per-module spring plungers; low
  one-handed input force (RISK-22 intent). Cuts moving parts 5–15× and removes the
  per-module short-arm problem — so the hex-size floor is now bezel + embedding,
  not lever arm.
- **IP:** each hex needs a perimeter gasket; total seam length rises vs 5 big
  zones, so IPX4 rides on 30 co-molded gaskets — a per-tile seam-length budget
  is required (RISK-16 gasket precedent).
- **Thermal:** whole-vault active tiling raises aggregate scalp thermal load; keep
  per-tile NTC + hardware throttle (42 °C limit unchanged).
- **Mold:** one universal module-shell mold; element population is FPC/loadings
  only. This is the inventory/tooling win.
- **Option A carries no new high-cycle flex on rigid components** — flex reliability
  is deferred to Option B.

## 7. Open items / gates

| ID | Item | Blocking for |
|----|------|--------------|
| GATE-1 | Curvature-scan bench (5–95th pct head map) validates Δκ≈0.0039 | Tooling |
| GATE-2 | PBM coupling bench: rigid 40 mm coupon at temporal worst case meets dose spec | Tooling; go/no-go A-vs-B |
| OI-HEXMAP-01 | Config-partition NVRAM HAL for the module map | FW integration |
| OI-HEXMAP-02 | Module I2C/1-wire `inventory_fn` | FW integration |
| REG-1 | Socket lattice registers to 10-20 (8–9 T1, ~19 T2 scalp) within tolerance, without violating the coverage/bezel budget. **§3.4 measured the real interior surface → ~80-socket v1 lattice.** Fix the row/lobe boundaries against shell CAD before re-cutting the generated artifacts. | Lattice design; EEG/tES placement; **artifact regeneration**; **clinical-03 evidence-grade claim gate** (`protocols/predefined/clinical-03-pbm-cognitive-1064.npps` — "Grade A"/gold-standard wording withheld until REG-1 lands and the zone is re-authored to the 1–2 module Fp2/F4 footprint the literature actually describes; see `docs/status/pending-decisions.md` §13.2c) |
| SCAN-1 | Confirm `SHELL_WALL_MM` proxy is moot now that the interior is scanned directly; measure the actual clear-window thickness + module face standoff for the emitting-face dose distance (§3.4) | Dose budget; emitting-face position |
| ACT-1 | Set the **active-surface boundary** deliberately from the over-ear audio-cup footprint + clinical coverage targets (≥ Neuronic active area); it defines which boundary tiles are element-masked (§3.4) | Active-surface descriptor; masking |
| ACT-2 | New firmware: `active_surface` descriptor + element-mask API extending `(socket:element)` addressing so boundary tiles disable out-of-surface elements (§3.4) | Masking enforcement |
| REGEN-1 | **DONE (v1, 2026-07-20, principal direction).** Re-cut `sync-socket-map.ts` / `00-zones.npps` / `socketMap.generated.ts` from the scan-grounded 80-socket lattice (widths 3 6 7 8 9 8 9 8 7 6 5 4). Artifacts stamped PROVISIONAL; REG-1 + ACT-1 still confirm the boundaries/active surface before v1 is treated as final. Active-surface descriptor deferred to ACT-2. | Generated artifacts |
| SMART-1 | Smart-socket coverage decision: all sockets I2C+TIA-capable vs a subset (governs where T1-C can seat) | Socket PCB cost/scope |
| SW-1 | Wire `np_module_map_check_placement()` presence-gates into modality enable (Oz-before-visual-stim; electrodes-before-tES) | Safety enforcement (primitive delivered) |
| OI-HUB-SOCKET-01 | Dispatch socket-addressed commands: socket-indexed control registry + per-socket safety-MCU enable (today `NP_SAFETY_EN_PBM_ZONE_0..4` is per-zone-slot). Until then `dispatch_command()` logs and DROPS a socket target rather than falling back to the slot path — a missed dose is recoverable, a wrong-site dose is not | Cranial session execution (parse + resolve delivered) |
| OI-HUB-SOCKET-02 | Socket-address the remaining socket-based modalities (1170 nm deep PBM, EEG/qEEG, BES/tACS/tDCS/HD-tDCS). They stay slot-addressed today because their param types carry no socket selector — EEG names 10-20 channels, tES names electrode pairs. Needs the param types to gain zone refs first | Per-socket cranial targeting beyond PBM transcranial |
| EMF-1 | Prototype 2-layer attenuation ≥ single-shell baseline | Shield claim |
| EMF-2 | Ground-bond ≤50 mΩ over clamp-cycle life; SHDR trend armed | Driven-shield function |
| EMF-3 | Gasket line-pressure map: min compression at back-center (PL–PR) span AND the two side (ear) spans ≥ seal threshold with the four-corner AL/AR/PL/PR pattern; if marginal → lip/gasket stiffening (or lateral/posterior-center latch) | Latch-pattern sign-off; RF seal |
| MECH-1 | Four-corner clamp (AL/AR/PL/PR) + posterior-center connector boss + Hall interlock detail | Shell tooling |
| MECH-2 | Module cluster-clamp design: cluster size (3–7), over-center lever-throw actuator (not a twist cam) + per-module spring plungers, curvature span, low one-handed input force | Inner-bowl tooling; serviceability |
| DOC-1 | CLAUDE.md §7/§13 integration once GATE-1/2 PASS | Baseline promotion |

## 8. Cross-references

- ISA: `docs/np_hex_zm_isa.md`
- **Optics: NP-OPT-PSF-001 (`docs/np_opt_psf_001.md`)** — Monte Carlo point-spread function
  for a module at 1064 nm. Establishes the spatial resolution floor at cortical depth
  (edge-spread function 10–90% **~26 mm**, range 24–31 mm across the CSF-thickness sweep;
  a 40 mm module's own edge measures 22.8 mm but that is aperture-limited, not a floor)
  and the contralateral energy fractions used to size zones. Relevant to §3 because it
  shows the chosen module pitch already sits near the optical resolution limit, so
  sub-module addressing granularity buys little at depth. Model: `scripts/pbm-optical-psf.ts`.
- Firmware: `firmware/hub_control/np_module_map.{h,c}`, `tests/np_module_map_tests.c`
- Wire format v2: `firmware/hub_control/include/np_hub_config.h` (target block),
  `include/np_hub_types.h` (`np_proto_target_kind_t`), `src/np_protocol.c`,
  `tests/np_protocol_tests.c`; app side
  `app/web/src/lib/hubCompiler.{ts,test.ts}`
- Predecessors: NP-TOOL-ZM-001 (legacy zone-module tooling), NP-DRV-SHELL-001
  (shell FPC routing), NP-HW-FPC-001 (FPC pinout), CLAUDE.md §3/§4.3/§7 (modality
  stack, EMF shielding, durability).
