date: 2026-11-04
## DHF
author: NeurOne Firmware Engineering
text: **{NP-SIM-FW-001 from → to}: the firmware-in-the-loop simulator runs all nine module drivers (PBM from the spike; stimulation, EEG, auricular VNS, audio, visual, intranasal, the T2 and accessory stubs and cervical VNS added), and finds that a cervical VNS session cannot get from its baseline to delivery (`REQ-SIMFW-13`, the Principal 2026-11-03; `OI-SIMFW-22`).**
## NP-SIM-FW-001
author: NeurOne Software Engineering
text: **New §13 to §20 as built (stimulation, EEG, auricular VNS, audio, visual goggles, intranasal Y-probe, T2 and accessory stubs, cervical VNS), and the §8 step 3 order set by the Principal (stim first, then the rest in the proposed order).** The bus is keyed by module kind and index and gains a start offset for injected faults; the device's own drivers, the cross-check, the HRV session library, the cervical VNS library with its Pan-Tompkins detector and the goggle Hall-edge handler run under 92 new scenarios; the gaps are listed with the measured line coverage of each source; `OI-SIMFW-15` to `OI-SIMFW-22` are raised, `OI-SIMFW-22` being a defect in `np_cvns_session.c` (a session cannot leave its baseline stage).
