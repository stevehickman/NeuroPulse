## DHF
author: NeurOne Software Engineering
text: **{NP-SIM-FW-001 from → to}: the simulator runs the device's own safety MCU sources in the loop (`OI-SIMFW-09` closed; `REQ-SIMFW-14`), and the register `UC-124` records the loop period it assumes.**
## NP-SIM-FW-001
author: NeurOne Software Engineering
text: **New §22 and `REQ-SIMFW-14`: the safety MCU's own sources run unmodified behind the hub's `np_safety_spi.c` over a 40-function host HAL, paced at the independent watchdog's refresh; 19 scenarios drive the heartbeat and independent watchdogs, both charge ceilings, the impedance and declaration gates, the thermal cutoff, the tier gate and the cardiac cutoff (with its persistence across a power-on reset) to their cutoffs; `OI-SIMFW-09` closed and `OI-SIMFW-24` to `OI-SIMFW-26` raised (a thermal table that does not match its stated divider, the paths no scenario reaches, and the bridge and browser).**
