## DHF
author: NeurOne Firmware Engineering
text: **NP-FW-HUB-001 Rev {NP-FW-HUB-001 from → to}, NP-SW-001 Rev {NP-SW-001 from → to}: the detector's two clinician switches have a home on the helmet and a screen in the app.** A replicated Config record and the `PPX_CONFIG` characteristic (`REQ-FWHUB-71`); the Compose Helmet owner screen offers them (`SWR-WO-04`). The override cannot be set while the detector has no validation. No limit changed, and the refusal of photic driving at 3 to 30 Hz stands.
## NP-FW-HUB-001
author: NeurOne Firmware Engineering
text: **`REQ-FWHUB-71` built; `OI-FWHUB-32` updated.** `np_ppx_config.c`, `NP_CFG_FILE_PPX_CONFIG` and `PPX_CONFIG` (0x001E, encrypted, read two bytes, write one). Safe defaults on every failure; no pre-set override; protecting more is immediate and protecting less waits for the next session. `np_ppx_config_tests` (six mutants caught) and `np_gatt_server_tests`.
## NP-SW-001
author: NeurOne Software Engineering
text: **§6.3: `SWR-WO-04` added: the Helmet owner screen's detector switches (built in Compose).** The Kotlin was not run where it was written; the iOS app is frozen.
