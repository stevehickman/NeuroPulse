## DHF
author: NeurOne Software Engineering
text: **{NP-APP-PRESENCE-001 from → to}, {NP-SW-001 from → to}: the presence gate no longer undoes itself (`OI-PRIV-62`): its own records are host-plain, a value sealed under the gated key is never opened ungated, the background timeout uses a monotonic clock, and key copies are wiped (new `REQ-PRES-10`; `SWR-MG-16` (a) names the exception).**
## NP-APP-PRESENCE-001
author: NeurOne Software Engineering
text: **§2, §3, §7: new `REQ-PRES-10` (`OI-PRIV-62`, `NP-PRIV-ANALYSIS-007` H1, H2, M9, M10). (a) The gate's own records (`np.presence.enabled`, `np.presence.passphrase-wrap`) are `hostPlain`: never sealed, in neither tier, as §3 already called the wrap record. (b) On a gate host a protected value sealed under a key other than the device key means the gate is on, and an ungated store over it holds the run in memory. (c) The background timeout is measured on a monotonic clock with the wall clock as a second witness, and a negative elapsed time is expired. (d) Key copies the stores load are wiped, and the re-seal runs once per unlock. §3 records that held writes are memory-only (`OI-PRIV-62` (5), a decision brief).**
## NP-SW-001
author: NeurOne Software Engineering
text: **§6.3.7 `SWR-MG-16` (a): the sealed set is every listed key except `hostPlain`, the presence gate's own two records (`REQ-PRES-10` (a), `OI-PRIV-62`).**
