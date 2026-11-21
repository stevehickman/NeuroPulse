date: 2026-11-21
## DHF
author: NeurOne Firmware Engineering
text: **{NP-FW-EMMC-002 from → to}: §H.3.4, the extended accelerometer record is written only after the spent budget persists; a failed budget write sends nothing (`OI-PRIV-36` (NP-PRIV-ANALYSIS-004), GitHub #964).**
## NP-FW-EMMC-002
author: NeurOne Firmware Engineering
text: **§H.3.4: the spent budget is persisted before the extended record is written, and a failed budget write sends nothing and returns `NP_ACCEL_ERR_BUDGET`; a failed record write leaves the slot spent (CHAR-2; `OI-PRIV-36` (NP-PRIV-ANALYSIS-004), GitHub #964).**
