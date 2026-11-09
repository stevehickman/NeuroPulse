date: 2026-11-09
## DHF
author: NeurOne Software Engineering
text: **{NP-SIM-FW-001 from → to}: the safety MCU's unreached paths (OI-SIMFW-25) are driven by 21 new scenarios (40 in all), and four behaviours that look surprising are recorded as OI-SIMFW-32 to OI-SIMFW-35, none decided.**
## NP-SIM-FW-001
author: NeurOne Software Engineering
text: **§7, §22, REQ-SIMFW-14: the fault latch across a warm reset, the repeated signature failure, the cardiac re-enable and its repeat contact check, the three geometry gates, the cross-check's failure paths, per-user scope, the cutoff log's rollover and an interrupted compaction, the thermal re-arm and two tier records that do not verify each have a scenario; line coverage of the safety MCU's sources re-measured (np_fault_latch.c 51 to 83 %, np_nv_state.c 62 to 93 %, np_session_sig.c 79 to 87 %, np_cardiac_interlock.c 79 to 97 %, np_charge_monitor.c 90 to 96 %, np_tier_identity.c 85 to 92 %, np_thermal_interlock.c 89 to 100 %, np_safety_main.c 85 to 100 %) with the method recorded; harness additions safety_geom, the cmd_flip fault, nv_program with a skip count, a warm-reset latch and silicon erase; OI-SIMFW-25 partly closed; OI-SIMFW-32 to OI-SIMFW-35 raised (a restored fault loses its slot and cannot be cleared, the signature lock, area and phase taken as sent, a lost compaction COMMIT withholds every user); UC-132 registered.**
