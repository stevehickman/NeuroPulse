date: 2026-11-19
## DHF
author: NeurOne Software Engineering
text: **{NP-APP-TELEMETRY-001 from → to}, {NP-ANALYTICS-001 from → to}: the principal's decision on the analytics network address (`OI-PRIV-05` (5)): an Oblivious HTTP relay, decided and not built, with an address-free edge as the floor until then; the proxy's logs no longer keep the address.**
## NP-APP-TELEMETRY-001
author: NeurOne Software Engineering
text: **New §4.1: analytics events are to go through an Oblivious HTTP relay (decided, not built); until then the edge hands the address to nothing and writes it in no log (`OI-PRIV-05` (5), the principal, 2026-11-19).**
## NP-ANALYTICS-001
author: NeurOne Software Engineering
text: **§3.4: the proxy's access log and default logger delete the client's address, port and forwarding headers, checked by `ci/test_posthog_infra.py` P8 and P9; the relay of `NP-APP-TELEMETRY-001` §4.1 is decided and not built.**
