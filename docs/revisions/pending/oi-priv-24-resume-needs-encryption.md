## DHF
author: NeurOne Firmware Engineering
text: **{NP-FW-HUB-001 from → to}, {NP-SW-001 from → to}, {NP-SIM-FW-001 from → to}: `OI-PRIV-24` (NP-PRIV-ANALYSIS-004 H4, #952), part built. The hub refuses a resume (`SESSION_PAUSE` 0x02) on a link that is not encrypted, whatever holds the session (`REQ-FWHUB-81` (d)); stop, pause and panic stay open. The false "can only reduce stimulation" reason for the §23 exception is corrected. The exception, who may resume a detector hold, and the hazard row are briefed to the principal and Safety.**
## NP-FW-HUB-001
author: NeurOne Firmware Engineering
text: **§2.5, `REQ-FWHUB-81`: new part (d), a resume on the pause write is refused with ATT 0x0F on an unencrypted link before any handler and independent of the hold cause (`OI-PRIV-24` (NP-PRIV-ANALYSIS-004)); (c) narrowed to stop, pause and panic; §2.5 gives resume its own row and says encrypted is not authenticated (`OI-SIMFW-72`). Tested in `np_gatt_server_tests` and `fw-bridge.test.js`.**
## NP-SW-001
author: NeurOne Software Engineering
text: **§6.3 `SWR-SS-05`: a resume is refused on a link that is not encrypted (`REQ-FWHUB-81` (d), `OI-PRIV-24` (NP-PRIV-ANALYSIS-004)).**
## NP-SIM-FW-001
author: NeurOne Software Engineering
text: **`OI-SIMFW-73`: the reason that the open writes can only reduce stimulation was false for resume; corrected, with the change that made it true (`OI-PRIV-24` (NP-PRIV-ANALYSIS-004)).**
