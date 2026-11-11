date: 2026-11-11
## DHF
author: NeurOne Firmware Engineering
text: **{NP-FW-HUB-001 from → to}, {NP-FW-PBM1064-001 from → to}, {NP-FW-HEXTILE-001 from → to}, {NP-HW-HEXTILE-001 from → to}, {NP-HW-NASAL-001 from → to}, {NP-NPPS-REF-001 from → to}, {NP-SIM-FW-001 from → to}, {NP-SOUP-LFS-001 from → to}, {NP-HW-CVNS-001 from → to}, {NP-FW-CVNS-001 from → to}, {NP-REG-CVNS-001 from → to}, {NP-RISK-002 from → to}: the hub control program refuses a value outside a limit and no longer clamps one (the principal, 2026-11-11: no silent clamping anywhere). Five clamps became refusals at the same limits (BES/tACS 1 mA, tDCS 2 mA and its 30 s ramp, auricular VNS 40 mA, PBM and intranasal duty 25 %), at dispatch and at upload (upload status NP_HUB_ERR_OUTSIDE_LIMIT, reason 40); the 25 % duty refusal is interim until OI-HEXTILE-31. No limit value, descriptor format or safety MCU change. On the principal's answers of 2026-11-11: the cervical VNS block runs for the length the protocol authors and the hardcoded 60-120 s session bounds are retired, with risk entry 25-f in NP-RISK-002 (not scored, evaluation pending) and a review note in NP-REG-CVNS-001; a reserved bit in the goggles' zone mask is refused.**
## NP-FW-HUB-001
author: NeurOne Firmware Engineering
text: **§8, §8.1, §8.3, §8.3.1, §8.4, §8.7, §5.2, §10.1, §13: new REQ-FWHUB-78. A hub driver refuses a command outside a limit it holds with NP_HUB_ERR_INVALID_ARG and never clamps it: BES/tACS above 1 mA, tDCS above 2 mA or with a ramp under 30 s, auricular VNS above 40 mA, intranasal and PBM duty above 0x32. The refusal precedes every HAL call, enable request and published commanded current, and leaves a running command as it was. np_runner_load makes the same check at upload with a status of its own, NP_HUB_ERR_OUTSIDE_LIMIT (reason 40; the principal, 2026-11-11), named outside_limit in the apps with a message in every locale; the shared core does not map it yet. The duty refusal is interim (OI-HEXTILE-31). OI-FWHUB-30 amended; new OI-FWHUB-35 lists what is left (a status of its own, the safety MCU's declared-phase clamp, the goggles' zone mask, the cervical VNS block length, the shared core's clamps, the throttle cascades). Parts (g) to (j): audio and cervical VNS join the upload check; BES/tACS is refused outside 0.5-40 Hz and clinical tACS below 0.5 Hz, so no admitted command has its declared phase clamped; an EEG gain code above 6 is refused; §8.9 requires the T2 drivers to refuse when built. Parts (k), (l), on the principal's answers of 2026-11-11: a cervical VNS block runs for the length the signed descriptor authors, told to the driver by the runner (no wire field added), and a start with no length is refused; a goggles command with a reserved zone-mask bit set is refused, and §8.6 documents the 12-bit mask with its per-eye meaning marked open. No limit value changes in (a) to (j). OI-FWHUB-35 (d) cites the proposal drafted for the safety MCU's owner on the declared-phase clamp (docs/human-input; nothing in firmware/safety_mcu is changed).**
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
## NP-HW-CVNS-001
author: NeurOne Firmware Engineering
text: **§3 REQ-CVNS-04: the session part (60-120 s) is retired in place by the principal's decision of 2026-11-11, because it had no documented health or safety source, only a gammaCore equivalence figure. The length of a cervical block is the one the signed protocol authors. The frequency, current and pulse-width parts are unchanged.**
## NP-FW-CVNS-001
author: NeurOne Firmware Engineering
text: **§3, §8: NP_CVNS_SESSION_MIN_S and NP_CVNS_SESSION_MAX_S are retired and removed from the firmware (the principal, 2026-11-11). np_cvns_stim_init refuses a configuration with no length and one no longer than the two ramps, which is geometry and not a health limit; any other length is the protocol's.**
## NP-REG-CVNS-001
author: NeurOne Firmware Engineering
text: **§3.2: a marked note that the session-duration row (60-120 s against gammaCore's 120 s) no longer describes a hardware bound after the principal's decision of 2026-11-11, and that the equivalence argument needs regulatory review. The table and the argument are not rewritten.**
## NP-RISK-002
author: NeurOne Firmware Engineering
text: **§4.3.2 C8, §4.3.3, §7: new hazard 25-f (cervical VNS session length is no longer bounded by a hardcoded limit), entered not scored and not accepted, with the controls that still apply and what none covers; new OI-RISK2-11 for its evaluation by Quality and Safety; C8 notes that REQ-CVNS-04's session part is retired. No existing rating changes.**
