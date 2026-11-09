date: 2026-11-09
## DHF
author: NeurOne Firmware Engineering
text: **{NP-SIM-FW-001 from → to}, {NP-FW-HUB-001 from → to}: OI-SIMFW-20 closed. The visual driver checks the status of the LED command and fails closed.** `np_mod_visual_control` discarded the status of `np_mod_visual_hal_led_set`, so a failed LED command reported success and requested the visual safety enable with nothing lit (found by the firmware-in-the-loop simulator). It now stops the LEDs, withdraws and never requests the enable, leaves the session inactive, does not start the EMDR half of a combined command, and returns the HAL's status. Class B only; no Class C file touched. No irradiance limit, MPE figure, timing or photic-zone refusal changed. New requirement `REQ-FWHUB-75`, verified by `np_mod_visual_tests` (fails 6 checks without the fix, passes with it); the simulator scenario now asserts the refusal. Also in the simulator only: `np_sim_safety_harness.c` passed the old arity to `np_safety_spi_send_hr_report` and the simulator did not link `np_cardiac_sens_level` since the cervical sensitivity change, so it did not build; it now passes level 0 and defines a sim `np_cardiac_sens_level` returning 0 (the validated cutoff).
## NP-SIM-FW-001
author: NeurOne Firmware Engineering
text: **§7, §17: OI-SIMFW-20 closed.** The driver checks the LED command's status and fails closed; the visual scenario that recorded the defect asserts it. The sim safety harness builds again after the cervical sensitivity change (a sim-side `np_cardiac_sens_level` returning 0).
## NP-FW-HUB-001
author: NeurOne Firmware Engineering
text: **New REQ-FWHUB-75: the visual driver fails closed on a failed LED command (OI-SIMFW-20).** The LEDs are stopped, no enable is requested, the session is not active and the HAL status is returned. Host test `np_mod_visual_tests`.
