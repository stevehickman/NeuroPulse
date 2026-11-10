date: 2026-11-10
## DHF
author: NeurOne Software Engineering
text: **{NP-SIM-FW-001 from → to}: first slice of the shared Compose helmet view recorded (written, not built).; and the twin takes the app's protocol as the device's framed bytes over the hub's own transport (`OI-SIMFW-38`, first slice); `OI-SIMFW-42` raised (the hub's call to the Ed25519 verifier does not match the library). {NP-FW-HUB-001 from → to}: the hub verifies a signed protocol with its own bounded Ed25519 check, `REQ-FWHUB-76` (`OI-SIMFW-42` closed).**
## NP-SIM-FW-001
author: NeurOne Software Engineering
text: **§7: `OI-SIMFW-37` records the first slice of the shared Compose helmet view (state and drawing rule, 2D socket map, legend, a test of `REQ-SIMFW-07`), that Gradle could not run so none of it is built or tested, and what is missing against `helmet.js`. Also: New §26 and `REQ-SIMFW-17`: the bridge relays an upload as validated hex, the hub's own transport and upload check take it in, and the app receives the hub's verdict frame; `OI-SIMFW-38` first slice built; `OI-SIMFW-42` (the hub's verifier call is wrong, so no signed protocol verifies), `OI-SIMFW-43` (the route stops at the runner) and `OI-SIMFW-44` (the signing stand-in) raised; `UC-136` registered.**
## NP-FW-HUB-001
author: NeurOne Firmware Engineering
text: **§4.4: `REQ-FWHUB-76`, the protocol signature check: key first, message of any length up to `NP_HUB_PROTO_BLOB_MAX`, an all-zero key refused; `np_protocol.c` calls `np_proto_verify_signature` (`np_proto_verify.h`) instead of the local declaration of `np_ed25519_verify` that had the message first and was capped at 256 bytes by the library (`OI-SIMFW-42`); no Class C file changed.**
