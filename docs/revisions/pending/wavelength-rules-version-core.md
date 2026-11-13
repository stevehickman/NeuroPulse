## DHF
author: NeurOne Software Engineering
text: **{NP-NPPS-REF-001 from → to}, {NP-SW-001 from → to}: the shared core builds the wavelength-rules version, and a `wavelength_rules` block may state a release `version` (GitHub #564, `OI-WRV-01`, `SWR-HV-03` parts a and b, build slices 1 and 2).**
## NP-NPPS-REF-001
author: NeurOne Software Engineering
text: **§7a, §7a.1 and §11: the `version` field of the `wavelength_rules` block, and the core's build status.** The field is optional, `major.minor`, and only a `level: global` block states one; the shipped file states `"1.0"`. A malformed version, or one on an owner's set, is a parse error. The golden parse vectors gain the field. The canonical form on a 0.1 pm grid, the hash, the 39-byte stamp, the register reader and the major/minor floor are in `common/npps-core`; the register is `common/npps/wavelength-rules-register.json`. No descriptor byte changes.
## NP-SW-001
author: NeurOne Software Engineering
text: **§6.3: `SWR-HV-03` moves from not built to partly built (parts a and b).** Part c, the session record, is the hub's and is not built. `requirement-parts.json` declares the three parts. No requirement text changed.
