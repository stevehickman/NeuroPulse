date: 2026-11-14
## DHF
author: NeurOne Software Engineering
text: **{NP-SIM-FW-001 from → to}: the simulator runs the device's own UHDR key scheme on synthetic credentials; development tool only, no device document changed.**
## NP-SIM-FW-001
author: NeurOne Software Engineering
text: **§5, §7, §28, §30, new §34: `REQ-SIMFW-26` and its five parts; `OI-SIMFW-52` partly closed (`np_uhdr_key.c` is linked under `NPTEST_HOST` and its provision, unlock, background and change-credential run on synthetic credentials; its eight platform seams are the device's own host model, not cryptography); `OI-SIMFW-80` raised (whether the twin links the Rust primitives of `CLAUDE.md` §22, and UHDR in the files stays plaintext); the seam table's counts corrected to the gate's (14 infra, 13 firmware-host, 25 absent); `UC-187` registered; the log file layer's row, registered as `UC-182` by the previous change, which another change had already used for the audio noise band, is renumbered `UC-186`.**
