date: 2026-11-15
## DHF
author: NeurOne Software Engineering
text: **{NP-SW-001 from → to}, {NP-APP-PRESENCE-001 from → to}: privacy review of `common/` (`OI-PRIV-01` to `-16`); the migration confirmation is keyed and a gated install with no key list no longer opens the host's store.**
## NP-SW-001
author: NeurOne Software Engineering
text: **§6.3 `SWR-MG-08`: the confirmation carries a tag made from the bundle key, the old phone records that tag when it seals the bundle, and it clears only on a confirmation carrying it (`OI-PRIV-01`).**
## NP-APP-PRESENCE-001
author: NeurOne Software Engineering
text: **`REQ-PRES-09` (b): a gated install that cannot get the list of protected keys holds the whole run in memory and does not touch the host's store (`OI-PRIV-03`).**
