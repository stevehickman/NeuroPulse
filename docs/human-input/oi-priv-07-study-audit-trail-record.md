# Question for the principal: OI-PRIV-07, which record holds the study participation audit trail

**Status:** ANSWERED 2026-11-20: Q1 A, Q2 A, Q3 A (the principal; recommendation accepted). Originally DRAFT, not sent. Not a controlled document; see `docs/human-input/README.md`.
**GitHub issue:** [#887](https://github.com/stevehickman/NeuroPulse/issues/887)
**From:** NeurOne Software Engineering (`OI-PRIV-07` in `docs/status/pending-decisions.md`; privacy review of `common/`, Finding 9, GitHub #885)
**To:** the principal
**Response needed:** an option for Q1, and for Q2 and Q3 if they apply; a short rationale; anything that would reopen it; your name, role and the date.

## 1. The question

Two locked sections disagree about one record.

- **`CLAUDE.md` §5.3** (and `docs/reference/data-architecture-detail.md` §5.3, "Audit trail (SHDR)"): for each research extract, the study ID, the study descriptor hash, the transmission timestamp and the byte count *"are logged in SHDR"*.
- **`CLAUDE.md` §5.1:** a record that tells us something about the person is UHDR, and *"when in doubt → UHDR"*.

SHDR (System Health Data Record) is linked to the device ID and the warranty token and is uploaded to the fleet database. A study ID resolves to a study in one of nine research areas (`ResearchCategory`: Alzheimer's and dementia, depression, PTSD, traumatic brain injury, sleep disorders, attention and ADHD, Parkinson's disease, healthy ageing, visual health). In SHDR, the audit trail tells the fleet database that the wearer of device X is in study Y, and on which day they left it.

Plain form: **is the study audit trail a fact about the device or a fact about the person?**

## 2. What is known

### 2.1 Nothing sends or stores a study identifier in SHDR today

Each of these was searched on 2026-11-15 for `study`, `studyId`, `descriptor hash`, `research` and `extract`, and searched again by a second session on 2026-11-16 with the same result (`ci/test_shdr_schema.py` and `scripts/check-map2-shdr-boundary.ts` both pass, and neither names a study field).

| Place | Finding |
|-------|---------|
| `firmware/` | No study identifier anywhere. `firmware/anon/` holds only the scratch-encryption code and the pipeline step list. `firmware/shdr/` holds only the accelerometer record. No per-study record and no research SHDR event is implemented. |
| `ci/shdr/shdr_fleet_schema.sql` (23 tables) | No study, research, descriptor or extract column. |
| `infra/fleet/` (gateway and relay) | Forwards opaque blobs. No study field. |
| App uploaders (`common/core/.../analytics/FleetUploader.kt`, `app/ios/NeurOne/Data/SHDRUploader.swift`) | Neither reads `ConsentStore` or the participation record. They name `ConsentStore` only in comments that say they must not reference it (`CLAUDE.md` §6.0). |

### 2.2 The record exists only in the app, and it is already handled as UHDR

- `StudyParticipationRecord` (`common/core/.../models/ConsentModels.kt`) holds `studyId`, `descriptorHash`, `enrolledOnDay`, `transmittedAtDay`, `extractBytes` and `withdrawnAtDay`. **Updated 2026-11-20:** the `OI-PRIV-06` work (closed 2026-11-19) replaced the record's "audit trail — SHDR-class, not UHDR" comment with one that says the enrolment and withdrawal days are the person's decisions, are not in §5.3's list, and that whether any of the record may leave the device as SHDR is this item. `ConsentStore.kt` still says "SHDR-class" in two places.
- It is stored on the phone under `np.consent.study-participations`. That key is sealed at rest like every stored key (`docs/np_app_presence_001.md` §4), and it sits in the `consent` group of `common/npps/migration-keys.json`, so it travels with the person's consent state in a migration backup. **Only the label says SHDR. The handling is UHDR.**
- `ConsentStore` already keeps a second list, `studyWithdrawals` (`np.consent.study-withdrawals`), commented "UHDR-class and kept apart from `studyParticipations`, which is SHDR-class". The withdrawal day is therefore held twice, once under each label.
- The retiring SwiftUI app (`app/ios/NeurOne/Models/ConsentModels.swift`) has the same record with a full `Date` where the shared core stores a day. Its comment was changed the same way; `app/ios/NeurOne/Consent/ConsentStore.swift` still says "study audit trail written to SHDR" and "SHDR-class".
- **No record has ever described a real transmission.** No extractor exists. The record is written when an invitation is accepted, with the enrolment day and no transmission (`transmittedAtDay` null, 0 bytes) since `OI-PRIV-06` point 4 was fixed.

### 2.3 No gate would stop a study identifier entering SHDR

- `ci/test_shdr_schema.py` refuses names such as `user_id` and `patient_id`. No pattern matches `study_id` or `descriptor_hash`.
- `scripts/check-map2-shdr-boundary.ts` checks Map 2 and sync-boundary fields. It does not look for study identifiers.
- The same schema test **does** refuse `TIMESTAMP`, `TIMESTAMPTZ` and `TIME` columns. So §5.3's "transmission timestamp" could not be added to the SHDR schema as written, whatever is ruled here. Only a month-truncated `DATE` passes.

### 2.4 Documents that specify device-side study data, none of it built

| Document | What it specifies |
|----------|-------------------|
| `NP-FW-ANON-001` §8.1 | A per-study budget record in the hub's Config partition: `study_id[32]`, `epsilon_spent`, `extract_count`. The document does not say whether that partition is UHDR or SHDR class. |
| `NP-FW-ANON-001` §8.2 | An `NP_ANON_CONSENT_WITHDRAWN` SHDR event, "no study content — flag only". Under the device-linked token it still says the wearer had consented to research and then withdrew. |
| `NP-FW-ANON-001` §5.3 | `anon_groups_suppressed: uint16` logged to SHDR. |
| `NP-SW-PORTAL-API-001` §4a | Restates the SHDR audit trail as "unchanged by this document". |
| `docs/status/completed-decisions.md` (research anonymization entry) | The original lock: "Audit trail of contributed studies in SHDR (user-readable, never shared with researchers)". |

### 2.5 A precedent in the per-field rulings

`data-architecture-detail.md` §5.1 already rules the anonymisation pipeline's `failed_step` **UHDR**, because a device-linked count of validation failures *"weakly signals"* that the wearer is a re-identification outlier. It allows SHDR at most a coarse `anonymization_failed` boolean. A study ID in a disease category is a stronger signal about the wearer than that one.

### 2.6 The research store holds the same link

`data-architecture-detail.md` §5.3 step 4: *"NeurOne servers store extract keyed to study ID and device ID only."* So NeurOne's research servers already hold (study, device) for every extract received, by design. This is a different database from the SHDR fleet database. It bears on what option (a) costs (§4) and it raises Q3.

**Assumed, not established:**
- That the "device ID" of step 4 is, or can be joined to, the device ID or warranty token that keys SHDR. No document says.
- That the audit trail is written by the app. `CLAUDE.md` §5.3 puts anonymisation in the app. `NP-FW-ANON-001` describes an engine in firmware. This brief does not settle which.
- That a study ID is not secret inside NeurOne, so anyone with fleet database access can resolve it to a study.

## 3. Questions

**Q1. Which record does the study audit trail belong to?**

- **A. UHDR. The whole record stays with the person.** Study ID, descriptor hash, enrolment day, transmission day, byte count and withdrawal day are UHDR, held in the app under the person's key. The person still inspects every study they contributed to, as §5.3 promises, from that record. SHDR gets nothing that names a study. Whether it gets a study-free total is Q2.
  - *Cost:* NeurOne has no device-side account of extracts in the fleet database (§4). §5.3 and its restatements change. `NP-FW-ANON-001` §8.1 and §8.2 need a class stated for the budget record and a decision on the withdrawal event.
- **B. SHDR, as §5.3 says.** The argument for it: a study ID is an administrative identifier NeurOne assigned. The record holds no measurement of anyone. It describes what the device did (sent N bytes under descriptor H). NeurOne already holds (study, device) in the research store (§2.6), so the log tells NeurOne nothing new. Study invitations are built from device ID and contact preferences with no UHDR, so being in a study is not a diagnosis.
  - *Cost:* the argument has to carry four points it does not carry today.
    1. §5.1's test asks what the record tells us, not whether NeurOne could learn it elsewhere. The fleet database has different readers (support, engineering, maintenance models) from the research store.
    2. **The consent subject is wrong.** SHDR upload is consented by the warranty owner, who may be a clinic (`CLAUDE.md` §6.0). Under B a clinic's warranty consent uploads a patient's study membership.
    3. SHDR is per device. On a shared device the log mixes several wearers' studies, and "the user can inspect" would show one wearer the others'.
    4. The withdrawal day is a personal decision and cannot be argued to be device condition. B would need it removed from the record at least. So would the timestamp (§2.3).
  - B also needs §5.1 rule 1's positive demonstration written down, and a reason why this record differs from `failed_step` (§2.5).
- **C. A split, row by row.** Study identity (study ID, descriptor hash, enrolment day, withdrawal day) is UHDR. SHDR gets one row per extract with the byte count and nothing that names the study.
  - *Cost:* the descriptor hash must go with the study ID, because NeurOne issued the descriptor and can look the hash up. A per-extract byte count can fingerprint a study, since each study's element list gives a characteristic size. Per-extract rows are dated by the upload they arrive in, which is the per-window delta that `NP-FW-NVRAM-001` D-18 forbids. With few studies running, a row in a given upload narrows to one study. So C gives NeurOne little that A with Q2 option B does not, and carries more risk.

**Q2. Under A or C, what may SHDR hold about research extracts at all?**

- **A. Nothing.** No research field in SHDR beyond the `anonymization_failed` boolean already ruled. Cost: no fleet-side signal of extract activity.
- **B. Two running totals per device: extracts sent and bytes sent.** No study identifier, no per-extract row, no date, running totals only (D-18). Cost: a non-zero total says a wearer of this device takes part in research. That is a personal decision, though not a health fact. On a clinic device it is the warranty owner who consents to its upload.

**Q3. Should the research store's key (study ID and device ID, §5.3 step 4) be reviewed under the same test?**

- **A. Yes, raise an open item.** The research store holds the same link this brief is about. Whether it can be joined to SHDR depends on what "device ID" means there. Cost: a further review, and possibly a change to the locked §5.3 flow.
- **B. No, it is accepted.** The research store is where the consented extract is meant to be, the person consented to that study, and the link is inherent in receiving an extract. Cost: the statement "no linkage table exists" then needs wording that says what is and is not linked.

## 4. What NeurOne loses under Q1 option A

- **Its own device-reported record that an extract was sent.** The fleet database would not show which devices sent what.
- **Reconciliation.** NeurOne could not compare what a device says it sent with what the research store received (a lost extract, a duplicate, or a received extract no device claims).
- **Evidence it holds itself.** To an IRB or a regulator asking "which descriptor version did this contribution come from, and did the device stop after withdrawal", NeurOne could show only its receipts. The device's side of the account is the person's to show.
- **Support.** NeurOne could not tell a person which studies their device contributed to. The app shows them.
- **Durability.** The trail is lost with the phone unless the person's UHDR backup holds it.

**What remains:** the research store's receipt of each extract, keyed to study ID and device ID (§2.6), which answers "what was received, for which study, from which device" for as long as step 4 stands. If Q3 later removes the device ID from that key, NeurOne keeps per-study counts only, and the per-device proof exists only with the person.

## 5. Recommendation

**Q1: A. Q2: A. Q3: A.**

- The record names a study in a disease category and a withdrawal day. Both are about the person. §5.1 rule 1 decides a doubtful case the same way.
- The `failed_step` ruling (§2.5) already put a weaker signal in UHDR.
- Under B the warranty owner would consent to uploading a fact about the wearer, which §6.0 exists to prevent.
- A costs nothing that is built. No path carries the record today, and the app already stores it as UHDR.
- NeurOne's need for its own record is met by the receipt in the research store.
- Q2 A because no present need for the totals is recorded, and "when in doubt" applies to them too. They can be added later by a ruling. A field already uploaded cannot be taken back.

## 6. What each ruling changes

Nothing below is done until the ruling is recorded.

| Ruling | Changes |
|--------|---------|
| Q1 A | `data-architecture-detail.md` §5.1 gains the per-field ruling and §5.3's "Audit trail (SHDR)" paragraph is rewritten. `CLAUDE.md` §5.3's last sentence changes. Entries in `completed-decisions.md` and `claude-md-revision-history.md`. The remaining "SHDR-class" comments (two in `ConsentStore.kt`, two in `ConsentStore.swift`) say UHDR. `withdrawnAtDay` and `studyWithdrawals` can be reconciled. `NP-SW-PORTAL-API-001` §4a and `NP-FW-ANON-001` §8 follow, with revision fragments. A pattern for `study` and `descriptor` is added to `ci/test_shdr_schema.py` so the ruling is gated. |
| Q1 B | `data-architecture-detail.md` §5.1 gains the ruling with the positive demonstration. The withdrawal day leaves the record. The timestamp becomes a month. The app comments stay. An upload path and schema table are then design work. |
| Q1 C | As A for the identity fields, plus a new SHDR table and its schema rules. |
| Q2 B | A new SHDR field pair and schema rows, with a running-total rule. |
| Q3 A | A new open item against §5.3 step 4. |

## 7. What I need back

1. The option for Q1, and for Q2 and Q3 where they apply.
2. A short rationale for each (`CLAUDE.md` §18).
3. Any condition that would reopen it, for example a regulator requiring a NeurOne-held device-side log.
4. Your name, role and the date.
