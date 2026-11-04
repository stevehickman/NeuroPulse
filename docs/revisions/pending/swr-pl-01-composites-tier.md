date: 2026-11-03
## DHF
author: NeurOne Software Engineering
text: **{NP-NPPS-REF-001 from → to}, {NP-SW-001 from → to}: composites compile in the shared core, uploads are identified by content hash, and the hub reports its tier over GATT (`SWR-PL-01`, `OI-NPPS-COMP-01`, `OI-HWFIT-01`).** `NP-NPPS-REF-001` §6 states what a composite compiles to and the rules it follows; `NP-SW-001` §6.3 moves `SWR-PL-01` to partly built. New open items `OI-NPPS-COMP-01` and `OI-HWFIT-01`, register row `UC-119`.
## NP-NPPS-REF-001
author: NeurOne Software Engineering
text: **§6: what a composite compiles to, and its rules (principal direction, option A of `OI-NPPS-COMP-01`).** Layers are laid on one timeline and compiled to one descriptor; two layers driving one modality at once are refused except transcranial PBM on sockets they do not share; a layer may not outrun its protocol; `intensity_scale` is above 0 and at most 1. Two shipped composites are refused by the overlap rule until a decision.
## NP-SW-001
author: NeurOne Software Engineering
text: **§6.3: `SWR-PL-01` moves to partly built.** The upload gate identifies an upload by its content hash and refuses one the library does not hold (`not_in_library`), in the shared core and the Kotlin, Swift and web libraries, with refusal dialogs in the shared Compose UI and iOS; a composite is compiled and identified by the hash of its plan. No requirement text changed.
