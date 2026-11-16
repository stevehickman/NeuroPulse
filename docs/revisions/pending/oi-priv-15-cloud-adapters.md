date: 2026-11-15
## DHF
author: NeurOne Software Engineering
text: **{NP-SW-001 from → to}, {NP-SW-CLOUDSTORE-001 from → to}: the cloud adapters accept and send no more than they need (`OI-PRIV-15`): a stricter Azure signature check, `https` on every address a provider hands back, no Dropbox identity scope, revocation on re-sign-in and erase, no request address in a log or exception, and live tests that refuse a host that is not loopback.**
## NP-SW-001
author: NeurOne Software Engineering
text: **§6.3: new `SWR-MG-28` (a credential goes to its provider over `https` and is written nowhere); `SWR-MG-26` (a) requires `sr=c`, `spr=https`, permissions within `rcwdl` and a readable expiry; `SWR-MG-23` (a) drops `account_info.read` and the free-space call; `SWR-MG-19` (e) revokes a replaced credential and every credential before an erase; `SWR-MG-20` (c) says what stays at the provider (`OI-PRIV-15`).**
## NP-SW-CLOUDSTORE-001
author: NeurOne Software Engineering
text: **New §4c.17: the six repairs of `OI-PRIV-15`, what each was read against on 2026-11-15, what stays and why, and what was not run; §4c.1, §4c.11, §4c.13 and §4c.14 brought into line.**
