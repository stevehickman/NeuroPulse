date: 2026-11-19
## DHF
author: NeurOne Software Engineering
text: **{NP-SW-001 from → to}, {NP-SOUP-RUST-001 from → to}: `OI-PRIV-12` stage 2, as the principal answered it on 2026-11-19.**
## NP-SW-001
author: NeurOne Software Engineering
text: **§6.3.7 `SWR-MG-28`: new part (g), the core's binding libraries overwrite every heap block with zeros before releasing it; part (c) is now tested by an allocator that searches each released block, which found and fixed a defect (`unhex` left the first bytes of a decoded key behind). New `SWR-MG-31`: every recording in a backup is sealed under a key of its own (HKDF-SHA-256 of the backup key with a 16-byte salt in the header), with the chunk's number as the nonce; it is part of format version 2 with the object ID of `SWR-MG-29` and takes no version of its own, because nothing has been released. New `SWR-MG-32`: a key crosses the core's boundary as bytes beside the request or is kept in the core behind a typed handle; built in the core and on every binding, and the Kotlin and TypeScript hosts have moved to it (the Swift and Windows apps have not, so a key given as hex is still read); part (f), a direct link's two channel keys are kept in the core and its flow state holds their handles (`SWR-MG-28` (d) is reworded to match); part (g), the secret an Oblivious HTTP answer is opened with is kept in the core and used once.**
## NP-SOUP-RUST-001
author: NeurOne Software Engineering
text: **`zeroize` 1.9.1 becomes a direct dependency of `common/npps-jni` (it wipes the secrets read from a Java array, `SWR-MG-32`), and the allocator the two binding libraries install uses it (`SWR-MG-28` (g)); no new crate and no version change.**
