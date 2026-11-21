date: 2026-11-21
## DHF
author: NeurOne Firmware Engineering
text: **{NP-SW-001 from → to}, {NP-FW-HUB-001 from → to}: firmware anti-rollback built (REQ-SW02-12) and the USB DFU path listed in the §23 transfer table (OI-PRIV-32 (NP-PRIV-ANALYSIS-004), GitHub #960); the floor's store and DFU encryption are briefed for the principal.**
## NP-SW-001
author: NeurOne Firmware Engineering
text: **§5.2.2: REQ-SW02-12 added. The bootloader installs no firmware image whose signed version is below a monotonic floor, on the OTA/DFU verify path and at the boot trial; an unreadable floor refuses every image; the floor rises only at confirmation and never falls. The store is not chosen, so the device build refuses every image until it is (OI-PRIV-32 (NP-PRIV-ANALYSIS-004), NP-PRIV-ANALYSIS-004 Finding M5).**
## NP-FW-HUB-001
author: NeurOne Firmware Engineering
text: **§2.5: USB DFU recovery row added (built, plaintext, not covered; payload encryption or a §23 exception is the principal's decision) and the OTA row notes the rollback floor (OI-PRIV-32 (NP-PRIV-ANALYSIS-004), NP-PRIV-ANALYSIS-004 Finding M6).**
