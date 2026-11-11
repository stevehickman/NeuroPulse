date: 2026-11-11
## DHF
author: NeurOne Firmware Engineering
text: **{NP-FW-HUB-001 from → to}, {NP-FW-PBM1064-001 from → to}, {NP-FW-HEXTILE-001 from → to}, {NP-HW-HEXTILE-001 from → to}, {NP-HW-NASAL-001 from → to}, {NP-NPPS-REF-001 from → to}, {NP-SIM-FW-001 from → to}, {NP-SOUP-LFS-001 from → to}: the hub control program refuses a value outside a limit and no longer clamps one (the principal, 2026-11-11: no silent clamping anywhere). Five clamps became refusals at the same limits (BES/tACS 1 mA, tDCS 2 mA and its 30 s ramp, auricular VNS 40 mA, PBM and intranasal duty 25 %), at dispatch and at upload; the 25 % duty refusal is interim until OI-HEXTILE-31. No limit value, wire format or safety MCU change.**
## NP-FW-HUB-001
author: NeurOne Firmware Engineering
text: **§8, §8.1, §8.3, §8.3.1, §8.4, §8.7, §5.2, §10.1, §13: new REQ-FWHUB-78. A hub driver refuses a command outside a limit it holds with NP_HUB_ERR_INVALID_ARG and never clamps it: BES/tACS above 1 mA, tDCS above 2 mA or with a ramp under 30 s, auricular VNS above 40 mA, intranasal and PBM duty above 0x32. The refusal precedes every HAL call, enable request and published commanded current, and leaves a running command as it was. np_runner_load makes the same check at upload (reason 2). The duty refusal is interim (OI-HEXTILE-31). OI-FWHUB-30 amended; new OI-FWHUB-35 lists what is left (a status of its own, audio and cervical VNS at upload, the declared-phase clamp, two bit masks, the shared core's clamps, the throttle cascades). No limit value changes.**
## NP-FW-PBM1064-001
author: NeurOne Firmware Engineering
text: **§5.3, §5.5, §10 (FAI-SM-05): np_pbm_drive_set_duty refuses a duty above DUTY_MAX_REG (0x32) with NP_PBM_ERR_INVALID_ARG and writes no register, where it clamped to 0x32; np_pbm_session_start refuses a preset above it before any hardware is touched. FAI-SM-05 checks the refusal. The 25 % value is unchanged and the refusal is interim until OI-HEXTILE-31.**
## NP-FW-HEXTILE-001
author: NeurOne Firmware Engineering
text: **§5.5: the hub row of the duty table and the paragraph under it say the hub refuses a duty above 25 % (it clamped until 2026-11-11, NP-FW-HUB-001 REQ-FWHUB-78); the compiler still clamps. No tile requirement changes.**
## NP-HW-HEXTILE-001
author: NeurOne Firmware Engineering
text: **§13 OI-HEXTILE-31: item (iii) notes that the hub clamp is a refusal at the same 25 % since 2026-11-11 (NP-FW-HUB-001 REQ-FWHUB-78). The item stays open: the 25 % bound is still in the hub and is removed only after the pre-signing check exists.**
## NP-HW-NASAL-001
author: NeurOne Firmware Engineering
text: **§3 REQ-NASAL-11, §5 item 3: the driver refuses a duty above INS_DUTY_MAX and writes neither side; it no longer clamps (NP-FW-HUB-001 REQ-FWHUB-78 (d)). The 25 % value is unchanged, still inherited and not derived (UC-016), and the refusal is interim until OI-HEXTILE-31.**
## NP-NPPS-REF-001
author: NeurOne Firmware Engineering
text: **§4.1 duty_cycle row: the validator and the hub refuse above 25 % and the compiler clamps at 25 %, until OI-HEXTILE-31 (the hub clamped until 2026-11-11). No grammar, range or requirement changes.**
## NP-SIM-FW-001
author: NeurOne Firmware Engineering
text: **§6 test rows: the stimulation, VNS and intranasal scenario suites now show a value above a driver limit refused where they showed it capped (NP-FW-HUB-001 REQ-FWHUB-78); check counts 66 to 68, 34 to 37 and 25 to 26. No simulator requirement changes.**
## NP-SOUP-LFS-001
author: NeurOne Firmware Engineering
text: **One sentence on the PBM duty path: np_pbm_drive_set_duty holds every mode to 25 % by refusing above it, where it clamped (NP-FW-HUB-001 REQ-FWHUB-78). The bound and the analysis are unchanged.**
