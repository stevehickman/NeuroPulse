date: 2026-11-03
## DHF
author: NeurOne Firmware Engineering
text: **{NP-SIM-FW-001 from → to}: the firmware-in-the-loop simulator runs the stimulation (BES/tACS and tDCS), EEG and auricular VNS modules, the first three of the remaining module types (`REQ-SIMFW-13`, the Principal 2026-11-03).**
## NP-SIM-FW-001
author: NeurOne Software Engineering
text: **New §13 (stimulation), §14 (EEG) and §15 (auricular VNS) as built, and the §8 step 3 order set by the Principal (stim first, then the rest in the proposed order).** The bus is keyed by module kind and index; the device's own `np_mod_stim.c`, `np_stim_xcheck.c`, `np_ramp_plan.c`, `np_mod_eeg.c`, `np_mod_vns.c` and the HRV session library run under 39 scenarios; the gaps are listed with the measured line coverage of each source; `OI-SIMFW-15` to `OI-SIMFW-18` are raised (the stimulation seam leaves charge and polarity undefined; an EEG stop that is lost cannot be retried; a non-finite VNS contact impedance enables stimulation; a refused VNS command leaves the PPG and reference routing on).
