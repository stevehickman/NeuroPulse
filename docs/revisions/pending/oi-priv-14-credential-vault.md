date: 2026-11-15
## DHF
author: NeurOne Software Engineering
text: **{NP-SW-001 from → to}, {NP-SW-CLOUDSTORE-001 from → to}: the cloud credential vault opens only over storage that seals it, and a Google client secret is refused on a phone (`OI-PRIV-14`, `SWR-MG-28`, `SWR-MG-29`, new).**
## NP-SW-001
author: NeurOne Software Engineering
text: **§6.3: `SWR-MG-28` (new): the cloud credentials reach the host's store only sealed; a locked run holds them in memory and a host that cannot seal offers no cloud accounts and says why (new key `CLOUD_ACCOUNTS_NEED_KEYSTORE`). `SWR-MG-29` (new): a Google client secret is refused on Android and iOS before anything is sent (`OI-PRIV-14`).**
## NP-SW-CLOUDSTORE-001
author: NeurOne Software Engineering
text: **New §4c.17 (`OI-PRIV-14`): which hosts run unsealed and offered cloud accounts, the vault by storage state, the values compiled into the app and why each is public (Google's and Apple's pages read 2026-11-15), and what must be registered at each provider. §4c.1, §4c.7 and §4c.13 point to it.**
