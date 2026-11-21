# Question for the principal: OI-PRIV-38 (NP-PRIV-ANALYSIS-004), what the Map 3 journal keeps, and whether accessory insertions stay events

**Status:** DRAFT, not sent. Not a controlled document; see `docs/human-input/README.md`.
**GitHub issue:** [#966](https://github.com/stevehickman/NeuroPulse/issues/966)
**From:** NeurOne Firmware Engineering (`OI-PRIV-38` (NP-PRIV-ANALYSIS-004) in `docs/status/pending-decisions.md`; firmware privacy review `NP-PRIV-ANALYSIS-004`, Finding M13)
**To:** the principal
**Response needed:** an option for Q1 and for Q2, a short rationale, anything that would reopen either, and your name, role and the date.

## 1. The questions

**Q1.** Three fields of the Map 3 journal row are UHDR class, and the row was designed to live in the Config
partition, whose key NeurOne can re-derive. **What does Map 3 keep in their place, and where?**

**Q2.** The accessory-authentication record (`0x83`) no longer carries a session count, but it is still one record
per insertion in an ordered SHDR stream. **Does it stay a per-insertion event, or become a lifetime total?**

Finding M13 had a third part (the EDF+ research gate wrote one SHDR fault per excluded file). That needed no ruling
and is done: the gate writes nothing to SHDR (`REQ-SW02-12`). It is not asked here.

## 2. What is known

### 2.1 Q1: the Map 3 row and the store it was designed for

- **The row** (`firmware/hub_control/include/np_map3_record.h`, `NP-FW-NVRAM-001` §3.3.1.5) is 64 bytes, one per
  module per session: `uid`, `seq`, `ordinal`, `session_count`, `dose_accum`, `throttle_count`, `fault_flags`.
- **The store.** `NP-FW-NVRAM-001` §3.1 puts Map 3 in the hub's Config partition, for four reasons: it is mounted
  before any session; factory reset already zeroes it, which rotates every `module_ref`; it costs no part; and its key
  custody. §3.2 is the rule that now decides it: *"The Config partition is encrypted under a key NeurOne can
  re-derive. Therefore nothing of UHDR class may be written to it."* Writing UHDR-class data there discloses it to
  NeurOne at the moment of the write, with no upload and no consent event.
- **What §8.1 classified and what it missed.** `seq`, the watermarks, `session_count` (as a bare count),
  `fault_flags` and the UID were classified through Rev 14. `ordinal`, `dose_accum` and `throttle_count` never were.
  This change classifies them by `CLAUDE.md` §5.1's tests and the rulings they repeat, which needs no ruling:
  | Field | Class | Why |
  |---|---|---|
  | `ordinal` | UHDR | A row per module per session keyed by `(uid, ordinal)` says which module was worn in which session: the session fitting record (UHDR) and the montage F1 rules UHDR |
  | `dose_accum` | UHDR | Dose per module per session is F2. A running total does not escape it: two consecutive rows give the session's dose |
  | `throttle_count` | UHDR | Which socket throttled is F8 |
- **The rows themselves are the problem, not only three columns.** Even with the three fields dropped, a journal
  appended once per module per session, in order, still says which modules were present in each session, because
  the set of UIDs between two `seq` boundaries is the fitting. So "drop the fields" alone is not an option.
- **Who reads Map 3.** Only the control software, at sync, into Map 2 (`NP-FW-NVRAM-001` §6.1). §8.3 already gives
  Map 2 UHDR-class handling (D-19): it is the user's and is never a source for an SHDR upload.
- **Nothing is built.** `np_map3_record.c` is the codec and its scanner. No code writes a row; the owner that would
  is not written (`NP-FW-NVRAM-001` Rev 5 note). No unit has shipped. No row exists anywhere.
- **Until this is ruled**, `NP-FW-NVRAM-001` §3.3.1.5 and §8.1 and the codec's header say the layout is not to be
  written to Config.

### 2.2 Q2: the insertion record

- `np_log_shdr_zone_auth()` writes tag, slot, module type and pass/fail. Until this change it also wrote the device
  session count. The intranasal probe is inserted for every intranasal session and the record is written once per
  insertion (`NP-FW-HUB-001` D-30), so the count made a per-session timeline of intranasal use. F14 forbids that
  shape for tiles: their record is one latest-state row per socket, *"with no session count and no run identifier,
  so SHDR never holds a log of which tiles a person chose to test, or when."*
- **Done here, no ruling needed:** the count is gone (`REQ-NASAL-16` a). The failure branch was unreachable, because
  the registry calls `init()` only after `detect()` has required authentication; the driver now writes a failure from
  `detect()`, once per insertion (`REQ-NASAL-16` b). "Accessory authentication pass/fail → SHDR" is already the
  enumerated rule (`data-architecture-detail.md` §5.1).
- **What remains.** SHDR is an ordered stream, and the session-open record (`0x86`) carries the session count. An
  insertion record written between two of them still places that insertion between those two sessions. The timeline
  is coarser (an insertion falls between two sessions rather than carrying one session's number) but not gone.
  Removing it means the record stops being an event.
- The cervical VNS driver writes a pass from `init()` and has no authentication challenge (`detect()` reads
  "present"). The same question applies to it.

## 3. Options

### Q1: Map 3

**A. Move the journal, as designed, to the UHDR partition.** The row is unchanged; it is written under the user's
key, as the session's UHDR log already is.
- *Keeps:* the whole design: per-window detail for `NP-MOD-ID-001` §6.5 reconciliation, D-12 (never block a session),
  the watermarks, the version rule.
- *Costs:* `NP-FW-NVRAM-001` §3.1 is rewritten (reasons 1 and 2 move with it: the UHDR partition's mount and erase
  semantics must be checked to match). Sync needs the person's key, which Map 2's handling already assumes. **To be
  confirmed before choosing:** that the UHDR partition is writable at every session close, including Mode 3 with no
  phone; and what a helmet with several users does (one journal per user, so a module's history is split by person).
  Engineering estimate, not costed: a few days of specification and the codec unchanged.

**B. Replace the journal with one latest-state row per module, kept in Config.** Upserted, not appended: UID,
lifetime session count, fault flags, `detail_lost`. No ordinal, no dose, no throttle count, no per-session row.
- *Keeps:* Config's custody and erase semantics (§3.1 stands); the totals the odometer reconciliation compares.
- *Costs:* per-window detail is gone, so §6.5 reconciles totals only and §6.3's journal-full policy disappears. Dose
  and throttling per module then exist only in Map 4 under `NP-MOD-ID-001` §7's opt-in, and in the UHDR session
  records. A redesign of §3.3.1.5, §4.2(b), §6 and §7.2 (the row's version rule), about a week.

**C. Retire Map 3's per-session content; the app builds Map 2 from the UHDR session files it already downloads.**
The fitting record and the PBM socket records already carry per-module, per-session use, in UHDR.
- *Keeps:* nothing new stored anywhere; the least data.
- *Costs:* Map 3 exists because *"nothing else remembers the sessions it holds"* (§7.2): a session file deleted or
  rotated before download is lost to Map 2, where the journal's watermark guaranteed it was not. That guarantee would
  have to move to the session-file retention rule.

*Keeping the fields in Config is not offered:* it would write UHDR-class data where NeurOne holds the key, which
`CLAUDE.md` §5.1 ("NeurOne access: NEVER") and `NP-FW-NVRAM-001` §3.2 forbid.

### Q2: the insertion record

**A. Keep a per-insertion event, without the count (as built now).** *Costs:* the residual in §2.2 stays: position in
the stream dates an insertion to between two sessions.

**B. Replace the event with lifetime totals.** Per accessory slot: insertions seen and authentication failures, as
counts, in a fixed-shape record written at every session end whether or not the accessory was used (the F13 shape)
or in the idle latest-state pass (the F14 shape). *Keeps:* the maintenance signal (a sleeve population that fails
authentication). *Costs:* a new SHDR record and fleet-schema columns; `np_session_log.c`'s session-end path is being
changed by `OI-PRIV-21` (NP-PRIV-ANALYSIS-004), so this lands after it. The per-insertion fault becomes a count.

**C. Totals for passes, an event for failures only.** Passes are the frequent, informative records; failures are
rare and are faults. *Costs:* a failure event is still positioned in the stream, but it records a failed sleeve,
not a session.

## 4. Recommendation

- **Q1: A**, if the two checks in A come back clean; otherwise **B**. The fields are UHDR and Map 3's only consumer,
  Map 2, is already on the UHDR side of the fence, so the journal moves to the side its consumer is on and keeps the
  design that §6 and §7.2 depend on. B is the fallback because it loses reconciliation detail; C moves a guarantee
  into a rule that does not exist yet.
- **Q2: B**, the F13 or F14 shape, consistent with `NP-FW-NVRAM-001` D-20 (mate cycles are a total, and the events
  are not written at all).

## 5. What happens on each answer

- Q1 A: `NP-FW-NVRAM-001` §3.1, §3.2 and §8.1 are revised, the Map 3 owner is specified against the UHDR partition,
  and the "not to be written to Config" notes are removed.
- Q1 B or C: §3.3.1.5 onward is redesigned and the codec changes (no unit has shipped and no row exists, so there is
  no migration).
- Q2 B or C: a new open item for the record and its schema, after `OI-PRIV-21` (NP-PRIV-ANALYSIS-004).
- `OI-PRIV-38` (NP-PRIV-ANALYSIS-004) closes when both are ruled and recorded in `data-architecture-detail.md` §5.1.
