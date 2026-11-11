date: 2026-11-11
## DHF
author: NeurOne Firmware Engineering
text: **{NP-FW-HUB-001 from → to}, {NP-SIM-FW-001 from → to}: the safety MCU's withhold report (window bytes 27 to 33, `REQ-FWHUB-81`), approved by the safety MCU's owner as `OI-SIMFW-61`; the hub, the simulator and the helmet view read it.**
## NP-FW-HUB-001
author: NeurOne Firmware Engineering
text: **§7.4, `REQ-FWHUB-81`: the safety MCU reports which gate (waveform declaration or electrode geometry) held a requested line, in bytes 27 to 33 of the 38-byte reply (magic `0x8E`, two line masks, an additive checksum); the frame, the 8-byte reply and the heartbeat timing are unchanged; the hub reads it behind `np_safety_spi_get_withhold_report` and never reports a stale cause; Class C change, host-tested, target build, MISRA analysis and review remain with the owner.**
## NP-SIM-FW-001
author: NeurOne Firmware Engineering
text: **§7, §25: `OI-SIMFW-61` built to the extent run here (the owner approved it as proposed and allowed the frame to change, which was not needed; the gates record the lines they hold from the request and the condition, not the bits cleared, a departure found by the scenario); the helmet view's `UNREPORTED` splits into `UNDECLARED_WAVEFORM` and `MISSING_GEOMETRY`; `UC-151` records the magic byte.**
