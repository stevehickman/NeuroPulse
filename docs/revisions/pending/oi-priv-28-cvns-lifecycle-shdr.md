## DHF
author: NeurOne Firmware Engineering
text: **{NP-FW-HUB-001 from → to}: of the cervical VNS re-enable lifecycle only the cutoff flag reaches SHDR; codes 0xC2–0xC6 retired (`REQ-FWHUB-84`, `OI-PRIV-28` closed, GitHub #956).**
## NP-FW-HUB-001
author: NeurOne Firmware Engineering
text: **§6.3, §10: new `REQ-FWHUB-84`. Of the §7a re-enable lifecycle only the cutoff (`0xC1`) is written to SHDR; confirm-open, contact failed, contact timed out, re-enabled and assert timeout (`0xC2`–`0xC6`) said whether the wearer resumed neck stimulation after a cardiac event, so they are retired and written nowhere. `np_cvns_reenable_shdr_code()` makes the choice (`OI-PRIV-28`).**
