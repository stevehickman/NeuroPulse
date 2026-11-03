date: 2026-11-03
## DHF
author: NeurOne Firmware Engineering
text: **{NP-SIM-FW-001 from → to}: the firmware-in-the-loop simulator runs eight of the nine module drivers (stimulation, EEG, auricular VNS, audio, visual, intranasal and the T2 and accessory stubs, with PBM from the spike); cervical VNS is the last (`REQ-SIMFW-13`, the Principal 2026-11-03).**
## NP-SIM-FW-001
author: NeurOne Software Engineering
text: **New §13 to §19 as built (stimulation, EEG, auricular VNS, audio, visual goggles, intranasal Y-probe, T2 and accessory stubs), and the §8 step 3 order set by the Principal (stim first, then the rest in the proposed order).** The bus is keyed by module kind and index and gains a start offset for injected faults; the device's own drivers, the cross-check, the HRV session library and the goggle Hall-edge handler (run as the platform interrupt would) run under 77 new scenarios; the gaps are listed with the measured line coverage of each source; `OI-SIMFW-15` to `OI-SIMFW-21` are raised.
