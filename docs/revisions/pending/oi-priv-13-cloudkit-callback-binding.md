date: 2026-11-18
## DHF
author: NeurOne Software Engineering
text: **{NP-SW-001 from → to}, {NP-SW-CLOUDSTORE-001 from → to}: a CloudKit sign-in runs only where its callback is bound to the request, `state` is checked before `error`, and a forged request no longer ends a sign-in (`OI-PRIV-13`, closed; `OI-PRIV-18` raised).**
## NP-SW-001
author: NeurOne Software Engineering
text: **§6.3 `SWR-MG-19`: (a) `state` is checked before anything else in the redirect and the provider's error text is never quoted; (f), (h) a launcher does not take a redirect that lacks this request's `state`; (n), new: a CloudKit sign-in needs a confined browser session or a verified per-request nonce, and is not offered elsewhere (`OI-PRIV-13`).**
## NP-SW-CLOUDSTORE-001
author: NeurOne Software Engineering
text: **§4c.17, new: CloudKit's callback carries the token with no `state` or PKCE, so it is bound to its request or CloudKit is not offered; what was read at Apple and what was not run. §4c.3 corrected: it said `state` and the PKCE verifier decide every answer, which was untrue for CloudKit. §4c.1, §4c.4, §4c.5, §4c.13 follow (`OI-PRIV-13`).**
