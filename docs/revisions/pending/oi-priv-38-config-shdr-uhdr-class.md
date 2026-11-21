## DHF
author: NeurOne Firmware Engineering
text: **{NP-FW-NVRAM-001 from → to}, {NP-FW-EMMC-002 from → to}, {NP-FW-HUB-001 from → to}, {NP-HW-NASAL-001 from → to}, {NP-SW-001 from → to}: OI-PRIV-38 (NP-PRIV-ANALYSIS-004). Map 3's ordinal, dose_accum and throttle_count classified UHDR and not to be written to Config (what Map 3 keeps is owed to the principal); the EDF+ research gate writes nothing to SHDR (REQ-SW02-12); the accessory-authentication record carries no session count and logs a failure once per insertion (REQ-NASAL-16).**
## NP-FW-NVRAM-001
author: NeurOne Firmware Engineering
text: **§8.1: ordinal, dose_accum and throttle_count classified UHDR class, not written to Config (F1, F2, F8; OI-PRIV-38 (NP-PRIV-ANALYSIS-004) part 1); §3.3.1.5 and §8.1 say the Map 3 row is not to be written to Config until the principal rules what it keeps.**
## NP-FW-EMMC-002
author: NeurOne Firmware Engineering
text: **§E.4: an excluded EDF+ file leaves nothing in SHDR; EDF_PRIVACY_HEADER_VIOLATION (0xE1) retired (OI-PRIV-38 (NP-PRIV-ANALYSIS-004) part 2, REQ-SW02-12).**
## NP-FW-HUB-001
author: NeurOne Firmware Engineering
text: **§5 and §6: the 0x83 accessory-authentication record is tag, slot, type and pass with no session count; the intranasal driver writes a failure from detect() once per insertion (OI-PRIV-38 (NP-PRIV-ANALYSIS-004) part 3, REQ-NASAL-16); the positional residual is recorded.**
## NP-HW-NASAL-001
author: NeurOne Firmware Engineering
text: **REQ-NASAL-16 added: the sleeve-authentication SHDR record carries no session count and a failure is recorded once per insertion (OI-PRIV-38 (NP-PRIV-ANALYSIS-004) part 3).**
## NP-SW-001
author: NeurOne Software Engineering
text: **§5.2.2: REQ-SW02-12 added: the EDF+ research-extract gate leaves nothing in SHDR (OI-PRIV-38 (NP-PRIV-ANALYSIS-004) part 2).**
