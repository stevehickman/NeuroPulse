## DHF
author: NeurOne Firmware Engineering
text: **NP-FW-HUB-001 Rev {NP-FW-HUB-001 from → to}, NP-SW-001 Rev {NP-SW-001 from → to}: the detector's sensitivity is a setting.** Level 0 is the validated setting and nothing is below it; the hardware owner can raise it from the Helmet owner screen (`REQ-FWHUB-68`, `SWR-WO-05`). The cervical cardiac cutoff has none and is put to the principal and Risk (`OI-FWHUB-33`). The detector is still unarmed and no limit changed.
## NP-FW-HUB-001
author: NeurOne Firmware Engineering
text: **`REQ-FWHUB-68` built for the detector; `OI-FWHUB-33` raised; `OI-FWHUB-32` updated.** Levels 0 to 3 on `PPX_SENSITIVITY` (0x0020, encrypted, read two bytes, write one); raising is immediate and lowering waits for the next session; the Config record is version 2. The detection rate and false-halt cost per level on synthetic noise are in `np_ppx_tests`. Seven mutants caught. The cervical cutoff is not changed.
## NP-SW-001
author: NeurOne Software Engineering
text: **§6.3: `SWR-WO-05` added: the Helmet owner screen's sensitivity slider (built in Compose).** The Kotlin was not run where it was written; the iOS app is frozen.
