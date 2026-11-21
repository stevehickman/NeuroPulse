## DHF
author: NeurOne Firmware Engineering
text: **{NP-FW-HUB-001 from → to}, {NP-SIM-FW-001 from → to}: the device-serial replay guard fails closed (`REQ-FWHUB-90`, `OI-PRIV-31` (NP-PRIV-ANALYSIS-004), GitHub #959); the twin's TEST device serial is non-zero.**
## NP-FW-HUB-001
author: NeurOne Firmware Engineering
text: **§4.2, §4.4, §10.1: new `REQ-FWHUB-90`. `np_protocol_verify_and_parse()` refuses with `NP_HUB_ERR_WRONG_DEVICE` when the helmet cannot read its own serial or reads it as all zero, where it used to skip the comparison (`OI-PRIV-31` (NP-PRIV-ANALYSIS-004), Finding M4). §4.2 also states that the guard is cross-device only and that same-device freshness is open (`docs/human-input/oi-priv-31-protocol-freshness.md`).**
## NP-SIM-FW-001
author: NeurOne Software Engineering
text: **§26: the twin's TEST device serial is 0x5a in every byte, no longer all zero, because the hub's guard now fails closed (`REQ-FWHUB-90`). The bridge and scenario tests stamp it into the golden descriptors as the app stamps `DEVICE_SERIAL` (`UC-137`).**
