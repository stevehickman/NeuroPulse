# OI-PRIV-05 (5) — do analytics events go through a relay that hides the network address?

**Status:** ANSWERED 2026-11-19 by the principal: **option A, with option B as the interim floor.** Recorded in `NP-APP-TELEMETRY-001` §4.1 and `docs/status/completed-decisions.md`. Two sub-questions of §4 are not answered (sharing the fleet relay; whether the first `track` call may ship before A is built). **One statement in §1 was wrong and is corrected in place below.** Originally: DRAFT, not sent. **From:** Software (Claude Code session, 2026-11-16). **To:** the principal, with the Privacy Lead (interim: the principal).
**GitHub issue:** [#903](https://github.com/stevehickman/NeuroPulse/issues/903)

**What is asked.** Pick one of options A to D below for how a research analytics event reaches the analytics host. Nothing here is a decision, nothing is built, and nothing in the repository may act as if an option has been chosen. No event is emitted by any app yet (`track` has no caller outside `ResearchAnalyticsGate`), so the choice can be made before the first one ships. `OI-PRIV-05` stays open on this point until it is.

## 1. What is known

- **The path today.** `PostHogAnalyticsBackend` (`common/core/.../analytics/PostHogAnalyticsBackend.kt`) posts each event straight to `https://<analytics host>/capture/`. The body carries the event name, the checked properties, and the installation's `distinct_id`: 16 random bytes kept in the app's store, the same for every event until research consent is withdrawn.
- **Who the host is.** A PostHog that NeurOne runs itself (`infra/posthog/`, `docs/np_analytics_001.md`), behind Caddy. "The vendor" in `NP-APP-TELEMETRY-001` is, for the address, NeurOne.
- **What the host sees.** The connection's network address beside the `distinct_id`, at the moment of each event. The app sets `$geoip_disable` and the project setting `anonymize_ips` is scripted, but neither is verified on a real instance (`OI-ANALYTICS-02`), and both are promises by the server about what it stores, not limits on what it receives. **Correction, 2026-11-19: the next sentence was wrong.** ~~`infra/posthog/caddy/Caddyfile` strips `X-Forwarded-For` and `X-Real-IP` for `/np/erase` only; the capture path passes them to PostHog~~. The Caddyfile already removed both headers on the capture path as well, and `ci/test_posthog_infra.py` P6 already checked it; the session had read only the first part of the file. What was true: Caddy's logs were not addressed. Its access log and its default logger wrote the client's address for the entries they kept (level WARN and above). That is the part of option B that had to be built.
- **The rule it meets.** `NP-APP-TELEMETRY-001` §4 prohibits the address: "Vendor must not log client IP address associated with any event". Today that is kept by server settings alone.
- **What the fleet path does.** The SHDR upload is Oblivious HTTP (RFC 9458; `common/npps-core/src/ohttp.rs`, `FleetOhttp.kt`, `NP-INFRA-001` §3.3b, `REQ-SHDR-07`): the record is sealed to a gateway's key and sent through a relay, so the party that reads the record never has the address. That protection holds only while the relay and the gateway are run by different parties and the relay keeps no log; a NeurOne relay beside a NeurOne gateway protects against a breach of one, not against NeurOne (`NP-INFRA-001` §3.3b).
- **What is at stake.** A network address is often enough to name a household, and sometimes a person or an employer. Joined to a stable `distinct_id`, it turns the pseudonymous launch history of a neuromodulation app into a history tied to a place. After this change (`OI-PRIV-05` (1) to (3)) the events themselves no longer mark sessions, consents or physiological halts, so the address is the main remaining link from an event to a person.

## 2. What is assumed, and not checked

- That the Rust OHTTP code can be reused unchanged: `ohttpSeal` and `ohttpOpen` take any method, authority, path and body, so sealing a `POST /capture/` looks like a matter of configuration. Not tried.
- That a PostHog can sit behind an OHTTP gateway. The fleet gateway is NeurOne's own service; whether it can forward to PostHog's capture endpoint, or a second gateway is needed, is not designed.
- That a relay operator exists for the fleet path. `NP-INFRA-001` §3.3b says the two-party arrangement is "a deployment fact and a contract"; no contract is recorded in the repository.
- Cost and latency per event are not estimated. Events are few (launches, screens, failures), so volume is likely small.

## 3. The options

| | Option | What the analytics host learns | Cost | What stays open |
|---|---|---|---|---|
| **A** | **Send analytics through the same kind of OHTTP relay as SHDR.** The app seals each event to a gateway key; a relay run by another party forwards it. | The event and the `distinct_id`. Never the address. | App: a sealed path in `PostHogAnalyticsBackend` (the cryptography exists, `CLAUDE.md` §22). Server: a gateway in front of capture, and the erasure request (`/np/erase`) should take the same path. A relay contract with a second party. A consent-screen sentence like the one `NP-INFRA-001` §3.3a requires for SHDR. | Whether one relay operator and one gateway may serve both SHDR and analytics. They are two consent subjects (`CLAUDE.md` §6.0) and the two gates must stay independent; a shared gateway would see both streams arrive from the same relay, and must not be able to pair a device token with a `distinct_id` by timing. This needs its own answer before A is built. |
| **B** | **Keep the direct path, and strip the address at NeurOne's edge.** Caddy drops the forwarding headers on `/capture/` as it does on `/np/erase` (it already did; see the correction in §1), keeps no address in a log, and CI checks both. | The address reaches NeurOne's edge and is discarded before PostHog. | Small: a Caddyfile change and a check. No app change. | It is a promise about NeurOne's own server, not a property the person can rely on: a misconfiguration, a breach of the edge, or a legal demand served on NeurOne reaches the address. It is the weaker of the two protections the fleet path was moved off. |
| **C** | **Keep the direct path as it is**, relying on `anonymize_ips` and `$geoip_disable`, once `OI-ANALYTICS-02` verifies them. | The address reaches PostHog, which is configured not to store it. | None beyond `OI-ANALYTICS-02`. | Everything in B, plus the address is inside the analytics application itself. `NP-APP-TELEMETRY-001` §4 is then kept by one unverified setting. |
| **D** | **Stop holding events under a stable ID** (a fresh ID per launch, or none), with or without a relay. | Events that cannot be joined into a history, so an address joined to one event says little. | Loses retention and funnel analysis, which is most of what `engagement_tier` and `onboarding_step` are for. The erasure queue (`REQ-SW03-19`) would have nothing to erase by. Reopens the `engagement_tier` ruling (`NP-APP-TELEMETRY-001` §3.2), because the tier would then be the only thing joining launches. | Whether the product questions analytics exists to answer survive it. |

A and B are not exclusive: B is cheap and can ship first as a floor, with A to follow. C is the state today. D changes what analytics is for and can be combined with A.

**The session's view, for what it is worth and not a ruling:** A, with B done at once as an interim floor. The reasoning is consistency: the project already decided, for the device-linked SHDR record, that a server setting is not enough to keep an address away from a stable identifier, and the analytics `distinct_id` is closer to a person than a device token is. The open question under A (one relay for two consent subjects) should be answered before it is built.

## 4. What to send back

- The option chosen (A, B, C, D, or a combination), and for A whether analytics may share the fleet path's relay operator and gateway.
- A short rationale (`CLAUDE.md` §18 wants a derivation), and whether the first `track` call may ship before the chosen option is built.
- Conditions that would reopen it (for example a change of analytics host to a third party, or a second identifier added to an event).
- Name, role and date.

The answer is recorded in `NP-APP-TELEMETRY-001` §4 and `docs/np_analytics_001.md`, and logged in `docs/status/completed-decisions.md`; `OI-PRIV-05` closes on it.
