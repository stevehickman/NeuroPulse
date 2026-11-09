date: 2026-11-09
## DHF
author: NeurOne Firmware Engineering
text: **{NP-SIM-FW-001 from → to}, {NP-FW-HUB-001 from → to}, {NP-HW-NASAL-001 from → to}: OI-SIMFW-21 point 2 fixed; point 1 stays open. A failed intranasal drive command leaves nothing emitting.** A bilateral command writes the left side and then the right; when the right write failed, `np_mod_intranasal_control` returned the fault but the left side kept emitting, with the driver's active flag never set, so its stop path could not reach it (found by the firmware-in-the-loop simulator). Any failed write now stops both sides, clears the active state, withdraws the enable request and reports the module fault. Class B driver only; no Class C file touched; no optical power, dose or timing figure changed. New host test `np_mod_intranasal_tests` verifies REQ-NASAL-15 (fails 3 checks without the fix, passes with it); the simulator scenario now asserts nothing is driven. **Point 1 (authentication read once at init) is not changed and stays open:** no document says when the sleeve must be re-read, and inventing a trigger is design work (CLAUDE.md §2.3, §18). The question for the Principal is recorded on the row.
## NP-SIM-FW-001
author: NeurOne Firmware Engineering
text: **§7, §18: OI-SIMFW-21 partly closed.** Point 2 (partial bilateral failure) fixed and its scenario asserts the fix; point 1 (authentication read once) stays open with the question stated.
## NP-FW-HUB-001
author: NeurOne Firmware Engineering
text: **§8.7: a drive command that fails on either side leaves nothing emitting (OI-SIMFW-21, REQ-NASAL-15).** New host test `np_mod_intranasal_tests`. When authentication must be re-read is unspecified and noted as open.
## NP-HW-NASAL-001
author: NeurOne Firmware Engineering
text: **REQ-NASAL-15 added: a drive command that does not fully apply leaves nothing emitting (OI-SIMFW-21).** Clarifies behaviour already required by the fail-closed rule; no figure changes.
