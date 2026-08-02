# Human Factors Engineering Plan

**Project:** NeurOne
**Document:** NP-HFE-001
**Revision:** A
**Date:** 2026-07-27
**Status:** ACTIVE
**Effective Date:** 2026-07-27
**Author:** Steve Hickman (CEO, interim Quality authority)
**Approved By:** Steve Hickman, CEO
**References:** IEC 62366-1:2015+AMD1:2020; FDA "Applying Human Factors and Usability Engineering to Medical Devices" (2016); NP-DT-001 Rev A §3 (DI-USE-01..07, DI-PERF-24); NP-RM-001 Rev A (RISK-15, RISK-22); NP-TOOL-ZM-001 Rev A (OI-4 eject lever); NP-FAI-ZM-001 Rev A (FAI-A09–A15); NP-DP-001 Rev A §6, §8; NP-HFE-002 Rev A (accessible zone-module position identification — child document, CT-01)
**Related Issues:** —
**Gate:** NP-COORD-001 G3 (VE-12)
**IEC 62304 Class:** N/A
**Applicable Standard:** IEC 62366-1:2015+AMD1:2020; FDA HFE Guidance 2016
**Next Review:** At G2 (formative results) and G3 (summative results) gate reviews
**Supersedes:** None (first issue). Referenced as "planned Month 9" in NP-DHF-001, NP-DP-001, NP-DT-001 VE-12/OI-DT-03 prior to this document.

---

## 1. Purpose and Scope

This document is the Human Factors Engineering (HFE) plan for NeurOne, satisfying IEC 62366-1 and FDA's 2016 HFE Guidance. It defines the use specification, the use-related risk analysis (URRA), the formative evaluation plan, the summative validation plan, and the acceptance criteria that close NP-DT-001 VE-12 and discharge DI-REG-07.

**In scope:** T1 NeurOne Home (all 8 modalities) and T2 NeurOne Pro (all 11 modalities), the companion iOS/Android app, and the control hub. Both the general consumer/wellness user population (T1) and the clinician/patient population (T2) are covered. The zone-module eject mechanism's accessibility for users with reduced hand dexterity (RISK-22) is treated as its own critical-task line because it is the one interaction NeurOne has explicitly designed for a named vulnerable population (Parkinson's, post-stroke hemiplegia).

**Out of scope:** T2 TMS coil placement by a clinician is evaluated as a clinician-only task (different training assumption than self-administered T1 tasks) and is deferred to the T2-specific summative addendum planned alongside the 510(k) submission; it is noted here but not detailed.

**This document does not itself contain formative or summative test results.** It is the plan. Results are entered into this document (as revision updates) and into NP-DT-001 VE-12 as they become available, per the schedule in §7 and §8.

---

## 2. Regulatory Basis

| Requirement | Source | How this plan satisfies it |
|---|---|---|
| Use-related risk analysis before design freeze | IEC 62366-1 §5.1–§5.4 | §4–§5 below |
| Formative evaluation of user interface design | IEC 62366-1 §5.7; FDA HFE Guidance §7 | §7 below |
| Summative (validation) usability testing of critical tasks | IEC 62366-1 §5.8; FDA HFE Guidance §8 | §8 below |
| Human Factors Engineering Report submitted with 510(k) (T2 only) | FDA HFE Guidance Appendix A | §9 below |
| Traceability of usability risk controls to design outputs | ISO 13485:2016 §7.3; NP-DT-001 | §10 below |

T1 (FDA-exempt wellness) does not require a formal HFE submission, but NeurOne applies the same URRA/formative/summative discipline to T1 because (a) the zone-module and shade-system hardware is shared across T1 and T2, and (b) RISK-22 explicitly targets a population (Parkinson's, post-stroke) that a wellness product would not otherwise be obligated to accommodate — the accommodation is a design commitment independent of regulatory tier.

---

## 3. Device Description Summary (HFE-relevant interfaces only)

Full device description: CLAUDE.md §1–§4. HFE-relevant physical and software interfaces:

| Interface | Relevant sections |
|---|---|
| Boa-style occipital fit dial | CLAUDE.md §4.4 |
| Zone module snap-in/eject (orientation feature + software placement validation + accessible placement design; cluster eject) | CLAUDE.md §3 modality 1; RISK-15, RISK-22; `docs/np_hex_zm_001.md` §4a/§5.4a; **NP-HFE-002** |
| Accessible position identification — tactile landmark grid, module-type tactile marking, companion-app guided placement | **NP-HFE-002** (`docs/np_hfe_002.md`); RISK-15 |
| Shade system (S1/S2/S3 swap) | CLAUDE.md §3 modality 8 |
| Status LEDs (power, in-use, fault) | CLAUDE.md §4.7 |
| Electrode/hydrogel tip replacement | CLAUDE.md §3 modality 3, 6 |
| App: session start, protocol selection, consent flows | app/ios/ISA.md |
| App: Adaptive Adjustments card, session history | NP-PRIV-NOTICE-001 §4 |

---

## 4. Intended Users, Uses, and Use Environments

| Population | T1 or T2 | Characteristics relevant to HFE |
|---|---|---|
| General adult consumer (self-administered) | T1 | Ages 18+; head circumference 52–62 cm (1 adult SKU); no assumed prior neurotech experience; onboarding via in-app setup wizard is the only training |
| Adult consumer with reduced hand dexterity (Parkinson's Hoehn & Yahr stage II–III, post-stroke hemiplegia) | T1 (accessibility target for zone module eject only) | Reduced grip strength and fine-motor precision; RISK-22 Option A (≤1 N eject force via 3:1 mechanical-advantage sliding lever) is the design control under test |
| Caregiver / bystander | T1 | Not a device operator; must be able to read the amber in-use LED at ≥3 m to confirm correct protocol without interacting with the device (DI-USE-03) |
| Clinician (neurologist, psychiatrist, TMS clinic staff) | T2 | Assumed trained on T2 scripting API and clinical protocols; not a naive user — training material is in scope, not a use-error mitigant of first resort |
| Patient under clinician supervision | T2 | May have the condition being treated (e.g., depression, PTSD) — cognitive/motor considerations per indication; device is applied or supervised by clinician for T2-only modalities (TMS, clinical tACS, HD-tDCS) |

**Use environments:** home (T1, primary), clinic/office (T2, primary), and travel/mobile via power bank (Mode 3 autonomous, both tiers). No use in the shower or submerged (IPX4 rating covers incidental splash only).

**Training:** T1 has zero mandatory training — the in-app setup wizard and onboarding consent flow are the entire training intervention, consistent with FDA general wellness expectations. T2 clinician training is a service-network responsibility (see `docs/reference/service-network.md`) and is out of scope for this plan except where it defines the "trained user" assumption for T2-only critical tasks.

---

## 5. Use-Related Risk Analysis (URRA)

### 5.1 Method

Per IEC 62366-1 §5.1–§5.4: for each user interface element, identify potential use errors, determine whether the resulting harm could be significant (informs "critical task" designation), and cross-reference to the existing ISO 14971 hazard analysis in NP-RM-001 where a design control already exists.

### 5.2 Critical tasks (failure could plausibly result in harm)

A task is **critical** if a use error would not be readily detected/corrected by the user and could result in harm above negligible severity, per FDA HFE Guidance §3.

| ID | Critical task | Potential use error | Existing design control (NP-RM-001 cross-ref) | HFE test type |
|---|---|---|---|---|
| CT-01 | Insert PBM module into any socket with correct orientation | Module inserted with wrong orientation, giving poor electrical/thermal/optical contact; OR module seated in a socket position that doesn't match what the running protocol intended (mechanically succeeds — all sockets share one shape — but is functionally wrong) | **REDESIGNED 2026-07-28 (replaces RISK-15 five-layer keying, retired):** All sockets share **one identical shape/footprint** — any module type mechanically fits any socket (SMART-1, `docs/np_hex_zm_001.md` §4a); there is no "wrong slot" to physically block anymore, only wrong *orientation*. Two independent controls, not one keying scheme: (1) **Orientation** — the socket/module mating feature is intentionally *not* rotationally symmetric where pin alignment requires it (power, data), so a module can only seat one way, without that implying anything about module *type*. (2) **Position correctness** — established in software, not mechanically: `np_module_map` auto-inventories each socket's module by UID, and `np_module_map_check_placement()` validates the live inventory against the running protocol's required module map before the protocol is allowed to start; a mismatch blocks the session rather than silently misdosing. (3) **Accessible position identification** — **gap CLOSED 2026-07-31 by NP-HFE-002** (`docs/np_hfe_002.md`), which replaces RISK-15 Layers 3/4/5 rather than re-scaling them. The retired braille/raised-numeral and N-tactile-dot layers were designed for exactly 5 positions and cannot be re-scaled to ~80 sockets — not only because an 80-symbol tactile code is not distinguishable by touch, but because the hex lattice *tessellates*, leaving only a few millimetres of structural web between sockets, and an ISO 17049 braille cell needs a ~6 × 10 mm footprint. NP-HFE-002 splits the problem the way the architecture already split identity (socket = position, module = type): **tactile marking follows *type* onto the module** (closed 3–4 member set, where tactile symbols work — NP-HFE-002 §7.3), while **position moves to guidance** — a small closed set of coarse tactile landmarks on the shell (dashed midline spine, three coded band ridges, standard-electrode-site markers) that makes a spoken instruction followable by touch (§7.1), plus **companion-app guided placement as the primary channel** (§7.2), with `np_module_map_check_placement()` as the backstop. **Bone-conduction audio (Layer 5) remains unusable for this task at any socket count — corrected 2026-07-28:** it requires transducer contact with the head, but inserting a module requires the helmet off-head. Insertion-time confirmation moves to the companion app, which already receives real-time module-status over BLE (`HardwareSetupManager` `SetupStep.zoneModules`); bone conduction is retained for while-worn readback only. See `docs/np_hfe_002.md`, `docs/np_fw_za_001.md`. | Formative (confirm orientation is discoverable without instructions; confirm placement-mismatch blocking is understood, not just tolerated; NP-HFE-002 §8 HFE-R-01/02/05/06/08/10 with blind and low-vision participants — note this is a **different population** from the §7.2 Parkinson's/post-stroke cohort and needs its own recruitment channel, NP-HFE-002 OI-HFE2-08) + **summative** (HFE-R-13: a blind user completes a standard T1 build unaided) |
| CT-02 | Extract zone module (all users, esp. Parkinson's/post-stroke) | Excess force required; user cannot extract module for cleaning/upgrade, or extracts wrong module | RISK-22 Option A: ≤1 N sliding eject lever, 3:1 mechanical advantage | Formative + summative (see §7.2, §8) |
| CT-03 | Select correct shade (S1 opaque vs S2/S3) before an immersive visual session | User runs a photic-driving session without S1 opaque shade, increasing external light artifact / photoparoxysmal trigger risk from ambient flicker interaction | Hardware: Hall sensor detects goggle lift regardless of shade; IR proximity + photoparoxysmal EEG detection at Oz halts session <200ms independent of shade choice — shade selection affects comfort/immersion, not the safety interlock | Formative (confirm shade swap mechanism is intuitive; safety is not shade-dependent) |
| CT-04 | Recognize and respond to fault indication (red power LED blink) | User continues a session, or attempts to restart, without addressing the fault | Firmware: fault conditions independently gate stimulation at the safety MCU (CLAUDE.md §4.2) regardless of whether the user notices the LED — LED is informational, not a safety interlock | Formative |
| CT-05 | Correctly interpret amber in-use LED pulse rate as caregiver | Caregiver misjudges protocol frequency at a glance (informational only, not a safety task) | DI-USE-03 | Formative (usability only — not safety-critical, included because it is the one interface element explicitly for a bystander) |
| CT-06 | Complete onboarding consent flow (age gate, BIPA/biometric release, research consent layers L1–L4) | User does not understand what they are consenting to; unintentionally opts into research contact or blanket consent | App: sequential mandatory screens, checkbox-gated Continue, plain-language decision-support document (CLAUDE.md §6.2); OI-PA-04 Privacy Lead copy sign-off | Formative (comprehension testing) |
| CT-07 | Respond to HRV breathing pacer / session controls without misreading coherence score as a clinical diagnosis | User over-interprets a wellness biofeedback score as medical information | App copy; NP-PRIV-NOTICE-001 §4 plain-language framing | Formative |

**Non-critical but usability-relevant tasks** (evaluated formatively only, no summative requirement): Boa dial fitting, hydrogel tip replacement, charging, cleaning, S3 Rx lens installation, app protocol browsing.

### 5.3 Tasks explicitly excluded from critical-task status (with rationale)

| Task | Why not critical |
|---|---|
| tDCS/tACS current level selection | App-constrained to safe presets; hardware charge-density limit (40µC/cm²) is enforced by the safety MCU and cannot be overridden by any user action, correct or incorrect |
| PBM dose/duration selection | Session presets only; real-time J/cm² dose metering and hardware duty-cycle enforcement (≤25%) make user timing error non-hazardous |
| TMS coil positioning (T2) | Performed by trained clinician, not the patient; evaluated under T2 clinician-training program, not this consumer-facing URRA |

---

## 6. Traceability to Design Inputs

| Design Input (NP-DT-001) | HFE treatment |
|---|---|
| DI-USE-01 (head fit, 52–62 cm, 5-position bridge) | Formative only |
| DI-USE-02 (zone module eject ≤1 N, Parkinson's target) | Formative + summative — CT-02, §7.2, §8 |
| DI-USE-03 (status LED caregiver readability) | Formative — CT-05 |
| DI-USE-04 (interface protection covers, tethered) | Formative only |
| DI-USE-05 (orientation feature + software placement validation + accessible position identification) | Formative + summative — CT-01; design in **NP-HFE-002**, requirements HFE-R-01..15 (§8 there), summative at HFE-R-13 |
| DI-USE-06 (EEG pod contact force) | Formative only |
| DI-USE-07 (electrode tip replacement) | Formative only |
| DI-PERF-24 (zone module ≤1 N eject force, ≥1,000 insertion cycles) | Formative + summative — CT-02 |
| DI-REG-07 (HFE testing overall) | This entire document; closed at §9 |

---

## 7. Formative Evaluation Plan

Formative evaluation identifies use-interface problems early enough to correct them before tooling is finalized. Conducted on functional prototypes or high-fidelity mockups, not necessarily production hardware.

### 7.1 General formative testing (CT-01, CT-03, CT-04, CT-05, CT-06, CT-07)

- **Method:** Simulated-use interviews + task completion, think-aloud protocol.
- **Population:** n=5–8 representative T1 users per round (general adult consumer, no reduced-dexterity requirement for these tasks).
- **Target window:** Month 10–12, concurrent with G2 tooling finalization so findings can still inform mold design review (NP-TOOL-ZM-001 §5 checklist, NP-TOOL-SHELL-001 checklist).
- **Deliverable:** Formative findings memo per round, filed as a revision update to this document; any design change triggers the NP-QMS-DC-001 design change process.

### 7.2 Eject lever formative study (CT-02) — Parkinson's / post-stroke population

This is the accessibility-specific study named in `docs/status/pending-decisions.md` and cross-referenced at NP-TOOL-ZM-001 OI-4 and NP-FAI-ZM-001 FAI-A15.

- **Population:** n=5 subjects with Parkinson's Hoehn & Yahr stage II–III or post-stroke hemiplegia affecting hand function. This is a **human subjects study requiring IRB review** — see NP-IRB-001. It is procedurally distinct from the general formative testing in §7.1 (which is not a regulated human-subjects research activity when limited to usability observation of a commercial product's ordinary use, per FDA HFE Guidance's usability-testing carve-out) precisely because it targets a clinical population by diagnosis; NeurOne's IRB counsel determines at protocol-drafting time whether this specific study needs full IRB review or qualifies for exempt/expedited review, and that determination is recorded in NP-IRB-001 rather than assumed here.
- **Task:** Extract a zone module using the sliding eject lever, unaided, from a device already fitted and powered off.
- **Measures:** Peak extraction force (calibrated force gauge on the lever, not the module — target ≤1 N per RISK-22), time to complete, number of attempts, qualitative difficulty rating (5-point scale), spontaneous verbalized confusion.
- **Acceptance threshold carried into summative (§8):** No subject requires >3 attempts or reports "difficult"/"very difficult"; peak measured force does not exceed 1.5× the ≤1 N design target during any successful extraction (headroom for calibration tolerance).
- **Target window:** Month 10–12. Requires: (a) IRB protocol active (NP-IRB-001), (b) functional zone module + eject lever hardware from FAI-A09 lifecycle testing, (c) recruitment via a clinical partner (candidate: neurology/rehab clinic contact, TBD — recruitment channel is an open item, see §11).
- **Relationship to FAI-A09–A15:** NP-FAI-ZM-001's accessibility FAI items are bench/engineering pass criteria (force gauge on the mechanism alone); this study is the human-factors confirmation that the mechanism, once meeting FAI-A09 bench criteria, is actually usable by the target population. FAI-A09 passing is a prerequisite gate before this study is scheduled — testing hardware that doesn't yet meet its own bench spec would waste subject time on a known-fail configuration.

### 7.3 Formative findings disposition

All formative findings are logged with: task, observed use error or difficulty, severity (per NP-RM-001 S1–S5), proposed design or labeling mitigation, and disposition (accepted / deferred with rationale / rejected with rationale). No formative finding may be silently dropped — deferred or rejected findings require a documented rationale entry, consistent with the NP-QMS-DC-001 design review record requirement.

---

## 8. Summative (Validation) Evaluation Plan

Summative testing validates that the final, production-representative design is safe and effective for its critical tasks, without coaching, using only the labeling/training that will actually ship.

- **Population:** n=15 minimum, representative of the intended T1 user population (per NP-DP-001 §6.4 gate criteria), stratified to include the accessibility population for CT-02 specifically (a subset of the 15, or a supplementary cohort — sizing finalized once formative results (§7.2) are in, since a clean formative pass may allow the summative accessibility check to be folded into the general n=15 rather than requiring a separate clinical cohort).
- **Tasks tested:** All critical tasks from §5.2 (CT-01 through CT-07), performed unaided using only shipped labeling/in-app guidance — no verbal prompting from test administrators beyond what a real user would have.
- **Pass/fail:** Per FDA HFE Guidance — use errors and close calls are documented and root-caused; the study concludes when either (a) no use errors occur across all critical tasks, or (b) any observed use error is shown not to result in harm above negligible severity (traced back to the NP-RM-001 hazard analysis), or (c) a design or labeling change is made and the affected task is retested.
- **Target window:** Month 14–18, per NP-DP-001 §6.4 (G3 gate) and NP-DT-001 VE-12.
- **Independence:** Test administrators must not be the engineers who designed the interface under test (mirrors the NP-QMS-CAPA-001 independence-of-review principle).

---

## 9. HFE Report and Regulatory Filing

A Human Factors Engineering Report is authored on completion of summative testing, per FDA HFE Guidance Appendix A structure: device description, intended users/uses/environments, summary of known use problems (from any predicate/prior-generation device — none exists for NeurOne, so this section states that explicitly), analysis of hazards related to use, description of formative evaluations and how findings were addressed, and the summative test protocol, results, and conclusion.

- **T1:** Report retained in the DHF; not submitted to FDA (wellness exemption), but available on request.
- **T2:** Report is a required component of the 510(k) submission (both cervical VNS NP-REG-CVNS-001 and any future T2 predicate-based submissions that share the zone-module/shell/shade hardware).

---

## 10. Open Items

| ID | Description | Blocking | Target |
|---|---|---|---|
| OI-HFE-01 | Select formative-study facilitator (internal or contracted usability specialist) | §7.1, §7.2 start | Month 9 |
| OI-HFE-02 | Identify clinical recruitment channel for the n=5 Parkinson's/post-stroke eject-lever study | §7.2 start | Month 9–10 |
| OI-HFE-03 | IRB protocol (NP-IRB-001) active and approved/exempt-determined before any subject is enrolled in §7.2 | §7.2 start | Concurrent with NP-IRB-001 Month 9 target |
| OI-HFE-04 | Confirm FAI-A09 bench pass (eject force ≤1 N on production-representative hardware) before scheduling §7.2 | §7.2 start | Pre-Month 10 |
| OI-HFE-05 | Finalize summative accessibility-cohort sizing once §7.2 formative results are available | §8 | Month 12 |
| OI-HFE-06 | T2 clinician-facing critical tasks (TMS coil placement, T2 API console) — scope a T2-specific summative addendum | 510(k) submission | Concurrent with T2 510(k) prep |
| OI-HFE-07 | Recruitment channel for **blind and low-vision** formative participants (NP-HFE-002 HFE-R-01/02/05/06/08/10). Distinct population from OI-HFE-02's Parkinson's/post-stroke cohort — do not assume one channel serves both | NP-HFE-002 formative start | Month 9–10 |
| OI-HFE-08 | NP-HFE-002 formative must be run on **helmet-representative concave curvature**, not a flat mock-up — tactile counting on a dome is the design's primary empirical risk (NP-HFE-002 OI-HFE2-03). A flat-mockup result would be falsely reassuring | NP-HFE-002 HFE-R-01/02/03 credibility | Pre-Month 10 |

---

## 11. Change Control

Any change to zone-module eject mechanism, shade system, status LED behavior, or onboarding consent flow after this plan is issued triggers re-evaluation of the affected critical task under NP-QMS-DC-001's design change process, and — if the change is significant — a re-run of the corresponding formative test before summative testing proceeds.
