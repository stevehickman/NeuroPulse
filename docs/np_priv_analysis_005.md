# Privacy Analysis — CI Privacy Gates and the Infrastructure They Check

**Project:** NeurOne  
**Document:** NP-PRIV-ANALYSIS-005  
**Revision:** 1
**Date:** 2026-11-21  
**Status:** DRAFT  
**Effective Date:** —  
**Author:** NeurOne Software Engineering  
**Approved By:** — (new document)  
**References:** NP-FW-EMMC-002, NP-INFRA-001, NP-ANALYTICS-001, NP-APP-TELEMETRY-001, NP-SW-CI-001, NP-PRIV-AUDIT-001, NP-PRIV-ANALYSIS-002, NP-PRIV-ANALYSIS-003  
**Related Issues:** GitHub #93, #443, #446, #725, #888, #903  
**Gate:** —  
**IEC 62304 Class:** —  
**Jurisdiction Scope:** All, as NP-PRIV-ANALYSIS-002 and -003 (US federal HIPAA, HBNR, FTC Act §5; US state BIPA IL, MHMD WA, CCPA/CPRA CA; EU/EEA GDPR; UK GDPR + DPA 2018; Canada PIPEDA)  
**Change Summary:** First issue. Privacy review of `ci/` and of the reference configurations and schemas its gates check. Eight findings, raised as `OI-PRIV-51` to `OI-PRIV-58`.

---

## 1. Scope and method

**Target.** Everything under `ci/`, and the files its privacy gates read:

| Gate | What it checks | Files it reads |
|---|---|---|
| `ci/test_shdr_schema.py`, `ci/test_shdr_schema_selftest.py`, `ci/test_shdr_char_gate_selftest.py`, `ci/run_shdr_constraint_tests.sh` | SHDR fleet schema (static names and types; live constraints) | `ci/shdr/shdr_fleet_schema.sql`, `ci/shdr/shdr_schema_constraint_tests.sql` |
| `ci/test_warranty_nojoin.py` | No join between the warranty registry and SHDR | `ci/warranty/warranty_registration_schema.sql`, the SHDR schema |
| `ci/test_fleet_ingest_config.py` | Fleet ingest edge, `REQ-SHDR-06` | `infra/fleet/Caddyfile` |
| `ci/test_fleet_relay_config.py` | Oblivious HTTP relay, `REQ-SHDR-07` (g) | `infra/fleet/relay/Caddyfile` |
| `ci/test_posthog_infra.py` | PostHog stack P1 to P9 | `infra/posthog/docker-compose.yml`, `infra/posthog/caddy/Caddyfile`, `infra/posthog/.env.example` |
| `ci/test_posthog_erase_relay.py`, `ci/test_posthog_event_retention.py`, `ci/test_posthog_project_settings.py` | Erasure relay, event TTL, project settings, against fakes | `infra/posthog/erase_relay.py`, `apply_event_retention.py`, `apply_project_settings.py` |

The non-privacy files in `ci/` (`host-test-*.txt`, `required-checks.txt`, `main-ruleset.json`, `req-trace-baseline.txt`) were read for what they say about enforcement only.

**Method.** One reading pass by one session on 2026-11-21. Every finding below was **checked**: the cited lines were read by that session. F2 was also **run**: the SHDR schema was loaded into a throwaway PostgreSQL 16 cluster, and a day-precise date was inserted into `devices.last_seen_month` and `fault_log.ingest_month` (both accepted) and into `module_inventory.last_seen_month` (rejected by `ck_module_inventory_month`). The gates were run as they stand and all pass: F1 to F5, R1 to R5, `OI-EMMC2-07`, `OI-EMMC2-06`, P1 to P8 and the three fake-store suites. P9 was skipped because no Docker daemon answered in the session. The findings are therefore about what the gates do not look at, not about a gate that is red.

**What this is not.** Nothing deployed was reviewed. The fleet edge, ingest service, gateway, relay and PostHog stack in the repository are reference configurations (`OI-FLEET-01` (e), `OI-ANALYTICS-02`).

## 2. Summary

**0 Critical, 0 High, 6 Medium, 2 Low.** Privacy failures predominate over security failures. One cause runs through most of them: **each gate tests a deny-list of names it already knows**, so a header, a column type or a table it has not heard of passes. The schema's own header says this about the SHDR gate ("Read the PASS for what it is"). This review found where that matters today.

The highest-leverage change is F1: delete every request header from both Caddy edges' logs and strip every client-address header before forwarding, instead of naming one or two. F2 and F3 cost almost nothing now and real migration later: the SHDR schema can be re-laid out at no cost only until the first device ships.

## 3. Findings

### F1. MEDIUM: the fleet edge and the PostHog edge remove only some client-address headers (`OI-PRIV-51`)

- **Where:** `infra/fleet/Caddyfile` (both `format filter` blocks; `reverse_proxy`), checked by `ci/test_fleet_ingest_config.py` F2 (`MUST_DELETE`) and F3. `infra/posthog/caddy/Caddyfile`, checked by `ci/test_posthog_infra.py` P6 and P8 (`MUST_DELETE`).
- **Category:** Privacy failure.
- **Issue:** Caddy writes every request header into the access log and passes every request header to the upstream except those it sets itself. The fleet edge's log filters delete `X-Forwarded-For` and `X-Np-Device-Token`, and keep `X-Real-Ip`, `Forwarded` (RFC 7239), `True-Client-IP`, `CF-Connecting-IP`, `X-Client-IP` and `Via`. Neither edge removes `Forwarded` upstream. A carrier or corporate proxy between the phone and the edge adds these headers, so the ingest service can receive the client's address in the same request as the device token: the pairing F3's own comment says must not happen ("it would write it beside the token"). `REQ-SHDR-06` (a) requires the edge to drop "any forwarded address", so the reference edge does not meet its requirement, and the gate checks the same short list. The relay (`infra/fleet/relay/Caddyfile`) already strips `Forwarded`, so the three configurations disagree. The PostHog edge has the same gap upstream (`Forwarded`) and in its logs (everything but the two names).
- **Reference:** PII in Logs anti-pattern; LINDDUN Linkability; GDPR Art. 5(1)(c); `CLAUDE.md` §5.1 (SHDR is linked to the device, never to a place or a person).
- **Remediation:** In every log filter, delete the whole header map (`request>headers delete`), or keep only an allow-list (`Content-Type`, `Content-Length`). Upstream, add `header_up -Forwarded`, `-Via`, `-True-Client-IP`, `-CF-Connecting-IP`, `-X-Client-IP` beside the two already removed. Change F2 and P8 to require the header map deleted, and F3 and P6 to require the full list.
- **Existing coverage:** New. `OI-PRIV-05` and GitHub #903 decide the analytics relay; `OI-FLEET-01` (e) says the deployed edge is not in the repository. Neither covers these headers in the reference configurations.

### F2. MEDIUM: 12 of the 21 SHDR month anchors are month-granular by DEFAULT only (`OI-PRIV-52`)

- **Where:** `ci/shdr/shdr_fleet_schema.sql`: `devices.last_seen_month`, and `ingest_month` in `firmware_history`, `ota_events`, `power_telemetry`, `accessory_auth_log`, `calibration_history`, `eeg_impedance_trend`, `shdr_accel_records`, `shdr_accel_characterisation`, `mode_f_telemetry`, `fault_log` and `storage_health`. Checked by `ci/test_shdr_schema.py` (`PROHIBITED_TIME_TYPE_PATTERNS`).
- **Category:** Privacy failure.
- **Issue:** A DEFAULT applies only when an insert leaves the column out. An ingest service that writes `CURRENT_DATE` stores the day of each docking, keyed to `warranty_token`: the docking-time side channel that TIME-01 and schema Rev B exist to remove. Shown live (§1): `devices` and `fault_log` accept `2026-08-11`; `module_inventory`, which has the CHECK, refuses it. The other nine anchors carry a `DATE_TRUNC` CHECK; the constraint tests probe one of them. The static gate rejects only the `TIMESTAMP`, `TIMESTAMPTZ` and `TIME` types, so a `BIGINT uploaded_epoch` or an `INTERVAL` column passes it.
- **Reference:** GDPR Art. 25; LINDDUN Detectability; `CLAUDE.md` §5.1 ("no timestamps in SHDR", as the schema header cites it); `NP-FW-EMMC-002` §G.
- **Remediation:** Add `CHECK (col = DATE_TRUNC('month', col)::date)` to the twelve. Add a static rule: every `DATE` column other than one named `*manufacture_date` carries that CHECK. Reject `INTERVAL`, and reject a column of any type whose name matches `_epoch`, `_ts`, `_at$`, `_time` or `unix`. Add one live reject case per table to `shdr_schema_constraint_tests.sql`. Proposed as blocking for the SHDR schema freeze (`OI-EMMC2-07`), because after the first device ships each CHECK needs a migration.
- **Existing coverage:** New. `shdr_schema_constraint_tests.sql` states the DEFAULT problem ("the DEFAULT only applies when the column is omitted") and tests `module_inventory` only.

### F3. MEDIUM: the fleet-wide row sequences are a clock and link the rows of one upload (`OI-PRIV-53`)

- **Where:** `id BIGSERIAL PRIMARY KEY` in 22 of the 23 SHDR tables.
- **Category:** Privacy failure.
- **Issue:** Each table's sequence numbers rows in insert order across the whole fleet. Within any month the ids order every device's dockings relative to each other, and the rows written by one docking get adjacent ids in every table, so firmware, fault and accelerometer rows can be grouped by session. Any outside time anchor turns the order back into roughly day-level time: ingest logs, WAL or backup file times, or `track_commit_timestamp`. The schema already carries per-device ordering (`session_at_install`, `session_index`, `gap_index`), so no function needs the global order.
- **Reference:** Weak / Predictable IDs anti-pattern; LINDDUN Linkability and Detectability.
- **Remediation:** Key rows by `gen_random_uuid()`, or by `(warranty_token, per-device counter)`. Keep `track_commit_timestamp` off on the fleet database. Have the ingest service batch rows and insert them in shuffled order. Add a static rule that no SHDR table has a `SERIAL` or `BIGSERIAL` column. Same freeze argument as F2.
- **Existing coverage:** New.

### F4. MEDIUM: the warranty registry schema has no minimisation, no retention and no gate on its content (`OI-PRIV-54`)

- **Where:** `ci/warranty/warranty_registration_schema.sql`.
- **Category:** Privacy failure.
- **Issue:**
  - `support_contact_ok BOOLEAN NOT NULL DEFAULT TRUE` opts the owner in to contact unless they act (`UC-199`).
  - `warranty_claims.claim_description TEXT` is free text beside `registered_name`, `registered_email` and `registered_address`. Claim text about a stimulation device will collect health statements (a burn, a symptom, the condition the person uses it for), which are special-category data under GDPR Art. 9, held by NeurOne with the person's identity.
  - `created_at` and `updated_at` are `TIMESTAMPTZ`, to the second, where `registration_date DATE` already exists.
  - No retention, erasure or archive rule exists for either table. On transfer the SHDR record is archived, not deleted (`NP-FW-EMMC-002` §A); nothing says how long the previous owner's name and address stay.
  - `ci/test_warranty_nojoin.py` checks only that the two schemas do not name each other's tables. Nothing checks the warranty schema's own columns.
- **Reference:** Opt-out Default; Permanent Storage; GDPR Art. 5(1)(c) and (e), Art. 9, Art. 17; CCPA §1798.105.
- **Remediation:** Default `support_contact_ok` to `FALSE`. Replace free text with an enumerated `claim_type` and, if notes are needed, a separate notes table with its own access role and a retention column. Use `DATE` for the two timestamps. Add a retention rule per table and a delete-by-token path, decided by the principal. Extend the no-join gate, or add a warranty gate, that holds these.
- **Existing coverage:** New. GitHub #565 and #888 cover warranty consent, not the registry's columns.

### F5. MEDIUM: the raw warranty token is in both databases, so the no-join rule is enforced by IAM and policy alone (`OI-PRIV-55`)

- **Where:** `warranty_registrations.warranty_token`; `devices.warranty_token` and every SHDR table that references it.
- **Category:** Privacy failure (misuse by an authorised party).
- **Issue:** Anyone who can read both databases joins SHDR to a name and address in one query. The barriers are the separate cloud projects with no shared IAM role and a static check of the DDL text. This is the design (`NP-FW-EMMC-002` §A.3: "joining them is prohibited in production"), so this is a hardening question for the principal, not a defect.
- **Reference:** God View anti-pattern; Pseudonymous Identity pattern; Audit Logging on Privileged Reads.
- **Remediation (for decision):** SHDR stores `HMAC(k, warranty_token)` instead of the token. `k` is held by a separate lookup service that answers warranty-support lookups one token at a time, with an audit log and break-glass review. A copy of both databases then cannot be joined without the service. Cost: a service to run, and the device-side upload still sends the raw token over the encrypted path, so the edge must hash it on receipt.
- **Existing coverage:** Accepted design (`NP-FW-EMMC-002` §A.3, `OI-EMMC2-06`). The hardening is new.

### F6. MEDIUM: the analytics erasure and the 30-day TTL cover one table (`OI-PRIV-56`)

- **Where:** `infra/posthog/erase_relay.py` and `infra/posthog/apply_event_retention.py` (`DEFAULT_TABLES = ("posthog.sharded_events",)` in both). `ci/test_posthog_erase_relay.py` asserts that default.
- **Category:** Privacy failure.
- **Issue:** A self-hosted PostHog can hold a `distinct_id` outside the events table: session tables, a dead-letter table, ingestion warnings, the Kafka buffer (24 hours in this compose file) and backups. Which of these exist depends on `POSTHOG_VERSION` and was not checked. An erasure or TTL that reaches only the events table leaves the ID elsewhere (`UC-200`).
- **Reference:** Backup Immortality anti-pattern; GDPR Art. 17; `NP-APP-TELEMETRY-001` §5.
- **Remediation:** When `OI-ANALYTICS-01` (2) runs against a real stack, list every table with a `distinct_id` column (`SELECT database, table FROM system.columns WHERE name = 'distinct_id'`) and every place outside ClickHouse that keeps the ID. Set `ERASE_TABLES` and the TTL tables from that list, record it in `NP-ANALYTICS-001` §7.6, and make the two suites assert it instead of the single-table default.
- **Existing coverage:** Partial. `OI-ANALYTICS-01` (2) and GitHub #725 track that nothing has run against a real stack, including whether `system.mutations` keeps erased IDs. That a ClickHouse mutation is asynchronous and the relay answers 200 on acceptance is in `NP-ANALYTICS-001`. The other tables are new.

### F7. LOW: the PostHog proxy logs have no retention bound (`OI-PRIV-57`)

- **Where:** `infra/posthog/caddy/Caddyfile` (`output stdout` in both logs); `infra/posthog/docker-compose.yml` (no `logging:` on any service).
- **Category:** Privacy failure.
- **Issue:** Docker's default `json-file` driver keeps container output with no size or age limit unless the host's daemon sets one. The fleet edge rolls its logs at 72 hours and F4 checks it; P8 has no retention check. The address is already removed (P8), so what remains is method, URI, user-agent, status and time.
- **Reference:** Permanent Storage anti-pattern; GDPR Art. 5(1)(e).
- **Remediation:** Add `logging: {driver: local, options: {max-size: ..., max-file: ...}}` to the `caddy` service (and to `erase-relay`), with figures entered in the unjustified-choices register like `UC-095`, and a P10 check.
- **Existing coverage:** New. The fleet figure is `UC-095`, under `OI-FLEET-01` (f).

### F8. LOW: the no-join gate cannot see a join on combinations of ordinary fields (`OI-PRIV-58`)

- **Where:** `warranty_registrations.serial_number` and `device_model`; `devices.device_model`, `hw_revision`, `factory_id`, `manufacture_date`.
- **Category:** Privacy failure.
- **Issue:** If a serial number encodes the factory, line or date of manufacture, the warranty row narrows the SHDR candidates without any shared identifier. Whether it does depends on the serial-number format, which this review could not find in the document set. The gate matches column names, so it cannot see this class of join.
- **Reference:** Aggregation Creep and Re-identification via Join anti-patterns; LINDDUN Identifiability.
- **Remediation:** Define the serial format so that it carries no manufacture data, or record the limit beside the module-UID one in `OI-EMMC2-08` and coarsen `manufacture_date` in SHDR to a month.
- **Existing coverage:** Partial. The same class as `OI-EMMC2-08` (GitHub #443), which says neither gate can see a self-join on module UID. The serial number is new.

## 4. Comparison with issues already identified

| Finding | Open item | Existing coverage |
|---|---|---|
| F1 Client-address headers | `OI-PRIV-51` | New. Adjacent: `OI-PRIV-05` / #903, `OI-FLEET-01` (e) |
| F2 Month anchors by DEFAULT | `OI-PRIV-52` | New. The constraint tests know the problem for one table |
| F3 Row sequences | `OI-PRIV-53` | New |
| F4 Warranty schema | `OI-PRIV-54` | New. Adjacent: #565, #888 (consent, not columns) |
| F5 Raw token in both stores | `OI-PRIV-55` | Accepted design, `NP-FW-EMMC-002` §A.3; hardening new |
| F6 Erasure and TTL scope | `OI-PRIV-56` | Partial: `OI-ANALYTICS-01` (2), #725 |
| F7 PostHog log retention | `OI-PRIV-57` | New. Fleet counterpart `UC-095` |
| F8 Serial-number join | `OI-PRIV-58` | Partial: `OI-EMMC2-08`, #443 |

**Reached and already tracked, so not raised again:**

| What the review met | Where it is tracked |
|---|---|
| Module UIDs re-link pseudonymous device identities across a factory reset and across devices | `OI-EMMC2-08`, GitHub #443 |
| `shdr_accel_records` rows reconstruct the aggregates §G.3 bans | `OI-EMMC2-11` |
| `devices.session_count` may be consumer health data under WA MHMDA | NP-PRIV-AUDIT-001, GitHub #93 |
| The characterisation enrolment and its `char_consent_granted` rows after a warranty revoke | `OI-PRIV-04` (decided and built), `REQ-SHDR-09`, GitHub #888 |
| `fault_log.fault_type` carries the cervical VNS heart-rate cutoff | Locked ruling (safety-interlock log to SHDR), unconditional redaction of its timing; `scripts/check-redaction-shape.ts` |
| No privacy gate is a required status check; zero approvals are required to merge | `OI-SWCI-54`, GitHub #446 |
| The PostHog settings, TTL and erasure have run against fakes only | `OI-ANALYTICS-01`, `OI-ANALYTICS-02`, GitHub #725 |
| The erasure relay answers 200 when ClickHouse accepts the mutation, not when the rows are gone | `NP-ANALYTICS-001`, `OI-ANALYTICS-01` (2) |
| A retention above 30 days exits 0 with a warning | Decided by the principal 2026-11-20, `OI-ANALYTICS-01` (3) |
| The fleet log retention of 72 hours has no derivation | `UC-095`, `OI-FLEET-01` (f) |
| The relay and gateway must be run by different parties, which no file can check | `OI-FLEET-01` (a) |

## 5. What looks good

- **Every gate proves it can fail.** Each one breaks a copy of the real file in one way and expects red, rather than being run only on a passing tree (`NP-CONV-001` §4.0.1a in practice).
- **The relay gate is the strongest of the set**: no log of any kind, one route, a body cap, five headers stripped, parse-time placeholders. It is the model for F1.
- **The erasure relay** binds the ID as a query parameter, keeps no log of IDs or addresses, holds only numeric counters, and sends credentials in headers, and its suite proves each.
- **SHDR carries no wall-clock column**, bans UHDR key material by name, and its live harness proves each constraint both rejects and accepts.
- **The schemas are honest about their limits.** The SHDR header says a pass does not mean SHDR cannot reveal health information, and the cross-device consequence of module UIDs is written down beside the table.

## 6. What could not be reviewed

- The deployed fleet edge, ingest service, gateway and relay, and the deployed PostHog stack.
- The Postgres and ClickHouse server settings in production (`track_commit_timestamp`, backups, WAL archiving).
- Which tables the pinned `POSTHOG_VERSION` creates (F6).
- The serial-number format (F8).
- `ci/test_posthog_infra.py` P9 (`caddy adapt` in the compose file's Caddy image): no Docker daemon answered in the session, so it was skipped. CI runs it with `--require-docker`.

## 7. Recommended next steps

1. **F1**, about an hour: delete the header map in both edges' logs, strip the full client-address set upstream, extend F2, F3, P6 and P8.
2. **F2**, about two hours: the twelve CHECKs, the static rule, the integer and `INTERVAL` patterns, one live reject case per table.
3. **F3**, about an hour, while the schema is still free to re-lay out: random keys or per-device keys, and the static rule.
4. **F4**: the contact default, enumerated claim types, `DATE` instead of `TIMESTAMPTZ` and a gate, about half a day; the retention periods are the principal's decision.
5. **F6**, with `OI-ANALYTICS-01` (2): list every place the analytics ID lives and widen the erasure and TTL to it.

Beyond these five: F5 (a decision for the principal), F7 and F8.

## 8. Open items raised

> **Renumbered 2026-11-21.** These items were first filed as `OI-PRIV-21` to `OI-PRIV-28`. NP-PRIV-ANALYSIS-004 (the firmware review, PR #972) merged first with those numbers and GitHub issues #949–#956, so this review's items became `OI-PRIV-51` to `OI-PRIV-58` in the same order (F1 = `OI-PRIV-51` … F8 = `OI-PRIV-58`). Nothing else changed.

The owning entries are in `docs/status/pending-decisions.md`, under "Privacy review of `ci/`, 2026-11-21". Choices this review found with no derivation are `UC-199` and `UC-200` in `docs/status/unjustified-choices.md` (`CLAUDE.md` §19).

| Open item | Finding | Closes when |
|---|---|---|
| `OI-PRIV-51` | F1 | Both edges delete all request headers from every log and strip the full client-address set upstream, and the gates require it |
| `OI-PRIV-52` | F2 | Every SHDR month anchor has its CHECK, the static gate requires it and rejects integer and interval clocks, and each is probed live |
| `OI-PRIV-53` | F3 | No SHDR table has a fleet-wide sequence, and the gate forbids one |
| `OI-PRIV-54` | F4 | The warranty schema defaults contact off, holds no free-text claim beside identity, has decided retention and erasure, and a gate checks it |
| `OI-PRIV-55` | F5 | The principal decides whether SHDR holds a keyed hash of the token, and the decision is recorded in `NP-FW-EMMC-002` §A |
| `OI-PRIV-56` | F6 | The places the analytics ID lives are listed from a real stack, and erasure and TTL cover them |
| `OI-PRIV-57` | F7 | The PostHog proxy's logs have a size and age bound and a check |
| `OI-PRIV-58` | F8 | The serial format is shown to carry no manufacture data, or the join is recorded and `manufacture_date` coarsened |
