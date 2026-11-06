## DHF
author: NeurOne Firmware Engineering
text: **NP-FW-HUB-001 Rev {NP-FW-HUB-001 from → to}, NP-SW-001 Rev {NP-SW-001 from → to}: the panic control is built.** The hub accepts a panic during a ramp-up and cuts every modality at once (`REQ-FWHUB-72`); the Compose app has the control in the lower left corner of every screen and a centred Stop / Resume dialog (`SWR-SS-09`). No limit changed.
## NP-FW-HUB-001
author: NeurOne Firmware Engineering
text: **`REQ-FWHUB-72` added; `OI-FWHUB-31` updated.** `SESSION_PAUSE` `0x03` runs `np_runner_panic()`: never refused for a ramp, tDCS cut with no ramp-down, an open ramp gate closed, what it had not started held for the resume. Four mutants caught in `np_runner_run_tests`. One path (a start gate closed before its second ramped modality started) is not exercised by a host test.
## NP-SW-001
author: NeurOne Software Engineering
text: **§6.3.6: `SWR-SS-09` status updated: built (hub and Compose app).** No requirement text changed. The Kotlin was not run where it was written.
