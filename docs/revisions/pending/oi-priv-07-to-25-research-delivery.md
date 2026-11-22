date: 2026-11-21
## DHF
author: NeurOne Software Engineering
text: **CLAUDE.md Rev 85 → 89, {NP-SW-001 from → to}, {NP-SW-PORTAL-API-001 from → to}, {NP-FW-ANON-001 from → to}, {NP-IRB-001 from → to}: the study participation audit trail is UHDR and SHDR holds no research field beyond `anonymization_failed` (`OI-PRIV-07`); the research store is keyed to study ID only (`OI-PRIV-21`); a study declares up front whether it needs tracking over time (`OI-PRIV-24`); research requests reach devices by subscription and publish, a device subscribing only while current for one of its users, with no consent at the server (`OI-PRIV-22`); every device-to-NeurOne research call goes through the Oblivious HTTP relay (`OI-PRIV-23`); the portal shows a participant count only (`OI-PRIV-25`). All the principal's rulings, 2026-11-20 and 2026-11-21.**
## NP-SW-001
author: NeurOne Software Engineering
text: **§6.3 new SWR-MG-33 and SWR-MG-34 (the core composes, signs, verifies and orders the superseded notice, built in `current_device.rs`); SWR-MG-33: a migration or restore asks the research consents again and is not finished until they are answered, then makes the device current for the person, subscribes it if needed and publishes a signed superseded notice, queued when offline; the current-device key is a carried item (the principal, 2026-11-20 and 2026-11-21, OI-PRIV-22). Not built.**
## NP-SW-PORTAL-API-001
author: NeurOne Software Engineering
text: **§4a: the study audit trail is UHDR, held on the device, never in SHDR (OI-PRIV-07); §3 amended for study requests (subscribe and publish, not pull; clinician requests stay pull-only); new §6a (DRAFT): the subscribe, unsubscribe, publish, fetch and supersede calls and the NP-SUPERSEDED-V1 form, all through the Oblivious HTTP relay (OI-PRIV-22, OI-PRIV-23).**
## NP-FW-ANON-001
author: NeurOne Firmware Engineering
text: **§8.1: the per-study budget record is UHDR and never copied to SHDR; §8.2: the `NP_ANON_CONSENT_WITHDRAWN` SHDR event is retired (`OI-PRIV-07`, the principal, 2026-11-20).**
## NP-IRB-001
author: NeurOne Software Engineering
text: **§5 and the Type B row: NeurOne research infrastructure stores extracts keyed to study ID only, with no device identifier (OI-PRIV-21); a study that declares tracking sends an identifier unique to the person and the study, so its extracts are coded and the exemption argument does not yet cover it, under review by the IRB adviser (OI-PRIV-24).**
## NP-SOUP-RUST-001
author: NeurOne Software Engineering
text: **`ed25519-dalek` row: gains a use, the signature and strict verification of a superseded notice in `current_device.rs`; no new crate and no version change (`OI-PRIV-22`, `SWR-MG-34`).**
