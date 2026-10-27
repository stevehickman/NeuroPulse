# Common Cloud Storage API — Provider Findings (Draft)

**Project:** NeurOne
**Document:** NP-SW-CLOUDSTORE-001
**Revision:** 2
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
**Change Summary:** Rev 2 (2026-10-27) — the provider pages were read first-hand (network access opened); §3 now grades each row. All four infrastructure stores document a create-if-absent; OCI's failure code, Azure's consistency and the consumer routes' create-if-absent remain open. Gap 6 added. Rev 1 (2026-10-27) — first draft from search excerpts. Nothing is tested, so `SWR-MG-11` stays "Not built".
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

## 5. To close #566

| Step | Needs |
|------|-------|
| Contract test per provider, including two simultaneous claim writes: documentation says what a provider promises, not that it keeps it | Test accounts at each provider; requirement rows first (CLAUDE.md §21, §18) |
| Close the open reads: OCI failure status and concurrent behaviour; Azure consistency; OneDrive simple upload with `conflictBehavior` and its consistency; Drive duplicate-ID behaviour; CloudKit native create-if-absent and web reach (§3.3) | The OCI API reference (script-built page), or a live test |
| Confirm consumer storage for AWS and Oracle Cloud (§3.2) | Provider pages |
| Decide Gaps 1 to 6 (§2.2) | Principal / Software Engineering |
| Check provider terms and app-store rules on what an app may store and how it signs in | Unchecked |
| Build-or-adopt evaluation: licence (GPL and AGPL excluded, CLAUDE.md §22), platform fit, exposure of the conditional write, a SOUP record per adopted library | Not started |
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

Search excerpts only (not read): [AWS strong consistency announcement](https://aws.amazon.com/about-aws/whats-new/2020/12/amazon-s3-now-delivers-strong-read-after-write-consistency-automatically-for-all-applications),
[Apple `CKContainer`](https://developer.apple.com/documentation/cloudkit/ckcontainer.md). Secondary:
[OVHcloud](https://docs.ovhcloud.com/en/guides/storage-and-backup/object-storage/s3-conditional-writes),
[CoreWeave](https://docs.coreweave.com/products/storage/object-storage/buckets/conditional-requests).
