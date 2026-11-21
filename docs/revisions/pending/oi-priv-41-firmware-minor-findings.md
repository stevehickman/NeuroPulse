date: 2026-11-21
## DHF
author: NeurOne Firmware Engineering
text: **{NP-FW-HUB-001 from → to}: `REQ-FWHUB-81` (d), the hub serves one BLE central at a time and a second link never inherits the first one's encryption (`OI-PRIV-41` (NP-PRIV-ANALYSIS-004) point 6, GitHub #969).**
## NP-FW-HUB-001
author: NeurOne Firmware Engineering
text: **§2.5 and `REQ-FWHUB-81` (d): the link state belongs to one link. `np_gatt_on_connect()` refuses a second link, no link is treated as encrypted while two are up, and the link left after one goes down is unencrypted until the stack reports it again (`OI-PRIV-41` (NP-PRIV-ANALYSIS-004) point 6; `np_gatt_server_tests`).**
