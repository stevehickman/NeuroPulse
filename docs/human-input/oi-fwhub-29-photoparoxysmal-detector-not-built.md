# OI-FWHUB-29: the Oz photoparoxysmal detector is listed as an ocular interlock and is not built

**Status:** ANSWERED 2026-11-05 (the principal): option A, then option C. **From:** a Claude Code session working for the principal. **To:** Risk, with Clinical (the detection
criterion) and FW Hub / Safety. No addressee is named.

## 1. The question

`CLAUDE.md` §4.2 lists, for EEG and visual stimulation, the interlock "photoparoxysmal detection → goggle halt, Oz electrode,
<200 ms, clinician-unlock for 3–30 Hz". `NP-RM-001` counts "Photoparoxysmal EEG detection → goggle LED cutoff within <200 ms" among
its controls (seizure from photostimulation is S3, a sustained seizure S4). `NP-FW-HUB-001` §8.6 lists it as the fourth of four ocular
layers, "as the code has it". **The code does not have it.** What do the documents say until it is built, and is the interim control
enough? Pick one:

| Option | What it means | Cost |
|---|---|---|
| **A. Say so, and tighten the interim control** | The documents state the detector is not built. Until it is, the hub (not only the app) refuses a photic frequency in 3–30 Hz unless a clinician unlock is present, so the one control that exists is not a warning that can be dismissed. | A Risk decision (`CLAUDE.md` §18: removing a control's claim is an ISO 14971 matter), a hub-side check, and wording changes in `NP-RM-001` and `CLAUDE.md` §4.2 (the latter is the principal's). |
| **B. Say so, keep the interim control as it is** | The documents state the detector is not built; the validator's warning (an error only where the clinician limit is set) stays the only control. | Wording changes only. Risk must accept the residual risk on that basis. |
| **C. Build the detector first** | Clinical defines the detection criterion; FW builds it on the ADS1299 Oz channel and wires it to `np_mod_visual_ppx_halt()`. | The largest: a clinically defined criterion (not invented here), a latency budget inside 200 ms, and tests. The visual modality stays unusable on a person until it is done. |

**Recommendation: A**, and C as the work item. B would leave a seizure-severity hazard resting on a dismissible warning.

## 2. What is known (read from the code at `main`, 2026-11-04)

- **No detector exists.** The EEG module (`np_mod_eeg.c`) reads band powers for neurofeedback and does no Oz analysis for photoparoxysmal
  activity. Nothing in `firmware/`, `common/` or the apps does.
- **The halt exists and is never called.** `np_mod_visual_ppx_halt()` stops the LEDs, withdraws the visual enable and calls
  `np_runner_abort(NP_ABORT_EEG_SEIZURE)`. **It has no caller anywhere**; only the run-loop test reaches it. Its own comment says it is
  "called from EEG ISR / task when photoparoxysmal pattern detected at Oz".
- **The runner bit for it, `NP_EV_EEG_PPX`, has no producer either.** It duplicates the halt's route and could be dropped; the missing part is the detector.
- **The live control is weaker than the table suggests.** In the protocol validator (`common/npps-core/src/validate.rs`) a photic frequency
  in 3–30 Hz (`VISUAL_HIGH_RISK_MIN_HZ`, `_MAX_HZ`) is an **error only if the clinician limit `blockHighRiskRange` is true**; otherwise
  it is a **warning**. The hub enforces nothing of it.
- **Other layers that do exist:** IR proximity, the goggle Hall edge as an interrupt (`REQ-FWHUB-21`), the hardware current limit at the
  IEC 62471 ceiling; a visual build needs an electrode at the Oz socket (`ISC-64`); bench mode excludes the visual modality (`D-6`).
- **Nothing is exposed today:** no hardware is committed (`CLAUDE.md` status). This is a design-time gap, not a field defect.
- **Not tracked before this:** no open item named the missing detector.

## 3. Also found, for the record (no safety gap)

- **`NP_EV_THERMAL_FAULT` has no producer.** Over-temperature is handled elsewhere: the safety MCU's thermal interlock reaches the hub as a safety fault
  (abort reason "safety MCU"), a socket whose NTC is at or over `NP_PBM_THERMAL_FAULT_C` has every channel latched off, and the 42 °C / 62 °C hardware limits sit
  below. The one effect of the missing producer is that the session-end record can never say `NP_ABORT_THERMAL`. Whether a hub-detected module over-temperature
  should end the session is a design question, not a hazard.
- **`NP_EV_POWER_FAULT` has no producer, and cannot have one yet.** The hub has no input that reads the negotiated USB-C PD contract, and the power
  governor refuses every load (`np_pbm_power_admit`, `OI-HEXTILE-09`, `OI-SESPWR-03`). It fails closed.
- **`NP_EV_SAFETY_FAULT`** is produced, by the heartbeat task, and is not part of this.

## 4. What I need back

The option (A, B or C); if A or C, who owns the interim wording and the detection criterion; a short reason; and your name, role and date.
Nothing in this file is a decision. Until an answer is recorded, `NP-FW-HUB-001` states the detector is not built and `CLAUDE.md` §4.2 and `NP-RM-001` are unchanged.

## 5. Answer received, 2026-11-05

The principal ruled: **pick option A and then C.** Recorded in `docs/status/completed-decisions.md`.

- **A is done:** `CLAUDE.md` Rev 79 §4.2 and `NP-RM-001` say the detector is not built, and the hub refuses photic driving at 3 to 30 Hz
  outright (`REQ-FWHUB-65`). There is no clinician unlock, because the mechanism a clinician unlock needs (a signed field in the
  descriptor, and a rule for who may set it) does not exist and is not invented here; the refusal is therefore absolute until the detector
  is built.
- **One protocol is affected:** `protocols/predefined/13-focus-prime.npps` drives its visual block at 20 Hz and is now refused at the
  hub. It is a clinical protocol and is not edited here (`OI-NPPS-LIMITS-01`). **Update 2026-11-05:** the hub now also refuses it when it is sent
  (`NP_HUB_ERR_PHOTIC_ZONE`, `REQ-FWHUB-66`), so the person is told at upload and not when the session runs.
- **C is owed:** the detector. Its first need is the criterion only Clinical can define: `docs/human-input/oi-fwhub-29-c-detection-criterion.md`.
