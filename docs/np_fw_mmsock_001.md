# Multi-Modality Drive at One Hex-Tile Socket — Design Study

**Project:** NeurOne
**Document:** NP-FW-MMSOCK-001
**Revision:** 2
**Date:** 2026-09-23
**Status:** DRAFT
**Effective Date:** —
**Author:** NeurOne Firmware Engineering
**Approved By:** Principal — P-1…P-6 (§7.2) accepted as recommended, 2026-09-23. Class C change C-1 implemented, **pending SW-01 review**
**References:** CLAUDE.md §3 (modality roster and hard limits), §4.2 (safety architecture), §5 (UHDR/SHDR), §18 (a requirement must be required); `NP-FW-HUB-001` Rev 2 §3.4 (socket dispatch registry), §4.3 (target block), §4.5 (versioning), §5.4 (electrode geometry hand-off), §5.6 (power gate), §7.4 (geometry gates), §10; `NP-HW-HEXTILE-001` Rev 11 §3, §4.5 (T1-B), §7.2 (pinout — pin 17 `ELEC`), §8.4 (D-8), §9.3 (`OI-HEXTILE-09`); `NP-HW-HUB-001` Rev 6 §7.2 (one `NP_SAFETY_EN_PBM_CRANIAL` bit), §7.2.2 (`HUB-REQ-C05`), §7.3; `NP-DRV-SHELL-002` Rev 4 §3.5 (electrode network N4), §6 (tES path interlock), §9.1 (`SH2-DRC-16`), REQ-EMI-03/04/08; `NP-HW-EEGNET-001` Rev 8 §0, §1.9.1 (pod diameter), §5, §5.5 (`REQ-NET-10`); `NP-HEX-ZM-001` Rev 3 §4a (tile taxonomy); `NP-DT-001` Rev 3 §3.2.1 (DI-SAFE-01 / DI-SAFE-01a); `NP-SES-PWR-001` Rev 1; `NP-RISK-003` Rev 1; `NP-NPPS-REF-001` Rev 16; `NP-CONV-001` Rev 8 §4, §5, §7.1; `firmware/hub_control/src/np_socket_dispatch.c`, `include/np_module_map.h`, `include/np_hub_types.h`, `src/np_session_runner.c`; `firmware/safety_mcu/src/np_charge_monitor.c`; `firmware/common/include/np_spi_wire_types.h`; `app/web/src/lib/hubCompiler.ts`; `protocols/predefined/`
**Related Issues:** PR #377 (the socket dispatch registry this study builds on; `OI-FWHUB-01`)
**Gate:** N/A — design study; its outputs gate `OI-FWHUB-09`'s successor work and any tES-by-socket wire change
**IEC 62304 Class:** SW-02 Class B for everything this document changes. **It changes no Class C code and no Class C wire format.** §5.3 states the SW-01 (Class C) review that any adoption of §5 would require.
**Supersedes:** None
**Parent Document:** `NP-FW-HUB-001`

---

> **Rev 2 (2026-09-23) — the principal accepted all six recommendations (§7.2), and one is built.**
>
> | | Decision | What happened |
> |---|---|---|
> | **P-1** | T1 tES on conventional **pads outside the lattice**; the T1-B electrode is recording-first, and stimulates only for **tACS where a pod fits the per-phase ceiling** (≳ 8–10 Hz at 1 mA) | `OI-MMSOCK-01` **closed**. No new frequency rule is written: the Class C per-phase ceiling, enforced against a pod's own area, already refuses the rest (CLAUDE.md §18). **Consequence found:** no T1 tES pad hardware is specified anywhere — `OI-MMSOCK-11` — **ACCEPTED 2026-09-23** |
> | **P-2** | tES gets a socket target **after P-1** — now satisfied, and scoped by it to tACS on the T1-B dual-rated pad | not built; `OI-MMSOCK-10` — **ACCEPTED 2026-09-23** |
> | **P-3** | PBM + EEG at a T1-B: **allow** | already the behaviour; the pinning test's comment now cites the decision — **ACCEPTED 2026-09-23** |
> | **P-4** | PBM + tES at a T1-B: **allow** | moot until `OI-MMSOCK-10` — **ACCEPTED 2026-09-23** |
> | **P-5** | Class C change C-1 **now** | **built** — see below; `OI-MMSOCK-02` implemented, closes on SW-01 review — **ACCEPTED 2026-09-23** |
> | **P-6** | **Two electrodes per T1-B**, one recording-only, one dual-rated | recorded; propagation to the socket, tile and net specifications is `OI-MMSOCK-12`, **before socket tooling** — **ACCEPTED 2026-09-23** |
>
> **C-1 as built differs from §5.3 in one respect, on purpose.** §5.3 and `OI-MMSOCK-02` proposed an
> authored `electrode_area_mcm2` in `np_mod_bes_tacs_params_t` (wire v4). Building it found that the hub
> already holds the BES pad area as a **fixed device constant**, `NP_BES_ELECTRODE_AREA_MCM2`, by a
> documented rule (`OI-CHARGE-07`: a fixed product part's area is a device property; a user-chosen
> consumable's is authored). P-1 keeps T1 tES on those pads, so the rule still applies and an authored
> field would have contradicted it. **What was built:** `NP_SESSION_STATUS_GEOM_REQ_BES` (bit 4) and a
> third safety-MCU gate arm, fail-closed; the hub arms it for every BES/tACS session and sends the pad
> constant. **What was not:** the wire field. A lattice electrode's area arrives with the tES socket
> target (`OI-MMSOCK-10`), which is where an area that is not the pad's first exists.
>
> **§3.6.1 was imprecise, and is corrected here rather than rewritten.** It said BES/tACS is enforced
> against the MCU's `NP_ELECTRODE_AREA_CM2` default. The hub computes its own 25 cm² constant — but
> **sent it only in sessions that also held tDCS or HD-tDCS**, so in every other session the MCU's
> default applied. Both are 25 cm², so §3.6.1's arithmetic stands. The same send rule dropped the VNS
> (0.5 cm²) and cervical-VNS (2 cm²) areas, which do differ: **a VNS-only session was enforced 50× looser
> than designed.** Fixed in this change (`NP-FW-HUB-001` Rev 3, `REQ-FWHUB-37`, `RISK-FWHUB-15`).
>
> ---
>
> **Summary — read this if nothing else.**
>
> The request was to let the helmet drive more than one modality on one hex-tile socket at the same
> time, or to establish, with reasons, which combinations it must not. The answer is shorter than the
> question suggests. **A socket has exactly two independent physical resources — the emitter drive and
> the one `ELEC` conductor — and every combination question reduces to what may share each.**
>
> | Combination at one socket | Verdict | Reason, and where it is traceable |
> |---|---|---|
> | Several PBM wavelengths | **Supported today** | one on-module driver command (`NP-FW-HUB-001` §3.4) |
> | PBM + EEG recording (T1-B) | **Recommended: allow** | no hazard found; interference is a signal-quality question with a named mitigation and an existing test to extend (§3.2) |
> | PBM + tES (T1-B) | **Recommended: allow, once tES is socket-addressable at all** | the co-drive adds ≤ 40 mW against ≥ 1.3 W of LED heat at the same tile (§3.3) |
> | EEG recording + tES on the same electrode | **Must not** | one conductor; stimulation saturates and back-drives the ADS1299 input; a stimulation current path through the recording front end is not a controlled path (§3.4) |
> | Two electrical channels on the same electrode | **Must not** | both charge ceilings are enforced per channel and cannot see a superposition on one electrode (§3.5) |
> | **tES through a T1-B pod, with anything or alone** | **Must not, today** | **BES/tACS is checked against a 25 cm² default when a pod is ≤ 1.02 cm² — a 24× fail-open** (§3.6.1); and no T1-B pod can carry any tDCS protocol in the shipped library under DI-SAFE-01 (§3.6.2) |
>
> **Two findings did not survive being written down, and they are larger than the question.**
>
> 1. **The BES/tACS channel has no geometry gate** (§3.6.1). tDCS and HD-tDCS fail closed if no
>    electrode area is declared; BES/tACS does not, and `np_mod_bes_tacs_params_t` has no area field
>    to declare. Routed through a T1-B pod, a tACS protocol would be enforced against
>    `NP_ELECTRODE_AREA_CM2` = 25 cm² — **24× more permissive than the pod**. Nothing reaches this
>    today, because tES has no socket target and its slot target names 10-20 sites rather than pods;
>    **it becomes reachable the moment either changes**, which is exactly the change this study was
>    asked about.
> 2. **The T1-B electrode cannot deliver the shipped tDCS library, alone or combined** (§3.6.2). Every
>    one of the 14 predefined tDCS protocols needs **8.0–24.0 cm²** of electrode under DI-SAFE-01's
>    150 mC/cm²; a T1-B pod is at most **1.02 cm²**, and **9 of the 14 need more than an entire 40 mm
>    tile face** (13.85 cm²). On a pod the safety MCU would cut each of them after **77–153 s**. That
>    is not a multi-modality finding — it is a finding about `NP-HEX-ZM-001` §4a's premise that T1-B's
>    dual-rated electrode is where T1 tES is delivered — and it goes to the principal as `OI-MMSOCK-01`.
>
> **What this change does.** It writes the analysis down, raises the open items, and makes **two test
> additions and one comment change** (in the socket dispatcher and its header), all of which pin behaviour the code
> already has (§11). **It decides nothing that is the principal's**: which combinations to support,
> whether tES gets a socket target, and every Class C change are recorded as open items in §10 and
> §7.2. **No Class C code, no enable-word bit and no wire-format byte changes.**

---

## 1. Scope

**In scope:** what may be driven concurrently at **one** lattice socket — PBM (T1-A/B/C emitters),
EEG recording and tES (BES/tACS, tDCS) through a T1-B's dual-rated electrode — and the firmware data
model, wire format, safety-enable ownership, dose records and power-governor interaction that follow.

**Out of scope, stated so its absence is not read as a finding:**

| Thing | Why | Where it lives |
|---|---|---|
| Concurrency *across* sockets — tES on socket A while EEG records on socket B | not a same-socket question; it is the conventional concurrent-tES-EEG artifact problem | `NP-DRV-SHELL-002` §9, `NP-HW-EEGNET-001` §5 |
| T2 clinical tACS / HD-tDCS through T1-B at density | its geometry is already declared and gated on `NP_SAFETY_CH_CLIN_STIM` (`NP-FW-HUB-001` §5.4); §5.2 notes where it would diverge | `NP-FW-HD-001`, `NP-HW-TACSDRV-001` |
| T2-D (1170 nm laser), TMS, cervical VNS | not socket-resident, or a separate tile type with no second resource | `NP-HW-TMS-001`, `NP-HW-CVNS-001` |
| The power governor's design | this document only states what multi-modality adds to its input (§6.3) | `OI-HEXTILE-09`, `OI-FWHUB-09` |

**Boundary rule.** The hub requests; the safety MCU grants (`NP-FW-HUB-001` §1). Nothing proposed here
substitutes for a Class C control, and every "must not" below is enforced — where it is enforced at all
— **in addition to** the safety MCU, never instead of it.

---

## 2. What one socket physically offers

Every socket carries the same 19-position interface (`NP-HW-HEXTILE-001` §7.2), because any tile type
may be inserted in any socket (R-2). Grouped by what can be commanded independently:

| Resource | Conductors | What drives it | Safety gating | Tile types that use it |
|---|---|---|---|---|
| **Optical** — emitter drive | `VLED`/`PGND` (N1), I²C + `SYNC` (N2) | on-module driver, one command per tile (base `cur_a`/`cur_b`, smart `ch_mask`) | per-cluster `SAFE_EN[n]` (Class B hardware) in series with the one `NP_SAFETY_EN_PBM_CRANIAL` bit (Class C) — `NP-HW-HUB-001` §7.2, §7.3 | T1-A, T1-B, T1-C |
| **Electrode** — pin 17 `ELEC` | **one conductor**, with `ELEC_SHLD` driven from the EEG DRL | the per-cluster N4 analog mux, which routes a socket's `ELEC` **either** to an ADS1299 input **or** to a tES driver channel — *"record/stim path select"* (`NP-DRV-SHELL-002` §3.2 BOM row, §3.5) | tES: the modality's own `NP_SAFETY_EN_*` bit (Class C) plus `SAFE_EN[n]` on the selector (`NP-DRV-SHELL-002` §6, *tES path interlock*); EEG: none — recording delivers nothing | T1-B only |
| Sense (no command) | `PD1_K`, `PD2_K`, `NTC` (N3), `ALERT#`, `SEAT#` | — | the NTC feeds the 42 °C / 62 °C chain, which cuts **`NP_SAFETY_EN_PBM_CRANIAL` only** (`np_thermal_interlock.c`) | all |

**Two consequences frame everything after this section.**

1. **The combinations that can exist are bounded by two lanes, not by the modality count.** PBM
   occupies the optical lane; EEG *and* tES both want the electrode lane. Nine modalities do not
   produce 9 × 8 / 2 pairings at a socket; they produce one optical user and at most one electrode
   user.
2. **The electrode lane is one conductor through a selector.** Whether its two uses may coexist is
   not a scheduling question; it is answered by what one wire connected to two circuits at once does
   (§3.4).

---

## 3. Which combinations — and why (question 1)

Each verdict carries its failure and its trace (CLAUDE.md §18). A verdict marked **recommended** is a
recommendation to the principal (§7.2), not a decision taken here.

### 3.1 Several PBM wavelengths — supported, no change

One tile, one on-module driver, one command: the smart `ch_mask` selects any of 660/808/1064 nm and the
base block drives `cur_a`/`cur_b` together (`NP-FW-HUB-001` §3.4 gate 5). A second PBM command on the
same socket **replaces** the first; a type change (base ↔ smart) stops first, because the two are
reached through different hardware paths. Nothing here needs revisiting: a socket cannot hold two PBM
tiles, so "two PBM modalities at one socket" can only ever mean two wavelengths on one driver.

### 3.2 PBM + EEG recording at a T1-B — recommended: **allow**

**This is the combination T1-B was designed for.** `NP-HEX-ZM-001` §4a: T1-B *"keeps base 660/808
PBM (reduced LED count around the pod) so PBM coverage stays continuous at electrode sites"*. It
is already expressible — PR #377's socket path admits a T1-B's emitters through placement (the power
gate then refuses them, as it refuses every drive) while EEG records through the fixed EEG slot,
because the two use different lanes. What was missing is a statement that
the concurrency is *intended* and what it has to be verified against.

**Hazard search — none found.** EEG recording delivers no energy; PBM's limits (irradiance, duty, the
42 °C chain) are enforced per tile and are indifferent to whether that tile also records. No entry in
`NP-RISK-003`, `NP-RISK-004` or `NP-FMEA-001` has a mechanism that the co-location creates.

**What it does to the signal — two effects, one already tested for, one not.**

| Effect | Mechanism | Status |
|---|---|---|
| Supply-borne artifact | the tile's own VLED current, pulsed at the therapeutic 2–40 Hz envelope, couples into `ELEC` across ~2–3 mm of laminate rather than the retired 18–22 mm | **Tested for.** `SH2-DRC-16` (*EEG recording, all LEDs at full PWM load, artifact < 5 µVpp*) is exactly this — **provided its setup includes a T1-B driving its own ring.** `NP-DRV-SHELL-002` §9.1 does not say whether it does |
| Optically-induced electrode artifact | back-scattered light reaching an Ag/AgCl contact produces a light-dependent electrode potential, synchronous with the PBM pulse | **Not tested for anywhere in the document set.** Its size at this geometry is unknown; it is in-band by construction, because the PBM envelope *is* the EEG band (`NP-DRV-SHELL-002` §9.2) |

**Why the second one matters more than its likely size.** The product's primary moat is EEG-adaptive
closed-loop stimulation (CLAUDE.md §1). A pulse-synchronous artifact at a co-sited electrode raises
band power **at the stimulation frequency** — so an adaptation rule keyed to that band can respond to
its own stimulus. The consequence is bounded (PBM irradiance and duty ceilings are firmware- and
hardware-enforced regardless of what the loop asks for), so this is **a dose-integrity and efficacy
risk, not a safety hazard** (`RISK-MMSOCK-04`). The mitigation already exists: `REQ-EMI-03/04` make
the PBM phase deterministic and locked to the EEG sample frame via `SYNC`, precisely so the artifact is
a known, subtractable line. What does not exist is the measurement that says it works at a co-sited
electrode — `OI-MMSOCK-05`.

**Verdict.** No "must not" is traceable. Recommend **allow**, with `SH2-DRC-16`'s setup extended to the
co-sited case and the self-artifact check of `OI-MMSOCK-05` added before the closed loop is claimed to
work at T1-B sites.

### 3.3 PBM + tES at a T1-B — recommended: **allow, once tES is socket-addressable at all**

The task named two candidate hazards. Both are real questions; neither yields a "must not" for the
**co-drive**.

**(a) Thermal stacking.** IEC 60601-1's 42 °C applies to the applied part whatever heats it. The
question is whether tES adds enough heat at a PBM tile to matter, and the arithmetic says it does not:

| Source at one T1-B | Power | Basis |
|---|---|---|
| tES Joule heating at the contact, worst T1 case | **≤ 40 mW** | I²R at 2 mA (tDCS ceiling, CLAUDE.md §3) into **10 kΩ** — an *assumed* upper contact impedance, 2× the VNS contact window's 5 kΩ upper bound (`NP-FW-HUB-001` §8.4); no tES impedance window is specified for T1 |
| LED drive at the same tile | **1.3–25.0 W** electrical | the authored-library range, `NP-SES-PWR-001` §2 / `NP-HW-HEXTILE-001` §9.2 note |

tES is **≤ 3 %** of the *smallest* authored per-tile LED load and ≤ 0.2 % of full drive. The per-tile
NTC throttle cuts the LEDs — the dominant term — so the existing 42 °C chain already bounds the
combined temperature to within the tES increment. **Two things this does not cover, stated rather than
assumed away:** tES heat is concentrated under a ~1 cm² contact where LED heat is spread over the
10.61 cm² field, and the NTC's position relative to the pod is not specified. The areal comparison is
therefore closer than the total (tES ~40 mW/cm² at the pod against ~120–2,400 mW/cm² of LED
electrical load averaged over the field) — still not close, but a measurement, not an argument, should
close it (`OI-MMSOCK-06`).

**(b) Current density at one site.** The electrochemical hazard of tES is governed by DI-SAFE-01/01a,
which the safety MCU enforces against the declared electrode area regardless of what else the tile is
doing. PBM delivers no current to tissue. The two doses are physically independent quantities with
independent ceilings; there is no traced mechanism by which illumination raises the charge-injection
risk of the adjacent contact, and none by which tES current raises the optical dose.

**(c) Found while answering, and not a co-drive hazard — routed, not absorbed.** A T1-B carries the
**24 V `VLED` rail and a scalp-contacting conductor on one tile**. A single insulation fault between N1
and N4 puts up to 24 V on the scalp contact through an unregulated path. That fault is live whenever
`VLED` is energised at that tile — i.e. whenever **any** PBM in its cluster runs, because `VLED` is gated
per cluster (`NP-HW-HEXTILE-001` D-8), not per tile — so forbidding co-drive at *this* socket would not
remove it. `NP-RISK-003` has no entry for it (its only T1-B row, RISK-HEX-03, is about emitter count).
**`OI-MMSOCK-07` routes it to `NP-RISK-003`**; it is recorded here as `RISK-MMSOCK-05` only so it is
not lost.

**Verdict.** No "must not" is traceable for the co-drive. Recommend **allow** — but it is moot until
tES can be commanded at a socket at all, and §3.6 is why it cannot be yet.

### 3.4 EEG recording + tES on the same electrode, concurrently — **must not**

This is not a preference and not an interference budget. It is what one conductor connected to two
circuits does:

| Failure | Trace |
|---|---|
| **The recording is destroyed.** 2 mA into a 0.5–10 kΩ contact is 1–20 V at the electrode; the ADS1299's differential full scale at the gain the EEG path uses (×24) is ±VREF/24 = ±187.5 mV. The input is saturated by 5–100× — there is no recording to keep | ADS1299 datasheet (full-scale range = ±VREF/gain, VREF 4.5 V); CLAUDE.md §3 (2 mA) |
| **The stimulation current acquires an uncontrolled path.** A stimulating electrode connected to the recording front end exposes the tES current to the ADC's input protection, bias and lead-off networks — a return path that is neither the paired electrode nor anything the charge monitor accounts for | `NP-DRV-SHELL-002` REQ-EMI-08 (a DRL-driven structure — which `ELEC_SHLD` is — may not serve as a tES return); DI-SAFE-01/01a are per-electrode commanded-dose limits that assume the commanded current leaves by the paired electrode |
| **The hardware already makes it a selection.** N4 is specified as a *record/stim **select***, not a tee | `NP-DRV-SHELL-002` §3.2 |

**Time-multiplexing on the same electrode within a session — also must not, for DC.** Switching a DC
channel's electrode to the recorder and back is an unramped current step, and tDCS carries a **30 s
ramp** as a hard limit (CLAUDE.md §3). For tACS the step is not a hazard of the same kind, but the
electrode's post-stimulation polarization dominates the first recording window, so the data is not
worth the switch. **Recommended granularity: an electrode is assigned to recording or to one stimulation
channel for the whole session** (§4.2).

**Enforcement: make it unrepresentable, not checked.** §4's data model gives each socket **one**
electrode lane, so "EEG and tES on socket *s*" cannot be expressed in the record, rather than being
expressed and then refused.

### 3.5 Two electrical channels on one electrode — **must not**

"tACS superimposed on a tDCS offset" (oscillating DC) is a real protocol family, and two channels
wired to one `ELEC` would express it. It must not be expressed that way, because **neither ceiling can
see the result:**

- The safety MCU enforces DI-SAFE-01 (DC, per session) and DI-SAFE-01a (charge-balanced, per phase)
  **per channel**, against that channel's commanded current (`np_charge_monitor.c`). The electrode
  receives the **sum**.
- The sum of a DC channel and a charge-balanced channel is **monophasic and not charge-balanced**.
  DI-SAFE-01a's rationale — *"net delivered charge on these waveforms is ~zero"* (`NP-DT-001` §3.2.1)
  — does not hold for it, and DI-SAFE-01's integral sees only the DC channel's share.

So a superposed waveform on one electrode is a waveform for which **no enforced ceiling is valid**.
A superposed protocol, if the principal wants one, must be a **single channel whose commanded waveform
is the superposition**, declared as DC for the purpose of DI-SAFE-01 — which is a Class C question about
waveform classes, not a routing one (`OI-MMSOCK-04`).

### 3.6 tES through a T1-B pod at all — **must not, today**

Everything above assumes tES could be commanded at a socket. It cannot (PR #377: tES slot targets name
10-20 sites through `electrode_pair`, and `REQ-FWHUB-33` refuses them on the socket path). Examining
what it would take found two problems that come *before* any combination.

#### 3.6.1 BES/tACS has no geometry gate — a 24× fail-open

| Channel | Area source | Fail-closed if undeclared? |
|---|---|---|
| `NP_SAFETY_CH_TDCS` (6) | `np_mod_tdcs_params_t.electrode_area_mcm2`, signed | **yes** — `GEOM_REQ_TDCS` (`OI-CHARGE-04`) |
| `NP_SAFETY_CH_CLIN_STIM` (13) | implied by HD-tDCS montage code (96 mcm²) | **yes** — `GEOM_REQUIRED` (`OI-CHARGE-03`) |
| **`NP_SAFETY_CH_BES_TACS` (5)** | **none — `np_mod_bes_tacs_params_t` has no area field** | **no** — the MCU applies `NP_ELECTRODE_AREA_CM2` = **25 cm²** |

The 25 cm² fallback is correct for what BES/tACS was: a sponge-pad montage at 10-20 sites. A T1-B pod
is bounded above by its body diameter — ⌀11.4 mm, contingent on `OI-HEXTILE-05`
(`NP-HW-EEGNET-001` §1.9.1) — so **A_pod ≤ π × 0.57² = 1.02 cm²**, and the contact face is smaller
than the body. Enforced against 25 cm², the per-phase ceiling admits **24.5×** the charge the pod's own
40 µC/cm² permits. That is the fail-open shape `OI-CHARGE-03` rejected for CLIN_STIM and
`OI-CHARGE-04` rejected for tDCS, on the one electrical channel neither item reached.

**Not reachable today, and why that is not reassurance.** Nothing routes BES/tACS to a pod: its target
is a slot and its `electrode_pair` names F3–F4 / P3–P4 / Fz–Pz. But the hardware delivers tES through
T1-B `ELEC` contacts — N4 is the only electrode network (`NP-DRV-SHELL-002` §3.5) — so the HAL that
eventually implements `np_mod_stim_hal_dac_set(pair, …)` (`OI-STIM-01`) **must** route to pods, and on
that day the gap is live whether or not anyone adds a socket target. **`OI-MMSOCK-02`: BES/tACS needs a
declared area and a fail-closed geometry gate before any HAL routes it to a T1-B electrode.** That is a
Class C change; §5.3 states its scope.

#### 3.6.2 No T1-B pod can carry the shipped tDCS library — the finding that is not about combinations

DI-SAFE-01 is enforced per electrode against the declared area. Run over every `tdcs` block in
`protocols/predefined/` (14 protocols; current, duration and declared area as authored, 35 cm² in each):

| Protocol | I | t | Charge | Area needed at 150 mC/cm² | On a 1.02 cm² pod | MCU cuts at |
|---|---|---|---|---|---|---|
| `08-adhd-focus` | 1.5 mA | 40 min | 3.60 C | **24.0 cm²** | 23.5× over | 102 s |
| `clinical-30-tdcs-depression` | 2.0 mA | 25 min | 3.00 C | 20.0 cm² | 19.6× | 77 s |
| `clinical-31/-32/-34/-35/-38/-39` | 2.0 mA | 20 min | 2.40 C | 16.0 cm² | 15.7× | 77 s |
| `clinical-37-tdcs-adhd` | 1.5 mA | 25 min | 2.25 C | 15.0 cm² | 14.7× | 102 s |
| `15-full-t1-immersive`, `clinical-29`, `-36`, `-40` | 1.0–2.0 mA | 15–30 min | 1.80 C | 12.0 cm² | 11.8× | 77–153 s |
| `clinical-33-tdcs-aphasia` | 1.0 mA | 20 min | 1.20 C | **8.0 cm²** | 7.8× | 153 s |

**Every protocol exceeds a pod by 7.8–23.5×. 13 of 14 need more than a tile's whole active field
(10.61 cm², `NP-HW-HEXTILE-001` §3) and 9 of 14 need more than the entire 40 mm hex face (13.85 cm²).**
No pod diameter `OI-HEXTILE-05` could choose fixes this; the tile is too small, not the pod.

**What this is and is not.** It is **not a hazard** — the safety MCU enforces the ceiling against the
declared area and would cut each session at the time in the last column, which is the control working.
It **is** a product-level contradiction: `NP-HEX-ZM-001` §4a makes T1-B's dual-rated electrode the T1
tES electrode (*"EEG **and** BES/tACS/tDCS (one electrode records + stimulates)"*), and at conventional
T1 currents it cannot deliver a single shipped tDCS protocol. Nothing in the document set has run this
arithmetic before. **`OI-MMSOCK-01` — principal.** The options it has to choose between are in §7.2;
this study does not choose.

**BES/tACS on a pod is marginal rather than impossible.** Per-phase charge of a sinusoid is
I/(πf), so the ceiling binds at low frequency (`NP-DT-001` §3.2.1 makes the same point about 0.5 Hz).
Over the 11 T1 `bes_tacs` blocks in the library, with `intensity` read as **peak** (the convention of
`NP-DT-001` §3.2.1's margin table):

| | Protocols | On a 1.02 cm² pod |
|---|---|---|
| 10–40 Hz | 8 (`13`, `14`, `15`, `clinical-42/-45/-47/-48/-49`) | fit, 16–78 % of the ceiling |
| 5–6 Hz theta | `clinical-41` (6 Hz), `clinical-44` (5 Hz) | over, 1.3× and 1.6× |
| 0.75 Hz slow oscillation | `clinical-46` | over, 7.8× |

If `intensity` is authored **peak-to-peak**, phase charge halves: the theta pair then fits and only
`clinical-46` (3.9×) exceeds. **Which convention the `.npps` grammar means is not stated in
`NP-NPPS-REF-001`** and changes this table's answer — `OI-MMSOCK-08`.

**Separately, and not quantified here:** conventional tES separates metal from skin with a sponge or a
gel-filled holder; a small electrode delivering DC without an electrolyte reservoir is the configuration
the high-definition tDCS literature designed holders to avoid (Minhas et al., *J Neurosci Methods*
2010). T1-B's contact is specified as dual-rated Ag/AgCl on a spring pod with no stated electrolyte
volume. This is part of `OI-MMSOCK-01`'s question, not an independent requirement.

### 3.7 The matrix, with enforcement point

| Combination | Verdict | Enforced where (as recommended) |
|---|---|---|
| PBM λ₁ + λ₂ | allowed | on-module driver (existing) |
| PBM + EEG | **allow** (recommended) | nothing to enforce; `SH2-DRC-16` extended, `OI-MMSOCK-05` |
| PBM + tES | **allow once tES is socket-addressable** (recommended) | nothing beyond tES's own gates |
| EEG + tES, same electrode | **must not** | unrepresentable in the socket record (§4.2); N4 is a selector (hardware) |
| tES ch *a* + tES ch *b*, same electrode | **must not** | unrepresentable (§4.2); parser rejects a socket in two electrode sets (§5.1) |
| any tES through a T1-B pod | **must not until `OI-MMSOCK-02` closes** | `REQ-FWHUB-33` (existing) — tES is not socket-addressable; pinned by test (§11) |
| EEG on pad *a* + tES on pad *b*, same tile | **not possible at one electrode per tile**; with two, a recommendation rather than a must-not (§3.8) | the per-element electrode lane (§3.8, §4.1) |

---

### 3.8 More electrodes per tile — what it unlocks and what it does not

The obvious response to §3.4 and §3.5 is to give a tile more than one electrode. `NP-HW-EEGNET-001`
§1.7 has already studied that for recording density (`OI-EEGNET-19`); this section asks the other
question — **which of this document's verdicts it changes.**

**It is physically possible.** Up to four ⌀11.4 mm pods fit in a tile's active field, 20.5 mm apart
on a square, or two at ±14.5 mm, 29.0 mm apart (`NP-HW-EEGNET-001` §1.7.1). Face area never binds —
a 30-contact array is ~4.5 % of the tile face (§1.7.2 of that document).

**What it costs — every item owned elsewhere, none decided here:**

| Cost | Size | Owner |
|---|---|---|
| **Socket contacts → clamp force** — the binding constraint | each electrode adds `ELEC`, plus `ELEC_SHLD` if shields are per-electrode: 19 → 20–21 contacts at two electrodes, 22–25 at four; plate load 34.2–57.0 N → 36.0–63.0 N (two) or 39.6–75.0 N (four). 34.2–57.0 N is **already** unanswered against one-handed operation (`OI-SHELL2-03(b)`, `RISK-22`) | `NP-HW-EEGNET-001` §1.7.2, `OI-EEGNET-20` |
| **The socket has no spare positions** | closed at 19 with the two reserved positions dropped; after socket tooling (`OI-SHELL2-09(i)`) the count is permanent at every socket | `NP-DRV-SHELL-002` §5.1.4 |
| ADC channels | T1 records 8; more electrodes record nothing more without a second ADS1299 | `NP-HW-HUB-001` §5 |
| **Safety MCU** | a recording-only pad costs it **nothing**. Each independently **stimulating** electrode is a charge-monitor channel, and the 38-byte heartbeat has zero spare bytes — a Class C wire-format change (`NP-HW-HEXTILE-001` §8.4.2) | `NP-HW-HUB-001` §7.2 |
| N4 mux width, socket BOM | per-cluster electrode inputs double at two electrodes; +$3–7 per headset at 21 contacts | `NP-DRV-SHELL-002` §3.5, §10.1 |

**Against this document's verdicts:**

| Verdict | Changed by more electrodes? |
|---|---|
| §3.4 — EEG + tES on one electrode, must not | **Moves, does not disappear.** With two electrodes the must-not still holds *per electrode*, but a tile can then record on one pad while stimulating on the other — the VNS clip's one-conductor-per-function pattern (`NP-HW-EEGNET-001` §0 note), at a tile. **What replaces the must-not is an unmeasured risk:** a recording pad 20–29 mm from a pad carrying up to 2 mA sits in the stimulation field, and whether that saturates the ADS1299 at ×24 gain is unknown. It is the across-socket artifact problem §1 excludes, brought closer — `OI-MMSOCK-09` (b) |
| §3.5 — two channels on one electrode, must not | **Resolved in the form that matters**: tDCS on one pad and tACS on another is two electrodes, each checked by its own ceiling, and no superposition exists at any electrode. **Residual:** at 20–29 mm spacing a share of the current shunts through the scalp between the pads rather than reaching cortex. That is an efficacy question, not a charge-ceiling one, and it is unquantified |
| §3.6.1 — BES/tACS geometry gate missing | **No.** More stimulating pads make C-1 more necessary, not less |
| §3.6.2 — no pod carries the tDCS library | **No.** Four pods total 4 × 1.02 = **4.08 cm²** against the library's smallest need of 8.0 cm². Ganging pods as one electrode does not help either: the split between them is set by contact impedance, which nothing measures, so the safety MCU must assume the whole channel current through the smallest pod (§5.2 item 2) — the same 1.02 cm². §3.6.2's conclusion is about the tile, and more pods on the tile leave it standing |

**Recommendation — if the goal is recording and stimulating at one site, two electrodes, one of them
dual-rated.** One recording-only pad plus one dual-rated pad per T1-B:

- opens the combination §3.4 forbids on one electrode, at the price of `OI-MMSOCK-09` (b);
- holds the safety MCU's stimulation-channel count flat, so no Class C frame change follows from it
  (`NP-HW-EEGNET-001` §1.7.1's decoupling lever);
- costs one contact with a shared shield (19 → 20; +1.8–3.0 N per plate) — the cheapest point on the
  force curve.

Four electrodes pay only for **recording density**, and there the 8-channel ADC binds before the tile
does. **None of this changes P-1**: T1 tDCS still needs a pad-scale electrode on or off the lattice.

**Effect on §4.** The per-(socket, lane) record already anticipates this (§4.1, *rejected — per-(socket,
element)*): the electrode lane becomes an array indexed by electrode element, one role and one channel
per element. The must-nots stay properties of the struct, now per element rather than per socket.

## 4. Data model (question 2)

### 4.1 Per-(socket, lane) — recommended

Replace one record per socket with **one record per socket per lane**:

```c
/* PROPOSED — not implemented. §7.1 D-3: build with the first lane that needs it. */
typedef enum { NP_ELANE_NONE = 0, NP_ELANE_RECORD = 1, NP_ELANE_STIM = 2 } np_elane_role_t;

typedef struct {
    struct {                              /* optical lane — as np_sock_disp_socket_t today   */
        bool              active;
        np_hub_mod_type_t mod_type;       /* PBM_BASE | PBM_SMART                            */
        uint8_t           params_len;
        uint8_t           params[NP_SOCK_DISP_PARAMS_MAX];
        uint32_t          stop_at_ms;
    } opt;
    struct {                              /* electrode lane — T1-B `ELEC`, one user only     */
        np_elane_role_t   role;           /* RECORD xor STIM: one conductor (§3.4)           */
        uint8_t           channel;        /* STIM: NP_SAFETY_CH_* — exactly one (§3.5)       */
        int8_t            polarity;       /* STIM: +1 anode / −1 cathode                     */
        uint32_t          stop_at_ms;
    } elec;
} np_sock_lanes_t;
```

**Why lanes and not modalities.** The lanes are the socket's independent physical resources (§2). A
per-modality record would make *"EEG and tES on socket s"* a representable state that a check must then
refuse; a per-lane record makes it a state the type cannot hold. **The two must-nots of §3.4 and §3.5
become properties of the struct**: one `role`, one `channel`.

**Rejected — per-(socket, modality):** up to nine slots per socket, of which the hardware can honour at
most two, and the invalid seven have to be policed by admission code forever. Fails the §3.4 enforcement
goal.

**Rejected — per-(socket, element):** `np_module_map` does address `socket:element`, but no element is
independently commandable. Emitters are lit as a set by one on-module command (a smart `ch_mask` is
already the per-element choice, carried *inside* the lane's params); a T1-B has one electrode. A
per-element record would store up to 128 entries per socket to represent state that is one command.
It becomes right only if `OI-EEGNET-19` puts several independently-routed electrodes on one tile — at
which point the electrode lane becomes an array indexed by electrode element, not a redesign.

### 4.2 Admission by lane

| Lane | Placement requirement (`np_module_map_check_placement`, one requirement per socket × need) | Conflict rule | Stop rule |
|---|---|---|---|
| optical | every emitter the params light (existing, `NP-FW-HUB-001` §3.4 gate 6) | a second PBM command replaces; a base↔smart change stops first (existing) | lane-scoped: stopping PBM never touches the electrode lane |
| electrode, RECORD | `NP_ELEM_DUAL_ELECTRODE` or `NP_ELEM_EEG_ELECTRODE` | refused if the lane is STIM this session | recording has no enable; stop is a mux release |
| electrode, STIM | `NP_ELEM_DUAL_ELECTRODE` or `NP_ELEM_TES_ELECTRODE` | refused if the lane is RECORD this session, or STIM on another channel; a new command on the **same** channel with different sockets **stops the old montage first** — re-routing a live DC channel is an unramped step (§3.4) | lane-scoped |

**Session-scoped role.** Once an electrode lane has been RECORD or STIM in a session it keeps that role
until session end (§3.4's time-multiplexing verdict). A STIM stop leaves the lane *reserved*, not free.

### 4.3 Enable ownership and reference counting

| Enable | Owned by | Requested | Released |
|---|---|---|---|
| `NP_SAFETY_EN_PBM_CRANIAL` (bit 0, one for the lattice) | the socket registry (existing, `REQ-FWHUB-34`) | after all of a command's optical lanes are configured | when **no optical lane** is active — electrode lanes do not hold it; on a failed optical stop, at once with every optical lane stopped |
| `NP_SAFETY_EN_BES_TACS` / `_TDCS` (bits 5, 6) | the socket registry, **once tES is socket-addressable** — the slot path's `slot_to_safety_bit()` must then stop owning them, or two owners can each drop the other's bit | after the channel's whole montage (every anode and cathode lane) is routed | when that channel has no STIM lane; **on a failed electrode stop, that channel's bit at once** |
| per-cluster `SAFE_EN[n]` (Class B hardware) | the hub (`HUB-REQ-C05`, `OI-FWHUB-11`) | when any lane in the cluster needs it — optical **or** STIM, since `NP-DRV-SHELL-002` §6 gates the tES selector with it too | when no lane in the cluster needs it |

**Why a failed electrode stop does not drop the lattice.** The optical and electrode lanes are cut by
different Class C bits; a lost mux on one T1-B says nothing about the emitters, and cutting all cranial
PBM would be over-cutting for no traced hazard (`NP-HW-HUB-001` §7.2: *"over-cutting is a usability
cost, never a hazard"* — true, and also not free). The narrower cut is the channel's own bit, which
removes the driver the mux was routing.

**One ownership rule the current code relies on and must keep.** Today `NP_SAFETY_EN_PBM_CRANIAL` has
exactly one owner because only the socket registry drives cranial emitters (the slot path's PBM entries
are unreachable, `NP-FW-HUB-001` §3.3). Moving tES bits to the socket registry repeats that shape for
bits 5 and 6: **one owner per bit**, never "whoever touched it last".

---

## 5. Wire format and the Class C interface (question 3)

**Nothing in this section is implemented, and all of it is the principal's decision** (§7.2, P-2).
It is specified far enough that the decision is between concrete options, not between intentions.

### 5.1 A socket target for tES — recommended shape, if adopted

`NP_PROTO_TARGET_SOCKET_MASK` is the wrong shape for tES: one bitmap cannot say which sockets are
anodes and which cathodes, and a bitmap's dedup property — its whole reason to exist (`NP-FW-HUB-001`
§4.3) — would **hide** the one error that matters, a socket named on both sides.

| Option | Shape | Verdict |
|---|---|---|
| **A — two disjoint bitmaps** ★ | new kind `NP_PROTO_TARGET_ELECTRODE_SETS`: anode mask (16 B) + cathode mask (16 B), same index-space convention as §4.3 | **Recommended.** Keeps the bitmap's duplicate-freedom *within* a side and makes overlap *between* sides a parse-time rejection (a socket on both sides is a short through the mux). Complete for every T1 bipolar montage |
| B — (socket, role) address list | variable length | rejected: re-admits duplicates the bitmap made unrepresentable, and a variable-length target is a parser gadget unless pinned (`NP-FW-HUB-001` §4.4) |
| C — keep slot targets; resolve `electrode_pair` to sockets on the hub | no wire change | rejected: the pair → socket map depends on shell geometry and `REG-1`; putting it on the hub is the topological-map-behind-a-boundary problem `NP-HW-HUB-001` §4.5.1 rejected for clusters |

**Costs of option A, all versioned per `REQ-FWHUB-10`:** `NP_HUB_PROTO_TARGET_MAX` 16 → 32;
`np_mod_bes_tacs_params_t` gains `electrode_area_mcm2` (+2 B) — a **length change**, so
`NP_HUB_PROTO_VERSION` bump (3 → 4 when written; **4 → 5** now that `OI-HEXTILE-25` took v4, 2026-09-25); the parser gains three rejections (either side empty, sides overlap,
`target_len` ≠ 32); `hubCompiler.ts` gains `electrodeSetsTarget()` and the `.npps` grammar needs a way to
name sockets for tES, which today it names by 10-20 site. T2's 21-channel clinical tACS needs a
*channel per electrode*, not two sides — a third kind, deliberately not designed here.

### 5.2 Declaring electrode area per socket

**Keep area in the signed descriptor; add a Class B cross-check; change no Class C semantics.**

1. **Declared, signed, per command.** A tES command declares the area of **one** electrode, as tDCS
   does today. Every socket on the command must be a pod of at least that area.
2. **Sent as the minimum, against the full channel current.** The hub already sends *"the smallest
   declared area"* across a channel's commands (`NP-FW-HUB-001` §5.4). With several sockets on one side
   the current divides between them in a ratio set by contact impedance, which nothing measures — so the
   only safe assumption is **the whole channel current through the smallest electrode**. That is what
   the safety MCU already computes, and it can only err low.
3. **Cross-checked, Class B.** The hub refuses a tES command whose declared area **exceeds** the contact
   area its placement check finds at any named socket. A declaration larger than the hardware is the
   permissive direction. This needs the contact area in the module inventory, which does not carry it
   (`OI-MMSOCK-03`). It is defence in depth only: **Class C still enforces the declared figure**, and a
   hub that skipped the cross-check would leave the Class C ceiling exactly as strong as it is today.

**Rejected — the safety MCU reads area from the inventory.** The MCU cannot see `np_module_map`, and
teaching it the socket→module map puts a topological structure behind the Class C boundary
(`NP-HW-HEXTILE-001` §8.4.1's argument, verbatim). **Rejected — derive area from the tile type.** A pod
diameter is `OI-HEXTILE-05`'s open decision and may vary by variant (`NP-HW-EEGNET-001` §1.10); a
type-implied area is the `NP_HD_SMALL_ELECTRODE_AREA_MCM2` shape, which is acceptable only where the
geometry is fixed by the montage code, as it is for HD-tDCS and is not here.

### 5.3 The SW-01 (Class C) review any adoption requires

**No enable-word bit and no heartbeat byte moves.** Stated item by item so the review can be scoped
before it is convened:

| # | Change | Class C artefact | Why it is needed | Layout impact |
|---|---|---|---|---|
| C-1 | **BES/tACS geometry gate**: a new `session_status` bit `NP_SESSION_STATUS_GEOM_REQ_BES` (bit 4 — bits 4–7 are unused) and a third arm in `np_charge_monitor_geom_gate()` clearing `NP_SAFETY_EN_BES_TACS` until an area for channel 5 has been applied | `np_spi_wire_types.h`, `np_safety_protocol.h`, `np_charge_monitor.c`, `np_safety_main.c` status decode; the charge-monitor tests | §3.6.1 — without it, BES/tACS on a pod is enforced against 25 cm² | **none** — a spare bit in an existing byte; `np_safety_chan_limit_cmd_t.area_mcm2[5]` already exists and `np_charge_monitor_set_channel_area_mcm2()` already accepts channel 5 |
| C-2 | Confirm, and record in `NP-SW-001` SW01-M03, that a channel's current is enforced against its declared area **as if it all flowed through one electrode** | documentation of existing behaviour | §5.2 item 2 — multi-socket sides make the assumption load-bearing where bipolar pads never did | none |
| C-3 | Decide how a superposed DC + AC waveform is classed for the waveform-declaration gate, if the principal wants one (§3.5) | `np_charge_monitor_decl_gate()` classes | §3.5 — no current class is valid for it | none, if it is declared as DC on one channel |
| C-4 | Confirm that `REQ-NET-10`'s inter-electrode impedance-matrix interlock is specified for **lattice** electrodes too, not only for the net | `NP-HW-EEGNET-001` §5.5 is written for option (b), the net | the wet-shunt mechanism does not care whether the electrodes are in a net or in tiles | none |

**C-1 is the only one that changes Class C behaviour**, and it changes it in the fail-closed direction
only: a session that declares BES/tACS geometry and loses the area command loses BES/tACS, exactly as
tDCS does today. It should be taken **regardless of how P-2 resolves** — the HAL that routes the slot
path's BES/tACS to pods (`OI-STIM-01`) makes §3.6.1 live without any wire change (`OI-MMSOCK-02`).

---

## 6. Dose records, SHDR, and the power governor (question 4)

### 6.1 UHDR — per-(socket, lane) dose, and the co-occurrence is UHDR

Today UHDR records which **modalities** a session delivered (`mods_active_mask`, one bit per
`mod_type`) and nothing per socket — per-socket dose metering does not exist (`OI-FWHUB-10`). A
multi-modality socket adds one thing to what that item must build:

| Record | Content | Class | Why |
|---|---|---|---|
| optical lane dose | J/cm² per wavelength per socket (PD-metered, `OI-FWHUB-10`) | UHDR | CLAUDE.md §5.1 lists *"PBM dose (J/cm²) per zone"* as UHDR |
| electrode lane, STIM | channel, polarity, commanded charge (DC) or peak phase charge (AC), declared area | UHDR | a stimulation dose delivered to a site of the person's head |
| electrode lane, RECORD | the socket's EEG channel assignment | UHDR | EEG waveforms are UHDR; which site they came from is part of them |
| **the fact that two lanes were co-driven at one socket** | derivable from the two above | **UHDR** | it describes a treatment given to a person at a site — the §5.1 defining test answers yes |

**D-14 of `NP-FW-HUB-001` generalises per lane:** a lane command that is refused sets nothing in UHDR,
and a stop sets nothing. Because each command addresses exactly one lane (a PBM command the optical,
a tES command the electrode), there is no partially-admitted multi-lane command to define.

### 6.2 SHDR — nothing new, deliberately

No SHDR field is proposed. A lane-conflict refusal is logged through the existing path — against
`NP_HUB_SLOT_NONE`, with the refusing status as the fault code — and **reuses `NP_HUB_ERR_INVALID_ARG`
rather than minting a "lane busy" code**. A dedicated code would tell the fleet database that someone
attempted recording and stimulation at one site, which is protocol composition about a person, and
CLAUDE.md §5.1's rule 1 settles that: when in doubt, UHDR. The general refusal code already carries
`mod_type`, which the slot path already logs; nothing is added.

An electrode wear count — Ag/AgCl degradation under tES current is the mechanism CLAUDE.md §2.3 names
for VNS clip pads — would be SHDR, keyed by module UID, with no socket and no timestamp. **It is not
proposed**: nothing consumes it until tES reaches a pod, and a requirement nothing needs is the thing
CLAUDE.md §18 forbids.

### 6.3 Power governor (`OI-FWHUB-09` / `OI-HEXTILE-09`)

**Multi-modality adds nothing the governor must count, and one thing its signature must not assume.**

- tES power is milliwatts (≤ 40 mW per electrode, §3.3) and is drawn by the tES driver, inside the
  non-PBM overhead `NP-HW-HEXTILE-001` §9.1 already subtracts. The governor stays **optical-only**; adding
  tES to it would add a term four orders of magnitude below its resolution.
- `np_pbm_power_admit(cmd, current)` takes today's per-socket record as `current`. If §4.1's lane record
  replaces it, the governor must be given the **optical lanes only**, so a future electrode field cannot
  leak into a watts calculation. The cheap moment to fix that signature is when `OI-FWHUB-09` replaces
  the function body — not before, because changing a closed function's input type buys nothing while it
  refuses everything (§7.1 D-3).
- The governor's own blocking items (`OI-SESPWR-03`, `OI-HEXTILE-02`, a PD-contract seam) are unchanged by
  anything here.

---

## 7. Decisions

### 7.1 Taken in this document (Class B, no behaviour change)

| # | Decision | Rationale |
|---|---|---|
| D-1 | The socket's combination space is analysed as **two lanes** (optical, electrode), not as modality pairs | §2 — the lanes are the independently commandable hardware |
| D-2 | `REQ-FWHUB-33` (only PBM is socket-addressable) stays, and is **now pinned for every electrode-bearing modality** by test, with §3.6 as its reason of record | §3.6, §11 |
| D-3 | **§4.1's lane record is specified, not built.** It is built with the first electrode-lane command type, not ahead of it | building a lane nothing can occupy adds code with no behaviour, and would change the governor's input type while the governor refuses everything (§6.3) |
| D-4 | No new SHDR field or fault code | §6.2 |

### 7.2 Put to the principal — all six ACCEPTED as recommended, 2026-09-23

| # | Question | Options | This study's recommendation |
|---|---|---|---|
| **P-1** | **T1 tES delivery electrode** (`OI-MMSOCK-01`) — T1-B pods cannot carry the shipped tDCS library (§3.6.2) | (a) T1 tES stays on conventional pads outside the lattice, and T1-B's electrode is **recording-only** at T1 currents; (b) a large-area tES electrode accessory at lattice positions, a new element type; (c) re-author T1 tDCS to pod-scale doses — a clinical change, not an engineering one; (d) keep T1-B dual-rated for **tACS ≥ ~8–10 Hz only**, where §3.6.2's table says a pod fits | **(a), with (d) as its extension** — it is the only option that keeps the shipped library as authored. `NP-HW-EEGNET-001` §0 notes the design already has a two-conductor precedent (the VNS clip) |
| **P-2** | Give tES a socket target (§5.1) | now (option A, v4 wire format) · after P-1 · never | **after P-1**: the wire format should follow the answer to *what* electrode tES uses |
| **P-3** | Allow PBM + EEG at a T1-B (§3.2) | allow · allow after `OI-MMSOCK-05` · forbid | **allow**, with `SH2-DRC-16`'s setup extended; claim the closed loop at T1-B sites only after `OI-MMSOCK-05` |
| **P-4** | Allow PBM + tES at a T1-B (§3.3) | allow · forbid | **allow**, moot until P-1/P-2 |
| **P-5** | Take Class C change C-1 (§5.3) now | now · with P-2 | **now** — it is fail-closed only, needs no layout change, and closes a gap the slot path can reach (`OI-MMSOCK-02`) |
| **P-6** | Electrodes per T1-B (§3.8, `OI-MMSOCK-09`) | one (today) · two, one dual-rated · four | **two, one dual-rated**, if recording and stimulating at one site is wanted — decided with `OI-EEGNET-19`/`-20` and **before socket tooling**, after which the contact count is permanent |

---

## 8. Rejected alternatives

| Alternative | Why rejected |
|---|---|
| Per-(socket, modality) records | admits representable-but-invalid states that code must refuse forever (§4.1) |
| Per-(socket, element) records | no element is independently commandable; 128 entries to hold one command (§4.1) |
| A single socket bitmap for tES | cannot express polarity; its dedup would hide an anode-equals-cathode short (§5.1) |
| Hub resolves `electrode_pair` → sockets | a shell-geometry map behind the hub's interface (§5.1 option C) |
| Safety MCU reads electrode area from the inventory | a topological map behind the Class C boundary (§5.2) |
| Time-multiplex one electrode between recording and DC stimulation | an unramped DC step against CLAUDE.md §3's 30 s ramp (§3.4) |
| Two channels summed on one electrode for oscillating tDCS | no enforced ceiling is valid for the sum (§3.5) |
| Forbid PBM co-drive at a T1-B to remove the 24 V-to-`ELEC` fault | does not remove it — `VLED` is gated per cluster, not per tile (§3.3 (c)) |
| Add tES to the power governor | a term ~10⁴× below its resolution (§6.3) |
| A "lane busy" SHDR fault code | leaks protocol composition about a person to the fleet database (§6.2) |

---

## 9. Risk

Hazards and dose-integrity risks this study found or touches. **No score in another document's register
is changed here**; `NP-RISK-003` / `NP-RISK-004` rows are proposed through the open items.

| ID | Hazard / risk | Sev | Current state | Mitigation | Residual |
|---|---|---|---|---|---|
| `RISK-MMSOCK-01` | BES/tACS delivered through a ≤ 1.02 cm² pod is enforced against the 25 cm² default — per-phase charge up to 24.5× the pod's DI-SAFE-01a ceiling | **High** | **Not reachable**: tES is not socket-addressable (`REQ-FWHUB-33`, now pinned for every electrode modality) and the stim HAL is a platform trap (`OI-STIM-01`) | C-1 (§5.3): BES/tACS geometry gate, fail-closed | **Open until `OI-MMSOCK-02`** — becomes reachable the day the stim HAL routes to a pod |
| `RISK-MMSOCK-02` | Recording and stimulation connected to one electrode: saturated recording plus a tES current path through the recording front end | High | not representable in software today (no electrode lane exists); N4 is specified as a selector | §4.1 makes it unrepresentable; the hardware selector is the independent control | Accepted with both in place |
| `RISK-MMSOCK-03` | Two electrical channels summed on one electrode produce a monophasic waveform no ceiling is valid for | High | not reachable (no electrode lane) | one `channel` per lane (§4.1); overlap rejected at parse (§5.1) | Accepted with both in place |
| `RISK-MMSOCK-04` | Closed-loop EEG adaptation at a T1-B responds to its own PBM's pulse-synchronous artifact | Medium (dose integrity / efficacy; bounded by PBM ceilings) | **not blocked by design** — PBM on a T1-B passes placement; it is unreachable today only because the power gate refuses every drive (`OI-FWHUB-09`) | `SYNC`-locked subtraction (`REQ-EMI-03/04`); extended `SH2-DRC-16`; `OI-MMSOCK-05` | **Open until measured** |
| `RISK-MMSOCK-05` | N1 (24 V `VLED`) to N4 (`ELEC`) insulation fault on a T1-B puts up to 24 V on a scalp contact | **High (candidate — not scored)** | exists whenever any PBM in the tile's cluster runs; **independent of co-drive** | none specified — `NP-RISK-003` has no row | **Unassessed — `OI-MMSOCK-07` routes it** |
| `RISK-MMSOCK-06` | tES heat concentrated under a pod adds to the tile's LED heat beyond what the tile NTC senses | Low (≤ 40 mW against ≥ 1.3 W) | not reachable (no tES at pods) | the 42 °C chain cuts the dominant term; `OI-MMSOCK-06` measures the local term | Accepted pending measurement |

---

## 10. Open items

| ID | Description | Owner | Blocking |
|---|---|---|---|
| ~~**`OI-MMSOCK-01`**~~ | ✅ **CLOSED 2026-09-23 — P-1 accepted:** T1 tES on conventional pads outside the lattice; T1-B stimulates only for tACS where a pod fits the per-phase ceiling. Consequences are `OI-MMSOCK-11` (pad hardware) and `-12` (propagation). *Was:* **Principal — T1 tES delivery electrode.** No T1-B pod (≤ 1.02 cm²) can carry any of the 14 shipped tDCS protocols under DI-SAFE-01: they need 8.0–24.0 cm², 9 of 14 more than a whole 40 mm tile face, and on a pod the MCU cuts each at 77–153 s (§3.6.2). `NP-HEX-ZM-001` §4a's premise that T1-B's dual-rated electrode delivers T1 tES does not survive the arithmetic. Options P-1 (a)–(d), §7.2 | Principal + Systems + Clinical | **Any T1 tES through the lattice; `NP-HEX-ZM-001` §4a; `OI-HEXTILE-05` scoping** |
| **`OI-MMSOCK-02`** | **IMPLEMENTED 2026-09-23 (P-5) — closes on SW-01 review.** Built as `NP_SESSION_STATUS_GEOM_REQ_BES` + a third gate arm, with the fixed pad constant as the area and **no wire field** (Rev 2 banner explains why); the lattice-electrode area is `OI-MMSOCK-10`'s. *Was:* **BES/tACS has no declared electrode area and no fail-closed geometry gate** (§3.6.1). Take Class C change C-1 (§5.3) and add `electrode_area_mcm2` to `np_mod_bes_tacs_params_t` (wire v4, `REQ-FWHUB-10`). SW-01 review required. Recommended **now**, independent of P-2, because the stim HAL (`OI-STIM-01`) must route to T1-B pods and makes the gap live without any wire change | FW (SW-01 + SW-02) + Safety | **Before `OI-STIM-01` routes any BES/tACS output to a lattice electrode** |
| **`OI-MMSOCK-03`** | The module inventory carries no electrode **contact area**, so the Class B cross-check of §5.2 item 3 has nothing to compare against. Needs an inventory field (UID-reported, NVRAM blob version bump) once pod geometry is fixed (`OI-HEXTILE-05`) | FW + HW | P-2 |
| **`OI-MMSOCK-04`** | Superposed DC + AC waveforms (oscillating tDCS): if wanted, define them as one channel and decide their waveform class for DI-SAFE-01 (§3.5, C-3) | Principal + Safety | Any superposed-waveform protocol |
| **`OI-MMSOCK-05`** | **Optically-induced artifact at a co-sited electrode is unmeasured.** Extend `SH2-DRC-16` so its setup includes a T1-B driving its own emitters while recording on its own electrode, and add a closed-loop self-artifact check: with the PBM envelope at *f*, adaptation keyed to the band containing *f* must not move (§3.2) | EE + FW + V&V | Claiming EEG-adaptive closed loop at T1-B sites |
| **`OI-MMSOCK-06`** | Measure pod-local temperature with tES and PBM co-driven at the worst authored T1-B case, and state the NTC's position relative to the pod (§3.3 (a)) | Thermal + HW | P-4 |
| **`OI-MMSOCK-07`** | **Route `RISK-MMSOCK-05` to `NP-RISK-003`**: N1 (24 V) to N4 (`ELEC`) insulation on a T1-B tile, with a scalp contact on N4. Needs a creepage/insulation requirement on the T1-B FPC and a hazard score; `NP-HW-HEXTILE-001` §4.5 (T1-B) is the owning layout | Safety + HW | T1-B layout (`OI-HEXTILE-05`; T1-B is out of `NP-HW-HEXTILE-001`'s current scope, §4.5) |
| **`OI-MMSOCK-08`** | `.npps` tACS `intensity` — peak or peak-to-peak — is not stated in `NP-NPPS-REF-001`, and it halves or doubles §3.6.2's phase-charge table. It also matters today, for the MCU's per-phase check on pads | FW + App | Correctness of every per-phase figure |
| **`OI-MMSOCK-09`** | **DECIDED 2026-09-23 (P-6): two electrodes per T1-B, one recording-only, one dual-rated.** Stays open only for (b), the unmeasured artifact at the recording pad; the contact-count change is `OI-MMSOCK-12`. *Was:* **Electrodes per tile, for co-drive rather than density (§3.8, P-6).** Linked to `NP-HW-EEGNET-001` `OI-EEGNET-19` (electrodes per tile) and `OI-EEGNET-20` (socket contact count in the MECH-2 force study), which it adds a second motive to: those items ask for density; this one asks for one recording-only pad beside one dual-rated pad. Two questions the density study does not carry: (a) the recommended split is one dual-rated + one recording-only, which holds the safety-MCU stimulation channel count flat; (b) **the stimulation artifact at a recording pad 20–29 mm from a pad carrying up to 2 mA is unmeasured** — whether it saturates the ADS1299 decides whether the second pad buys anything. Does **not** affect `OI-MMSOCK-01` or `-02` | Systems + ME + EE | **Socket tooling (`OI-SHELL2-09(i)`)** — the contact count is permanent after it |
| **`OI-MMSOCK-10`** | **Build P-2: a tES socket target, scoped by P-1 to tACS on a T1-B's dual-rated pad.** §5.1 option A (two disjoint bitmaps, wire v4, `NP_HUB_PROTO_TARGET_MAX` 16 → 32), **plus the electrode area of the targeted pad** — the first case where BES/tACS runs on something other than the fixed pad, and therefore where an area must travel with the command rather than come from `NP_BES_ELECTRODE_AREA_MCM2`. Needs `OI-MMSOCK-03` (contact area in the inventory) and `OI-HEXTILE-05` (pod geometry). Hub and app only; the Class C gate it needs is already built | FW + App | P-4 in practice; any tACS at a lattice electrode |
| **`OI-MMSOCK-11`** | **P-1 puts T1 tES on pads outside the lattice, and no such hardware is specified anywhere.** N4 is the only electrode network in the helmet (`NP-DRV-SHELL-002` §3.5) and `NP-ART-001` lists no pad, harness or connection point for T1 tES. Register it as an artifact, decide where the pads connect (hub accessory port, as cervical VNS does, is the precedent), and specify it — the measured pad area it produces is what closes `OI-CHARGE-07`'s BES limb | Systems + ME + EE | **Any T1 tES session on shipping hardware** |
| **`OI-MMSOCK-12`** | **Propagate P-1 and P-6 to the documents that own them** — not done here, because each is another owner's controlled document: `NP-HEX-ZM-001` §4a (T1-B = one recording electrode + one dual-rated; T1 tES on pads); `NP-HW-EEGNET-001` §0 and `OI-EEGNET-19` (answered: two, one dual-rated); `NP-HW-HEXTILE-001` §7.2 and `NP-DRV-SHELL-002` §5.1.4 (socket 19 → 20 contacts with a shared shield, which is `OI-EEGNET-20`'s force study); `NP-DRV-SHELL-002` §3.5 (per-cluster electrode mux width). **2026-09-23: the socket change lands on both tiers.** T1 tiles carry over to a T1 owner's T2 (`NP-REG-UPG-001` Rev 2 §7.0, `REQ-UPG-03`), so a 20-contact socket on one tier and 19 on the other would strand them. The shared socket tool makes this automatic mechanically. `REQ-UPG-03` makes it binding for the pinout and the inventory protocol | Systems + ME + EE | **Socket tooling (`OI-SHELL2-09(i)`)** |

---

## 11. What this change does to the code

**Test additions and one comment. No behaviour change.** Every assertion below passes against the code
as it stood before this change — that is the point: they pin decisions the code already embodies and
give each a reason of record.

| File | Change | Why |
|---|---|---|
| `firmware/hub_control/tests/np_socket_dispatch_tests.c` | `test_not_socket_addressable` extended from tDCS alone to **every electrode-bearing modality** — EEG, BES/tACS, tDCS, VNS/HRV, clinical tACS, HD-tDCS, cervical VNS — as drives and as stops | D-2: `REQ-FWHUB-33` was asserted for one electrical modality; §3.6 is the reason it must hold for all of them, and a later edit admitting BES/tACS "because it is just like PBM" is exactly `RISK-MMSOCK-01` |
| same | new `test_pbm_on_t1b_admitted`: a T1-B-shaped tile (660 + 808 + `NP_ELEM_DUAL_ELECTRODE` + PD + NTC) is admitted and driven like a T1-A, and the electrode element is neither required nor a reason to refuse | pins the behaviour P-3 recommends keeping. **If the principal decides P-3 against, this test is the one to invert** |
| `firmware/hub_control/src/np_socket_dispatch.c`, `include/np_socket_dispatch.h` | the gate-1 comment in each cites this document for why electrode modalities are not socket-addressable | a reader who finds the restriction should find its reason |

**Falsified, per `NP-CONV-001` §8.** Admitting `NP_MOD_BES_TACS` at gate 1 was tried against the
extended suite: it **fails the stop assertion and not the drive one**. That asymmetry is real and is
now written into the test: a stop skips every later gate, so only gate 1 can refuse it, while a
mis-typed drive also dies at the params gates because its block is read as the wrong struct. The stop
case is therefore the one that proves `REQ-FWHUB-33`; the drive case is kept because it is the
behaviour a reader expects to see asserted. Host suite: 39 of 39 pass.

`np_socket_dispatch_tests` gains no link dependency. `REQ-FWHUB-33`'s wording in `NP-FW-HUB-001` §10.1
is unchanged — it already says what the tests now pin.

---

### 11.1 Rev 2 — C-1 (P-5) and the area hand-off

| Where | Change |
|---|---|
| Class C — `firmware/common/include/np_spi_wire_types.h` | `NP_SESSION_STATUS_GEOM_REQ_BES` = bit 4 (bits 4–7 were unused) |
| Class C — `firmware/safety_mcu/` | `geom_required_bes` in the state; decoded in `np_safety_main.c`; third arm of `np_charge_monitor_geom_gate()`; a channel-5 area opens it; cleared per session |
| Class B — `firmware/hub_control/` | fifth flag in `np_safety_session_status_bits()`; `np_safety_spi_set_geom_required_bes()`; the runner's geometry scan extracted to `src/np_chan_decl.c`, which arms the BES gate and **sends every area it computes** |
| Tests | `np_charge_monitor_tests` +2 cases, `np_safety_spi_proto_tests` +1, `np_cvns_reenable_tests` +3, new `np_chan_decl_tests` (Class B 31 → 32, total 39 → 40). Four mutations — the old send rule, BES arming removed, the Class C gate line removed, the area never opening it — each fail the suite |
| CI | test counts in `build-all.yml` and `firmware-cross-build.yml` |

**Not changed:** the enable word, the heartbeat frame layout, the wire format, any app, any protocol file.
Both processors cross-compile on arm-none-eabi-gcc 13.2.1. `test_pbm_on_t1b_admitted`'s comment now cites
P-3 as decided.

## 12. Revision history

| Rev | Date | Author | Description |
|---|---|---|---|
| 2 | 2026-09-23 | NeurOne Firmware Engineering | **P-1…P-6 accepted by the principal as recommended; P-5 built.** Class C change C-1 — `NP_SESSION_STATUS_GEOM_REQ_BES` and a third geometry-gate arm — implemented and **pending SW-01 review**, taking the BES area from the fixed pad constant rather than an authored wire field, because `OI-CHARGE-07` makes a fixed part's area a device property and P-1 keeps T1 tES on pads. Building it found the runner sent electrode areas only in sessions with tDCS or HD-tDCS, so a VNS-only session was enforced 50× looser than designed — fixed (`NP-FW-HUB-001` Rev 3). §3.6.1's description of the BES default corrected in the banner, not rewritten. `OI-MMSOCK-01` closed; `-02` implemented; `-09` decided; new `-10` (build the tACS socket target), **`-11` (no T1 tES pad hardware exists)**, `-12` (propagate P-1/P-6 before socket tooling). §11.1 lists the code. |
| 1 | 2026-09-23 | NeurOne Firmware Engineering | **Initial issue.** Answers which modality combinations may share one hex-tile socket by reducing the socket to two physical lanes — optical and one `ELEC` conductor (§2). **Verdicts:** multi-wavelength PBM supported; PBM + EEG and PBM + tES at a T1-B **recommended allow** (no traced hazard; tES adds ≤ 40 mW against ≥ 1.3 W of LED heat); EEG + tES on one electrode and two electrical channels on one electrode **must not**, each traced (ADS1299 full scale and REQ-EMI-08; per-channel ceilings cannot see a superposition). **Two findings larger than the question:** the BES/tACS channel has **no geometry gate** and would enforce a ≤ 1.02 cm² pod against the 25 cm² default, **24.5× fail-open** (`OI-MMSOCK-02`, Class C change C-1 recommended now); and **no T1-B pod can carry any of the 14 shipped tDCS protocols** under DI-SAFE-01 — 8.0–24.0 cm² needed, 9 of 14 more than a whole tile face (`OI-MMSOCK-01`, principal). Specifies, without building, a per-(socket, lane) record that makes both must-nots unrepresentable, lane-scoped enable ownership, a two-bitmap tES target (wire v4) and a Class B area cross-check; states the SW-01 review scope (C-1…C-4; no enable-word or frame-layout change). No SHDR field added. Code: two test additions and one comment — no behaviour change. Five questions put to the principal (P-1…P-5); eight open items; six risk rows. **Amended within the same unmerged change (2026-09-23):** new §3.8 — more electrodes per tile: it moves §3.4's must-not from the socket to the electrode and resolves §3.5 across electrodes, but fixes neither §3.6 finding (four pods total 4.08 cm² against the library's smallest 8.0 cm²); recommends two electrodes, one dual-rated, if co-sited record-and-stimulate is wanted; P-6 and `OI-MMSOCK-09`, linked to `OI-EEGNET-19`/`-20`. Rev 1 is issued once, describing what merges. |
