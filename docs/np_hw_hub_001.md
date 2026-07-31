# Hub PCB — Cranial Socket Interface, I2C Fan-Out and PD Analog Front End

**Project:** NeurOne
**Document:** NP-HW-HUB-001
**Revision:** C
**Date:** 2026-07-29
**Status:** DRAFT
**Effective Date:** —
**Author:** NeurOne Hardware Engineering
**Approved By:** — (DRAFT — not yet baselined)
**References:** NP-HEX-ZM-001 §3.4/§4a/§5.4a/§7 (SMART-1, REG-1, MECH-2, OI-HUB-SOCKET-01); NP-HW-HUB-001 Rev B (retired, Appendix A); NP-HW-FPC-001 Rev E §5.3 (SUPERSEDED — TIA saturation physics retained); NP-FW-PBM1064-001 Rev B (SUPERSEDED); NP-SES-1064-001 Rev A (SUPERSEDED); CLAUDE.md §4.1/§4.2/§4.5
**Related Issues:** GitHub Issue #62 (OI-PBM-HW-01, closed against Rev B)
**Gate:** —
**IEC 62304 Class:** — (hardware; allocates firmware to SW-02 Class B and a new cluster-controller unit, §9.6)
**Supersedes:** NP-HW-HUB-001 Rev B (5-slot DG2788A gain switch + single PCA9546A I2C mux)
**Parent Document:** —

---

**Rev C (2026-07-29):** Complete re-architecture of the cranial socket interface for the
`NP-HEX-ZM-001` hex-tile lattice under **SMART-1** (every socket I2C/TIA-capable). Rev B's
hub-centric star — five per-slot TIAs, five `GAIN_SEL` GPIO, one 4-channel I2C mux — is
retired and archived verbatim in **Appendix A**. Rev C introduces a **distributed
cluster-controller tier** on the inner bowl: the hub PCB no longer terminates per-socket
analog or per-socket I2C at all, and **no longer encodes the socket count**. Rev B's
component-level circuit work (DG2788A dual-SPDT switching 47 kΩ ↔ 22 kΩ, and the silicon-vs-InGaAs
TIA saturation analysis that motivated it) is carried forward unchanged — §6 reuses it,
relocated and shared rather than replicated 80×.

> ### ⚠⚠ RECONCILIATION REQUIRED — NP-HW-HEXTILE-001 Rev A (2026-07-30) lands under this document
>
> **Read this before §3, §5, §6 or §8.** `NP-HW-HEXTILE-001` Rev A merged to `main` *after* this
> revision was drafted, and it fills the exact gap this document scoped out as unwritten
> (**OI-HUB-C01**: the per-socket FPC pinout and LED drive stage). It makes two decisions one level
> down that **delete large parts of the design below**. This banner records the collision honestly
> rather than leaving two documents asserting incompatible architectures; the merge is **not yet
> done** and is tracked as **OI-HUB-C15**.
>
> **What HEXTILE supersedes here:**
>
> | This document | Superseded by | Effect |
> |---|---|---|
> | **§6 entirely** — shared muxed TIA, one DG2788A per cluster, 16:1 PD current mux, per-sample gain, scan-timing budget | HEXTILE **D-4**: TIA + ADC move **on-module**; no PD analog crosses the socket interface | **Deleted, not revised.** HEXTILE states it outright: *"The ~80× DG2788A + cascaded-analog-mux Hub PCB NRE that SMART-1 opened is not redesigned — it is deleted."* §6 *is* that redesign. Gain switching ceases to exist: the module knows its own PD, so gain is fixed at design time. |
> | **§3.2** — "the tier needs local intelligence because LED drive must distribute" | HEXTILE **D-3**: constant-current driver on-module; socket carries a **24 V DC bus** (D-6), not per-string drive | The load-bearing argument for a cluster **MCU** is removed. |
> | **§5.2** — cluster MCU as I2C master behind a per-cluster PCA9548A, tunnelled transactions | HEXTILE **D-7**: **dynamic UID-based address assignment** (SMBus-ARP style) over per-cluster segments, driven directly from RT1062 LPI2C1–4 through **one** PCA9548A tier | The 0x30 collision is *removed*, not worked around, so muxing is needed only for bus capacitance — one tier, no cascade, no intermediate master. HEXTILE is right and this is the better answer. |
> | **§8 BOM** | above | Per-cluster TIA op-amp, DG2788A, PD mux and NTC mux all drop out. The $6.34/board figure is void. |
>
> **What survives, and is independently corroborated by HEXTILE:**
>
> - **The interconnect-first finding (§2).** HEXTILE §7 reaches it separately — *"80 sockets × up to 16 current-carrying conductors = ~1,280 power conductors… None of that is buildable."* Two documents, two routes, same conclusion.
> - **Per-cluster safety gating (§7.2).** HEXTILE **D-8** gates VLED per cluster, "replacing the retired `NP_SAFETY_EN_PBM_ZONE_0..4`", on the identical reasoning that an STM32G071 cannot present 80 GPIOs. Convergent, including the accepted coarse-granularity consequence.
> - **Electrical segmentation follows the mechanical clusters (§4.4).** HEXTILE **D-7** states it as *"one physical grouping serving mechanics, power, safety, and addressing rather than four incompatible partitions"* — the same answer to the same question.
> - **§4.2** anatomical socket numbering + generated `socket_id → (cluster, channel)` table; **§4.3** the hub not encoding socket count (REG-1 unblocked); **§4.5/4.5.1/4.5.2** the two address spaces, the rejection of 3-level addressing, and the unbacked-`NP_GROUP_KIND_LOBE` finding — all untouched by HEXTILE and still current.
> - **§9.5** calibration keyed to the module, not the socket — *strengthened*: with the TIA and PD both on-module, the coefficients are unambiguously module property.
>
> **The gap HEXTILE leaves, which this document must now answer (OI-HUB-C16).** HEXTILE's 16-pin
> socket reserves pins **13 `ELEC_SIG` / 14 `ELEC_SHLD` / 15 `AGND`** at *every* socket for T1-B, and
> is explicit that *"an electrode signal cannot be carried over I2C — it is a µV analog recording path
> to the ADS1299 **and** a stimulation current path from the tES driver."* It then declares T1-B out
> of scope for Rev A and never routes them. At 80 sockets that is **240 µV-analog + stimulation
> conductors to the hub** — the same unbuildable star HEXTILE just used to kill hub-side LED drive and
> hub-side PD analog, now applied to the one signal class that genuinely *cannot* be digitised at the
> module. It cannot be dodged by restricting T1-B to a subset of sockets: that re-imposes precisely
> the placement constraint SMART-1 was decided to remove.
>
> **So the cluster tier survives — with a different and stronger justification.** Not PD analog, not
> LED drive, not I2C mastering (HEXTILE takes all three), but an **electrode analog crosspoint**:
> routing any socket's ELEC_SIG to one of N ADS1299 / tES channels (8 for T1, 21 for T2), plus
> `/ALERT` and `SEAT_N` aggregation. That is per-cluster, and it aligns with the same partition D-7
> and D-8 already use. Whether it needs an MCU or reduces to a switch matrix is the open question.

> **⚠ STATUS: DRAFT, NOT BASELINED.** The socket count (~80, provisional pending REG-1) and every
> specific part number below are **open engineering decisions**, deliberately presented with the
> reasoning that produced them rather than as settled selections. §4.3 is the load-bearing claim:
> this design is *intended to be correct across the whole 30–128 socket range* so that REG-1 landing
> on a different number does not force a hub re-spin. Numbers that would change with the socket
> count are given as formulas plus a worked value at n = 80, never as bare constants.

---

## 1. Scope

This document specifies the NeurOne hub PCB's interface to the cranial socket lattice: how the hub
reaches ~30–128 hex-tile sockets for (a) I2C control of smart modules, (b) PD1/PD2 dose-metering
analog acquisition with switchable TIA gain, and (c) safety-gated PBM drive enable.

**In scope:** hub↔inner-bowl bus architecture; the cluster-controller tier and its allocation of
sockets; I2C fan-out and address-collision resolution; the PD analog chain and gain switching; the
safety-enable topology and its wire-format consequence; BOM at the new socket count; firmware and
HAL impact.

**Explicitly NOT in scope, and flagged rather than silently assumed (see §11):** the per-socket FPC
pinout and the LED drive stage. §3.2 shows the LED drive *must* distribute onto the same cluster tier
— that is a direct consequence of this architecture and is stated as a requirement here — but the
driver topology, current regulation, and connector pinout belong with the socket-interconnect spec
that `NP-HW-FPC-001` Rev E's supersession note correctly identifies as **unwritten, not merely
superseded**. Rev C does not attempt to write it.

---

## 2. Why Rev B does not extend — the constraint is interconnect, not mux count

The obvious framing of this problem is "one 4-channel I2C mux does not cover 80 sockets, so build a
mux tree." That framing is true but it is not the binding constraint, and designing to it produces a
hub PCB that cannot be built.

Rev B is a **star**: every socket's analog and digital lines terminate at the hub. Per socket, Rev B's
architecture requires the hub to terminate:

| Signal | Conductors | Note |
|---|---|---|
| LED_A drive + return | 2 | 120–180 mA per LED string |
| LED_B drive + return | 2 | |
| PD1, PD2 analog | 2 | high-impedance current-mode, to hub TIA |
| NTC analog | 1 | per-tile 42 °C / 62 °C interlock |
| SDA, SCL | 2 | smart modules (SMART-1: now *every* socket) |
| VCC_3V3, GND | 2 | |
| **Total** | **~11** | |

At the five slots Rev B was written for, that is ~55 conductors — a routable harness. At the v1
80-socket lattice it is **~880 conductors crossing the inner-bowl-to-hub boundary**, through the
single posterior blind-mate boss defined in `NP-HEX-ZM-001` §5.3c. That is not a hard problem; it is
a different product. It also multiplies against every other Rev B quantity:

| Rev B quantity | At 5 slots | Naive scale to 80 sockets |
|---|---|---|
| TIA op-amp channels on hub | 10 | **160** |
| Hub ADC channels consumed | 10 | **160** (i.MX RT1062 LPADC1+LPADC2 provide ~32 external — needs a second fan-in mux tier regardless) |
| `GAIN_SEL` GPIO from RT1062 | 5 | **80** |
| DG2788A gain switches | 5 | **80** |
| Precision Rf resistors | 20 | **320** |
| I2C mux ICs | 1 | **~27** (3-deep PCA9546A tree) |
| Conductors at the parting plane | ~55 | **~880** |

**The finding: the I2C mux count is the *least* severe of these.** A 3-deep PCA9546A tree is only
~$13.50 of silicon and its per-transaction overhead (three channel-select writes before each payload,
~150 µs at 400 kHz) is affordable against a 100 ms dose tick. It fails not on its own merits but
because it leaves the other six rows of that table untouched — it solves address collision while
still requiring 160 analog channels and 880 conductors to reach the hub.

So Rev C does not choose between mux topologies. It **moves the termination point**: the per-socket
analog front end, the per-socket I2C master, and the per-socket LED drive relocate from the hub PCB
onto the inner bowl, and the hub-to-inner-bowl link becomes a short multi-drop bus instead of a
wide star.

---

## 3. Architecture decision — distributed cluster-controller tier

### 3.1 The decision

**A tier of small cluster-controller boards is added on the inner bowl. Each serves up to 8 sockets.
The hub PCB talks to the cluster controllers over one differential I2C bus and has no per-socket
signal of any kind.**

```
  i.MX RT1062 (hub PCB)
        │  LPI2C  +  ATTN(open-drain)  +  PBM_CRANIAL_EN (from safety MCU)
        │
   [PCA9615-class I2C↔differential transceiver]      ← hub PCB, §5.1
        │
════════╪═══════ posterior blind-mate boss (NP-HEX-ZM-001 §5.3c) ═══════════
        │        ~10 conductors per cluster · ~100 total at n=80
        │
   ┌────┴──────────────── inner-bowl cluster bus (multi-drop) ─────────────┐
   │              │              │                          │
 [CC0]          [CC1]          [CC2]        …             [CC9]     ← §3.3
   │              │                                          │
   │  each cluster controller owns ≤8 sockets:
   │    · PCA9548A            → I2C isolation, 1 channel per socket   §5.2
   │    · 16:1 current mux    → PD1/PD2 of 8 sockets                  §6.2
   │    · 1 shared TIA        → DG2788A gain switch, per-sample       §6.3
   │    · 8:1 mux + ADC       → NTC per socket
   │    · LED drive stage     → gated by PBM_CRANIAL_EN               §7.2
   │
 socket ×8 (unchanged FPC per socket)
```

### 3.2 Why the tier needs local intelligence rather than remote muxes

A cheaper intermediate exists: put only *muxes* on the inner bowl (one analog mux + one I2C mux per
cluster, plus a GPIO expander to drive the select lines) and keep the TIA, ADC and all control on the
hub. This was considered and rejected.

It fails on **LED drive**. T1-A base tiles (the majority population — `NP-HEX-ZM-001` §4a: "the
majority of sockets take T1-A") carry no on-module driver; their LED current comes from the hub.
Eighty sockets × two channels of 120–180 mA drive cannot be star-routed any more than the analog can.
Once the LED drive stage has to distribute — and it does, unavoidably — the distributed board needs
PWM generation (2–40 Hz, ≤25 % duty, firmware-enforced), per-socket current setpoints, and per-socket
NTC thermal response. That is a microcontroller's job, and having placed one there, terminating the
PD chain and the I2C master locally costs almost nothing extra.

This is the same conclusion the T1-C module reached one level down: `NP-HW-FPC-001` §6.2 put an
ATtiny402 on the module because the connector had no pins left for a third drive channel. Rev C
applies the identical reasoning one level up, where the constraint is conductors at the parting plane
rather than pins at a connector.

### 3.3 Alternatives considered and rejected

| Option | Verdict | Reason |
|---|---|---|
| **Cascaded PCA9546A/PCA9548A tree, hub-centric** | Rejected | Solves address collision only. Leaves 160 analog channels, 80 `GAIN_SEL`, ~880 conductors. ~27 ICs, 3 mux writes per transaction, global mux state where one hung branch takes out a subtree. |
| **I2C address translation per socket (LTC4316-class)** | Rejected | ~$2.50 × 80 = **$200** silicon alone, still one IC per socket, still a star. Also consumes ~80 of ~112 usable 7-bit addresses on one bus. |
| **Dedicated socket-scanning MCU on the hub PCB** | Rejected | Moves the software problem off the RT1062 but changes no wire. The star, and every row of the §2 table, survives intact. |
| **Make every socket smart (mandate on-module ATtiny402 on T1-A)** | Rejected | Fixes interconnect, but by adding ~$0.50 × 80 = $40 of *module* BOM and erasing the T1-A/T1-C cost split that `NP-HEX-ZM-001` §4a exists to preserve. It is also a module decision, not a hub decision — out of this document's authority to mandate. |
| **Distributed cluster-controller tier** | **Selected** | Only option that addresses interconnect. Wins on I2C, TIA count, gain control and PD1/PD2 ratio accuracy as consequences rather than as separate design effort (§6.4). |

### 3.4 What Rev B contributed that survives

Carried forward unchanged, per Rev B's own supersession note:

- **The TIA saturation analysis** (Appendix A §2): silicon PD ≈ 0.47 A/W at 808 nm vs. Hamamatsu
  G12180-010A InGaAs ≈ 0.90 A/W at 1064 nm; at Rf = 47 kΩ the InGaAs PD1 reaches 3.38 V against a
  3.3 V rail and saturates; Rf = 22 kΩ yields 1.58 V. The physics is unchanged and the two resistor
  values are adopted as-is.
- **The DG2788A dual-SPDT** as the switching element, and Vishay Siliconix as an already-qualified
  analogue-front-end supplier.

What changes is only *how many* and *where*: one shared switched-gain TIA per cluster (§6) instead of
two per socket.

---

## 4. Socket-to-cluster topology

### 4.1 Cluster **capacity** = 8 sockets — a capacity, not a shape

Set by the analog mux, the tightest fit in the chain: one 16:1 low-leakage CMOS mux carries exactly
8 sockets × (PD1 + PD2). Other per-cluster resources are slack at 8 (PCA9548A is an 8-channel part —
exact fit; NTC needs an 8:1; the MCU ADC needs ~4 channels total).

**8 is a ceiling the board is built to, not a count every board must hit.** This distinction is
load-bearing — see §4.4. A board serving 7 sockets leaves 2 mux channels and 1 I2C channel spare,
and that spare costs **nothing**: I2C switches come in 4- and 8-channel parts and analog muxes in
4:1/8:1/16:1, so there is no 7-channel part to buy instead. Capacity 8 is the natural part boundary
and is the same price as a hypothetical capacity 7.

Board count = `ceil(n_sockets / sockets_per_cluster)`, with `sockets_per_cluster ≤ 8`.

Board counts at full capacity-8 population (the **lower bound** on board count; §4.4 tabulates the
7-hex-flower case, which is the recommended MECH-2 alignment and costs ~20 % more boards):

| n_sockets | Boards at 8/cluster | Note |
|---|---|---|
| 30 (§3.1 area-quotient lattice) | 4 | low end of the provisional range |
| 42 (§3.1 geometry ceiling at W = 34 mm) | 6 | |
| **80 (v1 scan-grounded lattice, PROVISIONAL)** | **10** | worked example throughout this document |
| 128 (`NP_HEXMAP_MAX_SOCKETS`) | 16 | design maximum at 8/cluster |

**16 boards × 8 sockets = 128 = the full 7-bit `NP_HEXMAP_SOCKET_BITS` domain exactly** — the tier's
maximum capacity lands precisely on the firmware addressing ceiling, so neither bound binds before
the other. Note this is an aesthetically pleasing coincidence, **not** an argument that every board
must be full: at the recommended 7-tile flower the count is 19 at n = 128, still inside the
32-controller tier-1 bus (§4.3, §4.4).

### 4.2 Socket numbering stays anatomical — the tempting bit-packing is rejected

The exact-fit above invites encoding `socket_id = (cluster_id << 3) | channel`, making the address
decode free. **This is rejected.**

Socket numbering is anatomical and load-bearing downstream: the row-major lattice order in
`NP-HEX-ZM-001` §3.2, the zone definitions in `protocols/predefined/00-zones.npps`, the generated
`app/web/src/lib/socketMap.generated.ts`, and the REG-1 10-20 registration all depend on socket ids
meaning positions. Cluster boundaries, by contrast, follow inner-bowl FPC routing and curvature
(and ideally the mechanical clusters of `NP-HEX-ZM-001` §5.4a, still open as MECH-2). Forcing
anatomical numbering to follow wiring convenience would contort a clinical addressing scheme for a
few gates of decode logic.

**Resolution:** the hub carries a `socket_id → (cluster_id, channel)` lookup table, 128 bytes,
generated by `scripts/sync-socket-map.ts` alongside the existing artifacts so it cannot drift from
the lattice. Anatomical numbering is preserved.

### 4.3 The hub PCB does not encode the socket count — the point of the whole design

Rev B baked `5` into hub tooling: five DG2788A footprints, five `GAIN_SEL` nets, a PCA9546A sized
4-of-5. Any change to the slot count meant a hub re-spin.

In Rev C the hub PCB terminates a bus with up to 16 addressable peers. It contains no per-socket
footprint, no per-socket net, and no constant derived from `n_sockets`. The lattice lives entirely on
the **inner bowl** — which is already the part that gets re-tooled when the lattice changes
(`NP-HEX-ZM-001` §5.1: the inner bowl is the module carrier; the outer bowl and the hub are not).

**Consequence: REG-1 stops blocking hub PCB tooling.** REG-1 can move the lattice anywhere in
30–128 sockets, and re-cut the row/lobe boundaries against shell CAD, without a hub Gerber change.
Only the inner-bowl cluster-board count and their FPC fan-outs move. This is the single most valuable
property of the architecture and it is worth more than the BOM difference between any two options in
§3.3.

The tier-1 bus is specified for **up to 32 cluster controllers** (5-bit strapped address), which
bounds every MECH-2 cluster shape worth considering — see §4.4.

### 4.4 Electrical clusters and the mechanical cluster clamps SHOULD be the same

`NP-HEX-ZM-001` §5.4a clamps modules in **mechanical** clusters — one over-center actuator per
cluster driving a plate of per-module spring plungers — with the **7-hex "flower"** (1 centre +
6 neighbours) named as the natural super-cell and a **3-hex triad** as the smaller alternative.
Final size is open as **MECH-2**. The question is whether that cluster and this document's
electrical cluster can be one and the same thing.

**They can, and they should. §4.1's capacity framing is what makes it nearly free.**

The apparent obstacle is that 8 (electrical) ≠ 7 (flower). It dissolves once the board is understood
as capacity-8 rather than exactly-8: a 7-tile flower populates 14 of 16 mux channels and 7 of 8 I2C
channels on the *same board design*, at the *same board cost* (§4.1). Neither number divides 80
anyway, and a bounded lattice with irregular row widths (`3 6 7 8 9 8 9 8 7 6 5 4`) has partial
clusters at its boundary under any tiling — so "every board full" was never achievable and is not
the property to optimise for.

**One universal cluster-board SKU, capacity 8, populated to whatever shape MECH-2 picks.** This is
the same move `NP-HEX-ZM-001` makes one level down for the tiles themselves — "standardize the sold
part, not the geometry" (§Principles), one module SKU absorbing the head's variability in the shell.
Applied one level up: one board SKU absorbing the lattice's boundary irregularity and MECH-2's
still-open shape choice. A partial boundary flower of 3–6 tiles takes the identical board.

**What alignment buys:**

1. **FPC routing.** The board sits at its cluster's centroid in the inter-bowl gap, beside the clamp
   actuator, with short symmetric tails. This matters more than usual here because the PD lines are
   current-mode analog and must be guarded away from LED drive (**HUB-DRC-C15**); misaligned clusters
   mean tails crossing cluster boundaries and getting longer for no benefit.
2. **Service atomicity.** Releasing one clamp takes exactly one board's sockets to "not present".
   `NP-HEX-ZM-001` §5.4a already relies on an unseated tile self-detecting via failed inventory poll;
   alignment makes that signal clean and per-cluster rather than smeared across two boards.
3. **One FRU.** Board + clamp + sockets becomes a single tested sub-assembly and a single service
   part — relevant to the partner-tier service network.

**Cluster shape is now DECIDED — CLUSTER-1, 2026-07-30, principal direction:** the **7-hex flower**
wherever the lattice allows one, **partial flowers** at the boundary. Recorded in `NP-HEX-ZM-001`
§5.4a; this document adopts it.

The deciding argument is **mechanical, not electrical**, and it is the stronger one. The flower is
the most compact possible 7-tile group (7 being the centred-hexagonal number), so an 8th tile must
attach on the perimeter and necessarily lengthens the cluster. Computed at W = 40 mm against the
R_m = 87 mm curvature median, by the sagitta method that reproduces `NP-HEX-ZM-001` §3.1's own
3.1 mm single-tile dome depth:

| Cluster | Longest span | Arc subtended | Dome depth |
|---|---|---|---|
| **7-hex flower ★** | **122.2 mm** | **89.2°** | **25.1 mm** |
| 8-tile (flower + 1) | 161.8 mm | 136.8° | 55.0 mm |

The 1.32× span increase costs clamp-plate bending stress ×1.75 and plate-mode deflection ×3.07 —
and plate compliance is exactly what §5.4a's per-module plungers exist to defeat ("a rigid plate
over a curved cluster could not seat evenly"). Stored dome depth more than doubles, so cluster
sub-assemblies pack far bulkier. An 8-tile patch spans 136.8° of the curvature sphere — over a third
of it — which makes a rigid plate arguably marginal rather than merely suboptimal, and pushes against
the ≤1 N one-handed input-force intent (RISK-22) because plate stiffness must rise to compensate.

**Why the earlier BOM-gradient argument is withdrawn.** A previous revision of this section
recommended the flower on cost (12 boards / $76.08 vs 10 / $63.40 at n = 80, with the 3-hex triad at
$171.18). Those figures rested on the $6.34/board BOM that `NP-HW-HEXTILE-001` Rev A has since
voided by moving the driver and TIA on-module (see the reconciliation banner and **OI-HUB-C15**), so
they are no longer quotable. The mechanical argument is unaffected by that revision — which is the
useful property: **the cluster shape is now settled independently of an electrical BOM still in
flux.** The one electrical constraint that survives is a bound, not a preference: the triad's 43
segments at n = 128 exceed the 32-segment addressing budget, so the triad remains excluded on
electrical grounds too.

MECH-2 now **verifies** the flower (plate seating and one-handed input force at the 122 mm span)
rather than selecting a cluster size.

### 4.5 Why this does NOT become a three-level address

The natural next thought is that an aligned cluster gives a three-level hardware address —
`(cluster : module-in-cluster : element-in-module)` — replacing today's two-level
`(socket : element)`. **Rejected**, for the same reason §4.2 rejects bit-packing `socket_id`, only
more strongly.

**The middle level is not a new identity.** "Module within cluster" and "socket" name the same
physical thing. Splitting one level into two adds no information; it re-expresses position as
`(where the wiring goes, which pin)` instead of `(which place on the head)`.

**And it swaps a stable axis for an unstable one.** Socket id is *anatomical* — it is what
`00-zones.npps`, `socketMap.generated.ts`, saved user protocols, published protocol files, and the
REG-1 10-20 registration all key on. Cluster membership is *topological* — it changes whenever
MECH-2 revisits the clamp shape or REG-1 re-cuts the lattice. Encoding cluster in the address makes
every clamp-pattern revision renumber the clinical address space. Worse, it routes a mechanical
service decision through the safety-critical addressing path: `np_module_map.h` is explicit that a
mis-resolved socket "is a wrong-site stimulation path, not a data-quality issue."

It also does not fit the wire. Cluster (5 bits, ≤32) + module (3 bits) + element (7 bits) = **15
bits**, against the 14-bit packed address `np_hex_addr_pack()` produces, `NP_HEX_ADDR_MAX = 0x3FFF`,
and the `_np_hexmap_socket_mask_fits` compile-time assert. A wire-format change, for negative value.

**The correct model is two address spaces, and conflating them is the trap:**

| | Logical / clinical | Physical / transport |
|---|---|---|
| Form | `(socket : element)` — 7 + 7 | `(cluster : channel : element)` — 5 + 3 + 7 |
| Axis | Anatomical, stable | Topological, changes with MECH-2 / REG-1 |
| Used by | Protocols, zones, groups, the app, REG-1 | The hub HAL and cluster-controller firmware only |
| Bridge | `socket_id → (cluster_id, channel)`, generated by `sync-socket-map.ts` (§4.2, **OI-HUB-C10**) | |

So the three-level structure **does** exist — it is real, and it is exactly what
`np_hub_cluster_read_frame(cluster_id, …)` (§9.3) addresses. It lives at the transport layer, below
the logical address, where a re-clustering costs one regenerated table and nothing else. The
existing firmware already has this shape latent: `np_module_map` separates GEOMETRY
(`socket → lobe/side/x/y`) from INVENTORY (`socket → UID/elements`); the cluster map is a third map
of the same kind, not a fourth address field.

**What is worth adding — a group kind, not an address level.** "Which sockets did that clamp
release just unseat", "this cluster controller stopped answering, mark its sockets absent", "run a
PD self-test on every photodiode in cluster 3" are genuinely useful operations, and all of them are
*cluster → sockets* enumerations. They belong in the existing group machinery: a
`NP_GROUP_KIND_CLUSTER = 3` alongside `NP_GROUP_KIND_LOBE` / `_SOCKET_SET` / `_ADDR_SET` in
`np_group_query_t`, plus a `uint8_t cluster_id` field, resolving through the §4.2 table.

Mechanically it is the cheapest possible addition: one ascending pass over the socket table visiting
each socket exactly once (so it cannot self-duplicate and needs no `seen` bitmap), with the predicate
`cluster_of(socket) == q->cluster_id`. It inherits the type filter and the resolver's dedup guarantee
for free.

#### 4.5.1 The discriminator: does changing this membership require re-tooling hardware?

An earlier draft justified this kind by noting it is structurally a clone of the existing
`NP_GROUP_KIND_LOBE` case. **That was the wrong justification** — LOBE is the example that should be
deleted, and the reason it should be deleted is exactly the reason CLUSTER is legitimate.

| Group kind | Source of truth | Changed by | Belongs in firmware? |
|---|---|---|---|
| `LOBE` | `protocols/predefined/00-zones.npps`, derived from the lattice | zone re-cut / **REG-1** — no hardware change | **No — retire (§4.5.2)** |
| `SOCKET_SET` | the protocol itself, via the socket bitmap | protocol author | n/a — passed in per query |
| `ADDR_SET` | the protocol itself | protocol author | n/a — passed in per query |
| `CLUSTER` | the inner-bowl FPC routing | **inner-bowl re-tool only** | **Yes — legitimately** |

> **Generalised by ZONE-1 (2026-07-30, principal — `NP-HEX-ZM-001` §3.3).** The rule below was
> written for firmware; it holds for **all code**. Zones live only in `00-zones.npps`, and
> nothing anywhere may define, derive or hardcode a lobe — including
> `scripts/sync-socket-map.ts`, which currently derives lobe membership from four anatomical
> constants and diffs it against the zone file. That derivation is redundant (the zone file is
> self-contained) and its stated justification is circular (the file is regenerated from those
> same constants, so the check proves only that the file matches the code — it validates no
> anatomy). Firmware's share of the cleanup is **OI-HUB-C14**; the generator and app share is
> ZONE-1.

**The test is whether the membership can change without re-tooling hardware.** Lobe membership can:
REG-1 will re-cut the row/lobe boundaries against shell CAD and regenerate `00-zones.npps`, with no
physical change whatsoever. A firmware-resident lobe table is therefore a second source of truth for
a data file, and one that goes stale silently. Cluster membership cannot: which sockets land on which
cluster board is a physical property of a built inner bowl, fixed at assembly, and no protocol
author, zone re-cut, or REG-1 iteration can alter it. It is a fact *about the board the firmware is
running on* — the only category that belongs in firmware.

#### 4.5.2 `NP_GROUP_KIND_LOBE` is not merely outdated — it is unbacked

Three findings, each verifiable by grep:

1. **No caller.** `np_module_map_predefined()` and the `NP_PGROUP_*` enum have zero references outside
   their own definition and `np_module_map_tests.c`. `NP_GROUP_KIND_LOBE` is reachable in production
   only through that uncalled function.
2. **No data.** There is no production `np_socket_geom_t` table anywhere in the tree — only the
   typedef, the `np_module_map_init()` parameter, and dereferences. The `lobe`/`side` fields are
   populated exclusively by test fixtures.
3. **No generator.** `scripts/sync-socket-map.ts` emits `socketMap.generated.ts` and
   `hardware/np_socket_map.json`. It emits no firmware C, so nothing could keep a firmware lobe table
   in sync with `00-zones.npps` even if one were written.

Meanwhile the live path carries zone membership end to end without touching any of it: the app reads
`00-zones.npps`, compiles to a `NP_PROTO_TARGET_SOCKET_MASK` bitmap, and firmware turns that into a
`NP_GROUP_KIND_SOCKET_SET` query via `np_protocol_socket_expand()`. Zones are data, and the data path
already works.

**This matters beyond tidiness.** If anything ever does call `np_module_map_predefined()` after REG-1
re-cuts the boundaries, it resolves against a lobe assignment with no generator behind it — a
wrong-site dose from a stale anatomical map, the hazard class `np_module_map.h` is otherwise loud
about. Retiring the path is the conservative action; leaving dead code that only becomes dangerous
when someone finds it is not.

Retirement is **not** done in this PR — it deletes an enum, a public function, struct fields, and
touches several of the 63 host checks, which is a firmware change that deserves its own review rather
than riding along with a hardware spec. Tracked as **OI-HUB-C14**. Note `np_physical_loc_t` also
carries `lobe`/`side` from `np_module_map_resolve()`; `x_mm`/`y_mm` stay (simulator selection uses
them), so the retirement needs to decide whether callers of `resolve()` lose the anatomical fields or
whether those become app-side lookups.

**Scope guardrail — device-state operations only, never therapeutic targeting.** Every use case
above is service, fault isolation, or diagnostics. **No clinical protocol may ever target a
cluster**, because a cluster is not an anatomical object: a protocol naming one would silently
change meaning when MECH-2 revisits the clamp shape — the exact hazard this section rejects for
addressing, re-entering through a side door.

**This guardrail needs no new enforcement — the existing boundary already provides it.**
`NP_GROUP_KIND_*` is firmware-internal and appears nowhere in `app/`: the app cannot name a group
kind at all. It emits a `NP_PROTO_TARGET_SOCKET_MASK` bitmap, and firmware builds a
`NP_GROUP_KIND_SOCKET_SET` query from it via `np_protocol_socket_expand()`. So a protocol
structurally cannot express a cluster target today, and adding this enum value does not change that.
It is recorded here so that no one later "helpfully" adds a cluster selector to the NPPS language or
the hub compiler.

**Minimum viable form.** If the type-filtered diagnostic case (PD self-test per cluster) turns out
not to be needed, the simpler `np_module_map_cluster_sockets(cluster_id, out, max, count_out)`
enumerator covers service and fault isolation on its own. The group kind subsumes it; start with
whichever the cluster-controller bring-up actually calls for rather than building both.

**What is not worth adding:** a `NP_PROTO_TARGET_CLUSTER_MASK` target kind in NP Hub Protocol v2.
The existing 16-byte socket bitmap already expresses any cluster; a cluster target kind would save
12 bytes on the wire and create a second way to say the same thing — precisely what §10 argues
against on dedup grounds. Expand cluster → sockets in the app compiler instead.

---

## 5. I2C fan-out architecture

Three tiers. Address collision (every T1-C ATtiny402 is factory-fixed at 0x30, not field-configurable
— Appendix A §4.1) is resolved at tier 2, one hop from the module, instead of being propagated to
the hub.

### 5.1 Tier 0 — hub to inner bowl (differential)

One RT1062 LPI2C instance → an I2C-to-differential transceiver pair (**PCA9615-class**, one at each
end) → the posterior blind-mate boss → the inner-bowl backbone.

**Why differential rather than plain I2C across the harness.** Three reasons, in order of weight:

1. **EEG aggression.** The bus runs across the inner bowl, millimetres from Ag/AgCl electrodes and
   their µV-level FPC leads. `NP-HEX-ZM-001` §5.3.1 point 3 rejects putting the Helmholtz coil on the
   inner bowl for exactly this reason — "drive current and switching harmonics … straight into the
   recording it exists to protect." A single-ended 400 kHz open-drain bus with uncontrolled edges
   would be the same mistake in a different component. Differential with controlled slew keeps the
   aggressor common-mode.
2. **Bus capacitance.** ~1 m of harness at ~50 pF/m plus 16 peers at ~10 pF each is ~170 pF against
   I2C's 400 pF ceiling — passable but with little margin once connector and FPC stubs are counted.
   The transceiver isolates the harness capacitance from the local segments entirely.
3. **Fault containment.** A shorted or wet harness pin cannot latch the hub's local I2C bus.

**Additional firmware mitigation, required not optional:** a **bus-quiet window** synchronised to EEG
conversion. The ADS1299 runs at 500 Hz (CLAUDE.md §3 modality 3); cluster-bus traffic is scheduled in
the inter-conversion gaps. This is specified as a firmware requirement in §9.4 and verified by
**HUB-DRC-C14**.

Also on tier 0: a shared open-drain **ATTN** line (any controller pulls low to request service —
hot-plug detected, fault latched) so the hub is not obliged to poll 16 peers at idle.

### 5.2 Tier 1 — the cluster bus, and Tier 2 — within a cluster

**Tier 1** carries up to 16 cluster controllers on one segment, each with a **unique** I2C slave
address strapped by resistors on the inner-bowl FPC (position-determined, so a controller board is a
single part number and its identity comes from where it is fitted — the same "socket = position,
module = type" split `NP-HEX-ZM-001` §4a uses for tiles). **No mux is required at this tier at all**:
distinct addresses, one bus.

**Tier 2** is where collision is actually resolved. Each cluster controller is the I2C **master** for
its ≤8 sockets, isolating them with **one PCA9548A** (8-channel — exact fit). All eight modules may
be 0x30; only one channel is enabled at a time.

**Total I2C mux silicon: `ceil(n/8)` parts — 10 at n = 80, 16 maximum.** Against the ~27 of a
hub-centric 3-deep tree, with one channel-select write per transaction instead of three, and mux
state that is local and independently recoverable rather than global.

Module transactions from the hub are **tunnelled**: the hub writes a relay request (target channel,
register, payload) to the cluster controller, which selects the PCA9548A channel and executes it.
This keeps the hub-side HAL socket-indexed (§9.2) so callers above the HAL are unaffected by the
topology.

### 5.3 Pull-ups

Per-cluster-controller, on the inner bowl: 2 × 4.7 kΩ on the local master segment, and 2 × 4.7 kΩ per
enabled PCA9548A downstream channel is **not** required — the PCA9548A passes the master-side pull-up
to the enabled channel, so one pull-up pair per controller suffices, with per-channel pull-ups added
only if a socket FPC stub proves too capacitive at bench (**OI-HUB-C04**). Hub side: one pair on the
LPI2C segment to the transceiver. This is a genuine simplification over Rev B's 12 discrete pull-ups
for 5 slots, which scaled to ~162 at 80.

---

## 6. PD analog front end and TIA gain switching

### 6.1 The gain-switching answer in one line

**There is no `GAIN_SEL` line from the i.MX RT1062 at all, and no shift register or I2C GPIO expander
either.** The gain control pin is a GPIO on the cluster-controller MCU — the same MCU that steps the
analog mux — so gain becomes a per-*sample* property set inside the scan loop, not a per-socket
latched state held on the hub.

### 6.2 Muxed PD current into one shared TIA

Each cluster controller has **one** TIA. The 16 PD channels (8 sockets × PD1/PD2) are multiplexed
into it as **currents**, ahead of the amplifier, at the TIA's virtual-ground summing node.

Multiplexing photodiode current pre-TIA is standard for PD arrays and is well inside tolerance here:

| Concern | Magnitude | Effect on a 27–72 µA signal |
|---|---|---|
| Mux on-resistance in series | ≤ 4 Ω into a virtual ground | Negligible (current-mode; adds a slight noise-gain term only) |
| Off-channel leakage | ≤ ±5 nA at 85 °C (spec floor) | ≤ 0.019 % |
| Charge injection at switch | ≤ 5 pC | Settles in µs; absorbed by the settling budget below |
| Unselected PD capacitance | Disconnected | **Improves** TIA stability vs. a bussed node — the amplifier sees one PD's junction capacitance, not sixteen |

Part requirement: 16:1 low-leakage CMOS mux, single 3.3 V supply, Ron ≤ 4 Ω, off-leakage ≤ ±5 nA at
85 °C. **Baseline: 2 × TI TMUX1308-class 8:1** (~$0.55 each, dual-sourceable, ±1 nA typical).
**Single-package alternative: ADI ADG706.** Selection remains open pending the leakage and
charge-injection bench (**OI-HUB-C02**).

### 6.3 Gain switching — DG2788A retained, one per cluster

Rev B's switching element and both resistor values are kept verbatim (§3.4). Only the count and the
control source change.

| | Rev B | Rev C |
|---|---|---|
| DG2788A count | 1 per slot (5) | 1 per **cluster** (10 at n = 80) |
| Sections used | A = PD1 gain, B = PD2 gain | A = shared-TIA gain; **B reserved** (§6.5) |
| Control source | `GAIN_SEL[0..4]`, RT1062 GPIO | Cluster-MCU GPIO — **no hub signal** |
| Granularity | Latched per slot, on module-type detect | Set per channel, inside the scan loop |
| Rf values | 47 kΩ / 22.1 kΩ, 1 % | **unchanged** — 47 kΩ / 22.1 kΩ, 0.1 % (§6.4) |

DG2788A propagation is < 1 µs against a ~20 µs per-channel settling budget, so per-sample switching
costs nothing. Gain for each channel is looked up from the module type the cluster controller learned
at inventory (UID-based, `np_module_map`) — **not** from the retired ZONE_ID resistor ladder.

This also removes a coherency hazard Rev B carried structurally: a latched per-slot gain can be stale
relative to the module actually present after a hot-swap. A per-sample gain derived from live
inventory cannot be.

### 6.4 Consolidation buys precision — the PD1/PD2 ratio gets strictly better

Sharing the TIA is not only cheaper. It removes the dominant error term from the measurement the
whole fouling-vs-aging discrimination rests on.

`NP-FW-PBM1064-001` §6.3 distinguishes PDMS fouling from LED aging by the **ratio** PD1/PD2, with
thresholds at ×0.80 and ±15 %. In Rev B, PD1 and PD2 went through two physically different TIAs, so
Rev B had to require "matched ≤ 0.5 % between PD1 and PD2 TIA channels on the same slot to preserve
PD1/PD2 ratio accuracy" (Appendix A §3.2) — a matching burden on 160 channels, and a residual error
directly in the discriminant.

In Rev C, PD1 and PD2 of a socket pass through **the same mux die, the same Rf, the same op-amp, the
same ADC**. Gain error, Rf tolerance, op-amp offset and ADC gain error are **common-mode in the ratio
and cancel exactly**. The per-slot matching requirement disappears; the surviving requirement is only
that Rf be stable between the two samples, microseconds apart.

Two further gains fall out of having 10 amplifiers instead of 160:

- **Spend more per amplifier.** A zero-drift CMOS part (MCP6V51 / OPA378 class, ~$1.00, offset
  ≤ 200 µV) costs $10 across the product where 160 mediocre channels at $0.20 cost $32. Offset error
  at Rf = 22 kΩ, 27 µA drops from ~0.17 % (1 mV part) to ~0.03 %.
- **Spend more per sample.** See §6.5.

### 6.5 Scan timing, averaging, and the one genuine regression

The regression to be honest about: PD1 and PD2 of a socket are now sampled **sequentially**, not
simultaneously. Under pulsed PBM (2–40 Hz, ≤25 % duty) two samples landing in different parts of a
pulse would corrupt the ratio outright. This is handled by budget, and the budget is comfortable:

| Item | Value |
|---|---|
| TIA settling (Rf = 47 kΩ, Cf ≈ 10 pF → τ ≈ 470 ns) + mux settling + ADC acquisition | ≤ 20 µs per sample |
| Samples averaged per channel per dose tick | **8** |
| Full cluster scan, 16 channels × 8 averages | **≈ 2.56 ms** |
| PD1→PD2 skew for one socket (adjacent in scan order) | **≈ 20–40 µs** |
| Shortest LED ON window to be supported (40 Hz at 15 % duty) | **3.75 ms** |
| Dose tick period | 100 ms |

**Requirement HUB-REQ-C01:** the full-cluster PD scan executes **inside a single LED ON window**, so
every channel in a cluster is sampled at the same point in the pulse and PD1/PD2 skew is ≈ 20–40 µs
— three orders of magnitude below the 25 ms period at 40 Hz. At 2.56 ms against a 3.75 ms worst-case
window there is 1.2 ms of margin; if a future protocol needs a shorter ON window, averaging is reduced
before the requirement is relaxed.

**Phase knowledge comes free for T1-A/T1-B**, because the cluster controller generates its own
cluster's PWM (§3.2) and therefore knows the pulse phase exactly — a hub-centric design would have
needed a phase-distribution signal. **For T1-C smart modules the on-module ATtiny402 owns the PWM**,
so the cluster controller does not know the phase; the driver register map already reserves
`CONFIG` bit[2] `sync_enable` (Appendix A cross-ref / `NP-FW-PBM1064-001` §5.1) for this, but the
sync signal's routing from module to cluster controller is **not yet defined** — **OI-HUB-C05**,
blocking for T1-C dose accuracy.

Total per-tick analog time is 2.56 ms of a 100 ms budget, leaving room to raise averaging if the
ratio-precision bench (**FAI-SM-07/08**) asks for it. DG2788A section B is held **reserved** for a
possible third gain step; adding one now would be an unvalidated third calibration state against a
saturation analysis that has only ever been validated at two, so the conservative default is to
leave the two proven values and keep the section spare.

---

## 7. Safety architecture

### 7.1 A hard wire-format finding: the enable bitmask cannot hold per-cluster bits

The safety enable mask is **16 bits** — `enable_lo` + `enable_hi` in `np_safety_rx_frame_t`
(`firmware/safety_mcu/include/np_safety_protocol.h`) — with 14 bits allocated
(`NP_SAFETY_EN_ALL_MASK = 0x3FFF`) and **2 spare**.

`NP-HEX-ZM-001` §7 OI-HUB-SOCKET-01 asks for "per-socket safety-MCU enable (today
`NP_SAFETY_EN_PBM_ZONE_0..4` is per-zone-slot)". Neither per-socket nor per-cluster fits:

- per-socket: 128 bits needed, 16 available;
- per-cluster: 16 bits needed for clusters alone, plus the 9 surviving non-PBM modality bits = 25 > 16.

Widening the frame is possible but touches the Class C safety wire format, its checksum, and both
sides of the SPI heartbeat — a disproportionate change to buy selectivity the interlock does not need.

### 7.2 Resolution — one `NP_SAFETY_EN_PBM_CRANIAL` bit, distributed gating

Bits 0–4 (`NP_SAFETY_EN_PBM_ZONE_0..4`) are replaced by a **single** `NP_SAFETY_EN_PBM_CRANIAL` bit.
Bits 1–4 become **reserved, not reused** (enable-bit positions appear in SHDR fault records; silently
recycling a position would make historical logs misread).

The argument that this is sufficient, not merely convenient:

- **The safety MCU's function is the interlock — cut stimulation — not dose selectivity.** Cutting
  all cranial PBM is always a safe response; over-cutting is a usability cost, never a hazard. The
  five per-zone bits in Rev B were an artifact of there happening to be five slots and spare bits, not
  a derived safety requirement.
- **Per-socket thermal response does not go through the safety MCU anyway.** CLAUDE.md §4.2 puts the
  per-tile 62 °C junction limit on a *hardware current throttle* driven by the per-zone NTC, and the
  42 °C surface limit on the safety MCU as an interlock. Per-socket dose-limit shutdown is a Class B
  action performed by writing duty = 0 (`NP-FW-PBM1064-001` §6.5), not by dropping a safety GPIO.
- **Physical gating still distributes.** The single logical enable is buffered to **one gate
  transistor per cluster**, on each cluster's LED drive rail. Per-cluster gates give fault isolation
  (a shorted cluster cannot drag the shared rail) and drive fan-out — without per-cluster *policy*
  bits.

**Accepted consequence, stated explicitly for safety review:** a safety-layer cut is all-or-nothing
across the cranial lattice; a single socket cannot be safety-cut without its cluster or, in policy
terms, without the whole lattice. **OI-HUB-C07** routes this to safety review for confirmation.

### 7.3 The Class B / Class C boundary is preserved

The cluster controllers are **IEC 62304 Class B** (SW-02, main-processor class), *conditional on* one
requirement:

**HUB-REQ-C02:** each cluster's LED drive rail is gated by the safety MCU's enable, **upstream** of
the cluster controller. A cluster-controller firmware fault, hang, or runaway therefore cannot
energise an LED. This preserves CLAUDE.md §4.2's dual-processor isolation claim ("safety MCU
physically owns all stimulation enable GPIO — app crash cannot cause unsafe stimulation") across the
new tier. Without it the cluster controllers would be Class C, and a ~$1.50 MCU × 16 would inherit the
safety MCU's certification burden.

`fault_slot` in `np_safety_tx_frame_t` is a `uint8_t` with 0xFF as "none", so it accommodates socket
ids 0–127 unchanged — no wire change needed there.

---

## 8. BOM

> **⚠ EVERY FIGURE IN §8 IS VOID — do not quote.** `NP-HW-HEXTILE-001` Rev A (**D-3**, **D-4**,
> **D-6**, **D-7**) moved the constant-current driver *and* the TIA + ADC on-module, put the socket on
> a 24 V DC bus, and removed the I2C address collision. That deletes the TIA op-amp, the DG2788A, the
> 16:1 PD mux, the 8:1 NTC mux, and the cluster MCU's role as I2C master from the bill below — i.e.
> most of the $6.34 board. The surviving cluster-tier function is the **electrode analog crosspoint**
> of **OI-HUB-C16**, which is not costed anywhere yet. §8 is retained unedited as the derivation of
> record for what the pre-HEXTILE architecture would have cost, and because §8.5's interconnect
> comparison (~100 vs ~880 conductors) stands on its own. Recosting is part of **OI-HUB-C15**.
>
> Note this does **not** disturb the cluster-shape decision: CLUSTER-1 (§4.4) rests on clamp-plate
> mechanics, not on any number below.

### 8.1 Per cluster controller (excludes LED drive stage — §1, out of scope)

| Component | Qty | Unit | Ext |
|---|---|---|---|
| STM32G071 (UFQFPN32) cluster MCU — §8.3 | 1 | $1.50 | $1.50 |
| PCA9548A 8-ch I2C mux (TSSOP-24) | 1 | $0.60 | $0.60 |
| 16:1 PD current mux (2 × TMUX1308-class) | 2 | $0.55 | $1.10 |
| 8:1 NTC mux | 1 | $0.35 | $0.35 |
| Zero-drift TIA op-amp (MCP6V51 / OPA378 class) | 1 | $0.60 | $0.60 |
| Vishay DG2788A gain switch (SOT-23-8) | 1 | $0.20 | $0.20 |
| Rf 47 kΩ + 22.1 kΩ, 0.1 %, ≤ 25 ppm/°C, 0402 | 2 | $0.02 | $0.04 |
| Pull-ups, address straps, decoupling, ESD | ~22 | — | $0.15 |
| Cluster PCB (small 4-layer + rigid-flex tail) | 1 | $1.80 | $1.80 |
| **Per-cluster subtotal** | | | **$6.34** |

### 8.2 Scaling, and the hub PCB delta

Cluster tier cost = **$6.34 × ceil(n_sockets / sockets_per_cluster)**. Board cost is flat regardless
of how many of its 8 channels are populated (§4.1), so the driver is board *count*:

| n_sockets | At 8/cluster | | At 7/cluster (**recommended flower**, §4.4) | |
|---|---|---|---|---|
| | Boards | Cost | Boards | Cost |
| 30 | 4 | $25.36 | 5 | $31.70 |
| 42 | 6 | $38.04 | 6 | $38.04 |
| 64 | 8 | $50.72 | 10 | $63.40 |
| **80 (v1 provisional)** | **10** | **$63.40** | **12** | **$76.08** |
| 128 (ceiling) | 16 | $101.44 | 19 | $120.46 |

The 3-hex triad is omitted: 27 boards / $171.18 at n = 80, and 43 boards at n = 128 exceeds the
32-controller tier-1 address strap (§4.4).

Hub PCB itself barely moves:

| Hub PCB Rev C delta vs Rev B | Qty | | |
|---|---|---|---|
| PCA9615-class I2C↔differential transceiver (hub end + inner-bowl end) | 2 | $1.10 | +$2.20 |
| Cluster-bus termination, ESD, pull-ups | — | — | +$0.30 |
| **Removed:** 5 × DG2788A, 1 × PCA9546A, 12 pull-ups, 20 Rf resistors (Appendix A §7) | — | — | −$1.41 |
| **Net hub PCB delta** | | | **≈ +$1.09** |

### 8.3 Why STM32G071 and not a $0.60 part

The STM32G071 is **already in the program** — it is the safety MCU (CLAUDE.md §4.1), selected over the
G031 because the G031's 8 KB SRAM was insufficient. Reusing it here gives one MCU family across two
tiers: one toolchain, one BSP, one set of errata, one supply-risk pool, and — the part that actually
matters — one IEC 62304 software-unit qualification lineage. Its 16-channel 12-bit ADC, two I2C
peripherals (slave to the hub, master to the PCA9548A, no software I2C needed), and timer complement
are comfortable rather than marginal for this workload.

A cheaper AVR-DB or ATtiny1627 would save roughly $0.70 × 10 = **$7 per unit at n = 80**. That is
rejected: $7 against a $405 Home Standard BOM does not buy a second toolchain, a second qualification
package, and a second supplier relationship on a device pursuing 510(k). Part selection is
nonetheless recorded as open (**OI-HUB-C03**) — if the cluster-controller firmware turns out to fit
comfortably in 8 KB, the G031 at ~$1.00 is a legitimate in-family downgrade with none of the
above costs.

### 8.4 Honest comparison, and the cost that must be netted out

Scaling Rev B naively to 80 sockets costs roughly: 80 × DG2788A ($16) + ~160 TIA channels in quads
(~$22) + 320 precision resistors ($3.20) + ~27 I2C muxes ($13.50) + hub-side analog fan-in muxes
(~$12) ≈ **$67** — *comparable to Rev C's $63.40*, while also requiring ~880 conductors through the
parting plane. **The cluster tier is not more expensive; it is the same money spent where it
works.**

**What this document cannot net out, and does not pretend to:** $63.40 is +16 % against the $405
Home Standard BOM, which is material. But it is not a pure addition — it partly *replaces* the
retired 5-zone-module drive electronics already inside that $405, and the hex redesign changes the
module BOM wholesale in ways `NP-HEX-ZM-001` has not yet costed. The net is a BOM-owner calculation
against a post-hex module BOM that does not exist yet. **Flagged as OI-HUB-C08**, not estimated here.

### 8.5 Interconnect — the headline number

| | Conductors at the parting plane |
|---|---|
| Rev B architecture scaled to 80 sockets | ~880 |
| **Rev C** (≈10 per cluster: 4 differential I2C, ATTN, PBM_CRANIAL_EN, VCC, GND, 2 × LED rail) × 10 clusters | **~100** |

An **8.8× reduction**, through the existing posterior blind-mate boss rather than a new one. LED power
distributes as a shared higher-voltage rail (e.g. 12 V) regulated locally per cluster rather than as
per-socket drive — consistent with the ~45–50 W T1 peak in CLAUDE.md §4.5, and part of the deferred
LED drive spec (§1, **OI-HUB-C01**).

---

## 9. Firmware impact

### 9.1 Retired outright

| Symbol | Disposition |
|---|---|
| `np_pbm1064_hal_tia_gain_set(slot, gain)` | **Retired from the hub HAL.** Gain is no longer a hub concern (§6.1). Equivalent logic moves into cluster-controller firmware. |
| `np_pbm1064_hal_tia_gain_boot_init()` | **Retired.** Replaced by a cluster-controller power-on self-test result reported over the bus; the hub asserts nothing. |
| `np_pbm1064_hal_i2c_mux_enable(slot, bool)` | **Retired.** Each cluster controller owns its own PCA9548A. |
| `np_pbm1064_hal_adc_read_zone_id(slot, …)`, `NP_PBM1064_ADC_SMART_MAX/_BASE_MIN/_NO_MODULE_MIN`, `np_slot_type_t`, `np_pbm1064_detect.c` §4 detection state machine | **Retired.** ZONE_ID resistor-ladder detection is replaced by UID-based auto-inventory (`np_module_map`), per `NP-FW-PBM1064-001`'s own supersession note. |
| `np_sm_slot_ctx_t.tia_gain` | **Retired** as hub state. Gain actually used is reported per-frame by the cluster controller for SHDR (§9.5). |
| `NP_SAFETY_EN_PBM_ZONE_0..4` | Replaced by `NP_SAFETY_EN_PBM_CRANIAL`; bits 1–4 reserved (§7.2). Touches `np_safety_protocol.h`, `np_hub_config.h`, `np_session_runner.c:68-72`, `np_mod_pbm.c:137`. |

### 9.2 Reshaped — hub-side signatures stay socket-indexed

Callers above the HAL should not learn the topology. Every `uint8_t slot` (0–4) becomes
`uint16_t socket_id` (0–127); the HAL resolves `socket_id → (cluster_id, channel)` via the §4.2
table.

| Function | Change |
|---|---|
| `np_pbm1064_hal_i2c_write/_read(socket_id, reg, …)` | Same signature shape; implemented as a **tunnelled** relay through the socket's cluster controller (§5.2). |
| `np_pbm1064_hal_i2c_probe(socket_id, addr, timeout)` | As above. |
| `np_pbm1064_hal_ntc_read(socket_id, …)` | Served from the cached cluster frame, not a direct ADC read. |
| `np_pbm1064_hal_safety_mcu_enable(…)` | Argument becomes the cranial-PBM enable, not a slot (§7.2). |

### 9.3 New — frame-oriented acquisition

The 10 Hz dose tick must **not** become 160 individual bus reads. New HAL primitive:

```c
/* One transaction per cluster per dose tick: all 8 sockets' PD1/PD2/NTC/status. */
np_hub_status_t np_hub_cluster_read_frame(uint8_t cluster_id,
                                          np_cluster_frame_t *out);
```

At n = 80 that is **10 frames per 100 ms tick** rather than 160 reads.
`np_pbm1064_hal_adc_read_pd(socket_id, pd_ch, counts)` is retained as a thin accessor over the cached
frame so `np_pbm1064_dose.c` needs no restructuring.

The frame also carries, per socket: module-present, UID-changed flag, fault latch, and **the TIA gain
actually used for each sample** (§9.5).

### 9.4 Scheduling requirement

**HUB-REQ-C03:** cluster-bus transactions are scheduled in the ADS1299 inter-conversion gaps
(500 Hz → 2 ms period). The dose tick's 10 frames and the 5 s status polls both fit; verified by
**HUB-DRC-C14**.

### 9.5 Calibration coefficients must be keyed to module UID, not socket — a real defect this exposes

`NP-FW-PBM1064-001` §6.2/§6.6 stores `K_PD1[zone][wl]`, `K_PD2[zone][wl]`, `K_ratio_nom[zone][wl]`
indexed by **zone slot**. That was correct when slots were fixed positions holding permanently
assigned modules. **Under hex tiling it is wrong**, and re-indexing it by socket — the obvious
migration, and the one that document's supersession note proposes ("needs re-indexing by socket") —
**would preserve the defect.**

These coefficients characterise a module's LEDs and photodiodes, measured on that physical module at
manufacture (§6.6: "per zone module, at manufacture, before the module ships"). Modules are universal
and swappable — that is the entire point of `NP-HEX-ZM-001`. Move a module to a different socket and
a socket-indexed table silently applies the wrong module's calibration, degrading the real-time
J/cm² dose claim that is the stated differentiator over Vielight (CLAUDE.md §3 modality 1) — and
doing so invisibly, because `cal_source` would still read `NP_CAL_FACTORY`.

**Recommended resolution:** key the coefficients to **module UID** and store them in the existing
`np_module_map` NVRAM record, which is already UID-keyed and already has the UID-change detection
machinery (`np_module_map_apply_poll`) to invalidate a stale entry. Record grows by 9 floats
(36 bytes): `NP_HEXMAP_REC_BYTES` 139 → 175, blob 17.8 KiB → 22.4 KiB, still ~0.13 % of the 16 MiB
Config partition. T1-A dumb tiles cannot hold their own coefficients, so a hub-side UID-keyed cache
is the only option that works for every tile type.

Raised as **OI-HUB-C06** against `NP-FW-PBM1064-001` Rev C and `NP-HEX-ZM-001`. It is out of this
document's scope to fix, but it is in scope to prevent the re-indexing from being done wrongly.

### 9.6 New firmware unit

`firmware/cluster_ctrl/` — STM32G071 bare-metal or minimal-RTOS, **IEC 62304 Class B** conditional on
HUB-REQ-C02 (§7.3). Owns: PD scan + per-sample gain, NTC scan, PWM generation for T1-A/T1-B, PCA9548A
channel control, per-socket UID/inventory poll (implementing `NP-HEX-ZM-001`'s **OI-HEXMAP-02**
`inventory_fn` seam), and the tier-1 slave register interface.

---

## 10. Consistency requirements on the two SUPERSEDED companion specs

Both are being redesigned in parallel. Rev C's requirements on them:

**`NP-SES-1064-001` (session descriptor, `zone[5]` / `smart_module_mask` retired).** The replacement
must **reuse the existing NP Hub Protocol v2 `NP_PROTO_TARGET_SOCKET_MASK` primitive** — the 16-byte,
128-bit, LSB-first, 0-based socket bitmap already defined in `firmware/hub_control/include/np_hub_config.h`
and already sized to `NP_HEXMAP_MAX_SOCKETS` — rather than inventing a third socket representation.
`NP-HEX-ZM-001` §4b's argument for a bitmap over an address list applies verbatim here: under
inclusive midline membership, a list carries duplicates to the driver and duplicates mean double
J/cm² on the same module. Rev C's hub expands a socket bitmap to per-cluster frames; any
socket-indexed representation works, but a third one would be a third place for the dedup guarantee
to be forgotten. Per-socket field semantics (current setpoints, PWM frequency, duty, channel mask)
carry over unchanged.

**`NP-FW-PBM1064-001` (5-slot LPI2C3 addressing).** "Hub enables LPI2C3 per slot independently
(5 separate buses)" is replaced by §5's three-tier scheme: the hub has **one** cluster bus and no
per-socket I2C peripheral. The three-channel driver register map (0x00–0x0D) and the
`DUTY_MAX_REG = 0x32` 25 % ceiling are unaffected — both are addressing-independent. §9.5 above is a
binding constraint on how its calibration coefficients are re-indexed.

---

## 11. Open items

| ID | Description | Blocking |
|----|-------------|----------|
| OI-HUB-C01 | **Per-socket FPC pinout + LED drive stage** for the hex tile — unwritten, not merely superseded (`NP-HW-FPC-001` Rev E note). §3.2 establishes it must sit on the cluster tier; the topology, current regulation and connector are unspecified. | Cluster-board design; inner-bowl tooling |
| OI-HUB-C02 | PD current-mux part selection bench: off-leakage at 85 °C, charge injection, settling into the shared TIA (§6.2) | Cluster-board BOM |
| OI-HUB-C03 | Cluster MCU final selection — confirm STM32G071 vs in-family G031 downgrade once cluster firmware SRAM footprint is known (§8.3) | Cluster-board BOM |
| OI-HUB-C04 | Confirm one pull-up pair per cluster controller suffices, or add per-PCA9548A-channel pull-ups if socket FPC stub capacitance requires (§5.3) | Cluster-board layout |
| OI-HUB-C05 | **T1-C PWM phase sync routing** from on-module ATtiny402 (`CONFIG` bit[2] `sync_enable`) to the cluster controller, so HUB-REQ-C01's in-ON-window scan holds for smart modules (§6.5) | T1-C dose accuracy; FAI-SM-06 |
| OI-HUB-C06 | **Calibration coefficients re-keyed to module UID, not socket** (§9.5) — against `NP-FW-PBM1064-001` Rev C and `NP-HEX-ZM-001` | Dose-metering accuracy claim |
| OI-HUB-C07 | Safety review to confirm all-or-nothing `NP_SAFETY_EN_PBM_CRANIAL` granularity is acceptable, vs. widening the Class C safety wire format (§7.1–7.2) | Safety wire format; OI-HUB-SOCKET-01 |
| OI-HUB-C08 | **Net the $63.40 cluster tier against the retired 5-zone-module drive electronics** already inside the $405 Home Standard BOM (§8.4) — needs a post-hex module BOM that does not yet exist | BOM sign-off |
| OI-HUB-C09 | **CLOSED 2026-07-30.** Electrical and mechanical clusters are **the same thing**: the board is **capacity-8**, not exactly-8, and capacity 8 costs the same as a hypothetical 7 (no 7-channel I2C switch or 14:1 mux exists), so one board SKU serves a full flower or any partial one. Shape settled by **CLUSTER-1** (principal, 2026-07-30): **7-hex flower wherever the lattice allows, partial flowers at the boundary** — decided on clamp-plate mechanics (span 122.2 vs 161.8 mm; stress ×1.75, plate deflection ×3.07, dome depth 25.1 → 55.0 mm, 136.8° subtended), *not* on the earlier BOM gradient, whose figures HEXTILE has voided. The triad stays excluded electrically too (43 segments at n=128 > the 32-segment budget). MECH-2 now verifies rather than selects. **Three-level `(cluster:module:element)` addressing remains explicitly rejected** (§4.5). | Closed — MECH-2 verifies |
| OI-HUB-C13 | Add `NP_GROUP_KIND_CLUSTER = 3` + `cluster_id` to `np_group_query_t`, resolving via the §4.2 table (single ascending pass, no `seen` bitmap needed). Legitimate as a firmware-resident group because socket→cluster changes only on an inner-bowl re-tool (§4.5.1) — unlike lobe. Covers clamp-release reporting, cluster-controller fault isolation, per-cluster diagnostics — **device-state operations only, never therapeutic targeting** (§4.5). Already unreachable from NPPS/the app by construction (`NP_GROUP_KIND_*` is firmware-internal; the app emits a socket bitmap), so no new gate is required — but **do not** add a cluster selector to NPPS or a `NP_PROTO_TARGET_CLUSTER_MASK` wire target. Consider the simpler `np_module_map_cluster_sockets()` enumerator instead if the type-filtered diagnostic case proves unnecessary | Service + fault-isolation UX |
| OI-HUB-C15 | **Merge this document with `NP-HW-HEXTILE-001` Rev A** per the reconciliation banner at the head of this file: delete §6 (TIA/gain switching — HEXTILE D-4 moves it on-module), rewrite §5.2 to HEXTILE's D-7 single-tier UID-addressed segmentation, drop the cluster-MCU LED-drive rationale in §3.2 (D-3/D-6: on-module driver, 24 V bus), and recost §8. Retain §2, §4.2–4.5.2, §7.2, §9.5, §10. **Until this lands the two documents assert incompatible architectures and HEXTILE is the more recent** | Rev C baselining — **blocking** |
| OI-HUB-C16 | **Route the T1-B electrode path at ~80 sockets.** HEXTILE reserves pins 13/14/15 (`ELEC_SIG`/`ELEC_SHLD`/`AGND`) at every socket, notes an electrode signal cannot be carried over I2C (µV to the ADS1299 *and* stimulation current from the tES driver), then leaves T1-B out of scope. 80 × 3 = **240 analog conductors to the hub** — the same unbuildable star HEXTILE used to kill hub-side LED drive and PD analog. Restricting T1-B to a socket subset is not available (re-imposes what SMART-1 removed). Proposed: a **per-cluster electrode crosspoint** onto N ADS1299/tES channels (8 T1, 21 T2) + `/ALERT`/`SEAT_N` aggregation — the surviving justification for the cluster tier. Open: switch matrix vs. MCU; contact-resistance and leakage budget on a µV path through a pogo contact and a crosspoint; tES current rating through the same switch | T1-B tile; EEG/tES at scale; cluster-tier scope |
| OI-HUB-C14 | **Retire the firmware lobe path** — `NP_GROUP_KIND_LOBE`, `np_pgroup_t`, `np_module_map_predefined()`, and the `lobe`/`side` fields of `np_socket_geom_t`. It is unbacked: no production caller, no production geometry table (test fixtures only), and no generator emitting firmware C (§4.5.2). Zones are data owned by `00-zones.npps` and already reach firmware as a socket bitmap → `NP_GROUP_KIND_SOCKET_SET`. Risk if left: a future caller resolves against a stale post-REG-1 lobe map — a wrong-site dose from dead code. Decide whether `np_physical_loc_t` keeps `lobe`/`side` (from `np_module_map_resolve()`) or those become app-side lookups; `x_mm`/`y_mm` stay for simulator selection. Touches several of the 63 host checks — own PR, own review | Firmware source-of-truth hygiene; REG-1 safety |
| OI-HUB-C10 | `scripts/sync-socket-map.ts` to emit the `socket_id → (cluster_id, channel)` table alongside existing artifacts so it cannot drift from the lattice (§4.2) | Generated artifacts |
| OI-HUB-C11 | Hub 3.3 V and cluster-rail current budget at 16 clusters (supersedes Rev B's OI-HUB-01, which sized 5 × 50 mA smart modules) | Pre-prototype |
| OI-HUB-C12 | Cluster-bus EMI qualification: confirm differential signalling + bus-quiet window keeps EEG noise floor within budget (§5.1) | EMF-1; EEG noise floor |

**Rev B's open items OI-HUB-01…05 are closed as moot** — all five concern the retired 5-slot hardware
(3.3 V budget for 5 modules, DG2788A-to-TIA trace length, Rev B Gerber release, `GPIO_B0_04..08`
IOMUX, the optional 500 mA LDO). OI-HUB-C11 is the surviving successor to OI-HUB-01.
**OI-PBM-HW-01 and OI-PBM-HW-02 remain closed** — the gain switch and I2C isolation are still
specified, now at §6 and §5 respectively.

---

## 12. Design review checklist

| Item | Description | Status |
|------|-------------|--------|
| HUB-DRC-C01 | Hub PCB contains **no** per-socket footprint, net, or `n_sockets`-derived constant (§4.3) | Open — Gerber review |
| HUB-DRC-C02 | Cluster count formula `ceil(n/8)` valid across 30–128 sockets; 16 × 8 = 128 = `NP_HEXMAP_MAX_SOCKETS` | ✓ (§4.1) |
| HUB-DRC-C03 | Socket numbering remains anatomical; `socket_id → (cluster,channel)` is a generated table | Open — OI-HUB-C10 |
| HUB-DRC-C04 | TIA saturation analysis still valid at Rf = 22 kΩ with InGaAs max current (1.58 V < 3.0 V swing) | ✓ (Appendix A §2, carried forward) |
| HUB-DRC-C05 | Rf = 47 kΩ / 22.1 kΩ, 0.1 %, ≤ 25 ppm/°C; **per-slot PD1/PD2 matching requirement removed** | ✓ (§6.4) — shared TIA makes it common-mode |
| HUB-DRC-C06 | Off-channel mux leakage ≤ 0.02 % of minimum PD signal at 85 °C | Open — OI-HUB-C02 bench |
| HUB-DRC-C07 | Full-cluster scan (16 ch × 8 avg ≈ 2.56 ms) fits the 3.75 ms worst-case ON window | ✓ by budget (§6.5) — confirm at bench |
| HUB-DRC-C08 | PD1/PD2 skew ≤ 50 µs for a given socket | ✓ by scan ordering (§6.5) |
| HUB-DRC-C09 | Zero `GAIN_SEL` nets between RT1062 and any socket | ✓ (§6.1) |
| HUB-DRC-C10 | PCA9548A + cluster-controller I2C address map non-conflicting with existing hub I2C peripherals | Open — hub I2C address audit |
| HUB-DRC-C11 | Each cluster's LED drive rail gated by safety-MCU enable **upstream** of the cluster MCU (HUB-REQ-C02) | Open — schematic review; gates Class B allocation |
| HUB-DRC-C12 | `NP_SAFETY_EN_PBM_ZONE_1..4` bit positions reserved, not reused | Open — `np_safety_protocol.h` / `np_hub_config.h` edit |
| HUB-DRC-C13 | Conductors at parting plane ≤ 120 at n = 80 | ✓ by count (§8.5) — confirm against harness CAD |
| HUB-DRC-C14 | Cluster-bus traffic confined to ADS1299 inter-conversion gaps; EEG noise floor unchanged | Open — OI-HUB-C12 bench |
| HUB-DRC-C15 | PD analog traces guarded and routed away from LED drive on the cluster board | Open — cluster-board layout DRC |

---

# Appendix A — Rev B (RETIRED 2026-07-28), archived

> **This appendix is history, retained per program convention. Do NOT use it for new design work.**
> It is the complete text of NP-HW-HUB-001 Rev B (BASELINED 2026-05-13), which sized the TIA
> gain-switch and I2C isolation hardware for exactly five zone slots. It was superseded on
> 2026-07-28 by SMART-1 (`NP-HEX-ZM-001` §4a/§7) and is replaced by §§1–12 above.
>
> **Why it is kept:** §2's TIA saturation analysis and §3.1–3.3's DG2788A switching topology are
> still the governing component-level engineering, carried forward unchanged into Rev C §6 (see
> Rev C §3.4). §7's BOM is the baseline the Rev C delta in §8.2 is measured against.
>
> **Rev B open items OI-HUB-01…05 are closed as moot** (Rev C §11). Its GPIO assignment (§3.4),
> slot-indexed I2C mux plan (§4.2), and 5-slot supply budget (§6) are retired outright.

## A.1 Scope *(Rev B §1)*

This document specifies the Rev B changes to the NeurOne hub PCB required to support the 1064nm smart zone module accessory (NP-FPC-ZM-SM-01). All changes are additive; the base hub PCB architecture is unchanged.

**Rev B adds:**
1. **Five Vishay DG2788A analog switches** — one per zone slot — switching the TIA feedback resistor Rf from 47 kΩ (base module / no module: silicon PD responsivity) to 22 kΩ (smart module: InGaAs PD, 2× higher responsivity). This is **OI-PBM-HW-01**, the blocking item for FAI-SM-04 and FAI-SM-06.
2. **NXP PCA9546A I2C bus switch** — 4-of-5 slot I2C isolation, preventing address collisions (all smart modules share 0x30). Fifth slot via direct LPI2C3 GPIO mux. This resolves OI-PBM-HW-02.
3. **Per-slot 4.7 kΩ I2C pull-up resistors** (10 resistors × 4.7 kΩ, 0402) on SDA and SCL lines for each smart-module-capable slot.
4. **3.3 V supply budget verification** for ≤5 simultaneous smart modules.

## A.2 Background — Why TIA Gain Switch Is Required *(Rev B §2 — CARRIED FORWARD, still governing)*

The hub PCB TIA front-end for PD1/PD2 dose-metering ADC channels was sized for the base module silicon photodiodes at 808 nm (responsivity ≈ 0.47 A/W). The smart module carries Hamamatsu G12180-010A InGaAs photodiodes with responsivity ≈ 0.90 A/W at 1064 nm — approximately **2× higher**.

**TIA saturation analysis (from NP-HW-FPC-001 Rev E §5.3):**

| Condition | PD current | Rf = 47 kΩ (Rev A) | Rf = 22 kΩ (Rev B) |
|-----------|-----------|---------------------|---------------------|
| Si PD1 at 8 mW/cm², 1 mm² | 37.6 µA | 1.77 V ✓ | 0.83 V ✓ |
| InGaAs PD1 at 8 mW/cm², 1 mm² | 72.0 µA | **3.38 V — saturates** | 1.58 V ✓ |
| InGaAs PD2 at 3 mW/cm², 1 mm² | 27.0 µA | 1.27 V ✓ | 0.59 V ✓ |

With Rf = 47 kΩ, the InGaAs PD1 output at typical irradiance levels exceeds the 3.3 V ADC rail. The gain switch is mandatory; no firmware workaround exists.

**Resolution:** Per-slot SPDT analog switch selects Rf = 47 kΩ (base module default) or Rf = 22 kΩ (smart module). Switch is asserted by hub firmware immediately after ZONE_ID debounce confirms a smart module, and **before** the I2C mux is enabled for that slot.

## A.3 TIA Gain Switch — Vishay DG2788A *(Rev B §3)*

### A.3.1 Component Selection *(§3.1 — CARRIED FORWARD)*

**Selected IC: Vishay DG2788A** (Vishay/Siliconix)

| Parameter | Value |
|-----------|-------|
| Configuration | Dual SPDT (2× single-pole double-throw) |
| Supply voltage | 2.7–5.5 V (single supply; 3.3 V rail) |
| RON (typical at 3.3 V) | ≤ 2.5 Ω |
| COFF | ≤ 5 pF |
| Control logic | TTL/CMOS compatible (VIH ≥ 2.0 V, VIL ≤ 0.8 V) |
| Package | SOT-23-8 or SC-74A |
| BOM cost | $0.15–0.25 per unit |
| BOM × 5 slots | **$0.75–1.25 total** |

One DG2788A per slot covers **both** PD channels (PD1 TIA and PD2 TIA) using the two independent SPDT sections (section A = PD1 gain, section B = PD2 gain). Both sections are driven by the same control signal (`GAIN_SEL[n]` GPIO from i.MX RT1062).

**Rationale for DG2788A over alternatives:**
- Single-supply operation at 3.3 V without negative rail
- RON ≤ 2.5 Ω negligible vs Rf values (47 kΩ, 22 kΩ)
- COFF ≤ 5 pF: no significant noise injection into TIA feedback at DC–100 Hz dose-metering bandwidth
- Dual SPDT in a single SOT-23-8 package covers both PD channels per slot with one component
- Vishay Siliconix preferred supplier already qualified for NeurOne analogue front-end

### A.3.2 TIA Feedback Switching Topology *(§3.2 — CARRIED FORWARD)*

Each zone slot has an independent TIA op-amp circuit for PD1 and PD2. The DG2788A switches the feedback resistor between two values.

**Topology (per TIA channel):**

```
                  +-------[Rf_A = 47 kΩ]---+
                  |       (DG2788A COM→NO)  |
PD_in ─ [Rbias] ─ ─ (−) ─ [TIA op-amp] ─── output ─→ LPADC1
                  |                         |
                  +-------[Rf_B = 22 kΩ]---+
                          (DG2788A COM→NC)
```

- **GAIN_SEL = LOW (default):** DG2788A selects COM→NO path (Rf_A = 47 kΩ). Used for base modules, absent slot, and during debounce before type is confirmed.
- **GAIN_SEL = HIGH (smart module):** DG2788A selects COM→NC path (Rf_B = 22 kΩ). Used after ZONE_ID debounce confirms ADC < 1100 counts.

The DG2788A COM pin is the op-amp feedback node. NO = 47 kΩ to op-amp output. NC = 22 kΩ to op-amp output.

**Resistor values:**

| State | Rf selected | Target | Achieved |
|-------|-------------|--------|---------|
| Base/absent (GAIN_SEL = LOW) | Rf_A | 47 kΩ | 47.0 kΩ (standard E24, 1%) |
| Smart module (GAIN_SEL = HIGH) | Rf_B | 22 kΩ | 22.1 kΩ (standard E24, 1%) |

Both resistors: 1%, 0402, low-temperature-coefficient (≤ 50 ppm/°C). Matched per slot to ≤ 0.5% between PD1 and PD2 TIA channels on the same slot to preserve PD1/PD2 ratio accuracy. *(Rev C note: this matching requirement is removed — the shared TIA makes the error common-mode. See Rev C §6.4.)*

### A.3.3 One DG2788A per Slot, Section Assignment *(§3.3)*

| DG2788A section | Signal | Switched Rf pair |
|-----------------|--------|-----------------|
| Section A | PD1 TIA (forward emission, behind PDMS) | 47 kΩ (LOW) ↔ 22 kΩ (HIGH) |
| Section B | PD2 TIA (scalp-facing backscatter) | 47 kΩ (LOW) ↔ 22 kΩ (HIGH) |

Both sections share the single `GAIN_SEL[n]` control line — PD1 and PD2 gain are always switched together, maintaining the PD1/PD2 ratio calibration validity.

### A.3.4 GPIO Assignment — GAIN_SEL[0..4] *(§3.4 — RETIRED)*

| GPIO name | i.MX RT1062 pin | Zone slot | Default state |
|-----------|----------------|-----------|---------------|
| GAIN_SEL_0 | GPIO_B0_04 | ZM-01 (slot 0) | LOW (47 kΩ) |
| GAIN_SEL_1 | GPIO_B0_05 | ZM-02 (slot 1) | LOW (47 kΩ) |
| GAIN_SEL_2 | GPIO_B0_06 | ZM-03 (slot 2) | LOW (47 kΩ) |
| GAIN_SEL_3 | GPIO_B0_07 | ZM-04 (slot 3) | LOW (47 kΩ) |
| GAIN_SEL_4 | GPIO_B0_08 | ZM-05 (slot 4) | LOW (47 kΩ) |

GPIO pins are i.MX RT1062 GPIO2 bank (GPIO_B0_xx). Configured as push-pull output, no pull resistor required (DG2788A input draws < 1 µA). Default LOW at power-on reset (3.3 V GPIO2 bank default state = LOW).

**Critical: GAIN_SEL must default LOW on power-on reset.** The i.MX RT1062 GPIO2 bank defaults to input mode (tri-state) on reset. Hub firmware boot sequence must configure GAIN_SEL[0..4] as outputs driven LOW before the zone detection task starts. See §A.5 for sequencing.

### A.3.5 TIA Op-Amp Selection Notes *(§3.5)*

The existing hub TIA op-amp must support stable operation across the full Rf range (22 kΩ to 47 kΩ) at the LPADC1 input bandwidth (≤ 100 Hz dose metering). The DG2788A RON ≤ 2.5 Ω adds negligible noise and offset at these values. No change to op-amp selection is required for Rev B; verify GBW and input bias current remain acceptable with Rf = 22 kΩ (verify output swing margin at maximum InGaAs PD current: 72 µA × 22 kΩ = 1.58 V < 3.0 V output swing limit for 3.3 V single-supply).

## A.4 I2C Bus Isolation — NXP PCA9546A (OI-PBM-HW-02) *(Rev B §4 — RETIRED)*

### A.4.1 Problem Statement *(§4.1 — the collision itself is still real; see Rev C §5)*

All smart zone modules implement I2C address 0x30 (factory-programmed into ATtiny402, not field-configurable). Up to five smart modules can be inserted simultaneously. A single shared I2C bus would result in address collisions: all five ATtiny402 ICs would respond simultaneously, corrupting bus arbitration.

### A.4.2 Solution — Per-Slot I2C Bus Isolation via PCA9546A *(§4.2 — RETIRED)*

**Selected IC: NXP PCA9546A** (4-channel I2C switch)

| Parameter | Value |
|-----------|-------|
| Channels | 4 (expandable; one PCA9546A covers slots 0–3) |
| Control | I2C register at address 0x70 (or 0x71–0x73 via A0/A1 pins) |
| Supply | 2.3–5.5 V; 3.3 V hub rail |
| Package | TSSOP-16 |
| BOM cost | ~$0.50 per IC |

**Slot assignment:**

| Slot | I2C isolation method | Notes |
|------|---------------------|-------|
| Slot 0 (ZM-01) | PCA9546A channel 0 | Switched by hub firmware via PCA9546A I2C at 0x70 |
| Slot 1 (ZM-02) | PCA9546A channel 1 | |
| Slot 2 (ZM-03) | PCA9546A channel 2 | |
| Slot 3 (ZM-04) | PCA9546A channel 3 | |
| Slot 4 (ZM-05) | Direct LPI2C3 GPIO mux (LPSPI_MUX GPIO) | i.MX RT1062 GPIO enables/disables pull-up pair for SDA/SCL independently |

Slot 4 uses direct GPIO control of the 4.7 kΩ pull-up enable (MOSFET switch on pull-up rail) to isolate it from the shared bus when not in use. This avoids requiring a second PCA9546A for a single remaining channel.

**Hub firmware accesses PCA9546A at 0x70 on the host-side LPI2C1 bus** (separate from the smart module slave buses). To address module at slot n: firmware writes channel-enable byte to PCA9546A (bits [3:0] = channel mask), then performs I2C transaction to 0x30 on the same bus behind the switch.

### A.4.3 Pull-Up Resistors *(§4.3 — RETIRED)*

Each smart-module SDA and SCL line (pins 10 and 11 of the 20-pin Hirose connector) requires 4.7 kΩ pull-ups to 3.3 V on the hub PCB.

| Placement | Count | Value | Total |
|-----------|-------|-------|-------|
| PCA9546A side (slots 0–3) | 2 per channel × 4 = 8 | 4.7 kΩ, 0402 | 8 resistors |
| GPIO mux side (slot 4) | 2 | 4.7 kΩ, 0402 | 2 resistors |
| Host side (hub LPI2C1 to PCA9546A) | 2 | 4.7 kΩ, 0402 | 2 resistors |
| **Total** | | | **12 × 4.7 kΩ, 0402** |

Pull-ups on the slave side (slots 0–4) must be located between the PCA9546A channel output (or GPIO mux output for slot 4) and the Hirose ZIF connector pin. This ensures only one channel's pull-ups are active at a time, controlled by the PCA9546A channel enable.

## A.5 ZONE_ID to Gain Switch Sequencing *(Rev B §5 — RETIRED with the ZONE_ID ladder)*

The sequence between ZONE_ID ADC detection and TIA gain switch assertion is **safety-critical**: if the TIA is still in high-gain mode (Rf = 47 kΩ) when the smart module InGaAs PD begins receiving light during I2C initialisation, the ADC output will be invalid (saturated). The firmware must follow the exact ordering below.

### A.5.1 Required Ordering (Smart Module Detection)

```
1. ADC reads ZONE_ID (pin 18) for the slot.
2. ZONE_ID debounce: 3× reads at 100 ms intervals, ≥ 2/3 must read < 1100 counts.
3. After debounce confirms smart module (ADC majority < 1100):
   a. Assert GAIN_SEL[n] = HIGH  →  DG2788A selects Rf = 22 kΩ.
      [minimum 10 µs setup; DG2788A switch propagation delay < 1 µs]
   b. Enable I2C mux for the slot (PCA9546A channel enable, or GPIO mux for slot 4).
   c. Probe I2C address 0x30 within 5 ms timeout.
4. On I2C probe success: proceed to session enable sequence (np_pbm1064_session_start).
```

**Rationale for ordering gain switch before I2C mux:** The ATtiny402 on the module powers up when VCC_3V3 (pin 12) is available. VCC_3V3 on the hub PCB is always live when the hub is powered; the InGaAs PD is therefore powered as soon as the module is physically connected. Although PBM LEDs are not enabled until after I2C session start, ambient light can produce non-trivial PD current. The TIA gain must be at Rf = 22 kΩ before any valid PD ADC readings are taken.

*(Rev C note: the hazard this sequencing prevents — sampling a saturated TIA — is eliminated structurally rather than by sequencing. Gain is set per sample from live UID inventory immediately before the conversion, so there is no window in which a stale gain can be applied. See Rev C §6.3.)*

### A.5.2 Required Ordering (Smart Module Removal)

```
1. ZONE_ID removal debounce: counts ≥ 4000 for 3 consecutive reads.
2. Disable I2C mux for the slot (PCA9546A channel disable, or GPIO mux deassert).
3. Deassert GAIN_SEL[n] = LOW  →  DG2788A returns to Rf = 47 kΩ (default).
4. Reset slot state to NP_SM_IDLE.
```

On removal, disable I2C first to prevent stale transactions if the module is being swapped for a base module. Then reset gain to HIGH to restore default state for a subsequent base module insertion.

### A.5.3 Power-On Boot Sequence Requirement

The i.MX RT1062 GPIO2 bank defaults to input (tri-state) on reset. The boot sequence must configure all GAIN_SEL[0..4] pins as GPIO outputs driven LOW **before** starting the zone detection task and before the first LPADC1 readings are taken. This ensures the hub PCB always starts in high-gain (47 kΩ) mode, which is safe for base modules and empty slots.

**Required boot sequence addition (main processor firmware):**
```c
/* Configure GAIN_SEL[0..4] as output LOW before zone detect task starts */
np_pbm1064_hal_tia_gain_boot_init();   /* drives GPIO_B0_04..08 LOW */
```

This function must execute before `np_pbm1064_detect_init()` is called. See `np_pbm1064_hal.h` (OI-PBM-HW-01).

## A.6 3.3 V Supply Current Budget (OI-PBM-HW-03) *(Rev B §6 — RETIRED; successor OI-HUB-C11)*

### A.6.1 Smart Module VCC_3V3 Load

Each smart module draws ≤ 50 mA on VCC_3V3 (pin 12): ATtiny402 quiescent + IRLML6344 gate drive. This is specified in NP-HW-FPC-001 Rev E §3.1.

**Maximum simultaneous smart module draw:**

| Scenario | 3.3 V load | Note |
|----------|-----------|------|
| 0 smart modules | 0 mA | |
| 1 smart module | 50 mA | |
| 5 smart modules (all slots) | **250 mA** | worst case |

### A.6.2 Existing 3.3 V Rail Capacity

Hub PCB 3.3 V rail is generated by an LDO or switching regulator supplying hub processor peripherals (i.MX RT1062 I/O, sensors, BT/Wi-Fi module 3.3 V). **Hub PCB designer must verify** that the 3.3 V regulator has ≥ 500 mA headroom above existing hub peripherals to accommodate 5× smart modules.

**Recommendation:** If existing regulator is ≤ 800 mA total budget, add a dedicated 500 mA LDO (e.g., TI TPS7A20 or equivalent, SOT-23-5, ≤ 200 mV dropout) on the smart module VCC_3V3 rail, powered from the hub 5 V supply. This isolates smart module transients from the hub processor 3.3 V rail. BOM delta: +$0.30–0.50 per hub PCB.

### A.6.3 DG2788A and PCA9546A Supply Current

| Component | Quiescent current | Active current (per unit) |
|-----------|-----------------|--------------------------|
| DG2788A × 5 | < 1 µA each | Negligible |
| PCA9546A × 1 | 10 µA typical | Negligible |

No significant contribution to 3.3 V budget from gain switch ICs.

## A.7 BOM Delta — Hub PCB Rev B *(Rev B §7 — baseline for the Rev C delta in §8.2)*

| Component | Qty | Unit cost | Total |
|-----------|-----|-----------|-------|
| Vishay DG2788A (SOT-23-8), dual SPDT gain switch | 5 | $0.15–0.25 | **$0.75–1.25** |
| NXP PCA9546A I2C switch (TSSOP-16) | 1 | $0.50 | **$0.50** |
| 4.7 kΩ, 0402, 1% pull-up resistors | 12 | $0.005 | **$0.06** |
| 47 kΩ, 0402, 1% (Rf_A, per TIA × 10) | 10 | $0.005 | **$0.05** |
| 22.1 kΩ, 0402, 1% (Rf_B, per TIA × 10) | 10 | $0.005 | **$0.05** |
| Optional: 500 mA LDO for smart module VCC_3V3 (if needed) | 1 | $0.30–0.50 | $0.30–0.50 |
| **Hub PCB Rev B total BOM delta** | | | **$1.41–1.91 (+ optional LDO)** |

## A.8 PCB Layout Notes *(Rev B §8 — retired as written; the principles carry to the cluster board, Rev C HUB-DRC-C15)*

1. **DG2788A placement:** Mount adjacent to the TIA op-amp for each slot. Keep switched feedback traces (Rf_A and Rf_B paths) as short as possible (< 5 mm) to minimise parasitic capacitance on the high-impedance feedback node.
2. **Rf_A / Rf_B resistors:** Place both resistors on the same PCB face as the DG2788A. Use 0402 with consistent copper pour treatment to minimise stray capacitance difference between the two paths.
3. **GAIN_SEL GPIO traces:** Route away from Rf traces and TIA input traces. 50 Ω controlled impedance is not required (low-frequency digital control, < 1 MHz transitions); however, keep traces < 20 mm to prevent coupling.
4. **PCA9546A placement:** Central hub PCB location; I2C bus segment from PCA9546A to each Hirose ZIF connector. Minimise bus segment length per channel to control capacitance (< 100 pF per segment recommended for 400 kHz fast-mode).
5. **Ground plane:** Ensure continuous ground plane beneath TIA and DG2788A circuits. No splits near feedback resistors.

## A.9 Firmware Implications *(Rev B §9 — superseded by Rev C §9)*

The firmware changes required by this hardware revision are specified in NP-FW-PBM1064-001 Rev A (amended by Issue #62):

1. **New HAL function:** `np_pbm1064_hal_tia_gain_set(slot, gain)` — asserts or deasserts `GAIN_SEL[n]` GPIO. Stub provided; platform team implements with actual GPIO_B0 register writes.
2. **New HAL function:** `np_pbm1064_hal_tia_gain_boot_init()` — configures all 5 GAIN_SEL pins as output LOW at boot. Called before zone detection task.
3. **Detection sequence change:** `np_pbm1064_detect.c` calls `np_pbm1064_hal_tia_gain_set(slot, NP_TIA_GAIN_LOW)` after smart module debounce confirms ZONE_ID < 1100, and **before** `np_pbm1064_hal_i2c_mux_enable(slot, true)`.
4. **Removal sequence change:** `np_pbm1064_detect.c` calls `np_pbm1064_hal_tia_gain_set(slot, NP_TIA_GAIN_HIGH)` after I2C mux disable on smart module removal.
5. **Per-slot gain state:** `np_sm_slot_ctx_t` tracks current TIA gain setting for SHDR logging and diagnostic purposes.

See `firmware/pbm_1064nm/` for implementation. FAI-SM-04 (three-channel bench verification) and FAI-SM-06 (InGaAs dose metering accuracy) require hardware Rev B PCB with DG2788A populated to pass.

## A.10 Open Items *(Rev B §10 — ALL CLOSED AS MOOT, Rev C §11)*

| ID | Description | Blocking | Rev C disposition |
|----|-------------|---------|---|
| OI-HUB-01 | Hub PCB designer to verify 3.3 V regulator budget ≥ 500 mA headroom above existing peripherals (§A.6.2). Add dedicated LDO if insufficient. | Pre-prototype | Moot — superseded by **OI-HUB-C11** (16-cluster budget) |
| OI-HUB-02 | Hub PCB layout DRC: Rf feedback trace length < 5 mm from DG2788A to TIA op-amp (§A.8). Sign-off required before Gerber release. | Pre-prototype | Moot — no DG2788A on hub PCB; principle carried to **HUB-DRC-C15** |
| OI-HUB-03 | Hub PCB Gerber release for Rev B (includes DG2788A footprint × 5, PCA9546A footprint, pull-up resistors, Rf pairs). | FAI-SM-04/06 bench build | Moot — Rev B Gerber never to be released |
| OI-HUB-04 | GPIO_B0_04..08 IOMUX configuration verified in i.MX RT1062 board support package (BSP) before bring-up. | Pre-prototype | Moot — no `GAIN_SEL` nets exist (**HUB-DRC-C09**) |
| OI-HUB-05 | Optional 500 mA LDO: decision pending 3.3 V regulator current audit (OI-HUB-01). | Pre-prototype | Moot — folded into **OI-HUB-C11** |

**OI-PBM-HW-01 is CLOSED (SPECIFIED)** — hardware design specified in this document. OI-PBM-HW-02 is CLOSED (SPECIFIED) by §A.4. *(Both remain closed under Rev C, now specified at Rev C §6 and §5 respectively.)*

## A.11 Design Review Checklist *(Rev B §11 — superseded by Rev C §12)*

| Item | Description | Status at supersession |
|------|-------------|--------|
| HUB-DRC-01 | DG2788A GAIN_SEL default LOW confirmed (GPIO power-on state analysis) | Open — requires i.MX RT1062 BSP review (OI-HUB-04) |
| HUB-DRC-02 | TIA saturation analysis complete for Rf = 22 kΩ + InGaAs max current | ✓ (§A.2) — 1.58 V < 3.0 V op-amp swing limit |
| HUB-DRC-03 | Rf_A = 47 kΩ and Rf_B = 22.1 kΩ, 1%, ≤ 50 ppm/°C, matched ≤ 0.5% per slot | Open — component selection to BOM |
| HUB-DRC-04 | DG2788A RON ≤ 2.5 Ω impact on TIA offset quantified | ✓ (§A.3.5) — 2.5 Ω << Rf; negligible |
| HUB-DRC-05 | PCA9546A I2C address non-conflicting with other hub I2C peripherals | Open — hub I2C address map audit |
| HUB-DRC-06 | 3.3 V supply budget (5× smart modules = 250 mA) verified | Open — OI-HUB-01 |
| HUB-DRC-07 | Gain switch assert before I2C mux enable sequencing confirmed in firmware | ✓ (§A.5.1; firmware/pbm_1064nm/src/np_pbm1064_detect.c Rev B) |
| HUB-DRC-08 | Boot init function configures GAIN_SEL[0..4] LOW before zone detect task | ✓ (§A.5.3; np_pbm1064_hal_tia_gain_boot_init) |
| HUB-DRC-09 | Feedback trace length ≤ 5 mm from DG2788A to TIA op-amp | Open — layout DRC (OI-HUB-02) |
| HUB-DRC-10 | PCA9546A channel enable/disable firmware tested with 5-module simultaneous scenario | Open — FAI-SM-04 bench |
</content>
</invoke>
