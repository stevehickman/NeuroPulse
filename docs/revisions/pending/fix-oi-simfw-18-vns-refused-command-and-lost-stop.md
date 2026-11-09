date: 2026-11-09
## DHF
author: NeurOne Firmware Engineering
text: **{NP-SIM-FW-001 from → to}, {NP-FW-HUB-001 from → to}, {NP-HW-VNSCLIP-001 from → to}: OI-SIMFW-18 closed. A refused auricular VNS command leaves nothing running, and a lost stimulator stop is reported and retried.** `np_mod_vns_control` started the PPG, the EEG reference routing and the HRV session before the stimulator write that can fail, left them on when it failed, and its stop path reported success whether or not the stimulator stop landed (found by the firmware-in-the-loop simulator). It now stops what the refused command started and tells the stimulator to stop; a stop that fails returns `NP_HUB_ERR_MOD_FAULT` with the enable still withdrawn, stays pending, and is retried by the next stop and the next start (a start is refused while it is lost). Class B driver only, no Class C file, no HAL signature changed. New requirement `REQ-VNSC-14`; six new host tests in `np_mod_vns_tests` (15 checks fail without the fix, all pass with it); the two simulator scenarios assert the fixed behaviour. Retry policy recorded as `UC-127`. Not changed: the simulator build on `main` fails to compile and link after the cardiac sensitivity change (`np_safety_spi_send_hr_report` argument count, `np_cardiac_sens_level`), found while verifying.
## NP-SIM-FW-001
author: NeurOne Firmware Engineering
text: **§7, §15: OI-SIMFW-18 closed.** The VNS driver stops what a refused command started and reports and retries a lost stimulator stop; the two scenarios that recorded the defect assert the fixed behaviour.
## NP-FW-HUB-001
author: NeurOne Firmware Engineering
text: **§8.4: a refused VNS start leaves nothing running and a lost stimulator stop is reported and retried (OI-SIMFW-18, REQ-VNSC-14).** Class B only; no HAL signature changed; new host tests in `np_mod_vns_tests`.
## NP-HW-VNSCLIP-001
author: NeurOne Firmware Engineering
text: **New REQ-VNSC-14: a refused start leaves nothing running, and a lost stop is reported and retried (OI-SIMFW-18).** No figure changes.
