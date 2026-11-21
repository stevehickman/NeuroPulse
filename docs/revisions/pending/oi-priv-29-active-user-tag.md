## DHF
author: NeurOne Software Engineering
text: **{NP-SW-FAULTMSG-001 from → to}: the active-user tag's replacement derivation is specified and its core built (OI-PRIV-29, NP-PRIV-ANALYSIS-004, GitHub #957).**
## NP-SW-FAULTMSG-001
author: NeurOne Software Engineering
text: **§9.2: adds REQ-FAULTMSG-01 (a per-(profile, helmet) HMAC-SHA-256 active-user tag, keyed by a per-profile secret; reserved values never produced; a pair moves from its old tag only when nothing is outstanding on the helmet, so no cardiac hold is released). States that today's tag is a profile-UUID prefix and UHDR-class. Core built in common/npps-core/src/active_user_tag.rs; app wiring and a Safety ruling on hazard 25-c are open (OI-PRIV-29, NP-PRIV-ANALYSIS-004).**
