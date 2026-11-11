# Principal decision for review: OI-UI-KMP-11 (GitHub #566), build or adopt for the cloud storage API, and three choices that follow

**Status:** PARTLY ANSWERED 2026-11-11 (Q1 answered by the principal; Q2, Q3 and Q4 open). Not a controlled document; see `docs/human-input/README.md`.
**From:** NeurOne Software Engineering (`OI-UI-KMP-11` M9 to M12; GitHub #566; `NP-SW-CLOUDSTORE-001` §7)
**To:** The principal
**Response needed:** a ruling on each question in section 3, a short rationale, conditions that would reopen it, name, role and date.
**Still needed:** a ruling on Q2, Q3 and Q4 (section 3), as section 4 sets out.
**Why now:** #566 asks for a build-or-adopt decision and cannot close without one. Nothing is blocked today: every cloud place carries the health-data backup only, and no migration runs over one.

## 1. The questions in one line each

1. Is "build thin adapters, adopt no storage library" the decision of record?
2. How is Oracle Cloud Infrastructure reached, if at all?
3. What must be shown before a cloud route may carry a migration, and who provides the test accounts?
4. Should Google Drive get a claim built on a pre-generated file ID, or stay backup-only?

## 2. What is known

All of it is in `docs/np_sw_cloudstore_001.md`; section numbers below are that document's.

- **Built:** eight adapters behind six calls (Amazon S3, Azure Blob, Google Cloud Storage, OneDrive, Google Drive, CloudKit, Dropbox, WebDAV), one shared contract suite, sign-in and credential storage, and the screens (§4b, §4c). No new library was added; the one piece of cryptography, AWS Signature Version 4, is in the Rust core (`CLAUDE.md` §22).
- **Direction already given:** on 2026-10-27 the principal directed "build the thin adapters as a reusable library" (§4b). It was acted on. It is not logged in `docs/status/completed-decisions.md`, and §4a still reads "evaluated, not decided".
- **Evaluated and not adopted (§4a):** Apache OpenDAL and `object_store` (Rust) cover three of the five named providers and give no create-if-absent for OneDrive, Google Drive or CloudKit; jclouds exposes no conditional write and is JVM-only; rclone shows no server-arbitrated create. With a network transport, either Rust library adds a C cryptography library the repository does not have, 134 to 180 crates (the core has 54) and 3.3 to 4.4 MB on a host build; OpenDAL with Azure carried an advisory with no fix when measured (2026-10-27).
- **Documentation, re-read 2026-11-11 (§7.4):** S3, Azure Blob, Google Cloud Storage and Oracle's native API each document a create-if-absent and strong read-after-write. OneDrive and CloudKit document a create-if-absent form and no consistency. Google Drive has none by name. iCloud Drive keeps both writers' versions, so it cannot claim.
- **Not known (§7.6):** how any provider actually behaves. No request has been sent to any provider. Ten documentation points are unverified, among them Azure's refusal status, Oracle's refusal status and whether its S3 compatibility endpoint honours the condition, and CloudKit's behaviour on a first save.
- **Oracle Cloud Infrastructure has no adapter** (§7.5), although it is one of the five providers named on 2026-10-09.

**Assumed, not established:** that the adapters' maintenance stays small enough for the team to carry. Five defects were found by reading provider pages or running server software after the code was written (§7.7); more should be expected at the first run against each provider.

## 3. Questions

**Q1. Is "build" the decision of record?** *Answered 2026-11-11: yes (section 5).*

| Option | What it means | Cost |
|--------|---------------|------|
| **A. Build (recommended).** Record the thin-adapter library as the decision; adopt no storage library | Nothing changes in code. §4a is closed as the evaluation behind the decision. No SOUP record is needed | NeurOne tracks every listed provider's API and sign-in itself; no maintainer is named |
| B. Adopt OpenDAL's core without a transport for S3, Azure Blob and Google Cloud Storage; keep NeurOne's adapters for the rest | Replaces three working adapters. Needs a platform HTTP transport behind the C ABI (not built), a run on each target, a SOUP record, and an anomaly evaluation at each upgrade of a pre-1.0 library | Engineering time for the transport seam and the device builds; about 2.3 MB on a host build with one service (§4a.5 item 6); the five other stores are unchanged |
| C. Leave it undecided | #566 stays open on this point | None now |

The sender recommends A, for the reasons in §7.7: no candidate covers the list, the adapters exist and pass one suite, and adoption's cost is measured while its benefit covers only the three best-documented stores.

**Q2. Oracle Cloud Infrastructure.**

| Option | What it means | Cost |
|--------|---------------|------|
| A. Offer it through the existing S3 form, at Oracle's S3 compatibility endpoint, for the health-data backup only | No new code; a person enters a Customer Secret Key and the endpoint host. Must be tried against an Oracle account first | Oracle's pages do not say the compatibility endpoint honours a conditional create, so it could never carry a migration on today's evidence |
| B. Write a native adapter | The native API documents the create-if-absent. Its request signature is RSA-SHA256 with the person's API private key, which `CLAUDE.md` §22 puts in the Rust core: a new crate in `NP-SOUP-RUST-001` and new cryptographic code | The largest of the three; an RSA crate's advisory status has to be checked first |
| C. Remove Oracle from the initial list until demand is shown | A change to the list decided on 2026-10-09 | None in engineering |

The sender leans to A as the cheap first step and has no basis for choosing between B and C; that depends on demand, which is not known.

**Q3. Promoting a cloud route to carry a migration.**

Proposed rule (§7.8): a route declares an exclusive create only when (a) the provider's own pages document both the create-if-absent and read-after-write, (b) the contract suite has passed against the provider itself, with the claim race run from two separate clients, and the run is recorded with its date, and (c) the change that flips the flag cites both. **Is that the rule? And who opens the test accounts** (AWS, Azure, Google Cloud, Oracle, an Apple developer container, a Microsoft and a Google consumer account) **and registers the OAuth clients** (§4c.13)? The accounts and registrations are the owner's to create; none exists, and no engineering step can stand in for them.

**Q4. Google Drive.**

Google's guide (read 2026-11-11) says a create with a pre-generated file ID that was already used is answered 409. A claim could be built on that: the phone that writes the bundle takes an ID from Google, carries it in the bundle, and each restoring phone creates the claim file with that ID.

| Option | Cost |
|--------|------|
| A. Stay backup-only (as built; recommended until Q3's run exists) | A Google Drive user cannot migrate through Drive |
| B. Design and build the ID-based claim | A change to what the bundle carries and to the store interface, resting on two unverified points: that the 409 holds between two different clients, and what happens to the ID after the claim is released |

## 4. What is needed back

For each question: the option chosen (or another), a one-line rationale, any condition that would reopen it, and name, role and date. For Q3, also who will create the accounts and registrations, or a statement that they are deferred.

An answer takes effect only when it is recorded in `docs/np_sw_cloudstore_001.md` and logged in `docs/status/completed-decisions.md`. Until then the repository treats the question as open.

## 5. Answers

**Q1, answered 2026-11-11 by the principal** (relayed to the sender by the session lead): "build thin adapters, adopt no storage library" is the decision of record. This is option A.

- **Recorded in:** `docs/np_sw_cloudstore_001.md` §7.1 and §7.7; `docs/status/completed-decisions.md` (2026-11-11).
- **What it ratifies:** the direction of 2026-10-27 to build the thin adapters as a reusable library, which was acted on then and not logged.
- **Rationale:** the principal gave none beyond the decision. The reasons in `NP-SW-CLOUDSTORE-001` §7.7 are the sender's own, from its evaluation, and are kept there as the supporting record; they are not the principal's words.
- **Conditions that would reopen it:** none were stated.
- **Not covered by this answer:** Q2, Q3 and Q4. Nothing in the repository acts as if they were answered.

**Q2, Q3, Q4:** open.
