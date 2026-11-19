## DHF
author: NeurOne Software Engineering
text: **{NP-SW-001 from → to}, {NP-SW-CLOUDSTORE-001 from → to}: the cloud credentials cannot be kept in the clear, and what ships in the app is treated as public (`OI-PRIV-14`, `SWR-MG-19` parts l and m).** The vault is built only over storage that seals its key or holds it in memory; a run with neither has no cloud sign-in and says so.
## NP-SW-001
author: NeurOne Software Engineering
text: **§6.3: `SWR-MG-19` gains parts (l) and (m) (`OI-PRIV-14`, privacy review of `common/`, Findings 37 and 38).** (l) `CredentialStore` is built only from the run's `ProtectedStorage`: sealed at rest when the storage seals its key, in memory for a locked run, and absent otherwise, where the app has no cloud sign-in, Settings says so and a credential document left in the clear is removed. (m) The value Google issues with a Desktop app client is public: it is sent only by a loopback-redirect client, the build refuses it beside any other redirect, and it is never a confidential client's secret. Tests: `CredentialStoreTests`, `CloudAuthWiringTest`.
## NP-SW-CLOUDSTORE-001
author: NeurOne Software Engineering
text: **§4c.1, §4c.7 and new §4c.13.1: where the cloud credentials may be kept, by storage state, and what the build compiles in (`OI-PRIV-14`).** §4c.7 names the hosts that can run unsealed with a sign-in wired and gives the vault's behaviour in each state. §4c.13.1 records that every build value is public, that Google's Desktop app client value is never a confidential client's secret and is refused by the build beside a non-loopback redirect, that the CloudKit API token is public by design and a server-to-server key is never set, and the six registrations the owner must make. No identifier is invented.
