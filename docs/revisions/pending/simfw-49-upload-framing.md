date: 2026-11-11
## DHF
author: NeurOne Software Engineering
text: **{NP-FW-HUB-001 from → to}, {NP-SW-001 from → to}, {NP-SIM-FW-001 from → to}: the protocol upload is the transport's byte stream on both transports (`OI-SIMFW-49` closed, option A): the hub publishes 0x0008, the apps' chunkers write the length-prefixed stream.**
## NP-FW-HUB-001
author: NeurOne Firmware Engineering
text: **§2.3, §2.5, §10.1: the BLE binding of the protocol upload (0x0008, an encrypted write-only stream of 1 to 512 bytes a write, bound to `np_transport_feed()`, `NP_ATT_APP_BUSY`, `np_gatt_on_disconnect()`) and the apps' framing stated on both transports (`REQ-FWHUB-80`); new §2.5, every path that carries data and what encrypts it, and `REQ-FWHUB-81` (every BLE characteristic that carries data requires an encrypted link, ATT 0x0F otherwise, the link state fails closed); the encryption rule is not met (no payload encryption, USB-C uncovered).**
## NP-SW-001
author: NeurOne Software Engineering
text: **§5.3.1: `REQ-SW03-11` restated for the stream framing (a 4-byte little-endian length, then the blob, in writes of at most 512 bytes) in place of the START, CONT, END and SINGLE frames.**
## NP-SIM-FW-001
author: NeurOne Software Engineering
text: **§5, §7, §26, §27: `OI-SIMFW-49` closed (the twin takes the apps' writes and accepts a TEST-key-signed golden over GATT); `REQ-SIMFW-19` updated; `OI-SIMFW-62` and `OI-SIMFW-64` closed, `OI-SIMFW-63` raised (the 512-byte writes whatever the MTU), `OI-SIMFW-70` to `OI-SIMFW-73` raised (the layer, end-to-end payload encryption including USB-C, authenticated pairing, the two unencrypted control writes); `UC-150` moved to justified and `UC-151` registered.**
