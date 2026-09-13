# LittleFS SOUP Record and Hazard Analysis — Hub Storage (SW-02, Class B)

**Project:** NeurOne
**Document:** NP-SOUP-LFS-001
**Revision:** 1
**Date:** 2026-09-13
**Status:** DRAFT — the hazard analysis (§4–§6) is complete and does not depend on a version. The IEC 62304 §7.1.2 anomaly-list evaluation (§7) is **deliberately not performed**, because it cannot be: it evaluates the anomaly list *for the version actually in use*, and no version is in use. §7 states the blocking action instead of a conclusion.
**Effective Date:** —
**Author:** NeurOne Firmware Engineering
**Approved By:** — (DRAFT, not approved)
**References:** `NP-SW-001` Rev 5 §9.4 (SOUP register — this record replaces its LittleFS row, and its second Class B SOUP note carries this document's conclusion); `NP-SOUP-CMSIS-001` Rev 1 (method precedent); `NP-FW-EMMC-001` Rev 2 `EMMC-FS-01` (instance parameters), `EMMC-CFG-01/-02` (Config contents and write whitelist), `EMMC-HW-01` (endurance); `NP-FW-EMMC-002` Rev 2 §C (UHDR mount); `NP-FW-NVRAM-001` Rev 2 §3.3, §4 (power-loss atomicity), §9 (Class B argument); `NP-FW-HUB-001` Rev 1 §6.5 (log durability model); `NP-CONV-001` Rev 6 §8 (a check must be falsified before it is trusted); `firmware/hub_control/include/np_log_backend.h`
**Related Issues:** #339 (`OI-NVRAM-12`), #75 (`NP-SBOM-001` — T2 510(k) cybersecurity submission)
**Gate:** G2
**IEC 62304 Class:** SW-02 Class B
**Supersedes:** None — new document. It replaces the single verification cell that `NP-SW-001` §9.4 previously carried for this component.
**Review Cadence:** On any change to the pinned version, on first integration, and at G2

---

> **⚠ READ FIRST — the finding that reorders this document.**
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
| Version in use | **none — not integrated** (§2) |
| Version proposed for pinning | a named upstream release tag, selected at integration (§7.1) |
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

## 7. What remains, and why §7.1.2 is not discharged here

### 7.1 Pin a version — blocking, and blocking for two reasons

`2.x` cannot be evaluated and cannot be vendored. A named upstream release tag must be selected at
integration and recorded in `firmware/vendor/littlefs/VERSION` with per-file provenance and SHA-256,
following the `firmware/vendor/cmsis_core/VERSION` pattern.

**No version is pinned in this revision, deliberately.** Pinning a tag whose anomaly list has not
been read, in a document whose purpose is to record that the anomaly list was read, would reproduce
`OI-DOC-01` in the file written to close its sibling. `OI-LFS-01`.

The second reason it blocks: **#75 (`NP-SBOM-001`) needs a version.** An SBOM entry reading `2.x` is
not an SBOM entry, and the T2 510(k) cybersecurity submission consumes it.

### 7.2 Then perform the evaluation, and record NeurOne's own verification

Once a tag is pinned, §7.1.2's activity is: read the published anomaly list **for that tag**, assess
each item against the seven claims in §3, and record the assessment — the `NP-SOUP-CMSIS-001` §3–§4
shape. The verification cell then records what NeurOne ran, which must include at minimum a
**power-loss injection test against L-1…L-4**: interrupt a program operation at each of a set of
offsets and assert that the previously synced prefix is intact and that at most one record is lost.

Per `NP-CONV-001` §8 that test must be **falsified before it is trusted** — perturb the commit
ordering and confirm it fails — because a power-loss test that has never been seen to fail is
indistinguishable from a test that does not run. `OI-LFS-02`.

### 7.3 Configuration is part of the SOUP record

The vendored subset and its configuration must be recorded together, as the FreeRTOS row does. At
minimum: static allocation (no `malloc` on a device whose heap is a fixed 64 KiB FreeRTOS pool),
`block_cycles` set explicitly rather than defaulted (it governs wear levelling, which is L-6), and
the `EMMC-FS-01` instance parameters (L-5) asserted at mount rather than assumed.

---

## 8. Risk rows

| ID | Hazard | Sev | Mitigation | Residual |
|---|---|---|---|---|
| `RISK-LFS-01` | Session records silently truncated or lost | Low | flush bound (`NP-FW-HUB-001` §6.5); per-record tags make truncation detectable | Accepted |
| `RISK-LFS-02` | Corrupted safe range reaches emitter drive | **High** | blob CRC → reject-and-rebuild → empty inventory (§5.3); 62 °C throttle as the last bound | **Conditional** on `REQ-LFS-01`; re-derive if it is ever breached |
| `RISK-LFS-03` | Atomicity claims relied on before the component exists | Medium | this document; `OI-LFS-01`/`-02` block the reliance, not the design | **Open until integration** |
| `RISK-LFS-04` | An unpinned version reaches the SBOM and the 510(k) submission | Medium | `OI-LFS-01` blocks #75's entry | **Open** |

---

## 9. Open items

| ID | Description | Owner | Blocking |
|---|---|---|---|
| **`OI-LFS-01`** | **Pin a named upstream release tag and vendor it** under `firmware/vendor/littlefs/` with a `VERSION` record carrying per-file provenance and SHA-256, plus the subset and configuration per §7.3. Supersedes the version half of `OI-NVRAM-12` | FW + Quality | **BLOCKING — any reliance on `NP-FW-NVRAM-001` §4 or `NP-FW-HUB-001` §6.5; and #75 (`NP-SBOM-001`)** |
| **`OI-LFS-02`** | **Perform the §7.1.2 anomaly evaluation against the pinned tag**, and write the NeurOne power-loss injection test against `L-1…L-4`, **falsified in both directions first** per `NP-CONV-001` §8. Supersedes the evaluation half of `OI-NVRAM-12` | FW + Quality | `NP-SW-001` §9.4 verification cell; G2 |
| **`OI-LFS-03`** | **Write and falsify the CI check for `REQ-LFS-01`** — that no value bounding an emission is stored under a tail-additive policy, i.e. that Map 3's journal is never read as a limit. `warranty-nojoin-ci.yml` is the pattern | FW + Safety | `REQ-LFS-01` durability; §6.2 |
| **`OI-LFS-04`** | `OI-NVRAM-10` — is there a Class C bound on per-tile emitter drive current independent of Map 1's ranges? Carried here because §5.3's third barrier is a thermal limit standing in for an optical one, and this document is where that now has a named consequence | Safety + EE | **Class B classification durability** |

---

## 10. Revision history

| Rev | Date | Author | Description |
|---|---|---|---|
| 1 | 2026-09-13 | NeurOne Firmware Engineering | **Initial release — brings LittleFS under SOUP management and performs the hazard analysis `OI-NVRAM-12` requires (Issue #339).** Replaces `NP-SW-001` §9.4's single cell (*"2.x"* / *"Power-loss testing per LittleFS test suite"*). **Principal finding: LittleFS is not unmanaged, it is absent** — `grep -rn "lfs_" firmware/` returns two hits, both comments in `np_log_backend.h` naming the `OI-LOG-06/07` seams, so every power-loss atomicity guarantee in `NP-FW-NVRAM-001` §4, `EMMC-FS-01` and `NP-FW-HUB-001` §6.5 rests on a component with no source, no version and no NeurOne test. **Second finding, and the one that outlives integration: the Class B classification is not a property of LittleFS but of `np_module_map`'s reject-and-rebuild policy** — every integrity failure becomes an *empty* inventory rather than a *wrong* one — and `NP-FW-NVRAM-001` §7.2 specifies Map 3 to need the opposite, tail-additive policy for independently correct reasons. `REQ-LFS-01` is the rule that keeps both decisions safe: no value that bounds an emission may be stored under a tail-additive policy. **The §7.1.2 anomaly evaluation is deliberately not performed** — it evaluates the list for the version in use and none is in use; recording a conclusion against *"2.x"* would reproduce `OI-DOC-01` inside the document written to close its sibling. Four risk rows, four open items, two of which supersede the halves of `OI-NVRAM-12`. Feeds #75 (`NP-SBOM-001`). |
