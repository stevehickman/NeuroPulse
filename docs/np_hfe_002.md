# Accessible Zone-Module Position Identification and Guided Placement

**Project:** NeurOne
**Document:** NP-HFE-002
**Revision:** 2
**Date:** 2026-08-20
**Status:** DRAFT
**Effective Date:** 2026-07-31
**Author:** NeurOne Systems Engineering (Rev 2); Steve Hickman (CEO, interim Quality authority) (Rev 1)
**Approved By:** — (Rev 2 pending review; Rev 1 approved by Steve Hickman, CEO, 2026-07-31)
**References:** NP-HFE-001 Rev 1 (CT-01); NP-HEX-ZM-001 Rev 1 §3.2/§3.4/§4a/§5.4a; NP-DT-001 Rev 2 (DI-USE-05); NP-RM-001 Rev 1 (RISK-15, RISK-22); NP-FW-ZA-001 Rev 1 (superseded — bone-conduction supersession note); NP-TOOL-ZM-SM-001 Rev 1 (superseded — F-05/F-06 braille + tactile dots); IEC 62366-1:2015+AMD1:2020; FDA HFE Guidance 2016; ISO 17049:2013; ISO 21542:2021 §75 (tactile wayfinding); NP-HW-EEGNET-001 Rev 3 §1.7.4/§1.8 (fifth-type pressure on L3); WCAG 2.2 / Apple VoiceOver + Android TalkBack platform conventions
**Related Issues:** —
**Gate:** NP-COORD-001 G3 (VE-12), via NP-HFE-001 §7/§8
**IEC 62304 Class:** SW-03 Class B (companion app guidance); SW-02 Class B (hub inventory reporting). No Class C content — no layer of this design is a safety interlock (see §10).
**Supersedes:** RISK-15 Layers 3, 4 and 5 (braille + raised numeral on module; N tactile dots on shell; bone-conduction insertion confirmation) — retired, not re-scaled
**Parent Document:** NP-HFE-001

---

## 1. Purpose and Scope

NeurOne carries a formal design input that blind and colour-vision-deficient users must be able to identify and place zone modules correctly **without any visual cue** (NP-DT-001 DI-USE-05; NP-HFE-001 CT-01; NP-RM-001 §7 usability hazard RISK-15). The hardware architecture that the original five-layer keying scheme was written against no longer exists. This document specifies the replacement.

**In scope:** how a user who cannot see the helmet establishes (a) *which module type is in their hand* and (b) *which socket they are touching*, to the accuracy a protocol's required module map demands, and how errors are surfaced and corrected.

**Out of scope:** module *extraction* force and grip (RISK-22 / CT-02 / DI-USE-02 — unchanged, and now a cluster-actuator problem per NP-HEX-ZM-001 §5.4a); the while-worn spoken configuration readback (a separate work item against the NP-FW-ZA-001 supersession note — this document constrains it in §7.5 but does not specify it); 10-20 registration itself (gate REG-1); the active-surface boundary (gates ACT-1/ACT-2).

**This document is a design specification, not a test report.** Its requirements enter NP-HFE-001's formative and summative plans; no human-factors data exists for it yet (§11).

---

## 2. What Broke

### 2.1 The retired scheme

RISK-15 protected a helmet with **five large, position-unique zone modules**. Each module *was* a position: ZM-01 "Frontal Left" through ZM-05 "Parietal Right". Five layers enforced correct placement:

| Layer | Mechanism | Status |
|---|---|---|
| 1 | Asymmetric mechanical key, unique per zone position | **Retired** — PR #223, "Remove type-differentiating mechanical key"; confirmed unnecessary by SMART-1 (NP-HEX-ZM-001 §4a) |
| 2 | ZONE_ID resistor ladder (FPC pin 18, 10 kΩ…220 kΩ) | **Retired** — replaced by `np_module_map` UID auto-inventory |
| 3 | ISO 17049 braille + raised numeral (1–5) on the module face | **Retired here** — §2.3 |
| 4 | N raised tactile dots (1–5) on the shell beside each slot | **Retired here** — §2.3 |
| 5 | Bone-conduction spoken confirmation at insertion | **Retired here** — §2.4 |

### 2.2 What replaced the architecture

NP-HEX-ZM-001 §4a states the change that governs everything below:

> Identity is split — **socket = position** (fixed major address from the geometry map) and **module = type** (self-reported on insertion via UID → element inventory).

One universal 40 mm hexagonal module mould. Three T1 tile types (T1-A base PBM, T1-B EEG/dual electrode, T1-C 1064 nm smart) plus T2-D, differing only in element population on an identical footprint. ~80 sockets on the scan-grounded v1 lattice (NP-HEX-ZM-001 §3.4), arranged in 12 rows of widths `3 6 7 8 9 8 9 8 7 6 5 4`, every row symmetric about the midline and row widths alternating parity.

### 2.3 Why Layers 3 and 4 cannot be re-scaled

Three independent reasons, any one of which is sufficient:

**(a) Discriminability.** The layers encoded position as a *count* (1–5). Counting raised dots is reliable to about 4–5 and degrades fast beyond that; nobody distinguishes 62 dots from 63 by touch. Braille numerals are worse for this purpose than they look: a single braille digit is learnable by a non-braille-reader in minutes, an 80-symbol arbitrary code is not, and multi-cell numerals ("46") require actual braille literacy — which most late-blind adults, the majority of the blind adult population, do not have. A scheme whose accessibility depends on braille literacy excludes most of the population it is for.

**(b) Geometry — the decisive one.** The retired shell had five discrete slots with broad shell surface between them, which is where the Layer-4 dots lived. The hex lattice **tessellates**: rigid hex prisms presenting a continuous emitting field touch at their innermost faces (NP-HEX-ZM-001 §3.4). The only shell surface left between adjacent sockets is the structural web, on the order of a few millimetres wide. ISO 17049 mandates 2.5 mm dot spacing within a cell and 6.0–7.0 mm cell-to-cell spacing — a braille cell has a footprint on the order of 6 × 10 mm. **A braille cell does not fit between two sockets, and neither does a 1–5 dot row for any position beyond the first two.** This is not a psychophysics judgement call; it is a dimensional one, and it holds for any plausible web width.

**(c) The module can no longer carry position.** Layer 3 put the numeral on the *module*, which worked only because a module *was* a position. There is now one universal mould and modules are type-agnostic — any type inserts into any socket. A position marking on a universal module would be a manufacturing lie. Position identity has moved onto the shell (which per (b) has no room) or into software.

**Conclusion:** per-socket tactile position marking is dead. Not "needs a better encoding" — the surface it would live on does not exist.

### 2.4 Why Layer 5 was never viable

Recorded in full at NP-FW-ZA-001's supersession note and reproduced here because it constrains §7.5: bone conduction requires the transducer in contact with the mastoid. Inserting a module requires the helmet off the head. The confirmation therefore fires when nobody can hear it. This predates the hex redesign and was true of the original 5-zone design. Bone conduction remains correct for **while-worn** confirmations and is retained for those (§7.5); it is wrong for this task at any socket count.

### 2.5 The task, stated honestly

A user who cannot see the helmet must be able to:

1. **Identify the module in hand** — one of 3 (T1) or 4 (T2) types. *Closed set, small.*
2. **Locate a named socket on the shell** — one of ~80. *Open set, large, and growing with every lattice re-cut.*
3. **Seat it in the correct orientation** — binary. *Already solved mechanically (§7.4).*
4. **Know whether they got it right**, and if not, what to do about it.

The old scheme collapsed (1) and (2) into a single marking because the architecture collapsed type and position into a single part. **The new architecture splits them, and the accessibility design must split with it** — because the two halves have opposite cardinality, and cardinality is exactly what decides whether a tactile symbol works. That split is the thesis of this document: tactile marking follows *type* onto the module, where the set is small enough for it to work; *position* moves to guidance, because no static marking of an 80-member open set can work.

---

## 3. Design Constraints Inherited from the Architecture

| # | Constraint | Source |
|---|---|---|
| C-1 | One universal module mould; no type- or position-differentiating mechanical key | NP-HEX-ZM-001 §4a (SMART-1) |
| C-2 | Sockets tessellate; inter-socket shell surface is a structural web of a few mm | NP-HEX-ZM-001 §3.4 |
| C-3 | Socket count is provisional (78 → 80 already, gated on REG-1/ACT-1) — any design that hard-codes a per-socket artefact re-tools on every lattice re-cut | NP-HEX-ZM-001 §3.4, §7 |
| C-4 | Modules are clamped in **clusters** of 3–7 (~4–10 actuators), not individually; a tile is only reliably seated once its cluster clamp is thrown | NP-HEX-ZM-001 §5.4a |
| C-5 | Insertion detection granularity is **module electrically seated and answering with its UID** — `np_module_map_apply_poll()`. There is no per-socket touch, hover, or partial-insertion sensing anywhere in the design | `firmware/hub_control/include/np_module_map.h` |
| C-6 | Nothing in code defines or derives a brain lobe; zones are authored only in `protocols/predefined/00-zones.npps`. The generator owns socket id, count, row, col, x/y and midline parity — and nothing else | ZONE-1 (2026-07-30), `scripts/sync-socket-map.ts` |
| C-7 | The app already receives live module status over BLE during setup (`SetupStep.zoneModules`, `zoneModulesPublisher`) | `app/ios/NeurOne/Setup/HardwareSetupManager.swift` |
| C-8 | `np_module_map_check_placement()` blocks a protocol whose required module map is unmet — delivered and host-tested | NP-HEX-ZM-001 §4a |
| C-9 | No mandatory subscription; core functions offline-capable permanently | CLAUDE.md §1 |

C-3 deserves emphasis. The lattice moved from 78 to 80 sockets *during documentation of this very redesign*, and REG-1 has not yet fixed the row/lobe boundaries against shell CAD. Any scheme requiring one moulded artefact per socket is a scheme that must be re-tooled every time the anatomy work lands. That is an argument against per-socket marking that is independent of §2.3 and would apply even if the web were 50 mm wide.

---

## 4. What "Position Identification" Must Actually Achieve

The requirement is not "the user can name any socket". It is "the user can execute a protocol's required module map". Those differ, and the difference is most of the design budget.

A protocol carries a required module map: one specifier per socket, either **DONT-CARE** or a subset of allowed types (NP-HEX-ZM-001 §4a). Real maps are overwhelmingly DONT-CARE. The representative T1 build is *"~8–9 × T1-B at the montage sites plus Oz, and the balance T1-A"* — that is **9 positions that matter out of ~80**, with the other ~71 interchangeable by construction.

Two consequences:

- **The addressing precision needed is per-position only where the map is not DONT-CARE.** Elsewhere any module of the right type in any socket is correct, and "correct" is not a judgement the user has to make.
- **The work must be structured as exceptions, not as an 80-item list.** "Fill every socket with a base module, then place these nine" is one bulk instruction plus nine guided placements. An implementation that walks 80 sockets in sequence is not merely tedious, it is a different and much harder task than the one the hardware actually poses. This is requirement HFE-R-09.

---

## 5. Options Considered

Five candidates, assessed against C-1…C-9 and §4.

### Option A — Per-socket tactile symbol (scale up Layers 3/4)

Give each socket a unique moulded tactile identifier.

| | |
|---|---|
| **For** | Continuous with the retired design; zero electronics; works unpowered and phone-free |
| **Against** | Fails §2.3(a) discriminability at 80; fails §2.3(b) — the artefact does not physically fit in the inter-socket web; fails C-3 — re-tools on every lattice re-cut; fails C-1 for the module half |
| **Verdict** | **Rejected.** Not a scaling problem; the surface it needs does not exist. |

### Option B — Coarse tactile landmarks + counting

A small closed set of moulded landmarks at anatomically meaningful boundaries, with sockets addressed as an offset from the nearest landmark — the way a blind reader navigates a long braille document by headings rather than by numbering every paragraph, or a tactile map by its coastline.

| | |
|---|---|
| **For** | Scales — landmark count is independent of socket count, so it survives a lattice re-cut (C-3); a *line* feature fits the structural web where a *symbol* does not (C-2); established practice in tactile wayfinding (ISO 21542 §75); works unpowered, phone-free, and for deaf-blind users; counting depths stay in the reliable ≤4 regime (§6) |
| **Against** | Requires the user to hold a mental model of the lattice; counting on a concave dome, reaching into a bowl, is materially harder than tracing a flat page — and that is an empirical question this document cannot settle; gives *location*, never *intent* — it cannot tell you what the protocol wants there; adds moulded features whose positions depend on REG-1 |
| **Verdict** | **Adopted as a layer, rejected as the whole answer.** It is the substrate that makes a spoken instruction followable. It cannot be the primary channel because it carries no protocol information. |

### Option C — "Point at a socket, the device speaks its name"

Touch a socket; the helmet announces the position.

Checked against the firmware rather than assumed: `np_module_map_apply_poll()` takes `(socket_id, reported_uid, health, …)`, and a socket reports only once its module is **electrically seated and answering on its I2C/1-wire link**. There is no touch, capacitive, proximity, hover or partial-insertion sensing at a socket anywhere in the design (C-5), and per C-4 a tile is not reliably seated until its cluster clamp is thrown.

| | |
|---|---|
| **For** | The most direct imaginable interaction; no counting, no mental model, no second device |
| **Against** | Requires ~80 new sense channels on the inner bowl. SMART-1 already put an I2C bus and a switchable-gain TIA stage on every socket, and NP-HEX-ZM-001 §7 records the Hub PCB I2C fan-out for ~80 sockets as *unscoped NRE* — this would be a second ~80-channel fan-out problem stacked on an unsolved one. It also needs an audio output reachable with the helmet off-head, which bone conduction is not (§2.4), so it needs a speaker too. Cost is real and the benefit duplicates Option D |
| **Verdict** | **Rejected on cost.** Revisit only if a future socket connector gains a low-current presence-and-UID contact at partial insertion for other reasons — see OI-HFE2-04, which asks a much cheaper version of the same question. |

### Option D — Companion-app guided placement

The phone/tablet/computer runs the placement task: an ordered, exception-structured work list, spoken through the platform screen reader, confirmed against live inventory over BLE.

| | |
|---|---|
| **For** | The only option that carries *protocol intent*, not just location — it knows what belongs where and can say so. Apple and Google have already solved accessible guided interaction; VoiceOver/TalkBack, braille-display routing, haptics and speech-rate control come free and are already familiar to the user. Confirmation and error correction come from data the app already receives (C-7). Zero new hardware. Survives a lattice re-cut for free (C-3). **It is the only option that serves a deaf-blind user at all** — no audio channel and no static tactile marking can deliver dynamic, protocol-specific guidance, but a refreshable braille display driven by VoiceOver/TalkBack can |
| **Against** | Requires a second device for a hardware task. Confirmation granularity is per-cluster, not per-module (C-4). Depends on the app's module-status pipeline being re-cut from the retired 5-element array to a socket-indexed inventory (OI-HFE2-02) |
| **Verdict** | **Adopted as the primary channel.** |

The "requires a phone" objection is weaker than it looks, and it is worth being explicit about why rather than waving it away:

- Module placement is a **setup and reconfiguration** task — first setup, an upgrade, a cleaning cycle — not a per-session one. CLAUDE.md's autonomy commitment is that *sessions* run in Mode 3 without a phone, and that no core function requires a subscription or a network. The app is free, local, and offline (BLE/USB peer, no cloud), so guided placement does not touch that commitment (C-9).
- **Sighted users have no better option.** There is nothing printed on a tessellating field of identical hex tiles, and per C-2 there is nowhere to print it. A sighted user staring at socket 46 sees exactly what a blind user feels: an anonymous hexagon. The app is not an accommodation bolted onto a visual design — **it is the only channel that exists, for everyone.** That is what makes it the right primary: the accessible path and the main path are the same path, so it cannot rot from disuse, and it is exercised by every user on every setup.

### Option E — Tactile placement card (raised diagram)

Ship a raised-line tactile diagram of the lattice with the standard map marked, per NP-HEX-ZM-001's standard maps.

| | |
|---|---|
| **For** | Genuinely accessible — a tactile map is traced, not decoded, so it evades the §2.3(a) discriminability ceiling entirely; phone-free; cheap to produce; a real answer for a user who has no second device |
| **Against** | Only serves the *standard* maps — a user-defined map or an OTA-delivered new standard map has no card; it is a static artefact against a provisional lattice (C-3); it requires the user to transfer a position from a flat card to a curved dome, which is a second mental transformation on top of Option B's counting |
| **Verdict** | **Adopted as a fallback**, restricted to shipped standard maps, and explicitly labelled as such. |

---

## 6. Decision

**A layered design: app-primary guidance, tactile substrate, module-type tactile marking, and the existing software placement gate as backstop.** No single layer is sufficient and none is a safety interlock.

| Layer | What it does | Set size | Where it lives | Status |
|---|---|---|---|---|
| **L1 — Tactile landmark grid** | Makes a spoken position instruction *followable by touch* | 4 feature kinds | Shell (inner bowl web + rim) | New, §7.1 |
| **L2 — App guided placement** | Carries protocol intent; issues, confirms and corrects each placement | unbounded | Companion app (SW-03) | New, §7.2 |
| **L3 — Module-type tactile marking** | Tells the user *what is in their hand* | 3 (T1) / 4 (T2), **encoding heads to 6** | Module face (universal mould, type-specific insert) | New, §7.3; encoding revised at Rev 2 |
| **L4 — Software placement gate** | Refuses to run a protocol against a mismatched map | — | `np_module_map_check_placement()` | **Delivered** (C-8) |
| **L5 — While-worn audio readback** | Confirms configuration when the helmet is on the head | — | Bone conduction | Separate work item, constrained at §7.5 |
| **F — Tactile placement card** | Phone-free fallback for shipped standard maps | standard maps only | In-box accessory | New, §7.6 |

The design's centre of gravity is the L1/L2 pair: **L2 knows what should go where and can say it; L1 makes what L2 says locatable.** Neither works alone. L3 is the piece the old design got right for the wrong reason and which is now correct for the right one — a closed 3–4 member set, exactly the regime where a tactile symbol is the best available technology.

---

## 7. The Design

### 7.1 L1 — Tactile landmark grid

Four feature kinds, ~15 moulded features total, **none of them per-socket**.

**(a) Midline spine — the primary reference.** The lattice's row widths alternate parity, so on even-width rows (6, 8, 8, 8, 6, 4 — rows r1, r3, r5, r7, r9, r11) the sagittal midline falls on a *socket boundary*, and on odd-width rows (3, 7, 9, 9, 7, 5 — rows r0, r2, r4, r6, r8, r10) a socket sits *on* the midline. The midline web is therefore already an interrupted line, and L1 profiles it into a deliberate **dashed spine**: a raised rib in the six even-width rows, a gap in the six odd-width rows.

Three properties fall out of this for free, and they are the reason this feature is worth more than its cost:

- The **gaps are the six midline sockets** — 2, 13, 29, 46, 62, 74 on the v1 lattice — which are precisely the anatomically significant midline addresses, including **74 = Oz**, the site the photoparoxysmal halt depends on.
- Whether the spine runs *through* the socket you are touching or *alongside* it tells you the row's parity, which is a free error check on any count.
- It costs no new real estate. The web between two sockets already exists; L1 specifies its profile.

**(b) Band boundary ridges — longitudinal index.** Three transverse ridges in the row-boundary web, at the boundaries the lattice model already uses:

| Ridge | Between rows | ~arc | Anatomical name | Code at midline crossing |
|---|---|---|---|---|
| **A** | r2 / r3 | ~27% | frontal band boundary (F–FC) | 1 bump |
| **B** | r5 / r6 | 50% | **central sulcus** | 2 bumps |
| **C** | r9 / r10 | ~82% | **parieto-occipital sulcus** | 3 bumps |

Ridge identity is coded as 1/2/3 bumps **where the ridge crosses the midline spine**, not along its length — a closed 3-member count, trivially discriminable, and it fits in the small area at the crossing rather than demanding three parallel ribs in a 2 mm web.

With the front and rear rim edges as natural landmarks, the 12 rows partition into bands of 3, 3, 4, 2. **Maximum longitudinal count from the nearest reference is 2 rows**, since a 4-row band is entered from either end.

**(c) Lateral reference — the spine itself, plus the active-surface edge.** Maximum row width is 9, so **maximum lateral count from the midline is 4**. The lateral edge of the active surface (the lattice stops above the ears, ACT-1) is a second natural reference giving an inward count, also ≤4. The two routes are independent, so agreement between them is a self-check the user can perform without help.

**(d) Standard-electrode-site marker — one uniform feature, ~9 instances.** A single tactile feature, identical at every instance, at the T1 neurofeedback montage sockets (Fp1/2, F3/4, C3/4, P3/4) plus Oz. It does **not** identify *which* site — L1(a)/(b)/(c) do that. It says only *"a T1-B electrode module belongs here"*, which collapses the most common T1 build to a rule a user can execute almost without counting: **place an EEG module wherever you feel the dimple, base modules everywhere else.**

This is the one feature whose *positions* depend on 10-20 registration, so it is **gated on REG-1** (OI-HFE2-01). Until REG-1 lands it must not be cut into tooling; L1(a)–(c) are not so gated, because they derive from lattice structure (row parity, row boundaries, midline) rather than from anatomy.

**Counting depth summary — the number that decides whether L1 works.** Computed against the shipped `app/web/src/lib/socketMap.generated.ts`, not asserted:

```
row -> longitudinal count:  r0:1  r1:2  r2:1  r3:1  r4:2  r5:1
                            r6:1  r7:2  r8:2  r9:1  r10:1 r11:1
MAX longitudinal depth  = 2
MAX lateral count       = 4
row widths              = 3 6 7 8 9 8 9 8 7 6 5 4   (parity strictly alternates: true)
midline sockets (gaps)  = 2, 13, 29, 46, 62, 74     (exactly one per odd-width row: true)
odd rows  (spine gaps)  = 0, 2, 4, 6, 8, 10
even rows (spine ribs)  = 1, 3, 5, 7, 9, 11
```

So: **≤2 rows longitudinally, ≤4 sockets laterally**, both inside the range tactile counting supports reliably, and the spine's six gaps land exactly on the six midline sockets including Oz (74). Both results are recomputable from the generated map and are the substance of HFE-R-03 and HFE-R-04 — they must be re-run on any lattice re-cut, since a different set of row widths could push a band past depth 2.

What this does **not** establish is that a depth-2/4 count is achievable *on this surface*. The user is reaching into a concave bowl, not tracing a flat page. **That is the design's primary empirical risk and it is not settled here** (§11, OI-HFE2-03).

### 7.2 L2 — Companion-app guided placement

Extends the existing `SetupStep.zoneModules` step (C-7) from a presence check into a guided task.

**Instruction grammar.** Every position is spoken as **landmark → band → row → side → count**, mirroring L1 exactly, with the zone name from `00-zones.npps` as context. Raw socket numbers are never the primary form; they are available on demand as a detail:

> "Place 3 of 9. EEG module — two ridges on the face.
>  Frontal Left. Find the midline spine and run back to the **two-bump** ridge — that's the crown ridge. **One row in front of it**, **left** side, **third socket out** from the midline.
>  You should feel a dimple in that socket."

The final sentence is the L1(d) cross-check: a correct target confirms itself before insertion.

**Error grammar.** Errors are spoken as a **corrective movement**, never as a failed socket id:

> "That EEG module is one socket too far out. Move it **one socket toward the midline**."

`np_module_map_check_placement()` returns failed socket ids (C-8); converting those ids into movements is the app's job, and it is the difference between a usable feature and a list of numbers.

**Confirmation and its granularity.** Per C-4, a tile is reliably seated only once its cluster clamp is thrown, so confirmation is **per-cluster (3–7 tiles), not per-module**. The work list is therefore organised by cluster: place a cluster's modules, throw its actuator, hear the whole cluster read back and corrected at once. This matches the physical workflow rather than fighting it, and it means the confirm-and-correct loop's cost is one clamp throw per batch — which is only acceptable because ejector springs make extraction near-zero-force and the actuator is specified for low one-handed input force (NP-HEX-ZM-001 §5.4a, RISK-22 intent). **If MECH-2 delivers a high-force or fiddly cluster actuator, this loop degrades badly**; that dependency is recorded at OI-HFE2-05.

**Exception structure.** Per §4, the work list is bulk-plus-exceptions, never 80 sequential items (HFE-R-09).

**Channels.** Native platform screen reader (VoiceOver / TalkBack) is the required path — not an in-app bespoke TTS, which would lose the user's speech rate, voice, verbosity and gesture settings, and would lose **refreshable braille display routing**, which is the deaf-blind user's only channel. Phone haptics confirm each accepted placement, giving a non-audio, non-visual acknowledgement. Everything runs offline over BLE/USB (C-9).

### 7.3 L3 — Module-type tactile marking

The retired Layer 3 put a tactile marking on the module. That was right; it identified the wrong thing. Type is a **closed 3-member set** in T1 (T1-A, T1-B, T1-C) and 4 in T2 (+T2-D) — the regime where tactile symbols are reliable and where a raised numeral or dot-count is learnable in a minute by a user with no braille literacy.

Marking is on the module's outward (shell-facing, non-emitting) face, reachable with the module in hand:

**Encoding (Rev 2): a nested figure, not a bar count.** Rev 1 specified 1/2/3/4 raised bars in a row.
That works at four types and stops there, because it is **counting**, and §2.3(a) puts tactile counting
at *"reliable to about 4–5 and degrades fast beyond that."* Rev 2 keeps the raised bar as the *element*
and changes what the elements build: each successive type adds one bar to a figure rather than one item
to a tally.

| Elements | Figure | Type | What discriminates it from its predecessor |
|---|---|---|---|
| 1 | one side of a square — a single bar | **T1-A** base PBM | — (most common; simplest symbol) |
| 2 | two sides — a corner | **T1-B** EEG / dual electrode | topology: a line versus an angle |
| 3 | three sides — an open U | **T1-C** 1064 nm smart PBM | gross shape |
| 4 | four sides — a closed square | **T2-D** 1170 nm laser | perimeter completeness |
| 5 | square + one diagonal | *reserved* | interior content |
| 6 | square + both diagonals — an X in a square | *reserved* | interior content |

**Why this is not merely a bigger alphabet.** A closed figure is recognised whole; it is not counted.
The scheme therefore steps outside the constraint that bounds Rev 1 at four, rather than stretching it.
It also turns a flat 1-of-N identification into a **two-stage decision of branching ≤3**: perimeter
open-or-closed splits the set into {1,2,3} and {4,5,6}, after which the user either counts sides (≤3)
or reads the interior (empty / one diagonal / X). That is *better* than Rev 1's flat 1-of-4, so the
change is worth making **even if the type set never exceeds four**.

**Two drawing rules, both load-bearing.**

1. **The gap sits at a fixed, known position** — always the same side, aligned to the module's
   mechanical orientation key. Without this, distinguishing 3 from 4 means tracing the whole perimeter
   to prove a negative; with it, it is a point check at a location the hand already found. The open
   figure's gap must be unmistakably wide, never a hairline break. **3-versus-4 is the weakest
   discrimination in the set and this rule is what carries it** (`OI-HFE2-10`).
2. **One side aligns to the mechanical key.** A 4-fold figure on a 6-fold hex body has no natural
   alignment and could invite a user to rotate the module to "square it up", competing with §7.4's
   mechanical orientation feature. Aligning one side to the key makes the figure *reinforce*
   orientation instead. Free, and it is the same feature rule 1 needs.

**What does not change.** The element is still a raised **bar**, discriminable by a single sweeping
touch rather than a fingertip census. **ISO 17049 braille may be co-moulded in addition, never
instead** — the design must not require braille literacy (§2.3(a)). Diagonal handedness carries no
information: "\" and "/" are the same figure rotated and both read as 5, so an arbitrary hold cannot
produce a misread.

**Fallback.** Rev 1's 1/2/3/4 bar row is retained as the fallback if `OI-HFE2-10`'s formative shows the
3-versus-4 discrimination does not hold. The fallback caps the taxonomy at four types, which is a
constraint on the *product*, not only on this document — see `NP-HW-EEGNET-001` §1.8, where a fifth
(electrode-only) tile type is under consideration.

**Tooling note.** C-1 forbids type-differentiating *mechanical keying*, which is about mating geometry
that blocks insertion. A marking on the non-mating outward face is not a key: it does not restrict which
socket a module enters, and it must not be allowed to become one. Implement as a type-specific insert in
the otherwise universal mould, on a face that touches nothing (OI-HFE2-06).

**Two tooling consequences of the nested figure specifically.** (i) All six variants are **subsets of
one master figure** (square + two diagonals), so the family is one insert geometry with selective
element suppression rather than four unrelated patterns — cheaper, and extensible without new geometry.
(ii) The figure needs a **compact 2D patch** where a bar row could hug an edge. That makes
`OI-HFE2-06` more binding, not less, and it surfaces a tension this document has carried since Rev 1:
§7.3 places the marking on the *"outward (shell-facing, non-emitting)"* face while the tooling note
requires *"a face that touches nothing"* — and the shell-facing face is also where the 19-contact pad
array mates to its socket. Which region of which face carries the marking, alongside the pad array and
any co-moulded braille cell, must be resolved under `OI-HFE2-06` before the insert is cut.

### 7.4 Orientation

Orientation is already mechanically enforced: the socket/module mating feature is deliberately not rotationally symmetric where pin alignment requires it (NP-HEX-ZM-001 §4a, NP-HFE-001 CT-01). Two accessibility requirements attach to it (HFE-R-05):

- The orientation feature must be **unmistakable by touch on both halves** — a single unambiguous notch or flat, not a subtle asymmetry a fingertip has to hunt for.
- A wrongly-oriented module must **fail to seat**, not seat poorly. A module that goes in far enough to feel seated but makes bad optical/thermal/electrical contact is a silent failure, and silent failure is exactly what a non-visual user cannot detect.

### 7.5 L5 — While-worn audio (constraint only)

Bone conduction is retained for confirmations delivered **while the helmet is worn** — session-start configuration readback, in-session fault alerts. It must not be used for at-insertion confirmation (§2.4). The separate work item redirecting insertion-time confirmation to the companion app is constrained by §7.2's grammar and channel requirements; it is not specified here.

### 7.6 F — Tactile placement card

A raised-line diagram of the lattice, one per shipped standard map, in the box. Traced rather than decoded, so it evades the §2.3(a) ceiling. Scope-limited and labelled as such: it serves **shipped standard maps only**, cannot serve user-defined or OTA-delivered maps, and is a static artefact against a provisional lattice (C-3). It is a fallback for a user without a second device, not a substitute for L2.

---

## 8. Requirements

Testable requirements entering NP-HFE-001's formative and summative plans. All are `Information for safety`/`usability` class — none is a risk control in the ISO 14971 §7.4 sense (see §10).

| ID | Requirement | Layer | Verification |
|---|---|---|---|
| HFE-R-01 | Every L1 feature is discriminable by touch, without vision, by a user with no prior training, in ≤10 s | L1 | Formative, n≥5 blind/low-vision |
| HFE-R-02 | The three band ridges are distinguished from one another by their midline bump code with ≥95% accuracy | L1(b) | Formative |
| HFE-R-03 | Longitudinal counting depth ≤2 rows and lateral counting depth ≤4 sockets from the nearest L1 reference, for every socket on the lattice | L1 | **Verified 2026-07-31** against `socketMap.generated.ts` (§7.1: max 2 / max 4). Static script check, not a study — must be re-run on any lattice re-cut |
| HFE-R-04 | The dashed midline spine's ribs and gaps correspond exactly to the even- and odd-width rows, and its gaps to the midline sockets, as reported by `scripts/sync-socket-map.ts` | L1(a) | **Verified 2026-07-31** — gaps = sockets 2, 13, 29, 46, 62, 74, exactly one per odd-width row; parity strictly alternates (§7.1). Promote to a build check alongside the existing structural zone checks |
| HFE-R-05 | A wrongly-oriented module does not seat; the orientation feature is locatable by touch on both module and socket in ≤5 s | orientation | Formative + bench |
| HFE-R-06 | Module type is correctly identified by touch alone with ≥98% accuracy across all types | L3 | Formative |
| HFE-R-07 | Every L2 position instruction is expressible in the §7.2 landmark grammar; no instruction requires a raw socket number to be actionable | L2 | Code review + content audit of generated instruction strings |
| HFE-R-08 | Every L2 error is expressed as a corrective movement, never as a bare failed socket id | L2 | Code review + formative |
| HFE-R-09 | The work list is exception-structured — a build requiring N placements of which K are non-DONT-CARE presents ≤K+1 guided steps | L2 | Unit test against the standard maps |
| HFE-R-10 | The complete placement flow is operable end-to-end using only VoiceOver (iOS) and TalkBack (Android), including a refreshable braille display, with no in-app bespoke TTS on the critical path | L2 | Accessibility audit + formative with screen-reader users |
| HFE-R-11 | Each accepted placement produces a haptic confirmation on the companion device | L2 | Instrumented test |
| HFE-R-12 | The entire flow completes with no network connectivity | L2 | Airplane-mode test |
| HFE-R-13 | A blind user completes a standard T1 build unaided, with all non-DONT-CARE placements correct at first protocol start | L1+L2+L3 | **Summative** |
| HFE-R-14 | No protocol starts against a module map that `check_placement` rejects | L4 | Already host-tested (C-8); re-confirmed at integration under SW-1 |
| HFE-R-16 | The three-sided (open) figure is distinguished from the four-sided (closed) figure by touch with ≥98 % accuracy, with the gap at its specified fixed position | L3 | **Formative — this is the weakest pair in the §7.3 encoding (`OI-HFE2-10`)** |
| HFE-R-17 | Identification of any L3 figure completes in ≤10 s by touch alone, without the module being rotated to a preferred orientation | L3 | Formative, n≥5 blind/low-vision |
| HFE-R-15 | Protocol required-maps do not use DONT-CARE at any socket where module type materially changes the delivered wavelength | authoring | Build check over `protocols/predefined/` — see §10.2 |

---

## 9. Traceability

| Input / hazard | Was | Now |
|---|---|---|
| DI-USE-05 (NP-DT-001) | Five-layer keying → then "open gap" | This document; orientation feature + L1/L2/L3 + L4 gate |
| CT-01 (NP-HFE-001 §5.2) | "Open gap, not yet solved" | This document; formative + summative scope per §8 |
| RISK-15 (NP-RM-001 §7 usability) | Five-layer keying | This document + `check_placement`; severity reframed per §10 |
| NP-TOOL-ZM-SM-001 F-05 / F-06 | Braille + numeral on module; N dots on shell | F-05 superseded by L3 (type, not position); F-06 superseded by L1 (landmarks, not per-socket dots) |
| NP-FW-ZA-001 Layer 5 | Bone conduction at insertion | Retired for this task (§2.4); retained while-worn (§7.5) |

---

## 10. Effect on the Risk Picture

### 10.1 RISK-15's severity is materially lower than it was, and it is worth saying why precisely

Under the retired architecture a mis-keyed module meant a zone driving the wrong site — a wrong-site dosing path. Under the current architecture that path is closed by software rather than by mechanics: `np_module_map_check_placement()` validates the live UID-derived inventory against the running protocol's required map and **blocks the session** on a mismatch (C-8).

The consequence is sharper than "there is a backstop", and the sharpness is the point:

- Where a protocol's map **names a type** at a socket, a misplacement is **detected and blocked** before any energy is delivered.
- Where a protocol's map is **DONT-CARE** at a socket, a misplacement is **undetected but therapeutically equivalent by the map's own declaration** — that is what DONT-CARE asserts.

So under a correctly-authored map, **every placement error that matters is caught, and the errors that are not caught do not matter.** The residual harm from a placement error is a protocol that will not start: frustration, rework, loss of independence and dignity — real costs, and the reason this design exists — but not S3+ physical harm. RISK-15's severity should be re-rated accordingly and its ALARP argument re-framed around *access and independence* rather than *wrong-site dosing*. **The recommendation is to re-rate at S2 (temporary, reversible — a blocked session and the frustration of rework), not to close the risk**; the formal re-rating is a change order under NP-QMS-DC-001 §8.1 and is not made by this document (OI-HFE2-07).

### 10.2 One gap this analysis found, and closed

The "errors that are not caught do not matter" claim depends entirely on maps being authored honestly. It breaks in one specific case: a protocol map that is DONT-CARE at a socket where the user might place a **T1-C (1064 nm)** instead of a **T1-A (660/808 nm)**. Firmware per-channel dose ceilings keep that within *safety* limits, but the user receives a different wavelength than the protocol's evidence base describes — an efficacy and claims problem, not a safety one.

The fix belongs in map authoring, not in the user's fingertips: **a required map must not use DONT-CARE at any socket where module type materially changes the delivered wavelength.** That is HFE-R-15, enforceable as a build check over `protocols/predefined/` alongside the existing lateralized-protocol audit in `scripts/sync-socket-map.ts`.

### 10.3 Not a safety interlock

No layer of this design is an ISO 14971 §7.4 risk control. L4 (`check_placement`) is the control, and it was already delivered and already credited. L1–L3 and F are `information for safety` and usability provisions. This is deliberate: an accessibility feature that is also a safety interlock acquires Class C obligations and a change-control burden that would make it slow to iterate, which is the opposite of what a feature needing formative-driven revision should carry.

---

## 11. Open Items

| ID | Item | Blocking | Owner |
|---|---|---|---|
| OI-HFE2-01 | L1(d) standard-electrode-site marker positions depend on 10-20 registration — **gated on REG-1**; must not be cut into tooling before it lands. L1(a)–(c) are not gated (they derive from lattice structure, not anatomy) | Shell tooling | REG-1 |
| OI-HFE2-02 | App module-status pipeline is still the retired 5-slot model: `NeurOneGATTManager.zoneModules: [UInt8] = [0,0,0,0,0]`, and `ZoneModuleConfiguration` (`app/ios/NeurOne/Models/ZoneModuleInfo.swift`) truncates with `rawSlotData.prefix(5)` and decodes each byte through the retired 5-zone `ZoneID` enum; `SetupError.zoneModulesMissing([Int])` carries slot indices. L2 needs a socket-indexed inventory over BLE. Flagged in NP-FW-ZA-001's supersession note; still not scoped | L2 implementation | App |
| OI-HFE2-03 | **Primary empirical risk** — tactile counting on a concave dome, reaching into a bowl, is materially harder than on a flat page. Formative must test counting accuracy *on helmet-representative curvature*, not on a flat mock-up, or it will produce a falsely reassuring result | HFE-R-01/02/03 credibility | NP-HFE-001 §7 |
| OI-HFE2-04 | Cheap version of Option C: does the socket connector give a low-current presence-and-UID contact at *partial* insertion, before cluster clamp force? If so, confirmation granularity improves from per-cluster to per-module at ~zero cost. Connector-design question for MECH-2 | L2 confirmation granularity | MECH-2 |
| OI-HFE2-05 | L2's confirm-and-correct loop assumes cheap re-opening of a cluster. Depends on MECH-2 delivering the low-force, large-target, one-handed actuator RISK-22 intent requires | L2 viability | MECH-2 |
| OI-HFE2-06 | L3 type marking must be implemented as a mould insert on a **non-mating** face and must not become a de-facto mechanical key (would violate C-1 / SMART-1). **Extended at Rev 2:** §7.3's nested figure needs a compact **2D patch** where a bar row could hug an edge, which makes the unresolved face question acute — §7.3 says *"outward (shell-facing, non-emitting)"* while this item says *"a face that touches nothing"*, and the shell-facing face also carries the 19-contact pad array. Resolve which region of which face carries the figure, the pad array and any co-moulded braille cell **before the insert is cut** | Module tooling | Tooling |
| OI-HFE2-07 | RISK-15 severity re-rating (§10.1) is a change order under NP-QMS-DC-001 §8.1; not made by this document | Risk file currency | Quality |
| OI-HFE2-08 | Recruit blind and low-vision participants for formative testing. NP-HFE-001 §7.2 already has an unresolved recruitment channel (OI-HFE-02) for the Parkinson's/post-stroke study; this is a **second, different** population and needs its own channel | Formative start | NP-HFE-001 OI-HFE-01/02 |
| **OI-HFE2-10** | **Validate the §7.3 nested-figure encoding, and specifically the 3-versus-4 (open U vs closed square) discrimination — the weakest pair in the set.** Rev 2 substitutes a *shape* claim for Rev 1's *counting* claim; that is very likely favourable, but it is an empirical claim of the same kind as §7.1's counting-depth result and gets the same treatment. Run inside `OI-HFE2-03`'s formative, against **HFE-R-16/17**. **Decision consequence:** if it fails, the bar-row fallback caps the taxonomy at four types, which constrains the product — `NP-HW-EEGNET-001` §1.8 has a fifth (electrode-only) tile type under consideration, and `OI-EEGNET-21` depends on this item | HFE + Module tooling | **OI-HFE2-03 formative; before insert tooling** |
| **OI-HFE2-11** | **L1(d)'s premise is contested from two directions and cannot serve both.** §7.1(d) is *one uniform feature* at ~9 instances whose value is the single rule *"place an EEG module wherever you feel the dimple."* `NP-HW-EEGNET-001` §7.1.4 would delete it (T1-B removed, *"9 positions that matter"* → zero); its §1.7/§1.8 would make it more load-bearing (more electrode-bearing tiles, possibly of two kinds). Whichever direction is adopted must state its effect on §7.1(d) explicitly rather than leaving it pointing at a premise that no longer holds | Systems + HFE | **REG-1, with OI-HFE2-01** |
| OI-HFE2-09 | Firmware `np_module_map.h` still carries `np_lobe_t` / `NP_LOBE_*` and a `lobe` field in `np_socket_geom_t`, after ZONE-1 deleted the lobe concept from the generator and web app. L2's instruction grammar must not consume it. Out of scope here; noted because it is a live inconsistency | — | Firmware |

---

## 12. Revision History

| Rev | Date | Author | Description |
|---|---|---|---|
| **2** | **2026-08-20** | NeurOne Systems Engineering | **§7.3 L3 encoding replaced; no other layer changed and no decision in §6 reversed.** *Was:* 1/2/3/4 raised bars in a row — a **count**, which §2.3(a) bounds at *"about 4–5"*, so the scheme stopped at four types. *Is:* a **nested figure** in which each type adds one bar to a shape — one side, corner, open U, closed square, square + diagonal, X in square — reaching **6**. *Cause:* a closed figure is recognised whole rather than counted, so the encoding steps outside the constraint instead of stretching it; and it converts a flat 1-of-N identification into a **two-stage decision of branching ≤3**, which is better than Rev 1 *even at four types*. The raised bar remains the element and the braille-in-addition-never-instead rule is unchanged. **Two drawing rules are load-bearing, not stylistic:** the gap sits at a fixed position aligned to the mechanical key (without it, 3-versus-4 requires tracing the perimeter to prove a negative), and one side aligns to that key so the figure reinforces §7.4's orientation feature rather than competing with it. **Rev 1's bar row is retained as an explicit fallback**, and its cost is named: it caps the taxonomy at four types, which is a product constraint, not merely a documentation one. **New: HFE-R-16/17** (3-versus-4 discrimination ≥98 %; identification ≤10 s without re-orienting the module). **New: OI-HFE2-10** — validate the encoding inside `OI-HFE2-03`'s formative; **OI-HFE2-11** — L1(d)'s premise is contested in opposite directions by `NP-HW-EEGNET-001` §7.1.4 and its §1.7/§1.8. **OI-HFE2-06 extended** with the 2D-patch area demand and the unresolved *"shell-facing"* versus *"touches nothing"* face question, which the nested figure makes acute. Two tooling consequences recorded: all six variants are subsets of one master figure, so the family is one insert geometry with selective suppression. **Rev 2 is unapproved**; Rev 1's approval is retained in the front matter. |
| 1 | 2026-07-31 | Steve Hickman (CEO, interim Quality authority) | Initial release. Replaces the RISK-15 Layer 3/4/5 accessible position-identification scheme, which did not survive the hex-tile redesign. Adopts a layered design: tactile landmark grid (L1), companion-app guided placement (L2, primary), module-type tactile marking (L3), existing `check_placement` gate (L4), while-worn bone conduction (L5, constrained only), tactile placement card (F, fallback). Rejects per-socket tactile symbols (geometry and discriminability) and per-socket touch-to-speak (cost). Records the RISK-15 severity reframe and the DONT-CARE/wavelength authoring gap (HFE-R-15). Closes the open gap flagged at NP-HFE-001 CT-01 and NP-DT-001 DI-USE-05. |
