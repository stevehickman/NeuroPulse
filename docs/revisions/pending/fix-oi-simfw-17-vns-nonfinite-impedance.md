date: 2026-11-08
## DHF
author: NeurOne Firmware Engineering
text: **{NP-SIM-FW-001 from → to}, {NP-FW-HUB-001 from → to}, {NP-HW-VNSCLIP-001 from → to}: OI-SIMFW-17 closed. The auricular VNS driver refuses a non-finite contact impedance.** `np_mod_vns_control` refused with `imp < min` or `imp > max`, which is false for NaN, so a NaN reading enabled stimulation (found by the firmware-in-the-loop simulator). It now refuses anything that is not a finite number in [500, 5000] ohm, with the out-of-range status, and starts nothing. The test is on the float's exponent bits because the hub builds with `-ffast-math`. Telemetry reports a non-finite read as 0 ohm (a NaN cast to `uint16_t` is undefined). Class B only; no Class C file touched. New host test `np_mod_vns_tests` verifies REQ-VNSC-07 (fails 7 checks without the fix, passes with it); the simulator scenario now asserts the refusal. Not changed: `np_stim_xcheck.c` uses `isfinite()` under the same flag, which may be folded away; not assessed here.
## NP-SIM-FW-001
author: NeurOne Firmware Engineering
text: **§7, §15: OI-SIMFW-17 closed.** The driver refuses a non-finite contact impedance; the VNS scenario that recorded the defect asserts the refusal.
## NP-FW-HUB-001
author: NeurOne Firmware Engineering
text: **§8.4: a non-finite contact impedance (NaN, infinity) is refused with the out-of-range status (OI-SIMFW-17, REQ-VNSC-07).** The check tests the exponent bits because of `-ffast-math`; new host test `np_mod_vns_tests`.
## NP-HW-VNSCLIP-001
author: NeurOne Firmware Engineering
text: **REQ-VNSC-07 clarified: a non-finite reading is not within [500, 5000] ohm and is refused (OI-SIMFW-17).** No figure changes.
