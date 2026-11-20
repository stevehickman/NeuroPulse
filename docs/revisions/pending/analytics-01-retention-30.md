date: 2026-11-20
## DHF
author: NeurOne Software Engineering
text: **NP-ANALYTICS-001 and NP-SW-001 revised, and new document NP-MAN-OPS-001 (the operations manual: PostHog configuration, maintenance and the real-stack check): analytics event retention is 30 days, configurable, enforced by a ClickHouse TTL (`OI-ANALYTICS-01` (3)).**
## NP-ANALYTICS-001
author: NeurOne Software Engineering
text: **§3.4 and §7.1: the principal set event retention to 30 days on 2026-11-20 (`NP-APP-TELEMETRY-001` §5 promises deletion of a withdrawn person's ID within 30 days). It is `EVENT_RETENTION_DAYS`, applied as a ClickHouse TTL by `apply_event_retention.py` and checked with `--check`; the policy is cited beside the value, and a value above 30 is flagged as breaking it. Not run against a real stack.**
## NP-SW-001
author: NeurOne Software Engineering
text: **§5.3.1: new `REQ-SW03-34`, the event-retention script and its check against a fake ClickHouse.**
