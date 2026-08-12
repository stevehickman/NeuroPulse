# Risk File Re-Baseline and NP-RISK-001 Disposition

**Project:** NeurOne
**Document:** NP-RISK-002
**Revision:** 1
**Date:** 2026-08-11
**Status:** ACTIVE
**Effective Date:** 2026-08-11
**Author:** NeurOne Quality (interim: Steve Hickman, CEO)
**Approved By:** Steve Hickman, CEO
**References:** NP-RISK-001 Rev 3 (superseded — `docs/superseded/np_risk_001.docx`); NP-RM-001 Rev 1 §4 (scales), §8.1 (baseline), §8.2 (change control); NP-RISK-003 Rev 1; NP-RISK-004 Rev 1; NP-ART-001 Rev 1; NP-FAI-001 Rev 1 §7; NP-FMEA-001 Rev 4; NP-FMEA-GEOM-001 Rev 1; NP-QMS-DC-001 Rev 1; NP-QMS-CAPA-001 Rev 1; NP-DHF-001 Rev 27; ISO 14971:2019
**Related Issues:** —
**Gate:** NP-COORD-001 G2
**IEC 62304 Class:** N/A (hardware risk file)
**Supersedes:** **NP-RISK-001 Rev 3** — as the ISO 14971 risk file of record. Content is dispositioned in §3, not discarded.
**Parent Document:** NP-RM-001 Rev 1

---

> **Read this first.** `NP-RISK-001` was not a stale specification. It was **the ISO 14971 baseline
> risk file**: `NP-RM-001` §8.1 placed RISK-01…25 under QMS change control by naming that document,
> `NP-QMS-DC-001` lists it as the risk-management record type, `NP-QMS-CAPA-001` §CAPA routes risk
> updates to it, `NP-DHF-001` carries it as ACTIVE, and `NP-FMEA-GEOM-001` and
> `NP-REQ-FANHEALTH-001` were both written to feed into it. Thirty-eight references point at it.
>
> It was also scoped, in its own title, to *the Zone Module FPC* — an assembly the 2026-07-15
> hex-tile decision deleted. Both of those things were true at once, which is why it survived four
> months past the architecture that produced it.
>
> **This document re-baselines the risk file. It does not delete a single risk.** §3 gives all
> twenty-six RISK IDs an explicit disposition — retired, carried forward, or still open — with the
> evidence for each. Five are structurally eliminated by the architecture change, twenty carry
> forward into per-artifact registers, and one is closed by having been *confirmed true* rather than
> mitigated.

---

## 1. Scope and structure of the successor risk file

`NP-RISK-001` held one register for one artifact and was then asked to hold the whole system. The
successor is split by artifact, matching `NP-ART-001` §2:

| Document | Covers | Artifacts |
|---|---|---|
| **NP-RISK-002** (this) | Risk file index, `NP-RISK-001` disposition, and risks with no artifact home yet | — |
| **NP-RISK-003** | Hex-tile module — shell, window, gasket, FPC, emitters, metering | A1, A2 |
| **NP-RISK-004** | Shell, socket, interconnect and hub | A3–A9 |

**Identifier rule (`NP-CONV-001` §6).** The `RISK-NN` sequence is append-only and **closed at 26**.
No new hazard takes an `RISK-NN` number, and **no retired number is ever reused** — a reused number
would silently re-point every one of the thirty-eight inbound references. New hazards take
artifact-prefixed IDs: `RISK-HEX-nn`, `RISK-SHELL-nn`, `RISK-HUB-nn`.

---

## 2. What changed, and what did not

| | |
|---|---|
| **Unchanged** | Every severity, probability and acceptability judgement in `NP-RISK-001`. The `NP-RM-001` §4 scales. The ISO 14971 process. No risk is re-scored here — with one stated exception, RISK-16, whose basis is in `NP-RISK-003` §1.3. |
| **Unchanged** | The `RISK-NN` numbers themselves, and their meanings. |
| **Changed** | Which artifact each risk attaches to, and therefore which register holds it and which FAI item controls it. |
| **Changed** | Nine risks lose their referent entirely. They are struck through and retained, per `NP-CONV-001` §6 — never deleted. |
| **New** | Three risks that the architecture change *created* and one that it *confirmed*. Recorded in §5. |

> **How "unchanged" was established.** Severity labels for all nineteen carried risks that hold a
> register row were extracted mechanically from the superseded `.docx` body and diffed against the
> rows in `NP-RISK-003` / `NP-RISK-004`, per `NP-CONV-001` §8 — *cross-document agreement is verified
> by mechanical diff, never by review*. **The diff earned its cost immediately: it caught three
> silent re-scores in the first draft** — RISK-18 and RISK-21 had drifted HIGH → MEDIUM and RISK-22
> MEDIUM → HIGH during transcription. All three were restored to the source values. Twenty-six rows
> hand-carried out of a binary is exactly where a label flips, and no amount of reading finds it,
> because a wrong severity reads as a severity. The diff now returns exactly one mismatch — RISK-16,
> which is the intended re-rating — and that is the check to re-run on any future revision of these
> registers.

---

## 3. Disposition of RISK-01 … RISK-26

Legend: **RETIRED** — the hazard's mechanism no longer exists in the design. **CARRIED** — the
hazard survives; register named. **OPEN** — carried and still unmitigated. **CLOSED-CONFIRMED** —
the risk was that something might be true; it was investigated and found true, and the design
changed in response.

| ID | Title (abbrev.) | Was | Disposition | Where it goes now, and why |
|---|---|---|---|---|
| ~~RISK-01~~ | Molex SlimStack wrong connector family | CRITICAL / MITIGATED | **RETIRED** | The hazard was a ZIF connector family with a 20–30-cycle rating against a 1,000-cycle requirement. Hex tiles have **no connector** — `NP-DRV-SHELL-002` §8.2 replaced the tail with a back-face compression pad array, so module swap actuates zero mating cycles on any ZIF. Nothing to select wrongly. |
| RISK-02 | FPC power trace current capacity underspecified | HIGH / MITIGATED | **CARRIED** → `NP-RISK-003` | Mechanism survives, numbers change: 24 V rail (D-6) puts ~1.04 A per tile instead of 3.6 A per zone, so conductor sizing is easier — but **OI-HEXTILE-12 leaves the tile stack-up, trace width and copper weight unspecified**, so the risk is not yet re-mitigated at the new numbers. |
| RISK-03 | 400 mW/cm² peak irradiance pending regulatory opinion | HIGH / **OPEN** (external) | **OPEN** → `NP-RISK-003` | Wholly architecture-independent. Regulatory counsel has still not been commissioned. Hardware remains resilient to any outcome — a lower approved ceiling makes every electrical spec easier. |
| RISK-04 | PDMS–polyimide CTE mismatch → thermal-cycling delamination | HIGH / MITIGATED | **CARRIED** → `NP-RISK-003` | A materials property, not a geometry property. The 15:1 CTE mismatch and the SiO₂-interlayer mitigation are unchanged. **Its BLOCKING qualification (FAI-TC02, 200-cycle IEC 60068-2-14) has still never been run** — see `NP-FAI-001` OI-FAI-01. |
| RISK-05 | LED parallel-string current hogging without per-string CCC | HIGH / MITIGATED | **CARRIED, re-scoped** → `NP-RISK-003` | The mitigation moved rather than the hazard: `NP-HW-HEXTILE-001` **D-3** puts a driver on every tile, so per-string regulation is now an on-module design question governed by **OI-HEXTILE-07**, not a BOM line on a shared FPC. |
| RISK-06 | Reference photodiode receives direct LED light → dose metering invalid | HIGH / MITIGATED | **CARRIED** → `NP-RISK-003` | The tile still carries PD1 behind the window and still needs a baffle and lock-in detection. The 40 mm tile makes it **harder**: the baffle competes with emitter area on a much smaller face. Control is `NP-TOOL-HEXTILE-001` F-TH-04. |
| ~~RISK-07~~ | BCR421U 150 mA rating insufficient for 180 mA | HIGH / MITIGATED | **RETIRED** | Part-specific to a shared-FPC linear driver topology that D-3 replaced with an on-module driver. The generic hazard — *a driver operated beyond its rating* — is now inside **OI-HEXTILE-07** and is not a distinct register entry. |
| RISK-08 | LED V_f binning not specified → current matching degraded | HIGH / MITIGATED | **CARRIED** → `NP-RISK-003` | Still live, and **the mitigation regressed**: `NP-PROC-FPC-001`'s V_f binning requirement was written against emitters that **OI-HEXTILE-02** has now un-selected. There is currently no bound emitter to bin. |
| ~~RISK-09~~ | 0.35 mm pitch connector current rating | MEDIUM / MITIGATED | **RETIRED** | Same basis as RISK-01 — no connector. |
| RISK-10 | FPC fabricator defaults to ED copper unless RA is specified | MEDIUM / MITIGATED | **CARRIED** → `NP-RISK-004` | Applies to any flex in the design. The surviving flex is the **cluster tail** (`NP-DRV-SHELL-002` §8.2, static-only) and the L1 rigid-flex laminate. Attaches to A5, not to the tile. |
| ~~RISK-11~~ | Shell tooling does not enforce 25 mm bend radius at the routing path | MEDIUM / NEEDS REVIEW | **RETIRED — structurally eliminated** | The hazard was fatigue cracking of a dynamically flexed FPC over 1,000 module swaps. `NP-DRV-SHELL-002` §8.2: *"The module interconnect has no dynamic-flex path"* — REQ-BR2-02's dynamic set is **empty**. The residual static bend is a different, milder hazard and is covered by REQ-BR2-01/03/04 and SH2-DRC-04/05. This is the single largest reliability improvement the architecture change bought. |
| RISK-12 | Module alignment tolerance ±0.5 mm not validated by shell tooling | MEDIUM / MITIGATED | **CARRIED, re-scoped** → `NP-RISK-004` | Tolerance tightened and the population grew: ±0.4 mm lateral blind-mate, held **across a full cluster** of up to 6 tiles simultaneously (REQ-SKT-01, SH2-DRC-05a) rather than one module at a time. |
| RISK-13 | LED PWM carrier frequency unspecified → EEG band contamination | MEDIUM / MITIGATED | **CARRIED** → `NP-RISK-004` | Survives with a stronger control set: REQ-EMI-03 (sense-quiet windows), REQ-EMI-04 (**spread-spectrum PWM prohibited** — the opposite of the usual EMI remedy, because dither smears energy into the acquisition band), verified by SH2-DRC-16 at < 5 µVpp. |
| RISK-14 | Single PD cannot distinguish LED ageing from PDMS fouling | HIGH / MITIGATED | **CARRIED** → `NP-RISK-003` | Option B dual-PD (PD1 forward, PD2 backscatter) is retained by the hex tile in every type. The PD2 aperture position is re-derived for the 40 mm face — the retired X=33.0 / Y=39.0 mm coordinates were the centre of a 66 × 78 mm array and do not transfer. |
| ~~RISK-15~~ | Zone keying — wrong module in wrong slot | LOW / MITIGATED | **RETIRED** | Already recorded as structurally eliminated in `NP-RISK-001` Rev 3 itself. Tiles are type-agnostic with an orientation-only key; identity is socket (position) × module UID (type). There is no wrong socket. `NP-TOOL-ZM-SM-001`'s F-SM-03 smart-module key went with it. |
| RISK-16 | IPX4 compliance after user field replacement unvalidated | LOW / MITIGATED | **CARRIED — severity increased** → `NP-RISK-003` | The only risk whose exposure the architecture change made **worse**, and the increase is structural: 5 perimeter seals become ~30, and the qualification cycle count goes from 50 swaps to ~300 (`NP-FAI-001` §5). `NP-HEX-ZM-001` §6 flags a per-tile seam-length budget that has not been set. |
| RISK-17 | FPC routing path not coordinated with shell tooling | MEDIUM / NEEDS REVIEW | **CARRIED, re-scoped** → `NP-RISK-004` | The coordination hazard survives the loss of the FPC. Its mitigation is now `NP-DRV-SHELL-002` (requirements + 33-item DRC) plus `NP-REV-SHELL-001` (the review record that gates first cut). |
| RISK-18 | ZONE_ID ENABLE interlock creates a session-blocking availability failure | HIGH / MITIGATED | **CARRIED, re-scoped** → `NP-RISK-004` | The ZONE_ID pin is gone, but the hazard shape — *a presence-detect false negative blocks a session* — transfers intact to `SEAT#`, and gains a worse sibling: SH2-DRC-10b's partially-seated tile that **answers I2C while reading a silently wrong dose**. A false negative costs availability; that one costs correctness. `OI-HEXTILE-08` holds the debounce requirement. |
| RISK-19 | Co-moulded silicone gasket may delaminate from the module body | HIGH / MITIGATED | **CARRIED** → `NP-RISK-003` | Groove-plus-primer retention transfers directly to the hex tile perimeter. Multiplied by ~30 gaskets — see RISK-16. |
| RISK-20 | CFRP rim Ra ≤ 1.6 µm may require secondary operations | HIGH / **OPEN** | **OPEN** → `NP-RISK-004` | Never mitigated, and never architecture-dependent. Still needs written confirmation from a tooling manufacturer (`NP-PROC-SUP-001` SUP-M-07 / SUP-B-01, both BLOCKING). The rim in question moved from 5 shell slots to ~80 socket rims, which does not change the process question but multiplies its cost consequence. |
| RISK-21 | EEG electrode cable routing inside the shell unspecified | HIGH / MITIGATED | **CARRIED, re-scoped** → `NP-RISK-004` | There are no EEG *cables* to route — electrodes live inside T1-B tiles on L1 (`NP-DRV-SHELL-002` §3.5, N4 sized by channel count). The integrity requirement survives the disappearance of its subject; see RISK-24. |
| RISK-22 | Gasket compression force and key alignment impede replacement for users with reduced grip or tremor | MEDIUM / MITIGATED | **CARRIED, mechanism changed** → `NP-RISK-004` | The retired mitigation was a per-module eject lever (`NP-TOOL-ZM-001` F-08, 3:1 advantage, ≤ 1 N at the tip). Cluster clamps replace it: one over-centre lever per 3–7 tiles at **34.2–57.0 N plate load**. Whether that is one-handed-achievable at Parkinson's H&Y II–III is **OI-SHELL2-03(b)** and is open. The accessibility requirement is unchanged; the design that satisfies it is not yet shown to. |
| RISK-23 | Mould requires several simultaneous co-moulded features — omission risk before first cut | HIGH / MITIGATED | **CARRIED, re-scoped** → `NP-RISK-003` | The hazard is generic to any multi-feature mould and its control is a consolidated feature checklist — which is precisely what `NP-TOOL-ZM-001` was. Its successor `NP-TOOL-HEXTILE-001` inherits the role. |
| ~~RISK-24~~ | ≥ 15 mm FPC-to-EEG separation may be geometrically unachievable | MEDIUM / MITIGATED | **CLOSED-CONFIRMED** | The risk was that a stated requirement might be impossible. `NP-DRV-SHELL-002` §9.1 investigated it and found it **is** impossible in the hex architecture — electrodes now sit inside tiles on the same layer as the LED bus, so the 15 mm separation cannot exist at any geometry. The requirement was withdrawn and replaced by four mechanisms retaining the **< 5 µVpp** artifact threshold it was a proxy for. This is the one entry in the register that closed by being proven right. |
| RISK-25 | Cervical VNS cardiac reflex — bradycardia/asystole near the carotid sheath | LOW / MITIGATED | **CARRIED** → §4 (no artifact register) | Wholly independent of the cranial architecture. Mitigation (safety-MCU cardiac interlock, HR change > 15 BPM within 5 s → cutoff < 100 ms) stands. Pending its FAI bench — which, per `NP-ART-001` OI-ART-05, **cannot run because `NP-FAI-CVNS-001` has never existed** and A14 has no hardware specification. |
| RISK-26 | Fan/heatsink airflow loss → scalp face > 42 °C while junction NTC ≤ 62 °C | HIGH / ALARP | **CARRIED** → `NP-RISK-004` | Path B1 (scalp-facing NTC co-located with PD2) selected; constants provisional pending verification-grade CFD and the THERM-1b bench. **`OI-SHELL2-11` adds a second heat source on the same path** — 18 continuously dissipating cluster controllers behind the dominant outward resistance — which was not in scope when RISK-26 was scored. |

**Totals — counted from the table above, not asserted:** 26 dispositioned · **5 RETIRED** (RISK-01, -07, -09, -11, -15) · **20 CARRIED** (10 → `NP-RISK-003`, 9 → `NP-RISK-004`, 1 — RISK-25 — held in §4 with no artifact register) · **1 CLOSED-CONFIRMED** (RISK-24).

Of the 20 carried, **three remain OPEN or unmitigated at the new numbers**: RISK-03 (regulatory opinion never commissioned), RISK-16 (no seam-length budget for ~30 seals), RISK-20 (CFRP rim finish unconfirmed). A fourth, RISK-08, has **regressed** — its V_f-binning control was written against emitters that OI-HEXTILE-02 has since un-selected.

> **The counts above were wrong in the first draft of this document** — it claimed 9 retired and 15 carried, from memory rather than from the table. They are now derived by parsing the disposition column. A total that is written rather than counted is exactly the kind of claim that reads as agreement (`NP-CONV-001` §8) and is worth re-deriving on every revision of this table.

---

## 4. Risks with no artifact register

Two carried risks have no per-artifact register because their artifact has no specification document
(`NP-ART-001` OI-ART-04). They remain in the risk file and are held here until one exists:

| ID | Hazard | Held for | Blocking |
|---|---|---|---|
| RISK-25 | Cervical VNS cardiac reflex | A14 cervical VNS accessory | `NP-ART-001` OI-ART-04, OI-ART-05 |
| — | Intranasal probe, audio cup and auricular clip hazards are **not assessed at all**. `NP-RISK-001` never covered them, and their absence was invisible while the file was zone-module-shaped. | A11, A12, A13 | `NP-ART-001` OI-ART-04 |

> **This is a gap in the ISO 14971 file, not merely in the documentation.** Three shipping T1
> modalities — intranasal PBM, neural audio entrainment, auricular VNS/HRV — have had no hazard
> analysis. Recorded as **OI-RISK2-02**.

---

## 5. Risks the architecture change created

New hazards, carried in the per-artifact registers under prefixed IDs so the `RISK-NN` sequence
stays closed:

| ID | Hazard | Register | Source |
|---|---|---|---|
| `RISK-HEX-01` | ~30 series perimeter seals for one IPX4 claim, with no per-tile seam-length budget | `NP-RISK-003` | `NP-HEX-ZM-001` §6 |
| `RISK-SHELL-01` | Partially-seated tile answers I2C and returns a plausible but wrong dose reading | `NP-RISK-004` | `NP-DRV-SHELL-002` §5.1.3a, SH2-DRC-10b |
| `RISK-SHELL-02` | 18 continuously dissipating cluster controllers behind the dominant outward thermal resistance | `NP-RISK-004` | `OI-SHELL2-11` |
| `RISK-SHELL-03` | `SAFE_EN[n]` polarity inverted between `NP-DRV-SHELL-002` §6 and the safety MCU — a power-on reset that is "safe" under one convention is "stimulation enabled" under the other | `NP-RISK-004` | `OI-CONV-01` |
| `RISK-HUB-01…03` | Tethered cover reaching the fan intake; finger access through the service door; Boa fatigue at a non-replaceable hub-segment bend | `NP-RISK-004` | `NP-FAI-HUB-001` §9 |

---

## 6. Change-control effect

`NP-RM-001` §8.1 currently reads that RISK-01…25 as documented in `NP-RISK-001` are under QMS change
control effective 2026-05-13. That statement is amended, not withdrawn:

> The ISO 14971 risk file of record is **`NP-RISK-002` together with `NP-RISK-003` and
> `NP-RISK-004`**, effective 2026-08-11. `NP-RISK-001` Rev 3 remains the baseline record for the
> period 2026-05-13 to 2026-08-11 and is retained in `docs/superseded/`. The `RISK-NN` sequence
> remains closed at 26 and append-only. No risk was removed from the file by this re-baseline.

`NP-QMS-DC-001`, `NP-QMS-CAPA-001` §8.2 and `NP-DHF-001` are updated to point here.

---

## 7. Open items

| ID | Description | Owner | Blocking |
|---|---|---|---|
| **OI-RISK2-01** | Re-score the carried risks against the `NP-RM-001` §4 severity × probability scales **at the new architecture's numbers**. This document deliberately re-scores nothing — disposition and re-scoring are separate acts, and mixing them would hide which judgements changed. RISK-16 (5 → ~30 seals) and RISK-22 (≤ 1 N lever → 34.2–57.0 N plate) are the two whose scores most plainly no longer hold. | Quality + ME | ISO 14971 file currency |
| **OI-RISK2-02** | **Hazard analysis for A11 (audio cup), A12 (intranasal Y-probe) and A13 (auricular VNS/HRV clip).** Three shipping T1 modalities with no entries in any risk register, ever. Blocked behind `NP-ART-001` OI-ART-04 (they have no specification to analyse). | Quality + Systems | **ISO 14971 completeness; T1 release** |
| **OI-RISK2-03** | Confirm the nine RETIRED dispositions with a second reader. Each rests on a claim that a mechanism no longer exists in the design — the failure mode of this document is a hazard retired because its *old* description stopped matching, while the hazard itself moved somewhere nobody looked. RISK-11 and RISK-15 are the two worth re-testing hardest. | Quality | Risk file integrity |
| **OI-RISK2-04** | RISK-03 (400 mW/cm² regulatory opinion) has been OPEN and externally blocked since 2026-05-06 with no counsel commissioned. It is the only risk in the file whose owner is the CEO. | CEO / Regulatory Counsel | Irradiance ceiling; RSET values |

---

## 8. Revision history

| Rev | Date | Author | Description |
|---|---|---|---|
| 1 | 2026-08-11 | NeurOne Quality | Initial release. Re-baselines the ISO 14971 risk file after the 2026-07-15 hex-tile architecture change, replacing `NP-RISK-001` Rev 3 as the file of record. **All twenty-six RISK IDs dispositioned individually (§3)** — 5 retired with the mechanism that produced them (RISK-01, -07, -09, -11, -15), 20 carried (10 → `NP-RISK-003`, 9 → `NP-RISK-004`, RISK-25 held in §4), 1 (**RISK-24**) closed by being *confirmed*: the 15 mm PBM-to-EEG separation was found geometrically impossible in the hex architecture and the requirement was replaced by the < 5 µVpp threshold it was a proxy for. Records that **RISK-16 is the one risk the architecture change made worse** (5 → ~30 perimeter seals) and that **RISK-11 is the largest improvement** (dynamic-flex set is now empty). No risk is re-scored — deliberately, and OI-RISK2-01 holds that work. §4 surfaces that three shipping T1 modalities (intranasal, audio, auricular clip) have **never had a hazard analysis**, invisible while the file was zone-module-shaped. §5 records five new hazards the architecture created, under prefixed IDs so the `RISK-NN` sequence stays closed at 26 and append-only. §6 amends `NP-RM-001` §8.1's change-control statement. Raises OI-RISK2-01…04. |
