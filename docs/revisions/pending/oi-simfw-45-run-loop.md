date: 2026-11-13
## DHF
author: NeurOne Software Engineering
text: **{NP-SIM-FW-001 from → to}: the simulator's twin runs the hub's own boot path and five tasks on a virtual-time scheduler, so a protocol it accepts starts a session; development tool only, no device document changed.**
## NP-SIM-FW-001
author: NeurOne Software Engineering
text: **§5, §7, §26, §28, §30, new §31: `REQ-SIMFW-23` and its seven parts; `OI-SIMFW-45` closed for the run loop and `OI-SIMFW-50` partly closed (the hub's five tasks, boot path, registry, session log and count and factory-reset boot check run from the device's own sources); how time works is decided and written down (a cooperative virtual-time scheduler on the one counter, with the POSIX-port and task-as-step alternatives and why they were not taken); `OI-SIMFW-74` (the hub's code never sends the session signature, and an electrical line was withheld after it in one run), `OI-SIMFW-75` (question for the principal: is a cooperative model of the kernel enough), `OI-SIMFW-76` (no session from the page alone; the Compose host does not boot or tick the twin) and `OI-SIMFW-77` (a signed blob over about 230 bytes cannot be reported) raised; seam table counts updated (`UC-177` to `UC-180`).**
