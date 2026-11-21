date: 2026-11-21
## DHF
author: NeurOne Firmware Engineering
text: **{NP-FW-HUB-001 from → to}: `OI-PRIV-37` (NP-PRIV-ANALYSIS-004, M12): a stop by the person or a pause timeout no longer lights the fault indicator, and stealth mode is built on the hub (RAM only, every fault still shows); `REQ-FWHUB-84`, `REQ-FWHUB-85`.**
## NP-FW-HUB-001
author: NeurOne Firmware Engineering
text: **§5.5a added and §2.5 gains the `STEALTH` row (`OI-PRIV-37`, NP-PRIV-ANALYSIS-004 M12): `np_runner_run()` returns `NP_HUB_ERR_STOPPED` for a stop or a pause timeout, which goes back to idle; `np_status_led.c` darkens idle and session in stealth and shows every fault regardless; `STEALTH` (0x0022) is an encrypted 1-byte setting held in RAM only. `REQ-FWHUB-84`, `REQ-FWHUB-85` added to §10.1.**
