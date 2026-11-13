## DHF
author: NeurOne Software Engineering
text: **{NP-NPPS-REF-001 from → to}, {NP-SW-001 from → to}, {NP-FW-HUB-001 from → to}: the wavelength-rules version: the shared core computes it, a `wavelength_rules` block may state a release `version`, and the signed session descriptor (version 3) carries the stamp (GitHub #564, `OI-WRV-01`, `SWR-HV-03` parts a and b, build slices 1 to 3).**
## NP-NPPS-REF-001
author: NeurOne Software Engineering
text: **§7a, §7a.1 and §11: the `version` field of the `wavelength_rules` block, and the build status.** The field is optional, `major.minor`, and only a `level: global` block states one; the shipped file states `"1.0"`. A malformed version, or one on an owner's set, is a parse error. The canonical form on a 0.1 pm grid, the hash, the 39-byte stamp, the register reader and the major/minor floor are in `common/npps-core`; the register is `common/npps/wavelength-rules-register.json`; the compiler writes the stamp into the descriptor.
## NP-SW-001
author: NeurOne Software Engineering
text: **§6.3: `SWR-HV-03` moves from not built to partly built (parts a and b).** Part c, the hub's copy into the session record, is not built. `requirement-parts.json` declares the three parts. No requirement text changed.
## NP-FW-HUB-001
author: NeurOne Firmware Engineering
text: **§4.1, §4.2, §4.4, §4.5 and `REQ-FWHUB-83`: the session descriptor is version 3, and its header carries the 39-byte wavelength-rules stamp.** The header is 107 bytes (68 + the stamp), the stamp is under the signature and outside the protocol's content hash, the hub keeps it and refuses an origin above 2 or a "not stated" stamp that is not zero, and a v2 descriptor is refused as `NP_HUB_ERR_BAD_VERSION`. No descriptor has shipped. The wire-format gate checks the stamp.
