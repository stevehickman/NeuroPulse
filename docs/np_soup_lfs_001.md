# LittleFS SOUP Record and Hazard Analysis — Hub Storage (SW-02, Class B)

**Project:** NeurOne
**Document:** NP-SOUP-LFS-001
**Revision:** 9
**Date:** 2026-09-24
**Status:** DRAFT — the hazard analysis (§4–§6) is complete and does not depend on a version. **Rev 9 closes `OI-LFS-12` (§13.13): the device session count is persisted in the Config partition as a replicated record, committed before each session's UHDR file is created.** **Rev 8 narrows the last two items (§13.11, §13.12): `OI-LFS-07` gets its host-decidable half — `erase()` may be a no-op on the eMMC, shown by sweep — and a silicon bring-up protocol with pass criteria; `OI-LFS-04` gets an answer — there is no Class C bound on per-tile drive magnitude — and the options that would create one, for Safety + EE to choose.** **Rev 7 closes `OI-LFS-11` (§13.10): UHDR logs are one file per session, as `EMMC-UHDR-12`/`-13` specify, so no file approaches littlefs's 2 GiB `file_max`; SHDR stays one file because its whole partition is 512 MiB.** **Rev 6 closes `OI-LFS-10` and applies `ECR-EMMC-002` (§13.9): the Config instance takes `EMMC-FS-01`'s remaining values, and `NP-FW-EMMC-001` Rev 3 now prints `read_size`/`prog_size` 512 in all three columns — so all three littlefs instances match their specification exactly.** **Rev 5 decides the first half of `OI-LFS-10`: the Config instance moves to `read_size`/`prog_size` 512, the XTS data unit, and `cache_size` follows to 512 (§13.8); `RISK-LFS-08` closed.** **Rev 4 builds the caller rules §11 asked for and closes five items (§13): `OI-LFS-03`, `-05`, `-06`, `-08`, `-09`.** The Config store (`np_cfg_store`) makes each rule a property of its API, a CI gate makes going around it fail, the UHDR/SHDR instances have parameters and a validator, and upstream #1205 is reproduced on `v2.11.3` and shown not to reach a caller. It also finds that `EMMC-FS-01` states every parameter Rev 2 said it did not, and that a `prog_size` below the 512-byte XTS unit loses committed data — which is the Config instance as built (`OI-LFS-10`). `OI-LFS-07` (the eMMC) and `OI-LFS-04` (Safety + EE) stay open. **Rev 3 closed `OI-LFS-02`: the IEC 62304 §7.1.2 anomaly evaluation is performed (§11) and the NeurOne power-loss injection test against `L-1…L-4` is written, run and falsified in both directions (§12).** `L-1…L-4` may now be relied on **against the `struct lfs_config` contract** — which is what they were stated against — and not against the eMMC beneath it, which no host test can reach and which is raised as `OI-LFS-07`. Rev 2 closed `OI-LFS-01` (littlefs `v2.11.3` pinned and vendored at `firmware/vendor/littlefs/`); Rev 1 performed the hazard analysis.
**Effective Date:** —
**Author:** NeurOne Firmware Engineering
**Approved By:** — (DRAFT, not approved)
**References:** `NP-SW-001` Rev 6 §9.4 (SOUP register — this record replaces its LittleFS row, and its second Class B SOUP note carries this document's conclusion); `NP-SOUP-CMSIS-001` Rev 1 (method precedent); `NP-FW-EMMC-001` Rev 2 `EMMC-FS-01` (instance parameters), `EMMC-CFG-01/-02` (Config contents and write whitelist), `EMMC-HW-01` (endurance); `NP-FW-EMMC-002` Rev 2 §C (UHDR mount); `NP-FW-NVRAM-001` Rev 2 §3.3, §4 (power-loss atomicity), §9 (Class B argument); `NP-FW-HUB-001` Rev 1 §6.5 (log durability model); `NP-CONV-001` Rev 6 §8 (a check must be falsified before it is trusted); `firmware/hub_control/include/np_log_backend.h`
**Related Issues:** #339 (`OI-NVRAM-12`), #75 (`NP-SBOM-001` — T2 510(k) cybersecurity submission), #382 (Rev 4 — `OI-LFS-03…09`), #340 (the UHDR key and scratch HAL the store will back)
**Gate:** G2
**IEC 62304 Class:** SW-02 Class B
**Supersedes:** None — new document. It replaces the single verification cell that `NP-SW-001` §9.4 previously carried for this component.
**Pinned version:** littlefs **v2.11.3** (tag commit `6cb4e86540eca0d9ba62500a298385c9d863c8be`), vendored at `firmware/vendor/littlefs/` with per-file SHA-256 — `firmware/vendor/littlefs/VERSION` is the SOUP record proper, and this document is its hazard analysis.
**Review Cadence:** On any change to the pinned version, on first integration, and at G2. §11 is re-run in full on any tag change — a §7.1.2 evaluation is a statement about one version and carries forward to no other.

---

> **⚠ REV 9 (2026-09-24) — `OI-LFS-12` CLOSED (§13.13).** The device session count is a Config file
> (`NP_CFG_FILE_SESSION_COUNT`, `REPLICATED`), committed at each session start *before* that session's
> UHDR file is created, and read at boot. The platform seam that was supposed to supply it
> (`np_hal_get_device_session_count()`) is retired. **The items still open in this document are
> `OI-LFS-07` (silicon) and `OI-LFS-04` (a Safety + EE choice).**

---

> **⚠ REV 8 (2026-09-24) — `OI-LFS-07` and `OI-LFS-04` narrowed, not closed (§13.11, §13.12).**
> Both need things this repository cannot supply — silicon, and a hardware decision. What changed is
> that each now says exactly what is left. **`OI-LFS-07`:** NeurOne's `erase()` on the eMMC is a
> **no-op** (littlefs: *"the state of an erased block is undefined"*; swept on the host with an erase
> that never touches the medium, 0 violations); `block_cycles` stays as specified; and
> `HW-LFS-01…05` are the bring-up tests, with pass criteria. **`OI-LFS-04`:** the answer to *"is
> there a Class C bound on per-tile drive current independent of Map 1?"* is **no** — the safety MCU
> owns the PBM enable line and its thermal cut, nothing else; and the tile's drive stage is specified
> two incompatible ways (`OI-HEXTILE-23`).

---

> **⚠ REV 7 (2026-09-24) — `OI-LFS-11` CLOSED (§13.10).** The log backend's *"the append-mode log
> file"* per partition becomes one UHDR file per session, created exclusively under
> `/uhdr/sessions/<count>`; SHDR keeps its single file. **The open items remaining in this document
> are `OI-LFS-07` (hardware) and `OI-LFS-04` (Safety + EE), plus `OI-LFS-12`, raised here** — the
> device session count is not persisted (`EMMC-SHDR-09`), which the logger now survives but does not
> fix.

---

> **⚠ REV 6 (2026-09-24) — `OI-LFS-10` CLOSED, `ECR-EMMC-002` APPLIED (§13.9).** The Config
> instance takes `EMMC-FS-01`'s `lookahead_size` 64, `block_cycles` 200, `name_max` 64, `attr_max`
> 256 and `metadata_max` 4,096; and `NP-FW-EMMC-001` is amended to Rev 3 so its table prints 512
> for `read_size`/`prog_size` in the UHDR, SHDR and Config columns. **Every "deviation" in §13.1 and
> §13.8 is therefore no longer one** — the text is kept as written, and "deviation" there means
> "from Rev 2 of the specification".

---

> **⚠ REV 5 (2026-09-24) — the Config instance moves to 512-byte programs (§13.8).** The principal
> decided `OI-LFS-10`'s hazard half: `read_size`/`prog_size` 512, `cache_size` 512 (forced by
> `lfs_init()`, and `EMMC-FS-01`'s own value). Rev 4's first warning below — *"the Config instance
> has a specific reason not to survive its medium"* — is **resolved, not refuted**: that reason is
> gone, and the sweep that found it now passes on the instance as built. **Every Config figure in
> §12 and §13 was obtained at 256** and is kept as the record of that configuration; §13.8 gives the
> re-run figures at 512. `lookahead_size`, `block_cycles` and `name_max`/`attr_max` still differ from
> `EMMC-FS-01` and remain `OI-LFS-10`'s open half.

---

> **⚠ REV 4 — WHAT CHANGED, AND WHAT DID NOT (2026-09-24, GitHub #382).**
>
> | | Rev 3 | Rev 4 | Where |
> |---|---|---|---|
> | What stops the applicable anomalies | caller rules that do not exist | **`np_cfg_store` — each rule a property of its API; #1205 reproduced and stopped** | §13.2–§13.4 |
> | `REQ-LFS-01` | remembered | **enforced in CI (`check-lfs-caller-rules.ts`, R4–R6)** | §13.5 |
> | One handle per file | true by accident | **a registry, and a gate that forbids going around it** | §13.3 |
> | Log instances (`L-1`, `L-2`) | no parameters | **`EMMC-FS-01`'s, one deviation, swept on the real 1,767,168-block geometry** | §13.1 |
> | `EMMC-FS-01`'s Config column | read as silent on four fields | **states them all, and the code disagrees on five** | §13.1.2, `OI-LFS-10` |
>
> **Two things a reader must not take from Rev 4.**
>
> **First: the Config instance has a specific reason not to survive its medium.** A 256-byte program
> into a 512-byte XTS unit is a read-modify-write of bytes littlefs already committed; the §13.1.3
> sweep loses a durable Map 3 record under that model. §12's and §13's Config results still hold
> against the `lfs_config` contract, which is all they ever claimed — but `OI-LFS-10` must be decided
> before they are cited against an encrypted partition.
>
> **Second: nothing is integrated.** The store and the log instances are tested code that no path
> in the image calls; the block device is still `OI-LOG-05..07`. And #1210 was **not reproduced**,
> so the store is shown bounding its trigger and replicating its worst victim — not preventing it.
>
> **The Rev 1–3 banners below are retained verbatim** (`NP-CONV-001` §7).

---

> **⚠ REV 3 — WHAT CHANGED, AND WHAT DID NOT (2026-09-14, closes `OI-LFS-02`).**
>
> | | Rev 2 | Rev 3 | Cause |
> |---|---|---|---|
> | §7.1.2 evaluation | performable, not performed | **performed — §11** | `OI-LFS-02` |
> | `L-1…L-4` | asserted, never exercised | **swept under 456 interrupted runs, 0 violations — §12** | ditto |
> | Reliance on `NP-FW-NVRAM-001` §4, `EMMC-FS-01`, `NP-FW-HUB-001` §6.5 | blocked | **unblocked against the `lfs_config` contract; still open against the eMMC** | `OI-LFS-07` |
> | What stops the applicable anomalies | — | **caller rules that do not exist yet** | `OI-LFS-08`, `OI-LFS-09` |
>
> **Two things a reader must not take from Rev 3.**
>
> **First: a green test is a statement about what it interrupted.** §12 cuts power inside the
> `struct lfs_config` contract — read/prog/erase/sync at `prog_size` granularity — because that is
> the contract `L-1…L-4` are written against. NeurOne's medium is an eMMC behind an XTS layer whose
> FTL may tear inside a 512 B sector and whose erase semantics are not raw-flash semantics. Upstream
> #1083 asks exactly this question and has no answer. **`OI-LFS-07` is the part of `OI-LFS-02` that
> a host could never have discharged**, and it is raised rather than absorbed.
>
> **Second: §11 did not come back clean, it came back conditional.** Three published anomalies are
> applicable to NeurOne's paths (§11.3). Two of them are stopped by properties of the CALLER —
> which is §6.2's finding arriving a second time by a different route — and those callers are the
> `OI-LOG-05..07` glue, which is unwritten. The evaluation's real output is two new caller rules
> (`OI-LFS-08`, `OI-LFS-09`) beside the standing `REQ-LFS-01`.
>
> **The Rev 1 and Rev 2 banners below are retained verbatim** (`NP-CONV-001` §7).

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

**Verification status as of Rev 3** — the table above is what the specifications *require*; this is
what has been *shown*:

| Claim | Status | Where |
|---|---|---|
| `L-1`, `L-2` | **Exercised.** 90 interrupted runs, 0 violations, against the `lfs_config` contract | §12.2, §12.3 |
| `L-3` | **Exercised.** 198 interrupted runs, 0 violations — and the safe ordering is a *caller* obligation, not a property of the component | §12.2 |
| `L-4` | **Exercised.** 168 interrupted runs, 0 violations | §12.2 |
| `L-5` | Enforced at mount and tested | §7.4, `np_lfs_config_validate()` |
| `L-6` | Configuration only — `block_cycles` 500 is set and checked by value. **Wear levelling itself is not measured by anything**, and on a managed-flash medium it may not mean what the claim assumes (`OI-LFS-07`) | §7.4 |
| `L-7` | Unexercised. The XTS layer is `NP-FW-EMMC-002` §C's and nothing joins the two yet | — |

**None of these is a statement about the eMMC.** §12 interrupts the block-device contract; whether
the medium honours that contract is `OI-LFS-07`.

**Rev 4 additions** (§13): `L-1`/`L-2` are now also swept on the **UHDR and SHDR** instances at their
real geometry (132 runs, 0 violations); `L-3` is re-swept for the **in-place `O_TRUNC`** replacement
the store uses instead of write-temp-then-rename (186 runs, 0 violations); `L-4` is swept **through
the store** (171 runs, 0); `L-5` is enforced for the log instances. And one result against the
table's own premise: under a 512-byte XTS read-modify-write, the Config instance's `prog_size` 256
loses a committed record (§13.1.3, `OI-LFS-10`).

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

## 7. What remained, and how each item was discharged

> **Heading changed at Rev 3.** It read *"What remains, and why §7.1.2 is still not discharged
> here"* through Rev 1 and Rev 2, and that was accurate both times. §7.1 was discharged at Rev 2,
> §7.2 at Rev 3 (§11, §12), §7.3 at Rev 2. The subsection titles carry which revision did what, and
> §7.5 is kept as written with its Rev 3 qualifications marked inline rather than edited away
> (`NP-CONV-001` §7).

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

### 7.2 Then perform the evaluation, and record NeurOne's own verification *(Rev 2; performed at Rev 3 — see §11 and §12)*

Once a tag is pinned, §7.1.2's activity is: read the published anomaly list **for that tag**, assess
each item against the seven claims in §3, and record the assessment — the `NP-SOUP-CMSIS-001` §3–§4
shape. The verification cell then records what NeurOne ran, which must include at minimum a
**power-loss injection test against L-1…L-4**: interrupt a program operation at each of a set of
offsets and assert that the previously synced prefix is intact and that at most one record is lost.

Per `NP-CONV-001` §8 that test must be **falsified before it is trusted** — perturb the commit
ordering and confirm it fails — because a power-loss test that has never been seen to fail is
indistinguishable from a test that does not run. `OI-LFS-02`.

> **Rev 3: both halves performed.** The anomaly evaluation is **§11**; the power-loss injection test
> is **§12**, and §12.4 is its falsification record — six deliberate perturbations, of which five
> were caught and the sixth was not, which is recorded as a negative result rather than dropped.
> The activity this paragraph asks for is done; what it did not anticipate is that a host test can
> only interrupt the *contract*, not the medium (`OI-LFS-07`), and that the evaluation's output
> would be two new **caller** rules rather than a verdict on the component (`OI-LFS-08`,
> `OI-LFS-09`).

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

### 7.5 What a reviewer may and may not cite Rev 2 for *(superseded in part by §11.5 and §12.5)*

Stated as a list because the failure mode here is optimistic reading, and because a directory under
`firmware/vendor/` looks like an answer.

**May be cited for:** a configuration identity for `#75` (`NP-SBOM-001`) — an SBOM entry can now name
`v2.11.3` and a commit; that the vendored bytes are the upstream tag's; that NeurOne's configuration
of the component is recorded, compiled and tested; that `L-5` is enforced rather than assumed.

**May NOT be cited for — and this is unchanged from Rev 1:**

- **`L-1`, `L-2`, `L-3`, `L-4`.** No power-loss behaviour has been exercised. `NP-FW-NVRAM-001` §4,
  `EMMC-FS-01` and `NP-FW-HUB-001` §6.5 remain **specified and unverified**. `OI-LFS-02`.
  *(Rev 3: the first two bullets are the ones that moved. Read §12.5 and §11.5, which state
  precisely how far — against the `lfs_config` contract, not against the eMMC.)*
- **IEC 62304 §7.1.2.** The published anomaly list for `v2.11.3` has not been read or assessed
  against §3's seven claims. It is now *performable*, which is the whole of what pinning bought.
  *(Rev 3: performed — §11.)*
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
| `RISK-LFS-03` | Atomicity claims relied on before the component is **verified** | Medium | this document; `OI-LFS-02` blocks the reliance, not the design. **Rev 2 narrowed the hazard and did not remove it** — the component now exists, which removes the "before it exists" half and makes the remaining half easier to misread, since a populated `firmware/vendor/littlefs/` looks like an answer. §7.5 is the mitigation for that reading. **Rev 3 narrows it a second time and again does not remove it**: `L-1…L-4` are now exercised against the `lfs_config` contract (§12), so what remains is reliance on them *against the eMMC*, which is a different and un-narrowed claim | **Reduced to `RISK-LFS-06` at Rev 3** — the contract half is discharged; the medium half continues as `OI-LFS-07` |
| `RISK-LFS-04` | An unpinned version reaches the SBOM and the 510(k) submission | Medium | ~~`OI-LFS-01` blocks #75's entry~~ — **CLOSED at Rev 2.** `v2.11.3` + commit `6cb4e865` + per-file SHA-256 is an SBOM entry | **Closed 2026-09-14** |
| `RISK-LFS-06` | **The `lfs_config` contract holds and the eMMC does not honour it.** Every `L-1…L-4` result in §12 is conditional on the medium behaving as `struct lfs_config` says a block device behaves — atomic-per-`prog_size` programs, erase-to-`0xFF`, no reordering. An eMMC's FTL is free to tear inside a 512 B sector and to answer an erase however it likes (upstream #1083, unanswered) | **Medium** | `OI-LFS-07` — power-loss injection on hardware at bring-up, on the real eMMC behind the real XTS layer. Until then §12.5 bounds what may be cited | **Open** — Rev 4: also carries the traversal-time measurement (§13.1.4) |
| `RISK-LFS-07` | **A Config file disappears silently and no integrity check fires** — upstream #1210's orphaned `INLINE` tag, whose data carries a valid CRC. The worst instance is `ukmd.rec`: its loss makes that user's UHDR permanently unmountable, with no NeurOne-held second copy (`NP-FW-NVRAM-001` §3.3.1.1). Not an emission path; a total loss of the user's own property | **Medium** | `OI-LFS-08` — bound the Config directory's create/delete churn, and give `ukmd.rec` a durability story that does not depend on one filesystem entry. ~~Nothing mitigates it today~~ **Rev 4 (§13.4):** the store never removes or renames, so churn is one create per file for the partition's life; `ukmd.rec` is two enveloped copies in two metadata pairs, repaired from its twin on first use | **Reduced** — #1210 was not reproduced, so the mitigation is shown bounding the trigger and replicating the victim, not preventing the defect. Both copies lost remains possible and is reported as an absence |
| `RISK-LFS-05` | The log partitions are mounted with parameters nobody chose — a `block_count` three orders of magnitude larger than Config's, against a `lookahead_size` sized for Config | Medium | ~~`OI-LFS-05`~~ — **Rev 4:** `np_lfs_log_instance` applies and validates `EMMC-FS-01`'s UHDR/SHDR columns (with `ECR-EMMC-002`'s deviation), and `L-1`/`L-2` are swept on the real geometry (§13.1) | **Closed 2026-09-24** |
| `RISK-LFS-08` | **A program smaller than the XTS data unit tears committed data.** The encryption layer must read-modify-write the whole 512-byte unit, so a power loss damages bytes littlefs has already synced — breaking the property `L-1…L-4` rest on from below the contract | Medium | Log instances: `prog_size` 512, validated and `_Static_assert`ed (§13.1.3, `ECR-EMMC-002`). ~~**Config instance: not mitigated** — built at 256 per `EMMC-FS-01`, shown to lose a durable Map 3 record under the RMW model~~ **Rev 5: Config instance moved to 512** (§13.8), `_Static_assert`ed and pinned by `np_lfs_config_tests` | **Closed 2026-09-24 (Rev 5)** — the Config instance is at 512 too (§13.8), and the sweep that found the Config loss now passes on the instance as built |

---

## 9. Open items

| ID | Description | Owner | Blocking |
|---|---|---|---|
| ~~`OI-LFS-01`~~ | **✅ CLOSED 2026-09-14 (Rev 2) — littlefs `v2.11.3` pinned and vendored** at `firmware/vendor/littlefs/`: byte-exact five-file subset verified by two independent downloads, per-file SHA-256, the configuration of §7.3 recorded *and* compiled *and* tested (`np_lfs_config_tests`, 19 falsified rejections), and an explicit "what this does not establish". §7.4. **Closing it unblocks #75 and unblocks `OI-LFS-02`; it unblocks no atomicity claim** — see §7.5, and note that the item below inherits the BLOCKING status this one carried | — (closed) | — |
| ~~`OI-LFS-02`~~ | **✅ CLOSED 2026-09-14 (Rev 3) — both halves performed.** The IEC 62304 §7.1.2 anomaly evaluation against `v2.11.3` is **§11**: five tracker queries plus the release notes and the `v2.11.2`→`v2.11.3` differential, thirteen items assessed against §3's seven claims, **three applicable** (#1210, #1205, #1086) and none of them on a path to an emission. The power-loss injection test is **§12**: `np_lfs_powerloss_tests`, NeurOne's own injecting block device, **456 interrupted runs across three scenarios and three tear models with 0 violations**, plus three unsafe orderings the same verifiers are required to catch and six hand perturbations of which five were caught and one (P4) was not and is recorded as a negative result. **Closing it unblocks `NP-FW-NVRAM-001` §4, `EMMC-FS-01` and `NP-FW-HUB-001` §6.5 only as far as §12.5 states** — against the `lfs_config` contract, not against the eMMC — and it leaves three successors below, two of which are constraints on code that does not exist yet | — (closed) | — |
| **`OI-LFS-07`** | **Power-loss injection on hardware.** §12 interrupts the `struct lfs_config` contract; the medium is an eMMC behind an XTS layer, and littlefs's guarantees are written for raw program/erase semantics. Undecided on both sides: what NeurOne's `erase()` callback does on a device with large erase groups, whether `block_cycles` 500 (claim `L-6`) means anything above an FTL that levels wear itself, and whether "reliable write" bounds a torn sector. Upstream #1083 asks exactly this and has no answer, #1203 is the same family. **This is the part of `OI-LFS-02` a host test could never discharge, and it is raised rather than absorbed** **Rev 4 adds two bring-up measurements:** the time of one allocator traversal on a realistically filled UHDR partition (§13.1.4 — ≈ 0.5 reads per block in use; if it exceeds the session logger's buffering headroom, revisit `lookahead_size`), and whether the eMMC + XTS stack tears a 512-byte unit as a whole or not at all (§13.1.3's model is the worst case) **Rev 8 (§13.11): narrowed.** Decided on the host: `erase()` is a **no-op** (lfs.h's contract; swept with a no-op erase, 0 violations); `block_cycles` stays at `EMMC-FS-01`'s values (it relocates logical metadata; the FTL levels physical wear; `EMMC-WE-03`'s WAF monitor measures the cost). **Left for silicon:** `HW-LFS-01…05` — tear granularity (and so whether eMMC reliable write is needed), the power-loss sweeps on the real stack with hang detection, traversal time against the logger's buffering, and write amplification | FW + EE | **Any reliance on `L-1…L-4` against the MEDIUM rather than the contract.** `RISK-LFS-06`; bring-up |
| ~~`OI-LFS-08`~~ | **✅ CLOSED 2026-09-24 (Rev 4, §13.4).** Bound the Config directory's create/delete churn, and stop `ukmd.rec` depending on one filesystem entry (upstream #1210). **Done:** `np_cfg_store` never removes or renames — replacement is an in-place `O_TRUNC` rewrite, re-swept for `L-3` (186 runs, 0 violations) — so the partition sees one create per file for its life and zero deletes; `ukmd.rec` is two enveloped copies in two directories (two metadata pairs), repaired from its twin on first use. **Not shown:** #1210 did not reproduce on `v2.11.3` in 3,000 churn cycles, so the store is shown bounding the trigger and replicating the victim, not preventing the defect. `RISK-LFS-07` reduced | — (closed) | — |
| ~~`OI-LFS-09`~~ | **✅ CLOSED 2026-09-24 (Rev 4, §13.2).** Validate stored values by content on every read. **Done:** `np_cfg_store` has no unverified read and no `stat` — a content check is mandatory on every read; a read error is an absence, never retried, and forces a remount before the next operation. **#1205 reproduced on `v2.11.3`** through raw littlefs and shown not to reach a store caller; hand-falsifying the remount found a second path (a failed read of the root metadata pair poisons littlefs's own cache) that is now a test, and the store's own test found a defect in it (a failed remount stuck) that is fixed | — (closed) | — |
| ~~`OI-LFS-03`~~ | **✅ CLOSED 2026-09-24 (Rev 4, §13.5).** The CI check for `REQ-LFS-01`: `scripts/check-lfs-caller-rules.ts`, run by `tooling-ci.yml` after its own 21-case `--self-test`. R4 pins the tail-additive Config files to exactly Map 3's journal; R5 allows the journal reader only from listed callers, each with why it reads history (none yet); R6 forbids every emission-limit consumer from naming the journal at all. In the API, each file's policy decides which call may touch it. **Reach:** textual — a value handed through a helper is not followed, which is why every `JOURNAL_READERS` entry must carry its reason | — (closed) | — |
| **`OI-LFS-04`** | `OI-NVRAM-10` — is there a Class C bound on per-tile emitter drive current independent of Map 1's ranges? Carried here because §5.3's third barrier is a thermal limit standing in for an optical one, and this document is where that now has a named consequence **Rev 8 (§13.12): answered — no.** The safety MCU owns the cranial and 1170 nm PBM enable lines and cuts them on over-temperature; it has no per-tile magnitude bound. The tile's on-current is set by hardware (bus voltage, string `V_f`, sense resistor — `NP-HW-HEXTILE-001` §6.2), not by Map 1, but nothing regulates it; Map 1 bounds drive only through firmware (duty/intensity codes). So an out-of-range Map 1 entry reaches average irradiance through Class B firmware, with the 62 °C cut as the only Class C backstop — `OI-NVRAM-10`'s residual, confirmed. The drive stage itself is specified inconsistently (`OI-HEXTILE-23`). Options in §13.12; the choice is Safety + EE's | Safety + EE | **Class B classification durability** |
| ~~`OI-LFS-05`~~ | **✅ CLOSED 2026-09-24 (Rev 4, §13.1).** The UHDR and SHDR instances' parameters. **The premise was wrong: `EMMC-FS-01` has UHDR and SHDR columns** (and states every Config value Rev 2 said it did not — `OI-LFS-10`). Decided: `EMMC-FS-01`'s values with **one deviation, `read_size`/`prog_size` 512** (`ECR-EMMC-002`: a smaller program is a read-modify-write of committed bytes under the 512-byte XTS unit, shown by sweep). `lookahead_size` kept at the specified 512/256 under CLAUDE.md §18 — full coverage would cost 237 KiB of on-chip RAM against an unmeasured stall — with its traversal cost measured in reads and its time added to `OI-LFS-07`. `np_lfs_log_instance` applies and validates them; `L-1`/`L-2` swept on the real 1,767,168- and 131,072-block geometry (132 runs, 0 violations). **Does not build the UHDR file glue** — that is `OI-LOG-05..07`, so `OI-FAULTMSG-03` narrows rather than unblocks. `RISK-LFS-05` closed | — (closed) | — |
| ~~`OI-LFS-06`~~ | **✅ CLOSED 2026-09-24 (Rev 4, §13.3).** One open handle per file, made checkable. Every Config handle comes from `cfg_open()`, which keeps a registry and refuses a second (`NP_HUB_ERR_STORE_BUSY`), tested; the gate's R1/R2 forbid littlefs calls outside the listed callers, `lfs_file_open()` anywhere, and `lfs_file_opencfg()` anywhere but inside `cfg_open()`. The log instances' future glue must be added to the gate's `LFS_CALLERS` with its reason | — (closed) | — |
| ~~`OI-LFS-10`~~ | **✅ CLOSED 2026-09-24 (Rev 5 §13.8, Rev 6 §13.9).** The Config instance disagreed with `EMMC-FS-01` on five fields, and its `prog_size` 256 lost committed data under the 512-byte XTS read-modify-write. **Decided by the principal in two steps:** `read_size`/`prog_size` 512 with `cache_size` 512 (Rev 5); then `EMMC-FS-01`'s `lookahead_size` 64, `block_cycles` 200, `name_max` 64, `attr_max` 256 and `metadata_max` 4,096 (Rev 6). `ECR-EMMC-002` applied: `NP-FW-EMMC-001` Rev 3 prints 512 in all three columns. The Config instance now equals the specification exactly. `RISK-LFS-08` closed | — (closed) | — |
| ~~`OI-LFS-11`~~ | **✅ CLOSED 2026-09-24 (Rev 7, §13.10).** The log backend's one append file per partition could not pass littlefs's 2 GiB `file_max` on the 6,903 MiB UHDR partition. **Decided:** the layout `NP-FW-EMMC-001` already specified — UHDR one file per session (`EMMC-UHDR-12`/`-13`, `/uhdr/sessions/<count>`), created exclusively, opened at session start and closed at session end; SHDR one file, which cannot reach `file_max` because its partition is 512 MiB. A single session file is capped at `file_max` (≈49 h at 12 kB/s) and fails that session closed. New seam `np_log_hal_part_close()` | — (closed) | — |
| ~~`OI-LFS-12`~~ | **✅ CLOSED 2026-09-24 (Rev 9, §13.13).** The device session count was incremented at session start but never persisted. **Done:** it is a Config file kept `REPLICATED` by `np_cfg_store` (`np_session_count`), committed at session start before the session's UHDR file is created, and loaded at boot. `np_hal_get_device_session_count()` retired (census 99 → 98); the cervical fault summary now records the current session's count | — (closed) | — |

---

## 10. Revision history

> **§11 and §12 follow this section rather than preceding it.** §8, §9 and §10 are cited by number
> from `NP-SW-001` §9.4, `docs/status/pending-decisions.md` and `NP-DHF-001`, and `NP-CONV-001` §5
> makes a section number an address — renumbering them to keep the revision history last would break
> those citations to save an ordering preference.

| Rev | Date | Author | Description |
|---|---|---|---|
| 9 | 2026-09-24 | NeurOne Firmware Engineering | **`OI-LFS-12` CLOSED — the device session count is persisted (§13.13).** New Config file `NP_CFG_FILE_SESSION_COUNT` (`REPLICATED`) and module `np_session_count` (load / commit, 4 bytes little-endian). `np_session_log` commits each new count through a hook *before* creating the session's UHDR file, so no file can exist whose count was not persisted; bring-up seeds the logger from the record. The cervical offline-fault summary now records the current session's count instead of a boot-time value. `np_hal_get_device_session_count()` (`OI-HUB-MAIN-03`) retired — SW-02 census 99 → 98. Tested in `np_cfg_store_tests` (reboot, lost entry, a power-loss sweep of the commit: 12 runs, 0 violations) and `np_log_backend_tests` (commit-before-create; reboot seeding); falsified two ways. Rev 8 → 9. |
| 8 | 2026-09-24 | NeurOne Firmware Engineering | **`OI-LFS-07` and `OI-LFS-04` narrowed (§13.11, §13.12).** `OI-LFS-07`: `np_lfs_powerbd` gains a no-op-erase mode (still cuttable, never changes the medium), proven active by a direct check that fails when the mode is disabled; the SHDR L-1/L-2 sweep and the Config journal through the store both pass with it (66 + 84 runs, 0 violations), so NeurOne's eMMC `erase()` is decided a no-op. `block_cycles` retained. Bring-up protocol `HW-LFS-01…05` written with pass criteria (≥1,000 cuts per scenario, 0 violations, 0 hangs; the rule of three bounds the per-cut failure rate below 0.3 % at 95 %). `OI-LFS-04`: answered **no** from the code and the hardware record; the tile's drive stage is specified two incompatible ways — raised as `NP-HW-HEXTILE-001` `OI-HEXTILE-23`; three options stated, not chosen. Rev 7 → 8. |
| 7 | 2026-09-24 | NeurOne Firmware Engineering | **`OI-LFS-11` CLOSED — UHDR logs become one file per session (§13.10).** `np_log_backend` gains `np_log_backend_session_begin(counter)` / `_session_end()`: UHDR is written only inside a session, to `/uhdr/sessions/<count>` (`EMMC-UHDR-12`/`-13`), created exclusively and never reopened; each session's staged tail is flushed into its own file; a per-file cap at `file_max` fails one session closed and clears at the next. UHDR is no longer opened at boot, when it is not yet mounted. SHDR keeps one file — its partition (512 MiB) is below `file_max`. `np_session_log` opens and closes the files at session start/end, increments the device session count there (`EMMC-SHDR-09`), and steps past a count whose file exists rather than lose the session (new `NP_HUB_ERR_LOG_EXISTS`). Lower HAL: `np_log_hal_part_open()` gains the segment argument; new seam `np_log_hal_part_close()` (SW-02 census 98 → 99). `np_log_backend_tests` gains seven cases and links the logger; falsified four ways. New `OI-LFS-12`: the session count is not persisted. Rev 6 → 7. |
| 6 | 2026-09-24 | NeurOne Firmware Engineering | **`OI-LFS-10` CLOSED and `ECR-EMMC-002` APPLIED (§13.9).** The Config instance takes `EMMC-FS-01`'s remaining values on the principal's decision — `lookahead_size` 64 (was 512), `block_cycles` 200 (was 500), `name_max` 64 and `attr_max` 256 (were unset), `metadata_max` 4,096 stated — now set by `np_lfs_config_apply()` and checked by `np_lfs_config_validate()`, with five new must-refuse perturbations. `inline_max` falls to 256 B (`attr_max` bounds it); `ukmd.rec`'s 208-byte envelope stays inline. `NP-FW-EMMC-001` amended Rev 2 → 3 by `editscripts/patch_emmc_ecr002_prog512.py` (idempotent; six cells changed with the superseded 256 retained, a banner, header and revision row) — the first edit to that `.docx` since Rev 2, made through the patch-script convention `OI-CONV-04` points to. All three instances now match `EMMC-FS-01` exactly. Suites re-run: §12 285 interrupted runs, the store 225, Config RMW 84 — 0 violations; falsifications still caught. Rev 5 → 6. |
| 5 | 2026-09-24 | NeurOne Firmware Engineering | **`OI-LFS-10`'s hazard half decided by the principal — the Config instance moves to `read_size`/`prog_size` 512 (§13.8).** `cache_size` follows to 512 because `lfs_init()` requires a multiple of `prog_size`, which also brings it into agreement with `EMMC-FS-01`. `np_lfs_instance.h` changed and `_Static_assert`ed (a 256 build no longer compiles); `np_lfs_config_tests` pins 512 and now requires 256 to be *refused*; the Config RMW sweep in `np_lfs_log_instance_tests` flips from *must lose data* to *must lose nothing*, and a raw-littlefs sweep at 256 keeps the evidence live. §12's suite re-run on the new instance: 285 interrupted runs, 0 violations, falsifications still caught. `ECR-EMMC-002` extended to the Config column. `RISK-LFS-08` closed. **Still open in `OI-LFS-10`:** `lookahead_size`, `block_cycles`, `name_max`/`attr_max`. Static RAM 1,024 → 1,536 B, plus 512 B per open file. Rev 4 → 5. |
| 4 | 2026-09-24 | NeurOne Firmware Engineering | **Builds the caller rules §11 asked for — `OI-LFS-03`, `-05`, `-06`, `-08`, `-09` CLOSED (GitHub #382), §13.** New Class B module **`np_cfg_store`**, the Config instance's only caller, in which each rule is a property of the API: a registry that refuses a second handle (`OI-LFS-06`); no unverified read, no `stat`, a read error reported as an absence and followed by a remount (`OI-LFS-09`); no remove or rename — replacement is an in-place `O_TRUNC` rewrite, re-swept for `L-3` — and `ukmd.rec` held as two enveloped copies in two metadata pairs, repaired from its twin (`OI-LFS-08`); and a per-file policy table whose tail-additive rows are pinned. **`scripts/check-lfs-caller-rules.ts`** makes going around the API fail CI and is `REQ-LFS-01`'s check (`OI-LFS-03`), self-tested in 21 cases. New module **`np_lfs_log_instance`**: the UHDR/SHDR parameters, validator and allocator primer (`OI-LFS-05`). Host targets `np_cfg_store_tests` and `np_lfs_log_instance_tests` (Class B 32 → 34, repo 42 → 44); `np_lfs_powerbd` gains a sparse medium, read-error injection and a read-modify-write tear unit, all off by default so §12's suite is untouched. **Findings:** upstream **#1205 reproduced** on `v2.11.3` and shown not to reach a store caller — hand-falsification of the remount found a second, metadata-pair path, now a test; **`EMMC-FS-01` states every parameter** Rev 2 said it did not, including UHDR and SHDR columns, and the Config code disagrees on five fields; **`prog_size` below the 512-byte XTS unit loses committed data** under a read-modify-write tear — the logs deviate to 512 (`ECR-EMMC-002`), the Config instance as built does not (`OI-LFS-10`, `RISK-LFS-08`); the specified UHDR `lookahead_size` costs one whole-filesystem traversal per 16 MiB written, ≈ 0.5 reads per block in use, kept under CLAUDE.md §18 with its time added to `OI-LFS-07`; the log backend's single-file layout cannot fill UHDR (`OI-LFS-11`). **Negative results recorded:** #1210 did not reproduce; the remount was at first caught only by a counter. Falsified by hand in 13 ways (§13.6). `RISK-LFS-05` closed, `RISK-LFS-07` reduced, `RISK-LFS-08` added. **Not closed:** `OI-LFS-07` (hardware) and `OI-LFS-04` (Safety + EE). Nothing is integrated. Rev 3 → 4. |
| 3 | 2026-09-14 | NeurOne Firmware Engineering | **Closes `OI-LFS-02`: the §7.1.2 anomaly evaluation is performed (§11) and `L-1…L-4` are exercised (§12).** **§11** evaluates littlefs `v2.11.3`'s published anomaly list — its GitHub tracker and release notes, because upstream publishes no numbered errata document — through five queries plus the `v2.11.2`→`v2.11.3` differential, and assesses thirteen items against §3's seven claims. **Three are applicable and none reaches an emission**: #1086's assertion becomes a halt the safety MCU converts into a stimulation cutoff; #1205's stale read cache becomes an absence because every stored value is CRC- or AEAD-checked at the point of use; and **#1210 is stopped by nothing that exists today** — it drops a file's `NAME` tag during compaction while keeping its `INLINE` data, silently, with a valid CRC, and the consequence to design against is a user's UHDR becoming permanently unmountable through the loss of `ukmd.rec`. **That is §6.2's finding arriving a second time by a different route**: the anomaly profile, like the Class B argument, rests on caller policy — and the callers are the unwritten `OI-LOG-05..07` glue. **§12** records `np_lfs_powerloss_tests` (Class B 28 → 29, repo 35 → 36): NeurOne's own injecting block device, written rather than vendored because vendoring upstream's `lfs_emubd` would make the old *"per LittleFS test suite"* citation look discharged; a cut swept across **every** medium-touching op of each commit sequence under three tear models; **456 interrupted runs, 0 violations**, every attempt remounting cleanly. `L-1` is checked in a sharper form than it is written — the recovered length must be an exact flush boundary — and `L-3`'s result is explicitly **a property of the write-temp-then-rename ordering, not of the component**. Falsified in both directions per `NP-CONV-001` §8: three unsafe orderings the same verifiers are required to catch (including truncate-and-rebuild, which is `REQ-LFS-01`'s own subject matter), and six hand perturbations of which **five were caught and P4 was not** — clearing the `lfs_t` across a simulated reboot changes nothing, because `lfs_mount()` re-initialises it, and that negative result is recorded rather than dropped. **What a reviewer may cite is bounded by §12.5**: the contract was interrupted, the eMMC was not. `RISK-LFS-03` reduced, `RISK-LFS-06` and `RISK-LFS-07` added. Three successors: `OI-LFS-07` (hardware injection over the real eMMC and XTS layer — the part of `OI-LFS-02` a host could never discharge), `OI-LFS-08` (#1210: bound the Config directory's create/delete churn and stop `ukmd.rec` resting on one filesystem entry) and `OI-LFS-09` (validate by content on every read — #1205 and #1164 converge on it). Rev 2 → 3. |
| 2 | 2026-09-14 | NeurOne Firmware Engineering | **Closes `OI-LFS-01`: littlefs is pinned at `v2.11.3` and vendored, with its configuration.** Five byte-exact files at `firmware/vendor/littlefs/` from tag commit `6cb4e865`, verified by two independent downloads and carrying per-file SHA-256; a `VERSION` SOUP record on the `cmsis_core` pattern; `np_littlefs` building in both modes. **The tag choice is a safety argument, not a preference**: `v2.11.3` is the first release containing upstream `488e84bb`, which fixes data corruption from two write handles on one file — a defect on exactly the axis `L-1…L-4` rest on — and it leaves behind a NeurOne requirement (one open handle per file, `OI-LFS-06`). §7.3's configuration is recorded, compiled and tested: `LFS_NO_MALLOC` with static buffers, `block_cycles` **500** checked by value because `-1` passes every non-zero test and silently disables `L-6`, `LFS_THREADSAFE` on because the Config instance has three specified writers, assertions retargeted to the `np_freertos_assert_failed` halt, and `np_lfs_config_validate()` as the mount-time check `L-5` names — **littlefs's own asserts check that a config is self-consistent and cannot check that it is NeurOne's.** `np_lfs_config_tests` (Class B 27 → 28, repo 34 → 35) re-derives the SHA-256s, compares the build configuration against the SOUP record, and requires 19 single-field perturbations to be rejected; it was falsified in seven ways first per `NP-CONV-001` §8. **One finding came out of building it**: setting `LFS_NO_ASSERT` beside a replacement `LFS_ASSERT` removes the *definition* of `lfs_mlist_isopen` while keeping the *call*, and that assertion is the run-time detector for the very corruption this tag was pinned for. **Rev 1's §7.1 reasoning is outweighed, not refuted** — "do not pin before the anomaly list is read" blocked its own precondition, since the list cannot be read without a tag; the absence of the evaluation is now recorded explicitly instead of being concealed behind a missing version. **`OI-LFS-02` inherits the BLOCKING status**: `NP-FW-NVRAM-001` §4, `EMMC-FS-01` and `NP-FW-HUB-001` §6.5 are exactly as unverified as at Rev 1, and §7.5 exists because a populated vendor directory reads as an answer. `RISK-LFS-04` closed, `RISK-LFS-03` narrowed, `RISK-LFS-05` added. Two new open items: `OI-LFS-05` (the log partitions have no stated instance parameters, and they are the ones `L-1`/`L-2` are about) and `OI-LFS-06`. Feeds #75 (`NP-SBOM-001`) — which an SBOM entry can now satisfy. Rev 1 → 2. |
| 1 | 2026-09-13 | NeurOne Firmware Engineering | **Initial release — brings LittleFS under SOUP management and performs the hazard analysis `OI-NVRAM-12` requires (Issue #339).** Replaces `NP-SW-001` §9.4's single cell (*"2.x"* / *"Power-loss testing per LittleFS test suite"*). **Principal finding: LittleFS is not unmanaged, it is absent** — `grep -rn "lfs_" firmware/` returns two hits, both comments in `np_log_backend.h` naming the `OI-LOG-06/07` seams, so every power-loss atomicity guarantee in `NP-FW-NVRAM-001` §4, `EMMC-FS-01` and `NP-FW-HUB-001` §6.5 rests on a component with no source, no version and no NeurOne test. **Second finding, and the one that outlives integration: the Class B classification is not a property of LittleFS but of `np_module_map`'s reject-and-rebuild policy** — every integrity failure becomes an *empty* inventory rather than a *wrong* one — and `NP-FW-NVRAM-001` §7.2 specifies Map 3 to need the opposite, tail-additive policy for independently correct reasons. `REQ-LFS-01` is the rule that keeps both decisions safe: no value that bounds an emission may be stored under a tail-additive policy. **The §7.1.2 anomaly evaluation is deliberately not performed** — it evaluates the list for the version in use and none is in use; recording a conclusion against *"2.x"* would reproduce `OI-DOC-01` inside the document written to close its sibling. Four risk rows, four open items, two of which supersede the halves of `OI-NVRAM-12`. Feeds #75 (`NP-SBOM-001`). |

---

## 11. IEC 62304 §7.1.2 — anomaly evaluation for littlefs v2.11.3 (performed 2026-09-14)

> **Read §11.4 before citing §11.5.** The conclusion is not "no applicable anomalies". It is "three
> applicable anomalies, each stopped by something — and two of the three are stopped by a caller
> that has not been written yet."

### 11.1 What the clause asks, and what "the published anomaly list" is here

§7.1.2 requires that the **publicly available list of known anomalies for the SOUP version actually
in use** be evaluated for anomalies that could result in a hazardous situation, and that the
evaluation be recorded. Rev 1 refused to do this against *"2.x"*, and was right to: the clause is
version-scoped, and a conclusion against a version family resolves to nothing. `v2.11.3` is now
pinned, so the activity is performable and is performed here.

**littlefs publishes no numbered errata document.** Unlike ST, which ships a *Known Limitations*
section per release (`NP-SOUP-CMSIS-001` §4.1), the upstream project's anomaly list is its GitHub
issue tracker plus its release notes. That is the list that exists, so that is the list evaluated —
and the fact that it is a tracker rather than an errata sheet is the largest limitation on this
evaluation, stated in §11.2 rather than left to be discovered.

### 11.2 Method, and what it does and does not cover

Performed 2026-09-14 against the pinned tag:

1. **The release notes for `v2.11.3` itself.** They enumerate the six commits since `v2.11.2` and
   publish no *known issues* section. They also carry two upstream status lines worth recording
   because they bound the pin's runway: *"littlefs2 status: feature freeze"* and *"littlefs3 status:
   in-progress, unstable"*. The v2 line receives fixes and no features; v3 is not a candidate.
2. **The `v2.11.2` → `v2.11.3` differential.** A release that fixes something is evidence that
   something was wrong in the version before it, so the six commits were read individually (§11.6).
3. **The upstream issue tracker, filtered five ways** — three keyword queries against the claims in
   §3, one label query, and one recency query to catch defects reported *after* the tag was cut:
   - `is:issue is:open label:"needs fix"` — upstream defines this label as *"we know what is wrong"*,
     which is the closest thing the project has to a published defect list. **12 items.**
   - `is:issue is:open power loss` — **10 items.**
   - `is:issue is:open corruption sync OR rename OR append` — **7 items.**
   - `is:issue is:open sort:created-desc` — the newest reports, because the tag is dated 2026-03-24
     and an anomaly reported since is still an anomaly *of* it. **12 items on the first page.**
   - `is:issue is:open label:bug` — **no such label exists** and the query returns nothing. Recorded
     so that a later reviewer re-running this evaluation does not read an empty result as a clean
     one, which is exactly the failure shape the old verification cell had.

**Limitations, stated rather than left implicit — and they are larger here than for CMSIS.**

- A tracker is not an errata list. It mixes defects with questions, porting problems, enhancement
  requests and discussions, and nobody curates it into "anomalies in version X".
- The search is keyword-driven and therefore **not exhaustive**. `NP-SOUP-CMSIS-001` §2 accepted the
  same residual on the strength of a five-header, declaration-only surface. **That argument is not
  available here**: `lfs.c` is 6,558 lines of logic carrying integrity guarantees for stored values
  that other code trusts. The residual is accepted on a different and weaker basis — that §5's
  consumer analysis bounds what *any* anomaly in this component can reach, whether or not this
  evaluation found it — and it is carried in §11.5 as the first residual rather than waved away.
- **Several applicable reports name no version, or an older one, and are undiagnosed.** Upstream has
  not said whether they are fixed. They are evaluated as *live* in each case, because the alternative
  is to assume in NeurOne's favour about somebody else's defect.

### 11.3 Anomalies touching the vendored subset and NeurOne's paths

The subset is `lfs.c`, `lfs.h`, `lfs_util.c`, `lfs_util.h` (§7.4), so every anomaly in upstream's
`bd/`, `tests/`, `benches/`, `runners/` and `scripts/` is out of scope by construction — the return
on the narrow-vendoring decision, as at `NP-SOUP-CMSIS-001` §3.1.

| Upstream item | State | Reaches | Hazard assessment |
|---|---|---|---|
| **#1210** — *compact produces orphan `INLINE` tag without corresponding `NAME` tag, resulting in silent data loss* | open, **no fix**, reported against v2.4.0 and v2.9.3; the reporter states the filter sub-traverse logic is unchanged across versions, so `v2.11.3` is **not excluded** | metadata compaction; inline files | **APPLICABLE — the most serious item found, and it is not an emission path.** The trigger is repeated create/delete of a lower-id file in one metadata pair, whose `SPLICE` tags accumulate between another file's `NAME` and `INLINE` tags until the `NAME` falls outside the compaction range. The Config partition has exactly that shape: `npmp.bin` is replaced through a temp file and a rename (create/delete churn), Map 3's journal and `ukmd.rec` sit beside it, and at `cache_size` 256 both Map 3's 32 B records and `ukmd.rec`'s 192 B are **inline**. **The loss is silent**: the orphan's CRC is valid, so §5.3's blob CRC never sees it and reject-and-rebuild never fires — what disappears is a whole file, not a wrong value. Ranked by consequence: (a) **`ukmd.rec` lost ⇒ that user's UHDR is permanently unmountable**, with no NeurOne-held second copy (`NP-FW-NVRAM-001` §3.3.1.1) — a total loss of the user's own property, and the worst outcome in this table; (b) Map 3's journal lost ⇒ history loss, and `REQ-LFS-01` guarantees no value bounding an emission was in it; (c) `npmp.bin` lost ⇒ an **empty** inventory, which is §5.3's safe direction. **No path to an emission.** The mitigation is a caller rule and it does not exist yet: `OI-LFS-08` |
| **#1205** — *rcache contains invalid data after a read error, causing incorrect data returned by `lfs_read()`* | open, no fix | the read path | **APPLICABLE, and it is the one item that lands squarely on §5.3's path.** `lfs_bd_read()` updates the cache's block/offset/size on an error return but leaves the stale buffer, so a later read can return wrong bytes **reporting success**. The precondition is a block-device read error, which an eMMC produces on an uncorrectable ECC failure — reachable, not theoretical. What stops it is, again, not the component: every NeurOne consumer of a stored value checks it at the point of use — blob CRC → reject-and-rebuild, Map 3 per-record CRC, `ukmd.rec`'s AES-256-GCM tag — so a silently wrong value becomes an integrity failure, and §5.3 step 2 turns every integrity failure into an **absence**. That holds **only if the check is repeated on every read rather than performed once at mount**, and only if a failed read is not retried through the same cache. `OI-LFS-09` |
| **#1086** — *assertion after continuous metadata block relocation during deletion* (`LFS_ASSERT(lfs_tag_size(lfs->gstate.tag) > 0 \|\| orphans >= 0)`) | open, **"needs fix"**, v2.8 | delete + relocate | **APPLICABLE, loud, and fail-safe by NeurOne's configuration.** `LFS_ASSERT` is retargeted to `np_lfs_assert_failed()`, which halts the main processor; the SPI heartbeat stops; the STM32G071 cuts all stimulation within <50 ms with hardware the main processor does not own (`CLAUDE.md` §4.2). The cost is a lost session and an unclean shutdown, not an unsafe emission. **It is also the second time `np_lfs_config.h` decision 2 pays**: with `LFS_NO_ASSERT` this would have been undefined behaviour over metadata instead of a halt |
| **#1211** — *littlefs stuck during write operation* after a power-loss-interrupted write | open, undiagnosed, **no version**, possibly a port defect | post-power-loss recovery | **Cannot be excluded — and it is the failure §12 structurally cannot see.** A hang is not a wrong value; a sweep that cuts and re-runs would not distinguish one from a slow run. On NeurOne it is fail-safe for #1086's reason (the heartbeat stops), and it is carried into `OI-LFS-07`'s hardware work, where a hang is directly observable |
| **#1174** — *after adding content to a file multiple times, the file content is incorrect* | open, **"needs investigation"**, v2.7.0 | the append path — `L-2` | **Not dismissible, and its configuration is NeurOne's exactly**: `prog_size` 256, `block_size` 4,096, `cache_size` 256. Reported on v2.7.0, i.e. before `488e84bb`, and plausibly that same multiple-write-handle defect — but **upstream has not said so**, and assuming it would be assuming in NeurOne's favour. §12's `L-1`/`L-2` sweep exercises the same shape at `v2.11.3` (repeated append + flush, file spanning blocks) across 90 interrupted runs and found no size or content anomaly. Carried as a **residual** (§11.5), not as a cleared item |
| **#1164** — *what is the file state upon power down?* | open, documentation | `L-1`'s own wording | **A specification finding, not a defect, and it changes what the glue must do.** The reported behaviour is that a file created and written but never synced **exists after a power loss and is empty**. So **the presence of a file is not evidence that its contents are durable**, and `lfs_stat()` is not a durability check. `ukmd.rec`, `npmp.bin` and Map 3's journal must be validated by **content**. Folded into `OI-LFS-09` |
| **#1061** — *corruption reported upon concurrent directory modification* | open, "needs fix", v2.10.1 | `lfs_dir_read` | Not applicable to any specified NeurOne path — nothing iterates a Config directory. If the log-rotation glue ever does, the failure is a **false** `LFS_ERR_CORRUPT`: loud, and in the safe direction |
| **#1201** — *`lfs_fs_gc` null dereference when called before mounting* | open | `lfs_fs_gc` | Not applicable: NeurOne calls no garbage collection. Recorded because `compact_thresh` is left at 0 and a later performance change is the kind of thing that reaches for `lfs_fs_gc` — the constraint is *after* mount, never before |
| **#1213** — *invalid pointer dereference in `lfs_dir_commitattr` via `lfs_migrate`* | open | `lfs_migrate` | Not applicable. `LFS_MIGRATE` is not defined and neither is `LFS_MULTIVERSION`; NeurOne writes the pinned library's own on-disk version (2.1) and reads no foreign media (`np_lfs_config.h`, *"Deliberately NOT defined"*) |
| **#1207** — *custom attribute changes won't trigger a metadata commit unless file data is written* | open | custom attrs | Not applicable. No instance uses custom attributes; `lfs_file_opencfg()` is used for its **buffer**, which `LFS_NO_MALLOC` makes mandatory, with `attr_count` 0 |
| **#1214** — *strange code in `lfs_ctz_traverse` triggering `clang-analyzer-security.ArrayBound`* | open | static analysis | Not a behavioural claim. Vendored SOUP is exempt from NeurOne's static-analysis and MISRA scans with documented origin (IEC 62304 §8.1.2; the Monocypher and `mpu_armv7.h` precedents, `NP-SOUP-CMSIS-001` §3.2) |
| **#1203** — *littlefs reports "Bad block at 0x0" on EEPROM (write == read correctly)* | open | bad-block detection | Adjacent to #1083 rather than separate: the reporter's medium is an EEPROM with no erase semantics. It belongs to the "littlefs on a medium that is not raw flash" family, which is `OI-LFS-07`'s subject |
| **#1083** — *eMMC questions*: what `erase()` should do on a device with 384 KiB erase groups, whether `block_cycles` should be 0 when the FTL levels wear itself, whether "reliable write" removes the need for power-loss monitoring | open, **no maintainer answer** | the premise of every claim in §3 | **The finding that outlives this evaluation, and it is not a defect in littlefs.** littlefs's guarantees are stated against raw program/erase semantics; NeurOne's medium is **managed** flash behind an XTS layer. Nothing on either side of that boundary is specified: NeurOne has not decided what its `erase()` does, and `block_cycles` 500 (claim `L-6`) is a wear-levelling decision taken above a device that levels wear itself. `OI-LFS-07` |

### 11.4 What the table actually says

Three items are applicable: **#1210**, **#1205** and **#1086**. Reading their assessments together
gives a result that is more useful than the verdict:

- **#1086 is stopped by NeurOne's configuration of the component** — assertions on, retargeted to a
  halt the safety MCU observes.
- **#1205 is stopped by a property of the callers** — every stored value is CRC- or AEAD-checked at
  the point of use, so a silently wrong value becomes an absence.
- **#1210 is stopped by nothing that exists today.** It is silent by construction: the orphaned data
  carries a valid CRC, so no integrity check fires, and the caller never learns that a file it wrote
  is gone.

**This is §6.2 arriving a second time by a different route.** §6.2 found that the Class B
classification rests on `np_module_map`'s reject-and-rebuild policy rather than on LittleFS. §11
finds that the *anomaly* profile rests on caller policy too — and the callers in question are the
`OI-LOG-05..07` glue, which is unwritten. An evaluation performed after that glue exists would be a
different document; performed now, its honest output is a set of constraints on code not yet
written, which is the cheapest moment to state them.

### 11.5 §11 conclusion and residuals

**Conclusion.** **No item on the published anomaly list for littlefs `v2.11.3` can contribute to a
hazardous situation on SW-02** — where "hazardous" means an unintended emission, which is the only
hazard class this component can be on the path to (§5). The three applicable items reach,
respectively, **data availability** (#1210), **an integrity-checked value that fails to an absence**
(#1205), and **a halt that the safety MCU converts into a stimulation cutoff** (#1086). The Class B
classification of §6.1 is unchanged, and §6.2's condition on it is unchanged.

**But the conclusion is conditional in two ways, and both are new work:**

1. **`OI-LFS-08`** — the Config directory's create/delete churn must be bounded and `ukmd.rec`
   protected against #1210, because the one consequence this document is least willing to accept is
   a user's UHDR becoming permanently unmountable through a filesystem defect nobody can observe.
2. **`OI-LFS-09`** — the glue must validate stored values by **content on every read**, never by
   presence and never once at mount, and must not retry a failed read through the same cache.

**Residuals carried forward:**

1. **The keyword search is not exhaustive** (§11.2), over 6,558 lines of logic rather than five
   headers of declarations. Mitigated by §5's consumer analysis, which bounds what *any* anomaly in
   this component can reach, and by the review cadence — this section is re-run in full on any tag
   change, never carried forward.
2. **#1174 is undiagnosed and its configuration is NeurOne's.** §12 exercises the same shape at the
   pinned version and found nothing, which is evidence and not proof.
3. **The evaluation is of the component, not of the stack.** #1083 and #1203 are both the same
   question — what littlefs's guarantees mean on managed flash — and it is unanswered upstream and
   undecided here. `OI-LFS-07`.
4. **This document is DRAFT and unapproved**, consistent with the rest of the SOUP set. It must be
   approved before G2, and §11 must be re-run if the pinned tag moves.

### 11.6 The `v2.11.2` → `v2.11.3` differential

Recorded because a release that fixes something says something was wrong before it, and because
§7.4's tag choice rests on one of these six commits.

| Commit | Touches NeurOne? | Finding |
|---|---|---|
| **Fixed data corruption with multiple write handles** (`488e84bb`, upstream #1194) | **Yes — this is why the tag was chosen** (§7.4) | A sync through one handle left a second open handle on the same file stale, after which the allocator could re-use still-referenced blocks. A data-corruption defect on exactly the `L-1…L-4` axis. NeurOne is on the fixed side of it, and the NeurOne-side requirement it leaves behind — one open handle per file — is `OI-LFS-06` |
| Using `LFS_ASSERT` instead of runtime checks | Yes, and benignly | Consistent with `np_lfs_config.h` decision 2: assertions are ON and retargeted to a halt, so a check that became an assertion is still a check here. It would **not** be under `LFS_NO_ASSERT`, which is the configuration Rev 2 found and rejected (§2.1) |
| Guard null callbacks in `lfs_dir_fetchmatch` | No effect | Defensive against a NULL callback. `np_lfs_config_validate()` refuses a config whose `read`/`prog`/`erase`/`sync` is NULL, so the guarded condition cannot arise on a NeurOne mount |
| Fixes for *"implicit conversion loses integer precision"* warnings | No | Diagnostics. Vendored sources build without `-Werror` (`firmware/vendor/littlefs/CMakeLists.txt`), on the same rule as `np_freertos` and the MCUX device layer |
| Fix broken README link for the emu device; comment typo | No | Documentation, and neither file is vendored |

**On-disk version unchanged at 2.1**, so the pin carries no format migration — which matters because
`LFS_MULTIVERSION` is deliberately not defined and a format step would therefore have been a
one-way door.

---

## 12. The NeurOne power-loss injection test — claims `L-1…L-4` (performed 2026-09-14)

> This is the half of `OI-LFS-02` that replaces *"Power-loss testing per LittleFS test suite"* — a
> cell that recorded an upstream project testing its own code. What follows is what **NeurOne**
> interrupted, how, how often, and what it was confirmed to be able to see.

### 12.1 What was built, and why not upstream's

| File | What it is |
|---|---|
| `firmware/hub_control/tests/np_lfs_powerbd.h` / `.c` | NeurOne's power-loss-injecting test block device. Host-only; no counterpart on the device |
| `firmware/hub_control/tests/np_lfs_powerloss_tests.c` | The suite: three scenarios, three tear models, a full sweep, and the falsification cases |
| `np_lfs_powerloss_tests` | Class B host target 29; repo total 35 → **36** |

**Upstream ships `bd/lfs_emubd.*`, which does this job, and it is deliberately not vendored.** The
reason is already on the record in `firmware/vendor/littlefs/VERSION` and it is not a preference:
the verification cell this whole document replaces cited upstream's own test suite as NeurOne's
verification, and vendoring that suite would make the citation *look* discharged while nothing
NeurOne wrote had run. §7.5 says the same thing about the vendor directory. An injector NeurOne
wrote, interrupting commit orderings NeurOne chose, is the only artefact that answers the clause.

**A power loss is modelled as a stop, not as an error return.** An error return leaves the caller
running and littlefs handles it; a power loss stops the processor and nothing after it executes. So
the injector applies the partial physical effect of the interrupted operation and then `longjmp()`s
out of littlefs entirely. The media array survives the cut — it is the eMMC; every byte of
littlefs's RAM state does not — that is the reboot.

**Three tear models**, because choosing one would be choosing the convenient one:

| Model | The medium after the cut |
|---|---|
| `NONE` | The operation never reached it |
| `PARTIAL` | A whole-`prog_size` prefix landed; the page in flight reads back **indeterminate** — a `0x5A` fill, *not* `0xFF`, because an erased-value fill would make every torn program indistinguishable from one that never started |
| `FULL` | The operation completed and power was lost immediately after |

**The instance under test is the real one.** `np_lfs_config_apply()` fills the `EMMC-FS-01` geometry
and `np_lfs_config_validate()` must accept it before the first sweep runs, so these results are
about a filesystem the device will actually mount — 4,096 blocks of 4,096 B, `prog_size` 256,
`cache_size` 256, `lookahead_size` 512, `block_cycles` 500 — and not about a toy geometry chosen to
make a test quick.

### 12.2 What is swept, and the results

A scenario is run once uncut to establish how many medium-touching ops it performs (`prog`, `erase`
and `sync`; `read` consumes no index, because cutting during a read is indistinguishable from
cutting before it). It is then re-run **once per op index per tear model**, each time from a
byte-identical pre-state, and each attempt is followed by a remount and a full verification.

| Claim | Scenario | Ops | Attempts | Violations |
|---|---|---|---|---|
| `L-1`, `L-2` | A 160-record session log (5,120 B — it spans blocks, so the append goes through the ctz skip list rather than living inline in one metadata pair), extended by two flushed batches of 32 | 30 | **90** | **0** |
| `L-3` | The 14,012-byte `"NPMP"` blob — §5.3's own arithmetic, `HDR(8) + 80 × 175 + CRC(4)` — replaced by write-temp, sync, close, rename | 66 | **198** | **0** |
| `L-4` | A 200-record Map 3 journal extended tail-additively, one flush per record (`NP-FW-NVRAM-001` §4.2 D-5) | 56 | **168** | **0** |

Aggregate across the whole suite, falsification runs included: **74,333 programs, 6,870 erases,
5,946 syncs, 232,921 reads, 1,377 cuts**, in 1.3 s.

**`L-1` is checked in a sharper form than it is written.** "At most the records appended since the
last flush are lost" would be satisfied by any surviving length at or above the durable prefix. The
suite requires the recovered length to be **exactly a flush boundary** — 160, 192 or 224 records —
because *"a flush commits the exact buffered tail"* forbids a partial batch becoming visible. A
length between two boundaries fails.

**`L-2` is checked in its observable form, and that is a deliberate limit.** The suite asserts that
every record committed before the run is byte-identical afterwards. It does **not** assert that no
already-written block is reprogrammed, because copy-on-write legitimately reprograms a block once
its contents have been relocated — a block-level check would fail on correct behaviour.

**`L-3`'s result is a statement about a caller, not about the component.** What was swept is the
write-temp-then-rename ordering. §12.4 shows the other ordering failing. littlefs does not make the
inventory record atomic; **that ordering does**, and the `OI-LOG-05..07` glue has to use it.

### 12.3 Two results worth recording beyond "0 violations"

- **Every one of the 456 attempts remounted successfully.** littlefs's own claim is that it falls
  back to the last known good state, and a failed mount is counted as a violation in its own right.
  None occurred, under any tear model, at any cut point — including `PARTIAL` erases, which leave a
  block that is neither the old contents nor a usable blank.
- **The commit boundaries are where they are supposed to be.** While falsifying the flush-boundary
  check (§12.4, P5) the suite reported a recovered length of exactly 192 records at cut 12 — one
  batch committed, the second not started. That is `L-1` being observed rather than assumed.

### 12.4 Falsification record — `NP-CONV-001` §8, in both directions

§8's rule is that a check must be falsified before it is trusted. For a power-loss test that means
two separate things, and both were done. **Direction 1** is built into the suite and re-runs on
every CI execution; the six perturbations **P1–P6** were performed by hand on 2026-09-14, with the
baseline confirmed to pass again after each, and they cover both directions.

**Direction 1 — the verifiers can see a violation.** Three deliberately unsafe commit orderings are
swept by the same verifiers, and the suite **fails if any of them survives**:

| Unsafe ordering | Attempts | Violations seen | Requirement |
|---|---|---|---|
| The log **rewritten in place** instead of appended | 624 | 550 | `L-2`'s check must notice a previously committed byte changing |
| The blob **removed, then rewritten** under the live name | 198 | 192 | `L-3`'s check must notice the live record absent, and it also caught a zero-length `npmp.bin` |
| The journal **truncated and rebuilt** instead of extended | 99 | 93 | `L-4`'s check must notice many records lost at once — and this one is `REQ-LFS-01`'s own subject matter: discard-and-rebuild applied to a record store, which §6.2 says is data loss |

**Direction 2 — the suite and the injector are not vacuous.** Asserted in the suite: a non-zero cut
count; **every armed index actually fired** (456/456 for the safe sweeps), so the coverage claim in
§12.2 is checked rather than asserted; a non-zero count of programs, erases *and* syncs; and an
uncut run that produces the complete expected content, because a scenario that silently wrote
nothing would pass every check above by having nothing to lose.

And then the suite was broken on purpose:

| # | Perturbation | Outcome |
|---|---|---|
| **P1** | The injector never cuts (`take_op()` returns false) | **Caught** — 4 failures, including *"no cut fired anywhere in this suite — every scenario above ran to completion and proved nothing"* |
| **P2** | The **safe** blob replacement swapped for the unsafe ordering | **Caught** — `L-3` fails |
| **P3** | `L-2`'s committed-prefix comparison disabled | **Caught** — the in-place-rewrite falsification stops seeing violations and the suite says so: *"it cannot distinguish append from overwrite, so the `L-1`/`L-2` result above is vacuous"* |
| **P4** | The cut no longer destroys littlefs's RAM state (`lfs_t` reused across the reboot) | **NOT caught — recorded as a negative result rather than dropped.** `lfs_mount()` re-initialises the structure from `lfs_config`, so clearing it changes nothing. The `memset` stays because it models the reboot honestly, but **no result in §12.2 rests on it**, and a reader must not cite it as a control |
| **P5** | The flush-boundary set narrowed to the durable prefix alone | **Caught** — the check fires, and in firing it reported the 192-record observation in §12.3 |
| **P6** | The pre-state no longer restored between attempts | **Caught** — armed indices stop firing, because the op sequence is no longer the one that was measured |

P4 is in this table because a falsification record that lists only the perturbations that were
caught is a record of what somebody chose to try.

### 12.5 What §12 does and does not establish

**May be cited for:** `L-1`, `L-2`, `L-3` and `L-4` **against the `struct lfs_config` contract**, at
`v2.11.3`, under NeurOne's build configuration and the `EMMC-FS-01` instance parameters, with the
caveat that `L-3` is a property of the write-temp-then-rename **ordering** and not of the component.
`NP-FW-NVRAM-001` §4, `EMMC-FS-01` and `NP-FW-HUB-001` §6.5 may be relied on to that extent, which
is the extent they were written at.

**May NOT be cited for:**

- **The eMMC.** The contract is interrupted at `prog_size` granularity; the medium is managed flash
  behind an XTS layer that may tear inside a 512 B sector, may reorder, and whose erase semantics
  are not raw-flash semantics. `OI-LFS-07`, and it needs hardware.
- **The log partitions.** These sweeps mount the **Config** instance, because it is the only one
  whose parameters any document states. `L-1` and `L-2` are claims about the UHDR and SHDR log
  partitions, whose `block_count` is 131,072 and 1,767,168 against Config's 4,096. `OI-LFS-05`.
- **Integration.** Nothing in the firmware calls littlefs. The suite supplies its own block device
  precisely because the device's own (`OI-LOG-05..07`) does not exist, and a passing test against a
  test block device is not a working storage stack.
- **A hang.** §12 cannot distinguish a post-power-loss hang from a slow run (§11.3, #1211).
- **`L-6`.** Wear levelling is configured and checked by value; nothing here measures it.

---

## 13. The caller rules, built — `OI-LFS-03`, `-05`, `-06`, `-08`, `-09` (performed 2026-09-24, Rev 4)

> §11 ended with a set of constraints on code that did not exist, and said that was the cheapest
> moment to state them. This section is the code, and what it was shown to do. GitHub Issue #382.
> Two items are **not** closed here and are not closable on a host: `OI-LFS-07` (the eMMC) and
> `OI-LFS-04` (a Safety + EE question). One new defect was found in the record itself (§13.1.2) and
> one in NeurOne's own configuration (§13.1.3, `OI-LFS-10`).

### 13.0 What was built

| File | What it is |
|---|---|
| `firmware/hub_control/include/np_cfg_store.h`, `src/np_cfg_store.c` | **The Config store** — the only caller of the Config instance. Each §11 caller rule is a property of its API rather than an instruction to its users |
| `firmware/hub_control/include/np_lfs_log_instance.h`, `src/np_lfs_log_instance.c` | **The UHDR and SHDR instances** — parameters, mount-time validator (claim `L-5` for the logs), and the allocator primer |
| `firmware/hub_control/tests/np_cfg_store_tests.c` | Class B host target 33 |
| `firmware/hub_control/tests/np_lfs_log_instance_tests.c` | Class B host target 34 — Class B 32 → **34**, repo total 42 → **44**, re-derived with `ctest -N` |
| `firmware/hub_control/tests/np_lfs_sweep.{h,c}` | The §12 sweep method as a reusable harness, so §12's own suite stays byte-identical and its recorded results stay results about the code that produced them |
| `firmware/hub_control/tests/np_lfs_powerbd.{h,c}` | Three additions, all **off by default** so `np_lfs_powerloss_tests` runs against exactly the device it always did: a sparse medium, read-error injection, and a read-modify-write tear unit |
| `scripts/check-lfs-caller-rules.ts` | **The `OI-LFS-03` / `OI-LFS-06` gate**, run by `tooling-ci.yml` job `lfs-caller-rules` after its own `--self-test` (21 cases) |
| `np_hub_types.h` | Three statuses: `NP_HUB_ERR_STORE_BUSY` / `_IO` / `_INTEGRITY` |

**Nothing here is integrated, and that has not changed.** The block device over the XTS-mounted
partition is still `OI-LOG-05..07`; `np_cfg_store` and the log instances compile into
`np_hub_control` under the ARM toolchain and no code path in the image calls them, so the link drops
them. What moved is that the rules the glue must obey now exist as code that is tested, rather than
as sentences in §11.5.

### 13.1 `OI-LFS-05` — the log instances' parameters (CLOSED)

#### 13.1.1 The decision

| Parameter | UHDR | SHDR | Source |
|---|---|---|---|
| `block_size` | 4,096 | 4,096 | `EMMC-FS-01`, `EMMC-FS-02` |
| `block_count` | 1,767,168 | 131,072 | `EMMC-FS-01` — and `_Static_assert`ed against `np_config.h`'s partition map, not against the table's own division |
| **`read_size`, `prog_size`** | **512** | **512** | **DEVIATION** from `EMMC-FS-01`'s 256 — §13.1.3, `ECR-EMMC-002` |
| `cache_size` | 4,096 | 4,096 | `EMMC-FS-01` |
| `lookahead_size` | 512 | 256 | `EMMC-FS-01` — examined and kept, §13.1.4 |
| `block_cycles` | 500 | 500 | `EMMC-FS-01`; claim `L-6`, and `OI-LFS-07` |
| `file_max` | 2,147,483,647 | 2,147,483,647 | `EMMC-FS-01` (= `LFS_FILE_MAX`) — but see `OI-LFS-11` |
| `name_max`, `attr_max`, `metadata_max` | 255, 1,022, 4,096 | same | `EMMC-FS-01` |

Static RAM for both instances: **17,152 B** (two 4 KiB caches each, plus 768 B of lookahead).

#### 13.1.2 The premise `OI-LFS-05` was raised on was wrong — and so is Rev 2's reading of `EMMC-FS-01`

`OI-LFS-05` said `EMMC-FS-01` *"states parameters for the Config partition only"*, and
`np_lfs_instance.h` says the same in code: *"The five EMMC-FS-01 owns"* and *"The four EMMC-FS-01
does not state — NeurOne decisions"*. **`NP-FW-EMMC-001` §5.2's `EMMC-FS-01` table has a UHDR, an
SHDR and a Config column, each with twelve parameters.** It states every value Rev 2 thought it did
not. That is recorded rather than quietly fixed, because it is the second time in this document that
a claim about what another record says was taken on trust (`NP-CONV-001` §7).

For the logs the consequence is benign: the decision became *take the specified values and find out
whether they survive*, and eleven of twelve do. **For the Config instance it is not benign** — the
code disagrees with its own specification on five fields, and one of the five is the hazard below.
That is `OI-LFS-10`.

| Config field | `EMMC-FS-01` | Built (`np_lfs_instance.h`) |
|---|---|---|
| `read_size` / `prog_size` | 256 / 256 | 256 / 256 — **and both are below the XTS unit, §13.1.3** |
| `cache_size` | 512 | 256 |
| `lookahead_size` | 64 | 512 |
| `block_cycles` | 200 | 500 |
| `name_max` / `attr_max` | 64 / 256 | not set (255 / 1,022) |

#### 13.1.3 The deviation: a program smaller than the XTS data unit is not a program

`EMMC-FS-01` prints `read_size`/`prog_size` 256. `EMMC-UHDR-05` sets the AES-XTS data unit to
**512 bytes, "matching the LittleFS read_size and prog_size"**, and `EMMC-SHDR-03` inherits it. Both
cannot hold. The clause that states its reason wins, and the reason is a hazard, not a preference:
XTS encrypts a whole data unit under one tweak, so a 256-byte program into a 512-byte unit makes the
encryption layer read, decrypt, merge, re-encrypt and **rewrite the whole unit**. A power loss inside
that rewrite damages the other 256 bytes — bytes littlefs already committed and synced. The one
property of `struct lfs_config` that `L-1…L-4` rest on is that a program disturbs nothing outside
itself, and this breaks it from below.

**Shown, not argued.** `np_lfs_powerbd` gained an `rmw_unit`: under a `PARTIAL` tear, a program
smaller than the unit leaves the **whole enclosing unit** indeterminate. Same sweep, same verifier:

| Instance | `prog_size` | RMW unit | Attempts | Violations |
|---|---|---|---|---|
| SHDR, as decided | 512 | 512 | 66 | **0** |
| SHDR, as `EMMC-FS-01` prints it | 256 | 512 | 66 | **1** — not a flush boundary (`L-1`) |
| SHDR, control | 256 | none | 66 | 0 — so the failure is the unit's, not 256's |
| **Config, as built today** | 256 | 512 | 123 | **1** — a durable Map 3 record lost (`L-4`) |
| Config, control | 256 | none | 123 | 0 |

One violation in 66 is not a rate anyone should read anything into; it is an existence proof, which
is what a falsification needs. The model is the **worst** case (the whole unit indeterminate) — a
real FTL might tear more gently, which is exactly the thing only `OI-LFS-07` can measure.

**`ECR-EMMC-002`** *(APPLIED at Rev 6 — `NP-FW-EMMC-001` Rev 3, all three columns, §13.9)* — raised against `NP-FW-EMMC-001` (a `.docx`, so not edited here, `OI-CONV-04`),
on the `ECR-EMMC-001` pattern. Clause `EMMC-FS-01`, rows `read_size` and `prog_size`, UHDR and SHDR
columns: replace *"256 bytes"* with *"512 bytes (= the XTS data unit, EMMC-UHDR-05; a smaller
program forces a read-modify-write of a unit whose other half littlefs has already committed)"*.
The Config column is `OI-LFS-10`'s decision and is deliberately not in this ECR.

Enforced three ways: `np_lfs_log_config_validate()` refuses 256; a `_Static_assert` requires
`prog_size` to be a whole number of 512-byte units; and the test above fails if the RMW model ever
stops producing the violation, which would leave the deviation with no evidence behind it.

#### 13.1.4 `lookahead_size` — examined, kept, and its cost measured

The specified windows cover 4,096 blocks (UHDR, 16 MiB) and 2,048 (SHDR, 8 MiB). Each time one is
used up, `lfs_alloc_scan()` traverses **the whole filesystem** — cost proportional to blocks in use —
and the writer waits. Full coverage would move every traversal to mount, at **220,896 B + 16,384 B**
of static RAM on a processor with no external SDRAM (`NP-SW-CI-001` §4.13, `OI-SWCI-46`). What fails
if the window stays small is a stall of the session logger whose **duration** is per-read eMMC
latency × blocks in use — a property of silicon. CLAUDE.md §18: a constraint needs a failure it
prevents, and this one's size is unmeasured. So the specified value stands, and the **count** is
measured instead:

| UHDR geometry, 16,384 blocks (64 MiB) in use | |
|---|---|
| Reads in one full traversal | **8,292** (≈ 0.5 per block in use) |
| A 6,144-block (24 MiB, ~30 min of EEG) session, unprimed | **2** traversals, worst single write 10,319 reads |
| The same session after `np_lfs_log_prime_allocator()` at mount | **1** traversal |

Extrapolated, a near-full UHDR partition costs on the order of **880,000 reads per traversal**, one
per 16 MiB written. Whether that is milliseconds or minutes is the eMMC's answer, and **it is added to
`OI-LFS-07`'s bring-up list**: measure one traversal's time at a realistic fill; if it exceeds the
session logger's buffering headroom, revisit this value with the measurement in hand. What is done
now costs nothing: `np_lfs_log_prime_allocator()` runs `lfs_fs_gc()` at mount, so the one traversal
that is certain happens where a delay costs nothing instead of inside the first session's first
append.

#### 13.1.5 What else the decision surfaced

`file_max` = `LFS_FILE_MAX` is right for `EMMC-UHDR-12`'s one-file-per-session layout and **wrong for
`np_log_backend.h`'s "the append-mode log file"** — one file per partition cannot exceed 2 GiB, under
a third of the UHDR partition. That is a finding against the log backend's layout, not against the
parameter: `OI-LFS-11`, for `OI-LOG-05`.

`OI-FAULTMSG-03` named `OI-LFS-05` as its storage blocker. **The parameters are decided; the UHDR
file glue those parameters are for is still `OI-LOG-05..07` (#340)** — so `OI-FAULTMSG-03` narrows
rather than unblocks.

### 13.2 `OI-LFS-09` — content, on every read, and never retried (CLOSED)

**There is no unverified read in `np_cfg_store`.** Every read takes a mandatory content check
(`NULL` is refused) and runs it on the bytes just read; there is no `stat`. Tested: a never-written
file reads as absent; **an existing, empty file (upstream #1164) is refused**; a single flipped bit in
a data block — which littlefs does not checksum — is refused; and the same corrupted read with an
accept-anything check comes back OK, so the refusal is the check's doing.

**Upstream #1205 was reproduced on `v2.11.3`.** Through raw littlefs: read the front of a
four-block file (the file cache now holds block X), seek into block Y, fail one read of Y, retry —
**the retry returns block X's bytes and reports success**. §11.3 assessed #1205 from its report; it
is now a NeurOne observation, and `np_cfg_store_tests` fails if it ever stops reproducing (which
would mean either the defect is gone — re-run §11 — or the mitigation is shown against nothing).

**Through the store it does not reach a caller**, for two reasons that turned out to be separable:

1. **A handle never outlives the call that opened it**, so a poisoned *file* cache dies with its
   handle. This alone stops the scenario above.
2. **A read error forces a remount before the next operation.** Hand-falsifying the remount showed
   (1) was hiding a second path: a failed read of the root **metadata** pair poisons littlefs's own
   read cache, which outlives every handle — without the remount, the next fault-free read reported
   `npmp.bin` as **absent**. With it, correct. `lfs_mount()` → `lfs_init()` zeroes both caches; it is
   the only public-API way to drop them, and patching `lfs_bd_read()` is not available (byte-exact
   SOUP).

And one defect in the store, found by its own test: when the remount *itself* met the fault, the
store stayed unmounted for the rest of the power cycle — every later read an absence, with nothing
wrong. A failed remount now stays pending and retries on the next call.

### 13.3 `OI-LFS-06` — one open handle per file (CLOSED)

Every handle on the Config instance comes from one static function, `cfg_open()`, which keeps a
registry and refuses a second handle on an open file (`NP_HUB_ERR_STORE_BUSY`). Tested through a
host-only hook that holds one handle and asks for another. **What makes it checkable rather than
true-by-construction** is the gate (§13.5 R1, R2): no translation unit in `firmware/` outside the
listed callers may call littlefs at all, `lfs_file_open()` is called nowhere, and
`lfs_file_opencfg()` exactly once, inside `cfg_open()`.

### 13.4 `OI-LFS-08` — bounded churn, and `ukmd.rec` on two entries (CLOSED)

**Churn.** The store never removes and never renames. A whole-file replacement is an **in-place
`O_CREAT | O_TRUNC` rewrite**: littlefs's truncation is lazy and is committed together with the new
contents at close, so the old file survives a cut at any point before that commit. **§12's `L-3`
result was about write-temp-then-rename, and this is a different ordering — so it was re-swept:**
186 interrupted runs, 0 violations, with the remove-then-write ordering required to fail under the
same verifier (192 of 198). Over 850 writes across all three files the partition sees **exactly one
create per directory and file, and zero removes**; after a reboot, zero creates at all — a create
there would mean a file was lost and re-made.

**`ukmd.rec`.** Two copies, `ra/ukmd.rec` and `rb/ukmd.rec`, in two **directories** — two chains of
metadata pairs, because #1210 acts on one pair while compacting it, and two names in one directory
would share the pair. Each copy is an envelope (magic, generation, length, CRC-32), because the store
must tell a good copy from a bad one **without the user's key** — `ukmd.rec`'s own GCM tag is still
checked by `np_uhdr_key` at every unlock. Written A then B; read picks the valid copy with the higher
generation and **repairs the twin on first use**. Tested: copy A's entry deleted outright (#1210's
consequence, applied directly) → served from B, A restored; B's content damaged → served from A, B
restored; A left one generation behind → the newer copy wins; both gone → an absence, never a
different record. Swept under power loss (12 runs, 0 violations — two inline commits, so few ops),
with "both copies destroyed first" required to fail (12 of 36).

**What was not shown, and is recorded as a negative result.** #1210 was **not reproduced** on
`v2.11.3`: 3,000 cycles of the old write-temp-then-rename churn beside `ukmd.rec` in one directory,
blob inline and not, lost nothing. So the suite cannot show the store *preventing* #1210. It shows
the two things the store does about it — the trigger bounded to a constant, and the one
unrecoverable file no longer resting on one entry. A later reproduction belongs in
`np_cfg_store_tests`, not in prose.

### 13.5 `OI-LFS-03` — `REQ-LFS-01` made observable (CLOSED)

Two halves. **In the API:** every Config file has one row in `s_files[]` with a policy
(`REBUILD`, `TAIL_ADDITIVE`, `REPLICATED`), and the policy decides which call may touch it — the
rebuild-cache reader refuses the journal, the journal reader refuses a rebuild cache, and nothing
appends to a rebuild cache. **In CI:** `scripts/check-lfs-caller-rules.ts`, on the
`warranty-nojoin-ci.yml` pattern — its self-test runs before it, proving each rule rejects and each
vacuity path refuses (21 cases):

| Rule | Checks | Item |
|---|---|---|
| R1 | only listed files call littlefs; everything else goes through the store | `OI-LFS-06`, `-09` |
| R2 | no `lfs_file_open()`; `lfs_file_opencfg()` exactly once, inside `cfg_open()` | `OI-LFS-06` |
| R3 | no `lfs_remove` / `lfs_rename` / `lfs_stat` in the Config store | `OI-LFS-08`, `-09` |
| R4 | the `TAIL_ADDITIVE` rows are exactly `NP_CFG_FILE_MAP3`; every file has a row | `REQ-LFS-01` |
| R5 | `np_cfg_store_journal_read()` is called only from `JOURNAL_READERS`, each with why it reads history (none yet — Map 3 has no consumer) | `REQ-LFS-01` |
| R6 | no emission-limit consumer names the journal — its id, its API or its path — and every listed consumer exists | `REQ-LFS-01` |

**Its reach, stated narrowly:** textual, like its siblings. It sees a direct call and a direct name,
not a value handed through a helper — which is why a `JOURNAL_READERS` entry must carry its reason:
the gate forces the question where a reader is added, the one place a text check can ask it.

### 13.6 Falsification record — `NP-CONV-001` §8

Built into the suites and re-run on every CI execution: the unsafe orderings above, the RMW control,
and "every armed cut fired" for every sweep. Performed by hand on 2026-09-24, baseline confirmed to
pass again after each:

| # | Perturbation | Outcome |
|---|---|---|
| S1 | Remount after a read error removed | **Caught** — first only by the remount counter, which is how §13.2's metadata-pair path was found; then by behaviour, once that path became a test |
| S2 | Handle registry disabled | **Caught** |
| S3 | Content check skipped after the read | **Caught** — the empty file and the flipped bit both accepted |
| S4 | Replacement done as remove-then-write | **Caught** — `L-3` sweep fails |
| S5 | Replica repair disabled | **Caught** — six assertions |
| S6 | Only copy A ever written | **Caught** — the suite aborts on a littlefs assertion |
| S7 | Replica generation ignored (always A) | **Caught** |
| S8 | Journal appended with `O_TRUNC` | **Caught** — `L-4` sweep fails at every cut |
| S9 | A failed remount left unmounted (the defect §13.2 found) | **Caught** |
| Q1 | RMW tear model disabled in the block device | **Caught** — both "256 must fail" cases fail |
| Q2 | Log validator skips the limits | **Caught** |
| Q3 | `np_lfs_log_prime_allocator()` a no-op | **Caught** |
| Q4 | Log validator accepts 256 | **Caught** — six refusals missing |

### 13.7 What §13 does and does not establish

**May be cited for:** the §11.5 caller rules existing as tested code for the Config instance; #1205
reproduced on `v2.11.3` and shown not to reach a store caller; `L-3` for the in-place replacement and
`L-4` through the store, against the `lfs_config` contract; the log instances' parameters, mount-time
validation, and `L-1`/`L-2` on their real geometry against the contract; `REQ-LFS-01` enforced in CI.

**May NOT be cited for:**

- **The eMMC** — unchanged from §12.5. `OI-LFS-07`, now also carrying the traversal-time
  measurement (§13.1.4).
- **The Config instance under XTS** *(Rev 4 wording; resolved at Rev 5, §13.8)*. §13.1.3 shows its
  `prog_size` 256 losing a committed record under a 512-byte RMW unit. Until `OI-LFS-10` is decided,
  every Config result in §12 and §13 holds against the contract **only** — which is where it always
  stood, but there is now a specific reason the medium might not honour it.
- **#1210.** Not reproduced, so not shown prevented (§13.4).
- **Integration.** `OI-LOG-05..07` are unwritten and nothing in the image calls the store.
- **`OI-LFS-04`.** A Class C bound on per-tile emitter drive current is a Safety + EE question. From
  the firmware side, for the record: `firmware/safety_mcu/` owns the cranial and 1170 nm PBM
  **enable** lines and cuts them on over-temperature (`np_thermal_interlock.c`); it holds **no bound
  on drive magnitude**. Whether the drive stage has one in hardware is not answerable from this
  repository.

### 13.8 `OI-LFS-10` — the Config instance moves to 512-byte programs (Rev 5, 2026-09-24)

**Decided by the principal:** the Config instance takes the same program unit as the log
instances. `read_size` and `prog_size` go from 256 to **512**, the XTS data unit (`EMMC-UHDR-05`),
for §13.1.3's reason — a smaller program makes the encryption layer rewrite a whole unit whose other
half littlefs has already committed. **`cache_size` follows to 512**, not by choice: `lfs_init()`
requires `cache_size` to be a multiple of `prog_size`, and 512 is the smallest that is. It is also
`EMMC-FS-01`'s own Config value, so that field now agrees with the table.

| | Rev 4 (built) | Rev 5 (built) | `EMMC-FS-01` |
|---|---|---|---|
| `read_size` / `prog_size` | 256 / 256 | **512 / 512** | 256 / 256 → `ECR-EMMC-002` |
| `cache_size` | 256 | **512** | 512 |
| `inline_max` (derived) | 256 | 512 | — |
| Static RAM | 1,024 B + 256 B per open file | **1,536 B + 512 B per open file** | — |

**What it does not move.** `ukmd.rec` (a 208-byte envelope) and Map 3's 32-byte records stay inline:
`inline_max` rose to 512 B. The file layout, the store and its caller rules are unchanged.
`ECR-EMMC-002` now covers the Config column's `read_size`/`prog_size` as well as UHDR and SHDR.

**Re-run on the instance as built** (the §12 and §13 figures above are the record at 256 and are
not edited):

| Suite | At 512 | Result |
|---|---|---|
| §12 `np_lfs_powerloss_tests` — `L-1`/`L-2`, `L-3`, `L-4` | 60 + 117 + 108 = **285** interrupted runs | **0 violations**; all three unsafe orderings still caught (343, 111, 54 violations) |
| §13 `np_cfg_store_tests` — in-place replace, journal, replicated write | 105 + 111 + 12 = **228** | **0 violations**; both falsifications still caught |
| Config journal through the store under a **512-byte RMW unit** | 87 | **0** — this case expected a loss at Rev 4, and found one |
| Config geometry at **256** under the same RMW unit (raw littlefs — the store now refuses 256) | 123 | **1** — required; keeps the evidence for the decision live |

Op counts fell by roughly a third at 512 — fewer, larger programs — which is why the attempt counts
are lower than at Rev 4; every armed cut fired in every sweep.

**Enforced three ways:** `_Static_assert(NP_LFS_CFG_PROG_SIZE % 512 == 0)` in
`np_lfs_instance.c` — verified by building at 256, which no longer compiles;
`np_lfs_config_tests` pins 512 and requires both 256 values to be refused by
`np_lfs_config_validate()`; and the RMW sweep above.

**Still open in `OI-LFS-10`** *(closed at Rev 6, §13.9 — all three adopted from `EMMC-FS-01`)*: `lookahead_size` (512 built, 64 specified), `block_cycles` (500 vs
200) and `name_max`/`attr_max` (unset vs 64/256). None bears on power-loss behaviour. The first is a
performance choice — 512 B covers the whole 4,096-block partition in one allocator pass, 64 B
covers an eighth of it — and all three need either adopting or an ECR.

### 13.9 `OI-LFS-10` closed, `ECR-EMMC-002` applied (Rev 6, 2026-09-24)

**The remaining Config fields take `EMMC-FS-01`'s values**, on the principal's decision:

| Field | Built at Rev 5 | Built at Rev 6 = `EMMC-FS-01` | Consequence |
|---|---|---|---|
| `lookahead_size` | 512 (whole partition per pass) | **64** (512 blocks, 2 MiB, per pass) | the allocator traverses once per 512 blocks allocated; on a partition holding a handful of files, a traversal reads a few metadata pairs and CTZ pointers |
| `block_cycles` | 500 | **200** | metadata pairs relocate more often — more even wear, slightly more writes; still explicit, still neither 0 nor −1 (`L-6`) |
| `name_max` | unset (255) | **64** | recorded in the superblock at format; the longest Config name is `ukmd.rec` |
| `attr_max` | unset (1,022) | **256** | bounds `inline_max`, which falls from 512 to **256 B** — Map 3 records (32 B) and `ukmd.rec`'s 208-byte envelope stay inline |
| `metadata_max` | unset (= `block_size`) | **4,096**, stated | no behavioural change; now validated rather than defaulted |

`np_lfs_config_apply()` sets all five and `np_lfs_config_validate()` refuses any other value —
`name_max`/`attr_max` because littlefs writes them into the superblock and refuses a mount whose
config is smaller than the disk's, so a drift is a latent mount failure. `np_lfs_config_tests`
pins them and requires five new perturbations to be refused, including each pre-`OI-LFS-10` value.
Static RAM: 1,088 B + 512 B per open file.

**`ECR-EMMC-002` applied — `NP-FW-EMMC-001` Rev 2 → 3.** `editscripts/patch_emmc_ecr002_prog512.py`
changes the six `read_size`/`prog_size` cells of the `EMMC-FS-01` table to *"512 bytes (= the XTS
data unit, EMMC-UHDR-05) [ECR-EMMC-002, Rev 3; was 256 bytes]"*, places a banner under the table,
and adds the header revision and a revision-history row. It follows the existing patch-script
convention (`patch_art07_tool_shell_f02_retirement.py`): nothing deleted, the superseded value
retained, idempotent (re-running is a no-op, verified). `OI-CONV-04`'s concern — revision strings
split across Word runs — was checked for this file: the header line and every changed cell are a
single run each. `EMMC-UHDR-05`'s *"matching the LittleFS read_size and prog_size"* is now true.
`editscripts/generate_fw_emmc_001.py` still prints 256; it already predates Rev 2 and regenerating
from it would discard both revisions, so it is not the record — the patched `.docx` is.

**Where that leaves all three instances:** each equals `NP-FW-EMMC-001` Rev 3's `EMMC-FS-01` column
exactly. The word *"deviation"* in §13.1 and §13.8 now means *"from Rev 2 of the specification"*.

**Re-run on the instance as built:** §12's suite 285 interrupted runs; the store's in-place replace,
journal and replicated write 105 + 108 + 12 = 225; the Config journal under the 512-byte RMW model 84
— **0 violations**; the falsifications and the 256-under-RMW case still fail as required.

### 13.10 `OI-LFS-11` — one UHDR file per session (Rev 7, 2026-09-24)

**The problem.** `np_log_backend.h` specified *"the append-mode log file"* — one file per partition —
and littlefs caps a file at `file_max` = 2,147,483,647 B. That is under a third of the 6,903 MiB
UHDR partition, so the log would have stopped with two thirds of the partition free.

**The decision: the layout `NP-FW-EMMC-001` already specified.** Nothing new was invented:

| Partition | Layout | Why |
|---|---|---|
| UHDR | **one file per session**, `/uhdr/sessions/<count>` with the count zero-padded to 20 digits (`EMMC-UHDR-12`, `-13`) | a session cannot approach 2 GiB (≈49 h at the EEG rate), and a per-session file is the unit `EMMC-UHDR-12`'s record format, the backup manifest and `EMMC-WE-01`'s *"max file size in /uhdr/sessions/"* telemetry already assume |
| SHDR | **one file**, unchanged | the whole partition is 512 MiB, so no file on it can reach `file_max`; `EMMC-SHDR-08`'s per-category directories are a separate matter the logger does not yet split by |

**What changed in the code.**

- `np_log_backend`: `np_log_backend_session_begin(counter)` and `_session_end()`. UHDR is written only
  inside a session (`NP_HUB_ERR_NO_SESSION` outside one) — which also means it is no longer opened at
  boot, when it is not even mounted (`np_uhdr_key_unlock()` mounts it). A session's staged tail is
  flushed into **its own** file at end, or at the next begin if end was missed. A file that would pass
  `NP_LOG_SEGMENT_MAX_BYTES` (= `file_max`) fails the rest of that session closed and clears at the
  next begin. A new session's status reports only the new file.
- Session files are **created exclusively** and never reopened: an existing file returns the new
  `NP_HUB_ERR_LOG_EXISTS`. Appending a new session to an old file, or truncating it, would each be a
  way to corrupt or destroy a user's earlier session.
- `np_session_log`: `np_log_session_start()` drains anything buffered into the previous file,
  increments the device session count (`EMMC-SHDR-09` — *incremented at session start*), and opens the
  file it names; `np_log_session_end()` writes the end records, drains, and closes. **The count is not
  persisted anywhere** (`OI-LFS-12`), so after a reboot it can repeat. The logger steps past any count
  whose file exists — up to `NP_LOG_SESSION_PROBE_MAX` (1,024) — so counts stay unique and monotonic
  and no session is lost to a stale count.
- Lower HAL: `np_log_hal_part_open(part, segment)` (UHDR: exclusive create of the session file; SHDR:
  the single file) and a new seam, `np_log_hal_part_close(part)` — SW-02 platform census **98 → 99**,
  which the cross-build asserts.

**Tested** in `np_log_backend_tests`, which now also links `np_session_log.c` and
`np_adaptation_log.c`: no UHDR outside a session; one file per session; the tail stays with its
session when a begin arrives without an end; an existing file refused with `LOG_EXISTS` and left
untouched; the per-file cap failing one session and not the next; the logger naming files by the
session count, the SHDR session-end record carrying the same count, and a stale post-reboot count
stepping past existing files. **Falsified by hand** four ways, each caught: the step-past loop removed
(2 failures), the drain at session end removed (3), the per-file cap removed (2), and
`_session_end()` not flushing (3).

**Not established.** The real `np_log_hal_part_*` over littlefs is still `OI-LOG-05..07`; the
exclusive-create and file-per-session behaviour are tested against the host model of that seam.
`np_lfs_log_instance` and `check-lfs-caller-rules.ts` already require that glue to be added to the
gate's caller list when it is written.

### 13.11 `OI-LFS-07` — what the host could decide, and the bring-up protocol for the rest (Rev 8)

`OI-LFS-07` asked four things of the eMMC + XTS stack. Two are decided here, two need silicon.

**1. What `erase()` does — DECIDED: nothing.** littlefs's contract (`lfs.h`): *"A block must be
erased before being programmed. The state of an erased block is undefined."* An eMMC has no
erase-before-program requirement — its FTL remaps every write — so the callback may be a no-op,
**provided littlefs never relies on an erased block reading as `0xFF`.** That proviso is testable on
the host and was tested: `np_lfs_powerbd` gained `erase_noop` (an erase still consumes an op index
and can be cut, but never changes the medium), proven active by a direct prog-then-erase check that
fails when the mode is switched off. With it set:

| Sweep | Runs | Violations |
|---|---|---|
| SHDR `L-1`/`L-2`, real geometry, no-op erase | 66 | **0** |
| Config `L-4` through `np_cfg_store`, no-op erase | 84 | **0** |

So NeurOne's block-device glue implements `erase()` as `return 0;` — no `CMD35/36/38`, which also
removes upstream #1083's erase-group question (384 KiB groups against 4 KiB blocks) entirely. A
discard/TRIM for wear is an optimisation that would need its own sweep; it is not required.

**2. What `block_cycles` means above an FTL — DECIDED: kept as specified.** It sets how often
littlefs relocates a metadata pair between *logical* blocks; the FTL levels *physical* wear
regardless. Keeping it costs a small amount of write amplification and buys nothing the FTL does not
already do — but it is `EMMC-FS-01`'s value, harmless, and `EMMC-WE-03` already requires the
firmware to measure write amplification and fault above 3×. `HW-LFS-05` confirms it with numbers.
Claim `L-6` is re-read accordingly: wear levelling is the eMMC's; littlefs's is belt and braces.

**3 and 4 — tear granularity and "reliable write", and the medium's behaviour under a real cut —
need silicon.** The bring-up protocol:

| ID | Test | Method | Pass criterion |
|---|---|---|---|
| **HW-LFS-01** | **Tear granularity** | Write known patterns to consecutive 512-byte units (XTS on, and once with XTS bypassed to separate the layers); cut eMMC `VCC` with a GPIO-driven load switch at random offsets inside `CMD24`/`CMD25`; after power-up classify every unit as old / new / mixed / damaged-but-not-in-flight. ≥ 1,000 cuts | **No unit other than the one in flight is ever damaged.** If one is, §13.1.3's RMW model is optimistic for this part: enable eMMC reliable write (`EXT_CSD` `WR_REL_SET`) and repeat |
| **HW-LFS-02** | **Power-loss sweeps on the real stack** | The §12/§13 scenarios — log append (`L-1`/`L-2`), in-place replace (`L-3`), journal (`L-4`), replicated `ukmd.rec`, per-session file open/close — run on target over the real XTS block device with `erase()` a no-op, power cut at random times by the same load switch. The host verifiers, compiled for target. ≥ 1,000 cuts per scenario | **0 violations, 0 mount failures, 0 hangs** (a hang = no SPI heartbeat within 5 s of power-up — upstream #1211's symptom, which only hardware can observe). With 0 failures in 1,000 cuts the per-cut failure probability is below 0.3 % at 95 % confidence (rule of three); a failure anywhere is a finding, not a retry |
| **HW-LFS-03** | **Mount after interrupted format and interrupted repair** | Cut during `np_cfg_store_format()` and during a replicated-record repair | Mount succeeds or reports cleanly; `ukmd.rec` never lost while one copy was valid before the cut |
| **HW-LFS-04** | **Allocator traversal time** | UHDR filled to 25 %, 50 % and 90 %; time `np_lfs_log_prime_allocator()` at mount and one traversal forced mid-session (§13.1.4 measured ≈ 0.5 reads per block in use) | A mid-session traversal completes inside the session logger's buffering headroom at the EEG rate; if not, `lookahead_size` (§13.1.4) or the logger's buffering is revisited **with this measurement** |
| **HW-LFS-05** | **Write amplification** | `EMMC-WE-03`'s WAF over a 100-session endurance run, with `EXT_CSD` life estimates before and after | WAF ≤ 3× (`EMMC-WE-03`'s fault threshold); the expected 1.1–1.5× is recorded against it |

**What this does not change.** `OI-LFS-07` stays **open** until `HW-LFS-01…05` have run: every
`L-1…L-4` result in this document is still a statement about the contract, and `RISK-LFS-06` stays
open. What changed is that the medium question is now a test list with numbers, and one of its four
sub-questions is closed.

### 13.12 `OI-LFS-04` — is there a Class C bound on per-tile drive current? The answer is no (Rev 8)

`OI-LFS-04` (= `OI-NVRAM-10`) asks whether anything in SW-01 — the Class C safety MCU — bounds a
tile's emitter drive current independently of Map 1's ranges. It is Safety + EE's to *decide*; it
is answerable from the record, and the answer is **no**.

**What the Class C side has.** `firmware/safety_mcu/` owns the cranial and 1,170 nm PBM **enable**
lines (`np_gpio_mgr.c`) and cuts them on over-temperature (`np_thermal_interlock.c`). It has no
per-tile current sense, no per-tile magnitude limit, and no wire-format field that carries one.

**What sets the current.** `NP-HW-HEXTILE-001` §6.2 specifies the on-module driver as the tile MCU's
PWM switching a low-side N-FET, with a series sense resistor per channel — inherited from
`NP-HW-FPC-001` §6.2, whose text is explicit: *"the FETs switch LED current via series sense
resistors"*, with over-current **detected by the tile firmware** reading that resistor (220 mA). In
that topology the magnitude while the FET is on is set by **hardware** — bus voltage, the string's
forward voltage, the resistor — and **not by Map 1**; Map 1's ranges reach the emitters only through
firmware, as duty or intensity codes. So:

| Path | Bounded by | Class |
|---|---|---|
| Peak current while on | the string's I–V curve against 24 V and the sense resistor — **unregulated**; forward voltage falls ~2 mV/°C per emitter, so a hot string draws more | hardware, uncontrolled |
| Average irradiance (duty) | hub firmware (≤ 25 % duty, the power governor) and the tile firmware | Class B |
| Over-current | tile firmware ADC threshold | Class B (tile) |
| Everything, eventually | the safety MCU's thermal cut of the enable line | **Class C — the only one** |

An out-of-range or substituted Map 1 entry therefore reaches **average** irradiance through Class B
firmware, with the 62 °C cut as the only Class C backstop: `OI-NVRAM-10`'s *"thermal limit standing
in for an optical one"*, confirmed rather than hypothetical. §6.1's classification stands on its
stated condition (`REQ-LFS-01` keeps limits in the reject-and-rebuild cache), and that condition is
enforced in CI (§13.5) — but the backstop behind it is thermal.

**A defect in the hardware record, found on the way.** `NP-HW-HEXTILE-001` §8.1.1 describes the stage
as **constant-current** with a **dropout** below which *"control falls out of regulation"*, and §4.3.1
claims the 150 mA full-drive point lands on the 400 mW/cm² ceiling *"so the array cannot be commanded
past its own optical limit even before firmware intervenes."* Both require a regulating element — an
amplifier or regulator holding the sense-resistor voltage to a reference. §6.2's parts list has none
(U2 is the photodiode TIA), and its sense-resistor row cites *"value set by string current, §6.3"*,
where §6.3 is the rigidizer fit. **Either the stage regulates and its regulator is missing from the
specification, or it is switched and §4.3.1's hardware-ceiling claim does not hold.** Raised as
`NP-HW-HEXTILE-001` **`OI-HEXTILE-23`**.

**The options — for Safety + EE, not decided here:**

| | Option | Bounds | Cost |
|---|---|---|---|
| **A** | Specify a regulating constant-current stage per channel with a **fixed hardware reference** (sense resistor + reference + amplifier or a regulator IC) | **peak** current, independent of every firmware and of Map 1 — what §4.3.1 and §8.1.1 already assume | parts and area on a 22 × 14 mm rigidizer; resolves `OI-HEXTILE-23` |
| **B** | A **hardware duty / on-time limit** on each channel (e.g. a retriggerable one-shot on the gate) | **average** irradiance, independent of firmware | small; complements A |
| **C** | Accept the thermal cut as the only non-firmware bound and **re-derive** §6.1's argument with that stated | nothing new | documentation; the residual stays a thermal limit for an optical hazard |

A without B bounds the peak but not the duty; B without A bounds the duty of an unregulated peak.
**A + B together** would give a bound on optical output that no stored value can move — which is the
property `OI-NVRAM-10` was asking about. `OI-LFS-04` stays open until one is chosen.

### 13.13 `OI-LFS-12` — the device session count is persisted (Rev 9, 2026-09-24)

**The gap.** `EMMC-SHDR-09`: *"Session count is incremented in Config partition at session start; it
is the only time-like reference in SHDR."* §13.10 made `np_session_log` increment it, but nothing
wrote it back; bring-up seeded it from `np_hal_get_device_session_count()`, a platform seam that
nothing implemented. After a reboot the count restarted, UHDR survived only because the logger steps
past existing files, and SHDR records could carry a count that jumped.

**The fix.**

| Piece | What it does |
|---|---|
| `NP_CFG_FILE_SESSION_COUNT` | a new Config file, **`REPLICATED`** — two enveloped copies in two metadata pairs, the higher generation winning — because the count must never go backwards and must survive one lost entry. It bounds nothing, so `REQ-LFS-01` does not reach it; the gate's R4 sees its row |
| `np_session_count` | `np_session_count_load()` / `_commit()`: 4 bytes, little-endian, through `np_cfg_store_replicated_*`. Absent (a new device) or unreadable seeds **0** |
| `np_session_log` | `np_log_set_count_commit(fn)`: at each session start the new count is **committed, then** the session's UHDR file is created. A power loss between the two costs an unused count — a gap, never a reused one. A failed commit does not stop the session; the step-past (§13.10) remains the backstop |
| bring-up | `np_session_count_load()` seeds `np_log_init()`; `np_session_count_commit` is registered as the hook |
| cervical fault summary | `np_mod_cvns` records `np_log_session_count()` — the current session — instead of the boot-time seam value, which had stamped every fault of a boot with the same number |
| platform | `np_hal_get_device_session_count()` (`OI-HUB-MAIN-03`) **retired**: the count is first-party data in a first-party store, not a driver. SW-02 census **99 → 98**, asserted by the cross-build |

**Tested.** `np_cfg_store_tests`: a new device reads absent and seeds 0; the count survives a reboot;
losing one entry does not lose it; and a power-loss sweep of a commit (41 → 42) leaves 41 or 42 at
every cut — 12 runs, 0 violations. `np_log_backend_tests`: the count is committed before its file is
opened, and a reboot seeded from the committed value opens the next session's file first time, with
no stepping. **Falsified:** creating the file before committing fails the order test; a commit that
writes nothing fails five assertions.

**What remains.** Losing **both** copies of the record resets the seed to 0; the step-past then keeps
UHDR files unique, but SHDR counts would restart. That is the same exposure every replicated record
has, and it is observable (the `repairs` and `integrity_fails` counters in `np_cfg_store_stats`).
Nothing here is integrated until the Config block device exists (`OI-LOG-05..07`).

