# Shell Socket Interconnect Architecture

**Project:** NeurOne
**Document:** NP-DRV-SHELL-002
**Revision:** B
**Date:** 2026-08-11
**Status:** DRAFT
**Effective Date:** —
**Author:** NeurOne Mechanical + Hardware Engineering
**Approved By:** — (unapproved; see §1.2)
**References:** NP-HEX-ZM-001 Rev A (§3.4 lattice, §4a taxonomy/SMART-1, §5 two-bowl shell + cluster clamps, §7 open items); NP-HELMET-GEOM-001 Rev A (§0 L0–L3 layer topology, §2 radial stack-up, §3.2 L1 material constraints); **NP-HW-HUB-001 Rev C** (§3.1 cluster-controller tier, §5 three-tier I2C, §7.4 interface contract, §7.5 synthesis, §8.1 controller BOM, §8.3 STM32G071 selection, OI-HUB-C07/C15/C17/C18/C19); **NP-HW-HEXTILE-001 Rev C** (D-3 on-module driver, D-4 on-module TIA — **not adopted**, D-5 **19-position** socket, D-6 24 V rail, D-7 32-segment I2C, §7.1–7.2 socket interface, §8.2.1 cluster derivation, §8.2.2 connector options, §8.4.1 enable class split, OI-HEXTILE-13/14); **NP-THERM-BEZEL-001 Rev A** (§4.5 bezel height); **NP-THERM-CFD-C2-001 Rev A** (§7 inter-bowl stagnant-air resistance); NP-HW-FPC-001 Rev E (superseded — dual-PD, TIA saturation analysis); NP-DRV-SHELL-001 Rev B (superseded — bend-radius basis, EEG separation); NP-OPT-PSF-001 Rev A; CLAUDE.md §3, §4.1–4.5, §5.1; IPC-2223D; IEC 60068-2-21
**Related Issues:** —
**Gate:** NP-COORD-001 G2 (pre-tooling) — this document is an input to shell tooling release
**IEC 62304 Class:** N/A (hardware architecture; safety-MCU firmware implications flagged, not specified here)
**Supersedes:** NP-DRV-SHELL-001 Rev B (architecture replaced, not revised — see §1.2)
**Parent Document:** None

---

> **Rev B (2026-08-11) — five Rev A positions overtaken by peer decisions. Nothing is silently
> overwritten; each replaced position is stated, sourced, and given its surviving reason.**
>
> Rev A was written on 2026-07-29 and has had one commit since. In the same window
> `NP-HW-HUB-001` reached Rev C, `NP-HW-HEXTILE-001` reached Rev B, and four principal decisions
> landed. Five Rev A positions do not survive that traffic, and other documents had begun to
> inherit them.
>
> | # | Rev A position | Rev B position | Basis | Where |
> |---|---|---|---|---|
> | 1 | Cluster carrier is **passive** — I2C segment switch, TIA/mux/ADC, electrode mux, power gate, no MCU | Cluster **controller** — adds an **STM32G071 (UFQFPN32)**, PCA9548A, 16:1 PD current mux, one shared switched-gain TIA (DG2788A), 8:1 NTC mux + ADC, cluster power gate | Principal direction 2026-08-11: NP-HW-HUB-001 Rev C §3.1/§8.3 prevails. **HUB-001 §3.2's own justification is NOT carried forward** — see §3.2a | §3.2, §3.3, §5, §7, §10.1 |
> | 2 | N1 bus rail **12 V** (est., OI-SHELL2-01) | **24 V** — vault bus current ~2.9 A → **~1.46 A**, I²R quartered, N1 conductor and gate part class re-rated | **OI-HUB-C17b ADOPTED** HEXTILE **D-6**; propagation requested by **OI-HUB-C18**. **OI-SHELL2-01 CLOSES** | §5.1, §5.4, §7.1, §9.3, §10.1, §12 |
> | 3 | **12 clusters** at the 80-socket lattice; **16** connector positions provisioned | **18 clusters**, provably minimal; **20** connector positions provisioned; two-level `8 × ≤2 = 16` I2C tree replaced by D-7's **32-segment** tree | **CLUSTER-1 + SYM-1 + CONTIG-1** (principal, 2026-08-04); derivation NP-HW-HEXTILE-001 §8.2.1; recommendation §8.2.2. **This is OI-HEXTILE-14** | §3.1, §3.4, §5.3, §6, §7.1, §10.1, §11, §12 |
> | 4 | Bezel height not stated; peers split 1.0 mm vs 2.5 mm | **1.0 mm** wherever bezel height binds | Principal direction 2026-08-11. NP-THERM-BEZEL-001 §4.5 is the only **calculated** value; the 2.5 mm figure appears in NP-HEX-ZM-001 §3.1 only as a table column header and was never derived | §4.1 note |
> | 5 | Passive carrier raised no inter-bowl thermal load | Eighteen **active** controller boards dissipate into the inter-bowl gap, which NP-THERM-CFD-C2-001 §7 characterises as **stagnant air (0.231 m²K/W — the dominant term in the outward path)** | New consequence of change 1; nobody owns it | **OI-SHELL2-11** (new), §10.3 |
>
> **The coupling change 1 creates, stated rather than buried.** Rev A's passive carrier — and
> NP-HW-HUB-001 §7.5.3, which inherited it verbatim ("the carrier stays passive-plus-switches with
> no MCU, which is SHELL-002's model") — were both **conditional on NP-HW-HEXTILE-001 D-4**, which
> moves the TIA and ADC on-module so the carrier loses its analog front end entirely. Keeping the
> front end on the carrier is the opposite of D-4. **This decision therefore effectively resolves
> OI-HUB-C17c against D-4**, and §3.3a records what that costs and what it buys. It is the
> **conservative** side of C17c's own stated worry — ADC drift 25 → 62 °C against the ±15 % dose
> claim (FAI-SM-06), which a cooler carrier-mounted ADC never faces — but **C17c's other half is
> untouched and remains open**: whether continuous on-tile dissipation fits inside the 42 °C face /
> 62 °C junction envelope is a question about the module heat-sink path, and nothing here answers it.
>
> **A sixth change, added during review: the socket contact count is now DECIDED at 19.** It is not
> settled by the MCU decision — that only changes its terms, because NP-HW-HUB-001 §7.5.2's 14–15
> synthesis figures assumed D-4 deletes network N3, and **N3 survives**, so those figures no longer
> apply. §5.1 works the question through from the surviving networks rather than restating three
> stale numbers: `SEAT#` adopted from D-5, `SYNC` and `DGND` retained with reasons, 2 reserved
> dropped, N3 retained → 17; then **`VLED+`/`PGND` = 3+3 by principal decision, 2026-08-11**, under
> the rule *loss of any one contact must still leave ≥2× derating* → **19**. Rev A's 18, D-5's 16 and
> HUB-001's 14–15 are all superseded, and **`NP-HW-HEXTILE-001` §7.1–7.2 must be co-revised**
> (OI-SHELL2-09). Two consequences: **REQ-SKT-01** makes the two-staggered-row pad array binding
> rather than advisory, and the clamp-plate load — despite two more contacts — lands *below* Rev A's,
> because the 18-cluster partition caps a plate at 6 tiles rather than 7 (§5.1.6).

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

**Cost consequence:** analog front ends scale with **cluster count (18)**, not socket count (~80)
— the same order of magnitude as the five the architecture replaced. *(Rev A said ~12; §3.1
corrects the count. The argument is unaffected — 18 is still an order of magnitude below 80.)*

> **Terminology, Rev B.** Rev A called this board the **cluster carrier**. It is now a **cluster
> controller** (§3.2) because it hosts an MCU. Both names appear in this document and in
> `NP-HW-HUB-001`; they denote the same physical board, and "carrier" is retained where the sense is
> mechanical (what carries the sockets and laminates into L1) rather than electrical.

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
     │ CLUSTER 1 │ │CLUSTER n│  **×18**        │  (posterior-centre, ONE aperture,
     │ MCU·mux·  │ │         │                 │   SEGREGATED returns — §4.3)
     │ AFE·ADC·sw│ │         │                 │
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
| **N5** | Safety enable | `SAFE_EN[n]`, one per cluster | Star, direct from the Safety MCU | Cluster PDN hard gate (§6) |

### 3.1 Cluster definition (topological, not lattice-bound)

A **cluster** is **≤8 sockets** that share one carrier board, one clamp actuator, one I2C segment
and one safety-enable domain. The natural super-cell of a hex lattice is the 7-hex "flower"
(1 centre + 6 neighbours), which NP-HEX-ZM-001 §5.4a already names; ≤8 is stated as the bound so
the boundary clusters that cannot form a complete flower are covered by the same rule.

**⚠ Rev A said 12. The count is 18.** Rev A computed `ceil(80/7) = 12` from CLUSTER-1's flower alone.
That arithmetic is right and the answer is wrong, because two further standing principal decisions
bind the *shape* of the partition, not only its cell size:

| Decision | Date | Effect on the count |
|---|---|---|
| **CLUSTER-1** | 2026-07-30 | 7-hex flower, partial flower at the lattice boundary |
| **SYM-1** | 2026-08-04 | The partition must be **mirror-symmetric about the sagittal midline** |
| **CONTIG-1** | 2026-08-04 | A cluster's petals must form a **contiguous arc** — no pendant petal on a cantilever arm |

SYM-1 is what costs the clusters. A cluster containing a midline socket must equal its own mirror
image, so its centre must sit *on* the midline; only the six odd-width rows carry one, so **six
midline clusters are forced**, absorbing 30 sockets. The residual 50 sockets form two mirror-image
lateral bands of 25, only 2–3 sockets wide, where most flowers cannot fill — exhaustive
branch-and-bound gives **6 per band**, not the naïve `ceil(25/7) = 4`.

> **6 + 2 × 6 = 18 clusters at the v1 80-socket lattice, and this is provably minimal** — not a
> greedy result. Cluster sizes are 3–6 tiles. Derivation: `NP-HW-HEXTILE-001` §8.2.1. Diagram:
> `docs/diagrams/np_hextile_cluster_map.svg`. Without SYM-1 the minimum is indeed 12; **12 is the
> count of a partition that no longer satisfies the standing decisions.**

**Nothing downstream of this section depends on 80 or on 18.** The tail pinout (§5.2), the hub
interface (§7) and the safety granularity (§6) are all expressed per cluster. A REG-1 re-cut
changes how many cluster controllers are populated — not the architecture, not the Hub PCB
interface, not the firmware addressing. **But the count is no longer free of the connector budget**:
18 does not fit the 16 positions Rev A provisioned, and §7.1 resolves that rather than renumbering
around it.

### 3.2 Cluster controller board contents

**⚠ Rev A specified this board as passive — explicitly "no MCU". It is now an active controller.**
Principal direction, 2026-08-11: where Rev A and `NP-HW-HUB-001` Rev C disagree on the cluster tier,
HUB-001's analysis prevails. Rev C §3.1 and §8.3 specify a distributed **cluster-controller** tier
with one **STM32G071 in UFQFPN32** per board.

| Function | Part class | Qty per controller | Note |
|---|---|---|---|
| **Cluster MCU** | **STM32G071, UFQFPN32** | **1** | **NEW in Rev B.** I2C slave to the hub, I2C master to the PCA9548A, 12-bit ADC, timers. Rationale §3.2a; part choice HUB-001 §8.3 |
| Socket contacts | spring-contact array | ≤8 × **19** (§5.1) | Seated by the §5.4a clamp plunger. **19, not 18** — two staggered rows, REQ-SKT-01 |
| I2C segment switch | **PCA9548A** 8-channel | 1 | Exact fit at capacity 8. Per-socket isolation — resolves the shared-0x30 collision |
| **PD current mux** | **16:1 (2 × TMUX1308 class)** | **1 set** | PD1 + PD2 of ≤8 sockets into one TIA. Rev A said "mux" without sizing it |
| PD front end | **one shared switched-gain TIA (DG2788A + zero-drift op-amp)** | 1 | Per-*cluster*, per-sample gain — not per socket (§3.3) |
| **NTC mux + ADC** | **8:1 mux → cluster-MCU ADC** | **1** | Rev A folded this into "TIA + mux + ADC"; it is a separate chain |
| Electrode mux | low-leakage analog mux | 1 | Record/stim path select (§3.5) |
| Cluster power gate | high-side load switch, **24 V-rated** | 1 | Gated by `SAFE_EN[n]` (§6). Part class changed by the rail decision (§5.4) |
| Local bulk decoupling | ceramic + polymer bulk | — | Sized for edges/carrier only (§9.2) |

### 3.2a Why the tier needs local intelligence — and why HUB-001's stated reason is not the reason

`NP-HW-HUB-001` §3.2 argues the tier needs an MCU because **LED drive must distribute**:

> *"It fails on LED drive. T1-A base tiles (the majority population) carry no on-module driver;
> their LED current comes from the hub. Eighty sockets × two channels of 120–180 mA drive cannot be
> star-routed any more than the analog can. Once the LED drive stage has to distribute — and it
> does, unavoidably — the distributed board needs PWM generation, per-socket current setpoints, and
> per-socket NTC thermal response. That is a microcontroller's job."*

**That premise is false, and it was falsified after Rev C was written.** `NP-HW-HEXTILE-001`
**D-3** (2026-08-04) fits an on-module constant-current driver to **every** tile type, not only
T1-C, and marks the decision **irreversible** ("sets the socket pinout and the Hub PCB Rev C
architecture"). No LED drive current crosses the socket at all — the socket carries a DC bus pair
and the tile's own ATtiny generates its PWM. HUB-001's own **OI-HUB-C15** already instructs Rev C to
*"drop the cluster-MCU LED-drive rationale in §3.2"*. **This document does not carry it forward, and
readers should not reconstruct it from Rev C's §3.2 as written.**

**The justification that survives is the analog scan, and it is sufficient on its own:**

> The controller retains a switched-gain TIA, a 16:1 PD current mux, an 8:1 NTC mux and an ADC.
> **Sequencing and scanning that set is an MCU's job.** A 16:1 current mux into one shared TIA
> requires per-sample channel selection, per-sample gain selection matched to the fitted PD, a
> settling delay, a conversion, and per-socket accumulation into a dose figure — at a 10 Hz dose
> tick across ≤8 sockets, phase-aligned to the emitters' ON-window (HUB-REQ-C01) and scheduled
> around the ADS1299 bus-quiet window (HUB-REQ-C03). The alternative — driving those select lines
> from the hub over I2C and returning raw samples — reinstates exactly the per-socket hub-side
> signal count this architecture exists to delete (§2.2), and puts a settling-critical analog
> sequence at the far end of a 400 kHz bus across the parting plane.

Two corollaries worth stating plainly:

- **The MCU is a consequence of keeping the AFE, not an independent addition.** If `D-4` had won and
  the AFE moved on-module, the surviving board would be a PCA9548A, an electrode mux and a power
  gate — and Rev A's "no MCU" model would have been correct. The two decisions are one decision;
  §3.3a treats them as one.
- **The tier stays IEC 62304 Class B.** Adding an MCU does not move the safety boundary: `SAFE_EN[n]`
  remains Safety-MCU-sourced and gates the high-side switch in hardware (§6), so no cluster-MCU
  firmware state can produce emission from a de-energized cluster. This is HUB-REQ-C02 and
  HUB-001 §7.3, and it is what keeps the new firmware unit out of Class C.

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

### 3.3a What keeping the front end on the controller costs — OI-HUB-C17c resolved against D-4

`NP-HW-HEXTILE-001` **D-4** takes §3.3's own argument one step further: move the TIA *and* the ADC
onto the tile, so the photocurrent node shrinks from ≤50 mm to ~5 mm and no spring contact sits in
the photocurrent path at all. `NP-HW-HUB-001` §7.5.3 accepted that and, as a consequence, described
this document's carrier as staying *"passive-plus-switches with no MCU"*.

**Rev A's passive carrier and HUB-001 §7.5.3 were therefore both conditional on D-4.** Keeping the
front end here is the opposite of D-4. **Recording the consequence explicitly: this decision
effectively resolves OI-HUB-C17c against D-4.** It should not be discovered later as an implication.

| | D-4 (TIA + ADC on-module) | **This document (AFE on the controller)** |
|---|---|---|
| Photocurrent node | ~5 mm | ≤50 mm (REQ-EMI-01, SH2-DRC-03) |
| Spring contacts in the photocurrent path | none | `PD1_K`, `PD2_K`, `AGND` — contact drift and fretting become dose-error terms (SH2-DRC-09) |
| Network **N3** | deleted | **survives** — and with it three socket contacts (§5.1) |
| ADC location | tile, at 25–62 °C junction | controller, in the inter-bowl gap |
| **ADC drift vs the ±15 % dose claim (FAI-SM-06)** | a **tile-level** spec across 25 → 62 °C | **never faced** — the conservative side |
| N2 bus traffic per 100 ms dose tick | ~18 ms / 18 % duty (80 per-tile reads, HUB-001 §7.5.4) | ~1/8 of that — the controller aggregates ≤8 tiles into one frame |
| Continuous on-tile dissipation inside the 42 °C face envelope | added (U1 + U2, continuous where emitters are duty-cycled) | **avoided** |

**Why this is the conservative call.** OI-HUB-C17c gates on thermal, in two parts. This decision
answers one part in the safe direction — a controller-mounted ADC in the cooler, fan-served
inter-bowl gap never faces the 25 → 62 °C drift question that HUB-001 §7.5.0a calls *"the binding
one"* against a **stated competitive claim**. Under `feedback_conservative_default`, an
architecture that does not need a drift budget to defend a dose claim beats one that does.

**What is *not* resolved, and must not be read as resolved.** C17c's other half — whether
continuous on-tile dissipation fits inside the 42 °C face / 62 °C junction envelope — is a question
about the **module heat-sink path**, and this decision does not touch it. It stays open with
`NP-THERM-CFD-R1-001`, the SR-FAN-01…06 forced-convection interlock, and OI-HEXTILE-06 (PD
population), exactly as HUB-001 §7.5.0a scopes it. What *has* changed is that the thermal load moved
sides: see **OI-SHELL2-11**.

**What is given up, honestly.** D-4's signal-integrity argument was the better one on its merits and
it is not refuted here — it is outweighed. Three contacts, three spring interfaces in a
high-impedance path, and a per-socket gain-coordination problem across mixed silicon/InGaAs tiles
(HUB-001 §7.5.0b) all survive as costs of this choice, and §6.3's per-sample switched gain is what
pays for the last of them.

### 3.4 Control network — the tree, and why the Rev A tree cannot reach 18

All modules of a given type present the same I2C address (the retired smart module was factory-burned
to 0x30, and a base-tile UID EEPROM likewise has a fixed address), so isolation, not addressing, is
the constraint. The tree is:

**⚠ Rev A's tree tops out at 16 clusters and there are 18.** Rev A specified:

```
Hub PCB host I2C
   └── hub-level 8-channel switch ────────── 8 branches
          └── ≤2 cluster carriers per branch  (distinct switch addresses, e.g. 0x70 / 0x71)
                 └── cluster 8-channel switch ── ≤8 sockets
```

`8 branches × 2 clusters × 8 sockets = 128` — which is exactly `NP_HEXMAP_MAX_SOCKETS`, and Rev A
made much of that agreement. **The socket ceiling is still 128 and is unaffected. The middle term
is what breaks:** `8 × 2 = 16 < 18`. Reaching 18 in Rev A's tree needs either a third branch tier or
3-deep branches, and Rev A provided neither. This is the C2 half of **OI-HEXTILE-14**.

**Resolution: adopt `NP-HW-HEXTILE-001` D-7's 32-segment tree.** It is `NP-HW-HEXTILE-001` §8.2.2
option 3, which that document recommends and which costs nothing new — it is already the standing
requirement in OI-HEXTILE-10.

```
i.MX RT1062 — LPI2C1 … LPI2C4                            (4 host buses)
   └── one PCA9548A-class 8-channel switch per bus        (4 × 8 = 32 segments)
          └── one cluster controller per segment          (18 of 32 used, 56 %)
                 └── controller's own PCA9548A ─────────── ≤8 sockets
```

**4 × 8 = 32 segments; 18 used; 14 spare** — headroom for a REG-1 re-cut with no re-layout. Three
things follow, and the third is the one worth noticing:

1. **The tier count does not grow.** This is still *one* tier of hub-side muxing, not the cascade
   NP-HEX-ZM-001 §4a anticipated, because D-7 removes the **address collision** rather than muxing
   around it: modules take UID-derived dynamic addresses (SMBus-ARP style, keyed on the UID
   `np_module_map` already depends on), so muxing is needed only for bus **capacitance**.
2. **Capacitance is comfortably inside budget either way.** At 400 kHz the 400 pF ceiling permits
   ~10–19 modules per segment; realised cluster sizes under CLUSTER-1 + SYM-1 are **3–6 tiles**,
   ≥1.7× margin even against a full 7-tile flower.
3. **The `= 128` coincidence Rev A relied on is gone, and it was never load-bearing.** Under D-7 the
   physical tree reaches `32 × 8 = 256`, well past `NP_HEXMAP_MAX_SOCKETS = 128`. The firmware
   ceiling stands on its own justification (NP-HEX-ZM-001 §3.4: the full 7-bit major domain) and
   does not need the bus tree to reproduce it. Rev A presented the agreement as corroboration; it
   was a coincidence of `8 × 2 × 8`, and losing it costs nothing.

**Pull-ups:** one 4.7 kΩ pair per segment, not per socket — **18 segments, 36 resistors**, against
160 for a per-socket scheme (`NP-HW-HEXTILE-001` §8.2). Whether one pair per controller suffices, or
per-PCA9548A-channel pull-ups are needed for socket FPC stub capacitance, is **OI-HUB-C04**.

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
| Gap-facing (outboard) | Cluster **controller** components, incl. the STM32G071; clamp plates (mechanically independent, §8.1) | Serviceable side; components out of the scalp thermal path — but see **OI-SHELL2-11**, because "out of the scalp path" is not the same as "cooled" |

> **Bezel height, where it binds: 1.0 mm** (principal direction, 2026-08-11). Nothing in this
> document's routing or contact geometry is a function of bezel height, so no figure here changes —
> but the value is recorded because peer documents split on it and this one is read alongside them.
> **NP-THERM-BEZEL-001 §4.5 is the only *calculated* figure** ("the knee is around 1.0 mm";
> module-face ceiling ~45.5 °C). The competing **2.5 mm was never derived** — it appears in
> NP-HEX-ZM-001 §3.1 only as a table column header, and `NP-HW-HEXTILE-001` §3 adopted it as the
> conservative-for-irradiance choice while flagging the conflict as **OI-HEXTILE-01**. Resolving to
> 1.0 mm raises the active field area 14.5 % and improves every irradiance figure in that document;
> it does not invalidate any conclusion there, by its own §3 note.

The L1 laminate wall itself separates N1 from N4. This *reproduces the retired design's mechanism —
"opposite sides of a wall" — but at ~2–3 mm instead of the 18–22 mm the shell wall gave.* §9.1 states
plainly why that is not sufficient on its own and what has to make up the difference.

### 4.2 Cluster tails to the posterior aggregation node

All **18** cluster tails converge on a **posterior aggregation node (PAN)** on L1 at the occiput
centreline, co-sited with the standalone blind-mate boss that NP-HEX-ZM-001 §5.3c already puts
there for the fluxgate/coil harness. The PAN hosts the inner-bowl end of the cluster bus, the
ADS1299 bank, the tES driver interface, and the PDN feed point. *(Rev A said "~12 tails" and put
the hub-level I2C branch switch here; under §3.4's D-7 tree the hub-side segmentation sits on the
Hub PCB — four PCA9548A on LPI2C1–4 — and the PAN carries the inner-bowl end of the differential
link, per `NP-HW-HUB-001` §5.1.)*

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

### 5.1 Socket contact budget — reconciled against `NP-HW-HEXTILE-001` D-5

**Status: NOT CLOSED, but the terms have changed and two of the three candidate numbers are dead.**
This section replaces Rev A's 18-contact table. It is the one place where this document and
`NP-HW-HEXTILE-001` §7.1–7.2 specify the *same physical interface* and disagree, so it is worked
through rather than asserted.

#### 5.1.1 First: the 14–15 figures no longer apply

`NP-HW-HUB-001` §7.5.2 offers **15** (`AGND` kept for N4) and **14** (`AGND` also dropped), and
those two numbers have propagated into OI-HUB-C17's summary as *"18 vs 14–15"*. **They are
conditional on D-4 and only on D-4.** §7.5.2's derivation is one sentence long — *"Deleting N3
removes `PD1_K`, `PD2_K`, `NTC` outright"* — and §7.5.1 states the premise plainly: *"N3 is deleted,
not relocated."*

§3.3a keeps the analog front end on the cluster controller. **N3 is therefore not deleted.**
`PD1_K`, `PD2_K` and `NTC` still cross the socket; `AGND` is no longer ambiguous, because it is
N3's sense return again and not merely a possible N4 reference. **14 and 15 are arithmetic on a
premise that no longer holds and must not be quoted forward.** The live pair is **18 vs 16**, and
neither is right.

#### 5.1.2 The two pinouts, aligned

| Signal | Net | Rev A (this doc) | HEXTILE **D-5** §7.2 | Agreed? |
|---|---|---|---|---|
| `VLED+` | N1 | **2** | **4** (pins 1–4) | ✗ → **3 ADOPTED** (§5.1.5) |
| `PGND` | N1 | **2** | **4** (pins 5–8) | ✗ → **3 ADOPTED**, paired 1:1 |
| `V3V3` / `VCC_3V3` | N1 | 1 | 1 (pin 9) | ✓ |
| `DGND` | N1 | **1** | **absent** — logic returns on `PGND` | ✗ |
| `SDA` | N2 | 1 | 1 (pin 10) | ✓ |
| `SCL` | N2 | 1 | 1 (pin 11) | ✓ |
| `SYNC` | N2 | **1** | **absent** | ✗ |
| `ALERT#` | N2 | 1 | 1 (pin 12) | ✓ — *HEXTILE Rev B called this `/ALERT`; **`ALERT#` adopted**, §5.1.7* |
| `PD1_K` | N3 | 1 | absent (D-4) | ✓ *now* — N3 survives (§5.1.1) |
| `PD2_K` | N3 | 1 | absent (D-4) | ✓ *now* |
| `NTC` | N3 | 1 | absent (D-4) | ✓ *now* |
| `AGND` | N3 | 1 | 1 (pin 15) | ✓ |
| `ELEC` | N4 | 1 | 1 (pin 13) | ✓ |
| `ELEC_SHLD` | N4 | 1 | 1 (pin 14) | ✓ — *Rev A called this `GUARD`; **`ELEC_SHLD` adopted**, §5.1.7* |
| `SEAT#` | — | **absent** | **1** (pin 16) | ✗ |
| reserved | — | **2** | **0** | ✗ |
| | | **18** | **16** | |

The two documents agree on nine signals and disagree on four. **The gap is not 2 contacts; the two
16s and the 18 are different interfaces that happen to differ by two.**

#### 5.1.3 The four disagreements, decided

**(a) `SEAT#` — HEXTILE is right, and this is the strongest single finding.**
Rev A argued *"presence **is** a successful I2C probe"*. HEXTILE §7.2 answers it directly and the
answer is better: *"a **partially** seated tile can answer I2C on two contacts while the PD, NTC, or
electrode contacts are marginal — the failure mode that produces a plausible-looking but wrong dose
reading."* Under §3.3a that objection is **stronger, not weaker**, because N3 survives: the
photocurrent now crosses three spring contacts that a successful I2C probe says nothing about. A
partially-seated tile answering I2C while `PD1_K` sits at elevated contact resistance produces an
*under-read* photocurrent, which firmware reads as low output and compensates by driving harder.
That is a dose-integrity failure against a stated competitive claim (CLAUDE.md §3 modality 1), and
it is silent. **`SEAT#` is adopted.** It is tied to `PGND` on the module through 1 kΩ and placed at
the mechanical extreme of the pad pattern so it is the **last** contact to mate (HEXTILE §7.3).

**(b) `SYNC` — Rev A is right, and HEXTILE has an unowned gap here.**
D-5 has no phase-reference contact. But **REQ-EMI-03** requires bus traffic and PBM pulse phase to
be *deterministic and phase-locked to the EEG/fluxgate sample frame*, with sense-quiet windows —
and **REQ-EMI-04** prohibits the usual EMC escape (dithered PWM) precisely so the artifact stays a
subtractable known line. A broadcast phase reference over I2C carries arbitration and
segment-switch jitter and cannot serve that. `NP-HW-HUB-001` **OI-HUB-C05** independently records
the same hole from the other end — *"T1-C PWM phase sync routing from the on-module ATtiny402 to the
cluster controller is **not yet defined**"*. **`SYNC` is retained**, and OI-HUB-C05 is its consumer.

**(c) `DGND` — Rev A is right, for a reason Rev A did not give.**
D-5 returns module logic on `PGND`. The obvious objection (ground bounce on the I2C reference) is
weak: at ≤50 mΩ contact resistance and 1.04 A tile current the offset is ~52 mV against a 0.99 V
I2C threshold. **The real reason is REQ-EMI-07**, which forbids the LED return from using *"any
structure other than its paired `PGND` conductor"* — the requirement that makes §9.3's broadside
pair cancel. If logic current also flows in `PGND`, then the current in the return conductor is no
longer the current in the supply conductor, and the broadside pair's cancellation is exact for the
LED term and open-loop for the rest. **`PGND` must be LED return only for REQ-EMI-06/07 to mean
anything.** One contact is a cheap price for keeping a requirement enforceable.

**(d) reserved ×2 — Rev A is wrong. Dropped.**
Rev A held two contacts as *"OTA-extensibility margin"*. Two arguments against, either sufficient.
First, the extensibility Rev A named — the `§4a` "grow-to-4" tile type — is **already served**:
HEXTILE §1 makes the socket pinout *"the union of every type's needs, including types not specified
in this revision"*, and a fourth wavelength adds no contact, because D-3 puts the driver on the
module and the socket carries only a DC bus pair. Second, contacts are not free in the direction
that matters — every one is spring force on an accessibility budget (§5.1.6). **Speculative margin
loses to a live RISK-22 constraint.** If a future type genuinely needs a conductor, that is a
pinout revision on a pre-tooling interface, which is what §1.3 already scopes.

#### 5.1.4 The reconciled budget — 19

| # | Contact | Qty | Network | Source of the decision |
|---|---|---|---|---|
| 1 | `VLED+` | **3** | N1 | **Principal decision, 2026-08-11** — neither Rev A's 2 nor D-5's 4. Sized on the *degraded* case, §5.1.5 |
| 2 | `PGND` | **3** | N1 | Paired 1:1 with `VLED+` — REQ-EMI-06 needs full broadside overlap. **LED return only** |
| 3 | `V3V3` | 1 | N1 | Agreed. ≤2 mA standby / ≤25 mA active (HEXTILE §8.3 — *not* Rev A's ≤50 mA) |
| 4 | `DGND` | 1 | N1 | Retained — §5.1.3(c) |
| 5 | `SDA` | 1 | N2 | Agreed |
| 6 | `SCL` | 1 | N2 | Agreed |
| 7 | `SYNC` | 1 | N2 | Retained — §5.1.3(b); consumer OI-HUB-C05 |
| 8 | `ALERT#` | 1 | N2 | Open-drain, wire-OR per cluster. **`ALERT#` adopted over HEXTILE's `/ALERT` — §5.1.7** |
| 9 | `PD1_K` | 1 | N3 | **Survives** — §3.3a resolves C17c against D-4 |
| 10 | `PD2_K` | 1 | N3 | Survives |
| 11 | `NTC` | 1 | N3 | Survives. 42 °C/62 °C interlock chain |
| 12 | `AGND` | 1 | N3 | Sense return — **separate from `PGND`**, star-referenced at the controller |
| 13 | `ELEC` | 1 | N4 | Agreed. Dual-rated: EEG record **and** tES drive |
| 14 | `ELEC_SHLD` | 1 | N4 | DRL-driven shield for contact 13. **Renamed from Rev A's `GUARD` — §5.1.7** |
| 15 | `SEAT#` | **1** | — | **Adopted from D-5** — §5.1.3(a). Last to mate |
| | **Total** | **19** | | |

> **19 is the closed count.** Rev A's 18, HEXTILE D-5's 16, and `NP-HW-HUB-001` §7.5.2's 14–15 are
> all superseded. **`NP-HW-HEXTILE-001` §7.1–7.2 has been co-revised — Rev C (2026-08-11) adopts
> this pinout, and the two documents now agree pin for pin.** Its D-5 becomes a 19-position, two-row,
> 3+3 interface; the pitch (2.00 mm), spring-on-socket choice, ≥0.8 µm hard-gold plating,
> ≤50 mΩ / ≥500-cycle specs and §7.3 mating sequence all carried over unchanged, with `DGND` promoted
> into the first mating group and the N3 sense lines added to the third. HEXTILE **HT-DRC-23** exists
> to keep the two in step. **OI-SHELL2-09(i) closes.**

**No `ZONE_ID` contact.** SMART-1 retired the resistor ladder; identity is a UID read over I2C.

**No UID EEPROM either — a Rev A BOM line that D-3 deletes.** Rev A budgeted a 24AA02UID on
T1-A/T1-B because only T1-C had an MCU. `NP-HW-HEXTILE-001` **D-3** fits a driver MCU to **every**
tile type, so every tile can self-report its UID and the EEPROM is redundant. This removes
~$8.50/headset from §10.1 and makes **OI-HEXMAP-02**'s `inventory_fn` a plain I2C read on all types.

#### 5.1.5 `VLED+` redundancy — **3 adopted** (principal decision, 2026-08-11)

> **DECIDED: 3 `VLED+` + 3 `PGND`.** Neither Rev A's 2+2 nor `NP-HW-HEXTILE-001` D-5's 4+4. The
> analysis below was written as an open question with a recommendation; the recommendation was
> accepted and the section is retained in full, because the *reason* 3 was chosen is the thing that
> must survive into the bench plan (**SH2-DRC-10a**) and into any future proposal to trim contacts.

This is where the 18-vs-16 gap actually lived: HEXTILE allots **4** `VLED+` and **4** `PGND`; Rev A
allotted **2** and **2**. The rail decision (§5.4) settles the *nominal* case and does not settle
the *degraded* case — and **the degraded case is what set the number.**

At 24 V, T1-A peak instantaneous = 25.0 W → **1.04 A per tile** (HEXTILE §8.1):

| `VLED+` count | Nominal per contact | **On loss of one contact** | Margin vs ≥1.0 A rating | Contacts spent |
|---|---|---|---|---|
| 2 (Rev A) | 0.52 A | **1.04 A** | **at the rating, zero margin** | 4 (with `PGND`) |
| **3 ★ ADOPTED** | **0.35 A** | **0.52 A** | **~2×** | **6** |
| 4 (D-5) | 0.26 A | 0.35 A | ~3× | 8 |

**Pros of 2+2 (this document):** four fewer contacts, ~1.2–2.0 N less per module and ~7–12 N less
per clamp plate, against a **live** accessibility constraint (RISK-22, Parkinson's H&Y II–III,
one-handed input force) that OI-SHELL2-03 exists to close; smaller pad span, so blind-mate tolerance
over a doubly-curved cluster is easier (§5.1.6); and it is sufficient with ~2× derating in the
nominal case, which is the case the tile is specified in.

**Cons of 2+2:** single-contact loss puts the survivor **exactly at its rating**, and a marginal
contact is not a clean open — fretting raises resistance, resistance raises local I²R heating, and
heating accelerates fretting. That is a runaway, and it is the specific wear mode **SH2-DRC-09**
already exists to bound. With 2 contacts there is no margin to absorb it; with 4 there is 3×.

**Pros of 4+4 (D-5):** tolerates a degraded contact with margin; matches the count HEXTILE derived
its own numbers against, so no second reconciliation is needed; and 4-way paralleling reduces
current *imbalance* sensitivity, which 2-way does not.

**Cons of 4+4:** eight contacts on power alone out of a ~19-contact interface, on a mechanism whose
accessibility requirement is stated and whose electrical requirement is met at 2.

**Decision: 3+3, giving a 19-contact interface.** Three preserves ~2× derating *under
single-contact loss* — the failure case that decides this — at half the contact cost of going to
four. It is not a compromise for its own sake: **the binding quantity is the degraded-case margin,
and 3 is the smallest count that keeps it.** Stated as a rule so it survives future trimming:

> **`VLED+` is sized so that the loss of any one contact still leaves ≥2× derating against the
> contact current rating.** At 24 V and 1.04 A/tile that is 3 contacts. If the rail, the tile peak
> power or the contact rating changes, re-derive the count from this rule — do not carry 3 forward
> as a constant.

**What choosing 3 gives up, and what it does not.** It gives up two contacts of accessibility
budget against RISK-22 (+0.6–1.0 N per module, +3.6–6.0 N per 6-tile plate versus 2+2) and it gives
up 4-way paralleling's better tolerance to *current imbalance* between nominally-identical contacts,
which 3-way improves on 2-way but does not match. It does **not** give up the nominal-case margin —
0.35 A against a ≥1.0 A rating is ~3× before any contact degrades.

**This was previously deliberately open**, on the grounds that it trades a bench-measurable
contact-degradation distribution against a human-factors force budget and could not be closed from
electrical reasoning alone. The principal closed it on the conservative side: **the failure mode is
silent, thermal and self-accelerating, and the accessibility cost is small, measurable and
recoverable through the §5.4a actuator's mechanical advantage.** That asymmetry is the argument.

**SH2-DRC-10a is not cancelled by this decision — it is re-pointed.** It no longer selects between
2, 3 and 4; it **verifies that 3 delivers the ≥2× degraded-case margin the rule above asserts**, on
real contacts with real degradation. If the bench shows the survivor exceeding 0.5 A under a
realistic single-contact fault, the rule — not the number — is what governs the response.

#### 5.1.6 Consequences of the count

> **Contact count is an accessibility variable, not only an electrical one.** At 0.3–0.5 N per
> spring contact:
>
> | Count | Per module | Per clamp plate (**max 6 tiles**, §3.1) | *Rev A's stated 7-tile figure* |
> |---|---|---|---|
> | 16 (D-5) | 4.8–8.0 N | 28.8–48.0 N | *33.6–56.0 N* |
> | 17 (2+2 variant) | 5.1–8.5 N | 30.6–51.0 N | *35.7–59.5 N* |
> | 18 (Rev A) | 5.4–9.0 N | 32.4–54.0 N | *37.8–63.0 N* |
> | **19 ★ ADOPTED (3+3)** | **5.7–9.5 N** | **34.2–57.0 N** | *39.9–66.5 N* |
>
> **Two things this table makes visible, and they pull in opposite directions.**
>
> **(1) The adopted 19 is the highest per-module force of the four candidates** — +0.9–1.5 N over
> D-5's 16. That is the accessibility price of the §5.1.5 redundancy rule and it is not hidden.
>
> **(2) It is still below Rev A's own stated load, because the plate shrank.** Rev A's
> "~38–63 N per 7-tile clamp plate" overstates for a reason nobody had flagged: **under the
> 18-cluster partition no cluster has 7 tiles.** Sizes are 3–6 (`NP-HW-HEXTILE-001` §8.2.1), so the
> worst plate carries 6. The SYM-1 correction that *raised* the cluster count therefore *lowered*
> worst-case plate load ~14 %, and **34.2–57.0 N at 19 contacts on a 6-tile plate is below Rev A's
> 37.8–63.0 N at 18 contacts on a 7-tile plate.** The contact-count increase is more than paid for
> by the partition correction — net, the clamp actuator sees *less* load than Rev A specified, not
> more.
>
> **OI-SHELL2-03's target must still be restated.** Its "dropping 18 → 12 would cut plate load by a
> third" was written against a 7-tile plate and a count that is now 19. The residual accessibility
> question is unchanged in kind — whether 34–57 N is one-handed-achievable through the §5.4a
> over-centre actuator for a user at Parkinson's H&Y II–III — and it belongs to MECH-2 and the HFE
> formative, not to contact arithmetic.

**Pad geometry consequence — now a requirement, not a suggestion.** D-5 fixes 2.00 mm pitch and
notes `16 × 2.0 = 32 mm across a 40 mm tile; fits with margin`. **At 19 the single-row span is
38 mm**, against a vertex-to-vertex 46.19 mm: it fits only along the long diagonal, and the row
tapers into the hex corners exactly where §7.1's ±0.4 mm lateral blind-mate tolerance is hardest to
hold over a doubly-curved cluster.

> **REQ-SKT-01 (new): the 19-contact pad array is laid out as two staggered rows**, nominally
> 9 + 10 at 2.00 mm pitch (~18 mm span), not a single row. This keeps the array inside the tile
> inradius rather than the circumradius, leaves room for §7.1's required mis-key asymmetry, and
> keeps `SEAT#` at a genuine mechanical extreme so §7.3's last-to-mate sequencing is real. Verified
> by **SH2-DRC-05a**.

Adopting 3+3 is what converts this from the conditional note Rev B first carried ("above ~17
contacts the array *should* go to two rows") into a binding layout requirement — a 19-contact single
row is not viable at 2.00 mm pitch on a 40 mm hex.

#### 5.1.7 Signal naming — one name per conductor (decided 2026-08-11)

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
| 11 | `SAFE_EN[n]` | N5 | **Direct from the Safety MCU** (§6) |
| 12 | `SHLD` | — | Drain / guard reference |
| | **12 conductors per cluster tail** | | |

N3 does not appear — it terminates on the controller by design. N4 lanes are a separate shared
bundle (§3.5), not part of the per-cluster tail. **Conductor 11 (`SAFE_EN[n]`) is conditional — see
§7.1a**; if the single Class C cranial enable is accepted the tail drops to **11 conductors**.

### 5.3 Conductor count — the honest comparison

| Design | Sockets served | Hub-side connectors | Conductors at the Hub PCB |
|---|---|---|---|
| Retired NP-DRV-SHELL-001 | 5 zone modules | 5 × 20-pin ZIF | 100 (+10 separate EEG cables = 110) |
| Option 1 (per-socket tails) | 80 | 80 × 20-pin ZIF | **1,600** |
| Rev A (cluster carriers, 12 clusters) | 80 | *12 × 12-pin* | *144* |
| **Rev B (cluster controllers, 18 clusters)** | **80** | **18 × 12-pin** (20 positions) | **216**, EEG included (240 if all 20 populated) |
| *Rev B if `SAFE_EN[n]` broadcasts (§7.1a)* | *80* | *18 × 11-pin* | *198 (220 at 20)* |

The replacement is **~2.0× the retired conductor count while serving 16× the sockets and absorbing
the EEG harness**, and **~7× fewer conductors** than the naive per-socket design. It is *not* a
reduction against the retired design and this document does not claim one — it is a change from
scaling with socket count to scaling with cluster count. **Rev A stated ~1.3× against 144; the
SYM-1 correction moves that to ~2.0×**, and the comparison is restated rather than left stale.

### 5.4 Power budget — recomputed at 24 V

The load-bearing fact is that **vault PBM power is capped by CLAUDE.md §4.5 regardless of socket
count**: T1 peak ~45–50 W and T2 peak ~70–74 W are *total system* draws, and the per-zone NTC
throttle at 62 °C junction plus the ≤25 % duty cap bound it further. Eighty sockets therefore draw
no more aggregate current than five did; the socket count changes *distribution*, not *magnitude*.

**⚠ Rev A computed this table at a 12 V estimate. The rail is 24 V.** `OI-HUB-C17b` **ADOPTED**
`NP-HW-HEXTILE-001` **D-6**, and `OI-HUB-C18` asks for exactly this propagation. D-6 is derived
twice — from contact current (1.04 A/tile at 24 V vs 2.08 A at 12 V) and from **series-string
length** (11 × 660 nm ≈ 23.1 V, keeping linear drive overhead ≤7 %; at 12 V the string halves,
doubling parallel strings and sense resistors on a tile HEXTILE says has no room for them). Rev A
addressed neither. **OI-SHELL2-01 closes.**

| Quantity | Rev A (12 V) | **Rev B (24 V)** | Basis |
|---|---|---|---|
| Vault PBM instantaneous ceiling | ~35 W | **~35 W** — unchanged | T1 peak 45–50 W less processor/EEG/audio/hub. Power, not voltage |
| Bus rail | 12 V (est.) | **24 V** | D-6 / OI-HUB-C17b. **Regulated, independent of PD** — see below |
| Vault bus current at ceiling | ~2.9 A | **~1.46 A** | 35 W / 24 V — **halved** |
| I²R in every N1 conductor | 1× | **0.25×** | Same conductor, half the current |
| Per-tile instantaneous allocation | ≤3 W / ~0.25 A | ≤3 W / **~0.125 A** | See the reconciliation note below |
| **Worst-case per-cluster feed** | *~1.75 A (7 tiles)* | **~1.46 A — the whole vault budget** | Corrected; see below |
| Per `VLED+` tail conductor (×2) | *~1.45 A* | **~0.73 A** | Sets N1 conductor sizing |
| Cluster power gate | 12 V-rated high-side switch | **24 V-rated** — part class changes | §3.2 |
| Whole-vault simultaneous full drive | **not permitted** | **not permitted** | Firmware vault-wide budget + PDN feed hard cap |

**Three consequences that are not just arithmetic:**

1. **The 24 V rail is a BOOST, not a buck, and it is new hardware.** CLAUDE.md §4.5 negotiates at
   most 20 V and Standard T1 runs from 15 V/2 A, so the vault rail sits *above* the input in normal
   operation. It must be regulated independently of what PD negotiated — Mode 3 runs from any power
   bank. `NP-HW-HUB-001` §7.4 sizes it (15–20 V in → 24 V out, ~35 W, ~1.46 A) and **OI-HUB-C19**
   sites it on the **Hub PCB (provisional, principal 2026-07-30)**: magnetics stay outside the
   shielded envelope away from the fluxgates, the ~1.8 W conversion loss lands on the fan-served
   side, and — the quantitative reason — hub placement carries **~17 % less modulated current across
   the parting-plane boss** (~1.46 A at 24 V vs ~1.75 A at 15–20 V if sited at the PAN), which
   directly helps §9.3. **HUB-REQ-C04** binds: boost control-loop bandwidth ≫40 Hz, so LED duty
   modulation stays in the *current* domain and never becomes 2–40 Hz **rail-voltage** ripple —
   which would be in-band at the entrainment frequencies and could masquerade as an EEG entrainment
   response.
2. **The 2-contact `VLED+` budget and the 12 V assumption were mutually inconsistent, and adopting
   24 V is what repairs it.** `NP-HW-HUB-001` §7.5.5 makes the point this document should have made
   itself: at 12 V, 2 contacts carry **1.04 A each — exactly the ≥1.0 A contact rating, zero
   derating**. At 24 V the same 2 contacts carry 0.52 A, ~2× derating. Rev A's contact count was
   only ever viable at a rail Rev A had not adopted. §5.1.5 carries the residual question, which is
   the *degraded*-contact case, not the nominal one.
3. **The per-cluster feed cannot be sized at 1/18 of the vault — it must be sized at the whole
   vault.** Rev A's "7 tiles hot → ~1.75 A" assumed the hot tiles spread across clusters.
   `NP-HW-HEXTILE-001` §9.2 caps concurrency at **~6 tiles vault-wide** on the power envelope — and
   nothing prevents a protocol from placing all six inside one cluster. So the worst-case cluster
   feed is the vault ceiling itself, **~1.46 A**, not 1/18 of it. This *tightens* N1 conductor sizing
   relative to Rev A's reading even though the current halved, and it is why the vault-wide budget
   must be enforced in firmware (**OI-HEXTILE-09**, the global concurrent-power governor) and not
   merely assumed from the partition.

**Reconciliation note, flagged not papered over.** Rev A's ≤3 W per-tile allocation sits *below*
`NP-HW-HEXTILE-001` §9.2's 6.25 W average for a dual-channel tile at the 25 % duty ceiling (25.0 W
instantaneous). The two figures answer different questions — Rev A's is a sustained thermal share,
HEXTILE's is the duty-cycled electrical average — but they are not reconciled anywhere, and the
conductor sizing depends on which one binds. Routed to **OI-SHELL2-05** alongside the BOM
confirmation.

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
| Hardware | **Per cluster (18)** | `SAFE_EN[n]` gates the cluster's 24 V high-side load switch — no rail, no emission | **No** |
| Software | Per socket (~80) | Module driver register over I2C | Yes — which is why it is not the safety layer |
| Global | Whole vault | PAN feed cut | **No** |

Consequences and rationale:

- **Granularity improves, it does not regress — but Rev A's GPIO claim does not survive the count
  correction.** Five hardware enable domains become **18**, not 12. Rev A asserted the Safety MCU
  *"is an STM32G071 with ample GPIO for 12 enables plus the existing modality enables"*.
  `NP-HW-HEXTILE-001` §8.4 finding 1 shows why that is not demonstrable: **the safety-MCU package is
  not specified anywhere in the document tree** (STM32G071 spans UFQFPN28 at ~22 I/O to LQFP64 at
  ~52), and finding 2 puts demand at 18 enables at **~38–40 I/O** — excluding every ≤32-pin package
  and leaving LQFP48 with almost no margin. **Rev A's "ample" is withdrawn**; the claim is now
  conditional on a package selection that has not been made. Note this is a cost of per-cluster
  *policy*, not of per-cluster *gates* — see §7.1a, which is the cheaper resolution.
- **Fail-safe by construction.** `SAFE_EN[n]` low removes the LED rail from the cluster.
  **⚠ This polarity is opposite to the safety MCU's house convention and the conflict is unresolved.**
  `np_safety_config.h:7-8` specifies *"All stimulation enable GPIOs are active-LOW open-drain:
  LOW = stimulation enabled, HIGH = disabled"*. Both schemes are internally fail-safe — this one
  relies on a pull-down and a de-energized gate, the firmware's on a pull-up and an open drain —
  but implementing `SAFE_EN[n]` to the firmware's convention would make **SH2-DRC-13's "defaults
  LOW at reset" mean *stimulation enabled at power-on*.** Raised as **`NP-CONV-001` OI-CONV-01**;
  assess with OI-FMEA-01 and OI-HUB-C07. Nothing here is changed pending that. No module
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
  carrier's record/stim selector must be gated by `SAFE_EN[n]` as well, so the ≤2 mA (T1) / ≤4 mA
  (T2) drive path cannot be established on a de-energized cluster. The 40 µC/cm² charge-density
  limit remains where CLAUDE.md §4.2 puts it — in the Safety MCU, not here.

---

## 7. Hub PCB Rev C interface contract

This section is the coordination surface for the parallel Hub PCB Rev C work. It states what Rev C
must provide and — equally important — what SMART-1's residual made it look like Rev C would need,
but does not.

### 7.1 What Rev C must provide

**⚠ Rev A provisioned 16 positions and there are 18 clusters. This is the real shortfall in
OI-HEXTILE-14, and it is fixed here rather than renumbered around.** Rev A's 16 was not a margin
figure — it was `8 branches × 2`, i.e. the §3.4 tree's own ceiling — so the same correction that
breaks the tree (§3.4) breaks the connector count. **Both are replaced.**

| Item | Rev A | **Rev B** | Note |
|---|---|---|---|
| Cluster tail connectors | *12 (16 positions)* | **18 populated, spec 20 positions** | `NP-HW-HEXTILE-001` §8.2.2 **option 1, recommended**. 20 rather than 18 absorbs a REG-1 re-cut without a second re-spin |
| Pins per connector | 12 | **12** (11 if §7.1a) | Pinout fixed by §5.2 |
| Total interface pins | *144 (192 at 16)* | **216** (240 if all 20 populated) | vs 100 on the retired 5-slot design |
| Host I2C segmentation | *1 × 8-channel branch switch* | **4 × PCA9548A on LPI2C1–4** | D-7's 32-segment tree (§3.4). 18 of 32 used |
| Safety-MCU enable GPIO | *12 (16 provisioned)* | **18 (20 provisioned)** — or **1**, per §7.1a | `SAFE_EN[n]`, Safety MCU sourced, default LOW at reset |
| `SYNC` driver | 1 | 1 | Broadcast, phase-locked to the EEG sample frame (§9.5) |
| `ALERT#` input | 1 per cluster, wire-OR | 1 per cluster, wire-OR | Or one wire-OR aggregate with I2C interrogation |
| PDN feed | *rated at the vault ceiling* | **24 V boost stage**, ~35 W / ~1.46 A | New Rev C hardware — §5.4, OI-HUB-C19 |
| ADS1299 bank interface | SPI | SPI | The bank sits at the **PAN on L1**, not on the Hub PCB (§3.5) |

**Why 20 rather than widening the tail.** Adding *tails* is the cheap axis and adding *conductors*
is the expensive one — §7.3 records that a single extra conductor costs **16 hub pins** (now 18–20),
because it is paid on every tail. Two extra connector positions cost 24 pins once. This asymmetry is
exactly why `NP-HW-HEXTILE-001` §8.2.2 recommends option 1 and rejects option 7 (pairing clusters
onto shared tails), which would also break "board + clamp + sockets as a single FRU".

### 7.1a A conditional that would make the connector count REG-1-proof — not decided

**Status: PROPOSED and conditional on OI-HUB-C07. Recorded, not adopted.**

`NP-HW-HEXTILE-001` §8.2.2 option 4 observes something about this document's own tail that this
document had not noticed: **N1 is already a tree, N2 is already a tree, and `SAFE_EN[n]` (N5) is the
only *star* component of the 12-conductor tail** (§3 network table). The star is the structural
reason each cluster must terminate at the hub individually.

If `NP-HW-HEXTILE-001` §8.4.1's proposal is accepted — per-cluster gates retained but owned by
**IEC 62304 Class B** for availability, with a **single Class C `NP_SAFETY_EN_PBM_CRANIAL`** in
series as the hard interlock — then `SAFE_EN[n]` becomes a **broadcast**:

- the tail drops **12 → 11 conductors**;
- clusters can tap a **multi-drop trunk** instead of each running a dedicated star leg;
- **connector count becomes largely insensitive to cluster count**, so a REG-1 re-cut no longer
  threatens the provisioning;
- safety-MCU I/O demand drops from ~38–40 to ~23, closing on a mid-range package with margin;
- the per-cluster high-side gate is **unaffected** — it already sits on the cluster controller
  (§3.2), so local gating survives intact.

**R-11 is preserved either way**: the Class C bit is in series, so the Safety MCU still physically
owns the enable path and no Class B fault can re-energise a cut lattice.

**This is conditional, not decided.** The arbiter is **OI-HUB-C07** (safety review), the same item
`NP-HW-HUB-001` §7.4 and `NP-HW-HEXTILE-001` OI-HEXTILE-13 both route there. Until it closes, **Rev
C must be built against 20 positions and 12-conductor tails** — the expensive-and-safe direction.
Do not pre-empt it by provisioning 11.

### 7.2 What Rev C does **not** need

| Feared requirement | Why it is not needed |
|---|---|
| ~80 × DG2788A TIA gain switches | TIA and its gain select moved to the cluster controllers (§3.3) — **18 DG2788A, one per cluster**, and gain is a cluster-MCU GPIO, not a hub signal |
| ~160 high-impedance analog channels crossing the shell | N3 terminates on the cluster controller and never leaves it |
| ~80 `GAIN_SEL` GPIO | Zero. There is no `GAIN_SEL` line from the i.MX RT1062 at all |
| Cascaded multi-stage muxing for 80 sockets | **One hub-side tier suffices** — 4 × LPI2C × PCA9548A = 32 segments, 18 used (§3.4). *(Rev A said "two levels, 8 × ≤2 × ≤8 = 128"; that tree tops out at 16 clusters — see §3.4.)* |
| 80 ZIF receptacles | **18 (20 provisioned)** |

The still-reusable content of `NP-HW-HUB-001 Rev B` is exactly what its own supersession note
predicts: *"the per-socket circuit topology — one DG2788A dual-SPDT switch covering both PD1 and
PD2 TIA gain from a single `GAIN_SEL` line — is component-level and per-socket-independent."*
Correct — and it is replicated onto the cluster carriers, not onto Rev C.

### 7.3 Assumptions the Rev C task should challenge if it disagrees

*(Rev A invited four challenges; `NP-HW-HUB-001` §7.4 answered all four. Their disposition:)*

| # | Rev A assumption | Disposition |
|---|---|---|
| 1 | 12 V bus rail (**OI-SHELL2-01**) | **CHALLENGED AND OVERTURNED.** 24 V, per D-6 / OI-HUB-C17b. §5.4. OI-SHELL2-01 closes |
| 2 | 12-conductor tail — adding a conductor costs 16 hub pins | **Accepted, unchallenged.** The cost is now **18–20** hub pins per conductor, which strengthens it. §7.1a may *remove* one |
| 3 | ADS1299 bank at the PAN, not the Hub PCB | **Accepted, and called the better placement** — keeps µV electrode lanes off the parting-plane boss. Residual **OI-SHELL2-10** stays open |
| 4 | `SAFE_EN[n]` sourced by the Safety MCU, not the i.MX RT1062 | **Accepted and *required*, not merely preferred** — it is HUB-REQ-C02, and it is what keeps the cluster tier IEC 62304 Class B rather than Class C |

**Two new assumptions Rev B adds, which the Rev C task should challenge if it disagrees:**

5. **The cluster controller keeps the analog front end** (§3.2, §3.3a) — this resolves OI-HUB-C17c
   against `NP-HW-HEXTILE-001` D-4 and is a principal direction, but the thermal half of C17c that
   motivated the deferral is still open, and it has moved sides (**OI-SHELL2-11**).
6. **20 connector positions, 18 populated** (§7.1) — conditional on OI-HUB-C07 not adopting §7.1a's
   broadcast enable, which would make the count largely irrelevant.

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

Cluster granularity makes the inner bowl **field-repairable at cluster level** — a damaged
controller board replaces **3–6 sockets** (the realised cluster sizes under the 18-cluster
partition; Rev A said 7, which no cluster reaches), not the whole bowl. This is the concrete advantage over Option 2 and it should
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

| Geometry | Loop area over a 200 mm cluster feed | *Rev A: 0.25 A @ 40 Hz (12 V)* | **Rev B: 0.125 A @ 40 Hz (24 V)** |
|---|---|---|---|
| Same-layer pair, 5 mm apart | ~1,000 mm² | *~1.9 µT* | **~0.95 µT** |
| **Broadside pair, 0.1 mm dielectric** | **~20 mm²** | *~37 nT* | **~19 nT** |

*(B ≈ µ₀·I·A / 2πr³, on-axis, order-of-magnitude only — bench confirmation is **SH2-DRC-17**.)*

**The 24 V rail halves every field figure in this table at equal delivered power** — the field is
linear in current and the geometry is unchanged. That is a real second-order benefit of D-6 that
neither `NP-HW-HEXTILE-001` (which derived 24 V from contact current and string length) nor
`NP-HW-HUB-001` (which derived the ~17 % boss-current benefit from *placement*) states: the
therapeutic-band self-field on L1, which §9.5 exists to make subtractable, is halved before any
mitigation is applied.

**The requirement does not relax.** REQ-EMI-06's ≤25 mm² loop-area limit is a *geometry* constraint
and is unaffected by rail voltage; halving the current is margin, not permission. Fifty-fold
remains the ratio between the two constructions. And ~19 nT is still the same order as the ambient
ELF the cancellation loop exists to null, which is precisely why §9.5 stays mandatory rather than a
refinement.

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

**⚠ Rev A's total ($125–216) was computed at 12 passive carriers. Both inputs changed.** The
controller tier is now costed from `NP-HW-HUB-001` §8.1's component-level derivation × 18, which is
the same **$114.12** that `NP-HW-HEXTILE-001`'s Rev B banner records — the two documents agree.

**Controller tier (per `NP-HW-HUB-001` §8.1 × 18):**

| Line | Qty | Unit | Total |
|---|---|---|---|
| **STM32G071 (UFQFPN32) cluster MCU** | **18** | $1.50 | **$27.00** |
| PCA9548A 8-ch I2C mux (TSSOP-24) | 18 | $0.60 | $10.80 |
| 16:1 PD current mux (2 × TMUX1308-class) | 36 | $0.55 | $19.80 |
| 8:1 NTC mux | 18 | $0.35 | $6.30 |
| Zero-drift TIA op-amp (MCP6V51 / OPA378 class) | 18 | $0.60 | $10.80 |
| Vishay DG2788A gain switch | 18 | $0.20 | $3.60 |
| Rf 47 kΩ + 22.1 kΩ, 0.1 %, ≤25 ppm/°C | 36 | $0.02 | $0.72 |
| Pull-ups, address straps, decoupling, ESD | ~400 | — | $2.70 |
| Cluster PCB (small 4-layer + rigid-flex tail) | 18 | $1.80 | $32.40 |
| **Controller-tier subtotal** | | | **$114.12** |

**Interconnect additions this document owns (not in HUB-001 §8.1):**

| Line | Qty | Unit (est.) | Total (est.) | Note |
|---|---|---|---|---|
| Low-leakage electrode mux (N4) | 18 | $0.60–1.20 | $11–22 | Record/stim select |
| High-side load switch, **24 V-rated** | 18 | $0.35–0.50 | $6–9 | Part class changed by §5.4 |
| Socket spring-contact array | 80 | $0.40–0.80 | $32–64 | **19 contacts each**, two staggered rows (§5.1, REQ-SKT-01) |
| Cluster tail flex + PAN | 1 set | $8–14 | $8–14 | Laminated into L1, 18 tails |
| Blind-mate boss contact set | 1 | $3–6 | $3–6 | 20 tail groups, segregated returns |
| Hub-side: 4 × PCA9548A + PCA9615 pair | 1 set | — | $4.60 | D-7 tree + differential link |
| ~~Module-side UID EEPROM~~ | ~~71~~ | — | **−$8.50** | **DELETED** — D-3 puts an MCU on every tile (§5.1.4) |
| **Estimated total, all lines** | | | **$175–225** | |
| *Retired architecture removed* | | | *−($18–30)* | 5 × FPC tails + 5 × Hirose FH34S + EEG harness |

Three honest observations, the second of which is new and unflattering:

1. **It is a real increase**, on the order of $150–200 at the L1 level, and it is the price of
   16× the sockets — roughly **$2.20–2.80 per socket**, against ~$4–6 per socket for the retired
   five large modules. Per socket the architecture is still cheaper; per headset it is not.
2. **Rev A understated it by ~40 %, and both corrections push the same way.** The cluster count rose
   12 → 18 (+50 % on every per-cluster line) and the carrier gained a $1.50 MCU it did not have.
   Rev A's *"$1.30–2.40 per socket"* is superseded. This is stated because Rev A's number has been
   quoted downstream.
3. **Where it lands still matters, and that argument survives intact.** The cost is concentrated on
   the cluster controllers — ordinary small 4-layer rigid-flex assemblies from a mature supply
   base — rather than on the Hub PCB, where SMART-1's residual implied ~80 analog front ends on a
   single board. `NP-HW-HUB-001` §8.4 makes the same point quantitatively: naively scaling Rev B to
   80 sockets costs ~$67 in silicon *and* ~880 conductors through the parting plane. **The cluster
   tier is not more expensive; it is the same money spent where it works.**

> **Not in this table, and much larger than it:** `NP-HW-HEXTILE-001` §6.4 puts on-module driver +
> metering at **~$11.53/tile ≈ $920/headset** at 80 populated sockets, of which **~$10/tile is two
> InGaAs photodiodes**. That is ~4× everything above and is orthogonal to this document — it is
> **OI-HEXTILE-06** (PD population), and it should be decided jointly with OI-HEXTILE-09.

### 10.2 Manufacturing impact

| Change | Consequence |
|---|---|
| **L1 becomes an electro-mechanical assembly**, not a molded part | New supplier category — rigid-flex lamination into a molded carrier. Add to `NP-PROC-SUP-001` as a new CAT (alongside CAT-A moulding / CAT-B CFRP / CAT-C PDMS). **OI-SHELL2-04** |
| **Module tails eliminated** | The T1-A/B/C tile is a sealed body with a back-face pad array. No FPC tail artwork, no ZIF, no stiffener, no per-module bend qualification |
| **80 × IPX4 socket seams** | NP-HEX-ZM-001 §6 already flags the per-tile seam-length budget; a compression contact array under a co-molded LSR land (NP-HELMET-GEOM-001 §3.1) is compatible, but the contact array is a new ingress path to qualify. **SH2-DRC-11** |
| **Cluster-level rework** | A failed socket is a carrier swap (7 sockets), not a bowl scrap. Improves yield economics and field service |
| **No per-zone molded FPC channels** | The retired REQ-ST-01..07 shell channel features are deleted from shell tooling. Net simplification of the shell tool; the complexity moves into the L1 lamination |
| **Contact plating** | Compression contacts need hard gold on both halves; `NP-HW-HEXTILE-001` §7.1 raises the spec to **≥0.8 µm over nickel, both sides** (from the retired ≥0.5 µm cobalt-alloyed precedent, NP-HW-FPC-001 §5.1) and adds ≤50 mΩ contact resistance and ≥500 mating cycles. Fretting is the wear mode, and §5.1.5 makes it the binding term in the `VLED+` count. **SH2-DRC-09** |
| **The cluster board is now an active assembly** | 18 boards carrying a programmed MCU means firmware load, functional test and traceability at the *board* level, not just the headset level. A new IEC 62304 Class B software unit (`NP-HW-HUB-001` §9.6) ships on it. Adds a programming/test step to L1 lamination that Rev A's passive carrier did not have. **OI-SHELL2-04** |

### 10.3 The thermal load this architecture adds — and nobody owns it

Rev A's passive carrier dissipated essentially nothing, so it raised no thermal question. **Eighteen
active cluster controllers do.** Each carries an STM32G071 plus a zero-drift op-amp, three analog
mux banks and an I2C switch, running **continuously** for the whole session — not duty-cycled like
the emitters.

The reason this is not obviously small is where it lands. `NP-THERM-CFD-C2-001` §7 characterises the
**inter-bowl gap as stagnant air** and puts it at **0.231 m²K/W — the single dominant term in the
entire outward resistance path** (total outward ≈ 0.393 m²K/W, i.e. the gap is ~59 % of it, and the
gap conducts only: Ra ≈ 300 at 6 mm, below the 1708 convection threshold). That analysis was run for
the *fan-off* worst case and concluded the outward path is **~4× more resistive than the inward path
to the perfused scalp** — which is why it predicted NO-GO on Path A and forced Path B1.

**The consequence, stated plainly:** heat added on the gap-facing side of L1 sits behind the most
resistive layer in the outward path, and the preferential direction out of that gap is *inward*,
through the module body and the scalp — the same 42 °C applied-part envelope the whole thermal
programme is defending. Rev A's §4.1 line that gap-facing components are *"out of the scalp thermal
path"* is **not established** and should not be relied on.

**This document does not size it** — it has no CFD and no per-board power figure. It raises it:
**OI-SHELL2-11**, cross-referenced to the THERM-1a CFD (`NP-THERM-CFD-001` case matrix, of which
C2 is one row). Two things the CFD case needs that do not exist yet: a measured or budgeted
per-controller dissipation, and a case with the source on the **gap-facing** side of L1 rather than
at the LED junction plane.

---

## 11. Design review checklist

To be completed in CAD/schematic review before this document can move DRAFT → BASELINED (which
additionally requires REG-1 and ACT-1 to close). Each item requires a named reviewer and a
pass/fail with supporting evidence.

| # | Check item | Method | Pass criterion | Owner |
|---|---|---|---|---|
| SH2-DRC-01 | Cluster partition of the lattice covers every socket exactly once, ≤8 per cluster | Generated from `sync-socket-map.ts` | 100 % coverage, no socket in two clusters | FW/ME |
| SH2-DRC-02 | Cluster count within the D-7 segment budget and the provisioned connector positions at the final lattice *(Rev A read "≤16 (8 branches × 2)" — that ceiling is retired with the Rev A tree, §3.4)* | Count | **≤32 segments and ≤20 connector positions**; 18 at the v1 lattice | ME/EE |
| SH2-DRC-02a | Cluster partition satisfies **CLUSTER-1 + SYM-1 + CONTIG-1** — mirror-symmetric, flower/partial-flower only, zero pendant petals | Generated from `sync-socket-map.ts` vs `np_hextile_cluster_map.svg` | Matches the 18-cluster partition; re-derive on any REG-1 re-cut | ME/FW |
| SH2-DRC-02b | Worst-case **single-cluster** feed sized at the vault ceiling, not at 1/18 of it (§5.4) | Schematic + firmware budget review | Cluster feed ≥ ~1.46 A; global governor present (OI-HEXTILE-09) | EE/FW |
| SH2-DRC-03 | Every cluster carrier ≤50 mm from its furthest socket (N3 run length) | CAD measurement | ≤50 mm | ME |
| SH2-DRC-04 | Cluster tail static bend radius at every formed bend | CAD measurement | ≥12.5 mm (REQ-BR2-01) | ME |
| SH2-DRC-05 | No bend within 5 mm of any rigid-flex transition, stiffener, boss or carrier edge | CAD | REQ-BR2-03 | ME |
| SH2-DRC-05b | Socket pin table matches `NP-HW-HEXTILE-001` §7.2 pin for pin **and name for name** | Mechanical diff of the two tables, not a review | Identical; zero signals differing only in spelling (§5.1.7) | EE |
| SH2-DRC-05a | **19-contact pad array is two staggered rows** and fits inside the tile inradius with mis-key asymmetry and `SEAT#` at a mechanical extreme (**REQ-SKT-01**, §5.1.6) | CAD | Span ≤20 mm; ±0.4 mm lateral blind-mate tolerance held across a full cluster | ME/EE |
| SH2-DRC-06 | No formed bend under a clamp plate footprint or a socket | CAD | REQ-BR2-05 | ME |
| SH2-DRC-07 | Zero dynamic-flex paths in the module interconnect | Design review | Set is empty (REQ-BR2-02) | ME |
| SH2-DRC-08 | Socket contact force, wipe distance and mating cycle rating | Bench | ≥1,000 cycles, wipe ≥0.3 mm | ME/EE |
| SH2-DRC-09 | Contact plating hard gold ≥0.5 µm both halves; fretting resistance | Coupon | Contact R drift <20 % over cycle life | EE |
| SH2-DRC-10 | Cluster clamp plate load with final contact count vs one-handed input force — **restate against a 6-tile plate, not 7** (§5.1.6) | Bench + HFE | RISK-22 intent met with §5.4a actuator | ME/HFE |
| SH2-DRC-10a | **The adopted 3 `VLED+` deliver the ≥2× degraded-case margin §5.1.5's rule asserts.** *(Re-pointed: this no longer selects between 2/3/4 — 3 is decided. It verifies the rule on real contacts.)* | Bench: force one contact to elevated R, measure current share + local ΔT | Survivor ≤0.5 A (≥2× vs ≥1.0 A rating); no thermal runaway over cycle life | EE/ME |
| SH2-DRC-10b | `SEAT#` asserts only when every other contact is home; a partially-seated tile that answers I2C is detected (§5.1.3a) | Bench: partial insertion sweep with PD readback | No plausible-but-wrong dose reading at any insertion depth | EE/FW |
| SH2-DRC-11 | IPX4 maintained at the socket contact array after 10 swap cycles | Test | IPX4 (RISK-16 precedent) | ME |
| SH2-DRC-12 | `SAFE_EN[n]` gates the cluster LED rail and the tES record/stim selector | Schematic + bench | No emission with `SAFE_EN[n]` low, any bus state | EE/Safety |
| SH2-DRC-13 | `SAFE_EN[n]` defaults LOW at Safety-MCU power-on reset | BSP review | LOW before any modality task starts | FW |
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

*(33 items at Rev B; 28 at Rev A; the retired document carried 23.)*

---

## 12. Open items

| ID | Description | Owner | Blocking |
|---|---|---|---|
| ~~OI-SHELL2-01~~ | **✅ CLOSED 2026-08-11 — N1 bus rail is 24 V.** Rev A assumed 12 V as an explicit estimate against this item. `OI-HUB-C17b` **ADOPTED** `NP-HW-HEXTILE-001` **D-6**, derived twice (contact current 1.04 A/tile; series-string length keeping linear overhead ≤7 %) plus a third argument from `NP-HW-HUB-001` §7.5.5 that Rev A's own 2-contact `VLED+` budget had **zero derating at 12 V and ~2× at 24 V** — i.e. Rev A's contact count and its rail were mutually inconsistent. **20 V PD-native was considered and rejected**: Mode 3 runs from any PD source and Standard T1 from 15 V, so the rail must be regulated independently of what PD negotiated, which puts a conversion stage back regardless. Propagated at §5.4, §5.1.5, §7.1, §9.3, §10.1. **Residual, tracked as OI-HUB-C19, not here:** sizing and siting the 15–20 V → 24 V boost (provisionally the Hub PCB) and verifying HUB-REQ-C04 | — (closed) | — |
| OI-SHELL2-02 | **Boss contact-group segregation and independent returns** (§4.3). Cheap now, expensive after MECH-1 tools the posterior boss. **Rev B changes the input, not the requirement: the boss now carries 20 tail groups rather than 16** (§7.1), and if §7.1a's broadcast enable is adopted the {N2/N5} group loses its per-cluster star leg and becomes a trunk — **which changes the segregation layout, not just its size**. Still time-boxed, and now additionally **gated on OI-HUB-C07** for the group structure | EE + ME Lead | **MECH-1 — time-boxed;** input from OI-HUB-C07 |
| OI-SHELL2-03 | Close the contact-count ↔ clamp-input-force coupling (§5.1) jointly with MECH-2 and the §5.4a HFE formative. **Rev B re-bases it, and the count half is now CLOSED.** (a) **The socket contact count is 19 and is decided** (§5.1.4): `SEAT#` adopted from D-5, `SYNC`/`DGND` retained, 2 reserved dropped, N3 retained, and **`VLED+`/`PGND` = 3+3 by principal decision 2026-08-11** under §5.1.5's rule — *loss of any one contact must still leave ≥2× derating*. Rev A's 18, D-5's 16 and **`NP-HW-HUB-001` §7.5.2's 14–15 (void — they assumed D-4 deletes N3, and §3.3a keeps N3)** are all superseded. (b) **What remains is the force question only**, and it is a human-factors one: is **34.2–57.0 N** on a 6-tile plate one-handed-achievable through the §5.4a over-centre actuator at Parkinson's H&Y II–III? Note the count increase is **more than paid for by the partition correction** — 19 contacts on a 6-tile plate is *below* Rev A's 18 on a 7-tile plate (§5.1.6). (c) OI-SHELL2-03's original target ("18 → 12 cuts plate load by a third") was written against a 7-tile plate and must be restated. (d) **REQ-SKT-01 is now binding, not conditional** — a 19-contact single row spans 38 mm and is not viable at 2.00 mm pitch on a 40 mm hex; two staggered rows, verified by **SH2-DRC-05a**. **SH2-DRC-10a is re-pointed** from selecting 2/3/4 to verifying that 3 delivers the asserted margin | ME + HFE + EE | MECH-2; RISK-22 accessibility; **SH2-DRC-05a**, **SH2-DRC-10a** |
| OI-SHELL2-04 | Add a rigid-flex-into-molded-carrier supplier category to NP-PROC-SUP-001. **Rev B widens the scope:** the board is no longer passive, so the category must cover MCU placement, firmware load, board-level functional test and IEC 62304 Class B software traceability on an assembly laminated into a molded part (§10.2) | Procurement + Quality | L1 sourcing; SW item registration |
| OI-SHELL2-05 | Confirm §10.1 BOM estimates against quotations. **Also reconcile the per-tile power allocation**: this document's ≤3 W sustained share vs `NP-HW-HEXTILE-001` §9.2's 6.25 W duty-cycled average (25.0 W instantaneous). They answer different questions and are nowhere reconciled; N1 conductor sizing depends on which binds (§5.4) | EE Lead | Costed BOM; N1 conductor sizing |
| OI-SHELL2-06 | Bus dwell-time budget: worst-case addressed-read sweep of all sockets vs closed-loop adaptation cadence | FW | Session timing |
| OI-SHELL2-07 | Set the fluxgate self-field budget the N1 bus must stay within (SH2-DRC-17 pass criterion) | EE Lead | EMF-1/EMF-2 sign-off |
| OI-SHELL2-08 | Reflect cluster-level repair in the service-network tiering | Service | `docs/reference/service-network.md` |
| OI-SHELL2-09 | **Controlled-document updates this architecture implies but does not make:** NP-HEX-ZM-001 §5.3.1/§5.4a (bus on L1 vs reasons 3–4), NP-HELMET-GEOM-001 §2 (L1 module depth assumes a "20-pin FPC"; tiles now have no tail) and §3.2, and a new FMEA entry for the §4.3 shared-return failure alongside FMEA-G07-01. **Rev B adds three:** (i) ~~**`NP-HW-HEXTILE-001` §7.1–7.2 (D-5)**~~ — **✅ DONE 2026-08-11, HEXTILE Rev C.** The 16-position count and pin table are superseded by §5.1.4's 19; the 2.00 mm pitch, spring-on-socket choice, ≥0.8 µm hard-gold plating, ≤50 mΩ / ≥500-cycle specs and §7.3 mating sequence carried over unchanged, and REQ-SKT-01's two-row array is reflected there. HEXTILE Rev C also raises **OI-HEXTILE-15** (its §5.3/§6 still read as though D-4 holds — module BOM, not tooling-blocking) and **OI-HEXTILE-16** (`GUARD` vs `ELEC_SHLD`, one conductor two names — pick one before Hub PCB Rev C release). (ii) `NP-HW-HUB-001` §3.2 (LED-drive rationale, per OI-HUB-C15), §7.4 (12/16 → 18/20 connectors), §7.5.2 (14–15 figures void) and §7.5.3 (passive-carrier model) — **the signal-naming portion is COMPLETE (2026-08-11): `ELEC`/`ELEC_SHLD` in §7.5.2, `ALERT#` and `SEAT#` in §125, and `ELEC_SIG` → `ELEC` in §113/§124 all aligned per §5.1.7. OI-HEXTILE-16 and OI-HEXTILE-17 both closed; no naming residual remains in this document set**. (iii) `NP-HEX-ZM-001` §5.4a MECH-2 (prices the flower at 12 boards / $76.08; actual 18 / $114.12) | Quality | DHF consistency; **(i) blocks socket tooling** |
| OI-SHELL2-10 | Decide whether the ADS1299 bank sits at the PAN (assumed) or the Hub PCB; moves an SPI interface across the boss. `NP-HW-HUB-001` §7.4 response 3 **accepts the PAN** as the better placement; the item stays open only for the Rev C schematic to confirm | EE Lead | Hub PCB Rev C |
| **OI-SHELL2-11** | **NEW — inter-bowl thermal load from 18 active cluster controllers (§10.3).** Rev A's passive carrier dissipated essentially nothing and raised no thermal question. Eighteen boards each carrying an STM32G071, a zero-drift TIA op-amp, three mux banks and an I2C switch dissipate **continuously** (emitters are duty-cycled; this is not) into the inter-bowl gap, which `NP-THERM-CFD-C2-001` §7 characterises as **stagnant air at 0.231 m²K/W — ~59 % of the entire outward resistance path** (total outward ≈ 0.393 vs inward ≈ 0.108 m²K/W). That analysis already concluded the outward path is **~4× more resistive than the inward path to the perfused scalp**, which is why Path A was NO-GO and Path B1 committed. Heat added on the gap-facing side of L1 therefore sits *behind* the dominant outward resistance, and §4.1's claim that gap-facing components are "out of the scalp thermal path" is **not established**. **Two inputs do not exist yet:** a budgeted per-controller dissipation figure, and a CFD case placing the source on the **gap-facing side of L1** rather than at the LED junction plane. **This is the thermal load that moved sides when OI-HUB-C17c resolved against D-4** (§3.3a) — D-4 would have put this silicon on the tile instead, which is the question C17c's *other*, still-open half asks. The two must be assessed as one budget, not separately | Thermal + EE Lead | **THERM-1a CFD** (`NP-THERM-CFD-001` case matrix); interacts with **OI-HUB-C17c**, SR-FAN-01…06, OI-FAN-01a |

---

## 13. Cross-references

- **Lattice, clusters, two-bowl shell, SMART-1:** `docs/np_hex_zm_001.md` (NP-HEX-ZM-001 Rev A)
  §3.4, §4a, §5.1–5.4a, §7
- **L0–L3 layer topology, L1 material constraints:** `docs/np_helmet_geom_001.md`
  (NP-HELMET-GEOM-001 Rev A) §0, §2, §3.2
- **Superseded predecessor (bend-radius basis §3; EEG separation §2.4; 23-item DRC §5):**
  `docs/neurone_shell_fpc_routing_review.docx` (NP-DRV-SHELL-001 Rev B)
- **Hub PCB Rev C — the cluster-controller tier this revision adopts:** `docs/np_hw_hub_001.md`
  (NP-HW-HUB-001 **Rev C**) §3.1 (decision), §3.2 (**rationale NOT carried forward — see §3.2a**),
  §5 (three-tier I2C), §7.4 (interface contract + 24 V boost), §7.5 (synthesis, OI-HUB-C17),
  §8.1 (controller BOM), §8.3 (STM32G071 selection), §11 (OI-HUB-C07/C15/C17/C18/C19).
  Rev B's TIA saturation analysis survives in its Appendix A §2 and is unchanged physics
- **Hex-tile electrical spec — the socket interface this revision reconciles against:**
  `docs/np_hw_hextile_001.md` (NP-HW-HEXTILE-001 **Rev B**) **§7.1–7.2 (D-5, 16-position pinout —
  the counterpart to §5.1)**, D-3 (on-module driver, every type), D-4 (on-module TIA — resolved
  against, §3.3a), D-6 (24 V), D-7 (32-segment tree), §8.2.1 (18-cluster derivation),
  §8.2.2 (connector options), §8.4.1 (enable class split), §9.2 (concurrency ceiling),
  OI-HEXTILE-01/06/09/13/14
- **Bezel height:** `docs/np_therm_bezel_001.md` (NP-THERM-BEZEL-001 Rev A) §4.5 — 1.0 mm, the only
  calculated value; the competing 2.5 mm is undrived (§4.1 note)
- **Inter-bowl thermal path:** `docs/np_therm_cfd_c2_001.md` (NP-THERM-CFD-C2-001 Rev A) §7 —
  stagnant-air 0.231 m²K/W, the dominant outward term (§10.3, OI-SHELL2-11)
- **Cluster partition diagram:** `docs/diagrams/np_hextile_cluster_map.svg` — the 18-cluster
  midline-symmetric partition with socket ids and cluster boundaries
- **Superseded hub design (TIA topology and saturation analysis, still valid per socket):**
  `docs/np_hw_hub_001.md` (NP-HW-HUB-001 Rev B, retained as Appendix A) §2, §3.1–3.3
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
| **B** | **2026-08-11** | NeurOne Mechanical + Hardware Engineering | **Five Rev A positions replaced, each stated with what it was and why it changed (banner at head of file).** (1) **Cluster carrier → cluster controller** — adds an STM32G071 UFQFPN32, PCA9548A, 16:1 PD current mux, one shared switched-gain TIA (DG2788A), 8:1 NTC mux + ADC, 24 V power gate (principal direction 2026-08-11; NP-HW-HUB-001 Rev C §3.1/§8.3 prevails). **HUB-001 §3.2's LED-drive justification is explicitly NOT carried forward** — HEXTILE D-3 fitted an on-module driver to every tile type and made it irreversible, and OI-HUB-C15 already directs its removal; the surviving justification is scanning/sequencing the retained TIA + PD mux + NTC mux + ADC (§3.2a). Records that this **effectively resolves OI-HUB-C17c against D-4** — the conservative side of C17c's ADC-drift worry (FAI-SM-06), with C17c's on-tile-dissipation half untouched and still open (§3.3a). (2) **N1 rail 12 V → 24 V** per OI-HUB-C17b / D-6; vault bus current ~2.9 A → **~1.46 A**, I²R quartered, N1 conductor sizing and gate part class re-rated, `VLED+` derating 1.0× → ~2×, §9.3 self-field figures halved; **OI-SHELL2-01 CLOSED**; boost siting noted as OI-HUB-C19 (Hub PCB, provisional). Finds the worst-case **cluster feed is the whole vault budget**, not 1/18 of it. (3) **12 clusters → 18** under CLUSTER-1 + SYM-1 + CONTIG-1, provably minimal; Rev A's `8 branches × ≤2 = 16` I2C tree **cannot reach 18** and is replaced by D-7's 32-segment tree; connector positions **16 → 20**, tails 12 → 18, interface pins 144 → 216 (OI-HEXTILE-14, §8.2.2 option 1 + 3). §7.1a records option 4 — broadcast cranial enable → 11-conductor tail and a multi-drop trunk — as **conditional on OI-HUB-C07, not decided**. (4) **Bezel 1.0 mm** where it binds (principal direction; NP-THERM-BEZEL-001 §4.5 is the only calculated value). (5) **Socket contact budget reconciled against NP-HW-HEXTILE-001 §7.1–7.2 (D-5) and CLOSED at 19**: `SEAT#` adopted from D-5 (a partially-seated tile answering I2C while `PD1_K` sits at elevated resistance produces a silent dose under-read), `SYNC` retained (REQ-EMI-03 needs a deterministic phase reference; OI-HUB-C05 is its consumer), `DGND` retained (REQ-EMI-07 requires `PGND` to be LED return *only* for §9.3's broadside pair to cancel), 2 reserved dropped, N3 retained → 17; then **`VLED+`/`PGND` = 3+3 by principal decision 2026-08-11** under the stated rule *loss of any one contact must still leave ≥2× derating* (2 leaves the survivor at exactly the rating, into the fretting→resistance→heating runaway SH2-DRC-09 bounds; 4 spends eight contacts against RISK-22) → **19**. Rev A's 18, D-5's 16 and **HUB-001 §7.5.2's 14–15 (voided — conditional on D-4 deleting N3)** all superseded. New **REQ-SKT-01**: the array is **two staggered rows** (a 19-contact single row spans 38 mm and is not viable at 2.00 mm pitch on a 40 mm hex) — binding, not advisory. Despite two more contacts the 6-tile plate load (34.2–57.0 N) lands *below* Rev A's 7-tile figure, because the 18-cluster partition caps a plate at 6. **`NP-HW-HEXTILE-001` §7.1–7.2 must be co-revised (OI-SHELL2-09(i) — blocks socket tooling).** New **OI-SHELL2-11** — inter-bowl thermal load from 18 active boards into NP-THERM-CFD-C2-001 §7's stagnant air (0.231 m²K/W, ~59 % of the outward path). BOM restated $125–216 → **$175–225**. DRC grows 28 → 33 items (adds SH2-DRC-02a/02b/05a/10a/10b); 11 open items (1 closed, 5 re-scoped, 1 new). Status remains DRAFT pending REG-1/ACT-1. |
| A | 2026-07-29 | NeurOne Mechanical + Hardware Engineering | Initial release. Replaces the retired 5-slot FPC routing architecture of NP-DRV-SHELL-001 Rev B with a cluster-carrier interconnect for the ~80-socket hex lattice. Five-network split (N1–N5); cluster as electrical aggregation boundary; two-level I2C tree reaching exactly `NP_HEXMAP_MAX_SOCKETS = 128`; TIA/AFE relocated from Hub PCB to cluster carriers; per-cluster hardware safety enable; Hub PCB Rev C interface contract (12 connectors × 12 pins, 16 positions provisioned); zero dynamic-flex paths; ≥15 mm EEG separation shown unachievable and replaced by four mechanisms retaining the <5 µVpp threshold; 28-item design review checklist; 10 open items. Status DRAFT pending REG-1/ACT-1. |
