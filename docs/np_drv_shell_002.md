# Shell Socket Interconnect Architecture

**Project:** NeurOne
**Document:** NP-DRV-SHELL-002
**Revision:** A
**Date:** 2026-07-29
**Status:** DRAFT
**Effective Date:** —
**Author:** NeurOne Mechanical + Hardware Engineering
**Approved By:** — (unapproved; see §1.2)
**References:** NP-HEX-ZM-001 Rev A (§3.4 lattice, §4a taxonomy/SMART-1, §5 two-bowl shell + cluster clamps, §7 open items); NP-HELMET-GEOM-001 Rev A (§0 L0–L3 layer topology, §2 radial stack-up, §3.2 L1 material constraints); NP-HW-HUB-001 Rev B (superseded — TIA gain switch, I2C mux); NP-HW-FPC-001 Rev E (superseded — 20-pin pinout, dual-PD, TIA saturation analysis); NP-DRV-SHELL-001 Rev B (superseded — bend-radius basis, EEG separation); NP-OPT-PSF-001 Rev A; CLAUDE.md §3, §4.1–4.5, §5.1; IPC-2223D; IEC 60068-2-21
**Related Issues:** —
**Gate:** NP-COORD-001 G2 (pre-tooling) — this document is an input to shell tooling release
**IEC 62304 Class:** N/A (hardware architecture; safety-MCU firmware implications flagged, not specified here)
**Supersedes:** NP-DRV-SHELL-001 Rev B (architecture replaced, not revised — see §1.2)
**Parent Document:** None

---

> **⚠ DRAFT — NOT A TOOLING BASELINE.** The socket lattice this document routes to is itself
> **PROVISIONAL** (NP-HEX-ZM-001 §3.4: the ~80-socket v1 lattice is stamped provisional pending
> gates **REG-1** and **ACT-1**). An interconnect cannot be more certain than its lattice. What is
> stable here is the **architecture** — the five-network split, the cluster as the aggregation
> boundary, and the Hub PCB interface contract — all of which are deliberately expressed in terms
> of *topology* (clusters of ≤8 sockets) rather than *lattice counts*, so a REG-1 re-cut changes
> how many clusters are populated and nothing else. Numeric BOM and force figures are engineering
> estimates requiring EE/ME Lead confirmation and are marked as such throughout.

---

## 1. Purpose and scope

### 1.1 What this document replaces

`NP-DRV-SHELL-001 Rev B` specified the routing path for **five** discrete zone-module FPC
bundles: each large (66 × 78 mm) zone module carried a 20-pin FPC tail from its fixed
shell-exterior slot, through a dedicated molded channel in the shell interior, to one of five
Hirose FH34S ZIF receptacles on the Hub PCB. It covered bend radius (§3), shell channel geometry
(§4), multi-FPC bundle management (§2.3), EEG-cable-to-FPC separation (§2.4), and a 23-item
design-review checklist (§5).

`NP-HEX-ZM-001` retired that architecture. There is no "Zone 1..5" any more — there is a
universal 40 mm hex tile populating a lattice of **~80 sockets** across the L1 inner bowl. The
retired document's supersession note states the gap plainly: *"no document yet specifies FPC/cable
routing, bend radius, or channel geometry for the ~80-socket hex-tile lattice."* This document
closes that gap.

It also absorbs three residuals that were left explicitly unowned:

| Residual | Source | Where addressed here |
|---|---|---|
| **SMART-1** — every socket must be I2C/TIA-capable; Hub PCB I2C fan-out for 30–80 sockets "not yet designed" | NP-HEX-ZM-001 §4a, §7 | §3.4, §5, §7 |
| **OI-HUB-SOCKET-01** — per-socket safety enable (today `NP_SAFETY_EN_PBM_ZONE_0..4`) | NP-HEX-ZM-001 §7 | §6 |
| **OI-HEXMAP-02** — module `inventory_fn` (how a module reports UID + elements) | NP-HEX-ZM-001 §7 | §5.4 |

### 1.2 Why a new document number rather than `NP-DRV-SHELL-001 Rev C`

The repo already distinguishes these two cases and this document follows the existing precedent:

- **Same number, new revision** is used when the parent's decisions are *inherited*. `NP-HW-FPC-001
  Rev E` is a variant layered on Rev D — "all base module FPC decisions … are inherited unchanged.
  Only the differences are documented here."
- **New number** is used when the architecture is *replaced*. `NP-HEX-ZM-001` took a new number
  when it replaced the model in `NP-TOOL-ZM-001`, rather than revising it.

Nothing in NP-DRV-SHELL-001's routing architecture is inherited: the five slots, the five bundles,
the five hub connectors, the per-zone channels, the per-zone DRC items and the per-zone open items
are all retired outright. Only two *architecture-independent* things carry over, and they are
re-derived here rather than inherited — the IPC-2223D bend-radius basis (§8) and the EEG artifact
threshold (§9). A new number therefore keeps `NP-DRV-SHELL-001` cleanly SUPERSEDED in the DHF
instead of reviving a retired record.

**Status is DRAFT, not BASELINED**, because REG-1 and ACT-1 are open (see the banner above). This
document is releasable as a design input to Hub PCB Rev C and to shell tooling scoping; it is not
releasable as a tooling baseline.

### 1.3 Out of scope

Shell CAD geometry and molded channel dimensions (successor to NP-TOOL-SHELL-001, authored when
the lattice locks); the Hub PCB Rev C schematic itself (§7 states the interface contract the Rev C
task must meet, not its internals); module-side FPC artwork and pinout for T1-A/B/C (the gap named
in NP-HW-FPC-001's supersession note — §5.1 sizes the socket contact count but releases no
artwork); bus-driver firmware; and any change to the lattice, active surface or 10-20 registration,
which belong to REG-1/ACT-1/ACT-2.

---

## 2. Architecture selection

### 2.1 The three candidates

**Option 1 — one thin FPC tail per socket, individually routed to a hub connector.**
This is the retired architecture scaled up, and it fails on arithmetic before it fails on physics.

| Metric | Retired (5 slots) | Option 1 at 80 sockets |
|---|---|---|
| Hub-side connectors | 5 × Hirose FH34S-20S | **80** ZIF receptacles |
| Conductors terminating at the Hub PCB | 100 | **1,600** |
| Discrete flex tails in the inter-bowl gap | 5 | **80** |
| ZIF lever access openings requiring 8 mm finger clearance (retired REQ-ST-06) | 5 | **80** |

A Hub PCB edge that seats 80 ZIF receptacles at 0.5 mm pitch needs roughly 80 × 12 mm ≈ **960 mm
of connector edge** — an order of magnitude more than the hub enclosure has (NP-TOOL-HUB-001). And
80 tails must share the inter-bowl gap that NP-HEX-ZM-001 §5.4a reserves for the cluster clamp
plates. **Rejected.**

**Option 2 — a single monolithic rigid-flex backplane spanning the whole L1 inner bowl.**
Electrically attractive (one part, one crossing) and mechanically hostile. L1 is a *compound
doubly-curved* bowl, so a single backplane must either be formed 3D (very high NRE for a low-volume
part) or be a large flat-ish sheet that fights the curvature and pushes the module body depth past
the 18–22 mm L1 subtotal budgeted in NP-HELMET-GEOM-001 §2. Worse, it makes the entire socket layer
a **single non-repairable part**: one damaged socket condemns the whole bowl. It also maximizes the
conductive area on L1, which is the layer NP-HELMET-GEOM-001 §3.2 constrains to *non-magnetic,
low-eddy* precisely because the fluxgate magnetometers mount there. **Rejected as the primary
structure** — though §4 keeps its one genuinely good idea, a laminated (not harnessed) conductor
path.

**Option 3 — cluster carriers (selected).**
Sockets are grouped into clusters; each cluster gets one small rigid-flex **cluster carrier board**
that hosts its sockets and all local conditioning, and presents *one* tail toward the hub.

The cluster is not invented here. NP-HEX-ZM-001 §5.4a already defines it for a purely mechanical
reason — a lever per module does not scale, so modules are clamped in clusters of 3–7 with one
over-centre actuator per cluster. That mechanical unit turns out to be the correct electrical
aggregation boundary for an entirely independent reason (§2.2). The interconnect therefore adds no
new partitioning of the bowl; it electrifies a partition the clamp design already committed to.

### 2.2 Why the cluster is the right electrical boundary

Decompose what actually has to cross the gap between a tile and the Hub PCB, and the signals sort
into three distance classes:

| Class | Signals | Tolerates distance? |
|---|---|---|
| Quasi-DC power | LED rail, logic rail, returns | **Yes** — bounded by IR drop and loop area, both engineerable |
| Digital | UID/command I2C, sample sync, fault alert | **Yes** — 400 kHz over ≤300 mm is unremarkable |
| High-impedance analog | PD1/PD2 photocurrent (14–72 µA), NTC, EEG electrode potential (sub-µV) | **No** — susceptibility scales with run length in exactly the band the therapy occupies |

The retired architecture sent **all three** classes down one bundle to the Hub PCB, because with
five modules it could. That inherited choice — *"the Hub PCB is where module signals are
conditioned"* — is what makes SMART-1's residual look unaffordable: it implies ~80 TIA front ends,
~160 high-impedance analog channels crossing the shell, and ~80 DG2788A gain switches on one board.

Retire the choice and the problem stops scaling with socket count. If high-impedance analog is
converted within **≤50 mm** of the tile, the only things that travel are power and digital. The
conversion point must be close to every tile, must already exist mechanically, and must have a
bounded fan-in. The cluster is all three.

**Cost consequence:** analog front ends scale with **cluster count (~12)**, not socket count (~80)
— the same order of magnitude as the five the architecture replaced.

### 2.3 The one-line statement of the change

> The retired architecture gave every module a **tail**. The replacement gives every module a
> **footprint**.

A hex tile has no FPC tail. It presents a contact-pad array on its back face and is seated by the
cluster clamp's own per-module spring plunger (NP-HEX-ZM-001 §5.4a). Everything that follows —
including the disappearance of dynamic flex (§8.2) — falls out of that.

---

## 3. The five networks

Each network is assigned the topology its physics wants, exactly as NP-HEX-ZM-001 §5.3.1 assigned
coils and sensors to the layer each one's physics wanted.

```
                                        ┌──────────────┐
   scalp ── L0 module faces             │  Hub PCB     │  (occipital arch, outside the shield)
        ▢ ▢ ▢ ▢ ▢ ▢ ▢ ▢ ▢ ▢            │  Rev C       │
        │ │ │ │ │ │ │ │ │ │            └──────┬───────┘
   ═════╪═╪═╪═╪══════╪═╪═╪═════  L1           │
     ┌──┴─┴─┴─┴──┐ ┌─┴─┴─┴──┐                 │  blind-mate boss
     │ CLUSTER 1 │ │CLUSTER n│  ×~12          │  (posterior-centre, ONE aperture,
     │ mux·AFE·  │ │         │                 │   SEGREGATED returns — §4.3)
     │ ADC·switch│ │         │                 │
     └─────┬─────┘ └────┬────┘                 │
           │            │                      │
      ═════╧════════════╧══════════════════════╪═══  N1 power · N2 control · N4 electrode ·
                  posterior aggregation node ──┘     N5 safety      (N3 never leaves a cluster)
```

| # | Network | Carries | Topology | Terminates at |
|---|---|---|---|---|
| **N1** | Power distribution | `VLED+` / `PGND`, `V3V3` / `DGND` | Broadside-coupled **tree** from the posterior aggregation node (PAN); never a ring (§9.4) | Cluster carrier local switch |
| **N2** | Control | `SDA`, `SCL`, `SYNC`, `ALERT#` | Two-level segmented I2C tree (§3.4) + broadcast sync | Per-socket I2C device |
| **N3** | Local sense | `PD1`, `PD2`, `NTC` | Point-to-point, **≤50 mm**, tile → cluster carrier | Cluster ADC — **never leaves the cluster** |
| **N4** | Electrode | EEG record lanes, tES drive lanes | Per-cluster mux onto **N shared guarded lanes** (§3.5) | ADS1299 bank at the PAN |
| **N5** | Safety enable | `SAFE_EN_n`, one per cluster | Star, direct from the Safety MCU | Cluster PDN hard gate (§6) |

### 3.1 Cluster definition (topological, not lattice-bound)

A **cluster** is **≤8 sockets** that share one carrier board, one clamp actuator, one I2C segment
and one safety-enable domain. The natural super-cell of a hex lattice is the 7-hex "flower"
(1 centre + 6 neighbours), which NP-HEX-ZM-001 §5.4a already names; ≤8 is stated as the bound so
the boundary clusters that cannot form a complete flower are covered by the same rule.

At the provisional 80-socket v1 lattice this gives **12 clusters** (12 × 7 = 84 capacity).

**Nothing downstream of this section depends on 80 or on 12.** The tail pinout (§5.2), the hub
interface (§7) and the safety granularity (§6) are all expressed per cluster. A REG-1 re-cut
changes how many cluster carriers are populated — not the architecture, not the Hub PCB interface,
not the firmware addressing.

### 3.2 Cluster carrier board contents

| Function | Part class | Qty per carrier | Note |
|---|---|---|---|
| Socket contacts | spring-contact array | ≤8 × 18 (§5.1) | Seated by the §5.4a clamp plunger |
| I2C segment switch | 8-channel I2C switch (TCA9548A class) | 1 | Per-socket isolation — resolves the shared-0x30 collision |
| PD/NTC front end | switched-gain TIA + mux + ADC | 1 set | Replaces the per-socket Hub PCB TIA (§3.3) |
| Electrode mux | low-leakage analog mux | 1 | Record/stim path select (§3.5) |
| Cluster power gate | high-side load switch | 1 | Gated by `SAFE_EN_n` (§6) |
| Local bulk decoupling | ceramic + polymer bulk | — | Sized for edges/carrier only (§9.2) |

### 3.3 What moving the TIA to the cluster resolves

`NP-HW-HUB-001 Rev B` §3 puts one Vishay DG2788A per zone slot on the Hub PCB, switching TIA
feedback between 47 kΩ (silicon PD) and 22 kΩ (InGaAs PD), because InGaAs responsivity at 1064 nm
is ~2× silicon and saturates the 3.3 V rail otherwise. That analysis is physics and survives
unchanged. What does not survive is its *placement*.

Per-cluster placement changes three things:

1. **Gain selection stops being a GPIO problem.** Rev B needed one `GAIN_SEL[n]` line per slot.
   With the TIA at the cluster, gain is a register write to the cluster's own front end over the
   existing I2C — **zero new hub GPIO**, at any socket count.
2. **The high-impedance node shrinks from ~200 mm to ≤50 mm**, which is the actual reason to do
   it. Rev B's own §8.1 layout note asks for `< 5 mm` from switch to op-amp on the *Hub PCB*; that
   care is wasted if the photocurrent has already crossed the shell.
3. **PD1/PD2 ratio integrity improves.** The RISK-14 Option B dual-PD scheme depends on the
   PD1/PD2 *ratio* (fouling vs LED aging). A ratio taken between two channels on the same small
   carrier, sharing one reference and one thermal environment, is better conditioned than one taken
   across two long shell-crossing runs.

### 3.4 Control network — the two-level tree, and why 128

All modules of a given type present the same I2C address (the retired smart module was factory-burned
to 0x30, and a base-tile UID EEPROM likewise has a fixed address), so isolation, not addressing, is
the constraint. The tree is:

```
Hub PCB host I2C
   └── hub-level 8-channel switch ────────── 8 branches
          └── ≤2 cluster carriers per branch  (distinct switch addresses, e.g. 0x70 / 0x71)
                 └── cluster 8-channel switch ── ≤8 sockets
```

**8 branches × 2 clusters × 8 sockets = 128.**

That is exactly `NP_HEXMAP_MAX_SOCKETS`, which NP-HEX-ZM-001 §3.4 set to 128 as the full 7-bit
major domain specifically to avoid "a different guessed margin." The physical bus tree and the
firmware addressing ceiling therefore agree with **no arbitrary sub-ceiling to re-justify** — the
same reasoning that produced 128 in firmware produces 128 here independently.

**Two access modes:**

- **Addressed read** — exactly one branch channel and one cluster channel open. Used for UID /
  element inventory (`np_module_map` power-on poll), PD/NTC readback, and fault interrogation.
- **Broadcast write** — all branch and cluster channels open simultaneously. Every module at the
  same address ACKs the same write together, so a whole-vault start/stop/parameter update is one
  transaction, not 80. This is what keeps session start deterministic; see **OI-SHELL2-06** for the
  dwell-time budget that bounds the addressed-read case.

### 3.5 Electrode network — sized by channel count, not socket count

This is the hardest constraint in the redesign and it is treated in full in §9.1. The sizing rule:

> The electrode network is sized by the **EEG/tES channel count** (8 for T1, ~21 scalp for T2), not
> by socket count. Any socket may host a T1-B tile, but only 8 (T1) / ~21 (T2) are ever
> *simultaneously* electrode-active, because that is the montage.

Each cluster carrier muxes its own sockets' electrode contacts onto the shared guarded lanes; the
lanes run on L1's scalp-facing face under a DRL-driven guard to the ADS1299 bank at the PAN. This
preserves the ADS1299 choice locked in CLAUDE.md §3 modality 3 and keeps electrode analog off the
parting plane entirely.

---

## 4. Physical routing

### 4.1 Layer assignment within L1

| Face of L1 | Carries | Why |
|---|---|---|
| Scalp-facing (inboard) | Sockets; **N4** electrode lanes under a DRL-driven guard plane | Electrode analog gets the quiet face and its own guard |
| Laminate core | **N1** broadside supply/return pairs; **N2**, **N5** | Loop area minimized by construction (§9.3) |
| Gap-facing (outboard) | Cluster carrier components; clamp plates (mechanically independent, §8.1) | Serviceable side; components out of the scalp thermal path |

The L1 laminate wall itself separates N1 from N4. This *reproduces the retired design's mechanism —
"opposite sides of a wall" — but at ~2–3 mm instead of the 18–22 mm the shell wall gave.* §9.1 states
plainly why that is not sufficient on its own and what has to make up the difference.

### 4.2 Cluster tails to the posterior aggregation node

All ~12 cluster tails converge on a **posterior aggregation node (PAN)** on L1 at the occiput
centreline, co-sited with the standalone blind-mate boss that NP-HEX-ZM-001 §5.3c already puts
there for the fluxgate/coil harness. The PAN hosts the hub-level I2C branch switch, the ADS1299
bank, the tES driver interface, and the PDN feed point.

Tails are **laminated into L1**, not harnessed — formed once at assembly and never handled again.
This is Option 2's one good idea kept without Option 2's monolith.

### 4.3 The parting-plane crossing — one aperture, segregated returns

NP-HEX-ZM-001 §5.3a makes the electromagnetic case for minimizing apertures: any continuous
residual slot must stay ≤ λ/20 at 6 GHz ≈ 2.5 mm. Adding a **second** crossing for the module
interconnect would be a second aperture to defend. So the module interconnect **shares the existing
posterior-centre blind-mate boss**.

But sharing an aperture must not become sharing a *return*. The Helmholtz coils are the actuator of
a cancellation loop whose sensor (the fluxgates) sits on L1; if the coil-driver return current and
the LED bus return current share an impedance at the boss, the actuator modulates its own sensor's
reference. The loop closes and the controller chases a field it is itself creating — nulling at the
fluxgate while the field at the brain diverges. That failure has the same shape as FMEA-G07-01: the
sensor reads nominal while the hazard grows.

**Requirement:** the boss presents **segregated contact groups with independent returns** —
{N1 power}, {N2/N5 digital+safety}, {N4 post-ADC digital}, {fluxgate/coil harness} — star-returned
at the Hub PCB, with no shared return conductor between the power group and the fluxgate/coil group.
See **REQ-EMI-05** and **OI-SHELL2-02**.

> **Time-boxed.** This is cheap to specify now and expensive later: it constrains the boss contact
> layout, which **MECH-1** tools. It must be settled before MECH-1 cuts the posterior boss.

---

## 5. Electrical interface

### 5.1 Socket contact budget

| # | Contact | Qty | Network | Note |
|---|---|---|---|---|
| 1 | `VLED+` | 2 | N1 | Two contacts for current sharing and redundancy |
| 2 | `PGND` | 2 | N1 | Paired with `VLED+`; LED return only |
| 3 | `V3V3` | 1 | N1 | Logic rail, ≤50 mA/tile |
| 4 | `DGND` | 1 | N1 | Logic return |
| 5 | `SDA` | 1 | N2 | Cluster-switched segment |
| 6 | `SCL` | 1 | N2 | |
| 7 | `SYNC` | 1 | N2 | Broadcast sample/pulse-phase reference (§9.5) |
| 8 | `ALERT#` | 1 | N2 | Open-drain module fault |
| 9 | `PD1_K` | 1 | N3 | Forward-emission photodiode |
| 10 | `PD2_K` | 1 | N3 | Scalp-facing backscatter photodiode |
| 11 | `NTC` | 1 | N3 | Per-tile thermistor, 42 °C/62 °C interlock chain |
| 12 | `AGND` | 1 | N3 | Sense return — **separate from `PGND`** |
| 13 | `ELEC` | 1 | N4 | Dual-rated electrode: EEG record **and** tES drive |
| 14 | `GUARD` | 1 | N4 | DRL-driven guard for contact 13 |
| 15 | reserved | 2 | — | OTA-extensibility margin (§4a "grow-to-4" type, bipolar electrode) |
| | **Total** | **18** | | |

**No `ZONE_ID` contact.** SMART-1 retired the resistor ladder; identity is a UID read over I2C.

**No dedicated presence contact.** Presence *is* a successful I2C probe — which is exactly the
`np_module_map` UID-change poll (NP-HEX-ZM-001 §4). This makes **OI-HEXMAP-02**'s `inventory_fn` a
plain I2C read. The minimum viable module identity device is a fixed-address UID EEPROM
(24AA02UID class, ~$0.12 est.) on T1-A/T1-B; T1-C's existing ATtiny402 already serves the role.

> **Contact count is an accessibility variable, not only an electrical one.** Spring contacts need
> roughly 0.3–0.5 N each, so 18 contacts is ~5.4–9.0 N per module and ~38–63 N per 7-tile clamp
> plate (est.). RISK-22 / §5.4a require low **one-handed input force** via the over-centre
> actuator's mechanical advantage. Dropping 18 → 12 contacts would cut plate load by a third. This
> coupling must be closed jointly with **MECH-2** and the §5.4a HFE formative — see
> **OI-SHELL2-03**.

### 5.2 Cluster tail pinout

| # | Conductor | Network | Function |
|---|---|---|---|
| 1–2 | `VLED+` (×2) | N1 | Cluster LED rail feed |
| 3–4 | `PGND` (×2) | N1 | Broadside-coupled return for 1–2 (§9.3) |
| 5 | `V3V3` | N1 | Cluster logic rail |
| 6 | `DGND` | N1 | Logic return |
| 7 | `SDA` | N2 | Branch I2C segment |
| 8 | `SCL` | N2 | |
| 9 | `SYNC` | N2 | Broadcast phase reference |
| 10 | `ALERT#` | N2 | Open-drain, wire-OR across cluster |
| 11 | `SAFE_EN_n` | N5 | **Direct from the Safety MCU** (§6) |
| 12 | `SHLD` | — | Drain / guard reference |
| | **12 conductors per cluster tail** | | |

N3 does not appear — it terminates on the carrier by design. N4 lanes are a separate shared bundle
(§3.5), not part of the per-cluster tail.

### 5.3 Conductor count — the honest comparison

| Design | Sockets served | Hub-side connectors | Conductors at the Hub PCB |
|---|---|---|---|
| Retired NP-DRV-SHELL-001 | 5 zone modules | 5 × 20-pin ZIF | 100 (+10 separate EEG cables = 110) |
| Option 1 (per-socket tails) | 80 | 80 × 20-pin ZIF | **1,600** |
| **This document (cluster carriers)** | **80** | **12 × 12-pin** | **144**, EEG included |

The replacement is **~1.3× the retired conductor count while serving 16× the sockets and absorbing
the EEG harness**, and **~11× fewer conductors** than the naive per-socket design. It is *not* a
reduction against the retired design and this document does not claim one — it is a change from
scaling with socket count to scaling with cluster count.

### 5.4 Power budget

The load-bearing fact is that **vault PBM power is capped by CLAUDE.md §4.5 regardless of socket
count**: T1 peak ~45–50 W and T2 peak ~70–74 W are *total system* draws, and the per-zone NTC
throttle at 62 °C junction plus the ≤25 % duty cap bound it further. Eighty sockets therefore draw
no more aggregate current than five did; the socket count changes *distribution*, not *magnitude*.

| Quantity | Value | Basis |
|---|---|---|
| Vault PBM instantaneous ceiling | ~35 W (est.) | T1 peak 45–50 W less processor/EEG/audio/hub |
| Bus rail | 12 V nominal (est., **OI-SHELL2-01**) | Sets IR-drop and contact current |
| Vault bus current at ceiling | ~2.9 A | 35 W / 12 V |
| Per-tile instantaneous allocation | ≤3 W / ~0.25 A (est.) | Sized for a *hot zone*, not the uniform share |
| Per-cluster feed (7 tiles hot) | ~1.75 A (est.) | Two `VLED+` conductors per tail |
| Whole-vault simultaneous full drive | **not permitted** | Firmware vault-wide budget + PDN feed hard cap |

Per-tile allocation deliberately exceeds the uniform share (35 W / 80 = 0.44 W) because real
protocols drive a zone, not the vault. The vault-wide total is then enforced twice — in firmware
and by the PAN feed rating — so a firmware fault cannot exceed the thermal envelope.

---

## 6. Safety architecture

CLAUDE.md §4.2 is unambiguous: the Safety MCU **physically owns all stimulation enable GPIO**, so
that an app or main-processor fault cannot cause unsafe stimulation. Today that is
`NP_SAFETY_EN_PBM_ZONE_0..4` — five enables for five zone slots. Eighty sockets cannot each have a
Safety MCU GPIO, and routing enable authority over the I2C bus would put the main processor in the
path, destroying the property the architecture exists to guarantee.

**Decision: hardware enable at cluster granularity; software enable at socket granularity.**

| Layer | Granularity | Mechanism | Defeatable by a main-processor or bus fault? |
|---|---|---|---|
| Hardware | **Per cluster (~12)** | `SAFE_EN_n` from the Safety MCU gates the cluster's high-side load switch — no rail, no emission | **No** |
| Software | Per socket (~80) | Module driver register over I2C | Yes — which is why it is not the safety layer |
| Global | Whole vault | PAN feed cut | **No** |

Consequences and rationale:

- **Granularity improves, it does not regress.** Five hardware enable domains become ~12. The
  Safety MCU is an STM32G071 with ample GPIO for 12 enables plus the existing modality enables.
- **Fail-safe by construction.** `SAFE_EN_n` low removes the LED rail from the cluster. No module
  firmware state, no stuck I2C transaction and no main-processor hang can produce emission from a
  de-energized cluster. This preserves the <50 ms all-stimulation cutoff on watchdog expiry
  (CLAUDE.md §4.2) because the cut is a rail cut, not a message.
- **`ALERT#` is wire-OR per cluster**, so a module fault reaches the Safety MCU without a bus
  transaction.
- **The single-socket hazard case closes, and coarsening costs availability rather than safety.**
  The obvious objection to cluster granularity is a fault confined to one socket — a module driver
  stuck on while its neighbours run legitimately. Cutting the cluster rail still stops it, because
  the faulted socket is inside the cluster; what per-cluster granularity costs is that seven tiles
  go dark instead of one. That is an **availability** regression, not a safety one, and it is the
  conservative direction. The detection path that closes the loop is unchanged and per-socket:
  per-tile NTC (42 °C/62 °C throttle chain), per-tile PD dose metering, and `ALERT#` wire-OR — all
  of which reach the Safety MCU without needing the main processor or a successful bus transaction.
  No hazard is reachable at socket granularity that cluster-granularity enable cannot stop.
- **This is the interconnect half of OI-HUB-SOCKET-01.** That open item asks for a "socket-indexed
  control registry + per-socket safety-MCU enable." This document supplies the hardware half and
  narrows the ask: per-socket enable is *software*; per-cluster enable is *hardware*. The firmware
  half — the socket-indexed control registry, and the present fail-closed behaviour where
  `dispatch_command()` drops a socket-addressed target rather than falling back to the slot path —
  remains with OI-HUB-SOCKET-01.
- **tES path interlock.** The `ELEC` contact is dual-rated (record + stimulate). The cluster
  carrier's record/stim selector must be gated by `SAFE_EN_n` as well, so the ≤2 mA (T1) / ≤4 mA
  (T2) drive path cannot be established on a de-energized cluster. The 40 µC/cm² charge-density
  limit remains where CLAUDE.md §4.2 puts it — in the Safety MCU, not here.

---

## 7. Hub PCB Rev C interface contract

This section is the coordination surface for the parallel Hub PCB Rev C work. It states what Rev C
must provide and — equally important — what SMART-1's residual made it look like Rev C would need,
but does not.

### 7.1 What Rev C must provide

| Item | Quantity | Note |
|---|---|---|
| Cluster tail connectors | **12** (spec **16** positions) | 12 populated at the v1 lattice; 16 positions = 8 branches × 2, giving lattice headroom with no re-layout |
| Pins per connector | **12** | Pinout fixed by §5.2 |
| Total interface pins | **144** (192 if all 16 populated) | vs 100 on the retired 5-slot design |
| Host I2C branch switch | 1 × 8-channel | The hub-level stage of the §3.4 tree |
| Safety-MCU enable GPIO | **12** (16 provisioned) | `SAFE_EN_n`, Safety MCU sourced, default LOW at reset |
| `SYNC` driver | 1 | Broadcast, phase-locked to the EEG sample frame (§9.5) |
| `ALERT#` input | 1 per cluster, wire-OR | Or one wire-OR aggregate with I2C interrogation |
| PDN feed | 1 | Rated at the vault ceiling, hard-capping §5.4 |
| ADS1299 bank interface | SPI | The bank sits at the **PAN on L1**, not on the Hub PCB (§3.5) |

### 7.2 What Rev C does **not** need

| Feared requirement | Why it is not needed |
|---|---|
| ~80 × DG2788A TIA gain switches | TIA and its gain select moved to the cluster carriers (§3.3); gain is an I2C register write |
| ~160 high-impedance analog channels crossing the shell | N3 terminates on the cluster carrier and never leaves it |
| ~80 `GAIN_SEL` GPIO | Zero. See above |
| Cascaded multi-stage muxing for 80 sockets | Two levels suffice: 8 branches × ≤2 clusters × ≤8 sockets = 128 (§3.4) |
| 80 ZIF receptacles | 12 (16 provisioned) |

The still-reusable content of `NP-HW-HUB-001 Rev B` is exactly what its own supersession note
predicts: *"the per-socket circuit topology — one DG2788A dual-SPDT switch covering both PD1 and
PD2 TIA gain from a single `GAIN_SEL` line — is component-level and per-socket-independent."*
Correct — and it is replicated onto the cluster carriers, not onto Rev C.

### 7.3 Assumptions the Rev C task should challenge if it disagrees

1. 12 V bus rail (**OI-SHELL2-01**) — sets contact current and IR drop.
2. 12-conductor tail (§5.2) — adding a conductor costs 16 hub pins.
3. ADS1299 bank located at the PAN, not the Hub PCB — moves an SPI interface across the boss.
4. `SAFE_EN_n` sourced by the Safety MCU, not the i.MX RT1062.

---

## 8. Mechanical requirements

### 8.1 Compatibility with the cluster clamp (MECH-2)

The cluster carrier and the cluster clamp plate are **different parts on opposite sides of the same
cluster footprint**, and they must stay decoupled:

- The **carrier is fixed** — laminated into L1, never moves.
- The **clamp plate moves** — lifting it releases 3–7 modules onto their ejector springs.

If the carrier were the clamp plate, every module swap would cycle the electrical connection of an
entire cluster. Keeping them separate means throwing the clamp actuates **no electrical
disconnection anywhere except at the module pads themselves**, which is where a separable interface
belongs. The plate's per-module spring plungers supply socket contact force (§5.1) through
clearance features in the plate; the plate carries no conductors.

### 8.2 Flex — what actually moves

| Path | Flex class | Requirement |
|---|---|---|
| Module ↔ socket | **None** — compression contact | Contact force per §5.1; wipe per **SH2-DRC-08** |
| Cluster carrier ↔ PAN (tails) | **Static only** — formed once at assembly | ≥12.5 mm static bend radius |
| PAN ↔ Hub PCB | **None** — blind-mate, translational | No bend permitted within 5 mm of the boss |
| Fluxgate / Helmholtz harness | Dynamic (unchanged, out of scope here) | ≥25 mm dynamic retained |

**The module interconnect has no dynamic-flex path.** This is the single largest reliability change
against the retired architecture, where every module swap actuated a 20-pin FPC through three bend
segments and the dominant failure mode was fatigue cracking at a stiffener edge over 1,000 swap
cycles (retired §2.2 Segment A/C, "catastrophic, immediate failure").

### 8.3 Bend radius requirements (carried-over basis, re-derived consumers)

The IPC-2223D basis in the retired §3 is architecture-independent and carries over. Its *consumers*
change, so the requirements are restated rather than inherited.

| ID | Requirement |
|---|---|
| **REQ-BR2-01** | Static bend radius ≥ **12.5 mm** at every formed bend in a cluster tail. IPC-2223D Table 4-1 gives 6 × total thickness as the dynamic floor (~1.2 mm at 0.20 mm stack); 12.5 mm is retained from the retired document as the deliberately conservative static value. |
| **REQ-BR2-02** | ≥ **25 mm dynamic** applies to any path that flexes in service. In this architecture that set is **empty** for the module interconnect. If a later revision reintroduces a service loop, this requirement binds it. |
| **REQ-BR2-03** | No bend within **5 mm** of a rigid-flex transition, stiffener edge, blind-mate boss, or cluster carrier edge. Bending at these transitions delaminates the flex from the stiffener rather than merely fatiguing copper. |
| **REQ-BR2-04** | L1 lamination geometry must **enforce** REQ-BR2-01 by construction. A tail must not be capable of taking a tighter radius during L1 assembly. |
| **REQ-BR2-05** | Rigid-flex bend regions must not fall under a cluster clamp plate footprint or under a socket, so clamp load never bears on a formed bend. |

### 8.4 Serviceability

Cluster granularity makes the inner bowl **field-repairable at cluster level** — a damaged carrier
replaces 7 sockets, not the whole bowl. This is the concrete advantage over Option 2 and it should
be reflected in the service-network tiering (`docs/reference/service-network.md`) — noted as
**OI-SHELL2-08**, not decided here.

---

## 9. EMI, EEG integrity, and the shielding envelope

### 9.1 The EEG separation constraint — the mechanism is gone, the threshold is not

The retired requirement was ≥15 mm PBM-to-EEG separation, verified in CAD (DRC-18a) with a
prototype oscilloscope check that injected EEG artifact stays **< 5 µVpp** (DRC-18c). It was met by
a geometric trick: *"Zone module FPCs (scalp-side) and EEG cables (outer-side) are on opposite sides
of the CFRP shell wall,"* achieving 18–22 mm and closing DRC-18 **without a barrier**.

**That trick is unavailable.** EEG electrodes now live *inside* T1-B hex tiles (NP-HEX-ZM-001 §4a),
on the same L1 carrier as PBM drive current, at a distance set by the 40 mm tile — not by the shell
wall. Within L1 the best geometric separation available is the laminate wall, ~2–3 mm.

**Therefore: ≥15 mm is not achievable and this document does not claim it.** Separation was always
a *mechanism*; <5 µVpp is the *end*. The end is retained and met by four mechanisms in combination:

| # | Mechanism | Requirement |
|---|---|---|
| 1 | **Shorten the analog path** — electrode analog is muxed at the cluster onto guarded lanes and digitized at the PAN; N3 never leaves its carrier | REQ-EMI-01 |
| 2 | **Guard, don't separate** — N4 lanes run under a continuous DRL-driven guard plane on L1's scalp-facing face, sandwiched away from N1 by the laminate core | REQ-EMI-02 |
| 3 | **Make the artifact deterministic and therefore subtractable** — §9.5 | REQ-EMI-03/04 |
| 4 | **Minimize the source** — broadside loop-area control, §9.3 | REQ-EMI-06 |

> **Verification changes, the threshold does not.** DRC-18c's oscilloscope test (EEG recording, all
> LEDs at full PWM load, artifact < 5 µVpp) is retained verbatim as **SH2-DRC-16**. It is now a
> harder test to pass and a more important one, because no geometric margin backs it up.

### 9.2 Why the therapeutic band cannot be filtered off the bus

A tempting answer is "decouple it on the tile." The arithmetic refuses:

To hold a 40 Hz / 25 % duty pulse (6.25 ms on-window) at 0.25 A within a 100 mV rail droop:

> C = I·t/ΔV = 0.25 A × 6.25 ms / 0.1 V = **15.6 mF per tile**

That is not placeable on a 40 mm tile. For the PWM carrier (≈20 kHz, 25 µs) the same expression
gives ~62 µF, and for switching edges a 100 nF + 10 µF pair suffices.

**Conclusion:** on-tile decoupling kills the carrier and the edges and **cannot touch the
therapeutic envelope**. The 0.5–100 Hz modulation *is* the therapy (CLAUDE.md §3: seven frequency
presets, 2/6/10/20/40 Hz), and it is by construction bus current in exactly the EEG band and
exactly the ELF band the Helmholtz loop cancels. It must be handled geometrically and
computationally, never by filtering.

### 9.3 Loop-area control — the primary mitigation

| Geometry | Loop area over a 200 mm cluster feed | Dipole field at 30 mm, 0.25 A @ 40 Hz (est.) |
|---|---|---|
| Same-layer pair, 5 mm apart | ~1,000 mm² | ~1.9 µT |
| **Broadside pair, 0.1 mm dielectric** | **~20 mm²** | **~37 nT** |

*(B ≈ µ₀·I·A / 2πr³, on-axis, order-of-magnitude only — bench confirmation is **SH2-DRC-17**.)*

Fifty-fold. And 37 nT is still the same order as the ambient ELF the cancellation loop exists to
null, which is precisely why §9.5 is mandatory rather than a refinement.

| ID | Requirement |
|---|---|
| **REQ-EMI-06** | Every `VLED+`/`PGND` pair is **broadside-coupled with full overlap** across its entire run, dielectric ≤0.2 mm. Loop area per cluster feed ≤ **25 mm²**. |
| **REQ-EMI-07** | Supply and return travel together for the whole path. **The LED return may not use a plane, a chassis, or any structure other than its paired `PGND` conductor.** |

### 9.4 Two prohibitions

| ID | Prohibition | Reason |
|---|---|---|
| **REQ-EMI-08** | **The shell, the shield stack, the mu-metal, and any DRL-driven structure may NOT serve as an LED or tES current return path.** | CLAUDE.md §4.3 bonds the shell to the EEG DRL output — it is a *driven shield*. Returning therapeutic-band current through it injects that current directly into the EEG reference. This is the single most consequential rule in this document. |
| **REQ-EMI-09** | **The N1 power distribution may NOT form a closed ring around L1.** Topology is a tree from the PAN. | A ring is a literal loop antenna at bowl scale and a ground loop; it would defeat REQ-EMI-06 no matter how well each segment is coupled. |

### 9.5 The fluxgate interaction — deterministic, not dithered

NP-HEX-ZM-001 §5.3.1 reason 3 kept the Helmholtz coils *off* L1 because *"the inner carrier holds
the EEG electrodes, tES drivers, and their µV-sensitive FPC leads. A dB/dt source millimetres away
injects its drive current and switching harmonics … straight into the recording it exists to
protect."* The N1 bus is a dB/dt source on L1. The rationale that justified the coil/sensor split
does not automatically survive it, and this document must not quietly weaken it.

Three requirements follow, and one of them inverts standard EMC practice:

| ID | Requirement |
|---|---|
| **REQ-EMI-03** | **Bus traffic and PBM pulse phase are deterministic and phase-locked to the EEG/fluxgate sample frame.** Reserve **sense-quiet windows** in which no I2C transaction and no commanded pulse edge occurs during an acquisition aperture. `SYNC` (§5.1 contact 7) exists for this. |
| **REQ-EMI-04** | **Spread-spectrum / dithered PWM is prohibited on the LED drivers.** The reflexive EMC fix smears bus energy into broadband noise across the EEG band, which neither the notch nor subtraction can remove. A deterministic artifact is a *known line* and is subtractable; a dithered one is irreducible noise. |
| **REQ-EMI-05** | **Feed-forward self-field subtraction.** The cancellation controller subtracts a *predicted* self-field rather than chasing the measured one. The prediction must bind to **measured PD/dose telemetry, not commanded current** — LED aging, PD-metered compensation and the 62 °C throttle all make actual current diverge from commanded, and a command-based model goes stale exactly when the throttle is active. This extends the existing "synchronous Helmholtz subtraction from EEG" and TMS-gated cancellation primitives (CLAUDE.md §4.3). |

Plus two structural rules:

| ID | Requirement |
|---|---|
| **REQ-EMI-10** | **No conductive addition to L1 without fluxgate re-qualification.** NP-HELMET-GEOM-001 §3.2 constrains L1 to non-magnetic / low-eddy *because the fluxgates mount there* — CFRP is rejected for L1 on that ground alone. The standard EMC response to bus artifact (guard pours, ferrites, shield cans) would itself violate that invariant. Any added conductive mass on L1 requires re-qualification against the fluxgate accuracy budget. |
| **REQ-EMI-11** | **Cancellation calibration is configuration-dependent.** Because any tile type may occupy any socket (§4a) and blanking plugs may be fitted, the bus current distribution — and therefore the self-field at each fluxgate — changes per configuration. §5.3.1 reason 4's "fixed coil-drive→field transfer" no longer holds unconditionally. Recalibration must be triggered on `np_module_map` rebuild, which already fires on any insertion change. |

### 9.6 The shielding envelope is not breached

Everything in N1–N5 lives **inside** the Faraday envelope: NP-HEX-ZM-001 §5.2 establishes that the
passive stack is entirely on the outer bowl and L1 nests inside it, so the parting plane is a
mechanical seam and not an aperture through the shield. This interconnect adds **no new aperture** —
it shares the existing posterior boss (§4.3). The external RF claim (≥40–60 dB RF, ≥35–45 dB ELF
magnetic) is therefore unchanged in mechanism.

What *is* new is an **internal** emitter inside the envelope. A Faraday cage does not protect the
EEG electrodes and fluxgates that share the enclosure with the source — which is why §9.1–9.5 are
requirements on the bus itself rather than on the shield. **EMF-1**'s acceptance criterion
(prototype two-layer attenuation ≥ single-shell baseline) is necessary but **not sufficient** for
this document; **SH2-DRC-16/17** add the internal measurements it does not cover.

---

## 10. BOM and manufacturing impact

### 10.1 BOM delta (per headset, estimates — EE Lead confirmation required)

> All figures below are **engineering estimates at architecture level**, derived from the component
> classes named in §3.2 and unit costs in NP-HW-HUB-001 Rev B §7 / NP-HW-FPC-001 Rev E §6.2. They
> are not quotations and must not be carried into a costed BOM without EE Lead confirmation
> (**OI-SHELL2-05**).

| Line | Qty | Unit (est.) | Total (est.) | Note |
|---|---|---|---|---|
| Cluster carrier rigid-flex PCB | 12 | $3.50–6.00 | $42–72 | Dominant new cost |
| I2C 8-channel switch (cluster) | 12 | $0.50 | $6.00 | TCA9548A class |
| I2C 8-channel switch (hub branch) | 1 | $0.50 | $0.50 | |
| Switched-gain TIA + mux + ADC set | 12 | $1.50–3.00 | $18–36 | Replaces ~80 hub TIA channels |
| Low-leakage electrode mux | 12 | $0.60–1.20 | $7–14 | Record/stim select |
| High-side load switch (safety gate) | 12 | $0.30 | $3.60 | Gated by `SAFE_EN_n` |
| Socket spring-contact array | 80 | $0.35–0.70 | $28–56 | 18 contacts each |
| Module-side UID EEPROM (T1-A/T1-B) | ~71 | $0.12 | $8.50 | T1-C reuses its ATtiny402 |
| Cluster tail flex + PAN | 1 set | $8–14 | $8–14 | Laminated into L1 |
| Blind-mate boss contact set | 1 | $3–5 | $3–5 | Shared aperture, segregated groups |
| **Estimated total** | | | **$125–216** | |
| *Retired architecture removed* | | | *−($18–30)* | 5 × FPC tails + 5 × Hirose FH34S + EEG harness |

Two honest observations about that number:

1. **It is a real increase**, on the order of $100–190 at the L1 level, and it is the price of
   16× the sockets — roughly **$1.30–2.40 per socket**, against ~$4–6 per socket for the retired
   five large modules. Per socket the architecture is cheaper; per headset it is not.
2. **Where it lands matters.** It is concentrated on the cluster carriers, which are ordinary
   rigid-flex assemblies from a mature supply base, rather than on the Hub PCB, where SMART-1's
   residual implied ~80 analog front ends on a single board.

### 10.2 Manufacturing impact

| Change | Consequence |
|---|---|
| **L1 becomes an electro-mechanical assembly**, not a molded part | New supplier category — rigid-flex lamination into a molded carrier. Add to `NP-PROC-SUP-001` as a new CAT (alongside CAT-A moulding / CAT-B CFRP / CAT-C PDMS). **OI-SHELL2-04** |
| **Module tails eliminated** | The T1-A/B/C tile is a sealed body with a back-face pad array. No FPC tail artwork, no ZIF, no stiffener, no per-module bend qualification |
| **80 × IPX4 socket seams** | NP-HEX-ZM-001 §6 already flags the per-tile seam-length budget; a compression contact array under a co-molded LSR land (NP-HELMET-GEOM-001 §3.1) is compatible, but the contact array is a new ingress path to qualify. **SH2-DRC-11** |
| **Cluster-level rework** | A failed socket is a carrier swap (7 sockets), not a bowl scrap. Improves yield economics and field service |
| **No per-zone molded FPC channels** | The retired REQ-ST-01..07 shell channel features are deleted from shell tooling. Net simplification of the shell tool; the complexity moves into the L1 lamination |
| **Contact plating** | Compression contacts need hard gold on both halves; retired precedent is ≥0.5 µm cobalt-alloyed (NP-HW-FPC-001 §5.1). Fretting is the wear mode. **SH2-DRC-09** |

---

## 11. Design review checklist

To be completed in CAD/schematic review before this document can move DRAFT → BASELINED (which
additionally requires REG-1 and ACT-1 to close). Each item requires a named reviewer and a
pass/fail with supporting evidence.

| # | Check item | Method | Pass criterion | Owner |
|---|---|---|---|---|
| SH2-DRC-01 | Cluster partition of the lattice covers every socket exactly once, ≤8 per cluster | Generated from `sync-socket-map.ts` | 100 % coverage, no socket in two clusters | FW/ME |
| SH2-DRC-02 | Cluster count ≤16 (8 branches × 2) at the final lattice | Count | ≤16 | ME |
| SH2-DRC-03 | Every cluster carrier ≤50 mm from its furthest socket (N3 run length) | CAD measurement | ≤50 mm | ME |
| SH2-DRC-04 | Cluster tail static bend radius at every formed bend | CAD measurement | ≥12.5 mm (REQ-BR2-01) | ME |
| SH2-DRC-05 | No bend within 5 mm of any rigid-flex transition, stiffener, boss or carrier edge | CAD | REQ-BR2-03 | ME |
| SH2-DRC-06 | No formed bend under a clamp plate footprint or a socket | CAD | REQ-BR2-05 | ME |
| SH2-DRC-07 | Zero dynamic-flex paths in the module interconnect | Design review | Set is empty (REQ-BR2-02) | ME |
| SH2-DRC-08 | Socket contact force, wipe distance and mating cycle rating | Bench | ≥1,000 cycles, wipe ≥0.3 mm | ME/EE |
| SH2-DRC-09 | Contact plating hard gold ≥0.5 µm both halves; fretting resistance | Coupon | Contact R drift <20 % over cycle life | EE |
| SH2-DRC-10 | Cluster clamp plate load with final contact count vs one-handed input force | Bench + HFE | RISK-22 intent met with §5.4a actuator | ME/HFE |
| SH2-DRC-11 | IPX4 maintained at the socket contact array after 10 swap cycles | Test | IPX4 (RISK-16 precedent) | ME |
| SH2-DRC-12 | `SAFE_EN_n` gates the cluster LED rail and the tES record/stim selector | Schematic + bench | No emission with `SAFE_EN_n` low, any bus state | EE/Safety |
| SH2-DRC-13 | `SAFE_EN_n` defaults LOW at Safety-MCU power-on reset | BSP review | LOW before any modality task starts | FW |
| SH2-DRC-14 | Watchdog expiry removes all cluster rails <50 ms | Bench | <50 ms (CLAUDE.md §4.2) | EE/Safety |
| SH2-DRC-15 | `VLED+`/`PGND` broadside overlap and loop area per cluster feed | CAD/stackup | ≤25 mm², dielectric ≤0.2 mm (REQ-EMI-06) | EE |
| SH2-DRC-16 | **EEG artifact with all LEDs at full PWM load** (retained from retired DRC-18c) | Oscilloscope, prototype | **<5 µVpp**, all frequencies | EE |
| SH2-DRC-17 | Fluxgate self-field from the N1 bus, per module configuration | Bench, magnetometer | Within cancellation-loop budget (**OI-SHELL2-07**) | EE |
| SH2-DRC-18 | No LED/tES return path through shell, shield, mu-metal or DRL structure | Schematic + continuity | REQ-EMI-08 — zero | EE |
| SH2-DRC-19 | N1 topology is a tree; no closed ring on L1 | Netlist inspection | REQ-EMI-09 | EE |
| SH2-DRC-20 | Boss contact groups segregated with independent returns | Schematic + boss layout | REQ-EMI-05 / §4.3 | EE/ME |
| SH2-DRC-21 | Sense-quiet windows honoured; no bus edge inside an acquisition aperture | Logic capture | REQ-EMI-03 | FW |
| SH2-DRC-22 | Spread-spectrum/dither disabled on all LED drivers | FW config review | REQ-EMI-04 | FW |
| SH2-DRC-23 | Feed-forward self-field model binds to measured PD/dose telemetry | Code review | REQ-EMI-05 | FW |
| SH2-DRC-24 | Cancellation recalibration fires on `np_module_map` rebuild | Bench | REQ-EMI-11 | FW |
| SH2-DRC-25 | No conductive addition to L1 beyond the qualified budget | Stackup review | REQ-EMI-10 | EE/ME |
| SH2-DRC-26 | Vault PBM ceiling enforced twice (firmware budget + PAN feed rating) | Bench | ≤ CLAUDE.md §4.5 envelope | EE/FW |
| SH2-DRC-27 | Electrode mux leakage and Ron mismatch impact on EEG CMRR | Bench | Leakage <1 nA; CMRR within ADS1299 spec | EE |
| SH2-DRC-28 | Broadcast-write and addressed-read modes verified with a full cluster set | Bench | Whole-vault stop in one transaction | FW |

*(28 items; the retired document carried 23.)*

---

## 12. Open items

| ID | Description | Owner | Blocking |
|---|---|---|---|
| OI-SHELL2-01 | Fix the N1 bus rail voltage (12 V assumed). Sets contact current, IR drop and tile driver headroom | EE Lead | Cluster carrier schematic; Hub PCB Rev C |
| OI-SHELL2-02 | **Boss contact-group segregation and independent returns** (§4.3). Cheap now, expensive after MECH-1 tools the posterior boss | EE + ME Lead | **MECH-1 — time-boxed** |
| OI-SHELL2-03 | Close the contact-count ↔ clamp-input-force coupling (§5.1) jointly with MECH-2 and the §5.4a HFE formative | ME + HFE | MECH-2; RISK-22 accessibility |
| OI-SHELL2-04 | Add a rigid-flex-into-molded-carrier supplier category to NP-PROC-SUP-001 | Procurement | L1 sourcing |
| OI-SHELL2-05 | Confirm §10.1 BOM estimates against quotations | EE Lead | Costed BOM |
| OI-SHELL2-06 | Bus dwell-time budget: worst-case addressed-read sweep of all sockets vs closed-loop adaptation cadence | FW | Session timing |
| OI-SHELL2-07 | Set the fluxgate self-field budget the N1 bus must stay within (SH2-DRC-17 pass criterion) | EE Lead | EMF-1/EMF-2 sign-off |
| OI-SHELL2-08 | Reflect cluster-level repair in the service-network tiering | Service | `docs/reference/service-network.md` |
| OI-SHELL2-09 | **Controlled-document updates this architecture implies but does not make:** NP-HEX-ZM-001 §5.3.1/§5.4a (bus on L1 vs reasons 3–4), NP-HELMET-GEOM-001 §2 (L1 module depth assumes a "20-pin FPC"; tiles now have no tail) and §3.2, and a new FMEA entry for the §4.3 shared-return failure alongside FMEA-G07-01 | Quality | DHF consistency |
| OI-SHELL2-10 | Decide whether the ADS1299 bank sits at the PAN (assumed) or the Hub PCB; moves an SPI interface across the boss | EE Lead | Hub PCB Rev C |

---

## 13. Cross-references

- **Lattice, clusters, two-bowl shell, SMART-1:** `docs/np_hex_zm_001.md` (NP-HEX-ZM-001 Rev A)
  §3.4, §4a, §5.1–5.4a, §7
- **L0–L3 layer topology, L1 material constraints:** `docs/np_helmet_geom_001.md`
  (NP-HELMET-GEOM-001 Rev A) §0, §2, §3.2
- **Superseded predecessor (bend-radius basis §3; EEG separation §2.4; 23-item DRC §5):**
  `docs/neurone_shell_fpc_routing_review.docx` (NP-DRV-SHELL-001 Rev B)
- **Superseded hub design (TIA topology and saturation analysis, still valid per socket):**
  `docs/np_hw_hub_001.md` (NP-HW-HUB-001 Rev B) §2, §3.1–3.3
- **Superseded module FPC (dual-PD architecture, InGaAs PD choice, on-module driver):**
  `docs/np_hw_fpc_001.md` (NP-HW-FPC-001 Rev E) §5, §6
- **Optical resolution floor (why sub-module addressing buys little at depth):**
  `docs/np_opt_psf_001.md` (NP-OPT-PSF-001 Rev A)
- **Firmware addressing and protocol wire format:**
  `firmware/hub_control/include/np_module_map.h`, `src/np_protocol.c`
- **CLAUDE.md:** §3 (modality stack), §4.1 (processor stack), §4.2 (safety architecture),
  §4.3 (EMF shielding), §4.5 (power), §5.1 (SHDR classification of module UID)

---

## 14. Revision history

| Rev | Date | Author | Description |
|---|---|---|---|
| A | 2026-07-29 | NeurOne Mechanical + Hardware Engineering | Initial release. Replaces the retired 5-slot FPC routing architecture of NP-DRV-SHELL-001 Rev B with a cluster-carrier interconnect for the ~80-socket hex lattice. Five-network split (N1–N5); cluster as electrical aggregation boundary; two-level I2C tree reaching exactly `NP_HEXMAP_MAX_SOCKETS = 128`; TIA/AFE relocated from Hub PCB to cluster carriers; per-cluster hardware safety enable; Hub PCB Rev C interface contract (12 connectors × 12 pins, 16 positions provisioned); zero dynamic-flex paths; ≥15 mm EEG separation shown unachievable and replaced by four mechanisms retaining the <5 µVpp threshold; 28-item design review checklist; 10 open items. Status DRAFT pending REG-1/ACT-1. |
