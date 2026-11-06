date: 2026-11-06
## DHF
author: NeurOne Firmware Engineering
text: **{NP-SIM-FW-001 from → to}, {NP-FW-CVNS-001 from → to}: `OI-SIMFW-23` closed. A faulted cervical VNS session keeps the interlock FAULT latched and can be re-enabled after the 30 s lockout with a fresh baseline (the principal's decision: keep FAULT latched).**
## NP-SIM-FW-001
author: NeurOne Firmware Engineering
text: **§20 and §7: `OI-SIMFW-23` closed (fixed in `np_cvns_session.c`, `np_cvns_interlock.c` and `np_mod_cvns.c`); the re-enable scenario now shows a refusal inside the lockout and an acceptance after it.**
## NP-FW-CVNS-001
author: NeurOne Firmware Engineering
text: **§5.3: re-arm after a session fault recorded (principal, 2026-11-06, `OI-SIMFW-23`): the interlock FAULT stays latched, the baseline re-forms from fresh PPG, the lockout runs from the tick that saw the fault; a re-enable re-arms the interlock only and delivery does not restart (undecided).**
