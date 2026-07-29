# Human Factors Engineering Plan

**Project:** NeurOne
**Document:** NP-HFE-001
**Revision:** A
**Date:** 2026-07-27
**Status:** ACTIVE
**Effective Date:** 2026-07-27
**Author:** Steve Hickman (CEO, interim Quality authority)
**Approved By:** Steve Hickman, CEO
**References:** IEC 62366-1:2015+AMD1:2020; FDA "Applying Human Factors and Usability Engineering to Medical Devices" (2016); NP-DT-001 Rev A §3 (DI-USE-01..07, DI-PERF-24); NP-RM-001 Rev A (RISK-15, RISK-22); NP-TOOL-ZM-001 Rev A (OI-4 eject lever); NP-FAI-ZM-001 Rev A (FAI-A09–A15); NP-DP-001 Rev A §6, §8
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
| Zone module snap-in/eject (5-layer keying + eject lever) | CLAUDE.md §3 modality 1; RISK-15, RISK-22 |
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
| CT-01 | Insert PBM zone module into correct slot / orientation | Wrong zone inserted; module inserted at wrong contact angle giving poor thermal/optical contact | RISK-15 five-layer keying (asymmetric mechanical key + ZONE_ID resistor ladder + braille/raised numeral + tactile dots + bone-conduction audio confirmation) — errors are prevented by keying, not merely detected. **⚠ SUPERSEDED 2026-07-28:** ZONE_ID detection is retired (see `docs/np_fw_za_001.md`), and the mechanical-key layer is confirmed removed under SMART-1 — every socket accepts every module type, so there is no "wrong slot" for *type* anymore (see `docs/np_hex_zm_001.md` §4a). Orientation-only mis-insertion and position confusion (is this really the frontal-left slot?) remain real use errors — re-scope CT-01 to test those against the hex-tile insertion UX once designed, not type-mismatch. | Formative (confirm keying is discoverable without instructions) |
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
| DI-USE-05 (five-layer keying) | Formative — CT-01 |
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

---

## 11. Change Control

Any change to zone-module eject mechanism, shade system, status LED behavior, or onboarding consent flow after this plan is issued triggers re-evaluation of the affected critical task under NP-QMS-DC-001's design change process, and — if the change is significant — a re-run of the corresponding formative test before summative testing proceeds.
