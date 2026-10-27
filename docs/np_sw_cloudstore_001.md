# Common Cloud Storage API — Provider Findings (Draft)

**Project:** NeurOne
**Document:** NP-SW-CLOUDSTORE-001
**Revision:** 1
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
**Change Summary:** Rev 1 (2026-10-27) — first draft. Records what was found so far for #566 and, as much, what could **not** be verified. **No provider's conditional-write behaviour is verified in this revision** (§3), so `SWR-MG-11` stays "Not built".
**Review Cadence:** On each provider verified, and at G2

---

## 1. What this draft is, and is not

#566 asks for a decision record: the supported providers, the common interface, the per-provider mapping against the
storage an individual has, a verified conditional-write result for each, the build-or-adopt decision, a contract-test
plan, and SOUP records for anything adopted. **This draft holds the research so far (task B of the issue). It decides
nothing and verifies nothing.** It is not the decision record.

**How the evidence is graded.** The provider documentation hosts could not be reached from the environment where the
research ran (DNS failure on the AWS, Microsoft, Google Cloud, Oracle and Google Drive documentation sites), so every
provider statement below comes from a search-result excerpt or from general knowledge. Each is labelled:

| Grade | Meaning |
|-------|---------|
| **Excerpt** | A search result quoted an official provider page. The page itself was not read. |
| **Secondary** | A non-official source (a vendor-compatible service, a forum, a blog). |
| **Unverified** | General knowledge, or an inference. Nothing was found. |

`SWR-MG-11` requires the result "verified against the provider's current documentation and recorded with the date". Only
a first-hand reading of the provider page, with its version and date, meets that. **No row in §3 does yet.**

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
   Bundle size is not stated in the documents read. This needs a decision and probably a change to `object_route.rs` or a
   driver-side chunking rule.
2. **Sign-in failure cannot be told from other failure.** No network, no permission, a refused write and a signed-out
   account all return `{failed: true}`. M12 asks the message to say which operation failed *and what the user can do*
   ("sign in again"). Whether the probe result can carry a reason is open.
3. **Consistency is assumed, not stated.** After `createClaim` the flow reads the claim back and checks it is ours
   (`claim_readback`). `OI-UI-KMP-11` M9 (3) says write-then-read-back narrows the race and does not close it on an
   eventually consistent store. Each provider's read-after-write for get after create, put and delete must be written down
   before the claim is called atomic on it.
4. **Name-based get and create may not exist on every consumer store.** The core addresses objects by fixed name. Google
   Drive allows several files with one name, so "get by name" is a query and "create if absent by name" has no native form.
   A client-chosen file identifier might serve; this is **unverified**. This is the largest risk to the consumer-account
   mappings.
5. **Where the adapters live.** Four driver languages are named; CLAUDE.md §20 says the UI is written once in Compose
   Multiplatform. Whether provider adapters are written once in shared Kotlin or once per platform is unstated, and it drives
   the cost of keeping up that #566 asks about.

## 3. Per-provider findings

### 3.1 Conditional create and consistency

| Provider (storage) | Conditional create | Consistency | Grade |
|--------------------|--------------------|-------------|-------|
| **AWS** S3 | `If-None-Match: *` on PutObject and CompleteMultipartUpload: a write to an empty key succeeds, to an existing key fails with 412; of concurrent conditional writes the first succeeds and the rest fail with 412. `If-Match` (ETag) gives compare-and-swap. Announced 2024-08-20; bucket-policy enforcement added 2024-11-25. | Strong read-after-write for GET, PUT and LIST announced 2020-12-01. DELETE not confirmed in what was read. Whether conditional writes are covered is not stated in what was read. | Excerpt (AWS announcements, InfoQ) |
| **Azure** Blob Storage | Put Blob accepts conditional headers; a failed ETag precondition returns 412. **`If-None-Match: *` on a destination blob was not seen documented.** | Not found. | Excerpt for the general model; create-if-absent **unverified** |
| **Google Cloud** Storage | Generation-match preconditions return 412 on mismatch. That `ifGenerationMatch=0` means "must not exist" is general knowledge, **not seen documented**. | Not found. | Excerpt for the precondition; the 0 case **unverified** |
| **Oracle Cloud** Object Storage | Oracle's SDK reference says `if-none-match` accepts only `*`, meaning fail if the resource exists. The status code and the behaviour under concurrent writers were not seen from Oracle. | Not found. | Excerpt (SDK reference); the rest **unverified** |
| **Apple** CloudKit | Default save policy `ifServerRecordUnchanged` compares the record change tag and fails with `serverRecordChanged`; `changedKeys` and `allKeys` do not compare. That a **new** record with an already-used record ID is refused is an inference by the fetch tool, **not documented**. Deletes by record ID carry no change tag. | Not found. | Excerpt for the policy; create-if-absent **unverified** |

Secondary sources (OVHcloud and CoreWeave S3-compatible documentation) describe the same 412 pattern for
`If-None-Match: *`. They say nothing about AWS, Oracle or any provider named here, and are not evidence for them.

### 3.2 What an individual has (the consumer-account question)

| Provider | Consumer storage | What the app would use | Grade |
|----------|------------------|------------------------|-------|
| **AWS** | Believed to be none; the user would need an AWS account and a bucket. | A bucket the user creates. | **Unverified** |
| **Oracle Cloud** | Believed to be none; same. | A bucket the user creates. | **Unverified** |
| **Azure / Microsoft** | Blob Storage is an infrastructure product. The consumer route is OneDrive through Microsoft Graph. | The OneDrive app folder (`special/approot`, scope `Files.ReadWrite.AppFolder`). A forum report says some users see it named "Graph" rather than the application name. | Excerpt (forum, secondary) |
| **Google Cloud / Google** | Cloud Storage needs a project. The consumer route is Google Drive. | The Drive `appDataFolder`, a hidden folder only the app can reach (a Drive guide excerpt, in Bengali). Conditional create not seen documented; see Gap 4. | Excerpt for the folder; the rest **unverified** |
| **Apple** | iCloud through CloudKit. The private database counts against the user's iCloud quota and needs an active iCloud account on the device (Apple documentation excerpt). | The CloudKit private database. | Excerpt |

**OneDrive, as far as seen.** `@microsoft.graph.conflictBehavior` takes `fail`, `rename` or `replace`, with `fail` the
default (answers forum and an upload-session guide; secondary). An `if-match` ETag header on an upload returns 412 on a
mismatch (Graph `createUploadSession` reference; excerpt). Where `conflictBehavior` is placed (URL, body, an `item` object)
is disputed in the forum threads. Whether replacement is atomic is not established. Uploads in slices must be multiples of
320 KiB (SDK guide), which bears on Gap 1.

### 3.3 Reaching a provider from every target

`OI-UI-KMP-11` M10 (d) says "Android to iOS rules out any provider that is not usable on both". **Nothing was found on
whether CloudKit can be reached from Android or the desktop and web hosts** (CloudKit Web Services is the candidate and
was not examined). If it cannot, the Apple route is excluded for the case that drove #566 and could serve iOS-to-iOS
migration and UHDR-only backup only. This is the largest open unknown.

## 4. What this suggests (not decided)

1. AWS and Oracle Cloud probably offer an individual no storage without an infrastructure account. If confirmed, the
   issue's own finding applies: the user creates one, or the provider is listed as not supporting consumer accounts.
2. The infrastructure object stores (S3, Cloud Storage, OCI, Blob) look closest to the six-call interface. The consumer
   routes (iCloud, Drive, OneDrive) are the weakest on a verified create-if-absent, and for Drive the fixed-name
   addressing is a structural mismatch (Gap 4).
3. A provider without a usable conditional write is not offered for migration and stays listed for UHDR-only backup
   (`SWR-MG-10`, M12 as confirmed 2026-10-09). The in-app check is a capability check, not a proof of claim safety under two
   simultaneous writers (`SWR-MG-12`).

## 5. To close #566

| Step | Needs |
|------|-------|
| Read each provider's current documentation first-hand and record version and date against every §3 row | Network access to the provider documentation hosts, or the pages supplied |
| Decide Gaps 1 to 5 (§2.2) | Principal / Software Engineering |
| Confirm CloudKit reach from Android, desktop and web (§3.3) | Apple's CloudKit Web Services documentation |
| Check provider terms and app-store rules on what an app may store and how it signs in | Unchecked |
| Build-or-adopt evaluation: licence (GPL and AGPL excluded, CLAUDE.md §22), platform fit, exposure of the conditional write, a SOUP record per adopted library | Not started |
| Contract-test plan, including two simultaneous claim writes per provider; requirement rows first (CLAUDE.md §21, §18) | Not started |
| Register any constant introduced without a derivation (CLAUDE.md §19) | None introduced here |

## 6. Sources

Excerpts quoted by search results; none read first-hand.

- [Amazon S3 now supports conditional writes](https://aws.amazon.com/about-aws/whats-new/2024/08/amazon-s3-conditional-writes)
- [InfoQ: Amazon S3 conditional writes](https://www.infoq.com/news/2024/08/amazon-s3-conditional-writes)
- [Amazon S3 strong read-after-write consistency](https://aws.amazon.com/about-aws/whats-new/2020/12/amazon-s3-now-delivers-strong-read-after-write-consistency-automatically-for-all-applications)
- [Azure: specifying conditional headers for Blob operations](https://learn.microsoft.com/rest/api/storageservices/specifying-conditional-headers-for-blob-service-operations)
- [Google Cloud Storage request preconditions](https://docs.cloud.google.com/storage/docs/request-preconditions)
- [OCI .NET SDK `PutObjectRequest`](https://docs.oracle.com/en-us/iaas/tools/dotnet/96.1.0/api/Oci.ObjectstorageService.Requests.PutObjectRequest.html)
- [Apple: `ifServerRecordUnchanged`](https://developer.apple.com/tutorials/data/documentation/cloudkit/ckmodifyrecordsoperation/recordsavepolicy/ifserverrecordunchanged.md)
- [Apple: `CKContainer`](https://developer.apple.com/documentation/cloudkit/ckcontainer.md)
- [Microsoft Graph: `createUploadSession`](https://learn.microsoft.com/en-us/onedrive/developer/rest-api/api/driveitem_createuploadsession)
- [Google Drive: app data folder](https://developers.google.com/workspace/drive/api/guides/appdata?hl=bn)
- [OVHcloud: conditional writes](https://docs.ovhcloud.com/en/guides/storage-and-backup/object-storage/s3-conditional-writes) (secondary)
- [CoreWeave: conditional requests](https://docs.coreweave.com/products/storage/object-storage/buckets/conditional-requests) (secondary)
