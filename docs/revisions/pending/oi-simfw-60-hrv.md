date: 2026-11-13
## DHF
author: NeurOne Software Engineering
text: **{NP-SIM-FW-001 from → to}: the simulator's scenarios reach the HRV biofeedback library through the driver's session with synthetic feeders; development tool only, no device document changed.**
## NP-SIM-FW-001
author: NeurOne Software Engineering
text: **§5, §7, §28, new §32: `REQ-SIMFW-24` and its five parts; `OI-SIMFW-60` partly closed (lines reached before then after: `np_hrv_eeg_biofeedback.c` 0 to 100 %, `np_hrv_ppg.c` 13 to 97 %, `np_hrv_coherence.c` 12 to 98 %, `np_hrv_tavns_sync.c` 21 to 87 %, `np_hrv_pacer.c` 23 to 98 %, `np_hrv_session.c` 41 to 95 %; the owner's path list is not given); `OI-SIMFW-78` raised for the owner of `NP-FW-HRV-001` (nothing in the device's code feeds or ticks the session, the taVNS enable is a stub, the stop-time sweep finalise cannot succeed, a steady rhythm scores the maximum coherence, and others); `UC-181` registered.**
