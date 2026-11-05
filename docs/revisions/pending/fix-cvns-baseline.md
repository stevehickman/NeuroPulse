date: 2026-11-05
## DHF
author: NeurOne Firmware Engineering
text: **{NP-FW-HUB-001 from → to}, {NP-FW-CVNS-001 from → to}, {NP-SIM-FW-001 from → to}: a cervical VNS session can now get from its baseline to delivery (`OI-SIMFW-22` fixed in `np_cvns_session.c`; `REQ-FWHUB-65`), and the simulator reaches the delivery stages; a second defect, that a faulted session cannot be re-enabled, is recorded and not fixed (`OI-SIMFW-23`); and the simulator draws accessories only while in use (`NP-SIM-FW-001` §21).**
## NP-FW-HUB-001
author: NeurOne Firmware Engineering
text: **New `REQ-FWHUB-65`: a cervical VNS session whose baseline is valid requests the enable once and waits for the grant without faulting.** Fails without it: every session faults one tick after its baseline completes. Verified by two new host tests in `np_mod_cvns_tests`, which fail without the fix and pass with it. IEC 62304 SW-02 Class B (the main-processor library); the Class C safety MCU is untouched.
## NP-FW-CVNS-001
author: NeurOne Firmware Engineering
text: **§8.1: the BASELINE stage requests the enable once and holds, rather than asking again every tick (`np_cvns_session.c`).** Before this, a session faulted one tick after its baseline completed with `NP_CVNS_ERR_SESSION_ACTIVE`, grant or no grant (`NP-SIM-FW-001` `OI-SIMFW-22`). No constant, limit, criterion or interlock threshold changed. IEC 62304 SW-01 Class C / SW-02 Class B: the Class B session logic changes; no Class C code is affected.
## NP-SIM-FW-001
author: NeurOne Software Engineering
text: **§20 and §7: `OI-SIMFW-22` closed (fixed) and `OI-SIMFW-23` raised (a faulted cervical VNS session cannot be re-enabled).** The cervical VNS scenarios grow from 15 to 22 and reach ramp-up, steady delivery, the safety MCU's refusal, R-peak data loss, stop in delivery and completion at the session length; the measured line coverage rises (`np_mod_cvns.c` 91.8 %, the interlock 77.1 %, the session 83.8 %, the stimulation ramp 75.9 %). **New §21: accessories in the browser** (the Principal, 2026-11-05): drawn only while in use, those attached to the helmet on the helmet, the others in a window each around the perimeter of the helmet window; the page's module registry is keyed by (kind, index).
