## DHF
author: NeurOne Software Engineering
text: **{NP-SIM-FW-001 from → to}: the bridge and the browser now drive the safety MCU in the simulator (`OI-SIMFW-26`, closed); a simulator change, not verification evidence (`REQ-SIMFW-06`).**
## NP-SIM-FW-001
author: NeurOne Software Engineering
text: **New §23 and `REQ-SIMFW-15`: the bridge whitelists every `safety_*` command from validated integers and whitelisted names (a line is named, never numbered; the OTP is programmed once, before the attach) and routes the MCU's events to the page; the page draws a module as driven only while the MCU's enable line for it is enabled, says `Safety MCU not in loop` when none is attached, shows each line's request, grant and enable, the status, the fault slot, the last cutoff and the watchdog, and carries a test aid that plays the heartbeat task and the app with the TEST root key. `OI-SIMFW-26` closed; `OI-SIMFW-27` (a heartbeat-loss cut that no reply reported), `OI-SIMFW-28` and `OI-SIMFW-29` raised; `UC-127` and `UC-128` registered.**
