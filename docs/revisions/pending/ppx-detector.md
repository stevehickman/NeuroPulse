## DHF
author: NeurOne Firmware Engineering
text: **NP-FW-HUB-001 Rev {NP-FW-HUB-001 from → to}: the photoparoxysmal detector is built in the hub and left unarmed.** `np_ppx_detect.c`, `np_ppx_policy.c` and the goggles' wiring; the halt is a pause through the panic. The evidence level in force is NONE, so the refusal of photic driving at 3 to 30 Hz stands. No limit changed.
## NP-FW-HUB-001
author: NeurOne Firmware Engineering
text: **`REQ-FWHUB-67`, `-69`, `-70` built and not armed; `-71` partly built; `OI-FWHUB-32` updated; §8.2 and §8.6 corrected.** The detector (spike / involved / generalised, a rule for when it cannot see), the policy that says what it may replace, the goggles' refusal to start when it cannot see and its halt when it loses sight or finds a discharge. `np_ppx_tests` on synthetic signals (eight mutants caught) and `np_mod_visual_tests`. `np_protocol_photic_admit` takes the policy's answer. Nothing feeds the detector yet and its channel roles are undefined.
