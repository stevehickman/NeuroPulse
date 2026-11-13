date: 2026-11-13
## DHF
author: NeurOne Software Engineering
text: **{NP-SW-001 from → to}, {NP-FW-HUB-001 from → to}: `SWR-HV-03` core, descriptor and hub halves built (issue #569, GitHub #564): the wavelength-rules stamp, descriptor version 3.**
## NP-SW-001
author: NeurOne Software Engineering
text: **§6.3: `SWR-HV-03` status updated, issue #569.** The shared core computes the rules' identity and 39-byte stamp (vectors V1 to V6 of `OI-WRV-01`), the compiler writes it into the signed descriptor, and the hub copies it into the session record. The register file, grammar field, app notice and refusal of an unstamped upload are not built. No requirement text changed.
## NP-FW-HUB-001
author: NeurOne Firmware Engineering
text: **§4.1, §4.2, §4.5: descriptor version 3 (`SWR-HV-03`).** The header grows from 68 to 107 bytes: a 39-byte wavelength-rules stamp after the reserved bytes, under the signature. The hub copies it unread into the UHDR fitting frame (format 2) and records an all-zero stamp as not stated. A v2 blob is refused as `NP_HUB_ERR_BAD_VERSION`. No requirement text changed.
