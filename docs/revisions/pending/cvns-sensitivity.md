## DHF
author: NeurOne Firmware Engineering
text: **NP-FW-CVNS-001 Rev {NP-FW-CVNS-001 from → to}, NP-HW-CVNS-001 Rev {NP-HW-CVNS-001 from → to}, NP-FW-HUB-001 Rev {NP-FW-HUB-001 from → to}, NP-SW-001 Rev {NP-SW-001 from → to}: the cervical cardiac cutoff has a sensitivity setting.** Level 0 is the validated 15 BPM of `REQ-CVNS-09` and nothing is less sensitive; two provisional levels above it (`REQ-CVNS-14`, `REQ-FWHUB-74`, `SWR-WO-06`, `UC-126`). Class C change in the safety MCU. The Risk re-score of hazard 25-e is owed.
## NP-FW-CVNS-001
author: NeurOne Firmware Engineering
text: **§14.8 added: cardiac cutoff sensitivity (`REQ-CVNS-14`).** Levels 1 and 2 cut at over 14 and 13 BPM; a higher level acts at once and a lower one at the next CVNS request; carried in the HR report's reserved word. Synthetic detection and false-trip costs per level are tabulated. The 15 BPM threshold of `REQ-CVNS-09` is unchanged at level 0. IEC 62304 SW-01 Class C: `np_cardiac_interlock.c` and `np_safety_main.c` change; verification is host tests only, with the ARM build and silicon timing not run.
## NP-HW-CVNS-001
author: NeurOne Firmware Engineering
text: **`REQ-CVNS-14` added: the owner may make the cardiac interlock more sensitive and never less.** `REQ-CVNS-09` as presented to FDA is the level-0 behaviour and is not changed.
## NP-FW-HUB-001
author: NeurOne Firmware Engineering
text: **`REQ-FWHUB-74` added; `OI-FWHUB-33` updated from "not built" to built with the Risk re-score owed.** `CARDIAC_SENSITIVITY` (0x0021, encrypted, read two bytes, write one); owner's Config record; the level rides in the HR report to the safety MCU every second. Hub and MCU update together.
## NP-SW-001
author: NeurOne Software Engineering
text: **§6.3: `SWR-WO-06` added: the Helmet owner screen's cardiac cutoff sensitivity slider (built in Compose).** The Kotlin was not run where it was written; the iOS app is frozen.
