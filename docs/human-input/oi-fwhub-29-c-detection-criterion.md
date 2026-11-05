# OI-FWHUB-29 (C): what is a photoparoxysmal response at Oz, for a detector that must halt the goggles in under 200 ms?

**Status:** DRAFT, not sent. **From:** a Claude Code session working for the principal. **To:** Clinical (an epileptologist or a
clinical neurophysiologist is the right reader), with Risk and FW Hub / Safety. No addressee is named.

## 1. The question

The principal ruled (2026-11-05) that the Oz photoparoxysmal detector be built (`OI-FWHUB-29`, option C). **Engineering cannot define what the
detector detects.** Until Clinical does, the hub refuses photic driving at 3–30 Hz outright (`REQ-FWHUB-65`) and stays refusing it.
What is needed:

1. **The detection criterion.** What pattern in an Oz recording, during photic stimulation, counts as a photoparoxysmal response that
   must stop the stimulus? (A spike-wave or polyspike-wave discharge, a generalised or posterior-dominant paroxysmal response, a
   threshold on band power or on a spike detector's output, a stimulus-locked response, or something else.) State it as something
   a program can compute from a sampled signal, with its parameters.
2. **How sure it must be.** The acceptable missed-response rate and the acceptable false-halt rate (a false halt costs a session; a miss
   is a seizure risk, S3 to S4 in `NP-RM-001`). Compare the same question for the cervical cardiac cutoff, `OI-CVNS-13`.
3. **Whether the zone is 3 to 30 Hz.** Those bounds are carried in the code and the protocol validator with no source (`UC-121`): cite the
   source or give another.
4. **What the detector does when it cannot see.** With no valid Oz signal (off-head, lead-off, saturated, a flat line) it should, I propose,
   keep the zone refused or halt a drive already running. Say which. (`NP-FW-BENCH-001` already notes the halt is vacuous off-head.)
5. **What a clinician unlock means once the detector exists**, if the zone may ever be driven: who may unlock, for how long, and what the
   detector does while unlocked.
6. **How it is validated.** Recorded photoparoxysmal EEG, synthetic signals, or both; and whether a clinical study is needed before it
   replaces the refusal. This decides when the refusal can be lifted.

## 2. What engineering supplies

- **The signal:** the ADS1299 at 500 Hz, 24-bit, 8 channels; the Oz element is the detector's input (`NP-HW-EEGNET-001`, `ISC-64`). The EEG task already
  receives the samples (`np_mod_eeg.c`); it does no Oz analysis today.
- **The action:** `np_mod_visual_ppx_halt()` already stops the LEDs through the hardware layer, withdraws the visual enable and ends the
  session with `NP_ABORT_EEG_SEIZURE`. The LED cut does not wait for the 200 ms heartbeat. It has no caller.
- **The budget:** under 200 ms from the response to the LEDs off (`CLAUDE.md` §4.2). The window the criterion needs counts against it, so the
  criterion and the budget must be settled together.
- **The proposed build:** a detector whose criterion is a parameter set, and which **cannot be armed until the parameters are marked
  validated**, so the refusal in `REQ-FWHUB-65` stays until then and a half-defined detector never gives false assurance. Host tests with
  synthetic signals; the clinical validation is Clinical's to specify.

## 3. What I need back

The criterion (1), the rates (2), the source or the replacement for the 3 to 30 Hz bounds (3), the behaviour with no signal (4), the unlock
meaning (5), and the validation route (6); a short reason; and your name, role and date. Nothing in this file is a decision, and nothing
in the repository may act as if a criterion has been given.
