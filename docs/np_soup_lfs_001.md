# LittleFS SOUP Record and Hazard Analysis — Hub Storage (SW-02, Class B)

**Project:** NeurOne
**Document:** NP-SOUP-LFS-001
**Revision:** 2
**Date:** 2026-09-14
**Status:** DRAFT — the hazard analysis (§4–§6) is complete and does not depend on a version. **Rev 2 closes `OI-LFS-01`: a version is pinned and vendored** (littlefs `v2.11.3`, `firmware/vendor/littlefs/`), so §7.1's blocking action is discharged and §2's "not in the tree" finding is superseded — see §2.1 and §7.4. The IEC 62304 §7.1.2 anomaly-list evaluation is **still not performed**: it is now performable and is `OI-LFS-02`, together with the NeurOne power-loss injection test. **No atomicity claim may be relied on yet.**
**Effective Date:** —
**Author:** NeurOne Firmware Engineering
**Approved By:** — (DRAFT, not approved)
**References:** `NP-SW-001` Rev 5 §9.4 (SOUP register — this record replaces its LittleFS row, and its second Class B SOUP note carries this document's conclusion); `NP-SOUP-CMSIS-001` Rev 1 (method precedent); `NP-FW-EMMC-001` Rev 2 `EMMC-FS-01` (instance parameters), `EMMC-CFG-01/-02` (Config contents and write whitelist), `EMMC-HW-01` (endurance); `NP-FW-EMMC-002` Rev 2 §C (UHDR mount); `NP-FW-NVRAM-001` Rev 2 §3.3, §4 (power-loss atomicity), §9 (Class B argument); `NP-FW-HUB-001` Rev 1 §6.5 (log durability model); `NP-CONV-001` Rev 6 §8 (a check must be falsified before it is trusted); `firmware/hub_control/include/np_log_backend.h`
**Related Issues:** #339 (`OI-NVRAM-12`), #75 (`NP-SBOM-001` — T2 510(k) cybersecurity submission)
**Gate:** G2
**IEC 62304 Class:** SW-02 Class B
**Supersedes:** None — new document. It replaces the single verification cell that `NP-SW-001` §9.4 previously carried for this component.
**Pinned version:** littlefs **v2.11.3** (tag commit `6cb4e86540eca0d9ba62500a298385c9d863c8be`), vendored at `firmware/vendor/littlefs/` with per-file SHA-256 — `firmware/vendor/littlefs/VERSION` is the SOUP record proper, and this document is its hazard analysis.
**Review Cadence:** On any change to the pinned version, on first integration, and at G2

---

> **⚠ REV 2 — WHAT CHANGED, AND WHAT DID NOT (2026-09-14, closes `OI-LFS-01`).**
>
> | | Rev 1 | Rev 2 | Cause |
> |---|---|---|---|
> | Version | *"none — not integrated"* | **v2.11.3**, pinned and vendored | `OI-LFS-01` performed |
> | Source in tree | none | `firmware/vendor/littlefs/`, 5 files, per-file SHA-256 | ditto |
> | Configuration | unrecorded | recorded and **tested** (§7.4) | `§7.3` performed |
> | §7.1.2 evaluation | not performable | **performable, and still not performed** | `OI-LFS-02` |
> | Any reliance on `L-1…L-4` | blocked | **still blocked** | `OI-LFS-02` |
>
> **The Rev 1 banner below is retained verbatim** (`NP-CONV-001` §7). Its central finding — that
> the component was absent — was **true and is now resolved**, not refuted; the reasoning it
> produced is what §6.2 turns on, and that survives untouched. §2.1 records what is in the tree now.
>
> **The one thing a reader must not take from Rev 2: a vendored directory is not a test.**
> `OI-LFS-01` was the *first* of two blocking items and it was never the one that verifies
> anything. `NP-FW-NVRAM-001` §4, `EMMC-FS-01` and `NP-FW-HUB-001` §6.5 are exactly as unverified
> today as they were yesterday — what changed is that the work that would verify them can now be
> done. §7.4 states what was performed and §7.5 what a reviewer may and may not cite it for.

---

> **⚠ READ FIRST — the finding that reorders this document (Rev 1, retained).**
>
> `NP-SW-001` §9.4 has carried LittleFS as an SW-02 Class B SOUP item since Rev 1, with version
> *"2.x"* and the verification *"Power-loss testing per LittleFS test suite."* `OI-NVRAM-12` raised it
> as the only Class B SOUP item that is neither vendored, version-pinned nor anomaly-evaluated.
>
> **It is worse than unmanaged: LittleFS is not in the tree at all.**
>
> `grep -rn "lfs_" firmware/` returns **two hits, both comments** — `np_log_backend.h:79` and `:83`,
> naming `lfs_file_write` and `lfs_file_sync` as what the `OI-LOG-06/07` HAL seams will eventually
> call. There is no LittleFS source anywhere in the repository, nothing links it, and nothing calls
> it. It is **nominated** SOUP, not integrated SOUP.
>
> Three consequences follow, and they are why this record is a hazard analysis rather than a
> vendoring ticket:
>
> 1. **Every power-loss atomicity guarantee in the document set is currently asserted against a
>    component that is not present.** `NP-FW-NVRAM-001` §4, `NP-FW-EMMC-001` `EMMC-FS-01`, and
>    `NP-FW-HUB-001` §6.5 all rest on it. None of them is wrong; all of them are **unverified in a way
>    that no reader of the SOUP table could tell**, because the table's verification cell named an
>    upstream test suite run by upstream against no pinned version. **That cell is the defect.** A
>    SOUP verification entry must record what *NeurOne* did; citing someone else's test suite is
>    citing the existence of diligence, not performing it.
> 2. **The §7.1.2 anomaly evaluation cannot be performed yet, and saying so is the honest output.**
>    IEC 62304 §7.1.2 evaluates the published anomaly list *for the version in use*. `2.x` is not a
>    version; it is a major-version family spanning years of releases. Recording a conclusion against
>    it would be the same failure as `OI-DOC-01` — a record that resolves to nothing. §7 names the
>    blocking action.
> 3. **The hazard analysis does not depend on any of that, and it is the half that matters now.**
>    What a filesystem anomaly could *reach* is a property of the callers, which exist and can be
>    read. §4–§6 do that, and they find one path worth stating: **the Class B classification of this
>    component is not a property of LittleFS. It is a property of `np_module_map`'s reject-and-rebuild
>    policy — and Map 3 is specified to need the opposite policy.** §6.2.

---

## 1. Scope

| | |
|---|---|
| Component | LittleFS — a power-loss-resilient filesystem for microcontrollers |
| Upstream | `littlefs-project/littlefs` |
| Version in use | **v2.11.3** — pinned and vendored 2026-09-14, `firmware/vendor/littlefs/` (§2.1). Rev 1 read *"none — not integrated"* |
| Version proposed for pinning | ~~a named upstream release tag, selected at integration (§7.1)~~ — **selected: `v2.11.3`** (§7.4) |
| SW item | SW-02 (hub control program, main processor) |
| Safety class | Class B |
| Reachable from SW-01 (Class C)? | **No.** The safety MCU is a separate CMake project with its own toolchain file; it has no eMMC interface and no storage of its own beyond its internal flash |

**In scope:** what NeurOne's specifications require of this component; what is actually integrated
today; the hazard analysis of every claim that rests on it; the conditions under which the Class B
classification holds; and what must be done before any of those claims may be relied on.

**Out of scope:** the partition layout and mount policy (`NP-FW-EMMC-001`), key custody
(`NP-FW-EMMC-002` §C), and the Map 3 store design itself (`NP-FW-NVRAM-001` Rev 2).

---

## 2. What is actually integrated

| Artifact | Present? | Evidence |
|---|---|---|
| LittleFS source under `firmware/vendor/` | **No** | `firmware/vendor/` holds `cmsis_core`, `cmsis_device_g0`, `freertos`, `mcux_sdk` — four directories, no fifth |
| A `VERSION` SOUP record | **No** | — |
| Any call into the library | **No** | `grep -rn "lfs_" firmware/` → 2 hits, both comments in `np_log_backend.h` |
| A NeurOne test exercising it | **No** | `np_log_backend_tests.c` exercises the **staging and coalescing layer above** the HAL seam; the seam itself is host-modeled under `NPTEST_HOST` |
| A build that links it | **No** | `np_application.elf` links with 0 unresolved symbols *because* `OI-LOG-05..07` are unimplemented seams, not because the filesystem is there |

**The HAL seam is where it will enter.** `np_log_backend.h` declares three lower entry points —
`np_log_hal_part_open` / `_append` / `_sync` (`OI-LOG-05..07`) — implemented by the platform LittleFS
glue on target and host-modeled in `np_log_backend.c` under `NPTEST_HOST`. That seam is the correct
place for it, and its existence is why the absence has been invisible: the layer above it is real,
tested, and passes.

### 2.1 What is actually integrated — as of Rev 2 (2026-09-14)

The table above is retained because it is the finding this document was written for, and because the
difference between the two tables is the whole of what `OI-LFS-01` bought. **Two of its five rows
change. Three do not, and those three are the ones that matter for reliance.**

| Artifact | Rev 1 | Rev 2 | Evidence |
|---|---|---|---|
| LittleFS source under `firmware/vendor/` | No | **Yes** | `firmware/vendor/littlefs/` — `lfs.c`, `lfs.h`, `lfs_util.c`, `lfs_util.h`, `LICENSE.md`, byte-exact from tag `v2.11.3` |
| A `VERSION` SOUP record | No | **Yes** | `firmware/vendor/littlefs/VERSION` — tag, commit, licence, per-file SHA-256, configuration, and an explicit "what this does not establish" |
| Any call into the library | No | **Still no** | `OI-LOG-05..07` are unimplemented. The only non-vendor `lfs_` references are NeurOne's configuration and its test |
| A NeurOne test exercising it | No | **Still no, in the sense that matters** | `np_lfs_config_tests` exercises the SOUP record, the build configuration and the instance parameters. It exercises **no filesystem behaviour** — there is no block device on a host |
| A build that links it | No | **Partly** | `np_littlefs` builds in both modes and `np_hub_control` links it, so the pinned bytes compile under NeurOne's configuration. Nothing in the image calls it |

**The third row is the one to read.** Pinning moved the component from *nominated* to *present*; it
did not move it to *integrated*. Every claim in §3 is still a claim about a component no NeurOne code
calls, and §5's hazard analysis is still an analysis of callers that do not yet exist. What changed
is that the analysis now has a specific version under it rather than a version family.

**One finding came out of the build itself**, and it is recorded here because it is the kind that is
invisible until something compiles: NeurOne's configuration originally set `LFS_NO_ASSERT` alongside
a replacement `LFS_ASSERT`, reading the define as *"do not include `<assert.h>`"*. Upstream reads it
as *"assertions are compiled out"* — `lfs.c:507` guards the **definition** of `lfs_mlist_isopen` with
it while `lfs.c:6178` keeps the **call** inside `LFS_ASSERT`. The result was an implicit declaration
and an undefined symbol that appears only once something pulls `lfs.o` into a link. Patching vendored
SOUP is not available (`NP-SW-CI-001` §9), so the configuration yielded. **What that assertion checks
is the point**: that a file is not already open through a second handle — the exact condition behind
the data-corruption defect `v2.11.3` was pinned for (§7.4). Compiling it out would have removed the
run-time detector for the failure the version choice was made against.

---

## 3. What NeurOne's specifications require of it

Collected here because the hazard analysis is an analysis of *these claims*, not of the library in
the abstract.

| # | Claim | Source | What it requires of LittleFS |
|---|---|---|---|
| L-1 | A flush commits the exact buffered tail and issues a block-device sync; at most the records appended since the last flush are lost on power loss | `NP-FW-HUB-001` §6.5 | `lfs_file_sync` is durable, and a torn append does not damage previously synced bytes |
| L-2 | The log file is append-mode, so a flush never rewrites a previously committed byte | `NP-FW-HUB-001` §6.5 | append does not rewrite earlier file content |
| L-3 | A write never destroys the live inventory record before its replacement is durable | `NP-FW-NVRAM-001` §4.2, §14 | atomic file replace, or copy-on-write commit ordering |
| L-4 | A torn Map 3 write costs one record, not all of them | `NP-FW-NVRAM-001` §4.2, §14 | per-record self-checking survives a partial program; the filesystem does not lose the whole file |
| L-5 | Config's instance is `block_size` 4,096, `block_count` 4,096, `prog_size` 256, `file_max` 65,536 | `NP-FW-EMMC-001` `EMMC-FS-01` | configurability, and that these are the operative bounds |
| L-6 | Wear is levelled across the partition within the 30,000 P/E budget | `EMMC-HW-01`, `NP-FW-NVRAM-001` §3.5 | dynamic wear levelling |
| L-7 | Bytes are encrypted at rest without the filesystem seeing keys | `NP-FW-EMMC-002` §C, `NP-FW-HUB-001` §6.5 | operates over a block device; is agnostic to XTS below it |

L-1 through L-4 are the ones that carry safety-adjacent weight. L-5 through L-7 are configuration and
layering facts that fail loudly if wrong.

---

## 4. Hazard analysis — method

Per `NP-SOUP-CMSIS-001` §2, the question is not "is the component good" but **"can a defect in it
reach a hazardous situation, and by what path."**

For a Class B item the IEC 62304 §7.1.2 anomaly-list obligation does not formally attach — the
precedent is `NP-SW-001` §9.4's Class B SOUP note, where the MCUX row's argument is **reachability**:
the component is compiled into `np_application` and into nothing else. **That argument is not
available here**, and this is the reason this document exists rather than a one-line note. MCUX is a
device header layer whose failure mode is a wrong register address — loud, and confined to SW-02.
LittleFS carries *integrity guarantees for stored values that other code later trusts*. Its failure
mode is quiet, and the question is what trusts those values.

So the method is: enumerate every consumer of a LittleFS-stored value, and for each ask whether a
silently-wrong value can influence an emission.

---

## 5. The three consumer families

### 5.1 Session log — UHDR and SHDR records

Written by `np_log_backend.c`; read by the app on Mode 4 download and by nothing on the device.

**A filesystem anomaly here loses or corrupts records. It cannot influence an emission**, because
nothing in the stimulation path reads the log back. `NP-FW-HUB-001` §6.5 already bounds the loss to
one flush interval.

**Not harm; still a duty.** UHDR is the user's property (CLAUDE.md §5) and is a design record under
`21 CFR §820.180` retention when it feeds a complaint investigation. Silent truncation of a session
record is a **records-integrity** issue, and the correct register for it is the risk file, not this
one. `RISK-LFS-01`, severity Low, mitigated by the flush bound and by per-record tags (`NP-FW-HUB-001`
§6.1) that make a truncated record detectable rather than plausible.

### 5.2 UID-keyed dose calibration

Stored in the Config partition; read at session setup.

**Fails closed by construction.** `NP-FW-NVRAM-001` §14 records the rule: UID-keyed calibration
**refuses to fall back**, exactly as `np_hex_addr_pack()` refuses to mask. A corrupted or unreadable
calibration record yields *no* calibration, and the module is not driven — it does not yield a
**default** calibration, which is the variant that would be dangerous.

The cost of this failing is commercial rather than clinical, and `NP-FW-NVRAM-001` §10.2 quantifies
it: without persistence, calibration falls back to firmware defaults on every boot, rendering inert
the $11.53/tile driver-plus-metering line — $346 per headset at 30 tiles. **That is a differentiator
loss, not a hazard**, and conflating the two would misrank this item.

### 5.3 The `"NPMP"` module-map blob — and this is the one

The blob carries, per socket, the module's UID, its element inventory, and — through Map 1 — the
**per-element safe ranges**. A range is an input to drive magnitude.

So the path exists: *filesystem anomaly → silently-wrong stored range → wrong emitter drive.*

Three things stand between it and a hazardous situation, and they must be stated in order because
only the first is a property of this component:

1. **A CRC over the whole blob.** `blob(n) = HDR(8) + n × 175 + CRC(4)`. A corrupted blob fails its
   CRC.
2. **Reject-and-rebuild.** On CRC failure `np_module_map` **discards the blob entirely** and
   re-enumerates from the sockets. This is the claim `NP-FW-NVRAM-001` §9 turns its Class B argument
   on: *"every failure path leaves the inventory empty."* An empty inventory drives nothing.
3. **The 62 °C junction throttle**, which is the only bound left if 1 and 2 are ever both defeated —
   and it is a **thermal limit standing in for an optical one**. That is `OI-NVRAM-10`, and it is
   carried, not closed.

**Conclusion for §5.3:** a LittleFS anomaly cannot reach an emission *while step 2 holds*, because
step 2 converts every integrity failure into an absence rather than a wrong value. §6.2 is about what
happens when step 2 stops holding.

---

## 6. Conclusions

### 6.1 The classification stands

**SW-02 Class B is correct for this component.** The Class C processor has no electrical path to the
eMMC (`NP-FW-NVRAM-001` §9); no stored value can widen `granted_mask` (`NP-FW-HUB-001` §7.2 — the
safety MCU grants, and its limits are resident constants); and the one consumer family that touches
drive magnitude converts every integrity failure into an empty inventory (§5.3).

### 6.2 But the argument is not a property of LittleFS, and Map 3 is specified to break its premise

This is the finding this document exists for.

Step 2 of §5.3 — reject-and-rebuild — is a property of **`np_module_map`**, not of the filesystem.
The Class B argument is therefore **conditional on a caller policy**, and `NP-FW-NVRAM-001` §7.2
already establishes, for entirely independent reasons, that **Map 3 must have the opposite policy**:

> *"`np_module_map`'s discard-and-rebuild is sound because the blob is a **cache** of facts the
> sockets will re-answer. Map 3 is a **record** of facts nothing will re-answer. Putting them in one
> blob converts a correct policy into data loss."*

Map 3 is consequently specified as **tail-additive forever, never discard-and-rebuild** — separate
file, own magic, own version, per-record self-checking so a torn write costs one record rather than
all of them (`NP-FW-NVRAM-001` §4.2, D-5).

**Both decisions are right, and together they make the classification argument load-bearing on which
file is which.** The rule that falls out is a requirement, not an observation:

> **`REQ-LFS-01` — No value that bounds an emission may be stored under a tail-additive policy.**
> Safe ranges, calibration and inventory stay in the reject-and-rebuild cache. Map 3's journal
> carries history — ordinals, counts, totals — and **must never become the source of a limit.** If it
> ever does, §5.3 step 2 no longer applies to that value, this classification must be re-derived, and
> `OI-NVRAM-10`'s thermal-limit-standing-in-for-an-optical-one residual becomes the *only* remaining
> bound rather than the third of three.

`OI-LFS-03` asks for the CI check that makes `REQ-LFS-01` observable rather than remembered.

### 6.3 What the old verification cell was worth

*"Power-loss testing per LittleFS test suite"* asserts that an upstream project tests its own code.
It records nothing NeurOne did, names no version, and would read as green in an audit. Replacing it
is the single highest-value change in this document, and §7.2 states what replaces it.

---

## 7. What remains, and why §7.1.2 is *still* not discharged here

### 7.1 Pin a version — blocking, and blocking for two reasons *(Rev 1; performed at Rev 2 — see §7.4)*

`2.x` cannot be evaluated and cannot be vendored. A named upstream release tag must be selected at
integration and recorded in `firmware/vendor/littlefs/VERSION` with per-file provenance and SHA-256,
following the `firmware/vendor/cmsis_core/VERSION` pattern.

**No version is pinned in this revision, deliberately.** Pinning a tag whose anomaly list has not
been read, in a document whose purpose is to record that the anomaly list was read, would reproduce
`OI-DOC-01` in the file written to close its sibling. `OI-LFS-01`.

The second reason it blocks: **#75 (`NP-SBOM-001`) needs a version.** An SBOM entry reading `2.x` is
not an SBOM entry, and the T2 510(k) cybersecurity submission consumes it.

> **Rev 2: performed, and the Rev 1 reasoning above is *outweighed*, not refuted** (`NP-CONV-001`
> §7). It remains true that a pinned tag with an unread anomaly list is not a discharged §7.1.2. What
> Rev 1 got wrong is the order it implied: it read "do not pin before the list is read" as a rule,
> when §7.2 immediately states the opposite dependency — **the list cannot be read until a tag
> exists.** Holding the pin therefore blocked its own precondition, and blocked `#75` besides.
> Rev 2 pins the tag and records the evaluation's *absence* explicitly (in `VERSION`, in §7.5, in
> `NP-SW-001` §9.4, and in `OI-LFS-02`) rather than concealing it behind a missing version. The
> `OI-DOC-01` failure Rev 1 feared is "a record that resolves to nothing"; a stated non-conclusion
> resolves to something.

### 7.2 Then perform the evaluation, and record NeurOne's own verification

Once a tag is pinned, §7.1.2's activity is: read the published anomaly list **for that tag**, assess
each item against the seven claims in §3, and record the assessment — the `NP-SOUP-CMSIS-001` §3–§4
shape. The verification cell then records what NeurOne ran, which must include at minimum a
**power-loss injection test against L-1…L-4**: interrupt a program operation at each of a set of
offsets and assert that the previously synced prefix is intact and that at most one record is lost.

Per `NP-CONV-001` §8 that test must be **falsified before it is trusted** — perturb the commit
ordering and confirm it fails — because a power-loss test that has never been seen to fail is
indistinguishable from a test that does not run. `OI-LFS-02`.

### 7.3 Configuration is part of the SOUP record *(Rev 1; performed at Rev 2 — see §7.4)*

The vendored subset and its configuration must be recorded together, as the FreeRTOS row does. At
minimum: static allocation (no `malloc` on a device whose heap is a fixed 64 KiB FreeRTOS pool),
`block_cycles` set explicitly rather than defaulted (it governs wear levelling, which is L-6), and
the `EMMC-FS-01` instance parameters (L-5) asserted at mount rather than assumed.

### 7.4 What Rev 2 performed — `OI-LFS-01` closed (2026-09-14)

**The tag: `v2.11.3`**, the newest release on the v2 line (tag commit `6cb4e865`, 2026-03-24).
"Newest" is the choice a reviewer is most likely to second-guess, so the reason is on the record and
not left to be inferred: **v2.11.3 contains upstream commit `488e84bb`, "Fixed data corruption with
multiple write handles"** — a sync through one handle left a second open handle on the same file
stale, after which the allocator could re-use still-referenced blocks. That is a data-corruption
defect on exactly the axis `L-1…L-4` rest on, and it is in no earlier v2 release. Any older tag would
be knowingly pinning a version carrying it. The remaining ten commits since v2.11.2 are a
null-callback guard, implicit-conversion warning fixes and README typos; the on-disk version (2.1) is
unchanged, so the pin carries no format migration.

**That defect also leaves a requirement behind, and it outlives the version choice.** No file on any
NeurOne instance may be open through two handles at once. The three Config files (the `"NPMP"` blob,
Map 3's journal, `ukmd.rec`) and the two log files each have exactly one writer *by design* — which
means it is currently true by accident of how the design happens to read, not by anything that would
notice if it stopped. It belongs to the `OI-LOG-05..07` glue, and it is why §2.1's `LFS_NO_ASSERT`
finding matters: `lfs_mlist_isopen` is the run-time detector for precisely this condition, and
NeurOne's first configuration would have compiled it out.

**The subset**: `lfs.c`, `lfs.h`, `lfs_util.c`, `lfs_util.h`, `LICENSE.md` — byte-exact, verified by
two independent clones and a per-file comparison, with per-file SHA-256 in
`firmware/vendor/littlefs/VERSION` so the claim stays checkable offline. Upstream's `bd/` test block
devices, `tests/`, `benches/`, `runners/` and `scripts/` are **deliberately not vendored**, and the
second of those is worth naming: the verification cell this whole document replaced cited upstream's
own test suite as NeurOne's verification. Vendoring that suite would make the citation look
discharged without anything having been run.

**The configuration** (§7.3's three minimums, and what else was needed to satisfy them):

| §7.3 asks for | In force | Where |
|---|---|---|
| static allocation | `LFS_NO_MALLOC`; all three buffers static, 1,024 B; `lfs_file_open()` unusable by construction, `lfs_file_opencfg()` only | `np_lfs_config.h`, `np_lfs_instance.c` |
| `block_cycles` explicit | **500**, and checked *by value* — `-1` passes every "non-zero" test and silently disables the wear levelling `L-6` rests on | `np_lfs_instance.h` |
| `EMMC-FS-01` parameters asserted at mount | `np_lfs_config_validate()`, plus eight `_Static_assert`s | `np_lfs_instance.c` |

Two further decisions the §7.3 list did not anticipate. **Assertions stay on** (`LFS_ASSERT`
retargeted to a handler that halts the main processor, so the safety MCU's 1.5 s watchdog cuts all
stimulation — the `np_freertos_assert_failed` discipline): `lfs_init()`'s configuration checks *are*
assertions, and a Class B filesystem compiled with its own invariants off converts a
misconfiguration from a halt into undefined behaviour over stored values other code trusts. And
**`LFS_THREADSAFE` is on**, because the Config instance already has three specified writers whose
callers are not one task; the alternative is a single-task assumption recorded nowhere and checkable
nowhere, which is the shape of failure this document exists because of.

**Why the validator exists at all, since littlefs validates its own config.** `lfs_init()`'s
assertions check that a configuration is *internally consistent*. They cannot check that it is
*NeurOne's*, because they have never heard of `EMMC-FS-01`: a config with `block_size` 512 and
`block_count` 32,768 passes every one of them and mounts a filesystem whose geometry no NeurOne
document describes. `np_lfs_config_validate()` is that missing half, and `L-5` is the claim it
discharges.

**`np_lfs_config_tests`** (Class B host target 28; repo total 34 → 35) holds all of it in place:
it recomputes each vendored file's SHA-256 against the `VERSION` record, compares the build-time
configuration against what that record tells a reviewer to rely on, pins the instance parameters, and
**requires nineteen single-field perturbations of a known-good config to be rejected**. Per
`NP-CONV-001` §8 the suite was falsified before being trusted — a corrupted recorded hash, an edited
vendored byte, `block_cycles` `-1` and `100`, a `lookahead_size` of 16, a hole punched in the
validator and `LFS_THREADSAFE` removed were each confirmed to fail it, and the baseline confirmed to
pass again afterwards. The `LFS_NO_ASSERT` finding in §2.1 was itself found this way.

### 7.5 What a reviewer may and may not cite Rev 2 for

Stated as a list because the failure mode here is optimistic reading, and because a directory under
`firmware/vendor/` looks like an answer.

**May be cited for:** a configuration identity for `#75` (`NP-SBOM-001`) — an SBOM entry can now name
`v2.11.3` and a commit; that the vendored bytes are the upstream tag's; that NeurOne's configuration
of the component is recorded, compiled and tested; that `L-5` is enforced rather than assumed.

**May NOT be cited for — and this is unchanged from Rev 1:**

- **`L-1`, `L-2`, `L-3`, `L-4`.** No power-loss behaviour has been exercised. `NP-FW-NVRAM-001` §4,
  `EMMC-FS-01` and `NP-FW-HUB-001` §6.5 remain **specified and unverified**. `OI-LFS-02`.
- **IEC 62304 §7.1.2.** The published anomaly list for `v2.11.3` has not been read or assessed
  against §3's seven claims. It is now *performable*, which is the whole of what pinning bought.
- **Integration.** Nothing calls the library (§2.1). `np_lfs_config_tests` tests the record and the
  configuration; it mounts nothing, because a host has no block device.
- **The UHDR and SHDR log instances.** Only the Config instance has stated parameters. `L-1` and
  `L-2` are claims about the *log* partitions, whose instance parameters no document states —
  `OI-LFS-05`. `np_lfs_instance.h` must not be reused for them by analogy: `block_count` alone
  differs by three orders of magnitude, which changes what `lookahead_size` can mean.

---

## 8. Risk rows

| ID | Hazard | Sev | Mitigation | Residual |
|---|---|---|---|---|
| `RISK-LFS-01` | Session records silently truncated or lost | Low | flush bound (`NP-FW-HUB-001` §6.5); per-record tags make truncation detectable | Accepted |
| `RISK-LFS-02` | Corrupted safe range reaches emitter drive | **High** | blob CRC → reject-and-rebuild → empty inventory (§5.3); 62 °C throttle as the last bound | **Conditional** on `REQ-LFS-01`; re-derive if it is ever breached |
| `RISK-LFS-03` | Atomicity claims relied on before the component is **verified** | Medium | this document; `OI-LFS-02` blocks the reliance, not the design. **Rev 2 narrowed the hazard and did not remove it** — the component now exists, which removes the "before it exists" half and makes the remaining half easier to misread, since a populated `firmware/vendor/littlefs/` looks like an answer. §7.5 is the mitigation for that reading | **Open until `OI-LFS-02`** |
| `RISK-LFS-04` | An unpinned version reaches the SBOM and the 510(k) submission | Medium | ~~`OI-LFS-01` blocks #75's entry~~ — **CLOSED at Rev 2.** `v2.11.3` + commit `6cb4e865` + per-file SHA-256 is an SBOM entry | **Closed 2026-09-14** |
| `RISK-LFS-05` | The log partitions are mounted with parameters nobody chose — a `block_count` three orders of magnitude larger than Config's, against a `lookahead_size` sized for Config | Medium | `OI-LFS-05`; `np_lfs_instance.h` is scoped to the Config instance in its own header and refuses to generalise | **Open** |

---

## 9. Open items

| ID | Description | Owner | Blocking |
|---|---|---|---|
| ~~`OI-LFS-01`~~ | **✅ CLOSED 2026-09-14 (Rev 2) — littlefs `v2.11.3` pinned and vendored** at `firmware/vendor/littlefs/`: byte-exact five-file subset verified by two independent downloads, per-file SHA-256, the configuration of §7.3 recorded *and* compiled *and* tested (`np_lfs_config_tests`, 19 falsified rejections), and an explicit "what this does not establish". §7.4. **Closing it unblocks #75 and unblocks `OI-LFS-02`; it unblocks no atomicity claim** — see §7.5, and note that the item below inherits the BLOCKING status this one carried | — (closed) | — |
| **`OI-LFS-02`** | **Perform the §7.1.2 anomaly evaluation against the pinned tag** — `v2.11.3` is now selectable, which is what `OI-LFS-01` bought — and write the NeurOne power-loss injection test against `L-1…L-4`, **falsified in both directions first** per `NP-CONV-001` §8. Supersedes the evaluation half of `OI-NVRAM-12`, **and inherits `OI-LFS-01`'s blocking status** | FW + Quality | **BLOCKING — any reliance on `NP-FW-NVRAM-001` §4, `EMMC-FS-01` or `NP-FW-HUB-001` §6.5.** `NP-SW-001` §9.4 verification cell; G2 |
| **`OI-LFS-03`** | **Write and falsify the CI check for `REQ-LFS-01`** — that no value bounding an emission is stored under a tail-additive policy, i.e. that Map 3's journal is never read as a limit. `warranty-nojoin-ci.yml` is the pattern | FW + Safety | `REQ-LFS-01` durability; §6.2 |
| **`OI-LFS-04`** | `OI-NVRAM-10` — is there a Class C bound on per-tile emitter drive current independent of Map 1's ranges? Carried here because §5.3's third barrier is a thermal limit standing in for an optical one, and this document is where that now has a named consequence | Safety + EE | **Class B classification durability** |
| **`OI-LFS-05`** | **Decide the UHDR and SHDR log instances' parameters.** `EMMC-FS-01` states parameters for the **Config** partition only, and `L-1`/`L-2` — the two claims `NP-FW-HUB-001` §6.5's whole durability model rests on — are claims about the **log** partitions. SHDR is 512 MiB and UHDR 6,903 MiB, so at `block_size` 4,096 their `block_count` is 131,072 and 1,767,168 against Config's 4,096: a full-coverage `lookahead_size` would be 16 KiB and 216 KiB respectively, which is a real decision and not a copy of Config's 512 B. Also decide `file_max` for an append-only log, where Config's 65,536 is plainly wrong. Raised by Rev 2 while writing the Config instance; `np_lfs_instance.h` is deliberately scoped so it cannot be reused for them by analogy | FW | **`OI-LOG-05..07` (the mount glue); `NP-FW-HUB-001` §6.5** |
| **`OI-LFS-06`** | **Make "one open handle per file" checkable.** `v2.11.3` was pinned partly for upstream `488e84bb`, which fixes corruption arising from two write handles on one file (§7.4). NeurOne's five files each have one writer by design, but nothing enforces or observes it; `lfs_mlist_isopen` catches it at run time only, and only because §2.1's `LFS_NO_ASSERT` finding was caught. Belongs with the `OI-LOG-05..07` glue — a single open-handle registry, or the `warranty-nojoin-ci.yml`-shaped check `OI-LFS-03` already establishes the pattern for | FW | `OI-LOG-05..07` |

---

## 10. Revision history

| Rev | Date | Author | Description |
|---|---|---|---|
| 2 | 2026-09-14 | NeurOne Firmware Engineering | **Closes `OI-LFS-01`: littlefs is pinned at `v2.11.3` and vendored, with its configuration.** Five byte-exact files at `firmware/vendor/littlefs/` from tag commit `6cb4e865`, verified by two independent downloads and carrying per-file SHA-256; a `VERSION` SOUP record on the `cmsis_core` pattern; `np_littlefs` building in both modes. **The tag choice is a safety argument, not a preference**: `v2.11.3` is the first release containing upstream `488e84bb`, which fixes data corruption from two write handles on one file — a defect on exactly the axis `L-1…L-4` rest on — and it leaves behind a NeurOne requirement (one open handle per file, `OI-LFS-06`). §7.3's configuration is recorded, compiled and tested: `LFS_NO_MALLOC` with static buffers, `block_cycles` **500** checked by value because `-1` passes every non-zero test and silently disables `L-6`, `LFS_THREADSAFE` on because the Config instance has three specified writers, assertions retargeted to the `np_freertos_assert_failed` halt, and `np_lfs_config_validate()` as the mount-time check `L-5` names — **littlefs's own asserts check that a config is self-consistent and cannot check that it is NeurOne's.** `np_lfs_config_tests` (Class B 27 → 28, repo 34 → 35) re-derives the SHA-256s, compares the build configuration against the SOUP record, and requires 19 single-field perturbations to be rejected; it was falsified in seven ways first per `NP-CONV-001` §8. **One finding came out of building it**: setting `LFS_NO_ASSERT` beside a replacement `LFS_ASSERT` removes the *definition* of `lfs_mlist_isopen` while keeping the *call*, and that assertion is the run-time detector for the very corruption this tag was pinned for. **Rev 1's §7.1 reasoning is outweighed, not refuted** — "do not pin before the anomaly list is read" blocked its own precondition, since the list cannot be read without a tag; the absence of the evaluation is now recorded explicitly instead of being concealed behind a missing version. **`OI-LFS-02` inherits the BLOCKING status**: `NP-FW-NVRAM-001` §4, `EMMC-FS-01` and `NP-FW-HUB-001` §6.5 are exactly as unverified as at Rev 1, and §7.5 exists because a populated vendor directory reads as an answer. `RISK-LFS-04` closed, `RISK-LFS-03` narrowed, `RISK-LFS-05` added. Two new open items: `OI-LFS-05` (the log partitions have no stated instance parameters, and they are the ones `L-1`/`L-2` are about) and `OI-LFS-06`. Feeds #75 (`NP-SBOM-001`) — which an SBOM entry can now satisfy. Rev 1 → 2. |
| 1 | 2026-09-13 | NeurOne Firmware Engineering | **Initial release — brings LittleFS under SOUP management and performs the hazard analysis `OI-NVRAM-12` requires (Issue #339).** Replaces `NP-SW-001` §9.4's single cell (*"2.x"* / *"Power-loss testing per LittleFS test suite"*). **Principal finding: LittleFS is not unmanaged, it is absent** — `grep -rn "lfs_" firmware/` returns two hits, both comments in `np_log_backend.h` naming the `OI-LOG-06/07` seams, so every power-loss atomicity guarantee in `NP-FW-NVRAM-001` §4, `EMMC-FS-01` and `NP-FW-HUB-001` §6.5 rests on a component with no source, no version and no NeurOne test. **Second finding, and the one that outlives integration: the Class B classification is not a property of LittleFS but of `np_module_map`'s reject-and-rebuild policy** — every integrity failure becomes an *empty* inventory rather than a *wrong* one — and `NP-FW-NVRAM-001` §7.2 specifies Map 3 to need the opposite, tail-additive policy for independently correct reasons. `REQ-LFS-01` is the rule that keeps both decisions safe: no value that bounds an emission may be stored under a tail-additive policy. **The §7.1.2 anomaly evaluation is deliberately not performed** — it evaluates the list for the version in use and none is in use; recording a conclusion against *"2.x"* would reproduce `OI-DOC-01` inside the document written to close its sibling. Four risk rows, four open items, two of which supersede the halves of `OI-NVRAM-12`. Feeds #75 (`NP-SBOM-001`). |
