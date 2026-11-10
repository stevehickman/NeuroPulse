date: 2026-11-10
## DHF
author: NeurOne Software Engineering
text: **{NP-SIM-FW-001 from → to}: the Compose helmet view shows the reason beside each state (`OI-SIMFW-37`, `REQ-SIMFW-18`#d); the app reaches the twin over GATT through a simulated BLE central (`OI-SIMFW-38`, `REQ-SIMFW-19`).**
## NP-SIM-FW-001
author: NeurOne Software Engineering
text: **§5, §7, §25: `REQ-SIMFW-18`#d (the reason beside each state: a closed set of typed reasons, only causes the firmware reports, a cause not reported shown as not reported); the table of reasons and where each is reported; `OI-SIMFW-47` raised (several causes reach the view as one: a driver's own check, an MCU withhold with status OK, a module fault no tile reports); `OI-SIMFW-37` third slice; `UC-138` registered. **§5, §7, §26, new §27, `REQ-SIMFW-19`: the hub's own GATT server (`np_gatt_server.c`, its producers and the config store over a RAM medium) runs in the twin; the bridge relays `gatt_*` as validated ids and hex; the app's `BleCentral` seam has a simulated implementation, the browser host takes it with `?hub=sim`, and the app says the hub is simulated; `OI-SIMFW-38` second slice; `OI-SIMFW-46` narrowed; `OI-SIMFW-48` (the link is bonded) and `OI-SIMFW-49` (the hub does not publish the protocol upload and the app's chunk framing is not the transport's) raised; `UC-139` registered.**
