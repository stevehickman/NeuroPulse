date: 2026-11-03
## DHF
author: NeurOne Firmware Engineering
text: **{NP-FW-HUB-001 from → to}, {NP-IFU-001 from → to}; new NP-SIM-FW-001 Rev 1: the firmware-in-the-loop simulator spike, a socket layout per hardware version, and the 32-socket bound removed (`REQ-FWHUB-64`, `OI-FWHUB-27`; `REQ-SIMFW-01` to `-13`).** New `NP-SIM-FW-001` (design, PBM spike, findings; scope direction of 2026-11-03: every module type). `NP-FW-HUB-001`: per-version socket layouts (`hardware/np_socket_map_v<N>.json`, generated firmware table), bound at bring-up and following the safety MCU's first report, refusing a helmet with none with `NP_HUB_ERR_NO_LAYOUT` (reason 29, app message in all eleven locales); the address validity check at load and in the PBM library; capacity the 7-bit address domain, not 32; the PBM bridge installed at bring-up; section 6.6 recording limit about 34 hours, not 49. `NP-IFU-001` `IFU-02` corrected to match. The cross-reference to the renamed socket map is corrected in five documents. Not corrected here: `NP-SES-1064-001` and `NP-FW-PBM1064-001` (still state 32) and `NP-HW-HUB-001`, `NP-HW-HEXTILE-001`, `NP-HW-EEGNET-001`, `NP-FW-CVNS-001` (still cite the old map file name): they have no history table, so the scheme cannot number a change to them (`OI-FWHUB-27`).

## NP-FW-HUB-001
author: NeurOne Firmware Engineering
text: **Socket layout per hardware version, the address validity check, the 32-socket bound removed, and the section 6.6 recording limit corrected (`REQ-FWHUB-64`, `OI-FWHUB-27`; the Principal): see the twelve parts at the head of this document.** New `np_socket_layouts` (generated table), `np_layout_bind` (bind at bring-up, follow the first report, admit, check protocol addresses), `NP_HUB_ERR_NO_LAYOUT`, `np_module_map_address_valid`, the PBM validator and bridge install, capacity 128, about 34 hours.

## NP-IFU-001
author: NeurOne Systems Engineering
text: **`IFU-02` corrected: the recording limit is about 34 hours, not 49.** The 49 hours counted EEG alone; each tracked socket adds one 40-byte UHDR record per second and `NP-FW-HUB-001` Rev {NP-FW-HUB-001 to} removed the 32-socket bound, so with EEG the per-session file cap is reached in 49.7 h with no socket tracked, 39.2 h with all 80 of the version 1 lattice and 34.8 h with all 128 addresses. The statement stays held behind its release gate; no statement is issued.

## NP-SW-001
author: NeurOne Software Engineering
text: **Minor change, `NP-QMS-DC-001` §8.2: the cross-reference to the socket map is corrected, `hardware/np_socket_map.json` is now `hardware/np_socket_map_v1.json`.** No requirement or figure changes.

## NP-HEX-ZM-001
author: NeurOne Mechanical Engineering
text: **Minor change, `NP-QMS-DC-001` §8.2: the cross-reference to the socket map is corrected, `hardware/np_socket_map.json` is now `hardware/np_socket_map_v1.json`.** No requirement or figure changes.

## NP-THERM-CFD-N1-001
author: NeurOne Thermal / Systems Engineering
text: **Minor change, `NP-QMS-DC-001` §8.2: the cross-reference to the socket map is corrected, `hardware/np_socket_map.json` is now `hardware/np_socket_map_v1.json`.** No requirement or figure changes.

## NP-THERM-SINK-001
author: NeurOne Thermal / Systems Engineering
text: **Minor change, `NP-QMS-DC-001` §8.2: the cross-reference to the socket map is corrected, `hardware/np_socket_map.json` is now `hardware/np_socket_map_v1.json`.** No requirement or figure changes.

## NP-EMC-CAV-001
author: NeurOne EMC / Systems Engineering
text: **Minor change, `NP-QMS-DC-001` §8.2: the cross-reference to the socket map is corrected, `hardware/np_socket_map.json` is now `hardware/np_socket_map_v1.json`.** No requirement or figure changes.
