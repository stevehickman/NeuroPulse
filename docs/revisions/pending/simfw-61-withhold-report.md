date: 2026-11-11
## DHF
author: NeurOne Firmware Engineering
text: **{NP-FW-HUB-001 from → to}, {NP-SIM-FW-001 from → to}: the safety MCU's withhold report (window bytes 27 to 37, `REQ-FWHUB-82`: the conditions that hold and the gate that decides), approved by the safety MCU's owner as `OI-SIMFW-61` and extended by the owner's choice of option 3; the hub, the simulator and the helmet view read it.**
## NP-FW-HUB-001
author: NeurOne Firmware Engineering
text: **§7.4, `REQ-FWHUB-82`: the safety MCU reports which gate (waveform declaration or electrode geometry) holds a requested line and which gate decides it, in bytes 27 to 37 of the 38-byte reply (magic `0x8E`, two condition masks, two decided masks, an additive checksum of bytes 0 to 8); a gate decides a line when nothing else the MCU evaluates holds it, read as conditions (the heartbeat grant's refusals, the cardiac block and its silent pre-arm and cross-check holds, the impedance gate, the tier gate, the other gate); the frame, the 8-byte reply and the heartbeat timing are unchanged and the window is now full; the hub reads it behind `np_safety_spi_get_withhold_report` and never reports a stale cause; Class C change, host-tested, target build, MISRA analysis and review remain with the owner.**
## NP-SIM-FW-001
author: NeurOne Firmware Engineering
text: **§7, §25: `OI-SIMFW-61` built to the extent run here (the owner approved it as proposed, allowed the frame to change, which was not needed, then chose option 3, every condition that holds plus the gate that decides; the gates record the lines they hold from the request and the condition, not the bits cleared, a departure found by the scenario; the cardiac interlock's silent holds were found by the T2 scenario); the helmet view's `UNREPORTED` splits into `UNDECLARED_WAVEFORM` and `MISSING_GEOMETRY`, named only when that gate decides the line; `UC-153` records the magic byte.**
