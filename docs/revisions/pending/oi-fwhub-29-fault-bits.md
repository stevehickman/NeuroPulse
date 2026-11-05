## DHF
author: NeurOne Firmware Engineering
text: **NP-FW-HUB-001 Rev {NP-FW-HUB-001 from → to}, NP-RM-001 Rev {NP-RM-001 from → to}, CLAUDE.md Rev 79: `OI-FWHUB-29` decided (option A, then C): the Oz photoparoxysmal detector is not built, and the goggles refuse photic driving at 3–30 Hz outright.** The principal ruled on the finding that nothing detects a photoparoxysmal pattern and that its halt function has no caller. `REQ-FWHUB-65`, `CLAUDE.md` §4.2 and `NP-RM-001` now say so; the detector (C) waits on a clinical criterion. The runner's other unused stop bits are not gaps.
## NP-FW-HUB-001
author: NeurOne Firmware Engineering
text: **`OI-FWHUB-29` decided: option A built, option C owed.** New `REQ-FWHUB-65`: the visual module refuses photic driving (modes 0 and 3) at 3 to 30 Hz inclusive (`np_mod_visual_control`, five mutants caught in `np_mod_visual_tests`). §8.2 and §8.6 state the Oz detector is not built; `np_mod_visual_ppx_halt()` has no caller. One shipped protocol, `13-focus-prime`, drives 20 Hz and is now refused at dispatch. The runner's `NP_EV_THERMAL_FAULT` and `NP_EV_POWER_FAULT` have no producer and are not gaps. The detector waits on `docs/human-input/oi-fwhub-29-c-detection-criterion.md`.
## NP-RM-001
author: NeurOne Risk
text: **§ software safety requirements: the photoparoxysmal EEG detection control is annotated as specified and not built (`OI-FWHUB-29`).** The goggles refuse photic driving at 3–30 Hz outright in its place (`REQ-FWHUB-65`). The control is not removed from the list; the risk acceptance that rested on it is for the principal and Risk to restate. No risk score changed.
