date: 2026-11-03
## DHF
author: NeurOne Firmware Engineering
text: **{NP-SIM-FW-001 from → to}: the firmware-in-the-loop simulator runs the stimulation (BES/tACS and tDCS), EEG, auricular VNS, audio and visual modules, the first five of the remaining module types (`REQ-SIMFW-13`, the Principal 2026-11-03).**
## NP-SIM-FW-001
author: NeurOne Software Engineering
text: **New §13 (stimulation), §14 (EEG), §15 (auricular VNS), §16 (audio) and §17 (visual goggles) as built, and the §8 step 3 order set by the Principal (stim first, then the rest in the proposed order).** The bus is keyed by module kind and index; the device's own `np_mod_stim.c`, `np_stim_xcheck.c`, `np_ramp_plan.c`, `np_mod_eeg.c`, `np_mod_vns.c`, the HRV session library, `np_mod_audio.c` and `np_mod_visual.c` (including its Hall-edge handler, run as the platform interrupt would) run under 64 scenarios; the gaps are listed with the measured line coverage of each source; `OI-SIMFW-15` to `OI-SIMFW-20` are raised.
