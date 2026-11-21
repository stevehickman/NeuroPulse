date: 2026-11-21
## DHF
author: NeurOne Firmware Engineering
text: **{NP-FW-HUB-001 from → to}, {NP-SIM-FW-001 from → to}: `OI-PRIV-37` (NP-PRIV-ANALYSIS-004, M12): a stop by the person or a pause timeout no longer lights the fault indicator, and stealth mode is built on the hub (RAM only, every fault still shows); `REQ-FWHUB-85`, `REQ-FWHUB-86`. The duplicate `REQ-FWHUB-65` (Rev 35, cervical VNS baseline) is renumbered `REQ-FWHUB-87`, and §10.1 is sorted.**
## NP-FW-HUB-001
author: NeurOne Firmware Engineering
text: **§5.5a added and §2.5 gains the `STEALTH` row (`OI-PRIV-37`, NP-PRIV-ANALYSIS-004 M12): `np_runner_run()` returns `NP_HUB_ERR_STOPPED` for a stop or a pause timeout, which goes back to idle; `np_status_led.c` darkens idle and session in stealth and shows every fault regardless; `STEALTH` (0x0022) is an encrypted 1-byte setting held in RAM only. `REQ-FWHUB-85`, `REQ-FWHUB-86` added to §10.1. Two rows had carried `REQ-FWHUB-65` since Rev 35: the photic refusal (Rev 34) keeps it, and the cervical VNS baseline enable request (Rev 35) is now `REQ-FWHUB-87`. §10.1 is sorted by ID and made one table (a blank line had split it after `REQ-FWHUB-37`).**
## NP-SIM-FW-001
author: NeurOne Firmware Engineering
text: **`OI-SIMFW-22` and its §20 note cite `REQ-FWHUB-87`, the cervical VNS baseline requirement renumbered from a duplicate `REQ-FWHUB-65` in `NP-FW-HUB-001`.**
