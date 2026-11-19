date: 2026-11-18
## DHF
author: NeurOne Software Engineering
text: **{NP-APP-PRESENCE-001 from → to}, {NP-SW-001 from → to}: the session history follows the presence lock, keeps no figure finer than it shows, and is kept as long as the person chooses (OI-PRIV-10).**
## NP-APP-PRESENCE-001
author: NeurOne Software Engineering
text: **§2, §3, §7: new REQ-PRES-01 (d): a store that caches UHDR holds none while locked, reads again at each unlock and merges what was done locked, never writing it in place of the sealed value. Built for the session history (OI-PRIV-10 (1)); the other stores are OI-PRIV-19, which closes before any host offers the gate. §3 records that a removal acting while locked is deliberate (OI-PRIV-10 (4)).**
## NP-SW-001
author: NeurOne Software Engineering
text: **§5.3.1: REQ-SW03-06 now also says a history record keeps no figure finer than the History screen shows (whole minutes, coherence to one decimal place; OI-PRIV-10 (2)); the Kotlin store coarsens a record an earlier build wrote when it reads it, and the Swift store is unchanged. New REQ-SW03-33: the person decides how long the session history is kept (default no age limit) and can clear it from Settings (the principal, 2026-11-19).**
