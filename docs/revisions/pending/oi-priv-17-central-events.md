date: 2026-11-19
## DHF
author: NeurOne Software Engineering
text: **{NP-SW-001 from → to}: the upload gate opens only on the 32-byte serial and a held upload asks for the read again; the Android, web and iOS centrals report refused reads and writes (`OI-PRIV-17`; iOS and Android not compiled); the SwiftUI app gates its protocol upload on the encrypted serial read; the other writes to encrypted characteristics wait for it too.**
## NP-SW-001
author: NeurOne Software Engineering
text: **§5.3.1 `REQ-SW03-31` (b): the read must return the 32-byte serial, and an upload held at the gate asks for the read again; (d) the other writes to encrypted characteristics wait for it; an unprovisioned hub takes no Bluetooth upload (`OI-PRIV-17`).**
