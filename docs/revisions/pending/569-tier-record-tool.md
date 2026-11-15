date: 2026-11-13
## DHF
author: NeurOne Software Engineering
text: **NP-REG-UPG-001 and NP-SOUP-RUST-001 revised: the host part of the tier-identity factory step is built (issue #569 package 8).**
## NP-REG-UPG-001
author: NeurOne Software Engineering
text: **§7.9, §7.5 (REQ-UPG-04) and §9 (OI-UPG-08): the byte-level stages of the factory step that need no manufacturer exist as `tier_record.rs` in `common/npps-core` (record builder, firmware-equivalent verifier, refusals of steps 2, 3, 5 and 6, tests pinned to the firmware's signer). No key, signer, fixture or lock exists; what waits and the principal's seven decisions are in a human-input request. No decision changed.**
## NP-SOUP-RUST-001
author: NeurOne Software Engineering
text: **`ed25519-dalek` gains a use in `tier_record.rs` (plain `verify`, mirroring the safety MCU's check); no new crate and no version change.**
