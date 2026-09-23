# 21-Channel Clinical tACS Driver Stage (T2) — Hardware Specification

**Project:** NeurOne
**Document:** NP-HW-TACSDRV-001
**Revision:** 2
**Date:** 2026-09-23
**Status:** DRAFT — **requirements-grade. No silicon is named and none is named here; §7 states why selecting one is `OI-TCAP-03`'s act and not this document's.**
**Effective Date:** —
**Author:** NeurOne Systems Engineering
**Approved By:** — (new document)
**References:** CLAUDE.md §3 T2 additions (clinical tACS ≤4 mA 21-ch; sLORETA-guided HD-tDCS), §3 hard-limits table (150 mC/cm² per session DC; 40 µC/cm² per phase charge-balanced), §4.2 (safety architecture), §2.1 (every cost figure is a floor; `OI-COST-10`); **NP-HW-TCAP-001 Rev 2 §2, §3 (`REQ-TCAP-02`), §4 (`REQ-TCAP-04`, `REQ-TCAP-05`), §7, §8** (the cap-side half of this interface, and the item this document completes); NP-FW-HD-001 Rev 6 §6.3, §6.4, §7.1, §12; NP-NPPS-REF-001 Rev 15 §4.12, §4.13; NP-DT-001 §3.2.1 DI-SAFE-01, DI-SAFE-01a; NP-SW-001 Rev 8 §5.1 SW01-M03; NP-COST-001 Rev 3 §5, **§5.1**; NP-HW-HUB-001 Rev 6 §7.4 (Hub PCB; `OI-HUB-C16`'s crosspoint residual; **`OI-HUB-C19`'s boost-placement rule**); NP-DRV-SHELL-002 Rev 2 §3.5, §4.2, §4.3 (network N4; the PAN; the one parting-plane aperture); **NP-HW-EEGNET-001 §6.2 `REQ-NET-16`**; NP-HW-TCAP-001 §4.3, §5; NP-ART-001 Rev 3 §1, §2, §6 (OI-ART-06, OI-ART-08); NP-FAI-001 Rev 3 §2; NP-CONV-001 Rev 6 §4, §5, §6; `firmware/hub_control/include/np_hub_config.h`, `np_hub_types.h`; `firmware/sloreta_hdtdcs/include/np_hd_config.h`
**Related Issues:** GitHub Issue #332 (`OI-TACS-02` — *"the 21-channel clinical tACS driver has no hardware source of truth and no costed BOM"*); `OI-TCAP-01`…`OI-TCAP-04`; `OI-TACS-01` (closed 2026-09-07, PR #318); PR #325
**Gate:** NP-COORD-001 G2 (pre-tooling) for the artifact; **G1 (PCB layout)** for §4's siting question
**IEC 62304 Class:** N/A (hardware specification). The stage sits behind the Class C enable `NP_SAFETY_EN_CLIN_STIM` and is the hardware the Class C charge monitor's two ceilings act on.
**Supersedes:** None — first issue. It removes the T2 firmware stubs and the wire format from the role of de-facto authority for a driver nobody had specified.
**Parent Document:** None

---

> **Why this document exists.** `OI-TACS-02` has two halves. Half (i) — the electrode-to-channel
> map — closed on 2026-09-09 with `NP-HW-TCAP-001` (Rev 1, now Rev 2), which specified the **cap** side of the
> interface and said in its own §8 that *"the 21-channel driver's silicon, compliance voltage and
> sense-path architecture"* were out of its scope. Half (ii) — the BOM — was **answered by a bound
> and found under-scoped** (`NP-COST-001` §5.1, `NP-HW-TCAP-001` §7.3). What has never existed is
> the thing the item's own title names first: **a hardware source of truth for the driver.**
>
> The gap is the same shape as the one `NP-HW-TCAP-001` closed on the other side of the connector.
> Since PR #318 the hub wire format has carried `channel_mask_ext` and every encoder and validator
> works in 1–21, so **the platform's software commits to 21 independently driven electrodes and no
> controlled hardware document states what those 21 channels must do.** `k_driver_channel[]` stopped
> being the authority for the *map*; nothing was ever the authority for the *driver*.
>
> **What this document does not do is select silicon.** That is `OI-TCAP-03`, it is an EE Lead /
> Design Authority decision, and §7 records why doing it inside a documentation pass would be the
> error `NP-COST-001` §5.1 declines to make. What a requirements document can do — and what has to
> exist before anyone can select a part — is state the envelope the part has to meet. §3 is that
> envelope, and §3.3 records that **one of its rows is currently unsatisfiable** for a reason
> `OI-TCAP-01` already owns.

---

## 1. Scope

**In scope, and binding:**

| § | What it pins |
|---|---|
| §2 | The artifact: what the driver stage is, and why it gets a register row of its own |
| §3 | The per-channel and aggregate electrical envelope, from both modalities that use it |
| §4 | Siting, and the interfaces that follow from it |
| §5 | Safety: one enable bit for 21 channels, and the two ceilings the Class C monitor applies |
| §6 | Verification the record already defines |
| §7 | Cost: the item's remaining half, reported and not re-derived |

**Out of scope:** the cap and its wiring (`NP-HW-TCAP-001`); HD-tDCS montage logic and sLORETA
targeting (`NP-FW-HD-001`); protocol authoring (`NP-NPPS-REF-001` §4.12, §4.13); and **silicon
selection** (`OI-TCAP-03`).

---

## 2. The artifact, and why it needs a register row

**The 21-channel driver stage is a manufactured assembly**: 21 independent current sources with
their sense paths, their compliance supply and their termination to the cap connector. It has, or
will have, its own fabrication package, its own incoming inspection and its own failure modes,
which is `NP-ART-001` §1's test for an artifact. It appears in **no row** of `NP-ART-001` §2.

That is the second instance of exactly the miss `OI-ART-06` asks about and `OI-ART-08` records for
the T2 clinical electrode cap: *an artifact absent from the source lists does not appear in the
register.* Both were found by following a citation rather than by auditing §2.

> **`REQ-TACSD-01`** — The driver stage is registered as **A16** in `NP-ART-001` §2.2, with its
> siting marked undecided (§4.1). **If `OI-TACSDRV-01` sites it on the Hub PCB, A16 folds into A9
> and the row is struck** — which is a legitimate outcome and is stated here so that the fold is a
> recorded decision rather than a quiet disappearance.

| Sub-part | What the record gives | Status |
|---|---|---|
| **A16.1 — current sources (×21)** | 21 independent channels, one per cap electrode, no sharing in either direction | Count fixed (`REQ-TCAP-02`); **no silicon** (`OI-TCAP-03`) |
| **A16.2 — per-channel sense path** | Required by `OI-TACS-02`'s own wording (*"5 additional precision current sources **plus their sense paths**"*) and by the commanded-vs-delivered cross-check `OI-FMEA-09` specifies | **No architecture** (`OI-TCAP-03`) |
| **A16.3 — compliance supply** | Nothing. See §3.2 — this is the figure the record is most conspicuously missing | **Unspecified** (`OI-TACSDRV-02`) |
| **A16.4 — cap-side termination** | `NP-HW-TCAP-001` §8: *"no connector is selected for the T2 cap"* (`OI-TCAP-05`) | **Unspecified**, and shared with the cap |
| **A16.5 — board and siting** | **Rev 2: RECOMMENDED, not decided** — its own T2-only board at the posterior aggregation node, compliance-supply magnetics on the Hub PCB (§4.1.1). Conditional on a PAN thermal budget nobody has written (`OI-TACSDRV-04`) | **`OI-TACSDRV-01`** (open) |

---

## 3. The electrical envelope

### 3.1 Two modalities, one stage

`CLAUDE.md` §3's T2 additions put both tES modalities on this stage: *"Current sourcing: 21-ch tACS
driver (already in T2) provides independently controlled channels — one per cap electrode, no
sharing — **no additional stimulation hardware**."* So the envelope is the union of the two, per
`NP-HW-TCAP-001` §4.1:

| Modality | Per-channel ceiling | Concurrency | Source |
|---|---|---|---|
| Clinical tACS | **≤ 4 mA** peak, **0.5–100 Hz**, charge-balanced biphasic, arbitrary waveform (`sinusoidal`/`square`/`triangular`) | **up to 21, independent** | `NP-NPPS-REF-001` §4.12; `CLAUDE.md` §3 |
| sLORETA HD-tDCS | **≤ 2 mA** DC, 30 s ramp | ≤ 5 (4×1 ring), 10 (bilateral) | `NP_HD_MAX_CURRENT_UA`; `NP-FW-HD-001` §7.1 |

> **`REQ-TACSD-02`** — Every channel and its sense path is rated for **4 mA continuous**, and the
> rating is **per channel and concurrent on all 21**. This is `REQ-TCAP-04` and `REQ-TCAP-05`
> restated on the driver side of the connector, and it must be restated: a driver sized on
> HD-tDCS's ≤ 5-of-21 concurrency would be sized for the wrong modality. Charge balance across a
> montage bounds the **sum**, never an individual channel.

> **`REQ-TACSD-03`** — Channel independence is **arbitrary-waveform independence**, not merely
> independent amplitude: `NP-NPPS-REF-001` §4.12 admits three waveform shapes and 0.5–100 Hz, and
> nothing constrains 21 channels to share a phase. Any architecture that derives channels from a
> single shared generator has to show it can meet the authored protocol set.

### 3.2 The figure the record does not contain: compliance voltage

Driving a defined current into an electrode requires headroom over the electrode–skin impedance.
**No compliance voltage appears anywhere in the document set**, and the input it would be derived
from is itself undetermined:

- the **cap** electrode is an Ag/AgCl sintered 3.5 mm pellet (`NP-HW-TCAP-001` §2), and no
  impedance figure is stated for it in any document;
- the auricular clip's contact window is `[500 Ω, 5000 Ω]` and cervical VNS's limit is 5 kΩ, but
  those are **different electrodes on different tissue** and neither transfers;
- 4 mA through a 5 kΩ contact is 20 V of electrode drop alone, so the answer is not obviously
  small, and the T2 hub is a USB-C PD device whose rails are specified for other purposes
  (`CLAUDE.md` §4.5).

> **`OI-TACSDRV-02`** — Specify the T2 cap electrode impedance range and the driver's compliance
> voltage together. They are one decision: compliance follows from impedance × 4 mA plus headroom,
> and **neither number exists today.** This also bounds A16.3's supply and is an input to the
> siting decision (§4.1), because a boosted rail on a head-worn node is a different proposition
> from one on the hub.

### 3.3 One row of this envelope is currently unsatisfiable, and it is not this document's to fix

`NP-HW-TCAP-001` §4.2 established that three stated limits **cannot all hold** on a 0.0962 cm²
electrode: 2 mA per electrode (L1), 6.0 A/m² tissue current density (L2) and 40 µC/cm² per phase
(L3). 2 mA is 34.6× L2; the 4 mA tACS ceiling exceeds L3 by 660× at 0.5 Hz and 3.3× at 100 Hz,
because charge density is frequency-dependent and **no constant in the system expresses that
dependence.**

> **`REQ-TACSD-04`** — The driver is specified to **4 mA** because that is the declared modality
> ceiling and a conductor or a source rated below it would foreclose a capability the platform
> claims. **It is not a statement that 4 mA is permissible into a 3.5 mm pellet.** `OI-TCAP-01`
> owns that question — its three resolutions are a cap redesign, a capability reduction and a
> safety-basis change — and `NP-HW-TCAP-001` is explicit that **it must not reach a cap drawing.**
> The same applies here: it must not reach a silicon selection either. **If `OI-TCAP-01` resolves
> by capability reduction, `REQ-TACSD-02`'s 4 mA falls with it** and this envelope is re-derived.

---

## 4. Siting and interfaces

### 4.1 Where the stage goes is undecided, and it is a real decision

Three candidate sitings exist in the record, none chosen:

| Candidate | What favours it | What it costs |
|---|---|---|
| **Own T2 board** | Keeps a 21-channel analog stage off a board (`NP-HW-HUB-001` Rev 4) that `OI-HUB-C01…C19` already leaves undesigned; makes A16 a clean artifact with its own FAI | A board, an enclosure position and an interconnect |
| **Hub PCB Rev C** | One board; the safety enable and the charge monitor already live hub-side | Folds 21 analog channels into a board with nineteen open items; A16 is struck and A9 absorbs the risk |
| **Posterior aggregation node** | Where `NP-DRV-SHELL-002` network N4 already terminates the **EEG** side, *"sized by channel count (8 T1 / 21 T2)"* | Puts a 4 mA × 21 source on a head-worn node; and `OI-HUB-C16`'s residual — **tES current rating through the same analog crosspoint** — is unresolved and sits with `NP-DRV-SHELL-002`. ***Rev 2: the residual does not attach to this stage — §4.1.1 (b).*** |

> **`OI-TACSDRV-01`** — Site the stage. It is a **G1 (PCB layout)** decision and it changes which
> artifact row owns the hardware, which risk register holds its hazards, and whose FAI inspects it.
> Note the third candidate has an open safety-relevant question already attached to it
> (`OI-HUB-C16`'s residual), which is a reason to decide it explicitly rather than by default.

### 4.1.1 Siting analysis and recommendation (Rev 2)

**Status: RECOMMENDED, not decided.** `OI-TACSDRV-01` is owned by the EE Lead and ME and sits at
G1. This subsection does the analysis the item asks for, from the record alone, and **stops short of
taking the decision**, the same way `NP-EMC-CAV-001` `REQ-CAV-04` does. Nothing here changes a locked
decision, a firmware constant or a cost figure.

**(a) The three candidates mix two questions, and splitting them is most of the answer.** "Own
board" is an **artifact boundary**. "Hub PCB" and "posterior node" are **locations**. An own board can
sit at either location, so Rev 1's table offered three answers to two questions. Taken separately:

| Question | Options | Recommendation |
|---|---|---|
| **Q1. Artifact.** Is A16 its own board, or part of A9? | own T2 board · fold into A9 | **Own T2-only board. A16 keeps its row.** |
| **Q2. Where do the 21 current sources and sense paths sit?** | Hub PCB (outside the shield) · PAN (inside the shield, where the cap tail lands) | **At the PAN** |
| **Q3. Where does the compliance supply's switching stage sit?** | Hub PCB · PAN | **On the Hub PCB**, following `OI-HUB-C19`'s rule. **Or nowhere**, if `OI-TACSDRV-02` gives a compliance voltage an existing rail can supply |

**(b) Rev 1's objection to the posterior node does not apply to this stage.** Rev 1 flagged
`OI-HUB-C16`'s residual, the tES current rating of the N4 analog crosspoint, against the PAN option.
That crosspoint is the **per-cluster mux on the shell sockets' `ELEC` contacts** (`NP-DRV-SHELL-002`
§3.5). **The T2 cap does not go through it.** `NP-HW-TCAP-001` §5 binds the cap to
`NP-HW-EEGNET-001` §6.2, and **`REQ-NET-16`** in that section terminates the tail at the PAN and
routes it through ***"no pogo contact and no analog mux."*** `REQ-TCAP-02`'s identity map drives
cap electrodes, not socket contacts. So the residual is still a live question, but it belongs to
**T1-B/socket tES**, and it would attach here only if T2 stimulation were ever routed through socket
`ELEC` contacts. Nothing in the record does that.

> **A record conflict found on the way, recorded here and not resolved.** `NP-DRV-SHELL-002` §3.5
> still sizes N4 at *"~21 scalp for T2"* through the socket mux. `NP-HW-TCAP-001` and
> `NP-HW-EEGNET-001` route the T2 electrodes on a cap tail that bypasses N4 entirely. Both can be
> true only if N4's T2 sizing is for a socket-borne T2 electrode set that no document specifies.
> This is raised as **`OI-TACSDRV-05`** because it is SHELL-002's to reconcile, not this document's.

**(c) Q2 — at the PAN, for the reason the ADS1299 is there.** `NP-HW-HUB-001` §7.4 accepted the
ADS1299 bank at the PAN because it *"keeps the µV electrode lanes from crossing the parting-plane
boss."* The drive lanes are the same conductors, going the other way. `REQ-NET-16` puts the cap
tail's termination at the PAN in either case, so:

| | Sources on the Hub PCB | **Sources at the PAN** |
|---|---|---|
| What crosses the boss for this stage | **21 analog drive lanes** at up to the compliance voltage, carrying 0.5–100 Hz stimulation current. That band is the EEG band (`NP-DRV-SHELL-002` §9.2), and it would share the aperture with the ADS1299's digital group | A compliance-supply pair, the `NP_SAFETY_EN_CLIN_STIM` line and a digital control/sense link |
| Boss contact groups (`NP-DRV-SHELL-002` §4.3 lists four, segregated) | A **fifth**, analog group, in the one aperture §4.3 is trying to keep small | No new analog group |
| Drive-lane length exposed to T2 TMS dB/dt | Tail **plus** the PAN-to-hub run outside the shield | Tail only |
| Where the cap connector (`OI-TCAP-05`) and A16.4 meet | On different boards, with the boss between them | **On the same board.** A16.4 and the cap-side connector become one interface |

The magnetic-field argument is **neutral**. The current sources are linear stages with no magnetics,
and the stimulation current flows through the tail and the head under either siting. What moving
the sources changes is the path the current takes, not how much of it there is.

**(d) Q3 — switching magnetics stay outside the shield.** `OI-HUB-C19` put the 24 V vault boost on
the Hub PCB for three reasons. Two of them transfer directly: **magnetics stay away from the
inner-bowl fluxgates**, whose fluxgate/coil harness boss is co-sited with the PAN
(`NP-DRV-SHELL-002` §4.2), and **conversion loss lands on the fan-served side**. The recommendation
follows the precedent and does not add to it. **Whether any new switching stage is needed at all
depends on `OI-TACSDRV-02`.** The PAN is already the vault's 24 V PDN feed point, but a compliance
voltage above roughly 20 V (24 V less the sources' headroom) rules that rail out. The one impedance figure in the record is
`NP-HW-TCAP-001` §4.3's **≤ 10 kΩ at 1 kHz** abort threshold, and 4 mA × 10 kΩ is **40 V**. That is
an abort threshold measured at 1 kHz, not the load at 0.5 Hz, so it does not close `OI-TACSDRV-02`.
It does mean the 24 V rail cannot be assumed to be enough.

**(e) Q1 — own board, not A9.** Three reasons, and none of them depends on Q2's answer:

1. **T1 would carry a T2-only stage.** `CLAUDE.md` §1 runs both tiers on one production line, and
   §2.1 records every T1 configuration as gross-margin negative. Board area on a shared Hub PCB is
   paid by every T1 unit, whether as populated parts, depopulated footprint or enclosure volume. The
   alternative is a T2 variant of A9. A separate T2-only board puts the cost on T2 only, and leaves
   T1 with a connector footprint. No figure is stated here: `NP-COST-001` §5.1 bounds the stage but
   cannot price it.
2. **Q2 puts the sources at the PAN, and the Hub PCB is not at the PAN.** Folding into A9 would
   decide Q2 the other way, and (c) gives the reasons against that.
3. **A9 already has 19 open items** (`OI-HUB-C01…C19`). A16 as its own artifact can get its own FAI
   and its own fabrication package without waiting on them.

Consequence for `REQ-TACSD-01`: **A16's row stays, and the fold into A9 is not recommended.** One
note on the host: **the PAN itself has no artifact row.** It hosts the ADS1299 bank, the PDN feed
and *"the tES driver interface"* (`NP-DRV-SHELL-002` §4.2), and none of those makes it an entry in
`NP-ART-001` §2. That makes it the third instance of the `OI-ART-06` miss. It is recorded against
`OI-TACSDRV-03`, not raised as a new item.

**(f) What would overturn this: heat at the PAN.** A linear current source dissipates
(V_c − I·Z)·I, which is largest into a low-impedance electrode. The worst case for the stage is
therefore about **V_c × 84 mA** (21 × 4 mA, concurrent per `REQ-TACSD-02`). At the 40 V in (d),
that is **~3.4 W**, delivered at the occiput, inside the shielded envelope where the 42 °C scalp
limit applies. **No thermal document in the set addresses the PAN**
(`NP-THERM-SINK-001` covers the tile field and the hub heatsink). The Hub PCB has a fan. So the
recommendation is conditional. **If the PAN cannot reject V_c × 84 mA inside the scalp limit, and a
tracking supply or a V_c reduction from `OI-TACSDRV-02` does not bring the figure down, Q2 falls to
the Hub PCB and the boss has to carry 21 analog lanes.** That is **`OI-TACSDRV-04`**. It is
time-boxed for the same reason as `NP-DRV-SHELL-002` §4.3: the boss contact layout is tooled at
**MECH-1**, so a hub fallback discovered after MECH-1 means re-tooling the boss.

**(g) What this analysis assumed without saying so: that T2 is always built as T2.** The record
treats a T1 → T2 upgrade as something that will happen. `NP-THERM-COOL-001` Rev 6 aligned the T1
block temperature so that *"a customer upgrading T1 → T2 meets the same usage limit,"* and
`CLAUDE.md` §2.2 includes the charger at every upgrade by serial-number tracking. **But no document
says how the upgrade is done**, and the answer changes Q1 and Q3:

| Upgrade model | Effect on this recommendation |
|---|---|
| **New T2 unit** (trade-in or second purchase) | None. (e)1's "T1 carries a connector footprint" holds as written |
| **Service conversion of the customer's T1 headset** | Every T1 unit must be **provisioned** to accept A16: a mounting position and connector at the PAN, the cap connector's shell opening, and the digital link and `NP_SAFETY_EN_CLIN_STIM` positions in the boss. The PAN is inside the shielded envelope under the boss, so fitting the board means opening the shell, which is heavier than the Tier B visit that `docs/reference/service-network.md`'s lever ZIF connectors were meant to avoid. The stack is aperture-limited (`NP-BIB-EMF-001`), so each conversion also needs its seams re-verified. **Q1 still holds, but (e)1's saving shrinks.** **Q3 weakens:** hub-side magnetics mean a conversion touches two assemblies, or every T1 hub carries the supply populated |
| **User-installed** | Not realistic at the PAN. Only snap-in tiles and the lens are user-serviceable |

Separately from the hardware: fitting 510(k)-target hardware into a unit sold as an FDA-exempt
wellness device changes what the device is, including its labelling, its identity and which
configuration was cleared. `docs/reference/regulatory-strategy.md` does not address it. This is
**`OI-TACSDRV-06`**, and `OI-TACSDRV-01` should not be decided before it.

> **`OI-TACSDRV-01` stays OPEN.** Its question is now three narrower ones with a recommendation on
> each (Q1 own board; Q2 PAN; Q3 hub-side magnetics or none). Its closure path is: **`OI-TACSDRV-06`
> fixes the upgrade model**, the principal takes or rejects the recommendation in its light, and
> Q2's condition is discharged by `OI-TACSDRV-04`.

### 4.2 Firmware and wire interface — fixed since PR #318

| Parameter | Value | Source |
|---|---|---|
| Channel count | **21**, `NP_HD_DRIVER_CHANNELS` = `NP_CLIN_TACS_CHANNELS` = 21, asserted equal in `np_protocol_tests.c` | `np_hd_config.h`; `np_hub_types.h` |
| Channel map | **Identity** — electrode *i* ↔ channel *i* | `NP-HW-TCAP-001` `REQ-TCAP-02` |
| Wire format | `np_mod_clin_tacs_params_t`, **8 bytes**: `channel_mask_lo` (0–7), `channel_mask_hi` (8–15), `channel_mask_ext` (16–20, bits 5–7 reserved and written clear), `waveform` at offset 7 | `np_hub_types.h`; `NP-NPPS-REF-001` §4.12 |
| Validation | `channel_count` **validated, not clamped**, 1–21, in all three runtimes | `NP-NPPS-REF-001` §4.12 |
| Safety enable | `NP_SAFETY_EN_CLIN_STIM` = bit 13 — **one bit covering both clinical tACS and HD-tDCS** | `np_hub_config.h`; §5 |
| Runtime distinctness check | `np_hd_montage_validate()` re-checks electrode→channel distinctness across both hemispheres | `NP-FW-HD-001` §6.3 |
| CI gate | `scripts/check-tcap-map.ts` diffs the map document against the firmware tables and both channel-count constants | `NP-HW-TCAP-001` §6 |

> **`REQ-TACSD-05`** — The three mask bytes' boundaries at 8 and 16 are **wire-format artifacts
> with no anatomical or electrical meaning** (`NP-HW-TCAP-001` §2). No driver architecture may
> group channels 0–7 / 8–15 / 16–20 on the grounds that the mask does — for example by putting one
> supply or one sense amplifier per byte. The grouping is an accident of a `uint8_t`.

---

## 5. Safety

### 5.1 One enable bit for 21 channels

`NP_SAFETY_EN_CLIN_STIM` gates the whole stage — both modalities, all 21 channels — and the safety
MCU's `NP_SAFETY_MAX_CHANNELS` (14) counts enable lines, not electrodes, so the driver is **one**
of them (`NP_SAFETY_CH_CLIN_STIM`).

This is a coarse gate by design, and the design has a precedent that should be named rather than
rediscovered: `OI-HUB-C07` decided that the cranial PBM safety enable is **one Class C policy bit**,
with per-cluster gates retained as IEC 62304 **Class B availability gates in series with it**
(`NP-HW-HUB-001` Rev 4 §7.2.1).

> **`REQ-TACSD-06`** — Any per-channel disable the driver provides is a **Class B availability
> gate**, not a safety control, and must be specified as one. The Class C cutoff is
> `NP_SAFETY_EN_CLIN_STIM` and it is all-or-nothing. A design that presented per-channel disables
> as the safety mechanism would be the inversion `OI-HUB-C07` settled against.

### 5.2 Two ceilings on one bit

Because both modalities share the enable, the Class C charge monitor runs **both** checks on this
channel (`NP-SW-001` §5.1 SW01-M03, `OI-CHARGE-05`):

- **DC (HD-tDCS):** accumulate commanded charge per electrode, cut at **150 mC/cm² × declared
  area** (DI-SAFE-01);
- **charge-balanced (clinical tACS):** per-heartbeat predicate on **charge per phase**, cut at
  **40 µC/cm² × declared area** (DI-SAFE-01a) — no session integral, because net charge is ~zero;
- **fail-closed:** an electrical channel whose waveform class was never declared is held out of
  `granted_mask`.

> **`REQ-TACSD-07`** — The driver must make its waveform class **declarable per session**, because
> the two ceilings are chosen by declaration and an undeclared channel is refused. A driver mode
> that blurs DC and AC on the same channel within one session has no admissible ceiling and is
> therefore not buildable against this safety architecture.

### 5.3 Commanded, never measured

Both ceilings are **commanded-dose** limits — what the protocol asked for, never an ADC reading —
and `NP-SW-001` Rev 8 records the reason as **signature independence, not privacy**: a measured
value cannot be signed in advance, so a Class C cutoff keyed to one would inherit Class B
integrity.

> **`REQ-TACSD-08`** — A16.2's sense path therefore exists for **`OI-FMEA-09`'s
> commanded-versus-delivered cross-check and its SHDR divergence flag**, not to feed the interlock.
> Specifying it as the interlock's input would undo the decision above. Its accuracy requirement
> follows from the divergence threshold `OI-FMEA-09` sets, which is **not yet set**.

---

## 6. Verification the record already defines

Nothing new is created here; these are the existing items that will inspect A16 once it exists.

| Item | What it checks | Where |
|---|---|---|
| `scripts/check-tcap-map.ts` | Document-to-firmware agreement on the map and both channel-count constants, in CI | `NP-HW-TCAP-001` §6 |
| `np_protocol_tests.c` | `NP_CLIN_TACS_CHANNELS == NP_HD_DRIVER_CHANNELS`, 8-byte struct size, every field offset | `OI-TACS-01` closure record |
| `np_hd_montage_validate()` | Electrode→channel distinctness at run time, across both hemispheres | `NP-FW-HD-001` §6.3 |
| FAI-HD01 (bench limb), HD03, HD04 | Inspect the **cap**, and have no nameable artifact checklist — the cap has no register row (`OI-FAI-06`, `OI-ART-08`, `OI-TCAP-06`) | `NP-FW-HD-001` §12 |

**No FAI item anywhere inspects the driver.** `NP-FAI-TACSDRV-001` is named here for the first
time, as a **named absence** under `NP-FAI-001` §2's closing rule, and it fails F1 (this document is
DRAFT), F2 (no silicon, no compliance voltage, no dimension) and F4 (no risk register entry).

---

## 7. Cost — the item's other half, reported not re-derived

`CLAUDE.md` §2.1 forbids quoting or acting on a cost figure without first reading
`docs/np_cost_001.md`; it has been read, and nothing below revises it.

`OI-TACS-02` half (ii) is **answered and re-scoped**, in `NP-COST-001` §5.1 and
`NP-HW-TCAP-001` §7. In summary, and without re-deriving:

- it **cannot be priced** — the 2026-08-05 decision records *"no sourcing document, no BOM line,
  and no named silicon,"* the analog counterpart of `OI-HEXTILE-02`;
- it **is bounded** — a **$305/channel** materiality threshold (Pro Entry binds) against a
  record-derived **≤$26.75/channel** ceiling, a factor of 11, so the delta cannot flip either Pro
  row; **stated falsifiably**, and selection above $305/channel reopens it;
- it is **under-scoped** — if the 16-channel baseline never had a BOM line, the uncosted quantity
  is the whole 21-channel stage, and the conclusion then survives on the **threshold** alone
  (`OI-TCAP-04`);
- the **larger uncosted term is next door** — `OI-EEGNET-07`'s 23-vs-44 conductor cap tail, for
  which `NP-COST-001` §2 A-1 has no row at all.

> **`REQ-TACSD-09`** — **This document sets no price and names no silicon**, and the sequence is
> the reason: `OI-TCAP-03` (name the silicon) must come before the bound can become a price, and
> `CLAUDE.md` §2.1's `OI-COST-10` sequencing constraint is untouched by anything here. What this
> document supplies is the **envelope a selection must meet** (§3), which is the input
> `OI-TCAP-03` has been missing — and §3.2 records that the selection additionally needs a
> compliance voltage that does not yet exist.

**What `OI-TACS-02` needs to close, stated plainly:** `OI-TCAP-03` (silicon named against §3's
envelope), `OI-TACSDRV-02` (electrode impedance and compliance voltage), `OI-TACSDRV-01` (siting),
and `OI-TCAP-04` (the cost half re-scoped to the 21-channel stage). The half the item's title names
first — *no hardware source of truth* — is what this document supplies.

---

## 8. What this document does **not** specify, and why

| Not specified | Why | Where it goes |
|---|---|---|
| Driver silicon and topology | EE Lead / Design Authority selection; naming one here would be a costing dressed as a specification | **`OI-TCAP-03`** |
| Compliance voltage and cap electrode impedance | §3.2 — one decision, neither number exists | **`OI-TACSDRV-02`** |
| Siting (own board / hub PCB / posterior node) | §4.1 — a G1 decision that changes which artifact and which register own it. **Rev 2: analysed and recommended in §4.1.1, not decided** | **`OI-TACSDRV-01`**; `OI-TACSDRV-04` |
| Sense-path accuracy | §5.3 — follows from `OI-FMEA-09`'s divergence threshold, which is unset | **`OI-FMEA-09`** |
| Whether 4 mA into a 3.5 mm pellet is permissible | §3.3 — three mutually unsatisfiable limits; must not reach a silicon selection any more than a cap drawing | **`OI-TCAP-01`** |
| Connector to the cap | Unselected on the cap side too | **`OI-TCAP-05`** |
| Board area and the tES rating of the analog crosspoint | `NP-HW-TCAP-001` §7.5 records these as **not bounded** by its cost argument; the crosspoint residual sits with `NP-DRV-SHELL-002` | **`OI-HUB-C16` residual** |

---

## 9. Open items

| ID | Description | Owner | Blocking |
|---|---|---|---|
| **OI-TACSDRV-01** | **Site the 21-channel driver stage** — own T2 board, Hub PCB Rev C, or the posterior aggregation node (§4.1). It decides which artifact row owns the hardware (A16 or A9), which risk register holds its hazards, and whose FAI inspects it. **Rev 2 — ANALYSED, RECOMMENDED, NOT DECIDED (§4.1.1).** The item is split into three questions. **Q1 artifact: own T2-only board, and A16 keeps its row.** **Q2 sources and sense paths: at the PAN**, where `REQ-NET-16` lands the cap tail, so the 21 drive lanes never cross the parting-plane boss. **Q3 compliance-supply magnetics: on the Hub PCB** per `OI-HUB-C19`, or none if `OI-TACSDRV-02` allows an existing rail. Rev 1's crosspoint objection to the PAN **does not attach**, because the T2 cap bypasses N4 (§4.1.1 (b)). Closes when **`OI-TACSDRV-06` fixes the T1 → T2 upgrade model** (which §4.1.1 (g) shows changes Q1's saving and Q3), the principal takes the recommendation, **and** `OI-TACSDRV-04` discharges Q2's thermal condition | EE Lead + ME | **G1 PCB layout**; **`OI-TACSDRV-06`**; `NP-ART-001` A16; **MECH-1** (boss contact layout, via `OI-TACSDRV-04`) |
| **OI-TACSDRV-02** | **Specify the T2 cap electrode impedance range and the driver's compliance voltage — one decision, neither number exists anywhere** (§3.2). 4 mA into a 5 kΩ contact is 20 V of electrode drop alone, and the T2 hub is a USB-C PD device. This is a precondition for `OI-TCAP-03`, not a consequence of it | EE Lead + Clinical | `OI-TCAP-03`; A16.3 supply |
| **OI-TACSDRV-03** | Register **A16** in `NP-ART-001` §2 and give it a per-artifact risk register once `OI-TACSDRV-01` decides whether it is its own artifact or part of A9. Second instance of the `OI-ART-06` miss, after `OI-ART-08`'s cap. **Rev 2:** if §4.1.1's recommendation is taken, A16 stays its own artifact. Its host, the **PAN**, is also missing from `NP-ART-001` §2, which makes a **third** instance, and it is recorded here rather than raised separately | Systems | `NP-FAI-TACSDRV-001` F4 |
| **OI-TACSDRV-04** | **The PAN has no thermal budget, and §4.1.1's Q2 recommendation depends on one.** Worst-case source dissipation is about **V_c × 84 mA** (~3.4 W at the 40 V that 4 mA into `NP-HW-TCAP-001` §4.3's 10 kΩ abort threshold implies), at the occiput, inside the shield. It needs (i) a PAN rejection figure against the 42 °C scalp limit and (ii) V_c from `OI-TACSDRV-02`, or a tracking-supply architecture from `OI-TCAP-03`. **If the PAN cannot reject the dissipation, Q2 falls to the Hub PCB** and the parting-plane boss must carry 21 analog drive lanes. **Time-boxed:** it must be answered, or the boss contact layout provisioned for the hub fallback, before **MECH-1** tools the boss | Thermal + EE Lead | `OI-TACSDRV-01`; **MECH-1** |
| **OI-TACSDRV-05** | **`NP-DRV-SHELL-002` §3.5 sizes N4 at "~21 scalp for T2" through the socket mux, but `NP-HW-TCAP-001` §5 / `NP-HW-EEGNET-001` `REQ-NET-16` route the T2 electrodes on a cap tail that bypasses N4.** Either N4's T2 sizing refers to a socket-borne T2 electrode set that no document specifies, or it is stale. Found while doing the §4.1.1 analysis. SHELL-002's to reconcile | ME + EE Lead | `NP-DRV-SHELL-002` N4 sizing; `OI-HUB-C16` residual scope |
| **OI-TACSDRV-06** | **Define the T1 → T2 upgrade path. The record assumes one exists and never specifies it** (§4.1.1 (g)). `NP-THERM-COOL-001` Rev 6 aligned the T1 thermal block *"so a customer upgrading T1 → T2 meets the same usage limit,"* and `CLAUDE.md` §2.2 includes a charger at every upgrade, but no document says whether T2 is a **new unit**, a **service conversion** of the customer's T1 headset, or **user-installed parts**. The answer decides (i) what every T1 unit must be provisioned with for A16, including the PAN mounting and connector, the cap connector's shell opening and the boss positions; (ii) whether §4.1.1 Q3's hub-side magnetics survive, since a conversion would touch two assemblies; (iii) whether a conversion that opens the shielded envelope needs its seams re-verified, and by which service tier; and (iv) the **regulatory status of a wellness unit converted to a 510(k) configuration**, which `docs/reference/regulatory-strategy.md` does not address. It reaches beyond A16: every T2 addition that lives in the shell or hub faces the same question | Product + Regulatory + ME | **`OI-TACSDRV-01`**; T1 shell and hub provisioning |

> Items owned elsewhere and deliberately not duplicated: **`OI-TCAP-01`** (the three unsatisfiable
> limits), **`OI-TCAP-03`** (silicon), **`OI-TCAP-04`** (cost re-scope), **`OI-TCAP-05`**
> (connector), **`OI-TCAP-06`** / **`OI-ART-08`** (the cap's register row), **`OI-EEGNET-07`** (cap
> tail conductor count), **`OI-FMEA-09`** (commanded-vs-delivered cross-check).

---

## 10. Cross-references

- **The cap side of this interface:** `docs/np_hw_tcap_001.md`
- **Cost position:** `docs/np_cost_001.md` §5, §5.1 — read before quoting any figure
- **Charge ceilings:** `docs/np_dt_001.md` §3.2.1; `docs/np_sw_001.md` §5.1
- **Montage and targeting firmware:** `docs/np_fw_hd_001.md`
- **Protocol grammar:** `docs/np_npps_ref_001.md` §4.12, §4.13
- **Artifact register:** `docs/np_art_001.md` §2.2 (A16), §6
- **FAI issue conditions:** `docs/np_fai_001.md` §2

---

## 11. Revision history

| Rev | Date | Author | Description |
|---|---|---|---|
| 2 | 2026-09-23 | NeurOne Systems Engineering | **`OI-TACSDRV-01` analysed and recommended, not decided (§4.1.1).** Rev 1's three candidates mixed two questions: own board is an artifact boundary, and hub vs PAN are locations. Split three ways, the recommendation is: **Q1: own T2-only board**, so A16 keeps its row and does not fold into A9. That keeps a T2-only stage off a Hub PCB every gross-margin-negative T1 unit pays for, and off a board with 19 open items. **Q2: sources and sense paths at the PAN**, where `REQ-NET-16` already lands the cap tail. The 21 in-band drive lanes then never cross the one parting-plane aperture, which is the ADS1299's reason for being there, applied to the same conductors going the other way. **Q3: compliance-supply magnetics on the Hub PCB**, following `OI-HUB-C19`'s rule, or no new stage if `OI-TACSDRV-02` allows an existing rail. **Corrects Rev 1:** `OI-HUB-C16`'s crosspoint residual does not attach to the PAN option, because the T2 cap bypasses N4 (`REQ-NET-16`: *"no pogo contact and no analog mux"*). Records that the only impedance figure in the record, `NP-HW-TCAP-001` §4.3's 10 kΩ at 1 kHz abort threshold, implies 40 V at 4 mA, so the PAN's 24 V rail cannot be assumed. Raises **`OI-TACSDRV-04`**: the PAN has no thermal budget, worst-case dissipation is about V_c × 84 mA (~3.4 W at 40 V), and it is the recommendation's falsifier, time-boxed to MECH-1. Raises **`OI-TACSDRV-05`**: SHELL-002's N4 T2 sizing conflicts with the cap-tail routing. Notes the PAN has no `NP-ART-001` row, a third `OI-ART-06` instance, against `OI-TACSDRV-03`. **§4.1.1 (g) records what the analysis assumed without saying so: that T2 is always built as T2.** A service conversion of a T1 headset shrinks Q1's saving, weakens Q3 and raises a regulatory question. Raises **`OI-TACSDRV-06`** (define the T1 → T2 upgrade path) and makes `OI-TACSDRV-01` depend on it. **`OI-TACSDRV-01` stays open. No silicon, price, cost figure, firmware constant or locked decision changes.** |
| 1 | 2026-09-20 | NeurOne Systems Engineering | Initial release, against GitHub #332's `OI-TACS-02` task. **Supplies the half of `OI-TACS-02` its own title names first and neither prior closure touched: a hardware source of truth for the driver.** Half (i) closed with `NP-HW-TCAP-001` (the cap-side map) and half (ii) was answered with a bound and re-scoped (`NP-COST-001` §5.1); since PR #318 the wire format, every encoder and every validator commit to 21 independently driven electrodes, **and no controlled hardware document stated what those channels must do.** §3 states the envelope as the union of both modalities that share the stage — **4 mA per channel concurrent on all 21**, 0.5–100 Hz arbitrary waveform, against HD-tDCS's ≤ 2 mA DC at ≤ 5-of-21 concurrency — and records that **channel independence means waveform independence** (`REQ-TACSD-03`). **§3.2 names the figure the record most conspicuously lacks: no compliance voltage exists anywhere, and neither does a T2 cap electrode impedance, which is the input it derives from** (`OI-TACSDRV-02`, a precondition for `OI-TCAP-03` rather than a consequence of it). **§3.3 states that the envelope's 4 mA row is currently unsatisfiable** on a 0.0962 cm² electrode per `NP-HW-TCAP-001` §4.2, that the driver is specified to it anyway so as not to foreclose a claimed capability, and that — exactly as that document rules for a cap drawing — **it must not reach a silicon selection**; if `OI-TCAP-01` resolves by capability reduction the envelope is re-derived. §4.1 sets out three candidate sitings and requires the choice to be made rather than defaulted, noting the posterior-node option inherits `OI-HUB-C16`'s unresolved tES rating through the analog crosspoint (`OI-TACSDRV-01`). §5 records that **one Class C bit gates all 21 channels**, names the `OI-HUB-C07` precedent, and requires any per-channel disable to be specified as a **Class B availability gate** (`REQ-TACSD-06`); §5.3 records that the sense path exists for `OI-FMEA-09`'s cross-check and **not** as the interlock's input, because the interlock is commanded-dose by decision. §2 registers the stage as **A16** — the second instance of the `OI-ART-06` miss after `OI-ART-08`'s cap — with the fold into A9 stated as a legitimate outcome if it is sited on the Hub PCB. **No silicon is named, no price is set, no cost figure is revised, `OI-COST-10`'s sequencing is untouched, and no locked decision changes.** Raises OI-TACSDRV-01…03. |
