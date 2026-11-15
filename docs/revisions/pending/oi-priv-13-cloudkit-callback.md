date: 2026-11-15
## DHF
author: NeurOne Software Engineering
text: **{NP-SW-001 from → to}, {NP-SW-CLOUDSTORE-001 from → to}: `OI-PRIV-13`: a CloudKit sign-in is withheld on every host whose launcher does not confine the callback; `state` is checked before a redirect's error is read; the loopback listener ignores a request without the waiting `state`.**
## NP-SW-001
author: NeurOne Software Engineering
text: **§6.3 `SWR-MG-19`: (a) `state` is checked before anything else in the redirect and a refusal is a fixed kind, never the redirect's text; (h) the loopback listener answers only a request carrying the waiting sign-in's `state`; new (l): a CloudKit sign-in is offered only through a launcher that confines the callback and reads "not set up" elsewhere (`OI-PRIV-13`).**
## NP-SW-CLOUDSTORE-001
author: NeurOne Software Engineering
text: **New §4c.17 and corrections to §4c.1, §4c.3, §4c.4, §4c.5, §4c.13 and §7.2 (`OI-PRIV-13`): CloudKit's sign-in callback carries no `state` and no PKCE, so the statement that they decide a forged answer was untrue for CloudKit; no nonce is added because Apple's documentation does not show one would be returned; the sign-in is withheld on Android, a computer and the web. `OI-CLOUDAUTH-01` holds what is unverified.**
