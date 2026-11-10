date: 2026-11-10
## DHF
author: NeurOne Software Engineering
text: **{NP-SIM-FW-001 from → to}: the twin takes the app's protocol as the device's framed bytes over the hub's own transport (`OI-SIMFW-38`, first slice); `OI-SIMFW-42` raised (the hub's call to the Ed25519 verifier does not match the library).**
## NP-SIM-FW-001
author: NeurOne Software Engineering
text: **New §26 and `REQ-SIMFW-17`: the bridge relays an upload as validated hex, the hub's own transport and upload check take it in, and the app receives the hub's verdict frame; `OI-SIMFW-38` first slice built; `OI-SIMFW-42` (the hub's verifier call is wrong, so no signed protocol verifies), `OI-SIMFW-43` (the route stops at the runner) and `OI-SIMFW-44` (the signing stand-in) raised; `UC-136` registered.**
