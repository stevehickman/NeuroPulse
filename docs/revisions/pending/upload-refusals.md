## DHF
author: NeurOne Firmware Engineering
text: **NP-FW-HUB-001 Rev {NP-FW-HUB-001 from → to}, NP-SW-001 Rev {NP-SW-001 from → to}: upload refusals an author can fix get statuses of their own.** The photic-zone refusal is made at upload (`NP_HUB_ERR_PHOTIC_ZONE`, `REQ-FWHUB-66`), a socket address the helmet lacks is `NP_HUB_ERR_BAD_ADDRESS` and too many commands has its own cause, each with a string in every app. What else can fail an upload without a status of its own, and why it is left, is `OI-FWHUB-30`. No limit changed.
## NP-FW-HUB-001
author: NeurOne Firmware Engineering
text: **`REQ-FWHUB-66` added and `OI-FWHUB-30` raised: the photic-zone refusal at upload, and an audit of upload failures.** `np_runner_load()` refuses a goggles command that drives photically at 3 to 30 Hz with `NP_HUB_ERR_PHOTIC_ZONE` (30) before anything is requested of the safety MCU. The layout's address check returns `NP_HUB_ERR_BAD_ADDRESS` (31), not `NP_HUB_ERR_INVALID_ARG`. `np_protocol_tests`, `np_runner_load_tests` and `np_socket_layouts_tests` cover them (four mutants caught). The transport's silent discards are not reachable until the upload characteristic is bound (`OI-HUB-MAIN-01`) and are recorded in `OI-FWHUB-30`.
## NP-SW-001
author: NeurOne Software Engineering
text: **§6.3: `SWR-UP-02` status updated (the upload report's causes).** The helmet's refusals for a photic goggles frequency of 3 to 30 Hz (30), a socket address it lacks (31) and too many commands (8) have causes of their own in the shared core (`photic_zone`, `bad_address`, `too_many_commands`) and a string in the Compose and iOS apps; reasons 2 and 9 stay `malformed`. No requirement text changed. The Kotlin, Swift and C# changes were not run here.
