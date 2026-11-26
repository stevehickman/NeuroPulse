# Hub NVRAM Hardware Abstraction Layer and the On-Helmet Module Record

**Project:** NeurOne
**Document:** NP-FW-NVRAM-001
**Revision:** 22
**Date:** 2026-11-26
**Status:** DRAFT — design study; not a release baseline. Every behaviour below is a proposed engineering commitment traced to a cited source. **No new part is proposed.** Rev 11 closes `OI-HEXMAP-01`: `np_hexmap_nvram_read/write` are bound to `npmp.bin` through `np_cfg_store` as first-party code, not platform seams, and `REQ-LFS-02` is a property of `np_module_map` itself, so a restored record answers nothing until the power-on poll confirms its UID (D-32, §4.2.1). Rev 10 closes `OI-NVRAM-18`: the hub decides that a mate happened, from its previous observation of the socket, and `U1` applies the increment at most once per power-up, so `mate_cycles_observed` no longer depends on a completed session (D-31, `NP-MOD-ID-001` §5.3.1). Rev 9 closes `OI-NVRAM-17`: the factory-reset boot check is called at bring-up before anything opens UHDR or SHDR or reads Config, and a result that does not permit them opens neither (§3.4.1). Rev 8 closes `OI-NVRAM-07`: the trajectory's axes are use, not time, SHDR already carries month-granular calendar age per module, and only sub-month cadence is missing, which no modelled mechanism needs; §7.4's fit must carry presence and calendar age beside exposure (D-30, §5.3.1). Rev 7 closes `OI-NVRAM-06`: the session an extraction interrupts is lost from Map 4 and not from the hub, so §7.4's model is unaffected if its exposure is hub-sourced (D-28), and §6.5 stops flagging that shortfall as a fault (D-29, §6.5.1). Rev 6 closes `OI-NVRAM-04`: nothing in this document needs the `SEAT#` pre-break window's duration, and the stagger dimension moves to the document whose own mechanism needs it (§4.3.1). Rev 5 gives the Config store a journal reader that steps by each row's own length, closing `OI-NVRAM-16` (§7.2.1). Rev 4 makes R-3 read its marker back before the first erase (§3.4.1). Rev 3 writes and falsifies the two CI checks §7.2 and §8.3 asked for, and corrects every claim that an SNVS flag survives power loss. Rev 2 resolves the store defects Rev 1 found (§3.3.1) so Map 3 can be built at all. See §11 (Decisions), §13 (Open items).
**Effective Date:** —
**Author:** NeurOne Firmware + Data Architecture
**Approved By:** — (pending design review)
**References:** CLAUDE.md §4 (processor stack, safety architecture, power), §5 (UHDR/SHDR), §6 (consent subjects); `NP-FW-EMMC-001` Rev 2 §4 (partition layout), §5.2 (LittleFS instance parameters), §9 (Config/Calibration partition), §12 (session data classification), §14 (write-endurance monitoring), §16 (processor ownership); `NP-FW-EMMC-002` Rev 2 §A (warranty token), §B.2 (factory reset R-7), §C.3 (Config UKMD record), §G.2 (record-denominated windows), §H.3.1–H.3.2 (no wall clock); `NP-MOD-ID-001` Rev 1 §4 (`module_ref`), §5 (on-module odometer, MODID-4), §6 (history portability, MODID-5/6), §7.5.1.1 (the two absences), §9 (open items); `NP-HW-HEXTILE-001` Rev 8 §6.2 (U1 on-module MCU), §6.4 (driver + metering BOM), §7.2–7.3 (19-position socket, `SEAT#`, contact sequencing); `NP-DRV-SHELL-002` Rev 4 §5.1.4 (UID EEPROM deleted), §6 (`SAFE_EN[n]`), §10.1 (interconnect BOM); `NP-HEX-ZM-001` Rev 3 §4a (SMART-1, `check_placement`), §5.4a (cluster clamp); `NP-COST-001` Rev 2 §6 (OI-HEXTILE-06 options); `NP-NPPS-REF-001` Rev 14 §1.6 (no build-time cache of protocol content); `NP-SW-001` Rev 3 §3.2 (Class B rationale for SW-02), §5.2 (SW-02 module inventory), §9.4 (SOUP); `NP-CONV-001` Rev 6 §4, §6, §8; `firmware/hub_control/include/np_module_map.h`; `firmware/hub_control/src/np_module_map.c`; `firmware/bootloader/include/np_config.h`; `firmware/hub_control/include/np_log_backend.h`; **added at Rev 2:** `NP-SOUP-LFS-001` Rev 1 (LittleFS SOUP record + hazard analysis), `NP-FW-HUB-001` Rev 1 §6.5 (log durability model), `NP-CONV-001` Rev 6 §7 (how a document revises its own position)
**Related Issues:** #339 (`OI-NVRAM-01`, `-02`, `-03`, `-12`, `-13`); #443 (`OI-NVRAM-07`); #444 (`OI-NVRAM-04`, `-05`, `-06`, `-08`, `-09`, `-16`, `-17`, `-19`, `OI-HEXMAP-01`)
**Gate:** — (no programme gate; this document specifies the resolution of `OI-HEXMAP-01`)
**IEC 62304 Class:** **SW-02 Class B.** Argued in §9, and the argument turns on the fact that the Class C processor has no electrical path to the store. **No SW-01 source changes and no bit added to any Class C wire format.**
**Supersedes:** None — new document.
**Parent Document:** `NP-FW-EMMC-001`

---

> **Rev 20 (2026-11-26) — `OI-NVRAM-21` CLOSED: `map3.jrn` and the Config store's journal API are removed.**
>
> Map 3 is the upserted record `NP_CFG_FILE_MODULE_USAGE` (D-35), so the per-session journal file it replaced had no
> reader and no writer outside its own tests. Removed: the file id `NP_CFG_FILE_MAP3` (`map3.jrn`, never written), the
> store's `TAIL_ADDITIVE` policy, `np_cfg_store_journal_append()`, `np_cfg_store_journal_read()` and
> `np_cfg_store_journal_read_rows()` with their record-check types, and the tests that existed only for them (the
> store's journal sweep, `np_cfg_store_tests` case 9 for the variable-length reader, the raw `L-4` sweep and its
> falsification in `np_lfs_powerloss_tests`). Two Config sweeps that used the journal only as a workload keep their
> coverage through the store's replicated write. `NP-SOUP-LFS-001` agrees: `L-4` retired, `check-lfs-caller-rules.ts`
> R4 narrowed to "no Config file outside `REBUILD` and `REPLICATED`", R5 and R6 retired. The retired journal
> discipline below (§4.2(b), §3.3.1.5, §6, §7.2.1) stays as the record. One question it leaves is that document's:
> whether Map 3's `REPLICATED` record needs a guard from emission-limit consumers (`NP-SOUP-LFS-001` `OI-LFS-15`).

> **Rev 18 (2026-11-25) — `OI-NVRAM-20` CLOSED: Map 3 is one running-total row per module, and the retired journal codec is gone.**
>
> The §8.5 row is complete. `NP_CFG_FILE_MODULE_USAGE` (`np_module_usage.c`, `REQ-FWHUB-93`) **is** Map 3: one
> upserted record of one 64-byte row per module (§3.3.1.6), holding sessions present and used with their time, faults
> per code, mate cycles, `dose_accum`, `throttle_count`, `fault_flags` and `detail_lost`. The per-session layout of
> §3.3.1.5 stays retired in place, and its codec (`np_map3_record.{h,c}`, `np_map3_record_tests`) is removed, because
> nothing may write it. §4.2(b), §6, §6.3 and §7.2 are revised for an upserted record: a download is a snapshot read that
> changes nothing on the helmet, a full table gives up a row visibly instead of dropping detail, and the record has one
> fixed length so a later version's longer row is read and carried through (D-35 to D-37). Who sees which field is
> §8.5's table: the app every field, the fleet the totals under the month anchor, `dose_accum` and `throttle_count`
> only under `NP-MOD-ID-001` §7's opt-in, and `fault_flags` bit 0 never. `map3.jrn` and the store's journal API have no
> consumer left; whether they go is **`OI-NVRAM-21`**.

> **⚠ REV 11 (2026-09-28, GitHub #444) — `OI-HEXMAP-01` CLOSED: the module map's blob has a store, and a restored record waits for the poll.**
>
> `np_hexmap_nvram_read()` and `np_hexmap_nvram_write()` were a pair of platform traps. They are now
> `firmware/hub_control/src/np_hexmap_nvram.c`, which reaches `npmp.bin` through `np_cfg_store` as a
> `REBUILD` file: every read is content-checked by `np_module_map_blob_verify()` (magic, socket count,
> exact length, CRC-32), a write is an in-place replacement littlefs commits at close (L-3), and a blob
> the map would refuse is never written. They are first-party code, the way the session count became
> in `OI-LFS-12`, so the SW-02 platform census goes **103 → 101**.
>
> **`REQ-LFS-02` is enforced by the module, not by a boot ordering (D-32).** A valid blob can be old,
> and then it describes whatever module was in the socket when it was written. Each socket record now
> carries a RAM-only `confirmed` flag that `load()` leaves false. Until `np_module_map_apply_poll()`
> reports the same UID for that socket in this boot, the record answers nothing: resolve,
> resolve_group, check_placement, get_cal, set_cal and socket_uid all treat the socket as empty. A
> matching poll confirms it without re-inventory, and a different UID replaces it. So the future boot
> integration cannot break the rule by calling in the wrong order.
>
> **Evidence (§4.2.1):** `np_module_map_tests` gains the rule across all six readers, falsified by
> reducing the check to `module_present` (10 failures). `np_cfg_store_tests` case 10 drives
> `persist()`/`restore()` through the real store on the 80-socket, 14,012-byte blob. It covers a reboot,
> a module swapped while the hub was off, medium damage restoring an **empty** map, a refused write that
> leaves the live blob alone, and a power cut at every op of `persist()` (35 ops × 3 tear models, 105
> cuts, 0 violations). **What is still not built:** the call sites. The power-on poll needs
> `OI-HEXMAP-02`'s `inventory_fn`, and the store needs #340's block device. Until then every restore
> reads an unbound store and leaves the map empty, which is the first-boot path.

> **⚠ REV 10 (2026-09-28, GitHub #444) — `OI-NVRAM-18` CLOSED: a mate is counted when it happens, not at session end.**
>
> `U1` sees every mate, because `VCC_3V3` is in contact group 3, but it cannot tell a mate from the
> hub powering up. The hub can tell them apart, but only for mates it was on to see. So the hub decides
> and `U1` applies. The hub counts a mate when a poll finds a UID where its **previous observation** of
> that socket was empty or a different UID. It never counts a first observation, such as after an
> absent or invalid inventory cache. It sends one increment command, and `U1` writes it at once,
> **at most once per power-up**, which makes a retry idempotent. The rule is `NP-MOD-ID-001` §5.3.1
> (MODID-4a). The count is a lower bound and names what it misses. It adds no field, and **D-20**
> holds: the hub records nothing about the event. **D-31.** `distinct_socket_count` has the same kind of
> gap and is raised as `NP-MOD-ID-001` `OI-MODID-09`. **Still open under #444:** `OI-HEXMAP-01`.

> **⚠ REV 9 (2026-09-28, GitHub #444) — `OI-NVRAM-17` CLOSED: the boot check has its bring-up call site.**
>
> `np_hub_control_app_main()` now calls `np_factory_reset_boot_check()` before anything opens UHDR or
> SHDR or reads Config. That is before `np_log_backend_init()`, and also before
> `np_session_count_load()`, `np_cvfs_init()` and `np_cons_init()`, because completing a reset erases
> all three partitions and writes fresh Config defaults. The result is read through one predicate,
> `np_factory_reset_boot_permits_data()`, which permits `NONE` and `RESUMED` and refuses everything
> else. On a refusal the hub calls `np_log_backend_refuse()` (nothing opened, every append fails
> closed, and `session_begin` opens no UHDR file) and `np_cvfs_init_unpersisted()` (the cervical
> fault summary runs from RAM and never loads from or saves to UHDR).
> `scripts/check-hub-bringup-order.ts` gains six constraints for this, plus a seventh that binds #340.
>
> **Until #340 binds a block device, every boot takes the refusal.** An unbound store cannot mount,
> and that reads as `UNKNOWN`, never as "no reset was running". This is the fail-closed answer, and
> the gate makes sure that when #340 adds `np_cfg_store_bind()`, the bind goes before the check.
> **`OI-NVRAM-19` raised and decided in the same revision (principal, option b):** a boot that refused
> UHDR and SHDR admits **no stimulation session**. `np_runner_load()` returns `NP_HUB_ERR_BOOT_REFUSED`
> before it touches the lease or the descriptor, so no session ever runs without its records. Until
> #340, that means no stimulation on any boot. That is accepted, because no target image can run a
> session yet, and a broken Config binding then fails loudly instead of stimulating unlogged. **Still open under #444:** `-18` and `OI-HEXMAP-01`.

> **⚠ REV 8 (2026-09-28, GitHub #443) — `OI-NVRAM-07` CLOSED: the trajectory lacks cadence, not time, and nothing modelled needs cadence.**
>
> §5.3 said an ordinal-indexed trajectory is a weaker predictive-maintenance input than a clock-indexed
> one. §5.3.1 checks that against `NP-MOD-ID-001` §7.4's model. Its covariates are use: drive time,
> sessions seated and thermal history, all measured without a clock. Idle aging acts over months, and
> SHDR's `module_life` row already holds `module_manufacture_date` and a month-truncated
> `last_seen_month`, so calendar age is known to about a month within `TIME-01`. Only sub-month cadence
> is missing. It stays missing (D-18), and its physical effect is heat, which the NTC measures.
>
> The finding the gate needed: a rarely-driven socket ages by the calendar and gains exposure slowly,
> so idle aging reads as a socket effect in a fit on exposure alone. **D-30** puts presence and calendar
> age in the §7.4 fit beside exposure, adds no field and no clock, and states the one assumption the
> gate cannot test. `NP-MOD-ID-001` §7.4 carries the note. `OI-EMMC2-13` stays open: it is about
> threshold provenance, and Map 3 was never an instance of it. **Still open under #444:** `-17`, `-18`
> and `OI-HEXMAP-01`.

> **⚠ REV 7 (2026-09-27, GitHub #444) — `OI-NVRAM-06` CLOSED: the interrupted session is accepted, bounded, and kept out of the fault flag.**
>
> A tile pulled mid-session never makes its session-end Map 4 write, so that session is missing from
> the part's odometer. The hub still holds the session and writes its Map 3 row (§4.3.1). §6.5.1
> answers the question the item asked. **It does not matter to `NP-MOD-ID-001` §7.4's model**, because
> Map 4 has no socket field, so every per-socket row the model fits is already the hub's. Map 4
> reaches the model only through a carried-in baseline, which MODID-6 already rounds down by up to
> 249 sessions. **D-28** makes that hold: every per-session value §7.2 admits is accumulated by the hub
> while the session runs, never read from the tile at session end or differenced from Map 4.
>
> Answering it found a consequence that does matter. Each interrupted session leaves Map 2 one session
> ahead of Map 4, which is §6.5's *Map 4 < Map 2* row, and that row calls it a fault. **D-29** marks the
> interrupted row with `fault_flags` bit 0 (`NP_MAP3_FAULT_UNSEATED_AT_CLOSE`) and compares Map 4 only
> against unflagged rows. A module moved without a completed session never records its mate cycle, and
> that is raised as **`OI-NVRAM-18`** against `NP-MOD-ID-001` §5.3. **Still open under #444:** `-07` (in
> #443), `-17` and `OI-HEXMAP-01`.

> **⚠ REV 6 (2026-09-27, GitHub #444) — `OI-NVRAM-04` CLOSED: the missing dimension is re-homed, not invented.**
>
> `OI-NVRAM-04` asked for the pad-length stagger between contact group 3 and `SEAT#`, and an
> extraction velocity, so that the pre-break window would have a duration. Asked the CLAUDE.md §18
> question, *what fails here if that number is missing?*, the answer is nothing. **D-8** already makes
> every record durable without a warning, and §4.3.1 shows that a dimensioned window would still not
> make a last-gasp write possible. The hub does not lose power on extraction, so no Map 3 write needs
> the window. The tile cannot see `SEAT#`, and the bus that would tell it breaks with its supply, so no
> Map 4 write can use the window. **No figure is written here, and no extraction velocity is assumed.**
>
> The stagger dimension is still required, but by `NP-HW-HEXTILE-001` §7.3's own mechanism, not by
> this document. That mechanism needs returns before supplies, and `SEAT#` last under worst-case tilt.
> Nothing tracked that dimension, so it is raised there as **`OI-HEXTILE-28`** (Rev 18), coupled to
> `NP-DRV-SHELL-002` `OI-SHELL2-12` (#437). **Still open under #444:** `-06`, `-07` (in #443), `-17`
> and `OI-HEXMAP-01`.

> **⚠ REV 5 (2026-09-27, GitHub #444) — `OI-NVRAM-16` CLOSED: the store can read a grown Map 3 row.**
>
> `np_cfg_store_journal_read_rows()` reads a tail-additive journal by stepping over each record by
> the length a caller-supplied check returns, never by one length fixed at the call.
> `np_map3_row_len()` is that check for Map 3. It is the same single-row test `np_map3_scan()` makes,
> so the store and the codec cannot disagree about where a row ends. `check-lfs-caller-rules.ts` R5 now
> governs both journal readers. `np_cfg_store_tests` case 9 appends a grown row (64 bytes plus a tail) between 64-byte rows
> through the real store and reads every row back. The same file read through the fixed-length reader
> loses every row from the grown one on, which is the falsifying direction (§7.2.1). Nothing calls
> either reader yet, because Map 3's owner is not written. **Still open under #444:** `OI-NVRAM-04`
> (#437's connector dimension), `-06` (a §6.5 decision), `-07` (in #443), `-17` and `OI-HEXMAP-01`
> (both wait on #340's block device).

> **⚠ REV 3 (2026-09-26, GitHub #444) — the two invariants now have CI checks, and the SNVS claims are corrected.**
>
> 1. **`OI-NVRAM-08` CLOSED.** The Map 3 row now exists in code (`np_map3_record.{h,c}`), and
>    `np_map3_record_tests` is the check that a journal written under version *n* loses no record
>    under *n+1*. It is falsified in both directions (§7.2.1). Writing it forced one layout decision:
>    §3.3.1.5's two padding bytes become a `len` + `ver` header, so every row describes itself
>    (**D-25**).
> 2. **`OI-NVRAM-09` CLOSED.** `scripts/check-map2-shdr-boundary.ts` checks that no file names Map 2
>    and SHDR together, and that no SHDR name marks a sync boundary or a per-window delta (§8.3.1). It
>    depends on one naming rule (**D-26**) and is falsified in both directions.
> 3. **`OI-NVRAM-05` CLOSED.** Every comment and specification clause that called an SNVS flag a
>    power-loss recovery flag now says warm-reset only. The anonymisation half needs nothing more,
>    because Scratch is erased on every boot. For the factory-reset half, the principal chose
>    §3.4.1's option A (**D-27**): a replicated marker in Config, written before the first erase. It
>    is implemented and tested, and its bring-up call site waits on #340 (`OI-NVRAM-17`).
> 4. **Two findings.** `np_cfg_store_journal_read()` takes a fixed record length, so it cannot read
>    the grown row D-24 budgets for (`OI-NVRAM-16`). `np_factory_reset_config.h` described the SNVS
>    domain as *"battery-backed"*, and it is not.
>
> `OI-NVRAM-04`, `-06` and `OI-HEXMAP-01` stay open. `-04` needs a connector dimension shared with #437,
> `-06` needs a decision on §6.5 that is not this revision's to take, and the HAL binding needs the block
> device (#340).

> **⚠ REV 2 — the store is now buildable, and the reason the Rev 1 defect existed is worth keeping.**
>
> Rev 1 found four defects on the store Map 3 needs, three of them on controlled specifications, and
> deliberately stopped at finding them (§3.3). Rev 2 **decides** them, because `OI-NVRAM-01` was
> blocking Map 3's implementation and a blocking item that nobody may resolve is a design that cannot
> start. The resolution is §3.3.1; the decisions are **D-21…D-24**. Per `NP-CONV-001` §7, §3.3's
> findings are **retained verbatim** and §3.3.1 sits after them — what it was, what replaced it, why.
>
> **The decision: the Config journal is LittleFS files. `EMMC-CFG-02`'s raw-write clause is void.**
>
> And the reason matters more than the verdict, because the clause was not merely unimplementable —
> **its stated justification was false.** It reserved a raw region *"no LittleFS overhead for
> high-frequency updates"*, and the arithmetic in §3.3.1 shows that LittleFS overhead is not what
> makes high-frequency updates expensive here. `prog_size` is 256 B, so appending a 32-byte record
> costs **one 256-byte program** inside the current block — a raw region cannot make the program unit
> smaller, because the program unit is a property of the NAND, not of the filesystem. What actually
> costs 4.6 years of endurance (§3.5) is **rewriting a 14 KB blob to record a 16-byte fact**, and that
> is a property of the blob being monolithic, which **D-5** already fixes by giving Map 3 its own
> file. *The raw region was solving the wrong problem, and it would have cost a re-implementation of
> the three properties §4 depends on — commit ordering, per-record CRC, wear levelling — on the Class
> B side, with no test suite.*
>
> **Two things Rev 2 also closes, and one it hands off.** `OI-NVRAM-02`'s *"Config partition offset
> 0x1000 — UKMD record"* becomes a named file (**D-22**); the code never depended on the offset —
> `np_uhdr_hal_config_read_ukmd()` has always abstracted it — so the address was a spec-side artifact
> only, and `NP-FW-EMMC-002` §C.3 is corrected in the same change. `OI-NVRAM-03`'s unlisted `"NPMP"`
> write is resolved by widening rather than forbidding (**D-23**), as Rev 1 predicted. And
> `OI-NVRAM-12` is **handed off, not closed**: `NP-SOUP-LFS-001` Rev 1 brings LittleFS under SOUP
> management and performs the hazard analysis — finding, on the way, that **LittleFS is not in the
> tree at all** (two `lfs_` tokens in all of `firmware/`, both comments), and that **this document's
> own Class B argument in §9 is not a property of LittleFS but of `np_module_map`'s
> reject-and-rebuild policy — the policy §7.2 specifies Map 3 to invert.** That is now `REQ-LFS-01`,
> and it is the one place where Rev 1's two correct decisions needed a rule to keep them safe
> together.
>
> **What Rev 2 does not do.** It does not edit `NP-FW-EMMC-001`, which is a `.docx` (`OI-CONV-04`).
> The three clause changes D-21…D-23 require are raised as a precise engineering change request,
> **`ECR-EMMC-001`** (§3.3.1.4), naming clause and replacement text, and `OI-NVRAM-01` stays open
> until that lands — **downgraded from BLOCKING to a documentation action**, because the design
> question it blocked is answered and Map 3 can now be implemented against this document.

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
>    `NP-FEAS-PBMCH-001` is on an unmerged branch (PR #292), as is `NP-FW-BENCH-001` (PR #297).
>    **This study's first reading of that — that `NP-FEAS-PBMCH-001` "does not exist anywhere in
>    the repository" — was wrong**, and wrong in a way this programme has now made twice: asserting
>    a negative about a repository known to have concurrent work in flight, having looked only at
>    `main`. The correct statement is *not on `main` yet*. Its no-wall-clock and classification
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

### 3.3.1 Resolution of (a), (b) and (c) — added at Rev 2

Rev 1 stopped at the findings above. `OI-NVRAM-01` was blocking Map 3, and it named two options
without choosing: *"Either the LittleFS `block_count` must shrink to leave the journal outside it, or
the journal must become LittleFS files and `EMMC-CFG-02`'s 'no LittleFS overhead' justification is
void."* This subsection chooses the second, on three grounds, the first of which refutes the clause
rather than outweighing it.

#### 3.3.1.1 The clause's justification does not survive the arithmetic

`EMMC-CFG-02` reserves the raw region *"raw-write, no LittleFS overhead for high-frequency updates"*.
That premise is testable against `EMMC-FS-01`'s own numbers.

```
EMMC-FS-01 (Config):  block_size 4,096 B   prog_size 256 B   file_max 65,536 B
Map 3 record (D-34):  64 B, prog-unit aligned

one 256 B program unit  = 4 records
one 4,096 B block       = 16 program units = 128 records, on ONE erase
```

A LittleFS append into an open file lands in the file's current block and costs **one 256-byte
program**. It does not erase a block; the block was erased once and is then programmed unit by unit
until full. **A raw region cannot improve on that, because the 256-byte program unit is a property of
the eMMC, not of the filesystem.** What LittleFS adds over a raw region is metadata — a commit tag
per program unit and a periodic metadata-block update — which is single-digit percent at this record
size, not a multiple.

So what was the clause protecting against? §3.5 answers it: the expensive operation in this design is
`np_module_map_persist()` **rewriting all 14,012 bytes of the inventory blob to record a 16-byte
fact**, which at a per-event cadence gives ~4.6 years at clinic rate. That cost is a property of the
blob being one monolithic record, and **D-5** already removes it by giving Map 3 its own file with its
own append discipline. The raw region was aimed at the right symptom and the wrong cause.

> **Recorded as a correction, not a preference** (`NP-CONV-001` §7). `EMMC-CFG-02`'s clause was
> *reasonable when written* — it predates the hex-tile architecture and the four-map record, and at
> that time "high-frequency Config updates" meant the session counter, not a per-module journal. What
> is retired is the **justification**, which no longer holds at the numbers now in force. The
> underlying concern — that write granularity is a first-class design parameter — is **not** retired;
> it is §3.5's conclusion and it is why D-24 exists.

#### 3.3.1.2 Shrinking `block_count` costs a migration on the one partition that must not be migrated

The alternative is to reduce Config's LittleFS `block_count` below 4,096 so a raw region sits above
the filesystem's last block. That is straightforward arithmetic and an unpleasant operation: it
**relocates every existing Config file**, and Config holds the **UKMD record** — the wrapped user key
material, without which a user's UHDR partition cannot be mounted at all (`NP-FW-EMMC-002` §C.4).
A migration that moves the UKMD record is a migration that can render a user's own health data
permanently unreadable, and NeurOne does not hold a second copy of that key (CLAUDE.md §5.1) so there
is no recovery path.

No device is provisioned today, so the migration is free today. **But the window closes at first
provisioning, not at first ship** — the same boundary error §7.2 corrects for the blob version bump,
and it is worth naming twice because both times the intuitive boundary is later than the real one.
Choosing the option that needs no migration at all removes the deadline rather than meeting it.

#### 3.3.1.3 A raw region means re-implementing, at Class B, exactly what §4 depends on

§4's atomicity argument needs three properties: power-loss-safe commit ordering (D-6), a per-record
CRC that localises a torn write (D-5), and wear levelling across the partition (§3.5). A raw region
has none of them. Choosing it means writing all three by hand, on the Class B side, with no upstream
test suite behind them.

`OI-NVRAM-12`'s complaint was that these properties were asserted against an **unmanaged**
dependency. The answer to an unmanaged dependency is to **manage** it — which
`NP-SOUP-LFS-001` Rev 1 now does — and not to replace it with a second dependency that is unmanaged
by construction because NeurOne wrote it last week.

#### 3.3.1.4 What changes, and where

**`ECR-EMMC-001`** — raised against `NP-FW-EMMC-001`, which is a `.docx` and therefore not edited
here (`OI-CONV-04`). Three clause changes, stated as replacement text so the ECR is actionable rather
than directional:

| Clause | Was | Becomes | Decision |
|---|---|---|---|
| `EMMC-CFG-02` | *"These fields occupy a dedicated Config partition journal area (raw-write, no LittleFS overhead for high-frequency updates)"* | *"These fields are held in LittleFS files on the Config partition. There is no raw-write region: `EMMC-FS-01`'s instance owns all 4,096 blocks, and at `prog_size` 256 B a raw region offers no smaller program unit than an append does."* | **D-21** |
| `EMMC-CFG-02` (whitelist) | session counter, lockout state, last-known PD profile | + the `"NPMP"` inventory blob (written by `np_module_map_persist()` on every UID change) + the Map 3 journal file | **D-23** |
| `EMMC-CFG-01` (contents) | — | + `"NPMP"` blob, + Map 3 journal, + the UKMD record as a named file | **D-22**, **D-23** |

**`NP-FW-EMMC-002` §C.3 is corrected in this change**, because it is Markdown and because the fix is
provably safe: the code has never used the offset. `np_uhdr_key.c` reaches the record through
`np_uhdr_hal_config_read_ukmd()` / `_write_ukmd()`, so *"Config partition offset 0x1000"* was a
spec-side artifact with no implementation depending on it. **D-22.**

#### 3.3.1.5 The Map 3 file budget — closes `OI-NVRAM-13` *(RETIRED Rev 16; replaced by §3.3.1.6 at Rev 18)*

> **RETIRED, NEVER WRITTEN.** Everything in this section describes the per-session journal the principal's ruling of
> 2026-11-23 replaced (§8.5). No helmet ever wrote a row of it, and its codec was removed at Rev 18 so that nothing
> can. It stays here, unedited below this note, as the record of what was replaced and why each number was chosen.
> The row Map 3 keeps is §3.3.1.6.

With the journal as its own LittleFS file (D-5), `file_max` = 65,536 B applies **per file**, so Map 3
has the whole of it rather than competing with the 14,012-byte inventory blob. §3.5's warning — that
a shared file would give 644 bytes per module with nothing warning until a run-time write failure —
does not arise.

**Record: 64 bytes**, chosen so that four records fit exactly one 256-byte program unit and **no
record straddles a program unit**, which is requirement 1 of §4.2(b). A 40-byte row would straddle one.
(D-34: the row was 32 bytes with 16-bit counts in Rev 2 to Rev 12 of this document; no hardware had shipped and no
row had been written, so the 64-byte row replaced it and there is no second layout.)

```
len           1 B      ver            1 B      uid                  8 B
seq           4 B      ordinal        4 B      session_count        4 B
dose_accum    4 B      throttle_count 4 B      fault_flags          1 B
reserved     29 B      crc32(len ‖ ver ‖ uid ‖ payload)             4 B
                                                                  ── 64 B
```

> **The header (D-25).** Offset 0 is `len` (the row's total bytes, CRC included) and offset 1 is `ver` (the layout
> version, ≥ 1, today 1). The CRC is the last four bytes, over `len ‖ ver ‖ uid ‖ payload`. The header is what lets a
> reader step by the row's own length, so a longer row written by newer firmware neither shifts every row after it nor
> ends the scan. The counts are 32 bits and are stored whole: no field holds a ceiling (`OI-UINT16-01`, the principal,
> 2026-11-13). `np_map3_record.h` holds the byte map.

> **Rev 15:** `ordinal`, `dose_accum` and `throttle_count` are UHDR class (§8.1). This layout is not
> to be written to Config until `OI-PRIV-38` (NP-PRIV-ANALYSIS-004) part 1 is ruled.
>
> **Rev 16 (ruled by the principal 2026-11-23, `OI-PRIV-38` (NP-PRIV-ANALYSIS-004)):** this
> per-session layout is **retired before it was ever written**. Map 3 becomes one running-total row per module (§8.1,
> §8.5); the new row is `OI-NVRAM-20`. The layout above stays in this section as the record of what was replaced, and
> nothing writes it.

**File bound: 49,152 B (48 KiB) = 768 rows.** Not `file_max`, deliberately: 16,384 B of headroom
means a future version that grows the row by up to 21 B still fits (768 × 85 = 65,280 < 65,536) without
re-deciding the bound, and §7.2's rule is that a **record's** version policy is tail-additive forever
— so the bound must survive a growth the blob's policy would simply discard through.

**Sessions-between-sync assumption, stated because `OI-NVRAM-13` requires one to be:** one row per
module per session, worst case all 80 sockets. 768 ÷ 80 = **9 rows per socket.**

| Use pattern | Sessions/day | Margin before `detail_lost` |
|---|---|---|
| Home, daily | 1 | **9 days** |
| Clinic | 12 | **0.8 days** |

The clinic figure is tight and is stated rather than rounded away. (It was 19 days and 1.6 days at the 32-byte row; the 64-byte row halves the row count, D-34.) Three things make it acceptable
and none of them is optimism: **(i)** a clinic device is on USB-C with the control software attached
for most of its duty cycle, so the sync interval there is hours, not days; **(ii)** exceeding the
assumption costs **detail, never a session and never a total** — §6.3's journal-full policy keeps the
running totals, drops per-window detail and sets `detail_lost`, and D-12 forbids both wrapping over
unsynced rows and blocking a session; **(iii)** the failure is visible, because `detail_lost` is a
flag the control software reads, not a silent truncation. **D-24.**

#### 3.3.1.6 The Map 3 row as built — added at Rev 18 (`OI-NVRAM-20`)

Map 3 is `NP_CFG_FILE_MODULE_USAGE` (`np_module_usage.h`): one REPLICATED record holding one row per module, upserted
at each session end (`NP-FW-HUB-001` §6.11, `REQ-FWHUB-93`). It is not a journal, so nothing is appended and nothing
is retired.

**The row: 64 bytes, version 1, little-endian.** A tile's row is keyed by its UID; an accessory's by its port (slot and
module type), because accessories report no identity (`NP-FW-HUB-001` `OI-FWHUB-38`).

```
off  size  field              off  size  field
  0     1  key kind             26     4  fault_code[4]
  1     1  fault_flags          30    16  fault_count[4]
  2     8  id (UID or port)     46     4  faults_other
 10     4  sessions_present     50     4  mate_cycles
 14     4  present_s            54     4  dose_accum (mJ/cm², summed over wavelengths)
 18     4  sessions_used        58     4  throttle_count
 22     4  used_s               62     1  detail_lost (0 or 1)
                                63     1  reserved, written 0, never reused
                                                                      ── 64 B
header: version (1) | row length (1) | row count (2)                  ──  4 B
```

Every count is 32 bits and stored whole, with no field holding a ceiling (`OI-UINT16-01`). The arithmetic adds with
saturation at 2³² − 1, as every running total in `np_module_usage.c` does. There is no `ordinal`, no `seq`, no
watermark and no clock (§8.5). A per-window value exists only in the app (§6).

**Size.** The version-1 fields take 63 bytes, and one reserved byte makes 64. **Rows:** `NP_MU_MAX_ROWS` = 128 socket
addresses (`NP_HEXMAP_MAX_SOCKETS`) + 19 hub slots (`NP_HUB_SLOT_MAX`) = **147**, one for every module that can be fitted
at once. At version 1 the rows take 4 + 147 × 64 = **9,412 B**.

**The record is 12,288 B whatever the row length** (`NP_MU_TABLE_BYTES`, `NP_CFG_REPLICA_TABLE_MAX_PAYLOAD`). The store
reads a replicated record at one fixed length, so a record whose length changed with its row would be unreadable to the
firmware on either side of the change (§7.2). Fixing the length leaves room for the row to grow: the longest row that
fits is ⌊(12,288 − 4) / 147⌋ = **83 B** (147 × 83 + 4 = 12,205 ≤ 12,288; at 84 B it would be 12,352). A later version
can therefore add up to 19 bytes per row without changing the record. 12,288 is 3 × 4,096, the smallest multiple of
4 KiB above 9,412 that leaves that room. **The 4 KiB multiple is not a medium bound** (Rev 19,
`OI-NVRAM-22` (b)): a copy is 12,304 B with its envelope, which is not a whole number of 4,096-byte blocks, so nothing on
the medium fixes the length, and how much room to leave stays a choice (UC-213). *(Rev 19, superseded Rev 21:* the one
growth any open item then named fitted: one dose total per wavelength in place of the sum, 8 B more.)* **The growth the
principal's ruling of 2026-11-26 names does not fit (Rev 21, §8.5).** Emitter use per emitter, at the finest
the hub can count, is one 32-bit on-time and one 32-bit session count per drive channel. A tile has 90 emitter sites
but at most three channels, one per wavelength (`NP-HW-HEXTILE-001` §4.2; T1-C carries 660, 808 and 1064 nm), so
those fields take 3 × 8 = 24 B. Dose per wavelength is UHDR and cannot stay in this Config row (D-3), so `dose_accum`'s
4 B go, and the row needs 24 − 4 = **20 B against the 19 B of room**, 1 B short. Time in 5 °F temperature bands, if
kept per module, adds 4 B per band, and the band count is not set (`OI-NVRAM-24`). Had dose stayed in the row per
wavelength (3 × 4 in place of 4), the growth would be 32 + 4N B. Nothing is resized here: the length is D-36's and
UC-213's, and how the fields get room is `OI-NVRAM-23`'s. No unit has shipped and no row exists, so whichever way it
goes there is no migration. **Accessory ports are counted too** (the principal, 2026-11-26), and a row has one layout
for every module, so it must hold the most channels any module drives, at 8 B each. The intranasal probe drives 4
(two sides, 660 and 808 nm on each, `REQ-NASAL-04`), so its 32 B make the need **32 − 4 = 28 B against 19**; at 147
rows a 4-channel row makes the record 147 × 92 + 4 = 13,528 B, past 12,288. The visual goggles' channel count is not
known: their 12-bit zone mask with two wavelengths would be up to 24 channels (192 B), but what the bits mean is open
(`OI-FWHUB-35`). Only 19 rows are ports, so a separate port record, or rows whose length differs by kind, are among
`OI-NVRAM-23`'s choices. Each copy on the medium is 12 + 12,288 + 4 = 12,304 B, in a file of its own
under `file_max` (65,536 B), and the two copies are 24,608 B.

**What the §4.2(b) rules become, and which are dropped.**

| §4.2(b) rule (journal) | For this record | Why |
|---|---|---|
| 1. No row straddles a program unit | **Does not bind.** 64 B would put four rows in a 256-byte unit, but the rows start at file offset 16 + 64 *i* (the 12-byte envelope header and the 4-byte table header), so row 3 spans bytes 208 to 271 | The rule protected a row appended alone, which a tear could half-program. This record is never written in part: each copy is replaced whole and littlefs commits it at close (D-6, L-3), the envelope's CRC-32 covers every byte, and the twin holds the previous generation. A torn write leaves a copy that fails its CRC, and the reader takes the other. No row can be half-accepted, so the row size is chosen by its content, not by the program unit |
| 2. A CRC per row, binding the UID | **Dropped.** One CRC-32 over the whole record, in the store's envelope | The per-row CRC stopped a row being recovered into the wrong module's history. Rows are never recovered one by one: the record verifies whole or is refused whole (`REQ-FWHUB-93` (d)) |
| 3. A monotonic `seq` per row | **Dropped** | Recovery is the twin, not a scan to the last good row |
| 4. Two watermarks | **Dropped** | Nothing is acknowledged or retired (§6) |
| `len` and `ver` per row (D-25) | **Kept, in the header:** one version and one row length for every row of the record | Every row of a record is written by one firmware at one length. The reader steps by the header's row length, as the journal's reader stepped by each row's `len` |

**Cost.** Static RAM in the store grows from 3 × 8,208 to 3 × 12,304 B (its two read buffers and its write envelope),
and `np_module_usage.c` holds the 12,288-byte image. Writes: 24,608 B per session end, which is 1/1,024 of the
25.2 MB per session §3.5 found would wear Config out in 4.6 years, so about a thousand times that life at the same rate.

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

### 3.4.1 The correction, and the marker the factory reset still needs — added at Rev 3

**What was corrected (2026-09-26, #444).** Every place that described `LPGPR1` or `LPGPR2` as a
power-loss flag now says warm-reset only. That covers `np_config.h`, `np_main.c`,
`np_factory_reset.h`, `np_factory_reset_config.h`, `np_anon_config.h`, `np_anon_scratch.c`, and
`NP-FW-EMMC-002` §B.4 and §D.6 (its Rev 4). The worst of them was `np_factory_reset_config.h`, which
said the SNVS domain was *"battery-backed"*. It is not. The flags keep working for the case they
really cover, a watchdog or software reset mid-sequence. No behaviour changed.

**The anonymisation half is closed by what already exists.** The bootloader erases Scratch on every
boot (`EMMC-SCR-01`, `zero_scratch_partition()`), and the per-run key was SRAM-only, so a power loss
mid-run leaves nothing readable and nothing to detect. `LPGPR2` is redundant, as the table above
said, and now its comments say so.

**The factory-reset half is open, and this is what it costs.** A cable pull between R-3 and R-11
boots with `LPGPR1` clear. The bootloader does not resume, and the device comes up in whatever state
the sequence reached:

| Power lost during | State at next boot | Consequence |
|---|---|---|
| R-4, or R-5 before SANITIZE completes | UHDR partly purged, SHDR and Config intact | The old warranty token and SHDR history survive. The next owner's usage attaches to the previous owner's pseudonym, which is exactly NP-PRIV-001 HIGH-03, the finding §B exists to close. Any UHDR that SANITIZE did not reach is still decryptable by the previous owner's key, because `ukmd.rec` is still in Config |
| R-6 | UHDR purged, SHDR partly zeroed, Config intact | The old token survives beside a partial SHDR history |
| R-7 | UHDR and SHDR gone, Config partly zeroed | Config may not mount. What the hub does with an unmountable Config in the field is unspecified; `np_cfg_store_format()` is manufacture-only |
| R-8 … R-10 | Config zeroed, new salt or token not yet written | No token, so no SHDR linkage. Recoverable only by a reset that nothing will now start |

**Specification of the marker (a requirement, not yet a design).** A factory reset must leave
durable evidence that it has started, before its first destructive step, in a place that (a)
survives a power removal, (b) the bootloader can read before mounting anything the reset erases, and
(c) is erased or cleared only after R-10 has committed. `LPGPR1` fails (a). Three candidates meet it:

| Option | Where the marker lives | For | Against |
|---|---|---|---|
| **A — the reset's own last erase** | A small `REPLICATED` file in Config, written at R-3 before R-5. Config is already erased last (R-7), so the marker outlives UHDR and SHDR | No new region and no hardware. The store, the replica envelope and the power-loss sweep already exist | A power loss **inside** R-7 can erase the marker along with Config. Closing that window needs a boot rule: an unmountable Config finishes the reset (R-7 … R-10). That rule also fires on a Config lost for any other reason. The cost there is low, because `ukmd.rec` is in Config and UHDR without it is already unreadable, but the rule mints a new warranty token and so de-links SHDR. That is a warranty-owner consequence, and the principal must accept it |
| **B — outside the erased set** | An eMMC boot or RPMB region, or a reserved LBA outside all four partitions | The marker is untouched by every erase, so no window remains | A partition-layout change to `NP-FW-EMMC-001` (a `.docx`, `OI-CONV-04`), and a second store with its own power-loss argument |
| **C — keep SNVS, give it a supply** | A `VBAT` feed to the SNVS domain, e.g. from the 22 F hub supercapacitor (CLAUDE.md §4.5) through a diode | Every existing caller becomes correct as written, and the RTC gains a backup domain too | A hardware change with its own hold-up arithmetic. It also changes §5's no-clock premise, which `NP-MOD-ID-001` §7.5.1.1 and TIME-01 rest on, so it reopens a privacy decision |

**Recommendation: A**, with the unmountable-Config rule stated as a decision rather than left
implicit. It needs no hardware and no `.docx` change, and it reuses a store that has already been
power-loss swept. Its one residual window is the one whose outcome (UHDR already unreadable) is
least harmful.

**Decided 2026-09-26: option A (principal). D-27.** Implemented the same day:

- **The marker.** `NP_CFG_FILE_RESET_MARKER` is a `REPLICATED` Config file (`ra/reset.mrk`,
  `rb/reset.mrk`) holding an 8-byte constant, with no time, count or identity in it.
  `np_factory_reset_execute()` writes it at R-3, after setting `LPGPR1` and **before R-5**. **A reset
  that cannot write its marker does not start.** It clears the flag and returns
  `NP_RESET_ERR_MARKER` with nothing erased. The store never removes the marker: R-7's erase of the
  whole partition takes it, so it adds one create per reset and no delete (`OI-LFS-08`).
- **The read-back (Rev 4, 2026-09-27).** R-3 then reads the marker back through
  `np_factory_reset_hal_marker_state()`, a fresh mount and a replicated read, and **the reset starts
  only if it reads `PRESENT`**. Anything else — `ABSENT`, `NO_STORE`, `UNKNOWN` — clears the flag and
  returns `NP_RESET_ERR_MARKER` with nothing erased. Why: `NP-SOUP-LFS-001` §13.18 found that an
  acknowledged Config write can be rolled back to the state before it (E4: a commit littlefs
  discards at its next fetch, or a medium that drops a program, `OI-LFS-07`). R-5's purge is
  irreversible, and it was the one step that acted on the marker's *acknowledgement* rather than
  its *presence* (`RISK-LFS-09`). Once verified, nothing else writes Config before R-7, so no later
  commit can take the marker with it. **Verified:** `np_factory_reset_tests` models an acknowledged
  write the medium did not keep and requires the refusal before any erase (trace `M`, no R-5). Its
  five new checks all fail with the read-back removed. `np_cfg_store_tests` restores the real medium
  to its pre-write bytes after an acknowledged write, and the same-boot read-back must say `ABSENT`.
  A read-back answered from RAM fails that check.
- **The boot rule.** `np_factory_reset_boot_check()` runs at bring-up before UHDR or SHDR is
  mounted. It completes the reset (R-5 … R-10, then clears the flag) when **any** of these holds:
  `LPGPR1` is set (a warm reset), the marker is present (a power loss between R-3 and R-7), or
  Config holds **no filesystem** (a power loss between R-7 and R-10's commit). It re-runs from
  **R-5**, not R-7, so an old SHDR history can never be uploaded under the token R-9 mints. A Config
  lost for another reason also takes the no-filesystem branch. That is the accepted cost of option
  A: UHDR is already unreadable without `ukmd.rec`, and the device gets a new warranty token.
- **Unknown is not absent.** Until this change, `np_cfg_store_mount()` returned `STORE_IO` both for
  a partition with no filesystem and for one that could not be read. It now returns
  `STORE_INTEGRITY` for the first. A read fault reports `NP_FR_BOOT_UNKNOWN`, and nothing is
  erased. The distinction puts a requirement on the unwritten block device (`OI-LOG-05..07`): a
  read fault must come back as `LFS_ERR_IO`, never `LFS_ERR_CORRUPT`.
- **Verified.** `np_factory_reset_tests` (58 checks, previously 24) confirms the marker precedes
  every erase. It simulates a power cut after each medium-touching step, with `LPGPR1` cleared as a
  power removal clears it, and the boot check completes the reset every time. A flag-only boot
  check, the pre-change design, fails 18 of those checks. `np_cfg_store_tests` adds a
  `test_reset_marker_is_durable` case covering the marker's survival across a power cycle and the
  loss of one copy. It also covers zeroed and erased Config (`NO_STORE`), a superblock read fault
  (`UNKNOWN`), and a 24-cut power-loss sweep of the marker write (0 violations: every cut leaves the
  marker absent or present). Reverting the mount distinction fails the two no-filesystem checks.
- **Not wired yet** *(superseded by the next bullet, Rev 9)*. Nothing mounts Config at bring-up until a block device exists (#340), so the
  boot check has no call site in `np_hub_control_app_main()` yet. It must precede
  `np_log_backend_init()`, the first thing that touches UHDR or SHDR. **`OI-NVRAM-17`.** The
  bootloader's `LPGPR1` branch is unchanged. It erases all three partitions, which leaves Config
  with no filesystem, so the application's boot check finishes R-8 … R-10 after it.
- **Wired (Rev 9, `OI-NVRAM-17` closed).** `np_hub_control_app_main()` calls the boot check first
  among the data steps. It precedes `np_log_backend_init()`, and also `np_session_count_load()`,
  `np_cvfs_init()` and `np_cons_init()`, which read Config or UHDR that the check may be about to
  erase. `np_factory_reset_boot_permits_data()` is the one place the rule is written: `NONE` and
  `RESUMED` permit, while `RESUME_FAILED`, `UNKNOWN` and any value outside the enum refuse. On a
  refusal, `np_log_backend_refuse()` opens neither log partition. Every append and flush then fails
  closed, and `np_log_backend_session_begin()` returns `NP_HUB_ERR_STORE_IO` without opening a file.
  `np_cvfs_init_unpersisted()` keeps the cervical fault summary in RAM for that boot. Config itself
  is still read and written (session count, consumable counts). A refusal is about UHDR and SHDR,
  and a Config write cannot remove the marker.
  - **Until #340, every boot refuses.** `np_cfg_store_mount()` on an unbound store returns
    `NP_HUB_ERR_INVALID_ARG`, which `np_reset_marker_classify()` reads as `UNKNOWN`. On target that
    replaces a trap in the unwritten log HAL with a deliberate refusal. `check-hub-bringup-order.ts`
    rule 1a requires any future `np_cfg_store_bind()` to precede the check. A bind placed after it
    would leave the device refusing forever while looking wired.
  - **Verified.** `np_factory_reset_tests` cases 19–20 cover the predicate over all four outcomes
    and an out-of-range value, and end to end over the unreadable, failed-resume and completed-resume
    paths. `np_log_backend_tests` `test_refuse_opens_nothing` shows that nothing is opened or written,
    including a whole session through the logger, and that a later init clears the refusal.
    `np_cvns_fault_summary_tests` `test_unpersisted_never_touches_uhdr` shows no load and no save,
    with the fault still framed and notified. Three mutants each fail: `session_begin` ignoring the
    refusal (4 checks), the summary saving regardless (2), and the predicate permitting everything
    (5). The gate's self-test proves each of its constraints rejects. Moving the check below the scan
    in the real file fails all six OI-NVRAM-17 constraints.
  - **No session on a refused boot** (`OI-NVRAM-19`, principal decision 2026-09-28, option b).
    `np_log_backend_refuse()` also clears `np_log_backend_admits_sessions()`, which is false from
    reset until `np_log_backend_init()` sets it. `np_runner_load()` checks it first and returns
    `NP_HUB_ERR_BOOT_REFUSED` before the lease, the descriptor or the safety MCU is touched.
    `check-hub-bringup-order.ts` rule 3 gates that order statically, and `np_runner_load_tests`
    (`NP-FW-HUB-001` Rev 28) asserts it on the host: a refused boot returns `NP_HUB_ERR_BOOT_REFUSED`
    before the lease is claimed, even with the lease already held.
    A later log fault is a different condition and keeps its existing behaviour: the session runs,
    and its log fails closed. Until #340 this refuses every protocol on every boot. That is accepted,
    because no target image can run a session before the platform layer exists either.

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

**(b) Map 3 — one record, upserted whole, two copies.** *(Rev 18; the journal discipline it replaced follows,
retired.)*

Map 3 is now one row per module (§8.5, §3.3.1.6), and a row is changed in place at each session end, never appended.
So it takes (a)'s discipline, not a journal's: the record is replaced whole, and littlefs commits the replacement at
close (D-6, L-3). Because nothing else remembers what it holds, it is not a cache, and it is kept REPLICATED: two
copies in two metadata pairs, each in an envelope with a generation and a CRC-32 over every byte, the newer valid copy
read and the other repaired from it (`np_cfg_store.h`).

1. **The whole record, at one fixed length**, written once per session end. A tear costs that one write: the copy
   being written fails its CRC and the twin, one generation older, is read.
2. **Unknown is never zero (D-7).** A record that exists and cannot be read is never overwritten, the session's
   contributions are dropped and counted, and each affected module's row is marked `detail_lost` at the next commit
   that succeeds (`REQ-FWHUB-93` (d), (g)). The download reports such a table as unknown (§6).
3. **The rows carry no recovery fields.** No per-row CRC, `seq` or watermark (§3.3.1.6 says why each is dropped).

**A torn write degrades to: the previous session end's record, intact.** The write amplification that made (a)'s
discipline wrong for a journal does not arise: the record is 12,288 B, written once per session, not 14 KB per event
(§3.3.1.6, cost). **D-35.**

> **Retired with the per-session journal (Rev 1 to Rev 17), kept for the record.** The discipline below was never
> built for a written row; `np_cfg_store`'s journal API, which had no production caller, was removed at Rev 20
> (`OI-NVRAM-21`).

*Map 3 — append-only, self-checking, fixed-size records (retired).*

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

#### 4.2.1 The blob's binding as built — added at Rev 11 (`OI-HEXMAP-01` closed)

(a) was written before a store existed. `NP-SOUP-LFS-001` Rev 4 §13 then built one and replaced
write-new-then-publish with an in-place `O_TRUNC` rewrite that littlefs commits atomically at close.
Rename is a delete, and create/delete churn is upstream #1210's trigger (§14, update of 2026-09-24).
**D-6's property is unchanged: the live blob is never destroyed before its replacement is durable.**
Only the mechanism that provides it has changed. This section records the binding that now sits on
it.

| Concern | Where it is enforced | Evidence |
|---|---|---|
| The file and its policy | `np_hexmap_nvram.c` names only `NP_CFG_FILE_NPMP` (`npmp.bin`, `NP_CFG_POLICY_REBUILD`). It calls no littlefs function (`check-lfs-caller-rules.ts` R1), and it is listed as a limit consumer, so it can never name the Map 3 journal (R6). *(Rev 20: the journal is removed and R6 retired with it; no Config file is tail-additive (R4), `OI-NVRAM-21`)* | the gate |
| Atomic replace (D-6, L-3) | `np_cfg_store_replace()` | `np_cfg_store_tests` case 10: a power cut at every op of `np_module_map_persist()`, 35 ops × 3 tear models, 105 cuts, 0 violations. The verifier requires the old blob or the new one, byte for byte, and the new one whenever `persist()` had returned OK |
| Content verified on every read (`OI-LFS-09`) | `np_module_map_blob_verify()`: magic, a socket count in 1…128, a length exactly equal to what that count serializes to, CRC-32. It is the same CRC `load()` checks, with one implementation. Version and geometry stay `load()`'s, because they mean "rebuild" for a reason other than damage | case 10: one bit flipped on the medium restores `NP_HUB_ERR_STORE_INTEGRITY` and an empty map. Falsified: with the check reduced to `true`, the damage, short-buffer and refused-write assertions fail |
| Never write what could not be read back | `np_hexmap_nvram_write()` runs the same check first | case 10: a headerless blob and a NULL blob are refused, and the live blob is unchanged |
| Every failure empties the map | `np_module_map_restore()`, unchanged | case 10: never written, damaged. Either way the next poll must re-inventory |
| **A valid blob can be old (`REQ-LFS-02`)** | **D-32**: `np_module_map`'s per-record `confirmed` flag. It is RAM only, false after `load()`, and set only by `apply_poll()` seeing the same UID or inventorying a new one | `np_module_map_tests` `test_restored_record_unused_until_poll`: all six readers refuse before the poll, a different UID is re-inventoried and neither module gets the stored calibration, and a second restore un-confirms. Falsified with the check reduced to `module_present`: 10 failures. Case 10 repeats the swap through the real store |

**What `REQ-LFS-02` asked of the boot integration, and why it is met before that integration exists.**
`NP-SOUP-LFS-001` §13.18.2 requires that a restored record be used for nothing until the power-on
poll has confirmed its socket's UID in the current boot. It says the rule binds `OI-HEXMAP-01`'s boot
integration, and that this item must cite it when it closes. A boot ordering would meet the rule only
while every caller kept to it. That is the caller-convention shape `np_module_map_restore()`'s own
contract already refuses ("a property of restore() itself, not of a caller convention"). So the
rule lives in the module. A caller that consults the map before the poll has run gets
`NP_HUB_ERR_NOT_PRESENT`, never another module's inventory or calibration. The cache still does its
job: a confirmed record costs no inventory transaction (`"Unchanged modules are never
re-inventoried"`).

**What is not built, and is not this item's.** Nothing in `firmware/` yet calls `restore()`,
`apply_poll()` or `persist()`. The poll needs the module link's `inventory_fn` (`NP-HEX-ZM-001`
`OI-HEXMAP-02`), and the store needs a block device (#340, `OI-LOG-05..07`). Until #340, the
store is unbound, `np_hexmap_nvram_read()` returns `NP_HUB_ERR_INVALID_ARG` and `restore()` leaves
the map empty. That is the first-boot path, and it is correct. §10.2's three consequences therefore
still hold **on target**. They now wait on those two items, not on this one.

*(Rev 18: for the upserted record, "declare the gap" is three behaviours. The download reports an unreadable table as
unknown, never as an empty one (`np_mu_snapshot()` returns `NP_HUB_ERR_STORE_IO` with no rows, `REQ-FWHUB-93` (i)). A
session end never writes over a record it could not read. A module whose contribution was dropped carries
`detail_lost`. The paragraph below states the principle in the journal's terms.)*

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
>
> *Rev 6: `OI-NVRAM-04` is closed. §4.3.1 says why this document never needed the number, and where
> the dimension now lives.*

A related consequence for Map 4, which *does* lose power on extraction: the same absence means the
module firmware cannot schedule its EEPROM write for the moment of removal either. `NP-MOD-ID-001`
§5.3's cadence — **one update per session end** — is therefore not merely convenient, it is the only
cadence available, and the cost is bounded and should be written down: **a session interrupted by a
tile being pulled mid-run contributes nothing to that tile's odometer.** `OI-NVRAM-06`.

*Rev 7: `OI-NVRAM-06` is closed. §6.5.1 bounds the loss, shows that `NP-MOD-ID-001` §7.4's model does
not see it (D-28), and keeps it out of §6.5's anomaly flag (D-29).*


#### 4.3.1 Why the window's duration is not this document's number — added at Rev 6 (`OI-NVRAM-04` closed)

§4.3 gave three reasons the window cannot be used, and the third was that its duration is unknown.
That made `OI-NVRAM-04` read as though dimensioning the stagger would re-open a last-gasp write. It
would not. Suppose the stagger were dimensioned and an extraction speed agreed. The window would then
have a duration, and neither record could use it:

| Record | Where the write lands | Why a window of any duration does not help |
|---|---|---|
| **Map 3 + inventory** | Hub Config partition | **No hub-side store loses power on extraction.** §4.1: `SAFE_EN[n]` removes a cluster's 24 V emitter rail, and the hub stays on USB-C. A Map 3 row is written from hub-side session state, which the hub still holds after the tile is gone. The write has no deadline to meet |
| **Map 4** | The tile's `U1` EEPROM | **The tile cannot see the warning.** `NP-HW-HEXTILE-001` §7.2 specifies `SEAT#` as *"tied to PGND on the module through 1 kΩ"*, direction module → socket. It is sensed at the socket and aggregated at the cluster controller, and nothing routes it to `U1`. To act on it, the tile would have to be commanded over I2C. But `SDA`, `SCL` and `VCC_3V3` are all in contact group 3 (§7.3), and the window **ends when group 3 breaks**. So the command path and the tile's supply are lost at the same instant, which is the end of the window. A pre-break Map 4 write would need a new module-side sense line or a hold-up supply, and that is a design change, not a missing number |

So **D-8 does not rest on the number's absence**, and CLAUDE.md §18 applies. Nothing in this document
fails if the stagger stays undimensioned, so this document states no requirement on it. **No
extraction-velocity assumption is raised either.** Nothing in the document set consumes one, and any
future pre-break consumer must bring its own requirement and derivation, not borrow a figure recorded
here.

**The stagger dimension is still required, but by a different mechanism and in a different place.**
`NP-HW-HEXTILE-001` §7.3 promises two properties. Every return mates before any supply. `SEAT#` mates
last, even under the worst-case tilt that the ±0.5 mm Z tolerance permits across two rows. Each gap
between contact groups must exceed that worst-case height differential, and all of the gaps must fit
inside the spring pin's usable working deflection. That deflection is itself unspecified
(`NP-DRV-SHELL-002` `OI-SHELL2-12`). These properties carry `RISK-SHELL-01`'s control (`SH2-DRC-10b`),
so the requirement is real, and no document tracked it. It is raised in the owning document as
**`NP-HW-HEXTILE-001` `OI-HEXTILE-28`**. It is dimensioned for mating-order margin, **not as a timing
window**.

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

*(Rev 18: Map 3 no longer has an ordering, a window or a watermark. Its row holds running totals only (§8.5,
§3.3.1.6), so D-9 now constrains nothing Map 3 writes. Its second half stands: no record this HAL writes has a clock
field.)*

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
introduces, and the honest position is that Map 3 inherits it rather than solves it. `OI-NVRAM-07`. *(Rev 8: that
paragraph overstated the loss. SHDR already holds a month-granular calendar axis per module, and the
part it lacks is sub-month spacing, which no modelled mechanism needs; §5.3.1, D-30.)*

#### 5.3.1 What the trajectory actually lacks — added at Rev 8 (`OI-NVRAM-07` closed)

`OI-NVRAM-07` recorded the paragraph above as a gap so that the `NP-MOD-ID-001` §7.4 review gate
would not discover it. Asked what fails, and where it is traceable (CLAUDE.md §18), the gap is
narrower than that paragraph says, and it is not the one §5.3 names.

**The model's axes are use, not time.** §7.4 fits
`degradation_rate ~ exposure + module_kind + (1 | socket)`. Each input is on a clock-free axis:

| Axis | What it measures | Where it comes from | Needs a clock? |
|---|---|---|---|
| Order | Which reading came first, for one module on one helmet | Session ordinal and `seq` (§5.2) | No |
| Exposure | Drive time: `emitter_on_seconds`, `thermal_seconds_over_threshold`, `throttle_events` | Hub, accumulated in session (D-28). Seconds of elapsed tick time inside a powered session, which the RT1062 counts without a wall clock | No |
| Presence | Sessions the module sat in the helmet, driven or not | The count of its per-session rows (D-24: one row per module per session); `module_session_count` | No |
| Thermal history | What cadence does to the part: heat soak across back-to-back sessions | `peak_ntc_celsius`, `thermal_seconds_over_threshold`, read by the NTC while it happens | No |

Emitter wear, window fouling and thermal cycling are all use-driven. Each is on one of those axes,
and an ordinal-indexed series in those units is not a weaker input than a clock-indexed one. It is
the right index. A clock would add only the idle time between sessions.

**Idle time is the only thing missing, and SHDR already carries it at the resolution it matters.**
Mechanisms that run while the part sits unused (EEPROM retention, `NP-MOD-ID-001` §5.4 /
`OI-MODID-03`; emitter and encapsulant shelf aging; humidity) act over months and years. The fleet
schema already holds a calendar axis for each module in one row of `module_life`:
`module_manufacture_date` and the month-truncated `last_seen_month` (`ci/shdr/shdr_fleet_schema.sql`).
The schema's own comment says the manufacture date is there to measure *calendar lifespan*. So a
module's calendar age is known fleet-side to about a month, which is within `TIME-01`'s rule and adds
nothing to it. §5.3 was right that the *helmet* has no clock. It was wrong to conclude that the
*model* has none.

**What remains unavailable is sub-month spacing: cadence.** No row says whether 30 sessions took a
week or a month. That is exactly what §8.2 and D-18 forbid, and what `NP-MOD-ID-001` §7.5.1.1 makes
the foundation of registrant-scoped consent. It is not recovered, and **no field is added to recover
it**. Nothing on the table above needs it. Cadence acts on the part through temperature, and the NTC
reads temperature directly.

**The confound the gate must not discover late.** Presence and exposure differ by socket. A module in
a rarely-driven socket builds presence and calendar age at the helmet's rate, and builds exposure
slowly. If idle aging contributes anything, that module shows more degradation *per unit exposure*,
which the §7.4 model as written attributes to the socket. A spurious socket effect is exactly the
false positive the gate's decision rule turns on. So **D-30**:

1. **The §7.4 fit carries presence and calendar age as covariates beside exposure.** Calendar age is
   `last_seen_month` minus `module_manufacture_date`, in
   months. Both come from the same `module_life` row as `module_session_count`, which is presence
   fleet-side. A socket effect that disappears when they enter is idle
   aging, not position.
2. **No new field, no finer clock and no Map 3 change.** Both covariates are already in SHDR. Map 3's
   row, version and upload rule (D-18) are untouched.
3. **The gate states one assumption it cannot test.** Cadence within a month is taken to act only
   through the measured thermal fields. A mechanism that needs sub-month spacing and leaves no thermal
   trace is outside what this fleet data can see. It would be characterised on the bench, never by
   adding a clock to SHDR.

The same holds for CLAUDE.md §5.2's Phase 2 LSTM (`docs/reference/data-architecture-detail.md`
§5.2). A sequence stepped by session ordinal, with exposure, presence and month-granular calendar
age as features, has every axis the modelled mechanisms act on. **`OI-EMMC2-13` is not closed by
this.** Its subject is derived flags whose thresholds carry no provenance, and nothing here changes
that. Rev 1 filed `OI-NVRAM-07` as an instance of it, and that link was wrong: an ordinal series is
not a thresholded flag.

---

## 6. Sync: what it means, who starts it, and what happens when the maps disagree

### 6.1 The definition

> **Rev 18 (§8.5, D-35): a sync is a snapshot read.** The control software reads the whole Map 3 table, every row as
> it stands (`np_mu_snapshot()`). Nothing on the helmet changes because it was read: there is no watermark, no
> acknowledgement and no row retired. The app keeps the snapshot it read last time, computes each row's difference
> from it, stamps the difference with its own time, and keeps both in Map 2 (§8.3, D-19). A table the helmet could not
> read is reported as unknown, never as a table with no rows (D-7).
>
> What follows from it, in place of properties 2 and 3 below:
>
> - **The app's previous snapshot is kept per helmet and per row.** A module's row counts only what this helmet saw
>   of it, starting at zero, and another helmet keeps its own row for the same UID. An accessory's row counts its port,
>   not the part. So a difference is taken against the same helmet's previous value for the same row key, and never
>   across helmets.
> - **A row that went down, or that disappeared, is not negative use.** A row disappears when a full table gave it up
>   (§6.3); the row it gets if the module comes back starts at zero with `detail_lost` set. The app takes the new value
>   as the difference when `detail_lost` is newly set or the value fell, and never subtracts.
> - **Properties 1 and 4 stand unchanged.** The control software initiates every read (D-10), and a read is not a
>   consent event and grants no upload (§8).
>
> **Not built:** the wire command that carries the snapshot to the app, and Map 2 itself. Nothing in this revision
> builds either; they must keep to the above.

*The journal's definition, Rev 1 to Rev 17, retired with it:*

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

*(Rev 18: nothing. The rows are running totals and stay as they are, so a control software instance that lost its own
store reads the same totals again. The paragraph below, and D-11, applied to the journal.)*

They stop being the authoritative copy and become reclaimable. They are **not** required to disappear
immediately, and there is a reason not to rush: a control software instance that loses its own store
(app reinstall, phone replacement) can re-request from a lower watermark for as long as the rows are
still there. The helmet reclaims lazily, when space is needed, oldest-acknowledged first. **D-11.**

### 6.3 The journal-full policy — the decision that is easy to get wrong

> **Rev 18 — what it becomes when rows are per module (D-37).** The table fills with **modules, not sessions**. It
> holds 147 rows, one for every socket address and hub slot, so every module that can be fitted at once has a row, and
> no number of sessions fills it. It fills only once a helmet has seen more distinct modules than that (tiles swapped in
> from a pool, for example). Then:
>
> | | Behaviour | Why |
> |---|---|---|
> | **A newcomer takes a row** | The row of the module with the fewest sessions present and nothing pending this session (UC-208) | A session is never refused and a fitted module is never left uncounted, for the reasons the table below gives against *Stop* |
> | **The loss is visible** | The given-up module's row is gone from the next snapshot. The newcomer's row starts with `detail_lost` set, and so does the row a given-up module gets if it returns | The helmet has forgotten a module, so it cannot say any new row's totals start there for the first time. Marking every row made in a full table is how the app learns that a returning module restarted at zero (§6.1) |
> | **A dropped commit is visible too** | When the record cannot be read at session end, the session's contributions are dropped, never written over it, and each affected module's row gets `detail_lost` at the next commit that succeeds | Unknown is not zero (D-7) |
>
> **What is lost, and what is not.** A given-up module loses the growth since the app's last snapshot of this helmet;
> what the app already read stays in Map 2 and in the fleet's last upload. Map 4 on the module is untouched, so its
> odometer never goes backwards. The journal's *detail* no longer exists to drop, so "keep totals, drop detail" becomes
> "keep every fitted module's totals, give up the least-used absent one, and say so". **The file bound** is the fixed
> record of §3.3.1.6: 12,288 B of payload, 12,304 B per copy, two copies, each under `file_max`. It does not grow with
> sessions, so there is no sessions-between-sync assumption to state.

*The journal's policy, Rev 1 to Rev 17, retired with it; its reasoning against Wrap and Stop still
decides the rule above:*

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

*(Rev 18: the row holds only what this helmet observed and starts at zero; it stores no baseline. A carried-in
baseline is Map 4's, read from the module, and the app holds it beside the row in Map 2. Reading Map 4 from a tile is
not built: it needs the module link, `OI-HEXMAP-02`.)*

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
| **Map 4 < Map 2** | The part reports *less* than this fleet already recorded | **Anomaly. Keep Map 2's value; never write the lower value back to the module; flag it.** Three causes are plausible — a replaced or reflashed `U1`, a slot recovered to an older generation after a power-loss during write, or a record transplanted between modules (which `NP-MOD-ID-001` §5.2's CRC-over-UID should already have caught). All three are device-condition events worth knowing about and none of them licenses a rollback *(Rev 7: a fourth, benign cause is a session interrupted by extraction. The comparison excludes it, so it is not flagged, §6.5.1, D-29.)* |

> **The magnitude of a divergence must never be uploaded, only its existence.** `NP-MOD-ID-001` §6.3
> establishes that an exact counter value is a join key across fleets — *"if fleet A's last reported
> partial is 4,317 sessions and a new ref in fleet B appears with `carried_in_session_count = 4,317`,
> those two rows can be joined on the value itself."* A divergence of 4,317 is exactly as joinable as
> a baseline of 4,317, and the mitigation transfers unchanged: coarsen, or upload a boolean.
> **D-14.** This is `OI-MODID-02`'s bucket-width question acquiring a second consumer, and the widths
> must be chosen once for both.


#### 6.5.1 The session an extraction interrupts — added at Rev 7 (`OI-NVRAM-06` closed)

**What is lost, and what bounds it.** `U1` writes Map 4 once, at session end (`NP-MOD-ID-001` §5.3).
Until then it holds the session's accumulation in RAM. A tile pulled mid-session loses `VCC_3V3`
before that write, and §4.3.1 shows that no warning can move the write earlier. So for that session
Map 4 misses the `session_count` increment and the session's `emitter_on_seconds`,
`thermal_seconds_over_threshold` and `throttle_events`. It misses `peak_ntc_celsius_ever` only if the
session set a new lifetime peak. `pd_ratio_last` stays one session stale. Nothing earlier is at risk,
because no write is in progress and the four-slot rotation holds the last good record. **Each
extraction costs at most one session's contribution.** The number of such losses cannot exceed the
number of extractions, which is the module's mate-cycle count.

So Map 4 is a **lower bound** on the part's use. It is never above the truth and never goes
backwards. It falls short by exactly the contributions of interrupted sessions. §6.5 reconciles by `max`, so
wherever Map 2 holds the higher count, that is the count kept.

**The hub does not lose the session.** It keeps its session state after the tile is gone (§4.3.1), so
the interrupted session's Map 3 row is written from hub state like any other.

**Does it matter to `NP-MOD-ID-001` §7.4's model?** The model is
`degradation_rate ~ exposure + module_kind + (1 | socket)`, fitted on §7.2's per-socket, per-session
rows. Taking each input in turn:

| Input | Where it comes from | Does the Map 4 loss reach it? |
|---|---|---|
| `exposure` (§7.2's `emitter_on_seconds` and the thermal fields, per socket per session) | **The hub.** Map 4 has no socket field (`NP-MOD-ID-001` §5.2), so a per-socket figure can only be the hub's. Attributing a Map 4 delta to a socket would need a read before and after the session in the same socket, and an extraction makes the second read impossible | **No, provided D-28 holds.** The interrupted session is in the data with the exposure it actually had |
| `socket` | The hub's inventory | No |
| `module_kind` | `major`, in the inventory blob | No |
| `degradation_rate` (the response) | PD metering the hub reads during sessions, and `pd_ratio_last` | At most one session's lag on a module whose last session was interrupted. The next completed session replaces it |
| Carried-in baseline, for a module that arrived from another fleet (§6.4) | **Map 4** | **Yes, and it is the only path.** MODID-6 rounds the baseline down to the nearest 250 sessions before it reaches SHDR (`NP-MOD-ID-001` §6.3), which already accepts up to 249 sessions of downward error. An undercount of *k* interrupted sessions is also downward and adds at most *k*. It is negligible while *k* ≪ 250, and *k* is bounded by the mate cycles the part saw elsewhere |

**So the loss does not matter to the model, on one condition, and D-28 makes it a rule.** The
condition is that exposure comes from the hub. **What fails without it** (CLAUDE.md §18): if a
per-session value were read from `U1` once at session end, or computed as a difference of two Map 4
reads, the interrupted sessions would drop out of the one covariate the model conditions on. Nothing
makes that loss random. A tile that runs hot or uncomfortable is a plausible reason to pull it
mid-session, so the missing rows could lean toward the thermally loaded tail the model exists to
attribute. That is a plausible mechanism, not a measured one, and D-28 removes the need to know. D-28
sets no polling interval. With in-session accumulation, a `U1`-local quantity is lost only for the
time since the hub's last read, not for the session.

**The consequence that does matter is §6.5's anomaly row.** Each interrupted session leaves Map 2 one
session ahead of Map 4, and that is the *Map 4 < Map 2* case. §6.5 calls it an anomaly and lists three
causes, all of them faults. Left alone, ordinary handling would raise the divergence boolean that §8.1
sends to SHDR, and the real causes (a reflashed `U1`, a slot rolled back after a torn write) would be
lost among the false ones. **D-29:**

1. **The hub marks the row.** When the hub closes a module's Map 3 row and the module no longer
   answers on its socket, it sets `fault_flags` bit 0, `NP_MAP3_FAULT_UNSEATED_AT_CLOSE`
   (`np_map3_record.h`). The field already exists at version 1, so the row does not change and no
   version is raised.
2. **The comparison excludes flagged rows.** Map 4 is expected to be at least the carried-in baseline
   plus the contributions of **unflagged** rows. A session is counted once per ordinal (§5.2), and it
   counts toward that expectation only if the uid's last row for that ordinal is unflagged. A tile
   pulled and re-seated in the same session was seated at session end, so `U1` wrote that session.
3. **Only a shortfall below that expectation is an anomaly**, handled exactly as §6.5 says. A value
   between the expectation and Map 2's full total is the interrupted-session shortfall, and it is not
   flagged.
4. **The difference is never written into Map 4.** Map 4 records what the part observed (§6.5). A
   helmet adding sessions to it would make the carried-in baseline one fleet's claim instead of the
   part's.

*(Rev 18: with running totals there is no row per ordinal to leave out. `fault_flags` bit 0 is sticky
(`NP_MU_FAULT_UNSEATED_AT_CLOSE`): set, it says the module closed at least one session unseated in this helmet. Each
interrupted session needs an extraction, and the module was mated again before Map 4 could be read, so the shortfall it
explains is at most the module's mate cycles in this helmet. The comparison becomes: Map 4 is expected to reach at least
the carried-in baseline plus sessions present, less the row's mate cycles when bit 0 is set; only a shortfall below
that is an anomaly. That is coarser than one exclusion per session and never flags the benign shortfall the rule exists
for, but it reads a mate the hub did not see as a real shortfall. Nothing sets bit 0 or counts a tile's mate yet: both
need the inventory poll, `OI-HEXMAP-02`.)*

**The flag stays off SHDR.** A count of mid-session extractions records how the wearer handles the
helmet, which is the same class of object as the placement-event rate `NP-FW-EMMC-002` `OI-EMMC2-08`
warns about. It lives in Map 3 and in Map 2, which is app-side and UHDR-class (D-19), and it is never
uploaded. What reaches SHDR is §8.1's divergence boolean, now computed against the corrected
expectation.

**A gap this analysis found and does not close.** `NP-MOD-ID-001` §5.2 carries
`mate_cycles_observed`, and nothing says when it is incremented. Under §5.3's one-write-per-session-end
cadence, a module seated and removed without a completed session never records that mate. That is
the insert-and-remove handling the ≥500-cycle contact rating is about. Mate cycles are not an input to
§7.4's model, so they are outside `OI-NVRAM-06`'s question. The count belongs to the odometer's own
design, which §1 leaves out of this document's scope. It is raised as **`OI-NVRAM-18`**.

*Rev 10: `OI-NVRAM-18` is closed in the owning document, `NP-MOD-ID-001` §5.3.1. The hub counts a mate
from its previous observation of the socket, and `U1` writes the increment at once, at most once per
power-up (D-31). A tile pulled mid-session has therefore already recorded the mate that seated it.*
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

> **Rev 18 — the rule for the upserted record (D-36).** Map 3 is now one record rewritten at each session end
> (§3.3.1.6), not a journal appended to, so rule 2 is restated for a record. The opposition to reject-and-rebuild is
> unchanged: the totals are history, and a firmware update or rollback that made them unreadable would drop every
> session after it (never overwrite them, `REQ-FWHUB-93` (d), but never count them either).
>
> 1. **The record has one length for ever, 12,288 B**, whatever the row length, because the store reads a replicated
>    record at a fixed length and a record of another length is unreadable.
> 2. **Bytes 0 to 63 of a row are fixed for ever**, the reserved byte 63 included. A later version appends fields after
>    byte 63 and raises the header's version and row length; it never moves, resizes or reuses a version-1 field.
> 3. **The reader accepts any version ≥ 1 and any row length from 64 to 83 B** (the most that fits, §3.3.1.6). It steps
>    from row to row by the header's row length, decodes the version-1 fields at their offsets and ignores the rest.
>    Version 0 is never written and is refused.
> 4. **The writer carries what it does not know.** It rewrites the version-1 fields of each row in place, leaves every
>    byte after them as it read them, and keeps the header's version and row length. A row it creates starts those bytes
>    at zero. So after a rollback, a newer firmware's fields are not lost: they stop growing while the older firmware
>    runs (a lower bound, like Map 4), and they are zero in a row the older firmware created.
> 5. **Not tail-additive, so it needs a converter that runs before the new firmware's first write:** a different record
>    length, a row longer than 83 B, or more rows than the older firmware's 147. An older firmware refuses such a record
>    whole and never writes over it.
>
> No helmet has written a record, so version 1 is the first layout and nothing is migrated. `np_module_usage_tests`
> is the check (§7.2.1).

#### 7.2.1 The check, as written — added at Rev 3 (`OI-NVRAM-08` closed)

> **Rev 18: the check below was for the retired journal, and it is removed with the journal's codec.** Its rule now
> binds the upserted record (§7.2, D-36), and `np_module_usage_tests` checks it against the production reader and
> writer (`REQ-FWHUB-93` (h)): a version-2 record with 76-byte rows and a tail after byte 63 is loaded with every
> version-1 field intact, the row length steps to the next row, a session's rewrite keeps the version, the row length
> and every tail byte, a row the rewrite creates has a zero tail, and the totals run on. It refuses version 0, a row
> shorter than 64 B or longer than 83 B, more rows than 147 and an unknown key kind, and a record of another length is
> unreadable, never empty. **Falsified by hand (2026-11-25):** with the reader refusing any version but 1 (the
> reject-and-rebuild policy, RISK-NVRAM-01's cause) the check fails 7 assertions; with the reader taking only a 64-byte
> row (fixed stride) it fails 7; with the writer zeroing every row before writing it (dropping a newer firmware's fields)
> it fails 1. Each mutant was reverted and the check passes. The store's own variable-length journal reader kept its
> check (`np_cfg_store_tests` case 9, on a fixture row of its own) until Rev 20, when the reader, the journal and the
> check were removed together (`OI-NVRAM-21`). The update below is kept as the record.

`firmware/hub_control/tests/np_map3_record_tests.c` (ctest `np_map3_record_tests`, Class B) checks
the production reader, `np_map3_scan()`. **Version tolerance is a property of the scan, not a
per-version branch.** The scan steps by each row's own `len`, accepts any `ver ≥ 1`, decodes the
version-1 fields at their fixed offsets and skips a newer row's tail (D-25). So the reader that
firmware *n+1* will run is this code, and the check can bind it before *n+1* exists.
`check_no_record_lost()` requires every row back, in order, with every version-1 field intact, in
five scenarios:

| | Scenario | What it models |
|---|---|---|
| S1 | 40 rows written by the real writer (version 1) | The journal as written |
| S2 | S1 plus 20 version-2 rows (+8 B) and 5 version-3 rows (+16 B) | A later firmware appending to that history |
| S3 | S2, read by a reader that knows only version 1 | Boot-bank rollback after an update |
| S4 | S2 with a partial last row | A torn append costs that row and no earlier one (L-4) |
| S5 | 768 rows of 64 bytes, exactly 49,152 bytes | D-24's bound at D-34's row size, all rows back |

The future rows are built only as §7.2 allows. The real encoder writes the row, the tail goes
between the v1 fields and the CRC, and the header and CRC are rewritten. The check is **falsified in
both directions**. It passes the production reader, and it **fails** each of four readers that break
the rule in a way someone could plausibly write: **M1** reject-and-rebuild (the blob's policy,
RISK-NVRAM-01's cause), **M2** fixed stride, **M3** exact length and **M4** version ceiling. Each
mutant is also shown to read a journal of current rows correctly, so a failure is proven to come from
versioning and not from a broken mutant. By hand, the production scan was patched to refuse
`ver > NP_MAP3_VERSION`, and the check failed.

**What it does not cover.** The check is on the codec. It does not go through `np_cfg_store`, whose
`np_cfg_store_journal_read()` takes a fixed `rec_len` and so cannot read a journal whose rows differ
in length. At version 1 every row is 64 B and nothing is lost. **Before any version grows the row,
the store needs a variable-length journal read, or the file needs a converter per rule 2.**
`OI-NVRAM-16`.

> **Update 2026-09-27 (Rev 5) — `OI-NVRAM-16` closed.** The store now has the variable-length read.
> `np_cfg_store_journal_read_rows(file, buf, cap, &count, &bytes, step, ctx)` calls `step` at each
> record with the bytes left to the end of the journal. It advances by the length `step` returns and
> ends the valid prefix on 0 or on a length past the end, so the store never reads past what it read.
> Map 3's step is `np_map3_row_len()`, which applies `np_map3_scan()`'s own row test. The fixed-length
> `np_cfg_store_journal_read()` stays for journals whose records are all one length. **Map 3 must not
> use it**, because it is exactly the reader this paragraph describes as losing rows.
>
> `np_cfg_store_tests` case 9 is the check. It appends four 64-byte rows, one grown row (the 64-byte row plus an 8-byte tail, as version 2) and five
> more 64-byte rows **through the store**, then reads all ten back, in order, with the prefix ending at
> the journal's last byte. It is falsified in both directions:
>
> - **The fixture is required to break the old reader.** The same file read through the fixed-length
>   reader must return exactly four rows. If it ever returned ten, the fixture would hold no grown row
>   and the pass would prove nothing.
> - **The new reader is required to fail when broken.** By hand, clamping the step's `left` to 64
>   bytes (a reader that cannot see past one row) fails five of the case's assertions.
>
> The case also shows that a step of 0 or of more than `left` ends the prefix, that a torn half-row
> costs only itself (L-4), and that the reader refuses a rebuild-cache file and a missing check.
> `check-lfs-caller-rules.ts` R5 matches both reader names, and its self-test gains a case for the
> new one (31 cases). **Not covered:** a converter per rule 3, which nothing needs while every change
> is tail-additive, and Map 3's owner, which is not written.

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
| `session_count` per module | Map 3 / Map 4 | **SHDR — as a bare unsigned count only** | Directly on precedent: *"device session count (unsigned integer) → SHDR; session timestamps → UHDR."* The qualifier is the whole rule (§8.2). *(Rev 15:* in Map 3's per-session row it is not a bare count but one step per session beside `ordinal`, so it goes with the `ordinal` row below until `OI-PRIV-38` (NP-PRIV-ANALYSIS-004) part 1 is ruled.) *(Rev 16, ruled: the count of sessions the module was present for, kept as a running total per module, is SHDR. Presence is not use; exposure to a session is wear (§8.5).)* |
| `mate_cycles_observed` | Map 4, mirrored in Map 3 *(Rev 18: Map 3's `mate_cycles` is this helmet's own count, §3.3.1.6)* | **SHDR as a total; per-event record is not written at all** | An aggregate insertion count is wear data on a contact rated ≥500 cycles. A per-event *record* is a log of when the wearer reconfigured their helmet — which areas they changed treating — and it is not needed for anything (§8.4) |
| `emitter_on_seconds`, `thermal_seconds_over_threshold`, `throttle_events`, `peak_ntc` | Map 4 | **SHDR only under `NP-MOD-ID-001` §7's opt-in**, per-socket, cohort-scoped | Already decided there and not re-opened. Per-socket optical duty at 80-socket resolution is treatment geography; `DOSE-01` is narrowed, not removed |
| `pd_ratio_last` | Map 4 | **SHDR** | The fouling-vs-aging discriminator; a property of the window and the emitter |
| `ordinal` *(Rev 15; retired Rev 16)* | Map 3 row, Config | **Not stored. The per-session row it keyed is retired (§8.5)** | *Rev 15 reasoning, kept:* A row per module per session keyed by `(uid, ordinal)` says which module was worn in which session. That is the content of the session fitting record, which is UHDR (`data-architecture-detail.md` §5.1, `NP_LOG_TAG_UHDR_FITTING`), and the per-socket montage the F1 ruling puts in UHDR. §8.2 found the hazard at upload; §3.2 makes the write itself the disclosure, because Config's key is NeurOne-derivable. `OI-PRIV-38` (NP-PRIV-ANALYSIS-004) part 1 |
| `dose_accum` *(Rev 15; ruled Rev 16; ruled again Rev 21)* | Map 3 row, Config (as built) | **UHDR class: the person's dose, kept per wavelength for their app (Map 2) only. Never reaches the fleet, opted in or not** (the principal, 2026-11-26, §8.5, D-38; `NP-MOD-ID-001` §7.2, `DOSE-01`). By D-3 it may not be written to Config, so the built sum moves to the UHDR side as one total per wavelength (`OI-NVRAM-23`). *Rev 16 to Rev 19 classed it SHDR as a running total, reaching the fleet under `NP-MOD-ID-001` §7's opt-in* | *Rev 15 reasoning, which still decides the per-session form:* Dose on one module in one session window is dose per socket per session, ruled UHDR (F2, `data-architecture-detail.md` §5.1). A running total does not escape it: two consecutive rows give the session's dose by subtraction. *Rev 21:* a total per wavelength can be clinically useful to the individual user, which is why the principal kept it per wavelength and for the person. `OI-PRIV-38` (NP-PRIV-ANALYSIS-004) part 1; `OI-NVRAM-22` (c) |
| `throttle_count` *(Rev 15; ruled Rev 16)* | Map 3 row, Config | **SHDR class as a running total per module, never per session or per window. Reaches the fleet only under `NP-MOD-ID-001` §7's opt-in** (§8.5) | *Rev 15 reasoning, which still decides the per-session form:* Which socket throttled in a session is ruled UHDR (F8); a per-module, per-session count is that ruling with a number attached. `OI-PRIV-38` (NP-PRIV-ANALYSIS-004) part 1 |
| On-time per emitter channel *(specified Rev 21, not built, `OI-NVRAM-23`)* | Map 3 row, Config | **SHDR as a running total per module per channel, never per session or per window. Reaches the fleet only from helmets opted in to `NP-MOD-ID-001` §7's characterisation, within its window, until the §7.4 review gate sets what general SHDR consent covers; afterwards under general SHDR consent if the gate puts it there and the owner agrees. Never a user's consent** (the principal, 2026-11-26, D-39) | How much each emitter is driven is wear on the emitter (the principal, 2026-11-26). Per emitter means per drive channel, for tiles and accessory ports alike (the principal, 2026-11-26, `OI-NVRAM-25` (a)). The class of `emitter_on_seconds` (`NP-MOD-ID-001` §7.2), in Map 3's narrower shape |
| Sessions per emitter channel *(specified Rev 21, not built, `OI-NVRAM-23`)* | Map 3 row, Config | **As the row above** | A bare count of the sessions in which the channel was switched on and off: on/off switching is wear (the principal, 2026-11-26). Not a count of pulses (the principal, 2026-11-26) |
| Time in 5 °F temperature bands *(specified Rev 21, not built, `OI-NVRAM-24`)* | Config, one record per helmet, outside the Map 3 row (§8.5.1, Rev {NP-FW-NVRAM-001 to}); the sensor is open | **SHDR as running totals (time per band), reaching the fleet only from helmets opted in to `NP-MOD-ID-001` §7's characterisation, within its window, until the §7.4 review gate sets what general SHDR consent covers** (the principal, 2026-11-26, D-39) | The working environment is wear (the principal). 5 °F is the principal's choice for detail during an initial period, to be revisited once a first dataset is analysed (UC-215). **Residual:** a tile's NTC sits against the scalp and its per-socket reading in a session is UHDR (F7), so a band total from tile NTCs needs `CLAUDE.md` §5.1 rule 1's positive demonstration, or a source off the scalp (`OI-NVRAM-24`). Humidity: no sensor; cost under investigation ([#1086](https://github.com/stevehickman/NeuroPulse/issues/1086)) |
| `seq`, `synced_upto`, `retired_upto` *(retired Rev 18)* | Map 3 store | **Not stored: the record has no journal, no sync acknowledgement and no watermark (§6.1, §3.3.1.6)** | §8.2, which they were classified under, still forbids any of them reaching SHDR |
| `detail_lost` flag (§6.3) | Map 3 row | **SHDR** | A storage-condition boolean with no magnitude. *(Rev 18: per row and sticky; set when the row took another module's place or a contribution to it was dropped, §6.3)* |
| Map 2 → Map 4 divergence | Reconciliation (§6.5) | **SHDR as a boolean only** | Magnitude is a join key, `NP-MOD-ID-001` §6.3. *(Rev 7: computed against the expectation that excludes interrupted sessions, §6.5.1, D-29)* |
| `NP_MAP3_FAULT_UNSEATED_AT_CLOSE` *(Rev 7; `NP_MU_FAULT_UNSEATED_AT_CLOSE` since Rev 18, sticky)* | Map 3 `fault_flags` bit 0, mirrored in Map 2 | **Device- and app-internal. Never uploaded in any form** | A count of mid-session extractions records how the wearer handles the helmet (compare `NP-FW-EMMC-002` `OI-EMMC2-08`). It is used only to compute the divergence boolean above (§6.5.1, D-29) |
| Anything else | — | **Not stored here at all** | §3.2's custody rule: Config's key is NeurOne-derivable |

> **Rev 15 (2026-11-21, `OI-PRIV-38` (NP-PRIV-ANALYSIS-004) part 1) — the Map 3 row as laid out in
> §3.3.1.5 carries three UHDR-class fields, so its writer may not be built against that layout.** Rev 1 to Rev 14
> classified `seq`, the watermarks, `session_count` and `fault_flags` and never `ordinal`, `dose_accum` or
> `throttle_count`. The rows above classify them by `CLAUDE.md` §5.1's tests and by the rulings they repeat; that
> direction needs no ruling. What Map 3 keeps in their place does: dropping them from the row, moving the journal to
> the UHDR partition, or reshaping it into a per-module latest-state record each changes what Map 3 can do for
> `NP-MOD-ID-001` §6.5 reconciliation and history portability. The options and their costs are in
> `docs/human-input/oi-priv-38-map3-journal-and-insertion-records.md`, for the principal. Nothing writes a Map 3 row
> today (`np_map3_record.c` is the codec; its owner is not written), so nothing in the field is affected.

> **Rev 16 (2026-11-23) — the principal's ruling, recorded in §8.5.** Map 3 keeps running totals
> per module, not rows per session.

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

#### 8.3.1 The check, as written — added at Rev 3 (`OI-NVRAM-09` closed)

`scripts/check-map2-shdr-boundary.ts`, run by the `tooling-ci.yml` job `map2-shdr-boundary`, follows
`warranty-nojoin-ci.yml`'s pattern: a self-test that must go red runs before the gate. It has two
clauses:

- **A (D-19).** No non-test source file under `app/` or `firmware/` names both Map 2 and SHDR. The
  scan strips comments and strings, but it keeps `#include`, `import` and `require` targets and the
  file's own path. So a Map 2 reader that calls an uploader fails, and so does an uploader that
  imports Map 2.
- **B (D-18).** No column of `ci/shdr/shdr_fleet_schema.sql`, which is the upload's field list, and
  no identifier in a firmware SHDR source names a sync, a watermark, `synced_upto` or
  `retired_upto`, a window's ends, the Map 3 ordinal or journal, or a bare delta. A rate
  (`*_delta_per_cycle`) passes. **TIME-01 cannot catch any of these**, because an ordinal is an
  integer. The app's SHDR code is deliberately not in B. `SHDRUploader.lastUploadedAt` is the phone
  dating its own uploads, which §8.2 says it can always do. The rule is that such a date does not
  travel.

**D-26, the naming rule clause A depends on.** Map 2's code carries `Map2` (or `map2`, `map_2`) as a
word in the name of every file, type and module that holds it. A Map 2 under another name is
invisible to clause A, so the rule is a decision, not a convention. Map 2 does not exist in code
yet, which makes clause A preventive. Its vacuity guard asserts that the SHDR writer population is
being seen.

**Reach.** Clause A is per file, so a value that crosses through a helper file that names neither
map passes it. Clause B is a name check, so a delta uploaded under an innocuous column name passes
it. Neither clause alone closes D-19. Together they make crossing it an edit a reviewer sees
flagged. **Falsified in both directions.** A hermetic self-test covers legitimate shapes (a Map 2
store alone, the independence test, the app's own upload date, firmware SHDR's §G/§H windows, a
per-session rate) and 20 seeded violations. By hand against the real tree, a schema column
`sessions_since_sync`, a Kotlin `import …ModuleMap2Store` in `ShdrUploader.kt` and a
`np_shdr_synced_upto` in `np_accel_shdr.h` each turned the gate red.

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

### 8.5 The ruling on Map 3's content — added at Rev 16 (the principal, 2026-11-23, `OI-PRIV-38` (NP-PRIV-ANALYSIS-004))

The question was `docs/human-input/oi-priv-38-map3-journal-and-insertion-records.md` Q1. The principal's reasoning,
in order:

1. **Presence is not use.** A module seated in a helmet was not necessarily driven by the protocol; in a clinic a
   helmet is fitted for the day. Presence does expose the module to the session environment, which is wear, so a
   count of sessions present is SHDR.
2. **Use does not need a session.** Dose and throttling measure use, but they can be accumulated with no link to any
   session: lifetime totals, or totals over a period.
3. **No clock is needed.** The helmet accumulates between downloads. The app, which has a clock, keeps the previous
   values and learns what happened between downloads, with whatever timestamps it has.

**The design that follows, and the reason for each limit:**

| Where | What | Why it stops there |
|---|---|---|
| Helmet, Config | One row per module, upserted, not appended: UID, sessions present, `dose_accum`, `throttle_count`, mate cycles, `fault_flags`, `detail_lost`. Running totals only. No per-session row, no `ordinal`, no per-window value | A download window lasts as long as the person waits between downloads, so at home it can be one session. A per-window value is then a per-session value (§8.2, D-18). Running totals carry nothing finer than their own growth |
| App, Map 2 | The previous snapshot; the difference at each download, stamped with the app's own time | Map 2 is already the person's (§8.3, D-19) |
| Fleet, SHDR | The running totals, under the upload's month anchor. Sessions present and mate cycles always; `dose_accum` and `throttle_count` **only under `NP-MOD-ID-001` §7's opt-in**, as Map 4's `emitter_on_seconds` and `throttle_events` already are *(Rev 21: `dose_accum` never, the principal, 2026-11-26; see the amendment below)* | A module usually stays in one socket, so per-module dose is treatment geography (`DOSE-01`). The principal kept the opt-in. Rates come from consecutive uploads at month granularity, which `TIME-01` already allows |

§6's sync and watermark machinery, §6.3's journal-full policy and §7.2's row-version rule were written for the
per-session journal and are revised with the new row. That work, and the codec, are **`OI-NVRAM-20`**.

**Amended by the principal, 2026-11-24 (Rev 17).** The totals are per module and measure **use as well
as presence**: a module can be fitted and not driven, and its warranty lifetime depends on how much it is used. Each
module's row therefore holds sessions present and the **time present** (exposure to hair, skin oil and sweat), sessions
**used** and the **time used** (each use is an on/off and a ramp up and down; sessions differ in length), and a count
per fault code. All of these are SHDR as running totals, for every modality, on the same terms as the table above: no
per-session row, no session number, no time stamp, nothing in the SHDR log, and the fleet sees them only in the
upload's snapshot under its month anchor. `dose_accum` and `throttle_count` keep the opt-in.

*Built as:* `NP_CFG_FILE_MODULE_USAGE`, one REPLICATED fixed-size record upserted at each session end
(`NP-FW-HUB-001` §6.11, `REQ-FWHUB-93`), not as rows in `map3.jrn`: a tail-additive journal can only append, and an
appended row per session is the per-session history this section rules out. *(Rev 18, `OI-NVRAM-20` closed:* the row
is complete, §3.3.1.6; §4.2(b), §6, §6.3 and §7.2 are revised for it; `np_map3_record` is removed, and `map3.jrn`,
never written, waits on `OI-NVRAM-21`.)* *(Rev 20: `map3.jrn` and the store's journal API are removed, `OI-NVRAM-21`.)*

**Who sees which field (Rev 18).** The helmet keeps every field below; nothing in this table adds a field to any
upload, and the hub side of the fleet upload is not built (`NP-FW-HUB-001` §2.5).

| Field | Helmet (Config) | App (Map 2, the person's) | Fleet (SHDR, month anchor) |
|---|---|---|---|
| Key (UID, or port) | Yes | Yes | A tile as `module_ref = HMAC(fleet_key, uid)`, never the raw UID (MODID-1); a port as its slot and module type |
| Sessions present, time present, sessions used, time used, faults per code | Running totals | Snapshot, and its differences stamped with the app's time | The totals (the 2026-11-24 amendment) |
| Mate cycles | Running total | As above | The total (the table above) |
| `dose_accum` | One sum, as built; to become one total per wavelength on the UHDR side, off Config (D-3, `OI-NVRAM-23`) | As above; per wavelength once built | **Never** (the principal, 2026-11-26; Rev 18 to Rev 19 said "only under the opt-in") |
| `throttle_count` | Running total | As above | The total, **only from helmets opted in to `NP-MOD-ID-001` §7's characterisation, within its window, until the §7.4 review gate sets what general SHDR consent covers** (D-39; Rev 16 to Rev 19 said only under the opt-in, which stands; an "either consent" reading the same day was replaced) |
| On-time and sessions per emitter channel, tiles and accessory ports; time in 5 °F bands *(Rev 21, specified, not built)* | Running totals (the bands: one record per helmet, specified in §8.5.1, not built) | As above | The totals, **only from helmets opted in to `NP-MOD-ID-001` §7's characterisation, within its window, until the §7.4 review gate sets what general SHDR consent covers**; never a user's consent (the principal, 2026-11-26, D-39) |
| `detail_lost` | Sticky boolean | As above | The boolean (§8.1) |
| `fault_flags` bit 0 | Sticky bit | Yes, for reconciliation (§6.5.1) | **Never** (§8.1, D-29) |
| A difference between two snapshots | Never computed | Yes | **Never** (D-18) |

> **Rev 19 — the fleet column for `dose_accum` is in question (`OI-NVRAM-22` (c)).** `NP-MOD-ID-001`
> §7.2 narrows `DOSE-01` for the opted-in cohort to drive time and throttle counts, and keeps delivered dose in J/cm² out
> of SHDR even for opted-in devices (§8.2 there: `DOSE-01` "continues to reject J/cm² and irradiance"; the schema gate,
> `ci/test_shdr_schema.py`, rejects any column named for dose). The table above sends `dose_accum` to the fleet under
> that opt-in. The two disagree, and the §7.4 model the opt-in feeds uses drive time, not dose, so it cannot say whether
> it needs dose per wavelength. Nothing is uploaded today, so nothing has moved. The question is put to the principal in
> `docs/human-input/oi-nvram-22-dose-accum-wavelengths.md`; until it is answered no fleet path for `dose_accum` is built.
> `throttle_count` is not in question (`throttle_event_count` is on §7.2's list).
>
> **Resolved Rev 21 (the principal, 2026-11-26): `dose_accum` never reaches the fleet.** The table above is
> corrected. `NP-MOD-ID-001` §7.2 and the `DOSE-01` gate stand unchanged.

**Amended by the principal, 2026-11-26 (Rev 21, `OI-NVRAM-22` (c)).** The question was
`docs/human-input/oi-nvram-22-dose-accum-wavelengths.md` Q1 and Q2. The principal's answer, in two messages, is in
that file's §7, verbatim. The rulings, as recorded:

1. **Delivered dose does not matter for SHDR.** It never reaches the fleet, opted in or not (Q1, option A).
2. **Dose is kept per wavelength**, because that can be clinically useful to the individual user (Q2, option B). It is
   the person's, for their app (Map 2) only. Config's key is NeurOne-derivable (D-3), so the per-wavelength totals live
   on the UHDR side, and the built `dose_accum` sum leaves the Config row when they are built (`OI-NVRAM-23`).
3. **SHDR says how much each emitter is used, as totals per emitter**, and **how many sessions each emitter was
   switched on and off in**, because on/off switching is wear. The recorded design adds both to each tile's row,
   **per emitter channel**, not per tile and not summed across wavelengths. The hub drives a tile per channel and never
   per site (§3.3.1.6), so per channel is the finest per emitter can mean, and the principal confirmed that reading
   the same day, with accessory ports counted the same way (`OI-NVRAM-25` (a)).
4. **Consent for SHDR data is the warranty owner's, never an individual user's** (`CLAUDE.md` §6.0). `NP-MOD-ID-001`
   §7's opt-in is already the warranty owner's (§7.5.1 there), so nothing recorded above made any field, `throttle_count`
   included, depend on a user. **General SHDR consent has no time limit and covers what predictive maintenance needs
   on an ongoing basis; the characterisation opt-in covers the extended set for its window, to find out what that is**
   (the principal, 2026-11-26, `OI-NVRAM-25` (b) as refined, `OI-NVRAM-27`). The `NP-MOD-ID-001` §7.4 review gate sets
   general consent's content from the characterisation data. Until it rules, general consent covers the baseline only,
   and the new per-channel totals, the temperature bands, `throttle_count` and §7.2's per-socket fields flow only from
   opted-in helmets within the window. General consent grows only by asking. The owner is prompted when the window ends
   and again when the analysis is done; silence keeps the consent at its current content level (`REQ-SHDR-12`; app
   build `OI-NVRAM-26`; the content link and the notice `OI-NVRAM-28`). D-39. *(Rev 22: an earlier reading the same day (Rev 21), that
   either consent admits the new fields, is replaced: the principal said general consent covers only what predictive
   maintenance needs, which is not known until the characterisation data is analysed.)*
5. **The working environment: time in 5 °F temperature bands, initially**, as SHDR running totals, chosen for detail
   and expected to be revisited once a first dataset is analysed (UC-215). Humidity is wanted if it is sensed. No
   sensor exists, and the cost of temperature and humidity sensing is under investigation
   ([#1086](https://github.com/stevehickman/NeuroPulse/issues/1086)). The band range, the sensor and the home are
   `OI-NVRAM-24`'s.

*The shape is unchanged:* running totals, no per-session row, no ordinal, no clock, differences computed by the app
between downloads, and no per-window value in the fleet (D-18). *Specified, not built:* the fields do not fit the
row's room as it stands (§3.3.1.6). Building them is `OI-NVRAM-23` and `OI-NVRAM-24`. D-38.

#### 8.5.1 The working-environment record, specified — added at Rev {NP-FW-NVRAM-001 to} (`OI-NVRAM-24`)

Ruling 5 above asks for time in 5 °F bands. This section specifies the record as far as it can be settled without a
sensor. **Not built: no sensor that measures the environment may feed SHDR today**, and which one will is put to the
principal (`docs/human-input/oi-nvram-24-working-environment-sensor.md`). Nothing below is a ruling. The range is derived,
and two choices that are not derived have register rows (UC-218, UC-219).

**The sensor (open).** The helmet's thermistors are the tile NTCs, the five cranial sense-domain NTCs and one hub NTC
(`NP_NTC_CHANNEL_COUNT`, `np_safety_config.h`). No ambient NTC exists (`NP-ENV-001` §5, `OI-ENV-05`).

- **Tile and sense-domain NTCs are excluded.** They sense the lattice against the scalp, and a tile NTC's per-socket
  reading in a session is UHDR (F7). Body heat is biology, so `CLAUDE.md` §5.1 rule 1's positive demonstration is not
  available. Reading them only while the helmet is off the head would make the record a function of whether it is worn,
  which `CLAUDE.md` §5.1 rule 2 forbids.
- **The hub NTC is off the scalp, but it is not a clean source.** It is read only by the safety MCU, for the 62 °C
  all-channel cutoff (`np_thermal_interlock.c`), and no reply or report window carries a temperature to the hub
  (`np_spi_wire_types.h`). It reads the hub heatsink's self-heat from the cranial lattice as well as room air
  (`NP-TOOL-HUB-001` §2). Whether head heat reaches it is unmeasured, and the hub's mounting is not fixed
  (`OI-HTOOL-01`). Its conversion (`adc_to_celsius()`) reads whole degrees from 1 to 110 °C only. Using it needs a
  Class C report or a second read path, the Privacy Lead's demonstration, and an extended table, and it would record the
  hub's temperature, not the room's.
- **A dedicated ambient NTC** off the thermal path and away from the head would measure the working environment. It does
  not exist, and its cost is [#1086](https://github.com/stevehickman/NeuroPulse/issues/1086)'s.

**The input the record needs, from whichever sensor.** A temperature in tenths of a degree Celsius (`int16_t` dC, the
unit `NP-FW-POE-001` §6 already assumes, `OI-POE-06`), covering at least −20.6 to +60 °C (the band range below), and a
flag for no valid reading. A whole-degree input would put each band edge up to 1 °C (1.8 °F) off its place, and a
5 °F band is 2.78 °C wide.

**The band range, derived (`CLAUDE.md` §18).**

| Bound | Value | Derived from |
|---|---|---|
| Range spanned | −20 to +60 °C (−4 to 140 °F) | `NP-ENV-001` §2.1, the survival envelope. Inside it the helmet is warranted against permanent degradation, so a reading inside is in specification and one outside is not; a wear record has to tell the two apart. The operating limits lie inside it (`NP-ENV-OPRANGE-001` §2) |
| Edge grid | Whole multiples of 5 °F | Puts the hot survival limit (140 °F = +60 °C) and the ambient block (95 °F = +35 °C, `NP-ENV-OPRANGE-001` §2) on edges. 144 °F is not a multiple of 5, so the two survival limits cannot both be edges; a grid anchored at −4 °F would put neither 140 nor 95 on one |
| Lowest edge | −5 °F (−20.56 °C) | The largest multiple of 5 °F at or below −4 °F. It lies 0.56 °C past the envelope, less than the 1 °C step of the conversion as built, so no reading in the built system could tell the difference |
| Highest edge | 140 °F (+60 °C) | The hot survival limit, on the grid |
| Bands in range | (140 − (−5)) / 5 = **29** | The two edges above and the 5 °F width (UC-215) |
| End bands | **2**: below −5 °F, and 140 °F or above | Saturating, so time outside the envelope is kept, which is the exposure a wear record most needs. With them, every valid reading lands in exactly one band |
| No valid reading | **1** count | A fault reading must not land in an end band. The ADC's fail-safe value reads as 110 °C (`np_hal_adc.c`), so a broken sensor would otherwise read as hot |

**31 bands and one count of seconds with no valid reading.** Invariant, for a test: the 32 counters add up to every
second counted (saturation aside). **Considered and not used:** 42 °C is a scalp limit and the sensor is off the scalp;
62 °C is a junction cutoff and lies inside the top end band.

**The band index** for a reading `t` in dC, in integer arithmetic: `k = floor((18·t + 3700) / 500)`. Index 0 when
`k < 0`; index `k + 1` when `0 ≤ k ≤ 28`; index 30 when `k ≥ 29`. Its derivation: 100 × °F = 18·t + 3200, and
k = floor((°F + 5) / 5). Checks: +60.0 °C gives `k` = 29, the top end band; +35.0 °C gives `k` = 20, the band that starts
at 95 °F; −20.5 °C gives `k` = 0; −20.6 °C gives `k` < 0, the bottom end band.

**Where it lives: one record per helmet, in Config, as its own replicated record.**

- *Per helmet,* because there is one sensor per helmet whichever is chosen, and the environment is the helmet's, not a
  module's. Map 3's row cannot hold it anyway: it has 19 B of room and emitter use alone needs 20 B (§3.3.1.6).
- *Config,* because the data is SHDR class, which §3.2 allows there (only UHDR class is barred, D-3), and because a
  replicated record is replaced whole and survives a torn write (§4.2). **Not the SHDR partition:** its writer,
  `np_log_backend`, only appends, and totals appended one after another form a series of per-window values, which D-18
  keeps out of the fleet. Factory reset zeroes Config (R-7) and the SHDR partition (R-6) alike, so neither keeps the
  totals across a reset.
- *Layout, version 1, 132 B:* a header of version (1 B), band count (1 B, 31), lowest edge in °F (`int8_t`, −5) and
  band width in °F (1 B, 5); then 31 band counters and the no-reading counter, each a `uint32_t` of seconds, stored
  whole and added with saturation (`OI-UINT16-01`). The store reads a replicated record at one fixed length (§7.2). A
  coarsening, which the principal expects may follow the first analysis (UC-215), only lowers the band count, so the
  length never grows. Running totals only: no per-session row, no ordinal, no clock (D-18).

**Time and cadence.**

- *What counts:* every second the hub is powered (UC-218, put to the principal as Q2). Unpowered time cannot be counted
  by any choice, because there is no clock and no backup battery (`CLAUDE.md` §4.5).
- *Sampling:* each second, add 1 to the band of the latest valid reading, or to the no-reading count. The second is the
  unit of Map 3's time totals (`present_s`, `used_s`). The error is at most one second per band crossing, against a
  self-heat decay of minutes (`NP-THERM-COOL-001` §7.5.4).
- *Persistence:* written with Map 3 at each session end and every hour of powered time (UC-219). A power cut loses at
  most the time since the last write, because there is no last-gasp write (§4.3). Each write is two copies of
  12 + 132 + 4 = 148 B, about 7 KB a day at the hourly rate, which is negligible against §3.5's endurance budget.

**Who sees it.** It reaches the fleet only on D-39's terms (§8.5 table). The fleet columns are specified in
`OI-NVRAM-24` and are not added to the schema. They are 31 band totals and the no-reading total, named for the band
edges in °F, never for dose (`DOSE-01`).

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

*Rev 11: `OI-HEXMAP-01` is closed (§4.2.1), and the functions exist and are tested through the real
store. The three consequences below still hold on target until the call sites exist, which waits on
`OI-HEXMAP-02` (the poll's `inventory_fn`) and #340 (the block device).*


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
| **D-5** | **Map 3 is a separate file with its own magic, version and policy** — reached independently from `file_max` (§3.5), from cache-versus-record (§7.2), and from write granularity (§3.5) *(Rev 18: Map 3 is now `NP_CFG_FILE_MODULE_USAGE`, a REPLICATED record of its own, not `map3.jrn`. The separation from the blob, and the opposite version rule, stand: D-35, D-36)* |
| **D-6** | **`np_hexmap_nvram_write()` must never destroy the live record before the replacement is durable.** No truncate-in-place; LittleFS's copy-on-write commit supplies A/B semantics without a second slot |
| **D-7** | **Fail-closed means "declare the gap", not "report zero", for a record.** An unreadable journal reports *unknown since ordinal S*; a fabricated zero would drive Map 2's totals backwards |
| **D-8** | **No last-gasp write.** `SEAT#` has no firmware consumer, is aggregated at Class B, and its pre-break window is undimensioned. Correctness never depends on advance warning *(Rev 6: the undimensioned window is not what D-8 rests on. A dimensioned one would support no write either, §4.3.1.)* |
| **D-9** | **Every ordinal is the existing Config eMMC session counter plus a per-record `seq`.** No new counter, no wall-clock field, ever *(Rev 18: Map 3 keeps no ordinal, window or watermark, so this binds nothing it writes; no clock field, still)* |
| **D-10** | **The control software initiates every sync.** The helmet cannot know which peer holds Map 2 for these modules |
| **D-11** | **Acknowledgement precedes retirement; retirement is a watermark advance, not an erase.** A peer that loses its own store can re-request from a lower watermark *(Rev 18: superseded for Map 3 by D-35. Nothing is acknowledged or retired; a read changes nothing)* |
| **D-12** | **Journal-full degrades: keep totals, drop detail, set `detail_lost`.** Never wrap over unsynced rows; never block a session for want of a phone *(Rev 18: superseded for Map 3 by D-37, which keeps "never wrap silently, never block a session")* |
| **D-13** | **Map 2 is not authoritative for a module's existence. Only the socket is** |
| **D-14** | **Reconcile monotone counters by `max`; never sum, never roll back, and never upload a divergence magnitude** — a divergence is as joinable as a baseline (`NP-MOD-ID-001` §6.3) |
| **D-15** | **Map 1 is a legitimate generated artefact under `NP-NPPS-REF-001` §1.6 on origin grounds, but §1.6's stated *reason* does not transfer** — a new tile minor needs no rebuild — which is what makes the update check mandatory rather than convenient |
| **D-16** | **Blob version 0x0003 → 0x0004 adds `(major, minor)`; no migration path, matching the existing rule.** `REC_BYTES` 175 → 177, blob(80) 14,012 → 14,172 |
| **D-17** | **An unknown `(major, minor)` is a limits problem, not an identity problem.** Elements are known from the tile's own inventory; ranges are not. Do not drive an element whose limits you do not hold — and **never substitute the nearest known minor** |
| **D-18** | **No sync-boundary field and no per-window delta may be uploaded.** What reaches SHDR is the running total. A delta is a timestamped count once the party holding it can date the interval's ends |
| **D-19** | **Map 2 is app-side and inherits UHDR-class handling**; no code path may read Map 2 and write SHDR, mirroring the `SHDRUploader`/`ConsentStore` independence in CLAUDE.md §6 |
| **D-20** | **Insertion events are not recorded anywhere.** Mate cycles exist as a total only; an event timeline is a record of changes in treatment target and nothing needs it |
| **D-21** *(Rev 2)* | **The Config journal is LittleFS files. `EMMC-CFG-02`'s raw-write region is void — and its *justification* is refuted, not merely outweighed.** At `prog_size` 256 B an append costs one 256-byte program; a raw region cannot make the program unit smaller, because the unit belongs to the eMMC. The cost the clause was aimed at is the 14 KB whole-blob rewrite, which **D-5** already removes. §3.3.1.1 |
| **D-22** *(Rev 2)* | **The UKMD record is a named LittleFS file, not "Config partition offset 0x1000".** A byte offset into a mounted partition names a block the filesystem owns and may relocate. Safe to correct because no code ever used it — `np_uhdr_hal_config_read_ukmd()` has always abstracted it. `NP-FW-EMMC-002` §C.3 corrected in the same change. §3.3.1.4 |
| **D-23** *(Rev 2)* | **Widen the Config specification, do not forbid the write.** `EMMC-CFG-01`'s contents list and `EMMC-CFG-02`'s normal-operation whitelist gain the `"NPMP"` blob and the Map 3 journal *(Rev 20: the journal file is removed unwritten, `OI-NVRAM-21`; Map 3 is `modusage.rec`, a Config file under the same widening)*. `np_module_map_persist()` is correct and the specification predates the hex-tile architecture; the divergence had to become a decision rather than stay silent. §3.3.1.4 |
| **D-24** *(Rev 2)* | **Map 3 is a 32-byte record in a 49,152-byte file — 1,536 rows, ~19 per socket at 80 sockets.** 32 B so eight records fit one program unit and none straddles it; 48 KiB not `file_max` so a future 40-byte row still fits without re-deciding the bound. Stated assumption: one row per module per session — 19 days of margin at home cadence, 1.6 days at clinic cadence, beyond which §6.3 degrades to totals-plus-`detail_lost` rather than failing. Closes `OI-NVRAM-13`. §3.3.1.5 *(Rev 18: retired with the journal; the record's size is §3.3.1.6)* |
| **D-25** *(Rev 3)* | **The Map 3 row describes itself: §3.3.1.5's two padding bytes are `len` and `ver`, and the reader steps by `len`.** Version 1 is the row of D-34 (64 B). A later version appends before the CRC; offsets [2, 60) are fixed for ever, and `reserved` is never reused. The reader accepts any `ver ≥ 1` and skips a tail it does not know. This is what makes §7.2's rule enforceable, and `np_map3_record_tests` enforces it (`OI-NVRAM-08`) *(Rev 18: retired with the journal; its self-description survives as the record header's version and row length, D-36)* |
| **D-26** *(Rev 3)* | **Map 2's code carries `Map2` as a word in the name of every file, type and module that holds it.** `check-map2-shdr-boundary.ts` clause A finds Map 2 by that name and cannot find it otherwise (`OI-NVRAM-09`, §8.3.1) |
| **D-27** *(Rev 3, principal 2026-09-26)* | **A factory reset keeps durable evidence that it started, in the Config partition it erases last (§3.4.1 option A).** Written at R-3 before any erase, and a reset that cannot write it does not start. At boot, the SNVS flag, the marker **or a Config with no filesystem** completes the reset from R-5. An unreadable Config does not. Accepted consequence: a Config lost for any other reason also resets the device and mints a new warranty token (`OI-NVRAM-05`) |
| **D-28** *(Rev 7)* | **Every per-session value `NP-MOD-ID-001` §7.2 admits is accumulated by the hub while the session runs, never read from `U1` once at session end and never computed as a difference of Map 4 reads.** Map 4 has no socket field and loses the session an extraction interrupts, so any other source drops those sessions from §7.4's exposure covariate, possibly not at random. With this rule the Map 4 loss reaches §7.4's model only through the carried-in baseline, within MODID-6's 250-session rounding. No polling interval is set (`OI-NVRAM-06`, §6.5.1) |
| **D-29** *(Rev 7)* | **A session interrupted by extraction is a benign Map 4 shortfall, not a §6.5 anomaly.** The hub sets Map 3 `fault_flags` bit 0 (`NP_MAP3_FAULT_UNSEATED_AT_CLOSE`) on a row it closes after the module has stopped answering on its socket. Map 4 is compared against the carried-in baseline plus unflagged rows only, counting a session once per ordinal. Only a shortfall below that is an anomaly. The difference is never written into Map 4, and the flag is never uploaded (§6.5.1, §8.1) *(Rev 18: bit 0 is sticky in the per-module row, and the comparison uses the row's mate cycles in place of per-ordinal exclusion, §6.5.1)* |
| **D-30** *(Rev 8)* | **`NP-MOD-ID-001` §7.4's fit carries presence (`module_session_count`) and calendar age (`last_seen_month` − `module_manufacture_date`, months, same `module_life` row) as covariates beside exposure.** Without them, idle aging in a rarely-driven socket reads as a socket effect, the false positive §7.4's decision rule turns on. Both are already in SHDR: **no field, no clock finer than `TIME-01`'s month and no Map 3 change** is added, and sub-month cadence stays unrecoverable (D-18). The gate states the assumption it cannot test: cadence acts only through the measured thermal fields. §5.3.1 |
| **D-32** *(Rev 11)* | **`REQ-LFS-02` is a property of `np_module_map`, not of the boot sequence.** Every socket record carries a RAM-only `confirmed` flag. `load()` leaves it false, and it is set only when `apply_poll()` sees the record's UID in that socket or inventories a new module there. Every reader (resolve, resolve_group, check_placement, get_cal, set_cal, socket_uid) answers only from a record that is present **and** confirmed. The alternative, an ordering rule on the boot integration ("poll before anything reads the map"), was rejected. Nothing enforces an ordering but its callers, and the failure it guards against, a valid old blob, is invisible to every check below it. §4.2.1 |
| **D-33** *(Rev 13, `OI-UINT16-01`)* | **SUPERSEDED by D-34.** It made Map 3 a 64-byte version-2 row beside a 32-byte version 1, with the version-1 count fields holding 0xFFFF above 16 bits. There is no version 2 or ceiling now. The 64-byte size and its reason (four rows to a program unit; a 40-byte row would straddle one) stand. §3.3.1.5 |
| **D-34** *(Rev 13, `OI-UINT16-01`)* | **Map 3 has one layout: a 64-byte row, version 1, with 32-bit counts stored whole and no ceiling field.** No hardware has shipped and no Map 3 row has ever been written, so there is no stored history and nothing to read in an older layout (the principal, 2026-11-19). The 32-byte row, its 16-bit counts, the duplicate 32-bit tail and the 0xFFFF compatibility ceiling are removed. Offsets: `len` 0, `ver` 1, `uid` 2, `seq` 10, `ordinal` 14, `session_count` 18, `dose_accum` 22, `throttle_count` 26, `fault_flags` 30, reserved [31, 60), CRC 60. §7.2's tail-additive rule is unchanged: a later version appends before the CRC and offsets [2, 60) are fixed for ever. File bound unchanged: 768 rows, 9 per socket at 80 sockets. §3.3.1.5 *(Rev 18: retired with the journal. Its rule for counts, 32 bits stored whole with no ceiling, carries over to the record, §3.3.1.6)* |
| **D-31** *(Rev 10)* | **A mate is decided by the hub and applied by `U1`, and is written when it happens, not at session end.** The hub counts a mate when a poll returns a UID and its previous observation of that socket (this power-on, or the valid inventory cache at boot) was empty or a different UID. It never counts a first observation. `U1` honours the increment at most once per power-up, because every genuine mate power-cycles it (`VCC_3V3` is in contact group 3). A count of `U1` power-ups alone was rejected, because every hub power cycle would add a mate to every seated tile. The count is a lower bound, and it adds no field. **D-20** holds: nothing about the event is stored. `NP-MOD-ID-001` §5.3.1 |
| **D-35** *(Rev 18, `OI-NVRAM-20`)* | **Map 3 is one upserted record of one row per module (`NP_CFG_FILE_MODULE_USAGE`), and a download is a snapshot read.** The record is replaced whole at each session end and kept REPLICATED. Reading it changes nothing on the helmet: no watermark, no acknowledgement, no row retired. The app differences each row against its previous snapshot of the same helmet and row, and stamps the difference with its own time, in Map 2. An unreadable table is reported unknown. §4.2(b), §6.1 |
| **D-36** *(Rev 18)* | **The record has one length for ever (12,288 B); its rows grow only at the tail, and a rewrite carries what it does not know.** The header holds one version and one row length; the reader takes any version from 1 and any row length from 64 to 83 B; bytes 0 to 63 never move; a row the writer creates starts the tail at zero. A different record length, a row past 83 B or more than 147 rows needs a converter first. The journal's per-row CRC, `seq` and watermarks are dropped because the store's envelope checks the record whole and nothing is recovered row by row. §3.3.1.6, §7.2 |
| **D-37** *(Rev 18)* | **A full table gives up a row visibly; a dropped commit is visible too.** The table fills with modules, not sessions. A newcomer takes the row of the least-present module with nothing pending (UC-208) and starts with `detail_lost` set, as does any row made in a full table; a module whose contribution was dropped because the record could not be read gets `detail_lost` at the next commit that succeeds. A session is never refused and a fitted module is never left uncounted. §6.3 |
| **D-38** *(Rev 21, the principal, 2026-11-26)* | **Delivered dose never reaches the fleet and is kept per wavelength for the person; emitter use and the working environment reach SHDR under the warranty owner's consent (D-39).** Dose is UHDR, one total per wavelength for the person's app, and by D-3 lives off Config. Each tile's row is to carry, per emitter channel, a running total of on-time and of the sessions in which the channel was switched on and off. Temperature is kept as time in 5 °F bands, initially (UC-215). `NP-MOD-ID-001` §7.2 and `DOSE-01` stand. Specified, not built (`OI-NVRAM-23`, `OI-NVRAM-24`); per emitter means per drive channel, accessory ports included (the principal, 2026-11-26, `OI-NVRAM-25` (a)). §8.5 |
| **D-39** *(Rev 21; rewritten Rev 22, the principal, 2026-11-26, `OI-NVRAM-25` (b) as refined, `OI-NVRAM-27`)* | **General SHDR consent has no time limit and covers what predictive maintenance needs on an ongoing basis, which the `NP-MOD-ID-001` §7.4 review gate sets; until it rules, the extended content flows only from opted-in helmets within the characterisation window; general consent grows only by asking.** The extended content is the per-channel on-time and session totals (tiles and accessory ports), the 5 °F temperature bands, `throttle_count` and §7.2's per-socket, per-session fields. §7.3's firmware-enforced stop and §7.5's bundled-consent argument stand for it. Until the gate rules, general consent covers the baseline (the device-condition fields it covers today). If the gate adds content, the owner is told in plain words what is added and asked; silence keeps them at what they agreed. The warranty owner is prompted (1) when the window ends, showing what general consent then covers, and (2) when the analysis is done, showing general consent as the gate set it with what is added named. The analysis-done notice is a broadcast through the app update mechanism to all warranty owners, carrying the gate's content set, never targeted (the principal, 2026-11-26). At both, the question is fresh, the default is no change, silence keeps the existing consent at its current content level, the end of the window is its own state (neither a revoke nor never asked), characterisation is never re-enrolled (`REQ-SHDR-09`), and the question is the warranty owner's alone (`REQ-CONSENT-01`). `REQ-SHDR-12`; `OI-NVRAM-26`, `OI-NVRAM-28`. *Replaced the same day:* the first form of D-39 said either of the warranty owner's consents admitted these fields and superseded §7.3 and §7.5 for them; the principal then said general consent covers only what predictive maintenance needs, which is not known until the characterisation data is analysed. §8.5 |

---

## 12. Risk register

Scales per `NP-RM-001` §4. Status: **MITIGATED** (controls in place, residual acceptable) · **OPEN**
(no adequate control yet) · **ALARP** (control selected, verification outstanding).

| ID | Sev | Hazard | Cause | Consequence | Control | Owner | Status |
|---|---|---|---|---|---|---|---|
| **RISK-NVRAM-01** | **HIGH** | Unsynced module history is destroyed by a firmware update | Map 3 inherits the inventory blob's reject-and-rebuild policy, which is correct for a cache and fatal for a record | Irrecoverable loss of the only on-device record of module usage; Map 2 totals silently regress below Map 4 | **D-5** separate file, separate version, tail-additive reader; **D-7** declare-the-gap; **D-14** reconcile by `max` so a regression cannot be written back | FW | **MITIGATED — `OI-NVRAM-08` check written and falsified (Rev 3, §7.2.1, D-25). The store reads a grown row (`OI-NVRAM-16` closed, Rev 5)** *(Rev 18: the control now binds the upserted record, D-36: one record length for ever, a reader that takes any version and row length that fits, a writer that carries the fields it does not know; checked and falsified in `np_module_usage_tests`, §7.2.1)* |
| **RISK-NVRAM-02** | **HIGH** | A per-window delta plus an app-side clock reconstructs per-socket usage over time in SHDR | Map 3's natural output is a delta; the initiating party holds a clock; `TIME-01` inspects for time *types* and an ordinal is an integer | `NP-MOD-ID-001` §7.5.1.1's registrant-scoped consent model loses the absence it rests on, in every configuration | **D-18** totals only, no boundary field; **D-19** structural separation of Map 2 from any SHDR writer | FW + Privacy | **MITIGATED — `check-map2-shdr-boundary.ts`, falsified (Rev 3, §8.3.1, D-26). Residual: a name check; see the reach it states** *(Rev 18: Map 3 now produces no delta at all; the app computes one from two snapshots and keeps it in Map 2, D-35. The controls stand)* |
| **RISK-NVRAM-03** | **HIGH** | The store this HAL needs cannot legally exist as specified | `EMMC-CFG-02`'s raw journal area has no address space inside a LittleFS instance that owns all 16 MiB | Either the journal is built on corrupting writes, or it is not built and Map 3 has no high-frequency substrate | None yet — it is a specification decision, not a firmware one | FW + Quality | **OPEN — `OI-NVRAM-01`, BLOCKING** |
| **RISK-NVRAM-04** | **HIGH** | A tile is driven against substituted limits | Unknown `(major, minor)`; nearest-known-minor substitution is the tempting recovery | Over- or under-drive of an emitter whose specifics the minor exists to track; only backstop is the 62 °C thermal cutoff | **D-17** absolute substitution ban; present-and-unclassified state; `check_placement()` refusal | FW + Safety | **MITIGATED — but see `OI-NVRAM-10`** *(2026-09-24: a hardware optical bound is decided, `NP-HW-HEXTILE-001` D-9, not yet built — `OI-HEXTILE-24`)* |
| **RISK-NVRAM-05** | MEDIUM | A torn write discards a good inventory unnecessarily | `np_hexmap_nvram_write()` implemented as truncate-then-write | One spurious full re-poll per interrupted write; user-visible bring-up delay, no data loss | **D-6** contract on the HAL, plus a power-fail-injection test at each program-unit offset | FW | **ALARP — test not written** |
| **RISK-NVRAM-06** | MEDIUM | Config partition wears out early | Per-event journalling implemented as full-blob rewrite: 25.2 MB/session, ~4.6 years at clinic rate (§3.5) | eMMC `PRE_EOL_INFO` reaches 0x03 and `EMMC-WE-02` blocks session start | **D-5** small append records instead of blob rewrite; `EMMC-WE-01` already logs the wear indicator every boot | FW | **MITIGATED by design choice** *(Rev 18: the upserted record rewrites 24,608 B per session end, 1/1,024 of the 25.2 MB the hazard names, §3.3.1.6)* |
| **RISK-NVRAM-07** | MEDIUM | Map 3 outgrows the file it shares with the inventory | `file_max` on Config is 65,536 B; the blob takes 14,012 at 80 sockets, leaving 644 B/module and no warning until a write fails | Silent truncation or a run-time write failure mid-session | **D-5** separate file with an explicit stated bound; **D-12** degrade-with-flag when it is reached | FW | **MITIGATED — bound not yet chosen (`OI-NVRAM-13`)** *(Rev 18: Map 3 is a fixed 12,288-byte record in files of its own, so it can neither outgrow nor share a file; the bound is fixed, §3.3.1.6)* |
| **RISK-NVRAM-08** | MEDIUM | An interrupted factory reset is not detected | `NP_SNVS_RESET_IN_PROGRESS` survives a warm reset, not a power removal, and there is no `VBAT` rail | Device boots with UHDR/SHDR/Config half-erased — the exact outcome `np_main.c` says must not happen | **D-27**: a durable Config marker written before the first erase; the boot check completes the reset on the marker or on a Config with no filesystem, and never on an unreadable one. Power-cut simulation at every step and a 24-cut sweep of the marker write | FW + Privacy | **MITIGATED — implemented and host-tested (Rev 3, §3.4.1). Residual: called at bring-up since Rev 9 (`OI-NVRAM-17` closed), but every boot refuses UHDR/SHDR until #340 binds a block device; a refused boot admits no session (`OI-NVRAM-19`, option b); target eMMC behaviour unverified (`OI-LFS-07`)** |
| **RISK-NVRAM-09** | MEDIUM | The filesystem every durability claim rests on is unmanaged SOUP | `NP-SW-001` §9.4 lists LittleFS as *"2.x"*, Class B, verification *"Power-loss testing per LittleFS test suite"* — no vendored copy, no pinned tag, no SHA, no §7.1.2 anomaly evaluation, and nothing under `firmware/vendor/` | Every atomicity property in §4 is asserted against a component with no configuration identity | None yet | FW + Quality | **OPEN — `OI-NVRAM-12`, BLOCKING** |
| **RISK-NVRAM-10** | LOW | Map 4's layout does not fit the part actually fitted | `NP-MOD-ID-001` §5.2 sizes four 32-byte slots at **exactly** 128 B, against ATtiny424/426/427; CLAUDE.md §3 still names the retired design's **ATtiny402** for the on-module MCU | Fewer slots than the crash-safety argument assumes, or no room for a version field | `NP-HW-HEXTILE-001` §6.2 selects ATtiny426/427-class, which is the governing document | FW + EE | **MITIGATED — documentation drift only (`OI-NVRAM-14`)** |

---

## 13. Open items

| ID | Description | Owner | Blocking |
|---|---|---|---|
| **OI-NVRAM-01** | **DECIDED at Rev 2 (§3.3.1, D-21) — DOWNGRADED from BLOCKING to a documentation action.** The journal is LittleFS files; the raw region is void and its "no LittleFS overhead" rationale is refuted by `prog_size` arithmetic, not merely outweighed. **Map 3 is now implementable against this document.** What remains is landing `ECR-EMMC-001`'s three clause changes in `NP-FW-EMMC-001`, which is a `.docx` and so not edited here (`OI-CONV-04`). Replacement text is given in §3.3.1.4 | Quality | Documentation consistency — **no longer blocking Map 3** |
| ~~**OI-NVRAM-02**~~ | ✅ **CLOSED 2026-09-13 (Rev 2, D-22).** The UKMD record is a named LittleFS file; `NP-FW-EMMC-002` §C.3 corrected in the same change. Safe because no code ever used the offset — `np_uhdr_key.c` reaches the record through `np_uhdr_hal_config_read_ukmd()`/`_write_ukmd()`, so it was a spec-side artifact only | — (closed) | — |
| **OI-NVRAM-03** | **DECIDED at Rev 2 (D-23) — widen, do not forbid**, as Rev 1 predicted. The write is correct and the specification predates the hex-tile architecture. Carried in `ECR-EMMC-001` with `OI-NVRAM-01`; the decision no longer waits on anything | Quality | Documentation consistency |
| ~~**OI-NVRAM-04**~~ | ✅ **CLOSED 2026-09-27 (Rev 6, #444). Re-homed, not dimensioned.** Nothing in this document requires the pre-break window's duration. The hub loses no power on extraction, the tile cannot observe `SEAT#`, and the I2C command path breaks with the tile's supply at the end of the window, so even a dimensioned window supports no write (§4.3.1). D-8 stands on its own. No extraction velocity is assumed. The stagger dimension *is* required by `NP-HW-HEXTILE-001` §7.3's mating order and `SEAT#`-last guarantee, and it is raised there as **`OI-HEXTILE-28`**. Original text: ~~The pad-length stagger between contact group 3 and `SEAT#` is not dimensioned, and no extraction-velocity assumption is stated, so the pre-break warning window has no duration. §4.3 designs around its absence; any future use of `SEAT#` as a timing signal needs this number~~ | — (closed; continues as `OI-HEXTILE-28`) | — |
| ~~**OI-NVRAM-05**~~ | ✅ **CLOSED 2026-09-26 (Rev 3, #444; option A chosen by the principal, D-27).** Every comment and clause that called either flag a power-loss flag says warm-reset only. The anonymisation half needs nothing more. The factory-reset half is fixed by a durable Config marker written before the first erase, and by `np_factory_reset_boot_check()`, which completes the reset on the flag, the marker or a Config with no filesystem, and never on an unreadable one. `np_cfg_store_mount()` now tells those last two apart. Tested by a power cut at every step, with a flag-only mutant failing, and by a power-loss sweep of the marker write (§3.4.1). **Continues as `OI-NVRAM-17`** (the bring-up call site) | — (closed) | — (`NP-MOD-ID-001` §10's rotation test is unblocked by design; it still needs a runnable image) |
| ~~**OI-NVRAM-06**~~ | ✅ **CLOSED 2026-09-27 (Rev 7, #444, §6.5.1).** **Quantified, accepted and documented.** Each extraction loses at most one session's contribution from Map 4, so Map 4 is a lower bound that never goes backwards. The loss **does not reach `NP-MOD-ID-001` §7.4's model**, because Map 4 has no socket field and the model's per-socket exposure is the hub's, which **D-28** now requires. Map 4 enters the model only through a carried-in baseline, which MODID-6 already rounds down by up to 249 sessions. The shortfall *would* have raised §6.5's anomaly flag on ordinary handling, so **D-29** marks the interrupted row (`NP_MAP3_FAULT_UNSEATED_AT_CLOSE`) and excludes it from the comparison. The mate-cycle gap it found continues as `OI-NVRAM-18`. Original text: ~~Map 4's one-write-per-session-end cadence means **a session interrupted by tile extraction contributes nothing to that tile's odometer**. Quantify whether that matters for `NP-MOD-ID-001` §7.4's model, or accept and document it~~ | — (closed) | — |
| ~~**OI-NVRAM-07**~~ | ✅ **CLOSED 2026-09-28 (Rev 8, #443, §5.3.1, D-30).** The trajectory lacks cadence, not time. §7.4's covariates are use (exposure, presence, thermal history), all clock-free, and SHDR's `module_life` row already gives calendar age to a month (`module_manufacture_date`, `last_seen_month`). Only sub-month spacing is missing; it stays missing (D-18) and its physical effect is measured by the NTC. The gate's real exposure was a spurious socket effect from idle aging in rarely-driven sockets; D-30 puts presence and calendar age in the fit and `NP-MOD-ID-001` §7.4 records it. Not an instance of `OI-EMMC2-13`, which stays open. ~~Map 3's ordinal indexing gives predictive maintenance a trajectory with unknown, non-uniform spacing in time. This is `OI-EMMC2-13`'s general defect acquiring another instance; recorded so the review gate does not discover it~~ | FW + Data | — (closed) |
| ~~**OI-NVRAM-08**~~ | ✅ **CLOSED 2026-09-26 (Rev 3, #444).** Written as `np_map3_record_tests` over the new `np_map3_record` codec (§7.2.1, D-25). It is falsified in both directions: it passes the production scan and fails four rule-breaking readers, each of which is shown correct on a pure-v1 journal. It surfaced `OI-NVRAM-16`. ~~Write and falsify the CI check that a journal written under version *n* loses no record when read under *n+1*~~ | — (closed) | — |
| ~~**OI-NVRAM-09**~~ | ✅ **CLOSED 2026-09-26 (Rev 3, #444).** Written as `scripts/check-map2-shdr-boundary.ts` and run by `tooling-ci.yml` `map2-shdr-boundary` (§8.3.1). It depends on D-26's naming rule, and it is falsified in both directions by a hermetic self-test and by hand against the real tree. Its reach is stated: per file, and by name. ~~Write and falsify the CI check that no code path reads Map 2 and writes SHDR, and that no SHDR column or upload field names a sync boundary or a per-window delta~~ | — (closed) | — |
| ~~**OI-NVRAM-10**~~ | **✅ CLOSED 2026-09-24 — decided (`NP-SOUP-LFS-001` Rev 10 §13.14; `NP-HW-HEXTILE-001` Rev 13 D-9).** Safety + Hardware Engineering chose options A and B together. Each tile CH_A/CH_B stage is regulated to a fixed hardware reference that caps on-irradiance at ≤ 400 mW/cm² (`REQ-TDRV-01`), and a gate-path limiter holds conduction ≤ 50 % over any window ≥ 250 ms, which caps the average at the 200 mW/cm² CW ceiling (`REQ-TDRV-02`). No Map 1 range, register or firmware can move either. **Decided, not built:** until `OI-HEXTILE-24` verifies the circuit, the thermal cut remains the only non-firmware bound, and §9's re-derivation condition still applies. *(Rev 8 note: 2026-09-24, `NP-SOUP-LFS-001` Rev 8 §13.12: **answered — no.** The safety MCU owns the PBM enable lines and their thermal cut only; the tile's on-current is set by hardware but unregulated, and Map 1 reaches drive through Class B firmware. The drive stage is specified inconsistently — `NP-HW-HEXTILE-001` `OI-HEXTILE-23`. Three options, not chosen, in §13.12.)* **Is there a Class C bound on per-tile emitter drive current that is independent of Map 1's ranges?** If not, a wrong range is bounded only by the 62 °C thermal cutoff — a thermal limit standing in for an optical one — and §9's classification must be re-derived before any tile variant with differing ranges ships | Safety + EE | — (closed; implementation `OI-HEXTILE-24`) |
| ~~**OI-NVRAM-11**~~ | ✅ **CLOSED 2026-09-13 (Rev 2).** The comment now derives `REC_BYTES` = 175 and gives blob(80) = 14,012 and blob(128) = 22,412, the values the macros compute and the tests assert. **A second stale claim in the same comment was found and corrected with it** — it placed the UKMD record *"at offset 0x1000"*, the address D-22 retires; the comment now states that everything on Config is a LittleFS file and that the binding per-file bound is `file_max` = 65,536 B, not the partition size | — (closed) | — |
| **OI-NVRAM-12** | **SUPERSEDED 2026-09-13 by `NP-SOUP-LFS-001` Rev 1**, which brings LittleFS under SOUP management, replaces `NP-SW-001` §9.4's cell, and performs the hazard analysis. Two findings come back to this document: **LittleFS is not merely unmanaged, it is absent** — two `lfs_` tokens in all of `firmware/`, both comments — so §4's guarantees rest on a component that is nominated rather than integrated; and **§9's Class B argument is a property of `np_module_map`'s reject-and-rebuild policy, not of LittleFS**, which is the policy §7.2 specifies Map 3 to invert. That is now `REQ-LFS-01`: no value bounding an emission may be stored under a tail-additive policy. Continues as `OI-LFS-01` (pin + vendor — **CLOSED 2026-09-14**, littlefs `v2.11.3` vendored with per-file SHA-256 and a tested configuration) and `OI-LFS-02` (§7.1.2 evaluation + a falsified power-loss injection test), **which inherits the blocking status** | FW + Quality | **`OI-LFS-02` remains BLOCKING for any claim of power-loss atomicity — closing `OI-LFS-01` moved that status, it did not lift it** |
| ~~**OI-NVRAM-13**~~ | ✅ **CLOSED 2026-09-13 (Rev 2, D-24, §3.3.1.5).** 32-byte record (eight per 256-byte program unit, none straddling), 49,152-byte file, 1,536 rows, ~19 per socket at 80 sockets. Assumption stated as required: one row per module per session — 19 days of margin at home cadence, **1.6 days at clinic cadence**, stated rather than rounded away, and bounded by §6.3's degrade-to-totals policy rather than by a failure | — (closed) | — |
| **OI-NVRAM-14** | CLAUDE.md §3 still names **ATtiny402** as the on-module MCU, which `NP-HW-HEXTILE-001` §6.2 identifies as the retired design's part (10-bit ADC) and replaces with ATtiny426/427-class. `NP-MOD-ID-001` §5.2 sizes Map 4 at exactly 128 B against the latter, with zero slack. Correct the CLAUDE.md bullet | Systems | Map 4 storage budget |
| **OI-NVRAM-15** | `NP-MOD-ID-001` is **DRAFT**, *"pending principal approval and the two BLOCKING open items in §9"*. This document builds on MODID-4/5/6 as though settled. If the odometer, the ref derivation or the coarsening rule changes, §6 and §8 change with them | Principal | This document's §6 and §8 |
| ~~**OI-NVRAM-16**~~ | ✅ **CLOSED 2026-09-27 (Rev 5, #444).** `np_cfg_store_journal_read_rows()` steps by the length a caller-supplied check returns, and `np_map3_row_len()` is Map 3's check, the same row test `np_map3_scan()` makes. `np_cfg_store_tests` case 9 reads a journal of v1 rows with a 40-byte v2 row among them back whole through the real store. The fixed-length reader on the same file must lose rows, and a reader clamped to 32 bytes fails the case. `check-lfs-caller-rules.ts` R5 governs both readers (§7.2.1). ~~**`np_cfg_store_journal_read()` cannot read a journal whose rows differ in length.** It takes one fixed `rec_len` and runs the per-record check on each `rec_len`-byte slice. D-24 budgets a future 40-byte row and D-25 makes the row self-describing, but the first version that grows the row would have every v1 row after the first read back misaligned through the store. **Before any Map 3 version grows the row, either the store gains a variable-length journal read that steps by a caller-supplied length, or rule 2's converter rewrites the file.** `check-lfs-caller-rules.ts` R5 governs who may call the reader, and a new read path must be added under it. Not a defect at version 1, where every row is 32 B~~ | — (closed) | — |
| ~~**OI-NVRAM-17**~~ | ✅ **CLOSED 2026-09-28 (Rev 9, #444, §3.4.1).** `np_hub_control_app_main()` calls `np_factory_reset_boot_check()` before `np_log_backend_init()`, `np_session_count_load()`, `np_cvfs_init()` and `np_cons_init()`. It opens UHDR and SHDR only when `np_factory_reset_boot_permits_data()` says so (`NONE`, `RESUMED`). On `RESUME_FAILED` or `UNKNOWN` it calls `np_log_backend_refuse()` and `np_cvfs_init_unpersisted()`, and neither partition is touched. `scripts/check-hub-bringup-order.ts` gains the six orderings and a when-present rule that `np_cfg_store_bind()` precedes the check. Wired ahead of #340 rather than after it: until the store is bound, the check reads `UNKNOWN` and every boot refuses, which is fail-closed. **Residual:** the answer is only `UNKNOWN` until #340 binds the block device (`OI-LOG-05..07`, `OI-HEXMAP-01`), and `OI-NVRAM-19` | — (closed) | — |
| ~~**OI-NVRAM-18**~~ | ✅ **CLOSED 2026-09-28 (Rev 10, #444, D-31).** Resolved in the owning document, `NP-MOD-ID-001` §5.3.1 (MODID-4a), as this item asked. The hub decides that a mate happened, from its previous observation of the socket, and never from a first observation. `U1` writes the increment at once, at most once per power-up. So a mate no longer depends on a completed session. The candidate named here, a `U1` write on every power-up, was rejected, because a hub power cycle powers up every seated tile and would count it as a mate. The count is a lower bound: re-seating in the same socket while the hub is off, a clamp release that leaves contacts made, and a break too short to reset `U1` are not counted, and §5.3.1 names each one. No field is added and no mate-limit threshold is set. Found in passing: `distinct_socket_count` has no computation either (`NP-MOD-ID-001` `OI-MODID-09`). Original text: ~~`mate_cycles_observed` has no specified increment point. `NP-MOD-ID-001` §5.2 carries the field, and §5.3's cadence writes Map 4 only at session end. A module seated and removed without a completed session therefore never records that mate, and that insert-and-remove handling is what the ≥500-cycle contact rating counts. Not an input to §7.4's model, so outside `OI-NVRAM-06`. Resolve in `NP-MOD-ID-001` §5.3, which this document does not re-open (§1). One candidate is a `U1` write on power-up after mating, which the four-slot rotation already protects against extraction. Not decided here~~ | — (closed) | — |
| ~~**OI-NVRAM-19**~~ | ✅ **DECIDED and CLOSED 2026-09-28 (Rev 9, principal: option b).** *Raised:* a boot that refused UHDR and SHDR still admitted a stimulation session, which wrote no UHDR record, no SHDR dirty-session marker (`OI-FMEA-09`) and no session end. Until #340 that was every boot. *Options:* (a) accept, since stimulation safety belongs to the safety MCU; (b) refuse sessions on a refused boot; (c) refuse only on `RESUME_FAILED`. **Chosen: (b).** `np_runner_load()` returns the new `NP_HUB_ERR_BOOT_REFUSED` when `np_log_backend_admits_sessions()` is false. That is checked first, before the lease, and is gated by `check-hub-bringup-order.ts` rule 3. **Accepted consequence:** until #340 binds a block device, no boot admits a session. No target image can run one before the platform layer exists anyway, and afterwards a broken Config binding fails loudly. A log fault after a permitted boot is unchanged | — (closed) | — |
| ~~**OI-NVRAM-20**~~ | ✅ **CLOSED 2026-11-25 (Rev 18, D-35 to D-37).** Map 3 is `NP_CFG_FILE_MODULE_USAGE`, one 64-byte row per module in one fixed 12,288-byte record (§3.3.1.6), now holding every field of §8.5: sessions present and used with their time, faults per code, mate cycles, `dose_accum`, `throttle_count`, `fault_flags` and `detail_lost` (`REQ-FWHUB-93` (f) to (i)). An accessory port counts a mate when its occupant changes while the hub runs; a tile's dose and throttle are added at session end. §4.2(b), §6, §6.3 and §7.2 are revised: a download is a snapshot read, a full table gives up a row and marks `detail_lost`, and the record has one length so a later version is read and carried through. Who sees which field is §8.5's new table (`dose_accum` and `throttle_count` reach the fleet only under the opt-in; nothing new is uploaded). The journal codec and its test are removed; `map3.jrn` is `OI-NVRAM-21`. **Not built here:** a tile's mate and `fault_flags` bit 0 need the inventory poll (`OI-HEXMAP-02`); the snapshot's wire command and Map 2 are the app's. *Was:* **PARTLY BUILT (2026-11-24): the per-module totals of sessions present and used, their time, and faults per code are `NP_CFG_FILE_MODULE_USAGE` (`REQ-FWHUB-93`, §8.5 amendment); the rest below is still owed.** **Build Map 3 as the §8.5 running-total row** (raised Rev 16, from the principal's ruling on `OI-PRIV-38` (NP-PRIV-ANALYSIS-004), 2026-11-23). Replace the per-session layout of §3.3.1.5 and `np_map3_record.{h,c}` with one upserted row per module; revise §4.2(b), §6 (sync becomes a snapshot read; the app computes differences), §6.3 and §7.2 to match; keep `dose_accum` and `throttle_count` out of any fleet upload unless `NP-MOD-ID-001` §7's opt-in is granted. No row has ever been written, so there is no migration. | Firmware | Map 3's owner |
| ~~**OI-NVRAM-21**~~ | ✅ **CLOSED 2026-11-26 (Rev 20): removed.** `NP_CFG_FILE_MAP3` (`map3.jrn`, never written), the store's `TAIL_ADDITIVE` policy, `np_cfg_store_journal_append()`, `np_cfg_store_journal_read()`, `np_cfg_store_journal_read_rows()` and their record-check types are gone, with the tests that existed only for them: the store's `L-4` sweep, `np_cfg_store_tests` case 9, and the raw `L-4` sweep and its truncate-and-rebuild falsification in `np_lfs_powerloss_tests`. The churn bound and the two Config sweeps in `np_lfs_log_instance_tests` (512-byte RMW, no-op erase) used the journal only as a workload and now run on the store's replicated write. `NP-SOUP-LFS-001` agrees: `L-4` retired in place; `check-lfs-caller-rules.ts` R4 narrowed to every Config file under `REBUILD` or `REPLICATED`, R5 and R6 retired. Raised from it there: `OI-LFS-15` (does `REQ-LFS-01`'s guard reach Map 3's `REPLICATED` record). `NP-FW-HUB-001` retires the journal clauses of `REQ-FWHUB-51` and `-53`. *Was:* **`map3.jrn` and the Config store's journal API have no consumer** (raised 2026-11-25, from `OI-NVRAM-20`). Map 3 is the upserted record (D-35), so `NP_CFG_FILE_MAP3` (tail-additive, never written), `np_cfg_store_journal_append()`, `np_cfg_store_journal_read()` and `np_cfg_store_journal_read_rows()` serve only their own tests. They were kept because removing them changes `NP-SOUP-LFS-001`'s caller rules (`REQ-LFS-01`, `check-lfs-caller-rules.ts` R4 to R6 pin the journal) and the store's power-loss sweeps, which use the journal as a fixture. Decide: remove the file and the API (and narrow R4 to R6 to "no tail-additive file"), or name the record that needs them. **Closes when** the store, the gate and `NP-SOUP-LFS-001` agree on one answer | Firmware | Nothing: no production code reads or writes the file |
| **OI-NVRAM-22** | **Three Map 3 choices have no derivation** (raised 2026-11-25, from `OI-NVRAM-20`; `docs/status/unjustified-choices.md`). (a) UC-208: which row a full table gives up (fewest sessions present, nothing pending). (b) UC-213: the record length, 12,288 B, which sets the room a later row has to grow (up to 83 B). (c) UC-214: `dose_accum` is one sum over a tile's wavelengths in mJ/cm², because §8.5 names one field. *Worked Rev 19: none of the three is derivable from a fixed bound or citable from an external source.* The table's size is derived (147 = 128 socket addresses + 19 hub slots, §3.3.1.6) but which row it gives up is not; the 4 KiB multiple of the record length is not a medium bound (§3.3.1.6); and (c)'s former closing condition cannot be met, because the `NP-MOD-ID-001` §7.4 model uses drive time and §7.2 there keeps dose out of SHDR even under the opt-in, which also contradicts §8.5's fleet column for `dose_accum` (callout in §8.5). **Closes when** (a) the fleet upload is built (`NP-FW-HUB-001` §2.5) and consecutive uploads show how often a module's totals restart at zero with `detail_lost` set (a module given up and later returned, the loss the rule causes), and the rule is checked against that; (b) the first version that grows the row shows whether 19 bytes was enough, or Firmware fixes the room from a list of fields a later row will carry; (c) the principal answers `docs/human-input/oi-nvram-22-dose-accum-wavelengths.md` (Q1: does `dose_accum` reach the fleet; Q2: one sum, one total per wavelength, or dropped). **Rev 21: (c) CLOSED by the principal's ruling, 2026-11-26 (§8.5, D-38):** Q1, `dose_accum` never reaches the fleet; Q2, dose is one total per wavelength, for the person (UC-214 retired). The item stays open for (a) and (b). The ruling also names growth that does not fit (b)'s 19 bytes: emitter use per channel needs 20 B even with `dose_accum` off the row (§3.3.1.6, `OI-NVRAM-23`), which is evidence for (b) and does not close it | Firmware ((a), (b)) | Nothing today. No fleet path for dose is ever built (D-38) |
| **OI-NVRAM-23** | **Build emitter use per channel and dose per wavelength** (raised Rev 21, from the principal's ruling of 2026-11-26, §8.5, D-38). (1) **Emitter use, SHDR:** each module's Map 3 row gains, per emitter channel (a tile has at most three, one per wavelength, `NP-HW-HEXTILE-001` §4.2; **accessory ports are in scope**, the principal, 2026-11-26: the intranasal probe has four, the goggles an unknown number until `OI-FWHUB-35` says what their zone bits mean), a running total of on-time and of the sessions in which the channel was switched on and off, 32-bit, stored whole, added with saturation (`OI-UINT16-01`). The fleet schema carries them only from opted-in helmets within the characterisation window until the `NP-MOD-ID-001` §7.4 review gate sets what general consent covers (D-39, `OI-NVRAM-28`); `DOSE-01` and `ci/test_shdr_schema.py` are never relaxed. (2) **Dose per wavelength, UHDR:** one running total per wavelength per module that meters dose (tiles; the intranasal probe reads only an uncalibrated proxy per side today) for the person's app only, on the UHDR side, because D-3 keeps UHDR-class data out of Config: a record in the UHDR partition, or totals the app builds from the UHDR session files (F2). Then the `dose_accum` sum leaves the Config row, and `np_mu_dose_mJ_cm2()` and its UC-214 comment go with it. (3) **First decide the room:** with `dose_accum` gone, (1) needs 20 B per tile row and 28 B for the intranasal probe against the row's 19 B, and more for the goggles, and per-module temperature bands (`OI-NVRAM-24`) add 4 B each (§3.3.1.6). A longer record, fewer rows or narrower fields each changes D-36 or UC-213; nothing has shipped, so none needs a migration. Running totals only throughout: no per-session row, no ordinal, no clock (D-18). **Closes when** (1) and (2) are written at session end and tested, (1) reaches the fleet schema on D-39's terms, and no dose remains in Config | Firmware | Nothing built depends on it. The goggles' channel count waits on `OI-FWHUB-35` |
| **OI-NVRAM-24** | **The working-environment record: time in 5 °F temperature bands, and humidity with no sensor** (raised Rev 21, from the principal's ruling of 2026-11-26, §8.5). **Ruled:** temperature is kept as time in 5 °F bands, initially, as SHDR running totals that reach the fleet only from opted-in helmets within the characterisation window until the `NP-MOD-ID-001` §7.4 review gate rules (D-39); the bands may be coarsened once a first dataset is analysed (UC-215). **Not set, and to be decided here:** the lowest and highest band and so the band count (documented limits that bear on it: the 42 °C scalp limit, the 62 °C hardware throttle and `NP-ENV-001` §2.1's −20 to +60 °C survival envelope), where band edges fall, which sensor (tile NTCs, hub NTCs, or an ambient NTC, `NP-ENV-001` §5), whether per module or per helmet, and where the totals live. **Before tile NTCs are used:** a tile's NTC sits against the scalp and its per-socket reading in a session is UHDR (F7, `data-architecture-detail.md` §5.1), so a band total from it needs `CLAUDE.md` §5.1 rule 1's positive demonstration that it carries no user biology; otherwise the source is one off the scalp. **Humidity:** no sensor exists, and `NP-ENV-001` §5 recommends none; the cost of temperature and humidity sensing is [#1086](https://github.com/stevehickman/NeuroPulse/issues/1086). **Closes when** the range, sensor, home and §5.1 demonstration are settled with any bound derived (`CLAUDE.md` §18), the totals are written and tested, and humidity is specified with a sensor or recorded as not wanted after #1086. **Worked Rev {NP-FW-NVRAM-001 to}, left open (§8.5.1):** the range is derived from `NP-ENV-001` §2.1's survival envelope on a whole-5 °F grid: 29 bands from −5 to 140 °F, two saturating end bands and a count of seconds with no valid reading (UC-215); one record per helmet in Config, 132 B, outside the Map 3 row; one sample a second of powered time (UC-218); written at each session end and hourly (UC-219). **The sensor is not settled:** tile and sense-domain NTCs are scalp-coupled and excluded (F7); the hub NTC is read only by the safety MCU, reads the hub's self-heat, has unmeasured exposure to the head and a 1 to 110 °C whole-degree table; no ambient NTC exists (`OI-ENV-05`). Put to the principal in `docs/human-input/oi-nvram-24-working-environment-sensor.md` (Q1 sensor, recommending a dedicated ambient NTC; Q2 powered or session time; Q3 the range). **Fleet columns (specified, not added):** `env_band_<lo>_<hi>f_s` for the 29 bands, `env_band_below_m5f_s`, `env_band_140f_up_s` and `env_unread_s`, each a running total in seconds, admitted on D-39's terms, with `ci/test_shdr_schema.py` and `DOSE-01` unchanged | Firmware, with the Privacy Lead for the F7 demonstration | Nothing built. The record waits on the principal's Q1 and on the sensor (#1086, `OI-ENV-05`) |
| ~~**OI-NVRAM-25**~~ | ✅ **CLOSED Rev 21 (the principal, 2026-11-26).** (a) Per emitter means per drive channel; a session is one in which the channel was driven, not individual pulses; emitters in accessory ports are counted. (b) Both of the warranty owner's consents cover the new fields; the characterisation opt-in is a time-limited form of the same consent that turns from yes to no at the window's end, when the owner is prompted for general SHDR consent (D-39, `REQ-SHDR-12`). *Refined the same day (D-39, `OI-NVRAM-27`):* general consent covers what predictive maintenance needs, set by the §7.4 review gate, so until it rules the new fields flow only from opted-in helmets within the window. Follow-ups: `OI-NVRAM-26` (the prompt), `OI-NVRAM-27` (what "generally" reaches; an unanswered prompt). *Was:* **Two readings of the 2026-11-26 ruling, awaiting the principal** (raised Rev 21, `docs/human-input/oi-nvram-22-dose-accum-wavelengths.md` §7). (a) **Per emitter means per drive channel**, at most three per tile, because the hub never drives a single site; and a session count is the sessions in which the channel was driven, not pulses. (b) **Which warranty-owner consent gates the new SHDR fields:** the warranty owner's SHDR consent at registration, per helmet (`SHDRUploader`, `REQ-SHDR-08`), or `NP-MOD-ID-001` §7.5's separate characterisation opt-in, which the warranty owner also grants. Under §7.3 the §7 fields stop when its window closes; §7.5 holds that folding an 80-socket duty map into warranty registration would be bundled consent, and per-channel totals per module are coarser than that map but a module usually stays in one socket. `throttle_count` rides on §7's opt-in today and is unchanged until this is answered. No user-level opt-in is involved either way. **Closes when** the principal confirms or changes each | — (closed) | — |
| **OI-NVRAM-26** | **Build the warranty owner's two prompts for general SHDR consent** (raised Rev 21, D-39, `REQ-SHDR-12`; extended to two prompts Rev 22). **(1) When a helmet's `NP-MOD-ID-001` §7 window closes** (its record budget is spent or the programme ends, `NP-FW-EMMC-002` §H.3.4) and that consent turns from yes to no: ask about general SHDR consent, showing what it covers then (the baseline, until the §7.4 gate rules). **(2) When an app update carries the review gate's result** (the principal's broadcast to all warranty owners): ask again, showing general consent as the gate set it, with what is added named in plain words. At both: a fresh question, default no change; a dismissal, a timeout or no answer keeps the existing consent exactly at its current content level; the end of the window is its own state, distinct from a revoke (`REQ-SHDR-08` (c)) and from never asked; never re-enrol characterisation (`REQ-SHDR-09` (b)); the warranty owner's page (`OI-PRIV-20`); `SHDRUploader` takes no reference to `ConsentStore` (`REQ-CONSENT-01`). Needs: a way for the app to learn the window has closed (the hub decides it in firmware), the app update that carries the gate's result (`OI-NVRAM-28`), and the text in all eleven locales (`CLAUDE.md` §17). **Closes when** both prompts are built and `REQ-SHDR-12` has tests | The owner of the warranty owner's page (`OI-PRIV-20`) | Nothing built; no window has opened, because the hub side of the fleet upload is not built (`NP-FW-HUB-001` §2.5) |
| ~~**OI-NVRAM-27**~~ | ✅ **CLOSED Rev 22 (the principal, 2026-11-26).** (a) The §7.4 review gate decides for §7.2's per-socket fields too, as for Map 3's; nothing changes before it rules. (b) Silence leaves the consent as it was: an unanswered prompt keeps it at its current content level (D-39, `REQ-SHDR-12`). *Was:* **Two questions the 2026-11-26 consent ruling leaves, for the principal** (raised Rev 21, D-39). (a) **Does "generally" reach beyond Map 3?** The ruling is applied to Map 3's totals and `throttle_count`. Read widely it would also admit `NP-MOD-ID-001` §7.2's per-socket, per-session fields (`emitter_on_seconds`, `thermal_seconds_over_threshold`, `throttle_event_count`, `peak_ntc_celsius`) and `NP-FW-EMMC-002` §H's accelerometer programme under general SHDR consent, past their firmware stop at the window's end (§7.3, §H.3.4), and §7.5 argues that a per-socket duty map under warranty registration is bundled consent. Until answered they stay as they are. (b) **What does an unanswered expiry prompt do to the consent the owner already gave?** `REQ-SHDR-09` (a) enrols only a helmet whose general consent is open, so at the window's end that consent is always open: leave it as it was, or let it lapse until the owner answers. Until answered, an unanswered prompt changes nothing (`REQ-SHDR-12` (b)). **Closes when** the principal answers each | — (closed) | — |
| **OI-NVRAM-28** | **Link general SHDR consent's ongoing content to the §7.4 review gate, and build the post-analysis notice** (raised Rev 22, D-39). The principal: general consent covers what predictive maintenance needs on an ongoing basis, which is known only once the characterisation data is analysed. (1) **The content set:** when the `NP-MOD-ID-001` §7.4 gate rules, record which fields general consent then covers (as much as characterisation collected, the baseline only, or between), as a versioned set the device and app can name; until it rules, the baseline. A helmet sends no field above the set its owner agreed to. (2) **The notice, ruled by the principal (2026-11-26):** "Analysis done would be broadcast through an app update mechanism to all warranty owners". A broadcast, the same to every warranty owner, carrying the content set the gate fixed; not targeted and needing no per-owner channel (the pattern of `CLAUDE.md` §6.3). Each owner's app then shows prompt (2) (`OI-NVRAM-26`). **Left to build, not designed here:** how the content set ships in an app update, and how the app knows to show prompt (2) once. (3) The fleet schema and `ci/test_shdr_schema.py` follow the set, and `DOSE-01` is never relaxed. **Closes when** the set is recorded and enforced on the helmet and in the schema, and the app update that carries it is built and prompt (2) is shown once per owner | Firmware and the Privacy Lead ((1), (3)); the owner of the warranty owner's page, `OI-PRIV-20` ((2), with `OI-NVRAM-26`) | The extended content's path after the window; prompt (2) |

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

**Open items.** Fifteen at Rev 1, of which three were blocking. **Rev 2 leaves one blocking, and it
is no longer this document's.** `OI-NVRAM-01` is decided (§3.3.1) and downgraded to a documentation
action — `ECR-EMMC-001`, three clause changes against a `.docx` — so **Map 3 is implementable
against this document now**; `OI-NVRAM-02` and `OI-NVRAM-13` are closed; `OI-NVRAM-03` is decided.
`OI-NVRAM-12` is superseded by `NP-SOUP-LFS-001`, and continues as `OI-LFS-01`, which **is** still
blocking any reliance on §4's atomicity claims — because the component under them is not in the tree.
**(Updated 2026-09-14: `OI-LFS-01` is CLOSED — `NP-SOUP-LFS-001` Rev 2 pins littlefs at `v2.11.3` and
vendors it. §4's atomicity claims are exactly as unverified as before; the blocking status passed to
`OI-LFS-02`, the §7.1.2 evaluation plus the NeurOne power-loss injection test. A vendored component
is not a tested one — `NP-SOUP-LFS-001` §7.5.)**

**(Updated 2026-09-14, second: `OI-LFS-02` is CLOSED, and §4 is unblocked to a stated depth.)**
`NP-SOUP-LFS-001` Rev 3 performs the §7.1.2 evaluation (§11) and the power-loss injection test (§12).
Two results land directly on this document. **D-5's tail-additive Map 3 journal was swept** — 168
interrupted runs, one flush per record, 0 violations: a torn write cost the record in flight and
never an earlier one. **And §4.2's atomicity for the `"NPMP"` blob was swept as write-temp,
sync, close, rename** — 198 interrupted runs, 0 violations — with the **remove-then-rewrite**
ordering swept alongside it and required to fail, which it did at 192 of 198 cut points. So §4.2's
atomicity is a property of **that ordering**, which the `OI-LOG-05..07` glue must use; littlefs does
not supply it. Three things §4 still may not claim: the eMMC behind the XTS layer was not
interrupted (`OI-LFS-07`); a post-power-loss hang is invisible to a sweep (upstream #1211); and
**upstream #1210 can silently drop a whole Config file with its data's CRC still valid**, which is
`OI-LFS-08` and which no CRC in this document would see. `REQ-LFS-01` is why Map 3 is not the worst
case of that; `ukmd.rec` is.

> **Update 2026-09-24 (`NP-SOUP-LFS-001` Rev 4 §13).** The glue this section says §4.2 depends on
> now exists for the Config instance as `np_cfg_store`, and it does **not** use write-temp-then-
> rename: a replacement is an in-place `O_TRUNC` rewrite that littlefs commits atomically at close,
> re-swept for `L-3` (186 runs, 0 violations) — because rename is a delete, and create/delete churn
> is #1210's trigger. `OI-LFS-08` is closed by that bound and by holding `ukmd.rec` as two copies in
> two metadata pairs, repaired from its twin; #1210 itself did not reproduce, so it is bounded, not
> shown prevented. Map 3 is `NP_CFG_POLICY_TAIL_ADDITIVE` in the store's file table, and
> `REQ-LFS-01` is now a CI gate. **A new caveat for §4:** the Config instance's `prog_size` 256 is
> below the 512-byte XTS unit, and under a read-modify-write tear the Map 3 journal lost a durable
> record (`OI-LFS-10`). *Resolved the same day (`NP-SOUP-LFS-001` Rev 5 §13.8): the Config
> instance now programs 512 bytes at a time, and the same sweep loses nothing.*
`OI-NVRAM-05` remains open and still blocks `NP-MOD-ID-001` §10's factory-reset rotation test; it is a
factory-reset correctness question rather than a storage-layer one, and Rev 2 does not touch it.

> **Update 2026-09-26 (Rev 3, #444).** Both CI checks this section counted in §10.3 now exist and are
> falsified: `np_map3_record_tests` (`OI-NVRAM-08`, over the new `np_map3_record` codec) and
> `check-map2-shdr-boundary.ts` (`OI-NVRAM-09`). `RISK-NVRAM-01` and `-02` move to MITIGATED.
> `OI-NVRAM-05` is closed. The principal chose option A (D-27), and the durable marker and boot
> check are implemented and tested (§3.4.1). Two new items: `OI-NVRAM-16` must land before Map 3's
> row can grow, and `OI-NVRAM-17` is the boot check's bring-up call site. **Still not built:** Map 3's owner (sync, watermarks, the §6.3
> journal-full policy) and the `np_hexmap_nvram_*` binding (`OI-HEXMAP-01`, waiting on #340's block
> device). *(Rev 11: the binding is built and `OI-HEXMAP-01` is closed, §4.2.1. Its call sites wait
> on `OI-HEXMAP-02` and #340.)*

> **Update 2026-09-27 (Rev 5, #444).** `OI-NVRAM-16` is closed: `np_cfg_store_journal_read_rows()`
> with `np_map3_row_len()` reads a journal whose rows differ in length, and `np_cfg_store_tests`
> case 9 checks it through the real store (§7.2.1). Map 3's row can now grow without a converter.

> **Update 2026-11-25 (Rev 18, `OI-NVRAM-20` closed).** The two-records summary above still holds in its principle
> and not in its mechanism: Map 3 is no longer appended. It is one upserted record of running totals per module,
> replaced whole and kept in two copies (§4.2(b)), read whole at a download (§6.1), and its version rule is the
> record's form of tail-additive (§7.2, D-36). The journal codec is removed; `map3.jrn` is `OI-NVRAM-21`.

> **Update 2026-11-26 (Rev 20, `OI-NVRAM-21` closed).** `map3.jrn` and the Config store's journal API are removed.
> The store keeps two policies, the rebuild cache and the replicated record, and no Config file is tail-additive.

---

## 15. Revision history

| Rev | Date | Author | Description |
|---|---|---|---|
| 1 | 2026-08-27 | NeurOne Firmware + Data Architecture | Initial release. Specifies the hub NVRAM HAL under the four-map module record — the resolution of `OI-HEXMAP-01` — with twenty decisions (D-1…D-20), ten risk rows and fifteen open items. **Corrects six claims in its own scoping brief:** Map 4's substrate is not open and is not inside `OI-HEXTILE-06` (which is a photodiode-population decision) — `NP-HW-HEXTILE-001` D-3 and `NP-MOD-ID-001` MODID-4 already specify it, at **$0.00 BOM**; the module-change power cut is a per-cluster `SAFE_EN[n]` emitter-rail cut, not a hub power loss, which makes the atomicity requirement broader rather than narrower; `SEAT#` cannot supply a last-gasp write window because it has no firmware consumer, sits at Class B, and its pre-break interval is undimensioned; the version-bump window closes at the first unsynced Map 3 row, not at first ship, because a cache and a record need opposite policies; and two documents cited as house-style precedent are not on `main` (`NP-FEAS-PBMCH-001` in PR #292, `NP-FW-BENCH-001` in PR #297 — an initial reading that the former did not exist at all was itself wrong, and is corrected in §1), so the in-force no-wall-clock precedent used here is `NP-FW-EMMC-002` §H.3.1–H.3.2. **Four defects found on the store itself:** `EMMC-CFG-02`'s raw journal area has no address space inside a LittleFS instance owning all 16 MiB (`OI-NVRAM-01`, blocking); `NP-FW-EMMC-002` §C.3's offset-`0x1000` UKMD address has the same problem; the two SNVS power-loss recovery flags can only be warm-reset flags on a device with no `VBAT` rail (`OI-NVRAM-05`); and LittleFS is the only Class B SOUP item that is neither vendored, pinned nor anomaly-evaluated (`OI-NVRAM-12`, blocking). **Privacy finding:** a per-window delta becomes a timestamped count in the hands of the only party that can date the window's ends, so no sync-boundary field or delta may be uploaded — this walks around `TIME-01` rather than breaching it, and `NP-MOD-ID-001` §7.5.1.1's consent model rests on exactly that absence. No code changed with this document. |
| 2 | 2026-09-13 | NeurOne Firmware Engineering | **Resolves the store defects Rev 1 found, so Map 3 can be built (Issue #339).** Rev 1 recorded four defects and deliberately stopped; `OI-NVRAM-01` was blocking Map 3, and a blocking item nobody may resolve is a design that cannot start. **D-21: the Config journal is LittleFS files, and `EMMC-CFG-02`'s raw-write region is void — its *justification* refuted, not outweighed.** At `EMMC-FS-01`'s `prog_size` of 256 B an append costs one 256-byte program inside the current block; a raw region cannot offer a smaller program unit, because the unit belongs to the eMMC and not to the filesystem. The cost the clause was aimed at is `np_module_map_persist()` rewriting 14,012 bytes to record a 16-byte fact, which **D-5 already removes** by giving Map 3 its own file — *the raw region was aimed at the right symptom and the wrong cause*. Two further grounds: carving the region shrinks `block_count` and so **relocates the UKMD record**, whose loss makes a user's UHDR permanently unmountable with no NeurOne-held second copy — free today, but the window closes at first **provisioning**, not first ship, the same boundary error §7.2 corrects for the version bump; and a raw region means re-implementing commit ordering, per-record CRC and wear levelling by hand at Class B, which is the wrong answer to `OI-NVRAM-12`. **D-22 closes `OI-NVRAM-02`** — the UKMD record becomes a named file and `NP-FW-EMMC-002` §C.3 is corrected here, provably safe because `np_uhdr_key.c` has always reached it through a HAL and never through the offset. **D-23 decides `OI-NVRAM-03`** by widening rather than forbidding, as Rev 1 predicted. **D-24 closes `OI-NVRAM-13`**: 32-byte record (eight per program unit, none straddling), 49,152-byte file, 1,536 rows, ~19 per socket, against a stated assumption of one row per module per session — 19 days of margin at home cadence and **1.6 days at clinic cadence, stated rather than rounded away**, bounded by §6.3's degrade-to-totals-plus-`detail_lost` rather than by a failure. **`OI-NVRAM-12` is superseded by `NP-SOUP-LFS-001` Rev 1**, which brings LittleFS under SOUP management and returns two findings to this document: it is **not in the tree at all** (two `lfs_` tokens in `firmware/`, both comments), so §4's guarantees rest on a nominated rather than integrated component; and **§9's Class B argument is a property of `np_module_map`'s reject-and-rebuild policy rather than of LittleFS** — the policy §7.2 specifies Map 3 to invert — now constrained by `REQ-LFS-01`. Per `NP-CONV-001` §7 every Rev 1 finding in §3.3 is retained verbatim and §3.3.1 sits after it. `NP-FW-EMMC-001` is not edited (it is a `.docx`, `OI-CONV-04`); its three clause changes are raised as `ECR-EMMC-001` with replacement text. **No code changed with this revision.** Rev 1 → 2. |
| 3 | 2026-09-26 | NeurOne Firmware + Data Architecture | **The two invariants get CI checks, and the SNVS claims are corrected (GitHub #444).** **`OI-NVRAM-08` closed:** the Map 3 row is now code (`np_map3_record.{h,c}`), and `np_map3_record_tests` checks that a journal written under version *n* loses no record under *n+1*. Across an upgrade, appended newer rows, a rollback, a torn tail and the D-24 bound, it is falsified in both directions against four rule-breaking readers (§7.2.1). **D-25:** §3.3.1.5's two padding bytes become `len` + `ver`, so the scan steps by the row's own length and accepts every version. **`OI-NVRAM-09` closed:** `scripts/check-map2-shdr-boundary.ts` (`tooling-ci.yml` `map2-shdr-boundary`) checks that no file names Map 2 and SHDR together, and that no SHDR column or firmware SHDR identifier names a sync boundary, watermark, ordinal or per-window delta (§8.3.1). **D-26** is the Map 2 naming rule it depends on. It is falsified by a hermetic self-test and by hand against the real tree. **`OI-NVRAM-05`:** every claim that `LPGPR1`/`LPGPR2` survive power loss is corrected in six firmware files and in `NP-FW-EMMC-002` Rev 4 §B.4/§D.6, including a comment calling the SNVS domain *"battery-backed"*. The anonymisation half needs nothing more. For the factory-reset half, §3.4.1 specifies three options. **The principal chose A (D-27), and it is implemented.** A replicated Config marker is written at R-3 before the first erase, a reset that cannot write it does not start, and `np_factory_reset_boot_check()` completes the reset on the flag, the marker or a Config with no filesystem. `np_cfg_store_mount()` now tells "no filesystem" from "unreadable", and the second never erases. **New:** `OI-NVRAM-16`, since the store's fixed-length journal read cannot read a grown row, and `OI-NVRAM-17`, since the boot check has no bring-up call site until Config is mounted (#340). `RISK-NVRAM-01`/`-02`/`-08` move to MITIGATED. `OI-NVRAM-04`, `-06` and `OI-HEXMAP-01` stay open. SW-02 code changed: the new codec and test, one ctest target (Class B 39 → 40), comment corrections in the bootloader and anon modules, and the factory reset's R-3 marker, boot check and `NP_RESET_ERR_MARKER` in `np_factory_reset`. It also adds `np_reset_marker.c`, and in `np_cfg_store` one file-table row and the mount-error distinction. **The factory reset's behaviour changed as D-27 specifies. No SW-01 file changed.** Rev 2 → 3. |
| 4 | 2026-09-27 | NeurOne Firmware Engineering | **R-3 reads the factory-reset marker back before the first erase (§3.4.1, D-27 unchanged).** From `NP-SOUP-LFS-001` §13.18 (`OI-LFS-13`): a Config write the store acknowledged can still be rolled back, and R-5's UHDR purge was the one step that acted on the marker's acknowledgement. `np_factory_reset_execute()` now calls `np_factory_reset_hal_marker_state()` after the write and starts only on `PRESENT`; anything else returns `NP_RESET_ERR_MARKER` with nothing erased. Tested and falsified in `np_factory_reset_tests` (a host medium that does not keep an acknowledged write) and `np_cfg_store_tests` (the real store, medium rolled back after the write). SW-02 code changed: `np_factory_reset.c`, its test HAL and tests, and `np_cfg_store_tests.c`. No SW-01 file changed. `NP-FW-EMMC-002` Rev 5 carries the step. Rev 3 → 4. |
| 5 | 2026-09-27 | NeurOne Firmware Engineering | **`OI-NVRAM-16` CLOSED (GitHub #444, §7.2.1).** `np_cfg_store` gains `np_cfg_store_journal_read_rows()`, a tail-additive journal reader that steps by the length a caller-supplied check returns (`np_cfg_record_step_fn`) and ends the valid prefix on 0 or on a length past the end. `np_map3_record` gains `np_map3_row_len()`, which factors out the single-row test `np_map3_scan()` already made, so the scan and the store step share one rule. `np_cfg_store_tests` case 9 appends a 40-byte v2 row among v1 rows through the real store and reads all ten back. It is falsified in both directions: the fixed-length reader on the same file must lose rows, and a reader clamped to 32 bytes fails five assertions. `scripts/check-lfs-caller-rules.ts` R5 matches both reader names, and its self-test grows to 31 cases. `RISK-NVRAM-01`'s residual is removed. SW-02 code changed: `np_cfg_store.{h,c}`, `np_map3_record.{h,c}` (a refactor of the scan, with behaviour unchanged, as `np_map3_record_tests` shows), `np_cfg_store_tests.c` and its CMake sources. No new ctest target; the executed-line floors of `np_cfg_store_tests` (2,644 → 2,816) and `np_map3_record_tests` (260 → 263) are re-measured in `ci/host-test-floors.txt`. No SW-01 file changed. `OI-NVRAM-04`, `-06`, `-17` and `OI-HEXMAP-01` stay open. Rev 4 → 5. |
| 6 | 2026-09-27 | NeurOne Firmware Engineering | **`OI-NVRAM-04` CLOSED (GitHub #444, §4.3.1): re-homed, not dimensioned.** Under CLAUDE.md §18, nothing in this document requires the `SEAT#` pre-break window's duration. D-8 does not rest on the number's absence either: the hub loses no power on extraction, so no Map 3 write has a deadline. The tile cannot observe `SEAT#` (`NP-HW-HEXTILE-001` §7.2), and the I2C command path is in contact group 3, whose break ends the window, so no Map 4 write can use a window of any duration. No stagger figure and no extraction velocity is written. The stagger dimension is required by `NP-HW-HEXTILE-001` §7.3's own mating order and `SEAT#`-last guarantee, and no document tracked it, so it is raised there as `OI-HEXTILE-28` (Rev 18), coupled to `NP-DRV-SHELL-002` `OI-SHELL2-12` (#437). New §4.3.1; §4.3 and D-8 annotated. No code changed. `OI-NVRAM-06`, `-07`, `-17` and `OI-HEXMAP-01` stay open. Rev 5 → 6. |
| 7 | 2026-09-27 | NeurOne Firmware Engineering | **`OI-NVRAM-06` CLOSED (GitHub #444, §6.5.1): quantified, accepted and documented.** A tile pulled mid-session misses its session-end Map 4 write, so each extraction loses at most one session's contribution, and Map 4 becomes a lower bound that never goes backwards. The hub keeps the session (§4.3.1). The loss does not reach `NP-MOD-ID-001` §7.4's model: Map 4 has no socket field, so the model's per-socket exposure is the hub's, and Map 4 enters only through a carried-in baseline that MODID-6 already rounds down by up to 249 sessions. **D-28** makes the condition a rule: §7.2's per-session values are accumulated by the hub during the session, never read from `U1` at session end or differenced from Map 4. **D-29:** the interrupted shortfall would otherwise raise §6.5's *Map 4 < Map 2* anomaly on ordinary handling. So the hub sets Map 3 `fault_flags` bit 0 (`NP_MAP3_FAULT_UNSEATED_AT_CLOSE`, defined in `np_map3_record.h`; the row layout and version are unchanged), and the comparison excludes flagged rows. The flag is never uploaded (§8.1). **`OI-NVRAM-18` raised:** `mate_cycles_observed` has no specified increment point, so a mate without a completed session is never recorded. It is to be resolved in `NP-MOD-ID-001` §5.3. §4.3 and §6.5 annotated. `OI-NVRAM-07`, `-17`, `-18` and `OI-HEXMAP-01` stay open. Rev 6 → 7. |
| 8 | 2026-09-28 | NeurOne Firmware Engineering | **`OI-NVRAM-07` CLOSED (GitHub #443, §5.3.1): the trajectory lacks cadence, not time.** `NP-MOD-ID-001` §7.4's covariates are use: exposure (hub-accumulated, D-28), presence and thermal history, none of which needs a clock. Idle-aging mechanisms act over months, and SHDR's `module_life` row already holds `module_manufacture_date` and a month-truncated `last_seen_month`, so calendar age is known to about a month within `TIME-01`. Only sub-month cadence is missing. It stays missing (D-18), and its physical effect is temperature, which the NTC measures. **D-30:** the §7.4 fit carries presence and calendar age beside exposure, because a rarely-driven socket otherwise shows idle aging as a socket effect. No field, clock or Map 3 change. §5.3's closing paragraph annotated; the link to `OI-EMMC2-13` corrected (that item is about threshold provenance and stays open). `NP-MOD-ID-001` §7.4 carries the note. `OI-NVRAM-17`, `-18` and `OI-HEXMAP-01` stay open. Rev 7 → 8. |
| 9 | 2026-09-28 | NeurOne Firmware Engineering | **`OI-NVRAM-17` CLOSED (GitHub #444, §3.4.1): the boot check has its bring-up call site.** `np_hub_control_app_main()` calls `np_factory_reset_boot_check()` before anything opens UHDR or SHDR or reads Config, and reads the result through the new `np_factory_reset_boot_permits_data()` (`NONE`/`RESUMED` permit, everything else refuses). A refusal calls the new `np_log_backend_refuse()` (no file opened, appends and flushes fail closed, `session_begin` returns `NP_HUB_ERR_STORE_IO`) and the new `np_cvfs_init_unpersisted()` (fault summary in RAM, no UHDR load or save). `scripts/check-hub-bringup-order.ts`: 4 → 10 ordering constraints plus a when-present rule (`np_cfg_store_bind()` before the check, for #340); self-test 11 → 25 cases. Until #340 binds a block device every boot reads `UNKNOWN` and refuses, which is fail-closed and documented. Tests: `np_factory_reset_tests` cases 19–20, `np_log_backend_tests` `test_refuse_opens_nothing`, `np_cvns_fault_summary_tests` `test_unpersisted_never_touches_uhdr`. Three mutants each caught. Host suite 55/55, and the sanitized leg passes on the four touched targets. Executed-line floors raised for the three touched targets. `RISK-NVRAM-08` residual updated. **`OI-NVRAM-19` raised and decided (principal, option b):** a refused boot admits no session. `np_runner_load()` returns the new `NP_HUB_ERR_BOOT_REFUSED` before the lease, `np_log_backend_admits_sessions()` is the flag, and gate rule 3 holds the order. SW-02 only. No SW-01 file changed. Rev 8 → 9. |
| 10 | 2026-09-28 | NeurOne Firmware Engineering | **`OI-NVRAM-18` CLOSED (GitHub #444, D-31): `mate_cycles_observed` has an increment point.** It is resolved in `NP-MOD-ID-001` §5.3.1 (MODID-4a), which is the document the item named. `U1` sees every mate, because `VCC_3V3` is in contact group 3, but it cannot tell a mate from the hub powering up. The hub can tell them apart, but only for mates it saw. So the hub counts a mate when a poll finds a UID where its previous observation of the socket (this power-on, or the valid inventory cache at boot) was empty or a different UID, and never from a first observation. `U1` writes the increment at once and honours it at most once per power-up, which makes a retry idempotent and bounds the writes. The count is a lower bound, and §5.3.1 names what it misses. No field, no upload and no event record is added (D-20). No mate-limit threshold or prompt is set: any prompt is an exposure count under CLAUDE.md §2.3 and needs its own design. `NP-MOD-ID-001` §5.2, §8.3, §9 (`OI-MODID-09`, `distinct_socket_count` has no computation) and §10 updated. §6.5.1 annotated. No code changed: no module firmware exists yet (`NP-HW-HEXTILE-001` `OI-HEXTILE-07`). `OI-HEXMAP-01` stays open. Rev 9 → 10. |
| 11 | 2026-09-28 | NeurOne Firmware Engineering | **`OI-HEXMAP-01` CLOSED (GitHub #444, §4.2.1, D-32): the module map's blob has a store, and a restored record waits for the poll.** `np_hexmap_nvram_read/write` move from platform traps to first-party code, `firmware/hub_control/src/np_hexmap_nvram.c`, over `np_cfg_store` (`npmp.bin`, `REBUILD`). The census goes 103 → 101, the way `OI-LFS-12` retired the session-count seam. Reads are content-checked by the new `np_module_map_blob_verify()` (magic, socket count, exact length, CRC-32, one CRC implementation), writes are the store's atomic in-place replacement, and a blob the map would refuse is never written. **`REQ-LFS-02` (NP-SOUP-LFS-001 §13.18.2) is cited and met as a module property (D-32).** A RAM-only `confirmed` flag, false after `load()`, gates all six inventory readers until `apply_poll()` sees the same UID in this boot. A different UID is re-inventoried, and no module gets the stored calibration. An ordering rule on the boot integration was rejected, because only its callers enforce it. Evidence: `np_module_map_tests` (the rule across all six readers; with the check reduced to `module_present`, 10 failures) and `np_cfg_store_tests` case 10 (end to end on the 80-socket 14,012-byte blob: reboot, swap, damage → empty map, refused write, and a power cut at every op of `persist()`: 105 cuts, 0 violations). `check-lfs-caller-rules.ts` lists the new file as a limit consumer (R6). **Not built:** the call sites, which wait on `OI-HEXMAP-02` (`inventory_fn`) and #340 (block device). §10.2 and §14 annotated. Rev 10 → 11. |
| 12 | 2026-11-03 | NeurOne Firmware Engineering | **§3.4.1 (`OI-NVRAM-19`): the sentence that said the runner compiles for ARM only is corrected.** `np_runner_load_tests` (`NP-FW-HUB-001` Rev 28) now runs `np_runner_load()` on the host and asserts that a refused boot returns `NP_HUB_ERR_BOOT_REFUSED` before the lease is claimed. `check-hub-bringup-order.ts` rule 3 still gates the order statically. No requirement changed. |
| 13 | 2026-11-21 | NeurOne Firmware Engineering | **§3.3.1.5, §7.2.1, D-33: Map 3 version 2 is a 64-byte row (the two counts carried whole as a tail, 24 bytes reserved), so no row straddles a 256-byte program unit; the file holds 768 version-2 rows; `np_cfg_store_tests` case 9 and `np_map3_record_tests` follow (`OI-UINT16-01`, the principal).** |
| 14 | 2026-11-21 | NeurOne Firmware Engineering | **§3.3.1.5, §7.2.1, D-25, D-33, D-34 (`OI-UINT16-01`, the principal, 2026-11-19): Map 3 has one layout, a 64-byte row at version 1 with 32-bit counts stored whole. The 32-byte row, the duplicate tail and the 0xFFFF ceiling are removed because no hardware has shipped and no row was ever written; D-33 is superseded. 768 rows, 9 per socket, margin 9 days home and 0.8 days clinic.** |
| 15 | 2026-11-23 | NeurOne Firmware Engineering | **§8.1: ordinal, dose_accum and throttle_count classified UHDR class, not written to Config (F1, F2, F8; OI-PRIV-38 (NP-PRIV-ANALYSIS-004) part 1); §3.3.1.5 and §8.1 say the Map 3 row is not to be written to Config until the principal rules what it keeps.** |
| 16 | 2026-11-24 | NeurOne Firmware Engineering | **§8.5 added: the principal's ruling on Map 3 (presence is wear and SHDR; use accumulates as running totals per module with no session link; dose and throttle reach the fleet only under NP-MOD-ID-001 §7's opt-in). §3.3.1.5's per-session layout is retired unwritten; §8.1 rows updated; OI-NVRAM-20 opened to build the new row.** |
| 17 | 2026-11-25 | NeurOne Firmware Engineering | **§8.5 amended (the principal, 2026-11-24): the per-module totals measure use as well as presence, with time present and time used, and faults per code, all SHDR as running totals; built as NP_CFG_FILE_MODULE_USAGE, not map3.jrn. OI-NVRAM-20 partly built.** |
| 18 | 2026-11-26 | NeurOne Firmware Engineering | **OI-NVRAM-20 closed: new §3.3.1.6 (the Map 3 row as built, 64 B, 147 rows, 12,288-byte record, with the arithmetic and which §4.2(b) rules are dropped and why); §3.3.1.5 marked retired in place; §4.2(b), §6.1 to §6.4, §6.5.1 (D-29), §7.2, §7.2.1, §8.1 and §8.5 revised for an upserted record (snapshot read, table-full policy, the record's version rule, who sees which field); D-35 to D-37 added and D-5, D-9, D-11, D-12, D-24, D-25, D-29, D-34 annotated; RISK-NVRAM-01, -02, -06, -07 updated; OI-NVRAM-21 (map3.jrn and the journal API have no consumer) and OI-NVRAM-22 (three choices with no derivation) raised.** |
| 19 | 2026-11-26 | NeurOne Firmware Engineering | **OI-NVRAM-22 worked, left open: §3.3.1.6 notes that the 12,288-byte length's 4 KiB multiple is not a medium bound (a copy is 12,304 B) and that per-wavelength dose would fit the row's room; §8.5 callout: the fleet column for dose_accum contradicts NP-MOD-ID-001 §7.2 and DOSE-01, and the §7.4 model uses drive time, so UC-214's closing condition could not be met; the question is in docs/human-input/oi-nvram-22-dose-accum-wavelengths.md and no fleet path for dose_accum is built until it is answered; §13 OI-NVRAM-22 row updated.** |
| 20 | 2026-11-26 | NeurOne Firmware Engineering | **OI-NVRAM-21 closed (§13): map3.jrn and the Config store's journal API are removed; new top callout; §4.2(b), §4.2.1, §7.2.1, §8.5, D-23 and §14 annotated; the retired journal discipline stays as the record.** |
| 21 | 2026-11-26 | NeurOne Firmware Engineering | **§8.5 and §8.1: the principal's ruling of 2026-11-26 recorded; the dose never reaches the fleet and is UHDR, one total per wavelength, off Config by D-3; the Rev 19 callout is resolved; on-time and sessions per emitter channel and time in 5 °F bands added as SHDR under the warranty owner's consent, specified, not built. §3.3.1.6: emitter use needs 20 B per row against 19 B of room even with the dose off the row, plus 4 B per temperature band if kept per module; nothing resized. §11: D-38. §13: OI-NVRAM-22 (c) closed; OI-NVRAM-23, -24 and -25 raised D-39 and §8.5: either of the warranty owner's consents admits the Map 3 SHDR wear fields and throttle_count, and the window's end prompts for general consent; accessory-port channels counted (28 B needed for the intranasal probe's row against 19). §13: OI-NVRAM-25 closed; OI-NVRAM-26 and -27 raised.** |
| 22 | 2026-11-26 | NeurOne Firmware Engineering | **D-39 rewritten and §8.1 and §8.5 corrected: general SHDR consent covers what predictive maintenance needs, set by the NP-MOD-ID-001 §7.4 review gate; until it rules the extended content flows only from opted-in helmets within the window; general consent grows only by asking; two prompts, the second on an app-update broadcast to all warranty owners; the either-consent reading of Rev 21 marked replaced. §13: OI-NVRAM-27 closed; OI-NVRAM-26 extended to two prompts; OI-NVRAM-28 raised; OI-NVRAM-23 and -24's fleet path is opt-in only until the gate rules.** |
