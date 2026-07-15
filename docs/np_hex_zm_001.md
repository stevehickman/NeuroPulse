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

Governing relation — for a hexagon of flat-to-flat width **W**, circumradius
a = W/√3, on a spherical module cap R_m against a skull region R_s:

> **Δs ≈ (W²/6)·|1/R_s − 1/R_m|** (peak module-to-scalp mismatch)

- Module curvature fixed at the **curvature-median R_m = 87 mm** (κ = 0.0115 mm⁻¹),
  which symmetrizes the worst-case mismatch across temples (tight, ~65 mm) and
  crown (flat, ~130 mm). Worst-case astigmatic **Δκ ≈ 0.0039 mm⁻¹**.

| W (flat-to-flat) | Vertex span | Dome depth | Worst-case mismatch | Active coverage (2.5 mm bezel) |
|---|---|---|---|---|
| 34 mm | 39 mm | 2.2 mm | 0.75 mm | 73% |
| **40 mm ★** | **46 mm** | **3.1 mm** | **1.04 mm** | **77%** |
| 42 mm | 48 mm | 3.4 mm | 1.15 mm | 77% |
| 46 mm | 53 mm | 4.0 mm | 1.38 mm | 79% |

- **Workable 34–46 mm.** Floor = bezel + lever arm + element embedding (all push
  up); ceiling = rigid fit (pushes down). Ideal **38–42 mm**; design point **40 mm**.
- Worst-case 1.04 mm at 40 mm is absorbed by the PDMS window standoff + a ≤0.8 mm
  compliant gasket; most of the vault sees ~0.25 mm.
- **~27–30 modules** over a ~420 cm² tileable vault (24–33 for area uncertainty);
  geometry ceiling ~54–64 tiles at the smallest workable W.
- The densest tile (tri-wavelength PBM ~90 elements at ~3.5 mm pitch, or a single
  spring EEG pod) fits inside the 40 mm inner field — embedding floor cleared.

## 4. Two-level addressing + NVRAM element map (firmware — DELIVERED)

Implemented and verified in `firmware/hub_control/np_module_map.*` (63 host
checks; Cortex-M7 `-Werror` clean; CI test #12). Summary:

- **HW address = (socket_id : element_id)** packed 7 + 7 bits. Socket ≥7 bits so
  addressing never binds before the ~54–64 geometry ceiling; element 7 bits covers
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

## 5. Two-layer shell + the EMF seam (detailed)

### 5.1 Why two layers

Tiling the interior removes the interior real estate the module eject/insert
levers used to occupy — levers on the tiled face would leave coverage gaps. So
levers move OFF the interior. But a lever that pierces the 5-layer EMF stack
punches an aperture in the shield. The resolution is a **two-nested-bowl shell**:

- **Outer bowl = the complete EMF envelope.** The full passive stack lives on its
  INNER face, unbroken: CFRP outer (30–50 dB RF) · 0.2 mm mu-metal L2 (15–25 dB
  ELF magnetic) · palladium-polyester L3 (40–60 dB RF) · carbon-loaded absorber
  L4. The Helmholtz cancellation coils mount here too. This bowl is NEVER opened
  for a module swap.
- **Inner bowl = the module carrier.** It holds the keyed sockets, the per-socket
  module levers (on its OUTER, gap-facing face), the fluxgate sensors, and the
  tiled module field on its interior (scalp-facing) face. It **nests inside** the
  outer bowl.
- The two bowls **clamp together for use** and **separate for module replacement.**

### 5.2 The key insight — the split is not a cut in the shield

Because the passive shield is entirely on the outer bowl and the inner bowl nests
**inside** it, the module carrier lives within the Faraday envelope. The
layer-parting plane is therefore a **mechanical** seam between the shielded outer
bowl and the unshielded inner bowl — **not an aperture through the passive
shield.** Module levers sit in the inter-bowl gap and are reached by unclamping
the bowls; **they never pierce the shield.** Module swaps never touch shielding.

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
fingers (or conductive elastomer) at the three clamp latches**, target contact
resistance **≤50 mΩ**. Hard gold resists the fretting/oxidation that would
otherwise raise the bond impedance over clamp cycles.

**(c) Sensor / coil harness crossing.** The active cancellation splits across the
seam by design: **fluxgate magnetometers on the inner bowl** (near the scalp,
where they must sense the field the wearer experiences) and **Helmholtz coils on
the outer bowl** (with the passive shield). Both route to the hub controller via a
**blind-mate connector at the posterior-center clamp**, mated automatically when
the bowls close.

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

- **Three layer-clamp latches:** one anterior, two posterior L/R, seated at the
  rim **between** the ear (audio) and neck attachment zones so they don't collide
  with those accessories. They reuse the recessed-lever concept (flush when
  closed, no snag). The posterior-center latch also carries the blind-mate
  sensor/coil connector.
- Each latch integrates the ground-bond spring fingers (§5.3b) so clamping the
  bowls simultaneously closes the shield-to-ground bond.

### 5.5 Layer-closed interlock (safety + EMF integrity)

A **Hall/contact sensor on each of the three latches** reports closed/open. The
safety architecture **refuses to enable any modality unless all three report
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
   ┌──lever──┐   ← inter-bowl gap: module levers (reached only when unclamped)
 ──────────────  ← inner bowl: sockets + fluxgate sensors
  ▢ ▢ ▢ ▢ ▢ ▢   ← tiled hexagonal module field
 ~~~~~~~~~~~~~~  ← scalp
        ▲
   clamp latch (×3: 1 ant, 2 post L/R) → Hall sensor + BeCu ground-bond fingers
   posterior-center latch also = blind-mate sensor/coil connector
```

## 6. Reliability + manufacturing

- **Levers:** ≤1 N extraction, ≥13 mm arm at W=40 (RISK-22 precedent; Parkinson's
  H&Y II–III accessibility). Below ~34 mm the arm gets too short — a floor driver.
- **IP:** each hex needs a perimeter gasket; total seam length rises vs 5 big
  zones, so IPX4 rides on ~27–30 co-molded gaskets — a per-tile seam-length budget
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
| EMF-1 | Prototype 2-layer attenuation ≥ single-shell baseline | Shield claim |
| EMF-2 | Ground-bond ≤50 mΩ over clamp-cycle life; SHDR trend armed | Driven-shield function |
| MECH-1 | 3-latch clamp + blind-mate connector + Hall interlock detail | Shell tooling |
| DOC-1 | CLAUDE.md §7/§13 integration once GATE-1/2 PASS | Baseline promotion |

## 8. Cross-references

- ISA: `docs/np_hex_zm_isa.md`
- Firmware: `firmware/hub_control/np_module_map.{h,c}`, `tests/np_module_map_tests.c`
- Predecessors: NP-TOOL-ZM-001 (legacy zone-module tooling), NP-DRV-SHELL-001
  (shell FPC routing), NP-HW-FPC-001 (FPC pinout), CLAUDE.md §3/§4.3/§7 (modality
  stack, EMF shielding, durability).
