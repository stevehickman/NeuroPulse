date: 2026-11-16
## DHF
author: NeurOne Software Engineering
text: **{NP-APP-TELEMETRY-001 from → to}, {NP-SW-001 from → to}, {NP-ANALYTICS-001 from → to}: analytics event names, screen values and error codes are literal lists in the shared core (`OI-PRIV-05` points 1 to 4); the relay question is asked, not decided.**
## NP-APP-TELEMETRY-001
author: NeurOne Software Engineering
text: **§3: event names are a literal list (new §3.3); `screen_name` loses the session and consent screens (§3.1); `error_code` is a literal list of app failures, each above 255, never a helmet fault code (new §3.4); `engagement_tier` is ruled to stay (§3.2); §7's terms are checked against the list by a test (`OI-PRIV-05`).**
## NP-SW-001
author: NeurOne Software Engineering
text: **§5.3.1 `REQ-SW03-07`: an event's name must be one of the listed names, no screen value records a session or a consent, and an error code is a listed app failure (`OI-PRIV-05`).**
## NP-ANALYTICS-001
author: NeurOne Software Engineering
text: **Permitted events table follows `NP-APP-TELEMETRY-001` §3.3: `consent_granted` and `consent_revoked` removed, `screen_viewed` and `error_reported` added; §8.1 marked out of date (`step_name` and `feature_name` were never permitted) (`OI-PRIV-05`).**
