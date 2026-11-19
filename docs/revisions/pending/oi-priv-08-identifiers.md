## DHF
author: NeurOne Software Engineering
text: **{NP-SW-001 from → to}, {NP-ANALYTICS-001 from → to}, {NP-APP-TELEMETRY-001 from → to}: identifiers no longer outlive the consent or device state they belong to (`OI-PRIV-08`, closed): the erasure queue drops an analytics ID only on a 2xx, an erase asks the relay before it wipes, the suggestion author token goes with the portal opt-in or blanket consent, and raw Bluetooth address rows are migrated at launch.**
## NP-SW-001
author: NeurOne Software Engineering
text: **§5.3.1: `REQ-SW03-19` (b) now drops an analytics ID only when the relay accepted it (a 4xx keeps it and the drain goes on) and gains (d), a device erase forgets the ID and asks the relay before anything is wiped; `REQ-SW03-25` (c), (d): the ID is made with the first event, forgotten on reset, and queued by a withdrawal whether or not the gate was configured in that run; `REQ-SW03-12` is split into parts and gains (e) no suggestion and no author token without the portal opt-in and (f) the token is forgotten when the portal opt-in or blanket consent is withdrawn (`OI-PRIV-08` (1) to (3)).**
## NP-ANALYTICS-001
author: NeurOne Software Engineering
text: **§7.6: the app keeps a queued ID on every 4xx and asks again at its next launch; only a 2xx drops it, so a relay deployed at the wrong path or behind a refusing proxy loses nothing (`OI-PRIV-08` (2), `REQ-SW03-19` (b)).**
## NP-APP-TELEMETRY-001
author: NeurOne Software Engineering
text: **§5: the erasure queue keeps the identifier until the relay has accepted it (a refusal never drops it), and a device erase asks the relay before it wipes the queue (`REQ-SW03-19` (b), (d)).**
