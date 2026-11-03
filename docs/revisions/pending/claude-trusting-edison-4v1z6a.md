date: 2026-11-03
## DHF
author: NeurOne Firmware Engineering
text: **{NP-FW-HUB-001 from → to}: the revision scheme starts a history table for a document that has none (`scripts/assign-revisions.ts`), and the six documents withdrawn from the previous change are corrected: `NP-SES-1064-001`, `NP-FW-PBM1064-001` (32 → 128 sockets), `NP-HW-HUB-001`, `NP-HW-HEXTILE-001`, `NP-HW-EEGNET-001`, `NP-FW-CVNS-001` (map file name).** The Principal, 2026-11-03.

## NP-FW-HUB-001
author: NeurOne Firmware Engineering
text: **`OI-FWHUB-27` item 5 updated: the six documents are corrected now that the revision scheme can number them (the Principal, 2026-11-03).** No requirement or figure changes.

## NP-SES-1064-001
author: NeurOne Firmware Engineering
text: **Minor change, `NP-QMS-DC-001` §8.2: the stated session capacity is corrected from 32 sockets to 128, the 7-bit address domain, with the rule that a session is limited by the addresses the helmet's version has and not by a count (`NP-FW-HUB-001` `REQ-FWHUB-64`).** Section 1 (expansion bound) and section 6.7 (record sizes). No session descriptor wire layout changes.

## NP-FW-PBM1064-001
author: NeurOne Firmware Engineering
text: **Minor change, `NP-QMS-DC-001` §8.2: the stated session capacity is corrected from 32 sockets to 128, the 7-bit address domain, with the rule that a session is limited by the addresses the helmet's version has and not by a count (`NP-FW-HUB-001` `REQ-FWHUB-64`).** The two capacity statements. No behaviour of the 1064 nm driver changes.

## NP-HW-HUB-001
author: NeurOne Hardware Engineering
text: **Minor change, `NP-QMS-DC-001` §8.2: the cross-reference to the socket map is corrected, `hardware/np_socket_map.json` is now `hardware/np_socket_map_v1.json`.** No requirement or figure changes.

## NP-HW-HEXTILE-001
author: NeurOne Hardware Engineering
text: **Minor change, `NP-QMS-DC-001` §8.2: the cross-reference to the socket map is corrected, `hardware/np_socket_map.json` is now `hardware/np_socket_map_v1.json`.** No requirement or figure changes.

## NP-HW-EEGNET-001
author: NeurOne Hardware Engineering
text: **Minor change, `NP-QMS-DC-001` §8.2: the cross-reference to the socket map is corrected, `hardware/np_socket_map.json` is now `hardware/np_socket_map_v1.json`.** No requirement or figure changes.

## NP-FW-CVNS-001
author: NeurOne Firmware Engineering
text: **Minor change, `NP-QMS-DC-001` §8.2: the cross-reference to the socket map is corrected, `hardware/np_socket_map.json` is now `hardware/np_socket_map_v1.json`.** No requirement or figure changes.
