# Common Cloud Storage API — Provider Findings (Draft)

**Project:** NeurOne
**Document:** NP-SW-CLOUDSTORE-001
**Revision:** 4
**Date:** 2026-10-27
**Status:** DRAFT
**Effective Date:** —
**Author:** NeurOne Software Engineering
**Approved By:** — (new document, DRAFT, not approved)
**References:** CLAUDE.md §5.1, §17, §18, §19, §20, §22 · `docs/np_sw_001.md` §6.3.7 (`SWR-MG-07`, `SWR-MG-10`, `SWR-MG-11`, `SWR-MG-12`) · `common/npps-core/src/object_route.rs` · `docs/np_soup_btle_001.md` (SOUP record precedent)
**Related Issues:** GitHub #566, #565; `OI-UI-KMP-11` (M9, M10, M11, M12), `OI-UI-KMP-10` (`docs/status/pending-decisions.md`)
**Gate:** None yet
**IEC 62304 Class:** Not assigned. No SOUP is adopted by this draft.
**Supersedes:** None
**Change Summary:** Rev 4 (2026-10-27) — §4a.5 now holds measurements of `opendal` and `object_store` (dependency count, licences, RustSec, size, targets). Both with a built-in transport pull a C cryptography library and cannot be built for iOS or Android on this host; `opendal` with the S3 service and **no** built-in transport checks clean for web, iOS and Android. Rev 3 (2026-10-27) — new §4a, the build-or-adopt evaluation (#566): six candidates read against the issue's tests. **No candidate is adopted and no SOUP record is made.** Rev 2 (2026-10-27) — the provider pages were read first-hand (network access opened); §3 now grades each row. All four infrastructure stores document a create-if-absent; OCI's failure code, Azure's consistency and the consumer routes' create-if-absent remain open. Gap 6 added. Rev 1 (2026-10-27) — first draft from search excerpts. Nothing is tested, so `SWR-MG-11` stays "Not built".
**Review Cadence:** On each provider verified, and at G2

---

## 1. What this draft is, and is not

#566 asks for a decision record: the supported providers, the common interface, the per-provider mapping against the
storage an individual has, a verified conditional-write result for each, the build-or-adopt decision, a contract-test
plan, and SOUP records for anything adopted. **This draft holds the research so far (task B of the issue). It decides
nothing.** It is not the decision record.

**How the evidence is graded.** Rev 1 rested on search excerpts. **Rev 2 (2026-10-27) reads the provider pages first-hand**
(downloaded 2026-10-27 and converted to text; the page's own "last updated" date is given where the page shows one).

| Grade | Meaning |
|-------|---------|
| **Read** | The provider's own page was read on 2026-10-27 and says so. |
| **Excerpt** | A search result quoted an official page that was not read. |
| **Unverified** | General knowledge, an inference, or nothing found on the pages read. |

`SWR-MG-11` requires the result "verified against the provider's current documentation and recorded with the date".
**A "Read" row meets that for what it states. It is documentation, not a test:** a provider's page does not prove the
behaviour, and the contract tests (two simultaneous claim writes) are still owed per provider (§5).

## 2. The interface already in the repository

The common API is, in effect, written. `common/npps-core/src/object_route.rs` defines six object calls on named
objects; a provider implements only these (`SWR-MG-11`):

| Call | Meaning |
|------|---------|
| `objectGet(name)` | the object, or null when there is none |
| `objectPut(name, data)` | write, replacing any earlier object |
| `objectCreate(name, data)` | write **if and only if** no such object exists, arbitrated by the store itself (`SWR-MG-07`) |
| `objectDelete(name)` | true also when there was no such object |
| `objectList(prefix)` | names under a prefix |
| `objectSpace()` | free bytes, or null when the provider cannot say |

Any call may return `{failed: true}`; a flow then ends `route_unavailable`. The core fixes the object names
(`np-migration-bundle.npbk`, `np-migration-claim.json`, `np-migration-confirmation.json`, probe prefix `np-probe-`) and
holds the claim, read-back and probe logic. The Kotlin, Swift, C# and TypeScript `ObjectStore` drivers are named; **no
provider is implemented in any of them.**

### 2.1 The issue's list against the six calls

| #566 item | Covered by | Note |
|-----------|-----------|------|
| 1. Put and get the bundle | `objectPut`, `objectGet` | **Gap 1:** one call carries the whole bundle as a hex string. No chunked or resumable put. |
| 2. Find the bundle | `objectGet` by fixed name, `objectList` | Covered. |
| 3. Claim, read the claim | `objectCreate`, `objectGet` | Covered. |
| 4. Write and read the confirmation | `objectPut`, `objectGet` | Covered. The confirmation is an unconditional replace. |
| 5. Release a claim | `objectDelete` | Covered. |
| 6. Delete the bundle | `objectDelete` | Covered. Whether the process deletes it is not decided. |
| 7. Read-after-write per provider | none | **Gap 3:** not an interface matter; it belongs in the decision record (§3). |
| 8. Authentication on the user's own account | none | **Gap 2:** the host handles it; the interface has one undifferentiated `failed`. |
| 9. All four app targets, cross-platform bundle | one driver per language | **Gap 5:** where the adapters live is unstated. |
| M12 space check | `objectSpace` | Covered. |

### 2.2 Gaps and questions raised

1. **Large bundles.** `objectPut` and `objectCreate` take the whole payload as one hex string, which doubles its size in
   memory, and there is no chunked or resumable upload. The issue says "resumable or chunked if a provider needs it".
   Bundle size is not stated in the documents read. Graph needs byte ranges in multiples of 320 KiB and recommends resumable
   upload above 10 MiB (§3.2). This needs a decision and probably a change to `object_route.rs` or a
   driver-side chunking rule.
2. **Sign-in failure cannot be told from other failure.** No network, no permission, a refused write and a signed-out
   account all return `{failed: true}`. M12 asks the message to say which operation failed *and what the user can do*
   ("sign in again"). Whether the probe result can carry a reason is open.
3. **Consistency is assumed, not stated.** After `createClaim` the flow reads the claim back and checks it is ours
   (`claim_readback`). `OI-UI-KMP-11` M9 (3) says write-then-read-back narrows the race and does not close it on an
   eventually consistent store. Each provider's read-after-write for get after create, put and delete must be written down
   before the claim is called atomic on it.
4. **Name-based get and create may not exist on every consumer store.** The core addresses objects by fixed name. Google
   Drive's own reference says a name "isn't necessarily unique within a folder", so "get by name" is a query and "create if
   absent by name" has no native form (**confirmed**, §3.2). A client-chosen file ID might serve; whether a repeated create
   is refused is **not stated** on the pages read. This is the largest risk to the consumer-account mappings.
5. **Where the adapters live.** Four driver languages are named; CLAUDE.md §20 says the UI is written once in Compose
   Multiplatform. Whether provider adapters are written once in shared Kotlin or once per platform is unstated, and it drives
   the cost of keeping up that #566 asks about.
6. **`objectCreate` has two outcomes and the providers have more.** The core's `{created}` is true or false, and any call may
   be `{failed}`. S3 can answer **409** when a concurrent delete wins (retry for a PutObject, restart for a multipart
   upload), OneDrive reports a name conflict only at the end of an upload session, and a lost response leaves a claim
   that may or may not have been written. The mapping from each provider's answers to created, already exists and
   indeterminate has to be defined, because the flow treats a claim "written but not read back" as not owned
   (`SWR-MG-07`).

## 3. Per-provider findings

Every row was read on 2026-10-27 unless marked otherwise. Page dates are the page's own.

### 3.1 Conditional create and consistency

| Provider (storage) | Conditional create | Consistency | Grade |
|--------------------|--------------------|-------------|-------|
| **AWS** S3 | `If-None-Match: *` on PutObject, CopyObject and CompleteMultipartUpload; needs `s3:PutObject`. No object of that key: 200. An existing object: **412 Precondition Failed**. Of several conditional writes to one key, the first to finish succeeds and the rest get 412. **409 Conflict** is also possible if a concurrent delete completes before the conditional write (a PutObject may be retried; a CompleteMultipartUpload must restart from CreateMultipartUpload). In a versioned bucket only the current version counts, and a delete marker counts as absent. `If-Match` (ETag) gives compare-and-swap. Page: "Conditional writes" (page date not shown in the text). | "Strong read-after-write consistency for PUT and DELETE requests of objects … in all AWS Regions", for new objects, overwrites and deletes; updates to a single key are atomic. LIST consistency is **Excerpt** only (the 2020-12-01 announcement), not on the pages read. The pages do not say in so many words that conditional writes are covered. | Read |
| **Azure** Blob Storage | "Conditional headers" page (last updated 2025-11-25): `If-None-Match` takes an ETag or the wildcard `*`; "Specify the wildcard character (*) to perform the operation only if the resource doesn't exist, and fail the operation if it does exist." Failure is **412 Precondition Failed**. The Put Blob page (last updated 2026-01-20) says Put Blob supports conditional headers. The page also lists conditions on the blob for "version 2025-11-05 and later". | **Not found** on the pages read. The redundancy page notes that with geo-replication the secondary region "is typically behind the primary", so reads must be pinned to the primary. | Read |
| **Google Cloud** Storage | "Request preconditions" page (last updated 2026-10-22): `ifGenerationMatch=0` "only proceeds if no object with the specified name exists in the bucket or if there are only noncurrent versions"; "if there is a live version … the request fails with a status code of 412 Precondition Failed". The 0 value is for writing object data only. | "Strong global consistency" for object read-after-write, read-after-delete and object listing; a written object is "immediately available for reading … as soon as you receive a success response" (consistency page, last updated 2026-10-22). | Read |
| **Oracle Cloud** Object Storage | The Python SDK reference for `put_object`: `if_none_match` "The only valid value is '*', which indicates that the request should fail if the resource already exists". `if_match` also exists. **The failure status code and the behaviour under concurrent writers are not stated** on the page read. The OCI API reference page itself is a script-built page and could not be read as text. | "STRONG CONSISTENCY: When a read request is made, Object Storage always serves the most recent copy of the data that was written to the system" (service overview). | Read (SDK reference, not the API reference) |
| **Apple** CloudKit | **Native:** `ifServerRecordUnchanged` compares the record change tag; if the server record has a newer tag the save is not made and an error is reported (Apple documentation page, 2026 copyright). That a new record whose ID is already in use is refused is **not stated** on that page. **CloudKit Web Services** (Documentation Archive): operation type `create` is documented as "Create a new record. This operation fails if a record with the same record name already exists", the only documented create-if-absent in the CloudKit pages read. `forceUpdate`, `forceReplace` and `forceDelete` ignore conflicts. | **Not found.** | Read; native create-if-absent **Unverified** |

Secondary sources (OVHcloud and CoreWeave S3-compatible documentation) describe the same 412 pattern. They say nothing
about the providers named here and are not evidence for them.

### 3.2 What an individual has (the consumer-account question)

| Provider | Consumer storage | What the app would use | Grade |
|----------|------------------|------------------------|-------|
| **AWS** | Believed to be none; the user would need an AWS account and a bucket. Free-tier terms not checked. | A bucket the user creates. | **Unverified** |
| **Oracle Cloud** | Believed to be none; same. Free-tier terms not checked. | A bucket the user creates. | **Unverified** |
| **Microsoft** | Blob Storage is an infrastructure product (an Azure storage account). The consumer route is OneDrive through Microsoft Graph. | The OneDrive **app folder**: `special/approot`, scope `Files.ReadWrite.AppFolder`. The folder "behaves like any other folder in the user's OneDrive, users can add, modify, and remove content" and can rename or move it; it is created on the first call and named after the app (app folder page, last updated 2026-05-14). | Read |
| **Google** | Cloud Storage needs a project. The consumer route is Google Drive. | The Drive **appDataFolder**: hidden, "only accessible by your app", not reachable through the Drive user interface, scope `drive.appdata` (non-sensitive); files in it cannot be shared, moved or trashed (guide, last updated 2026-09-03). | Read |
| **Apple** | iCloud through CloudKit. The private database counts against the user's iCloud quota and needs an active iCloud account on the device (Apple documentation, from Rev 1 excerpt). | The CloudKit private database. | Excerpt |

**Google Drive, conditional create.** The files reference (last updated 2026-07-14) says a file's name "isn't necessarily
unique within a folder", which **confirms Gap 4**: there is no create-if-absent by name. `files.generateIds` produces IDs
"which can be provided in create or copy requests". Neither the files resource nor the create page read shows an ETag or a
precondition parameter, and **whether a second create with an already-used ID is refused is not stated** on the pages
read. A client-chosen ID is the only candidate for an atomic claim on Drive and it is **Unverified**.

**OneDrive, conditional create.** The `createUploadSession` page (last updated 2025-10-15): `@microsoft.graph.conflictBehavior`
is `fail` (default), `replace` or `rename`; an `if-match` ETag that does not match returns 412, and an `if-none-match` ETag
that matches returns 412. A conflict that arises during an upload session is reported **only when the last byte range is
uploaded**, as `409 Conflict` with code `nameAlreadyExists`. Byte ranges must be multiples of 320 KiB (327,680 bytes),
resumable upload is recommended above 10 MiB, and an explicit completion step (`deferCommit`) is supported only on OneDrive
for Business and SharePoint. **Not read:** a simple (single-request) upload with `conflictBehavior`, and any consistency
statement.

### 3.3 Reaching a provider from every target

`OI-UI-KMP-11` M10 (d) says "Android to iOS rules out any provider that is not usable on both". The CloudKit Web
Services reference (Documentation Archive) says CloudKit web services and CloudKit JS let an app reach its CloudKit
databases from the web. **How the private database is reached from a non-Apple native app, what sign-in it needs, and
whether the archive page is current were not examined.** The Apple route stays open for Android until that is read. The
other four providers are plain HTTPS services reachable from any target.

## 4. What this suggests (not decided)

1. AWS and Oracle Cloud probably offer an individual no storage without an infrastructure account. This is still
   **Unverified**; if confirmed, the issue's own finding applies: the user creates one, or the provider is listed as not
   supporting consumer accounts.
2. The four infrastructure object stores each document a create-if-absent (S3 `If-None-Match: *`, Blob `If-None-Match: *`,
   Cloud Storage `ifGenerationMatch=0`, OCI `if_none_match`), and S3, Cloud Storage and OCI document strong consistency.
   Azure's consistency and OCI's failure code and concurrency behaviour are the open items.
3. The consumer routes differ. OneDrive has a documented `fail` conflict behaviour, with the drawbacks in Gap 6 and an app
   folder the user can alter. CloudKit documents `create` only in its web-services form. Google Drive has no documented
   create-if-absent by name, so it depends on a client-chosen ID that nothing read confirms.
4. A provider without a usable conditional write is not offered for migration and stays listed for UHDR-only backup
   (`SWR-MG-10`, M12 as confirmed 2026-10-09). The in-app check is a capability check, not a proof of claim safety under two
   simultaneous writers (`SWR-MG-12`).

## 4a. Build or adopt

**Status: evaluated, not decided.** The decision is the principal's. Nothing here adopts a library, so no SOUP record is made
(`NP-SOUP-RUST-001` and `NP-SOUP-KOTLIN-001` are where one would go).

### 4a.1 What a candidate must pass

From the issue and from the standing rules (`CLAUDE.md` §22; `NP-SOUP-KOTLIN-001` §2a):

1. It exposes the **conditional create**. The issue: "a library that does not expose the conditional write is not enough".
2. It runs on **every app target**: Android, iOS, desktop and web (`CLAUDE.md` §20).
3. Its licence does not oblige NeurOne's code to be copyleft (§22; §2a rule 5).
4. It is not a pre-release (§2a rule 2), and it is not used for cryptographic work that §22 gives to the Rust core.
5. It would need a SOUP record (IEC 62304), and it covers providers the app offers.

### 4a.2 Candidates

Read on 2026-10-27 from the crate index and the projects' source on GitHub; the projects' own documentation sites could not
be reached, so capability rows come from source, not from a documentation page.

| Candidate | Licence and version | Conditional create | Providers of the five | Targets and maturity |
|-----------|--------------------|--------------------|-----------------------|----------------------|
| **Apache OpenDAL** (Rust) | Apache-2.0; `opendal` 0.59.5, 2026-10-18; pre-1.0 | `write_with_if_not_exists` is **true** for S3, Google Cloud Storage and Azure Blob (source). **OneDrive: `if_match` only, no create-if-absent flag.** **Google Drive: neither flag.** | S3, Cloud Storage, Blob, OneDrive (no create-if-absent), Drive (none). **No OCI or iCloud/CloudKit service seen** in the README. OCI through its S3-compatible endpoint is untested. | Rust core. Swift binding is "WIP … unreleased … not suitable for production use" (its README). A Java binding is published. Web not examined. |
| **`object_store`** (Rust, Apache Arrow) | MIT or Apache-2.0; 0.14.2, 2026-09-15 | `PutMode::Create`: "an atomic write operation, returning `Error::AlreadyExists`"; `PutMode::Update(version)` for compare-and-swap (source). | S3, Azure Blob, Cloud Storage (and generic HTTP, local files). Nothing for OCI, OneDrive, Drive or CloudKit. | Rust. Its README: the `aws`, `azure`, `gcp` and `http` features are **not supported on `wasm32-unknown-unknown`**; each bundles a `reqwest` transport. |
| **Apache jclouds** (Java) | Apache-2.0; `master` is `2.7.1-SNAPSHOT` (release status not checked) | **None exposed:** `PutOptions` carries multipart and executor options only (source). | Not examined further. | JVM only: no iOS and no web. **Fails tests 1 and 2.** |
| **rclone** (Go) | MIT | `--immutable` is "existing files will never be updated"; a transfer-level rule, **not shown to be a server-arbitrated create-if-absent**. | Backends for OneDrive, Drive and iCloud exist (iCloud setup needs the user's Apple ID password and 2FA; app-specific passwords not accepted). OCI not confirmed. | Go. `librclone` (the embedding route) is marked experimental in its README. Needs a Go runtime and cgo. |
| **Vendor SDKs**, one per provider | Not checked per SDK. | Each SDK exposes its provider's own precondition, but five SDKs are five stacks. | The AWS SDK for Kotlin README lists JVM, Android and GraalVM targets, **not iOS or web**. The Azure, Google, OCI, Graph and CloudKit SDKs were not examined. | Mixed; per provider. |
| **Build thin adapters** | NeurOne's own code | Written to the provider's documented form (§3.1, §3.2), behind the six existing calls. | All five. | Each target's driver (`ObjectStore`). |

`NP-SOUP-KOTLIN-001` §2 lists no cross-platform HTTP client: `ktor-network` is sockets only and `okhttp` is the Android
app's SHDR upload. A thin adapter in Kotlin therefore also needs an HTTP client, which would itself be SOUP.

### 4a.3 What the evidence supports

1. **No candidate covers the five providers.** OpenDAL and `object_store` expose a conditional create for the three
   infrastructure stores they share with the issue's list (S3, Cloud Storage, Azure Blob). Neither covers OCI natively, and
   neither gives a create-if-absent for OneDrive, Google Drive or CloudKit, which are the consumer routes the issue most
   cares about. **A library does not remove the per-provider work for the hardest rows.**
2. **jclouds and rclone fail the issue's own tests.** jclouds exposes no conditional write and is JVM-only. rclone offers no
   shown atomic create and is a Go runtime to embed.
3. **Adopting a Rust library does not fit `npps-core` as it stands.** The core does no network input or output: a host
   answers the six object calls, and the crate asks for no entropy (`object_route.rs`; the `getrandom` note in its
   `Cargo.toml`). OpenDAL or `object_store` would bring a `reqwest` transport. It would have to be a separate crate behind the
   same six calls, as `btle-jni` is, not part of the core. That is a placement decision for the principal.
4. **Some of what a thin adapter needs is cryptography.** S3 request signing is HMAC-SHA256, which `CLAUDE.md` §22 puts in the
   Rust core (which already depends on `hmac` and `sha2`). Not a reason to adopt a library, but a reason not to write it in
   Kotlin.
5. **The interface is smaller than any candidate's surface.** The six calls are a subset of what OpenDAL and `object_store`
   do, which supports the issue's own suspicion that "building thin adapters may be cheaper than adopting a large library".
   **This draft does not show it is cheaper**, because the cost of the dependencies was not measured (4a.5).
6. **Sign-in is not in any candidate.** Google and Microsoft sign-in, and the iCloud account for CloudKit, are the platform's
   and the host's. The issue's item 8 stays whichever route is chosen.

### 4a.4 Cost of keeping up

The provider contract suite (`SWR-MG-11`) is the same under every route, so it is not a difference between them. What
differs is who tracks provider changes. With thin adapters NeurOne does, for five providers. With OpenDAL or `object_store`
the library tracks three infrastructure providers and NeurOne still tracks OCI, OneDrive, Drive and CloudKit, and each
upgrade of a pre-1.0 library (`opendal` is 0.59.5) is a SOUP change with its anomaly evaluation.

### 4a.5 Measurements (2026-10-27)

**Method.** Three throwaway crates outside the repository, each a small program that builds the S3, Google Cloud Storage and
Azure Blob backends and calls the create-if-absent write, read, delete and list, so the code is not dropped by the linker:
`base` (Tokio only), `od` (`opendal` =0.59.5 with `services-s3`, `services-gcs`, `services-azblob`, `reqwest-rustls-tls`) and
`os` (`object_store` =0.14.2 with `aws`, `gcp`, `azure`). Rust 1.97.0, host `x86_64-unknown-linux-gnu`, release profile with
`opt-level = "z"`, LTO, one codegen unit, `strip` and `panic = "abort"`. Dependency and licence counts are
`cargo tree -e normal` on the host; the advisory scan is `cargo-audit` 0.22.2 against 1,380 advisories (database loaded
2026-10-27). **These are host builds of a probe, not of the app; the mobile and web sizes are not measured.**

| | `base` | `od` (OpenDAL, 3 services + transport) | `os` (`object_store`, 3 services) |
|---|---|---|---|
| Release binary, host | 0.40 MB (396,296 B) | **3.74 MB** (3,735,136 B) | **4.80 MB** (4,804,168 B) |
| Added over `base` | 0 | +3.34 MB | +4.41 MB |
| Unique crates in the normal graph | 8 | 180 | 134 |
| `cargo-audit` | not run | **1 vulnerability** (below) | none |
| Licences (declared) | n/a | MIT, Apache-2.0, Unicode-3.0, BSD-3-Clause, Unlicense, ISC and dual forms | the same set |

For scale, the repository's own `neurone-npps-core` has **54** crates in its normal graph, and `common/Cargo.lock` holds
**no** `reqwest`, `rustls`, `ring` or `aws-lc` package.

**Findings.**

1. **Both configurations pull a C cryptography library.** `aws-lc-rs` 1.18.2 and `aws-lc-sys` 0.46.0 (through `rustls`) are in
   both trees, with `hmac`, `sha1`/`sha2` and `md-5` additionally in `od`. The repository has none of this today. This meets
   `CLAUDE.md` §22 head-on ("not taken from a platform or third-party library"), though the core already depends on the
   RustCrypto crates. **Whether a TLS transport counts as the "cryptographic" work §22 means is for the principal.**
2. **`opendal` with Azure carries RUSTSEC-2023-0071** (`rsa` 0.9.10, "Marvin Attack: potential key recovery through timing
   sidechannels", **no fixed upgrade available**), through `reqsign-azure-storage` and `opendal-service-azblob`. Whether the
   Azure path the app would use ever performs an RSA private-key operation was not examined. The `object_store` build
   has no advisory.
3. **Neither full configuration builds for iOS or Android on this host.** `cargo check` for `aarch64-apple-ios` and
   `aarch64-linux-android` stops in `aws-lc-sys`'s build script, because the Apple SDK and the Android NDK are not installed
   here. **That is not evidence the build is impossible with them.** It does show a C toolchain per mobile target is needed.
4. **Web.** `od` with the transport fails on `wasm32-unknown-unknown` at `getrandom`; `os` fails on Tokio and `aws-lc-rs`
   (its README says the same). Part of that is the probe's own Tokio features, so these two are **inconclusive for OpenDAL**
   and **consistent with the README for `object_store`**.
5. **The same OpenDAL core with the S3 service and no transport checks clean on every target.** A fourth throwaway crate with
   only `services-s3` and `default-features = false` (no `reqwest`, no TLS) ran `cargo check` clean for
   `wasm32-unknown-unknown`, `aarch64-apple-ios` and `aarch64-linux-android`; 103 crates; no `aws-lc`, `rustls` or `reqwest`
   (it keeps `hmac`, `sha1`, `sha2`, `md-5`); `cargo-audit` found no vulnerability. **That it type-checks is all this shows.**
   It was not run, a host-supplied transport was not wired, and the Google Cloud Storage and Azure services were not
   added to this variant.

**Still not measured:** the app-size effect on a device (iOS, Android, web); a run against any provider; whether OpenDAL
accepts a host-supplied HTTP transport in practice; the licences and targets of the Azure, Google, OCI, Graph and CloudKit SDKs;
and whether OCI's S3-compatible endpoint honours `If-None-Match: *` (OCI's "Amazon S3 Compatibility API" page, read
2026-10-27, says nothing about conditional writes either way).

**AWS SDK for Kotlin.** Its targets page (read 2026-10-27) lists JVM, Android (API 24+) and GraalVM; no iOS and no web. Its
licence is Apache-2.0 (the repository `LICENSE`).

### 4a.6 Leading option (not decided)

Thin adapters per provider behind the existing six calls, because only that route reaches all five providers and the
consumer routes. The measurements change what a library spike would look like: a full OpenDAL or `object_store` build adds
3.3 to 4.4 MB on the host, a C cryptography library the repository does not have, and a mobile C toolchain, and OpenDAL with
Azure carries an advisory with no fix. **The one variant that checked clean everywhere is OpenDAL's core without a transport
(4a.5 item 5).** If the principal wants a library for the three infrastructure stores, that variant, with the transport
supplied by the host so no TLS stack joins the core, is the one worth a spike; it is unproven.

## 5. To close #566

| Step | Needs |
|------|-------|
| Contract test per provider, including two simultaneous claim writes: documentation says what a provider promises, not that it keeps it | Test accounts at each provider; requirement rows first (CLAUDE.md §21, §18) |
| Close the open reads: OCI failure status and concurrent behaviour; Azure consistency; OneDrive simple upload with `conflictBehavior` and its consistency; Drive duplicate-ID behaviour; CloudKit native create-if-absent and web reach (§3.3) | The OCI API reference (script-built page), or a live test |
| Confirm consumer storage for AWS and Oracle Cloud (§3.2) | Provider pages |
| Decide Gaps 1 to 6 (§2.2) | Principal / Software Engineering |
| Check provider terms and app-store rules on what an app may store and how it signs in | Unchecked |
| Decide build or adopt from §4a; the host measurements are in §4a.5, the device and run-time ones are not. A SOUP record per library adopted | Evaluated and measured on the host (§4a), decision pending |
| Register any constant introduced without a derivation (CLAUDE.md §19) | None introduced here |

## 6. Sources

Pages read on 2026-10-27 (dates in §3 are the pages' own):

- [S3: conditional writes](https://docs.aws.amazon.com/AmazonS3/latest/userguide/conditional-writes.html)
- [S3: user guide, data consistency model](https://docs.aws.amazon.com/AmazonS3/latest/userguide/Welcome.html)
- [Azure: specify conditional headers for Blob operations](https://learn.microsoft.com/en-us/rest/api/storageservices/specifying-conditional-headers-for-blob-service-operations)
- [Azure: Put Blob](https://learn.microsoft.com/en-us/rest/api/storageservices/put-blob)
- [Azure: storage redundancy](https://learn.microsoft.com/en-us/azure/storage/common/storage-redundancy)
- [Cloud Storage: request preconditions](https://docs.cloud.google.com/storage/docs/request-preconditions)
- [Cloud Storage: consistency](https://docs.cloud.google.com/storage/docs/consistency)
- [OCI: Object Storage overview](https://docs.oracle.com/en-us/iaas/Content/Object/Concepts/objectstorageoverview.htm)
- [OCI: Python SDK `ObjectStorageClient`](https://docs.oracle.com/en-us/iaas/tools/python/latest/api/object_storage/client/oci.object_storage.ObjectStorageClient.html)
- [Google Drive: app data folder](https://developers.google.com/workspace/drive/api/guides/appdata)
- [Google Drive: files resource](https://developers.google.com/workspace/drive/api/reference/rest/v3/files)
- [Google Drive: files.generateIds](https://developers.google.com/workspace/drive/api/reference/rest/v3/files/generateIds)
- [Microsoft Graph: createUploadSession](https://learn.microsoft.com/en-us/graph/api/driveitem-createuploadsession?view=graph-rest-1.0)
- [OneDrive: the app folder](https://learn.microsoft.com/en-us/onedrive/developer/rest-api/concepts/special-folders-appfolder)
- [Apple: `ifServerRecordUnchanged`](https://developer.apple.com/documentation/cloudkit/ckmodifyrecordsoperation/recordsavepolicy/ifserverrecordunchanged)
- [Apple: CloudKit Web Services, modify records](https://developer.apple.com/library/archive/documentation/DataManagement/Conceptual/CloudKitWebServicesReference/ModifyRecords.html)
- [Apple: CloudKit Web Services reference](https://developer.apple.com/library/archive/documentation/DataManagement/Conceptual/CloudKitWebServicesReference/index.html)

Build-or-adopt candidates, read 2026-10-27 (crate index and source on GitHub):

- [crates.io: `opendal`](https://crates.io/crates/opendal) and [OpenDAL source](https://github.com/apache/opendal) (`core/core/src/types/capability.rs` and the `services` backends)
- [OpenDAL Swift binding README](https://github.com/apache/opendal/tree/main/bindings/swift)
- [crates.io: `object_store`](https://crates.io/crates/object_store) and [its source](https://github.com/apache/arrow-rs-object-store) (`PutMode`, README)
- [Apache jclouds `PutOptions`](https://github.com/apache/jclouds/blob/master/blobstore/src/main/java/org/jclouds/blobstore/options/PutOptions.java)
- [rclone: iCloud Drive backend, `--immutable`, `librclone`](https://github.com/rclone/rclone)
- [AWS SDK for Kotlin README](https://github.com/awslabs/aws-sdk-kotlin)

Search excerpts only (not read): [AWS strong consistency announcement](https://aws.amazon.com/about-aws/whats-new/2020/12/amazon-s3-now-delivers-strong-read-after-write-consistency-automatically-for-all-applications),
[Apple `CKContainer`](https://developer.apple.com/documentation/cloudkit/ckcontainer.md). Secondary:
[OVHcloud](https://docs.ovhcloud.com/en/guides/storage-and-backup/object-storage/s3-conditional-writes),
[CoreWeave](https://docs.coreweave.com/products/storage/object-storage/buckets/conditional-requests).
