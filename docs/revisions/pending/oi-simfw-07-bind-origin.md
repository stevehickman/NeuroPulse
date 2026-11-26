## DHF
author: NeurOne Software Engineering
text: **{NP-SIM-FW-001 from → to}: OI-SIMFW-07 closed; the simulator server binds 127.0.0.1 in every mode and refuses a WebSocket upgrade from a non-loopback Origin (new REQ-SIMFW-27, tested).**
## NP-SIM-FW-001
author: NeurOne Software Engineering
text: **§5 new `REQ-SIMFW-27` (loopback bind in every mode; a 403 for any Origin but a loopback one, a missing Origin let in; the D-2 container publishes on 127.0.0.1 only, not built); §7 `OI-SIMFW-07` closed: `simulator/server/origin.js`, `index.js` and `bind-origin.test.js`, run by `simulator-ci.yml`.**
## LOG
text: **{date} — `OI-SIMFW-07` CLOSED: the simulator's device API binds `127.0.0.1` in every mode (it bound every interface without `NP_SIM_FW_BIN`) and refuses with 403 any WebSocket upgrade whose `Origin` is not a loopback `http:`/`https:` origin; a client with no `Origin` is let in (NP-SIM-FW-001 {NP-SIM-FW-001 to} §5 `REQ-SIMFW-27`, §7).** Tested by `simulator/server/bind-origin.test.js` in `simulator-ci.yml`, which now installs the server's `ws` dependency. The `D-2` container's 127.0.0.1-only port publication is part (c) of the requirement and is not built.
