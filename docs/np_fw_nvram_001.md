# Hub NVRAM Hardware Abstraction Layer and the On-Helmet Module Record

**Project:** NeurOne
**Document:** NP-FW-NVRAM-001
**Revision:** 1
**Date:** 2026-08-27
**Status:** DESIGN STUDY — not a release baseline. Every behaviour below is a proposed engineering commitment traced to a cited source. **No new part is proposed and no code changed with this document.** See §11 (Decisions), §13 (Open items).
**Effective Date:** —
**Author:** NeurOne Firmware + Data Architecture
**Approved By:** — (pending design review)
**References:** CLAUDE.md §4 (processor stack, safety architecture, power), §5 (UHDR/SHDR), §6 (consent subjects); `NP-FW-EMMC-001` Rev 2 §4 (partition layout), §5.2 (LittleFS instance parameters), §9 (Config/Calibration partition), §12 (session data classification), §14 (write-endurance monitoring), §16 (processor ownership); `NP-FW-EMMC-002` Rev 2 §A (warranty token), §B.2 (factory reset R-7), §C.3 (Config UKMD record), §G.2 (record-denominated windows), §H.3.1–H.3.2 (no wall clock); `NP-MOD-ID-001` Rev 1 §4 (`module_ref`), §5 (on-module odometer, MODID-4), §6 (history portability, MODID-5/6), §7.5.1.1 (the two absences), §9 (open items); `NP-HW-HEXTILE-001` Rev 8 §6.2 (U1 on-module MCU), §6.4 (driver + metering BOM), §7.2–7.3 (19-position socket, `SEAT#`, contact sequencing); `NP-DRV-SHELL-002` Rev 4 §5.1.4 (UID EEPROM deleted), §6 (`SAFE_EN[n]`), §10.1 (interconnect BOM); `NP-HEX-ZM-001` Rev 3 §4a (SMART-1, `check_placement`), §5.4a (cluster clamp); `NP-COST-001` Rev 2 §6 (OI-HEXTILE-06 options); `NP-NPPS-REF-001` Rev 14 §1.6 (no build-time cache of protocol content); `NP-SW-001` Rev 3 §3.2 (Class B rationale for SW-02), §5.2 (SW-02 module inventory), §9.4 (SOUP); `NP-CONV-001` Rev 6 §4, §6, §8; `firmware/hub_control/include/np_module_map.h`; `firmware/hub_control/src/np_module_map.c`; `firmware/bootloader/include/np_config.h`; `firmware/hub_control/include/np_log_backend.h`
**Related Issues:** —
**Gate:** — (no programme gate; this document specifies the resolution of `OI-HEXMAP-01`)
**IEC 62304 Class:** **SW-02 Class B.** Argued in §9, and the argument turns on the fact that the Class C processor has no electrical path to the store. **No SW-01 source changes and no bit added to any Class C wire format.**
**Supersedes:** None — new document.
**Parent Document:** `NP-FW-EMMC-001`

---

> **⚠ READ FIRST — what this document is, and the six claims in its own brief that did not survive
> checking.**
>
> A principal decision of 2026-08-25 established a four-map architecture for module identity and
> history: **Map 1** the module *type* map (`(major, minor)` → element types and ranges), **Map 2**
> the module *instance* map held by the control software, **Map 3** the *on-helmet* map of everything
> collected on a module since the last sync, and **Map 4** the module's own permanent record. Map 3
> lives in helmet NVRAM. This document specifies the hardware abstraction layer under it, and in
> doing so it has to specify what "helmet NVRAM" physically is, because nothing in the repository
> says.
>
> **Six things asserted as settled context are wrong, and the design changes because of them.**
>
> 1. **Map 4's substrate is not open, and it is not inside `OI-HEXTILE-06`.** `OI-HEXTILE-06` is a
>    *photodiode population* decision — `NP-HW-HEXTILE-001` §6.4 routes exactly three options to it
>    (populate fewer sockets; silicon PD on T1-A; one PD pair per cluster), and `NP-COST-001` §6 runs
>    all three. On-module non-volatile storage appears in none of them. It is **already specified**:
>    `NP-HW-HEXTILE-001` **D-3** fits a tinyAVR 2-series MCU (`U1`, ATtiny426/427-class) to *every*
>    tile type, and `NP-MOD-ID-001` **MODID-4** already specifies the odometer that lives in its
>    128 bytes of EEPROM, with a four-slot layout, a CRC bound to the module's own UID, and a
>    write-endurance budget. **Map 4 exists, is designed, and costs nothing.** §10.
> 2. **The module-change power cut is not a hub power cut.** `SAFE_EN[n]` gates a cluster's 24 V
>    high-side load switch (`NP-DRV-SHELL-002` §6) — it removes the *emitter rail*, not `VCC_3V3`
>    and not the hub, which is USB-C powered throughout (CLAUDE.md §4). So hub-side writes are not
>    "racing a power cut by design". The real power-loss adversary is USB-C disconnect and power-bank
>    depletion in Mode 3, which is **uncorrelated with module changes and can hit any write at any
>    time.** That makes the atomicity requirement *stronger* than the brief's framing, not weaker: it
>    cannot be scoped to module-change writes. §4.
> 3. **There is no write window to commit into, and the design must not assume one.** `SEAT#` breaks
>    first on extraction (`NP-HW-HEXTILE-001` §7.3 break order), which looks like a warning. But
>    `grep -rn "SEAT" firmware/` returns nothing — **`SEAT#` has no firmware consumer** — it is
>    aggregated at the cluster controller (Class B), and the pad-length stagger that would set the
>    window's duration is **not dimensioned anywhere in the document set**. A last-gasp write is
>    therefore unimplementable and unquantifiable. §4.3, `OI-NVRAM-04`.
> 4. **The Config partition has no room for the raw journal its own specification promises.**
>    `NP-FW-EMMC-001` **EMMC-CFG-02** reserves *"a dedicated Config partition journal area (raw-write,
>    no LittleFS overhead for high-frequency updates)"* — and **EMMC-FS-01** mounts LittleFS on Config
>    with `block_count` 4,096 × 4 KiB = **16 MiB, which is the entire partition.** The journal area has
>    no address space. `NP-FW-EMMC-002` §C.3's *"Config partition offset 0x1000 — UKMD record"* points
>    into LittleFS-managed blocks by the same arithmetic. Both are live defects on the exact store this
>    HAL needs. `OI-NVRAM-01`, **blocking**. §3.3.
> 5. **The version-bump window does not close at first ship. It closes the first time an unsynced
>    Map 3 row exists** — which is the first powered prototype in anyone's hands, not the first
>    customer unit. The brief's reasoning is right and its boundary is wrong, and the boundary is the
>    part that matters: `np_module_map`'s discard-and-rebuild is sound because the blob is a **cache**
>    of facts the sockets will re-answer. Map 3 is a **record** of facts nothing will re-answer.
>    Putting them in one blob converts a correct policy into data loss. §7.2.
> 6. **Two documents the brief names as house-style and precedent are not on `main`.**
>    `NP-FEAS-PBMCH-001` does not exist anywhere in the repository (`docs/np_feas_fnirs_001.md` does),
>    and `NP-FW-BENCH-001` exists only on an unmerged branch. Its no-wall-clock and classification
>    arguments are used here as *reasoning*, not cited as controlled precedent; the in-force precedent
>    for record-denominated windows is `NP-FW-EMMC-002` §H.3.1–H.3.2, which is on `main` and is what
>    §5 cites.
>
> **One further finding, recorded because it is on the store this HAL owns.** `np_module_map.h`'s
> NVRAM-sizing comment still states the **v2** figure — *"8 + 128*139 + 4 = 17,804 bytes"* — after the
> v3 calibration payload took `NP_HEXMAP_REC_BYTES` to 175 and the blob to 22,412 bytes, which is the
> number the code computes and the tests assert. Stale by one revision, in the header a future
> integrator sizes the partition region from. `OI-NVRAM-11`.

---

## 1. Scope

**In scope:** what the hub NVRAM HAL abstracts and what it deliberately does not; the physical medium
and why; the commit discipline and what a torn write degrades to; the ordinal that replaces wall
time; the sync protocol between Maps 2, 3 and 4 and the reconciliation rule when two of them
disagree; the blob version bump and the unknown-`(major, minor)` path; the UHDR/SHDR class of every
field introduced; the IEC 62304 classification; and the cost, in parts and in what is lost by not
doing it.

**Out of scope:** the on-module odometer's own format and firmware, which is `NP-MOD-ID-001` §5 and is
not re-opened here; the SHDR fleet schema DDL; the module I2C inventory link (`OI-HEXMAP-02`); the
control software's own storage of Map 2; and the generation of Map 1 as a build artefact, which is an
app-toolchain deliverable (§7.1).

**This document is the specification of `OI-HEXMAP-01`** — *"Config-partition NVRAM HAL for the module
map"*, open since `NP-HEX-ZM-001` §7 and today an unimplemented pair of externs
(`np_hexmap_nvram_read` / `np_hexmap_nvram_write`, `np_module_map.h`). It is not a new workstream and
it does not reinvent one: the serializer, the CRC, the fail-closed restore contract and 1,278 lines of
host tests already exist and are correct. What is missing is the layer beneath them, and one
architectural decision — §3.2 — that the existing code silently assumes and must not.

---

## 2. What the hub already persists, and which of it this HAL owns

A HAL that names its store loosely will be pointed at the wrong partition by the next integrator. So
the list is enumerated first, from the partition table in `firmware/bootloader/include/np_config.h`
and `NP-FW-EMMC-001` §4.

| # | Store | Medium | Contents | Owned here? |
|---|---|---|---|---|
| 1 | Firmware Bank A / B | eMMC, 128 MiB each, raw | Signed firmware images, `"NPFW"` header | **No** — `SW02-M01`, bootloader |
| 2 | Safety MCU staging | eMMC, 4 MiB, raw | SW-01 image, delivered onward page-by-page over SPI | **No** |
| 3 | **Config / Calibration** | eMMC, **16 MiB, LittleFS, AES-256-XTS** | UHDR Argon2id salt · wrapped UKMD record · SHDR key under SNVS OTPMK · device serial · **eMMC session counter** · auth lockout state · bootloader version history · factory calibration · last PD profile · BT/Wi-Fi credentials · warranty token (`NP-FW-EMMC-002` §A) · `fleet_key` (`NP-MOD-ID-001` §4.3) · **the `"NPMP"` module-map blob** | **Yes — the module-map region only** |
| 4 | SHDR | eMMC, 512 MiB, LittleFS, AES-256-XTS | Fleet telemetry; NeurOne-owned | **No** — `np_log_backend`, `OI-LOG-01..07` |
| 5 | UHDR | eMMC, 6,903 MiB, LittleFS, AES-256-XTS under the UKMD | User biology; NeurOne holds no key | **No** — same |
| 6 | Scratch | eMMC, 500 MiB, raw, **zeroed every boot** | OTA staging, anonymisation workspace | **No** — by definition it persists nothing |
| 7 | SNVS `LPGPR0/1/2` | On-die registers | Boot bank, OTA attempts, factory-reset-in-progress, anonymisation-in-progress | **No** — and see §3.4, because their retention is weaker than two callers assume |
| 8 | eMMC OTP / fuses | OTPMK, HAB keys | Root of trust | **No** |
| 9 | **On-module EEPROM** (`U1`, 128 B) | Inside each tile's tinyAVR | **Map 4**, per `NP-MOD-ID-001` §5 | **No** — but §6 specifies how the hub reads and reconciles it |

> **The HAL owns exactly one region of exactly one partition, and it abstracts the *record*, not the
> medium.** Callers above it — `np_module_map_persist()` / `_restore()` — pass a byte range and a
> meaning. Everything below the HAL (LittleFS instance, XTS layer, eMMC block driver, USDHC2) is
> already owned by `SW02-M02` and is not re-specified here. **D-1.**

The distinction earns its place because "NVRAM" in `np_module_map.h` reads as a medium and is not
one. There is no NVRAM part in this design — no FRAM, no MRAM, no discrete NOR, no battery-backed
SRAM. There is an eMMC with a partition table, and one partition of it is where non-volatile
configuration lives. Every property the map depends on — wear levelling, power-loss behaviour,
encryption, erasure at factory reset — is a property of *that partition*, not of a chip called NVRAM,
and a future reader who believes otherwise will look for guarantees that no component provides.

---

## 3. The medium

### 3.1 Why Config, and not a new part

Four properties decide it, and the fourth is the one that would be expensive to rebuild elsewhere.

1. **It exists and is already mounted before any session may begin.** `EMMC-FS-04` makes a mount
   failure on any of the three LittleFS instances a FAULT that blocks session start. The module map
   is needed at exactly that moment — `np_module_map_restore()` runs at bring-up, before the first
   poll — so the store's availability window and the map's need are already aligned.
2. **Its erasure semantics are already correct for this data.** Factory reset step **R-7** zeroes the
   Config partition (`NP-FW-EMMC-002` §B.2), which destroys `fleet_key` and rotates every
   `module_ref` (`NP-MOD-ID-001` §4.3). A module history that survived a factory reset would defeat
   that rotation from inside the device. Placing Map 3 in Config makes the correct behaviour the
   default rather than a step somebody must remember to add.
3. **Its key custody is already the right one, and only just.** See §3.2 — this cuts both ways and is
   the sharpest constraint in this document.
4. **No new part means no BOM delta.** Every T1 configuration is gross-margin negative at the floor
   (`NP-COST-001` §4, CLAUDE.md §2), so a design that added a discrete NVM die would be arguing
   against a live cost decision with an alternative that is already sitting unused. §10.

**A discrete serial NOR or FRAM is rejected**, and not only on cost. It would need its own SPI or
QSPI chip select on a hub PCB whose interface contract is at 18 populated / 20 provisioned connector
positions and 216/240 pins (`NP-HW-HUB-001` §7.4); it would need its own power-loss story, its own
wear model, its own encryption (Config's XTS layer would not cover it), and its own entry in the FAI
and risk registers per `NP-ART-001` §1. It buys one property Config does not have — independence from
eMMC end-of-life — and `NP-FW-EMMC-001` §14 already handles that case by blocking session start at
`PRE_EOL_INFO = 0x03`. **D-2.**

### 3.2 The custody rule, which decides what may be stored here at all

> **The Config partition is encrypted under a key NeurOne can re-derive. Therefore nothing of UHDR
> class may be written to it, whatever the upload policy says.**

`EMMC-CFG-01` mounts Config under *"AES-256-XTS with device manufacturing key"*, and `EMMC-SHDR-05`
states plainly that *"NeurOne can re-derive the SHDR key for any device using MRK + device serial
number."* The UHDR partition is the opposite case by construction: its UKMD is wrapped under a
credential-derived WKMD and never leaves SRAM while unlocked (`np_uhdr_key.h`; `NP-FW-EMMC-002` §C).

This matters because CLAUDE.md §5's boundary is usually applied as an *upload* rule — what may be
transmitted to the fleet database. On a store whose key the manufacturer holds, that is the wrong
test. **Writing UHDR-class data into Config discloses it to NeurOne at the moment of the write**, with
no upload and no consent event anywhere in the path. The boundary here is therefore a **storage**
boundary, and it is stricter than the upload boundary that governs `np_log_backend`.

Consequence, stated as a rule the HAL enforces by refusing to offer the capability: **the module-map
region carries device-configuration and device-condition fields only. It has no writer for anything
whose classification is UHDR or contested.** §8 works through every field against that test. **D-3.**

### 3.3 Two defects on this partition, found while sizing the region

**(a) The raw journal area promised by `EMMC-CFG-02` has no address space.** The clause reads:

> *"Only the session counter, lockout state, and last-known PD profile may be written during normal
> operation. These fields occupy a dedicated Config partition journal area (raw-write, no LittleFS
> overhead for high-frequency updates)."*

`EMMC-FS-01` gives the Config LittleFS instance `block_size` 4,096 and `block_count` **4,096**. That
is 16,777,216 bytes — the whole of partition 4 (`NP_CONFIG_SIZE_LBA` = 16 MiB). A raw-write region
inside those blocks is not a region; it is filesystem corruption on the next compaction. Either the
LittleFS `block_count` must shrink to leave the journal outside it, or the journal must become
LittleFS files and `EMMC-CFG-02`'s "no LittleFS overhead" justification is void. **This is not
academic for this study** — §4.2 wants exactly the append-structured, high-frequency, raw-write
behaviour that clause describes, and it cannot be built until the clause is made true.
**`OI-NVRAM-01`, blocking.**

**(b) `NP-FW-EMMC-002` §C.3's *"Config partition offset 0x1000 — UKMD record"* is an address of the
same impossible kind.** A byte offset into a LittleFS-mounted partition names a block the filesystem
owns and may relocate. Whichever way (a) resolves, this citation resolves with it. Recorded rather
than fixed here, because the UKMD record is not this document's to move: **`OI-NVRAM-02`.**

**(c) `EMMC-CFG-02`'s whitelist is already violated, in tested code.** It permits three fields to be
written during normal operation. `np_module_map_persist()` writes a fourth — the whole `"NPMP"` blob —
whenever any socket's UID changes, which is every hot-plug and every tile swap. The blob is not in
`EMMC-CFG-01`'s contents list either. The right resolution is almost certainly to widen the
specification rather than to forbid the write, since the write is correct and the specification
predates the hex-tile architecture; but it must be a decision, not a silent divergence.
**`OI-NVRAM-03`.**

### 3.4 SNVS is not a candidate, and two existing callers assume otherwise

The i.MX RT1062's SNVS low-power general-purpose registers look like the natural home for a small
commit flag. They are not available for it, and the reason is the same one that removes wall time
(§5): **the headset has no battery, no coin cell and no `VBAT` rail** (CLAUDE.md §4; `NP-FW-EMMC-002`
§H.3.1). Without a supply on the SNVS low-power domain, `LPGPR0..2` are reset by a power removal, and
their retention is a **warm-reset** property only — which is exactly what
`firmware/bootloader/include/np_boot_selector.h` says: *"SNVS_LPGPR0 persists through warm resets."*

Two callers were written against the stronger property:

| Caller | What it assumes | Why that is not available |
|---|---|---|
| `np_config.h`, `NP_SNVS_RESET_IN_PROGRESS` | *"If set on boot, a reset was interrupted **by power loss**; re-run SANITIZE"* (`np_main.c` re-erases UHDR/SHDR/Config on it) | A power loss clears the flag, so the interrupted reset is **not** detected and the device boots with data half-erased — the outcome the comment says must never happen |
| `np_main.c`, `NP_SNVS_ANON_IN_PROGRESS` | Recovery from *"a research anonymization run interrupted by power loss"* (`NP-FW-EMMC-002` §D.6) | Same. Here the residual is benign — Scratch is zeroed on every boot anyway by `EMMC-SCR-01` — so the flag is redundant rather than wrong |

Neither is this document's to fix, and the first is a factory-reset correctness question rather than a
storage-layer one. Both are recorded: **`OI-NVRAM-05`**, and it is blocking for the factory-reset
verification in `NP-MOD-ID-001` §10, which asserts that reset rotates every ref. **For this HAL the
conclusion is simply that there is no register that survives a cable pull, so every durability
property must be carried in the eMMC record itself.** **D-4.**

### 3.5 Sizing, and the ceiling nobody has hit yet

Exact figures from `np_module_map.h`, recomputed rather than quoted:

```
NP_HEXMAP_REC_BYTES = UID_LEN(8) + present/health/count(3) + MAX_ELEMENTS(128) + CAL_BYTES(36)
                    = 175
blob(n) = HDR(8) + n * 175 + CRC(4)
blob(80)  = 14,012 bytes      ← the shipped lattice
blob(128) = 22,412 bytes      ← NP_HEXMAP_NVRAM_MAX_BYTES, the addressing ceiling
```

| Constraint | Value | Source | Headroom at 80 sockets |
|---|---|---|---|
| Partition size | 16 MiB | `NP_CONFIG_SIZE_LBA` | 0.08 % used |
| LittleFS `block_size` | 4,096 B | `EMMC-FS-01` | blob spans 4 blocks |
| LittleFS `prog_size` | 256 B | `EMMC-FS-01` | — |
| **LittleFS `file_max` (Config)** | **65,536 B** | `EMMC-FS-01` | **51,524 B remain** |
| eMMC endurance | 30,000 P/E, SLC | `EMMC-HW-01` | see below |

**`file_max` is the binding constraint, and it is the one the brief's architecture walks into.** If
Map 3 were appended into the same file as the inventory, the whole four-map record for up to 80
modules would have to fit in 51,524 bytes — **644 bytes per module** — a per-module history budget
that no document has set and that nothing warns about until a write fails at run time. This is a
second, independent reason for **D-5** (§4.2): Map 3 gets its own file, with its own explicit bound,
so the bound is a decision rather than a discovery.

**Endurance is not the constraint; write granularity is.** At 16 MiB and 30,000 P/E cycles the
partition's byte budget is 5.03 × 10¹¹, which is 3.59 × 10⁷ full-blob rewrites at 14,012 bytes. At a
clinic's 12 sessions a day and one rewrite per session that is ~8,200 years. But the same arithmetic
run against a per-event cadence inverts: journalling once a second through a 30-minute session is
1,800 rewrites, 25.2 MB per session, and 19,970 sessions — **about 4.6 years at clinic rate.** The
difference between a comfortable margin and a warranty problem is entirely the decision to rewrite
14 KB to record a 16-byte fact. Map 3's write granularity is therefore a first-class design
parameter, not an implementation detail. §4.2.

---

## 4. Power-loss atomicity

### 4.1 What is actually racing what

The brief's premise is that a module change cuts power, so writes race a power cut by design. §Read
First item 2 corrects the mechanism: `SAFE_EN[n]` removes a cluster's 24 V emitter rail
(`NP-DRV-SHELL-002` §6, *"LOW removes the rail"*), the hub keeps running on USB-C, and the only thing
that loses supply on extraction is **the tile itself**, when its `VCC_3V3` contact breaks.

So there are two atomicity problems, with different owners and different answers:

| Write | Loses power when | Owner | Answer |
|---|---|---|---|
| **Map 4**, on-module EEPROM | The tile's contact breaks — every removal, by design | `U1` module firmware | **Already solved.** `NP-MOD-ID-001` §5.3: four 32-byte slots written round-robin, newest valid CRC wins, and §5.3 states explicitly that *"the four-slot rotation is not an endurance measure — endurance is a non-issue — it is a crash-safety measure"* |
| **Map 3 + inventory**, hub Config partition | USB-C disconnect; power-bank depletion in Mode 3; brownout; watchdog reset | This HAL | §4.2 |

**The hub-side case is the harder one precisely because it is uncorrelated with anything.** A
module-change power cut at least has a cause the firmware can see coming. A user unplugging a cable
mid-session, or a power bank reaching its cut-off in Mode 3 (CLAUDE.md §4 gives ~38–42 min at T1 peak
on 10,000 mAh), arrives with no notice at an arbitrary instruction. Every write must therefore be
crash-consistent at every instant, not merely flushed at tidy moments.

### 4.2 The commit discipline

Two records, two disciplines, because they have opposite failure economics.

**(a) The inventory blob — whole-file replace, never truncate-in-place.**

The existing code is already correct above the HAL: `np_module_map_load()` validates magic, version,
socket count and CRC-32 before touching a record, and `np_module_map_restore()` is documented as
fail-closed on every path, leaving the inventory **empty** rather than partial. What is undefined is
the HAL beneath it, and there is exactly one way to get it wrong:

> **`np_hexmap_nvram_write()` must not destroy the live record before the replacement is durable.**
> Implemented as `open(..., O_TRUNC)` → write → sync, a power loss anywhere in the middle leaves a
> zero-length or short file, which fails the length check and discards a good inventory for no
> reason. Implemented as write-new → sync → atomically publish, LittleFS's copy-on-write metadata
> commit gives A/B semantics for free: the reader sees either the whole old record or the whole new
> one.

No explicit A/B slot pair is needed for the blob, because the filesystem already provides one and
duplicating it would double the `file_max` pressure of §3.5 for no gain. What *is* needed is that the
HAL's contract says so, since the property is invisible in the call signature. **D-6.**

Torn write degrades to: **CRC or length mismatch → `NP_HUB_ERR_BAD_MAGIC`/`_CRC` → inventory empty →
the hub re-polls every socket.** That is the same recovery as a first boot, it is already tested, and
it is correct **because the blob is a cache** (§7.2).

**(b) Map 3 — append-only, self-checking, fixed-size records.**

An accumulating record cannot use (a)'s discipline: rewriting 14 KB to append 16 bytes is the write
amplification §3.5 costs at 4.6 years, and worse, it puts the *entire* history at risk on every
single append. The discipline is instead:

1. **Fixed-size records, `prog_size`-aligned in batches.** `EMMC-FS-01` gives Config `prog_size` 256;
   a record that does not straddle a program unit cannot be half-programmed.
2. **A CRC over `uid ‖ payload` in every record**, adopting `NP-MOD-ID-001` §5.2's idiom wholesale —
   binding the CRC to the UID means a record recovered into the wrong module's history fails
   validation rather than silently transferring one part's usage to another.
3. **A monotonic `seq` per record** (§5), so recovery is "scan forward to the last record that
   validates" and requires no state outside the file.
4. **Two watermarks, not one:** `synced_upto` (the highest ordinal the control software has
   acknowledged — §6) and `retired_upto` (the highest ordinal whose storage has been reclaimed).
   Retirement lags acknowledgement, and reclamation happens at block granularity.

**A torn append degrades to: the torn record fails its CRC, is discarded, and every record before it
survives.** Nothing before the tear is at risk, which is the property (a) cannot offer and is the
whole reason for the split. **D-5.**

**Fail-closed means something different for a record than for a cache, and the difference must be
stated.** For the inventory, fail-closed is "assume nothing is present and re-poll" — availability is
cheap because the truth is one I2C transaction away. For Map 3 there is no re-poll: the sessions
happened and nothing else remembers them. Fail-closed for the journal is therefore **"declare the
gap, never fabricate a zero"**: if the journal is unreadable, the helmet reports *unknown since
ordinal S* to the control software and Map 2 must not treat that as *nothing happened since
ordinal S*. A silent zero would let a corrupted journal drive Map 2's totals **downward**, past
Map 4's odometer, and §6.4's reconciliation rule is what catches it if this one is forgotten.
**D-7.**

### 4.3 No last-gasp write, and why the obvious warning signal cannot supply one

`SEAT#` is socket pin 19, tied to `PGND` through 1 kΩ, positioned at the mechanical extreme so it is
the last contact to mate and, by the reverse of `NP-HW-HEXTILE-001` §7.3's break order, **the first to
break**. That looks like an interrupt that arrives a few milliseconds before `VCC_3V3` does, which is
exactly the shape a flush-on-eject design wants.

It cannot be used, for three independent reasons, any one of which is sufficient:

- **It has no firmware consumer.** `grep -rn "SEAT" firmware/` returns nothing — no pin allocation, no
  handler, no test. The signal exists in three hardware documents and in no code.
- **It is not on the Class C side.** `NP-HW-HUB-001` places `ALERT#` and `SEAT#` aggregation at the
  **cluster controller**, so the earliest tier that can observe it is Class B, arriving over the
  cluster bus.
- **The window's duration is not derivable.** It is set by the pad-length stagger between group 3 and
  `SEAT#` divided by the extraction velocity of a human hand. `NP-HW-HEXTILE-001` §7.3 specifies the
  stagger's **order** and not one dimension of it; no document states an extraction-speed assumption.
  A design that commits inside an undimensioned window is a design with an invented constant in it.

> **The HAL therefore performs no write in response to any impending-removal signal, and its
> correctness never depends on one.** Every record is already durable, or it is not yet claimed to
> exist. **D-8.** `OI-NVRAM-04` records the missing dimension, because if a future revision wants a
> pre-break notification for any purpose, that is the number it will need.

A related consequence for Map 4, which *does* lose power on extraction: the same absence means the
module firmware cannot schedule its EEPROM write for the moment of removal either. `NP-MOD-ID-001`
§5.3's cadence — **one update per session end** — is therefore not merely convenient, it is the only
cadence available, and the cost is bounded and should be written down: **a session interrupted by a
tile being pulled mid-run contributes nothing to that tile's odometer.** `OI-NVRAM-06`.

---

## 5. There is no clock, and what that costs "since the last sync"

### 5.1 The constraint, restated from the in-force source

The headset has no battery, no coin cell and no `VBAT` rail; it is USB-C powered and Mode 3 runs off a
power bank (CLAUDE.md §4). The SNVS RTC therefore has no backup domain, **wall time is lost on every
disconnect and re-supplied by the phone** — `np_edf_write_header(..., time_t session_ts)` takes it as
a parameter. `NP-FW-EMMC-002` §H.3.1 states this and draws the conclusion that a calendar expiry
would be *"both losable and settable backwards, which is the opposite of fail-closed"*; §H.3.2
replaces it with a record budget, and §G.2 records the same correction applied to a rolling window
that had been written in days.

### 5.2 The ordinal already exists — do not invent a second one

`NP-FW-EMMC-001` **EMMC-CFG-01** puts the *"eMMC session counter (unsigned 64-bit integer, incremented
at each session start)"* in the Config partition and calls it *"the only time-like SHDR reference"*;
**EMMC-SHDR-09** makes it the sole timestamp form in SHDR, citing CLAUDE.md §5's boundary rule
directly.

> **Every ordering, window and watermark in this HAL is denominated in that session counter, plus a
> per-record `seq` for intra-session ordering. No new counter is introduced and no wall-clock field
> exists in any record this HAL writes.** **D-9.**

This is not merely available, it is the correct choice on its own merits: a second monotonic counter
would have to be kept consistent with the first across factory reset, and two counters that can
disagree are a worse foundation than one that cannot.

### 5.3 What it costs, said plainly

"Everything collected since the last sync" sounds temporal and cannot be. Four consequences, none of
which are defects but all of which will surprise someone:

1. **A Map 3 window has no duration.** It is a half-open interval of session ordinals `(S₀, S₁]`. Two
   helmets whose journals both cover "50 sessions since last sync" may span a week and a year.
2. **No rate is computable on the device.** Sessions per day, hours per week, mean time between
   insertions — none of these can be formed on-helmet, ever. Anything expressed as a rate must be
   computed by the control software, from its own clock, and must stay there (§8.3 explains why that
   is a privacy property and not only an inconvenience).
3. **"Last sync" is not a moment the helmet can name.** It is an ordinal it was told. The helmet
   cannot distinguish "you synced yesterday" from "you synced eleven months ago", and must not behave
   as if it could — no staleness warning, no "it has been a while" prompt, no expiry.
4. **A sync that happens with no sessions in between is a no-op, correctly.** The window is empty
   because the ordinal did not move, which is the right answer and needs no special case.

The one place where this is a real loss rather than a rephrasing is **predictive maintenance**:
CLAUDE.md §5's Phase 2 LSTM is specified over *trajectories*, and a trajectory indexed by an ordinal
whose spacing in time is unknown and non-uniform is a weaker input than one indexed by a clock. That
is a known, recorded gap for the whole programme (`OI-EMMC2-13`), not something this document
introduces, and the honest position is that Map 3 inherits it rather than solves it. `OI-NVRAM-07`.

---

## 6. Sync: what it means, who starts it, and what happens when the maps disagree

### 6.1 The definition

> **A sync is a transfer of Map 3 records above a watermark the control software supplies, followed by
> an acknowledgement by ordinal, after which the helmet may retire the acknowledged rows.**

Four properties, each with its reason.

1. **The control software initiates. Always.** The helmet must not push, because it cannot know
   whether the software it is connected to is the one holding Map 2 for these modules — a clinic
   tablet, a second phone, and a service laptop are all plausible peers, and only the peer knows what
   it already has. **D-10.**
2. **The watermark is per-uid, not per-device.** Modules move between helmets (`NP-MOD-ID-001` §6.1);
   a device-scoped watermark would either re-send another helmet's records or skip this helmet's.
3. **Acknowledgement precedes retirement, and retirement is a watermark advance, not an erase.** Rows
   below `retired_upto` become reclaimable at block granularity; nothing is destroyed to make room for
   something that has not been received.
4. **A sync is not a consent event and grants no upload.** What the control software does with Map 3
   afterwards is governed by §8, and the answer for most of it is "nothing leaves the app".

### 6.2 What happens to Map 3 rows after a successful sync

They stop being the authoritative copy and become reclaimable. They are **not** required to disappear
immediately, and there is a reason not to rush: a control software instance that loses its own store
(app reinstall, phone replacement) can re-request from a lower watermark for as long as the rows are
still there. The helmet reclaims lazily, when space is needed, oldest-acknowledged first. **D-11.**

### 6.3 The journal-full policy — the decision that is easy to get wrong

A bounded journal that fills while unsynced has three candidate behaviours, and two of them lose data
invisibly:

| Option | Behaviour | Disposition |
|---|---|---|
| Wrap — overwrite oldest unsynced | Journal never fills; oldest history vanishes | **Rejected.** The loss is silent, and it is exactly the history of the device that syncs least, which is the one whose history is most needed |
| Stop — refuse further sessions | History is perfect; the device becomes unusable because a phone was not connected | **Rejected.** This makes a data-collection convenience into an availability failure, which `NP-MOD-ID-001` §7.5.2's non-coercion invariant forbids in spirit and CLAUDE.md §1's *"no mandatory subscription — all core functions offline-capable permanently"* forbids in letter |
| **Degrade — keep totals, drop detail, set a flag** | Per-uid running totals continue to accumulate exactly; per-record detail stops; a `detail_lost` flag is set and reported at the next sync | **ADOPTED — D-12** |

The adopted option preserves the property that actually matters. **Map 4 is an odometer, and an
odometer's totals must never go backwards or lose count**; the detail in Map 3 is what enables
attribution and trend fitting, and it is genuinely losable. Degrading from a record to a counter keeps
the invariant and discards the enrichment, which is the correct order of sacrifice. The flag is what
stops the loss being invisible — the same reasoning that makes `NP_CAL_DEFAULT` a reported value
rather than a silent fallback (`NP-FW-PBM1064-001` Rev 4).

### 6.4 A module the control software has never met

No special case is needed, and inventing one would be a mistake. `NP-MOD-ID-001` **MODID-5** already
settles it: *"there is no transfer protocol, because none is needed"* — the receiving device reads the
odometer from the module it is physically holding, and learns total sessions, total mate cycles and a
degradation summary with **no linkage record created anywhere**. So:

1. The socket reports a UID that is not in Map 3 → the hub reads Map 4 from the module.
2. Map 4's totals seed the helmet's local baseline for that uid.
3. At the next sync, Map 2 learns of a module it has never seen, with a **carried-in baseline** —
   and per **MODID-6** that baseline is **coarsened before it reaches SHDR**, because an exact
   carried-in count is joinable against the previous fleet's last reported partial.

**Corollary, worth stating because it inverts an intuition: Map 2 is not authoritative for a module's
existence. Only the socket is.** A uid in Map 2 that no socket reports is a module that is elsewhere
or gone; it is not an error, and the helmet must not attempt to reconcile it. **D-13.**

### 6.5 Reconciliation when Map 2 and Map 4 disagree about the same uid

Both hold monotone counters for the same part, from different vantage points: Map 4 is what the part
says about itself, Map 2 is what this fleet observed. They disagree routinely and benignly — Map 2 has
not seen the sessions the part ran on someone else's helmet.

| Case | Meaning | Rule |
|---|---|---|
| Map 4 > Map 2 | The normal carried-in case (§6.4) | **Map 4 seeds Map 2.** Reconcile by `max`, never by sum — summing double-counts every session both already knew about |
| Map 4 = Map 2 | Steady state | No action |
| **Map 4 < Map 2** | The part reports *less* than this fleet already recorded | **Anomaly. Keep Map 2's value; never write the lower value back to the module; flag it.** Three causes are plausible — a replaced or reflashed `U1`, a slot recovered to an older generation after a power-loss during write, or a record transplanted between modules (which `NP-MOD-ID-001` §5.2's CRC-over-UID should already have caught). All three are device-condition events worth knowing about and none of them licenses a rollback |

> **The magnitude of a divergence must never be uploaded, only its existence.** `NP-MOD-ID-001` §6.3
> establishes that an exact counter value is a join key across fleets — *"if fleet A's last reported
> partial is 4,317 sessions and a new ref in fleet B appears with `carried_in_session_count = 4,317`,
> those two rows can be joined on the value itself."* A divergence of 4,317 is exactly as joinable as
> a baseline of 4,317, and the mitigation transfers unchanged: coarsen, or upload a boolean.
> **D-14.** This is `OI-MODID-02`'s bucket-width question acquiring a second consumer, and the widths
> must be chosen once for both.

---

## 7. Versioning, and the module the hub cannot classify

### 7.1 Map 1 is a legitimate generated artefact — and its permission is narrower than it looks

`NP-NPPS-REF-001` §1.6 (Rev 14) bans build-time caches of protocol content in terms that would
otherwise catch Map 1, and then carves out precisely this case:

> *"**A generated artefact may still hold hardware facts.** `SocketLattice.generated.{swift,kt}` and
> `socketMap.generated.ts` carry socket count, numbering and lattice geometry. Those are not protocol
> content: they do not come from `.npps` and change only on an inner-bowl re-tool, which is a rebuild
> in any case."*

Map 1 satisfies the **origin** half of that permission cleanly: `(major, minor)` → element types and
ranges is derived from tile specifications, not from any `.npps` file, and no zone, protocol,
condition or composite is involved. A reader who mistakes Map 1 for the thing §1.6 bans should be
pointed at that sentence.

**But its stated *reason* does not transfer, and the difference is load-bearing.** The lattice
artefacts are safe because they *"change only on an inner-bowl re-tool, which is a rebuild in any
case"* — the physical fact and the software artefact are updated by the same act. **A new tile minor
is not like that.** A tile variant can be manufactured, sold and inserted into a helmet whose control
software was built before that variant existed, with no rebuild anywhere in the causal chain. Map 1
is therefore the one generated artefact in this system that **can** go stale against physical reality
while everything behaves normally — which is the failure mode §1.6 exists to prevent, arriving through
a door §1.6 does not watch.

That is not an argument against generating Map 1. It is the argument for §7.3: **the update check is
not a nicety, it is the mechanism that makes a build-time hardware map safe**, and it is mandatory for
that reason rather than for convenience. **D-15.**

### 7.2 The blob version bump, and when the free window actually closes

`np_module_map.c` records the policy already, and it is right: version history 0x0001 → 0x0002 →
0x0003, *"there is deliberately NO migration path"*, load rejects any differing version, restore
clears and propagates, and the hub rebuilds by polling. The 0x0003 bump was not defensive — records
grew 36 bytes, and a v2 blob read as v3 *"hands each socket the neighbouring socket's UID — on a map
that gates PBM and tES element addressing"*.

**0x0003 → 0x0004** adds `(major, minor)` to each socket record. Two bytes per record if the fields are
`uint8_t` each; `NP_HEXMAP_REC_BYTES` 175 → 177 and blob(80) 14,012 → 14,172. The bump destroys
nothing, because no fielded hardware holds a v3 blob and the recovery is a re-poll that is already
the first-boot path. All of that is exactly as the brief states. **D-16.**

**What the brief gets wrong is when the window shuts.** It is not first customer shipment. It is
**the first moment an unsynced Map 3 row exists anywhere**, which is the first powered prototype in a
lab or a clinic. And the reason is a distinction the current code does not have to make and the
four-map architecture forces:

| | Inventory blob (Maps 1–2 projection) | Map 3 journal |
|---|---|---|
| What it is | A **cache** of what the sockets will say | A **record** of what happened |
| Rebuildable? | Yes — poll every socket, always correct | **No.** Nothing else remembers |
| Cost of discard | One re-poll at boot | Irrecoverable history loss |
| Correct policy | Reject-and-rebuild | **Migrate, or refuse to touch** |

> **Therefore Map 3 does not share the `"NPMP"` blob, does not share its version, and does not inherit
> its discard-and-rebuild policy.** It is a separate file with its own magic and its own version, and
> its version rule is the opposite one. **D-5**, arrived at here from a second direction and by
> `file_max` in §3.5 from a third.

**The policy after the window closes, stated now so it is not improvised later:**

1. **The inventory blob keeps reject-and-rebuild forever.** It is a cache; the policy is correct at
   every point in the product's life and costs one re-poll.
2. **The Map 3 journal never gets a reject-and-rebuild path at all.** From its first line of code its
   record header is designed to be extended at the tail only, its reader accepts a **version range**
   and ignores trailing fields it does not know, and any change that is not tail-additive requires a
   converter that runs before the new firmware's first write. A firmware update that discards
   unsynced history is a defect, not a policy.
3. **Which implies a rule for the field the bump introduces:** if `(major, minor)` is ever needed
   inside a Map 3 record it must be written as a value, not as an index into a version-specific
   layout.

`OI-NVRAM-08` records that (2) needs a CI check — a test that loads a v_n journal under v_n+1 firmware
and asserts no record is lost — and per `NP-CONV-001` §8 that check must be **falsified in both
directions** before it is trusted.

### 7.3 The unknown pair: it is a limits problem, not an identity problem

A module reports a `(major, minor)` the hub cannot find in Map 1. The brief's flow is right — check for
a software update; if current, report a maintenance error; never silently ignore, never refuse to
boot — and the flow becomes much sharper once the actual deficit is named.

**The hub does not need Map 1 to know what elements a tile has.** The tile self-reports its element
inventory over I2C (`np_hexmap_inventory_fn`, `OI-HEXMAP-02`), and `np_module_map` stores the resulting
`np_elem_type_t` list per socket. What Map 1 supplies that the tile does not is the **`[min : incr :
max]` range per element type** — the safe operating envelope for driving it.

> **So an unknown `(major, minor)` means: the elements are known and their limits are not. The hub
> must not drive an element whose limits it does not hold.** **D-17.**

That single sentence decides the whole behaviour, and it decides it more narrowly than "disable the
module":

| Situation | Behaviour |
|---|---|
| Unknown pair, update available | Prompt for the software update. The module contributes no drivable elements until it is applied. Every other socket is unaffected |
| Unknown pair, software current | **Maintenance error, reported** — `np_module_map` records the module as present-and-unclassified, which is a state the wire format already has (`NP_ZN_MODULE_UNKNOWN`, *"a seated module we simply can't name"*, distinct from a fault) |
| A protocol names that socket | `np_module_map_check_placement()` fails on it and the session is refused, exactly as it does today for an absent module (`NP-HEX-ZM-001` §4a gate SW-1) |
| A protocol does not name that socket | **The session runs.** An unclassifiable module removes only itself from the addressable set |
| Any case | **The hub never substitutes the nearest known minor.** See below |

**The substitution ban is the important half.** The tempting behaviour, when minor 7 is unknown and
minors 1–6 are known, is to use minor 6's ranges — they are probably close, and the module probably
works. That reasoning is wrong in the same way and for the same reason as the two errors this
codebase has already made and corrected: `np_hex_addr_pack()` refuses to mask an out-of-range socket
id into a valid one because *"this module addresses PBM and tES elements, so that is a wrong-site
stimulation path"*, and UID-keyed calibration refuses to fall back to another module's coefficients
because that applies the previous occupant's calibration *"invisibly, because `cal_source` still
reads `NP_CAL_FACTORY`"*. A substituted limit is the same shape: a plausible number, silently wrong,
on a drive path. **A minor exists precisely to track emitter specifics** — it is the field that changes
when the thing being bounded changes.

**And a note on what "unknown" costs the user standing there.** The failure is annunciated as a
maintenance condition and it is recoverable by an action the user can take (update the software) or by
one they can understand (this tile needs service). It is not a boot refusal and it is not a session
refusal unless the protocol actually needs that socket. That is the same graceful-degradation posture
`np_module_map` takes throughout: fail closed on the element, not on the device.

---

## 8. UHDR / SHDR classification of every field

CLAUDE.md §5 is the governing rule, its boundary-case list is precedent, and §3.2 above adds the
custody constraint that makes this a storage question and not only an upload question.

### 8.1 The fields

| Field | Where | Class | Reasoning |
|---|---|---|---|
| `module_uid` (8 B) | Map 3 store, hub RAM | **SHDR class, never uploaded raw** | A component identifier, *"analogous to accessory authentication"* (`np_module_map.h`). `NP-MOD-ID-001` MODID-1 requires the raw UID never leave the device; what leaves is `HMAC-SHA256(fleet_key, uid)` |
| `major`, `minor` | Inventory blob | **SHDR** | Device configuration. It is what the device is, not who used it |
| `health` (opaque byte) | Inventory blob | **SHDR** | Device condition, by definition |
| `cal[9]` factory coefficients | Inventory blob | **SHDR** | Measured on the part at manufacture (`NP-FW-PBM1064-001` §6.6); a factory fact |
| Element type list | Inventory blob | **SHDR** | Configuration |
| `session_count` per module | Map 3 / Map 4 | **SHDR — as a bare unsigned count only** | Directly on precedent: *"device session count (unsigned integer) → SHDR; session timestamps → UHDR."* The qualifier is the whole rule (§8.2) |
| `mate_cycles_observed` | Map 4, mirrored in Map 3 | **SHDR as a total; per-event record is not written at all** | An aggregate insertion count is wear data on a contact rated ≥500 cycles. A per-event *record* is a log of when the wearer reconfigured their helmet — which areas they changed treating — and it is not needed for anything (§8.4) |
| `emitter_on_seconds`, `thermal_seconds_over_threshold`, `throttle_events`, `peak_ntc` | Map 4 | **SHDR only under `NP-MOD-ID-001` §7's opt-in**, per-socket, cohort-scoped | Already decided there and not re-opened. Per-socket optical duty at 80-socket resolution is treatment geography; `DOSE-01` is narrowed, not removed |
| `pd_ratio_last` | Map 4 | **SHDR** | The fouling-vs-aging discriminator; a property of the window and the emitter |
| `seq`, `synced_upto`, `retired_upto` | Map 3 store | **Device-internal. Never uploaded in any form** | §8.2 |
| `detail_lost` flag (§6.3) | Map 3 store | **SHDR** | A storage-condition boolean with no magnitude |
| Map 2 → Map 4 divergence | Reconciliation (§6.5) | **SHDR as a boolean only** | Magnitude is a join key, `NP-MOD-ID-001` §6.3 |
| Anything else | — | **Not stored here at all** | §3.2's custody rule: Config's key is NeurOne-derivable |

### 8.2 The finding this section exists for: a delta is a timestamped count once anyone can date its ends

Map 3 is, by construction, *"all info collected on that module **since the last sync**"*. That is a
**delta over an interval**. The helmet cannot date the interval's ends — §5 — and that is what makes
the on-device record benign.

**The control software can.** The app runs on a phone with a clock; it is the party that initiates
every sync (§6.1); and it therefore knows, to the second, when each watermark was set. A stream of
`(uid, sessions_in_window, window_end_ordinal)` rows, uploaded from a party that timestamped its own
sync events, reconstructs **sessions per module per calendar interval** — that is per-socket usage over
time, which is treatment geography over time. It arrives with no clock field anywhere in it.

This matters more than it might appear, because `NP-MOD-ID-001` §7.5.1.1 makes the absence of a
sub-month clock in SHDR **the load-bearing foundation of the entire registrant-scoped consent model**,
enforced by CI check `TIME-01`, and states in terms that *"anyone removing or narrowing those checks
must understand they are not tightening data hygiene — they are removing the foundation of this
consent model."* A sync-boundary field would not remove `TIME-01`; it would walk around it. `TIME-01`
inspects the schema for time-typed columns, and an ordinal is an integer.

> **Rule: no field that identifies a sync boundary, and no value expressed as a per-window delta, may
> be uploaded. What reaches SHDR is the running total, aggregated across all windows. D-18.**

The delta is the transport, not the datum. Uploading `total = 4,712` discloses a count. Uploading
`Δ = 31 in window ending at ordinal 4,712` discloses a count **and** an interval, and the interval is
datable by the only party that ever holds both halves.

This is the 2026-08-12 conditional-redaction lesson (CLAUDE.md §5) in a different key. There, a
redaction applied on a sensitive predicate leaked the predicate, because the *pattern* of protection
was itself information. Here, a value that is individually innocuous acquires a clock from a fact
about **who is holding it and when**, so the field's classification cannot be read off the field. Both
say the same thing: classify the observable, not the column.

### 8.3 Map 2 is app-side, and app-side is the UHDR side of the fence

Map 2 is *"stored with the control software"* — on the phone or tablet, beside `ConsentStore`, beside
the user's identity, beside a clock. There is precedent for classifying data that lives there:
CLAUDE.md §5 routes the anonymisation pipeline's `failed_step` to *"UHDR/app-side only"*.

Map 2 holds no user biology, so it is not UHDR in the ordinary sense. But it sits in the one location
where module usage counters and a wall clock are held by the same process, which is what §8.2 shows
is sufficient to make a usage timeline. **Map 2 therefore inherits app-side UHDR-class handling: it
is the user's, it is not a source for any SHDR upload path, and no code may read Map 2 and write
SHDR.** That is the same code-structural independence CLAUDE.md §6 already requires between
`SHDRUploader` and `ConsentStore`, applied to a new pair. **D-19.** `OI-NVRAM-09` — this needs a
check, and the existing `warranty-nojoin-ci.yml` is the pattern to follow.

### 8.4 Where the brief's suspicion lands

The brief asks whether module usage counts and insertion counts are a proxy for when and how often a
person used the device. Worked through, the answer has three parts and only one of them is the
expected one:

1. **A bare total is not.** It is a count with no denominator and no anchor — precisely the form
   CLAUDE.md §5 already admits to SHDR for device session count, and `NP-MOD-ID-001` §7.5.1's
   analysis of why (no wearer identifier exists in any store) holds unchanged here.
2. **A per-window delta is**, for the reason in §8.2, and this is the real hazard. It is also the
   form Map 3 naturally produces, which is why the rule has to be written down rather than left to
   good sense.
3. **A per-event insertion record would be**, more sharply than a session count: swapping tiles
   changes *which regions are treated*, so a timeline of insertion events is a timeline of changes in
   treatment target. The design answer is not to classify it carefully — it is to **not write it**.
   Map 4 holds `mate_cycles_observed` as a total (`NP-MOD-ID-001` §5.2) and nothing in the maintenance
   question needs the events. **D-20.**

This is the `OI-EMMC2-11` lesson applied where it fits: an append-per-event table on a non-unique key
reconstructs by aggregation what its column list forbids, and *"a column-level control cannot express
a row-set-level hazard"*. Where the remedy of collapsing to one upserted row per device is available —
and for mate cycles it is — take it.

---

## 9. IEC 62304 classification

> **SW-02, Class B, for the whole of this HAL and every record it writes. Not Class C.**

The argument is architectural, and the first leg is close to dispositive.

**(i) The Class C processor has no electrical path to the store.** The eMMC hangs off USDHC2 on the
i.MX RT1062 (`NP_USDHC2_BASE`, `np_config.h`). The STM32G071 has no eMMC controller, no connection to
that bus, and no code that could use one: `NP-FW-EMMC-001` §16's processor-ownership table assigns the
eMMC block driver, all three LittleFS instances, the AES-256-XTS layer and every telemetry writer to
the RT1062 at Class B. **That table has exactly one Class C entry, and it is the apparent
counterexample worth naming: *"Safety MCU firmware update via SPI"*, at "Class B (main) / Class C
(Safety MCU self-program)".** The Class C half is the G071 programming *its own flash* from an image
the hub hands it page-by-page over SPI with readback — the safety MCU's own firmware reaches it
without the MCU ever addressing the eMMC. The one Class C row on the storage table is therefore not
storage access; it is the absence of it, made explicit.
`np_module_map.c` states the corollary directly: its ~34.8 KB of `.bss` is fine on the RT1062 and would
exhaust the G071's 36 KB, which *"is a separate bare-metal project that links neither this translation
unit nor any hub_control include path."* Making this store Class C would mean routing eMMC into the
Class C boundary — a cost that has to be earned by a hazard.

**(ii) No stored value can widen a Class C permission.** SW-01 computes `granted_mask` as
`requested_mask & NP_SAFETY_EN_ALL_MASK` when `active_faults == 0`, and `0` otherwise. It consumes no
storage-derived quantity. Every ceiling that bounds a hazard is enforced independently of anything in
this store:

| Barrier | Tier | Independent of NVRAM because |
|---|---|---|
| 62 °C junction thermal cutoff | **C** | Reads NTC ADC; indifferent to what any map says a tile is |
| 40 µC/cm² charge density | **C** | Integrates commanded current per electrode pair |
| tES contact confirmation | **C** | Measures impedance at enable time |
| IEC 62471 MPE ceiling | **C** | Hardware current limit |
| Session-descriptor Ed25519 signature | **C** | Verified on the descriptor, not on stored state |
| 200 ms heartbeat / 1.5 s watchdog | **C** | Timing only |

**(iii) The map's own failure direction is toward less capability, never more.** Every documented
failure path — bad magic, wrong version, geometry mismatch, CRC failure, HAL error — leaves the
inventory **empty**, and an empty inventory drives nothing. `np_hex_addr_pack()` returns an
out-of-band sentinel rather than masking, and `np_module_map_resolve()` rejects it. A `memset` query
falls to the default arm and is rejected. There is no reachable state in which a corrupt store causes
more emission than a healthy one.

**Residual, stated rather than buried.** Legs (ii) and (iii) hold for *stimulation enable*. They do not
automatically hold for **per-tile drive magnitude**. If a future tile variant's maximum emitter
current is bounded only by Map 1's `[min : incr : max]` — with no independent electrical limit on the
cluster's high-side switch or in the on-module driver — then a wrong or substituted range becomes an
over-drive path whose only backstop is the 62 °C thermal cutoff, i.e. a *thermal* limit standing in
for an *optical* one. That is survivable but it is not the same argument, and it is why §7.3's
substitution ban is stated as an absolute. **`OI-NVRAM-10`** asks the direct question: is there a
Class C bound on per-tile emitter current that is independent of Map 1? If the answer is no, this
classification must be re-derived before any tile variant ships whose ranges differ.

**No SW-01 source changes and no bit added to any Class C wire format.** The store is invisible to
SW-01, and it must stay that way for the same reason `NP-SW-001` §3.2's Class B justification for
SW-02 rests on SW-01 being an independent backstop.

---

## 10. Cost

### 10.1 New parts: none, and the brief's premise is the thing to correct

| Map | Substrate | New part? | BOM delta / tile | At 20 tiles | At 30 tiles |
|---|---|---|---|---|---|
| Map 1 | Generated artefact shipped with the control software | No | **$0.00** | $0.00 | $0.00 |
| Map 2 | Control software's own storage | No | **$0.00** | $0.00 | $0.00 |
| Map 3 | eMMC Config partition, already in the BOM | No | **$0.00** | $0.00 | $0.00 |
| **Map 4** | **128 B EEPROM inside `U1`, the tinyAVR 2-series already at $0.50/tile in `NP-HW-HEXTILE-001` §6.4** | **No** | **$0.00** | **$0.00** | **$0.00** |

`NP-MOD-ID-001` §5.1 says it in one line and it is worth repeating because the brief assumed
otherwise: *"This is not a new component, a new pin, or a BOM change — it is unused capability in a
part already specified."* `NP-HW-HEXTILE-001` **D-3** fits `U1` to **every** tile type, which is also
why `NP-DRV-SHELL-002` §5.1.4 could delete the separate 24AA02UID line.

**The counterfactual, priced from the record rather than invented.** That deleted line is the
repository's own quote for a discrete per-tile serial NVM: *"−$8.50"* across *71* tiles, i.e.
**~$0.12/tile**. Had `U1` not been universal, Map 4 would have cost **$2.40 at 20 tiles and $3.60 at
30**. Against `NP-COST-001` §6's Home Standard rows — BOM $959 at 30 tiles, $844 at 20 — that is
0.3–0.4 % of BOM: real money on a gross-margin-negative configuration, and avoided entirely.

### 10.2 The cost of not doing it

This is the larger number, and it is not a storage number.

`OI-HEXMAP-01` is open. `np_hexmap_nvram_read/write` are unimplemented externs. Until they exist,
`np_module_map_persist()` and `_restore()` cannot function, with three consequences:

1. **Every boot re-inventories every socket**, defeating the design property `apply_poll()` was built
   around — *"Unchanged modules are never re-inventoried."* At ~80 sockets over the module I2C link
   this is bring-up latency the user waits through, on every power-on, forever.
2. **UID-keyed dose-metering calibration is lost on every power cycle.** The nine coefficients per
   module live in the blob (`OI-HUB-C06`, `NP-FW-PBM1064-001` Rev 4). With no persistence,
   `np_module_map_get_cal()` returns `NOT_PRESENT` on every boot and every socket falls back to
   firmware defaults reporting `NP_CAL_DEFAULT` — degraded, correctly labelled, and permanent.
3. **Which renders inert the most expensive line in the tile BOM.** The dual-photodiode metering that
   the fallback degrades is **$11.53/tile of driver + metering, ~$10 of it two InGaAs photodiodes** —
   **$346 per headset at 30 tiles** — and `NP-HW-HEXTILE-001` §6.4's Rev 7 note argues it is not
   merely a feature but *"the structural answer to the failure mode this product category is known
   for"*, against an independently measured **−79 %** declared-vs-measured gap on a marketed 1070 nm
   helmet. `NP-COST-001` §6 declines option 3 for exactly that reason.

> **So the cost of not building a ~14 KB persistence layer is that a $346-per-headset measurement
> capability runs at firmware defaults, and the product's primary technical differentiator over
> Vielight is unavailable at every boot.** That is the number this study is worth, and it is
> arithmetic on figures already in the record.

### 10.3 Engineering effort — and a correction about its unit

**No design study in this repository quotes engineering effort in any unit.** `NP-COST-001` is BOM,
COGS and GM% only; `NP-HW-HEXTILE-001` §6.4 and `NP-DRV-SHELL-002` §10.1 are per-part BOM; the only
effort-like figures anywhere are the *external engagement* costs in `docs/status/pending-decisions.md`
§13.1, in the shape "$8,000–15,000 · 3–5 weeks". The brief's instruction to use "the units the other
studies use" has no referent, and inventing a person-week figure would be exactly the invented number
this document is meant not to produce.

Effort is therefore given as **named deliverables, sized against comparable modules already in the
tree**, which is a figure that can be checked:

| Deliverable | Comparable in tree | Scale |
|---|---|---|
| Config-partition block/file HAL behind `np_hexmap_nvram_read/write` (`OI-HEXMAP-01`) | `np_log_backend.c`, 256 lines, backing four HAL entry points over two partitions | ~1 module, comparable |
| Map 3 journal: record format, append, scan-recovery, watermarks, journal-full degradation | `np_accel_shdr.c`, 325 lines | ~1 module, somewhat larger |
| Sync protocol, hub side (§6) | — no close comparable | 1 module |
| Reconciliation + coarsening (§6.5) | `np_pbm_cal_bridge.c` pattern | small |
| Host tests | `np_module_map_tests.c` 1,278 lines / `np_log_backend_tests.c` 221 | 2 new test binaries |
| CI checks | `OI-NVRAM-08` (journal survives a version bump), `OI-NVRAM-09` (no Map 2 → SHDR path) | 2, each falsified in both directions per `NP-CONV-001` §8 |

**The two dependencies that are not effort and cannot be worked around by it** are `OI-NVRAM-01`
(the Config journal area has no address space) and `OI-NVRAM-12` (LittleFS is unvendored SOUP). Both
are decisions, and no amount of firmware work substitutes for either.

---

## 11. Decisions

| ID | Decision |
|---|---|
| **D-1** | **The HAL owns one region of one partition and abstracts the record, not the medium.** "NVRAM" names no component in this design; every guarantee belongs to the Config partition |
| **D-2** | **The medium is the eMMC Config partition.** No discrete NOR, FRAM or MRAM. It already exists, already mounts before session start, is already zeroed by factory reset R-7, and adds no BOM on a gross-margin-negative configuration |
| **D-3** | **Config's key is NeurOne-derivable, so nothing of UHDR class may be written there** — a storage boundary, stricter than the upload boundary, and it decides what the HAL is allowed to offer a writer for |
| **D-4** | **No SNVS register is part of any durability property.** With no `VBAT` rail the LP GPRs are warm-reset-only; two existing callers assume otherwise (`OI-NVRAM-05`) |
| **D-5** | **Map 3 is a separate file with its own magic, version and policy** — reached independently from `file_max` (§3.5), from cache-versus-record (§7.2), and from write granularity (§3.5) |
| **D-6** | **`np_hexmap_nvram_write()` must never destroy the live record before the replacement is durable.** No truncate-in-place; LittleFS's copy-on-write commit supplies A/B semantics without a second slot |
| **D-7** | **Fail-closed means "declare the gap", not "report zero", for a record.** An unreadable journal reports *unknown since ordinal S*; a fabricated zero would drive Map 2's totals backwards |
| **D-8** | **No last-gasp write.** `SEAT#` has no firmware consumer, is aggregated at Class B, and its pre-break window is undimensioned. Correctness never depends on advance warning |
| **D-9** | **Every ordinal is the existing Config eMMC session counter plus a per-record `seq`.** No new counter, no wall-clock field, ever |
| **D-10** | **The control software initiates every sync.** The helmet cannot know which peer holds Map 2 for these modules |
| **D-11** | **Acknowledgement precedes retirement; retirement is a watermark advance, not an erase.** A peer that loses its own store can re-request from a lower watermark |
| **D-12** | **Journal-full degrades: keep totals, drop detail, set `detail_lost`.** Never wrap over unsynced rows; never block a session for want of a phone |
| **D-13** | **Map 2 is not authoritative for a module's existence. Only the socket is** |
| **D-14** | **Reconcile monotone counters by `max`; never sum, never roll back, and never upload a divergence magnitude** — a divergence is as joinable as a baseline (`NP-MOD-ID-001` §6.3) |
| **D-15** | **Map 1 is a legitimate generated artefact under `NP-NPPS-REF-001` §1.6 on origin grounds, but §1.6's stated *reason* does not transfer** — a new tile minor needs no rebuild — which is what makes the update check mandatory rather than convenient |
| **D-16** | **Blob version 0x0003 → 0x0004 adds `(major, minor)`; no migration path, matching the existing rule.** `REC_BYTES` 175 → 177, blob(80) 14,012 → 14,172 |
| **D-17** | **An unknown `(major, minor)` is a limits problem, not an identity problem.** Elements are known from the tile's own inventory; ranges are not. Do not drive an element whose limits you do not hold — and **never substitute the nearest known minor** |
| **D-18** | **No sync-boundary field and no per-window delta may be uploaded.** What reaches SHDR is the running total. A delta is a timestamped count once the party holding it can date the interval's ends |
| **D-19** | **Map 2 is app-side and inherits UHDR-class handling**; no code path may read Map 2 and write SHDR, mirroring the `SHDRUploader`/`ConsentStore` independence in CLAUDE.md §6 |
| **D-20** | **Insertion events are not recorded anywhere.** Mate cycles exist as a total only; an event timeline is a record of changes in treatment target and nothing needs it |

---

## 12. Risk register

Scales per `NP-RM-001` §4. Status: **MITIGATED** (controls in place, residual acceptable) · **OPEN**
(no adequate control yet) · **ALARP** (control selected, verification outstanding).

| ID | Sev | Hazard | Cause | Consequence | Control | Owner | Status |
|---|---|---|---|---|---|---|---|
| **RISK-NVRAM-01** | **HIGH** | Unsynced module history is destroyed by a firmware update | Map 3 inherits the inventory blob's reject-and-rebuild policy, which is correct for a cache and fatal for a record | Irrecoverable loss of the only on-device record of module usage; Map 2 totals silently regress below Map 4 | **D-5** separate file, separate version, tail-additive reader; **D-7** declare-the-gap; **D-14** reconcile by `max` so a regression cannot be written back | FW | **OPEN — `OI-NVRAM-08` check not written** |
| **RISK-NVRAM-02** | **HIGH** | A per-window delta plus an app-side clock reconstructs per-socket usage over time in SHDR | Map 3's natural output is a delta; the initiating party holds a clock; `TIME-01` inspects for time *types* and an ordinal is an integer | `NP-MOD-ID-001` §7.5.1.1's registrant-scoped consent model loses the absence it rests on, in every configuration | **D-18** totals only, no boundary field; **D-19** structural separation of Map 2 from any SHDR writer | FW + Privacy | **OPEN — no CI guard (`OI-NVRAM-09`)** |
| **RISK-NVRAM-03** | **HIGH** | The store this HAL needs cannot legally exist as specified | `EMMC-CFG-02`'s raw journal area has no address space inside a LittleFS instance that owns all 16 MiB | Either the journal is built on corrupting writes, or it is not built and Map 3 has no high-frequency substrate | None yet — it is a specification decision, not a firmware one | FW + Quality | **OPEN — `OI-NVRAM-01`, BLOCKING** |
| **RISK-NVRAM-04** | **HIGH** | A tile is driven against substituted limits | Unknown `(major, minor)`; nearest-known-minor substitution is the tempting recovery | Over- or under-drive of an emitter whose specifics the minor exists to track; only backstop is the 62 °C thermal cutoff | **D-17** absolute substitution ban; present-and-unclassified state; `check_placement()` refusal | FW + Safety | **MITIGATED — but see `OI-NVRAM-10`** |
| **RISK-NVRAM-05** | MEDIUM | A torn write discards a good inventory unnecessarily | `np_hexmap_nvram_write()` implemented as truncate-then-write | One spurious full re-poll per interrupted write; user-visible bring-up delay, no data loss | **D-6** contract on the HAL, plus a power-fail-injection test at each program-unit offset | FW | **ALARP — test not written** |
| **RISK-NVRAM-06** | MEDIUM | Config partition wears out early | Per-event journalling implemented as full-blob rewrite: 25.2 MB/session, ~4.6 years at clinic rate (§3.5) | eMMC `PRE_EOL_INFO` reaches 0x03 and `EMMC-WE-02` blocks session start | **D-5** small append records instead of blob rewrite; `EMMC-WE-01` already logs the wear indicator every boot | FW | **MITIGATED by design choice** |
| **RISK-NVRAM-07** | MEDIUM | Map 3 outgrows the file it shares with the inventory | `file_max` on Config is 65,536 B; the blob takes 14,012 at 80 sockets, leaving 644 B/module and no warning until a write fails | Silent truncation or a run-time write failure mid-session | **D-5** separate file with an explicit stated bound; **D-12** degrade-with-flag when it is reached | FW | **MITIGATED — bound not yet chosen (`OI-NVRAM-13`)** |
| **RISK-NVRAM-08** | MEDIUM | An interrupted factory reset is not detected | `NP_SNVS_RESET_IN_PROGRESS` survives a warm reset, not a power removal, and there is no `VBAT` rail | Device boots with UHDR/SHDR/Config half-erased — the exact outcome `np_main.c` says must not happen | None. Recorded, not fixed here | FW + Privacy | **OPEN — `OI-NVRAM-05`** |
| **RISK-NVRAM-09** | MEDIUM | The filesystem every durability claim rests on is unmanaged SOUP | `NP-SW-001` §9.4 lists LittleFS as *"2.x"*, Class B, verification *"Power-loss testing per LittleFS test suite"* — no vendored copy, no pinned tag, no SHA, no §7.1.2 anomaly evaluation, and nothing under `firmware/vendor/` | Every atomicity property in §4 is asserted against a component with no configuration identity | None yet | FW + Quality | **OPEN — `OI-NVRAM-12`, BLOCKING** |
| **RISK-NVRAM-10** | LOW | Map 4's layout does not fit the part actually fitted | `NP-MOD-ID-001` §5.2 sizes four 32-byte slots at **exactly** 128 B, against ATtiny424/426/427; CLAUDE.md §3 still names the retired design's **ATtiny402** for the on-module MCU | Fewer slots than the crash-safety argument assumes, or no room for a version field | `NP-HW-HEXTILE-001` §6.2 selects ATtiny426/427-class, which is the governing document | FW + EE | **MITIGATED — documentation drift only (`OI-NVRAM-14`)** |

---

## 13. Open items

| ID | Description | Owner | Blocking |
|---|---|---|---|
| **OI-NVRAM-01** | **`EMMC-CFG-02`'s raw-write journal area has no address space.** Config's LittleFS instance is `block_size` 4,096 × `block_count` 4,096 = 16 MiB = the whole partition. Either shrink `block_count` to carve the journal out, or make the journal LittleFS files and void the clause's "no LittleFS overhead" rationale. Map 3's append substrate depends on the answer | FW + Quality | **BLOCKING — Map 3 implementation** |
| **OI-NVRAM-02** | `NP-FW-EMMC-002` §C.3's *"Config partition offset 0x1000 — UKMD record"* addresses a byte offset inside a LittleFS-managed block range. Resolve with `OI-NVRAM-01` | FW + Security | UKMD record integrity |
| **OI-NVRAM-03** | `EMMC-CFG-01`/`-02` do not list the `"NPMP"` blob among Config's contents or among the fields writable during normal operation, and `np_module_map_persist()` writes it on every UID change. Widen the specification (probable) or forbid the write (implausible), but decide | FW + Quality | Config specification correctness |
| **OI-NVRAM-04** | **The pad-length stagger between contact group 3 and `SEAT#` is not dimensioned**, and no extraction-velocity assumption is stated, so the pre-break warning window has no duration. §4.3 designs around its absence; any future use of `SEAT#` as a timing signal needs this number | EE + ME | Any pre-break notification |
| **OI-NVRAM-05** | **`NP_SNVS_RESET_IN_PROGRESS` and `NP_SNVS_ANON_IN_PROGRESS` are documented as power-loss recovery flags and can only be warm-reset flags** — the SNVS LP domain has no `VBAT` supply. The factory-reset case is the serious one: a power loss during R-4…R-9 is undetected and the device boots half-erased | FW + Privacy | **`NP-MOD-ID-001` §10's factory-reset rotation test** |
| **OI-NVRAM-06** | Map 4's one-write-per-session-end cadence means **a session interrupted by tile extraction contributes nothing to that tile's odometer**. Quantify whether that matters for `NP-MOD-ID-001` §7.4's model, or accept and document it | FW | Odometer fidelity |
| **OI-NVRAM-07** | Map 3's ordinal indexing gives predictive maintenance a trajectory with unknown, non-uniform spacing in time. This is `OI-EMMC2-13`'s general defect acquiring another instance; recorded so the review gate does not discover it | FW + Data | `NP-MOD-ID-001` §7.4 review gate |
| **OI-NVRAM-08** | **Write and falsify the CI check** that a journal written under version *n* loses no record when read under *n+1*. Per `NP-CONV-001` §8 it must be falsified in both directions before it is trusted | FW + CI | `RISK-NVRAM-01` |
| **OI-NVRAM-09** | **Write and falsify the CI check** that no code path reads Map 2 and writes SHDR, and that no SHDR column or upload field names a sync boundary or a per-window delta. `warranty-nojoin-ci.yml` is the pattern; note that `TIME-01` cannot catch this because an ordinal is an integer | FW + Privacy | `RISK-NVRAM-02` |
| **OI-NVRAM-10** | **Is there a Class C bound on per-tile emitter drive current that is independent of Map 1's ranges?** If not, a wrong range is bounded only by the 62 °C thermal cutoff — a thermal limit standing in for an optical one — and §9's classification must be re-derived before any tile variant with differing ranges ships | Safety + EE | **Class B classification durability** |
| **OI-NVRAM-11** | `np_module_map.h`'s NVRAM-sizing comment still carries the v2 figure (*"8 + 128*139 + 4 = 17,804 bytes"*) after the v3 calibration payload took `REC_BYTES` to 175 and the blob to 22,412 — the number the code computes and the tests assert. Stale in the header an integrator sizes the partition region from | FW | Documentation accuracy |
| **OI-NVRAM-12** | **LittleFS is the only Class B SOUP item that is neither vendored, version-pinned, nor anomaly-evaluated.** `NP-SW-001` §9.4 gives it *"2.x"* and one line of verification, against per-file SHA-256 provenance records for FreeRTOS, Monocypher, CMSIS-Core and CMSIS-Device. Nothing exists under `firmware/vendor/`. Every power-loss guarantee in §4 is asserted against it | FW + Quality | **BLOCKING — any claim of power-loss atomicity** |
| **OI-NVRAM-13** | Choose Map 3's per-device size bound and the per-uid detail budget within it, against a stated sessions-between-sync assumption. §3.5 gives the ceiling that must not be crossed; nothing in the record gives the expected sync interval | FW + Product | Map 3 implementation |
| **OI-NVRAM-14** | CLAUDE.md §3 still names **ATtiny402** as the on-module MCU, which `NP-HW-HEXTILE-001` §6.2 identifies as the retired design's part (10-bit ADC) and replaces with ATtiny426/427-class. `NP-MOD-ID-001` §5.2 sizes Map 4 at exactly 128 B against the latter, with zero slack. Correct the CLAUDE.md bullet | Systems | Map 4 storage budget |
| **OI-NVRAM-15** | `NP-MOD-ID-001` is **DRAFT**, *"pending principal approval and the two BLOCKING open items in §9"*. This document builds on MODID-4/5/6 as though settled. If the odometer, the ref derivation or the coarsening rule changes, §6 and §8 change with them | Principal | This document's §6 and §8 |

---

## 14. Deliverable summary

**What the HAL is.** One region of the eMMC Config partition, reached through
`np_hexmap_nvram_read/write` — the resolution of `OI-HEXMAP-01`. It abstracts a record, not a medium;
there is no NVRAM component in this design and calling the partition NVRAM has been costing clarity.

**Two records, two disciplines.** The inventory blob is a **cache** and keeps reject-and-rebuild for
the life of the product, with the single added contract that a write never destroys the live record
before its replacement is durable. Map 3 is a **record**, in a separate file with its own magic and
version, appended as small self-checking records so that a torn write costs one record rather than all
of them, and so that its version rule can be the opposite one: tail-additive forever, never
discard-and-rebuild.

**No clock, and no pretence of one.** Every ordinal is the existing Config eMMC session counter.
"Since the last sync" is a half-open interval of ordinals with no duration, no computable rate, and no
staleness the helmet can perceive.

**Sync.** The control software initiates, supplies a per-uid watermark, receives the rows above it,
and acknowledges by ordinal; the helmet retires lazily. A full journal degrades to totals-plus-a-flag
rather than wrapping over unsynced rows or blocking a session. Map 4 seeds a helmet that has never
met a module; disagreements reconcile by `max`, never roll back, and never upload a magnitude.

**The unknown pair.** It is a limits problem. The tile's elements are known from its own inventory;
what is missing is the safe range. So the hub checks for an update, then reports a maintenance error,
records the module as present-and-unclassified — a state the wire format already has — refuses only
protocols that name that socket, and **never substitutes the nearest known minor**, for the same
reason `np_hex_addr_pack()` refuses to mask and UID-keyed calibration refuses to fall back.

**Classification.** Everything the HAL stores is device configuration or device condition, and the
constraint that decides it is custody: Config's key is NeurOne-derivable, so a UHDR-class write
discloses at the moment of writing, with no upload in the path. The finding that took the most work:
**Map 3's natural output is a delta, and a delta is a timestamped count as soon as the party holding
it can date the interval's ends** — which the app can and the helmet cannot. Totals go to SHDR;
boundaries and deltas do not; Map 2 stays app-side and no code reads it and writes SHDR.

**IEC 62304.** SW-02 Class B, because the Class C processor has no electrical path to the eMMC, no
stored value can widen `granted_mask`, and every failure path leaves the inventory empty. The residual
is per-tile drive magnitude, and it is `OI-NVRAM-10`.

**Cost.** **Zero new parts and $0.00 BOM delta at 20 and at 30 tiles** — Map 4 lives in EEPROM already
paid for at $0.50/tile, a correction to the brief's premise. Had it needed a discrete part the record's
own price is ~$0.12/tile, $2.40 and $3.60 respectively. The cost of *not* doing it is the large one:
without persistence, UID-keyed calibration falls back to firmware defaults on every boot, which
renders inert the **$11.53/tile driver-plus-metering line — $346 per headset at 30 tiles** — that is
the product's primary technical differentiator.

**Open items.** Fifteen, of which three are blocking: `OI-NVRAM-01` (the Config journal area has no
address space), `OI-NVRAM-12` (LittleFS is unvendored, unpinned, unevaluated SOUP under every
atomicity claim here), and `OI-NVRAM-05` (blocking the factory-reset rotation test that
`NP-MOD-ID-001` §10 requires).

---

## 15. Revision history

| Rev | Date | Author | Description |
|---|---|---|---|
| 1 | 2026-08-27 | NeurOne Firmware + Data Architecture | Initial release. Specifies the hub NVRAM HAL under the four-map module record — the resolution of `OI-HEXMAP-01` — with twenty decisions (D-1…D-20), ten risk rows and fifteen open items. **Corrects six claims in its own scoping brief:** Map 4's substrate is not open and is not inside `OI-HEXTILE-06` (which is a photodiode-population decision) — `NP-HW-HEXTILE-001` D-3 and `NP-MOD-ID-001` MODID-4 already specify it, at **$0.00 BOM**; the module-change power cut is a per-cluster `SAFE_EN[n]` emitter-rail cut, not a hub power loss, which makes the atomicity requirement broader rather than narrower; `SEAT#` cannot supply a last-gasp write window because it has no firmware consumer, sits at Class B, and its pre-break interval is undimensioned; the version-bump window closes at the first unsynced Map 3 row, not at first ship, because a cache and a record need opposite policies; and two documents cited as house-style precedent are not on `main` (`NP-FEAS-PBMCH-001` does not exist; `NP-FW-BENCH-001` is unmerged), so the in-force no-wall-clock precedent used here is `NP-FW-EMMC-002` §H.3.1–H.3.2. **Four defects found on the store itself:** `EMMC-CFG-02`'s raw journal area has no address space inside a LittleFS instance owning all 16 MiB (`OI-NVRAM-01`, blocking); `NP-FW-EMMC-002` §C.3's offset-`0x1000` UKMD address has the same problem; the two SNVS power-loss recovery flags can only be warm-reset flags on a device with no `VBAT` rail (`OI-NVRAM-05`); and LittleFS is the only Class B SOUP item that is neither vendored, pinned nor anomaly-evaluated (`OI-NVRAM-12`, blocking). **Privacy finding:** a per-window delta becomes a timestamped count in the hands of the only party that can date the window's ends, so no sync-boundary field or delta may be uploaded — this walks around `TIME-01` rather than breaching it, and `NP-MOD-ID-001` §7.5.1.1's consent model rests on exactly that absence. No code changed with this document. |
