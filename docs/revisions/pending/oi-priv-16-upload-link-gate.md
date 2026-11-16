date: 2026-11-15
## DHF
author: NeurOne Software Engineering
text: **{NP-SW-001 from → to}, {NP-FW-HUB-001 from → to}: the Compose apps send no protocol over a Bluetooth link until the hub's encrypted-only serial has been read on it, and a refused write reaches the shared manager (`OI-PRIV-16`, `REQ-SW03-31`).**
## NP-SW-001
author: NeurOne Software Engineering
text: **§5.3.1 `REQ-SW03-31` (new): no protocol write before a successful `DEVICE_SERIAL` read on the same link, the showing is per link, a refused write is an event the shared manager sees, and the desktop central prints the kind of a failure only (`OI-PRIV-16`).**
## NP-FW-HUB-001
author: NeurOne Firmware Engineering
text: **§2.5: what the apps do about the link is restated: the Compose apps hold the protocol upload until `DEVICE_SERIAL` has been read on the link (`REQ-SW03-31`), which shows nothing about a peer that is not a hub; the Swift app has no such gate. No hub behaviour changes.**
