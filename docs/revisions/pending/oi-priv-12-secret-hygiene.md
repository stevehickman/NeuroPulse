date: 2026-11-16
## DHF
author: NeurOne Software Engineering
text: **{NP-SW-001 from → to}, {NP-SOUP-RUST-001 from → to}: secrets are cleared from the memory the shared core and its wrappers control (`OI-PRIV-12` stage 1, new `SWR-MG-28`).**
## NP-SW-001
author: NeurOne Software Engineering
text: **§6.3.7: new `SWR-MG-28`. `npps_free` and the web wrapper fill a buffer with zeros before releasing it; the core's decoded keys and secrets are cleared when a call ends; a direct link's state drops the pairing code, the SPAKE2 randomness and the shared key at the turn that last uses them; the pairing's randomness is never handed out twice; the text of a pairing offer, a session record and an adaptation event holds no code and no health value. The row states what is not covered: keys still cross the boundary as strings (`OI-PRIV-12` stage 2).**
## NP-SOUP-RUST-001
author: NeurOne Software Engineering
text: **`zeroize` 1.9.1 becomes a direct dependency of `common/npps-ffi` as well as of the core, and `aes-gcm` gains its `zeroize` feature; no new crate and no version change (`SWR-MG-28`).**
