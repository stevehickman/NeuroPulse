date: 2026-11-03
## DHF
author: NeurOne Firmware Engineering
text: **{NP-SIM-FW-001 from → to}: the firmware-in-the-loop simulator runs the stimulation module (BES/tACS and tDCS) first of the remaining module types (`REQ-SIMFW-13`, the Principal 2026-11-03).**
## NP-SIM-FW-001
author: NeurOne Software Engineering
text: **New §13, stimulation as built, §8 step 3 order set by the Principal (stim first, then the rest in the proposed order).** The bus is keyed by module kind and index, the seven `np_mod_stim_hal_*` seams are modelled, the device's own `np_mod_stim.c`, `np_stim_xcheck.c` and `np_ramp_plan.c` run under 17 scenarios (99.4 % and 93.7 % of the lines of the first two), the gaps are listed, and `OI-SIMFW-15` is raised (the seam leaves charge and polarity undefined).
