date: 2026-11-18
## DHF
author: NeurOne Software Engineering
text: **{NP-SW-001 from → to}, {NP-SOUP-RUST-001 from → to}: stage 1 of `OI-PRIV-12`: secrets the shared core and its bindings handle are wiped or dropped where the code can decide it (`SWR-MG-28`).**
## NP-SW-001
author: NeurOne Software Engineering
text: **§6.3.7 `SWR-MG-28` added: `npps_free` and the web binding wipe a buffer before releasing it, the core's key buffers are wiped on drop, a direct-link state keeps only the channel keys once the shared key is made, the pairing's random source refuses to repeat bytes, and four classes no longer print the pairing code or UHDR from `toString` (`OI-PRIV-12` stage 1; stage 2, keys as strings across the boundary, is not built).**
## NP-SOUP-RUST-001
author: NeurOne Software Engineering
text: **§2: `common/npps-ffi` names `zeroize` directly and `aes-gcm` gains its `zeroize` feature (`SWR-MG-28`, `OI-PRIV-12`); no crate and no version is added to the tree.**
