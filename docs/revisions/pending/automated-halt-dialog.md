## DHF
author: NeurOne Firmware Engineering
text: **NP-FW-HUB-001 Rev {NP-FW-HUB-001 from → to}, NP-SW-001 Rev {NP-SW-001 from → to}: the Stop / Resume dialog opens for a halt the helmet makes on its own.** The hub says why a session is held on `SESSION_HOLD`; the Compose app puts the question to the person when the detector halted. No limit changed, and the detector is still unarmed.
## NP-FW-HUB-001
author: NeurOne Firmware Engineering
text: **`REQ-FWHUB-72` extended with the hold cause; `OI-FWHUB-32` updated.** `SESSION_HOLD` (0x001F, READ/NOTIFY, one byte); `np_runner_halt(cause)`; a detector cause outranks a person's. A separate characteristic so the frozen iOS app, which checks `SESSION_PROGRESS` for exactly 22 bytes, is not broken. `np_session_hold_tests` and `np_runner_run_tests`.
## NP-SW-001
author: NeurOne Software Engineering
text: **§6.3.6: `SWR-SS-09` status updated: the dialog for an automated halt is built (Compose).** No requirement text changed. The Kotlin was not run where it was written.
