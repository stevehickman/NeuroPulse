date: 2026-11-03
## DHF
author: NeurOne Firmware Engineering
text: **{NP-SIM-FW-001 from → to}: the firmware-in-the-loop simulator runs the stimulation (BES/tACS and tDCS) and EEG modules, the first two of the remaining module types (`REQ-SIMFW-13`, the Principal 2026-11-03).**
## NP-SIM-FW-001
author: NeurOne Software Engineering
text: **New §13 (stimulation) and §14 (EEG) as built, and the §8 step 3 order set by the Principal (stim first, then the rest in the proposed order).** The bus is keyed by module kind and index; the device's own `np_mod_stim.c`, `np_stim_xcheck.c`, `np_ramp_plan.c` and `np_mod_eeg.c` run under 27 scenarios (99.4 %, 93.7 % and 100 % of their lines for the first, second and fourth); the gaps are listed; `OI-SIMFW-15` (the stimulation seam leaves charge and polarity undefined) and `OI-SIMFW-16` (an EEG stop that is lost cannot be retried) are raised.
