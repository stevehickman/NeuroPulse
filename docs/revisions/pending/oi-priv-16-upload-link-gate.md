date: 2026-11-18
## DHF
author: NeurOne Software Engineering
text: **{NP-SW-001 from → to}, {NP-FW-HUB-001 from → to}: the Compose app holds the protocol upload until the hub's encrypted-only serial has been read on the link and reports a write the link refuses (`REQ-SW03-31`, `OI-PRIV-16` closed, `OI-PRIV-17` raised); software only, no firmware changed.**
## NP-SW-001
author: NeurOne Software Engineering
text: **§5.3.1: new `REQ-SW03-31` (`OI-PRIV-16`): a refused Bluetooth write reaches the shared hub manager as an event (`Event::CharacteristicWriteFailed`, `BleCentralListener.onCharacteristicWriteFailed`) and marks a refused upload; no protocol upload write is made until `DEVICE_SERIAL` has been read on the same link (`ProtocolUploader.Result.LinkNotEncrypted`), which shows the peer answered an encrypted-only read and not that it is a genuine hub; the desktop Bluetooth library prints an error's kind and never the library's message.**
## NP-FW-HUB-001
author: NeurOne Firmware Engineering
text: **§2.5: "what this says about the apps" now states what the Compose app does (`REQ-SW03-31`): the upload waits for a successful `DEVICE_SERIAL` read on the link, which is not a check of the link's encryption and not authentication of the peer; no firmware behaviour and no row of the table changed.**
