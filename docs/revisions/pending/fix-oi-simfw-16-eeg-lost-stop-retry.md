date: 2026-11-09
## DHF
author: NeurOne Firmware Engineering
text: **{NP-SIM-FW-001 from → to}, {NP-FW-HUB-001 from → to}: OI-SIMFW-16 closed. A lost EEG converter stop is reported and retried.** `np_mod_eeg_hal_stop_continuous` returned nothing and `np_mod_eeg_control` cleared its recording flag whether or not the stop landed, so a stop that did not land was neither reported nor repeated (found by the firmware-in-the-loop simulator). The seam now returns `np_hub_status_t` (header `np_sw02_platform_hal.h`, the device stub `np_platform_stub.c`, the simulator `np_sim_eeg.c`; approved by the principal's coordinator because the seam is a device HAL file outside hub_control). The driver clears the flag only on `NP_HUB_OK`; a lost stop is returned to the caller and retried by the next stop or shutdown (one attempt per call, no timer: `UC-129`). Class B driver and seam only; no Class C file touched. New requirement `REQ-FWEEG-01`; new host test `np_mod_eeg_tests` (8 checks fail without the fix, all 16 pass with it); the simulator `lost stop` scenario asserts the error and a retry that lands. Counts: the host-test totals are raised from 70/59 to 71/60 against the main at branch time, and will need raising again after any other open PR that adds a target merges. Not decided: whether a bounded retry, a timer or a latched fault is wanted (`UC-129`).
## NP-SIM-FW-001
author: NeurOne Firmware Engineering
text: **§7, §14: OI-SIMFW-16 closed.** The EEG driver reports a lost stop and keeps its recording flag so the next stop retries; the `lost stop` scenario asserts the fixed behaviour.
## NP-FW-HUB-001
author: NeurOne Firmware Engineering
text: **New REQ-FWEEG-01 and §8.2: a lost EEG stop is reported and retried (OI-SIMFW-16).** The HAL stop returns a status; the driver clears its recording flag only when the stop landed. New host test `np_mod_eeg_tests`. No figure changes.
