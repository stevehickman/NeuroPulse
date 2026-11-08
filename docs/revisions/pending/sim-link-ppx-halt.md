## NP-SIM-FW-001
author: NeurOne Software Engineering
text: **§18 and its gaps: the simulator links the photoparoxysmal detector's own sources (`np_ppx_policy.c`, `np_ppx_detect.c`) and replaces the `np_runner_abort` double with an `np_runner_halt` double in `np_sim_spike_link.c`, so every simulator test target links again after #789 to #791; no file under `firmware/hub_control` changed (`REQ-SIMFW-03`).**
## DHF
author: NeurOne Software Engineering
text: **{NP-SIM-FW-001 from → to}: simulator links the photoparoxysmal detector sources and doubles `np_runner_halt`; all simulator test targets build and pass again.**
