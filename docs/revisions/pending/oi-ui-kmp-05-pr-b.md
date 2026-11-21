date: 2026-11-21
## DHF
author: NeurOne Software Engineering
text: **{NP-SW-001 from → to}, {NP-APP-PRESENCE-001 from → to}: the SwiftUI iOS app is retired and `app/ios` deleted (`OI-UI-KMP-05` PR B, the principal, 2026-11-21); `NP-APP-ISA-001` moved to `docs/superseded/` and its index row and the §6b source paths re-pointed to the shared Kotlin code; `OI-PRES-05` closed; the device-run gate was waived, not met.**
## NP-SW-001
author: NeurOne Software Engineering
text: **§5.3.1: the SwiftUI iOS app is deleted (`OI-UI-KMP-05` PR B). `REQ-SW03-18` (a) is retired in place, and the Swift key-manager clauses of `REQ-SW03-08` are retired in place (the core's derivation, split and destroyed-key refusal stay required, verified by `UhdrKeyDerivationTests` and `bundle_key.rs`); the shared iOS host is the iOS app, and the Swift test groups, `ModelsParityTests` and the iOS CI workflow are gone.**
## NP-APP-PRESENCE-001
author: NeurOne Software Engineering
text: **§6: `OI-PRES-05` is closed: the Swift app was retired instead of given the gate, and `app/ios` is deleted (`OI-UI-KMP-05` PR B, 2026-11-21).**
