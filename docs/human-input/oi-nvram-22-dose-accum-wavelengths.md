# OI-NVRAM-22 (c): does Map 3's `dose_accum` reach the fleet at all, and is it one sum over a tile's wavelengths or one total per wavelength?

**Status:** ANSWERED 2026-11-26 (the principal, in two messages the same day; §7). Q1: A, delivered dose never reaches the fleet. Q2: B, one dose total per wavelength, for the person's own app. SHDR gets emitter use per emitter and the working environment instead, under the warranty owner's consent. The principal answered the two readings put back in `OI-NVRAM-25` the same day (§7.1): per emitter is per drive channel, accessory ports included, and either of the warranty owner's consents admits the new fields, with a prompt for general SHDR consent when the characterisation window ends.
**GitHub issue:** none yet. Rule 7 of `docs/human-input/README.md` asks for one; the session that wrote this did not open an issue unasked. The finding comes from working `OI-NVRAM-22`, raised by pull request [stevehickman/NeuroPulse#1072](https://github.com/stevehickman/NeuroPulse/pull/1072).
**Sender:** the session that worked on `OI-NVRAM-22` (a Claude agent), drafted 2026-11-26.
**Addressee:** the Principal, with the Privacy Lead and Firmware.
**Owning open item:** `OI-NVRAM-22` part (c) (`docs/status/pending-decisions.md`; `NP-FW-NVRAM-001` §13); register row UC-214 (`docs/status/unjustified-choices.md`).
**Not a controlled document and not a design input** (`docs/human-input/README.md`): no serial, not in `NP-DHF-001`, and it writes no requirement (`CLAUDE.md` §18).

**How to read it.** **[E]** is established, with the file that shows it. **[P]** is proposed here.

---

## 1. What I need from you

| # | Question | Recommendation |
|---|----------|----------------|
| Q1 | Does a module's `dose_accum` reach the fleet under `NP-MOD-ID-001` §7's opt-in? **A:** no. `NP-MOD-ID-001` §7.2 stands: delivered dose stays out of the System Health Data Record (SHDR) even for opted-in devices, and `dose_accum` is kept on the helmet and read only by the person's app. **B:** yes, as a per-module running total under the month anchor, for opted-in devices only; `NP-MOD-ID-001` §7.2 and the `DOSE-01` schema gate are amended to admit it. | **A** |
| Q2 | On the helmet, is `dose_accum` one sum over a tile's wavelengths (as built), or one total per wavelength? **A:** one sum, as built. **B:** one total per wavelength (660, 808 and 1064 nm). **C:** neither; drop `dose_accum` from the row, because drive time per module is already counted (`used_s`) and per tile on the module itself (Map 4, `emitter_on_seconds`). | **A**, unless Q1 is B and the fleet model is changed to use dose (then decide Q2 with that model's owner) |

What to send back: the option for each, a short reason (`CLAUDE.md` §18 wants one), anything that would reopen it, and your name, role and date.

## 2. Why this is asked

`OI-NVRAM-22` (c) and register row UC-214 say the wavelength sum waits on "the opt-in fleet model (`NP-MOD-ID-001` §7.4)" saying whether it needs dose per wavelength. Working the item showed that this closing condition cannot be met as written, because that model never receives dose:

- **[E]** The model's exposure covariate is drive time, not dose: `degradation_rate ~ exposure + module_kind + (1 | socket)`, where the exposure fields are `emitter_on_seconds`, `thermal_seconds_over_threshold` and `throttle_event_count` (`NP-MOD-ID-001` §7.2, §7.4).
- **[E]** The opt-in narrows `DOSE-01` and does not remove it: "delivered *dose* in J/cm² and raw irradiance remain prohibited, being direct readouts of the therapeutic quantity rather than of drive time" (`NP-MOD-ID-001` §7.2; §8.2: `DOSE-01` "continues to reject J/cm² and irradiance").
- **[E]** The schema gate rejects any SHDR column whose name contains `dose` (`ci/test_shdr_schema.py`, `PROHIBITED_DOSE_COLUMN_PATTERNS`).
- **[E]** `NP-FW-NVRAM-001` §8.5 (your ruling of 2026-11-23, as recorded) sends `dose_accum` and `throttle_count` to the fleet "only under `NP-MOD-ID-001` §7's opt-in, as Map 4's `emitter_on_seconds` and `throttle_events` already are". Your answer in `docs/human-input/oi-priv-38-map3-journal-and-insertion-records.md` §6 says "The opt-in of `NP-MOD-ID-001` §7 for dose and throttle totals stays." That opt-in admits drive time and throttle counts. It has never admitted dose.

So two recorded rules disagree about whether `dose_accum` may reach the fleet, and the model UC-214 waits on cannot settle the wavelength question either way. The throttle total is not in question: `throttle_event_count` is on `NP-MOD-ID-001` §7.2's list.

## 3. What is known

- **[E]** `dose_accum` is built: one 32-bit running total per module in the Map 3 row, the session's metered dose summed over the tile's three wavelengths (660, 808 and 1064 nm) in whole mJ/cm² (`firmware/hub_control/src/np_module_usage.c`, `np_mu_dose_mJ_cm2`; `np_pbm_socket_telem.h`, `dose_J_cm2[3]`; `NP-FW-NVRAM-001` §3.3.1.6).
- **[E]** No field of Map 3 reaches the fleet today. The hub side of the fleet upload is not built (`NP-FW-NVRAM-001` §8.5, "Who sees which field"; `NP-FW-HUB-001` §2.5). Nothing has to be undone whichever way Q1 goes.
- **[E]** Map 3 lives in the Config partition, whose key NeurOne can re-derive (`NP-FW-NVRAM-001` §3.2). Anything written there is readable by NeurOne at the moment of the write, without an upload. That is why §8.5 keeps every Config field a running total.
- **[E]** Per socket per session, dose per wavelength (F2) is the User Health Data Record (UHDR) (`data-architecture-detail.md` §5.1, the socket-lattice rulings). A module usually stays in one socket, so per-module dose is treatment geography (`DOSE-01`; `NP-FW-NVRAM-001` §8.5).
- **[E]** The sum of doses at different wavelengths is not a physical quantity any wear model in the repository uses (UC-214). Nothing in the repository says what `dose_accum` is for beyond the person's own app (Map 2) and the fleet row in question.
- **[E]** If Q2 were B, the row would grow by 8 bytes (three 4-byte totals in place of one). The record leaves room for 19 bytes per row (`NP-FW-NVRAM-001` §3.3.1.6, UC-213), so B needs no change to the record length. No unit has shipped and no row exists, so there is no migration.
- **Assumed, not checked:** that the opt-in fleet model will keep §7.4's form. If its owner changes the exposure covariate to dose, Q1 and Q2 change with it.

## 4. Options and their costs

### Q1: the fleet

**A. No fleet path for `dose_accum` (recommended).** `NP-FW-NVRAM-001` §8.5's fleet row is corrected to say `dose_accum` stays on the helmet and in the person's app. `throttle_count` keeps the opt-in.
- *Keeps:* `NP-MOD-ID-001` §7.2 and the `DOSE-01` gate as they are; the opt-in cohort gives drive time, not the therapeutic quantity.
- *Costs:* the fleet never has per-module dose. The §7.4 model does not ask for it, so nothing it needs is lost. A later model that wanted dose would need this question reopened.

**B. A fleet path under the opt-in.** `NP-MOD-ID-001` §7.2 admits a per-module running total of dose for opted-in devices; `DOSE-01` and its CI check are amended with a named exception.
- *Keeps:* §8.5 as recorded.
- *Costs:* the therapeutic quantity per module enters SHDR for the opted-in cohort, which §7.2 refused on the ground that it is "a direct readout of the therapeutic quantity". A schema exception in `DOSE-01`. The §7.4 model would still have to be changed to use it, or the field would be collected with no consumer.

### Q2: the helmet

**A. One sum, as built (recommended).** UC-214 then records your ruling as the reason the field is one sum, and that no consumer needs it finer.
- *Costs:* the total cannot be split later for sessions already counted. A sum across wavelengths means little for wear, so its use is mainly the person's own record.

**B. One total per wavelength.** Three fields at the row's tail (8 bytes more).
- *Costs:* finer treatment data in a store NeurOne can read (which wavelengths a person's protocols drive, per module). With Q1 A no consumer outside the person's app has been named for it.

**C. Drop `dose_accum`.** The person's app already receives per-session dose per socket per wavelength in the UHDR session files, so it does not need Config to hold a total.
- *Costs:* changes the field list of your 2026-11-23 ruling. A session file lost before download (`np_log_pbm_socket` writes per-socket dose per wavelength to UHDR) takes its dose with it, where the total would have kept it; that guarantee is why Map 3 exists (`NP-FW-NVRAM-001` §7.2). The row frees 4 bytes for later use.

## 5. Recommendation

**Q1 A, Q2 A.** The recorded rule (`NP-MOD-ID-001` §7.2) and the gate already keep dose out of SHDR, and the model the opt-in exists for is built on drive time. Keeping the sum on the helmet changes nothing that is built and adds no finer data to Config. C is reasonable too, and is the least data, but it changes a field you named.

## 6. What happens on each answer

- **Q1 A:** `NP-FW-NVRAM-001` §8.5 and §8.1 are corrected (the fleet column for `dose_accum`), as is `data-architecture-detail.md` §5.1's Map 3 bullet. **Q1 B:** `NP-MOD-ID-001` §7.2 and §8.2, `ci/test_shdr_schema.py` and the fleet schema are amended first, and `OI-NVRAM-22` (c) waits on the §7.4 model's owner saying whether it uses dose and per which wavelength.
- **Q2 A:** UC-214 moves to the register's "Justified or retired" table, citing the ruling, if you give a reason that §18 accepts (for example, data minimisation: no consumer needs a finer total). **Q2 B:** a version-2 row adds the three fields at its tail (`NP-FW-NVRAM-001` §7.2's version rule). **Q2 C:** the field is removed from the row before any unit ships.
- `OI-NVRAM-22` closes only when parts (a) and (b) close too; they wait on fleet data and on the first version that grows the row, not on you.

## 7. Answer (the principal, 2026-11-26)

**The principal's words, verbatim.** The first message:

> should dose_accum reach the fleet under the opt-in at all? SHDR should include information about how much each emitter is used. Delivered doses don't matter for SHDR. totals should be per emitter. Session counts per emitter can also be useful for SHDR because it indicates on/ off switching. Measured temperature and humidity (if we had it) would reflect the working environment.

The clarification, the same day:

> Warranty opt in (for SHDR data) comes from the warranty owner, not individual users. Dose should be per wavelength because that can be clinically useful for individual users. Set up to track temperature by time in bands (5 deg F) initially because it's more detailed. Create an issue to investigate the cost of adding temp and humidity sensing. If we have the sensing, we may find after an initial period that detailed temperature band info is not needed - but that will require an initial set of data to analyze.

**What is ruled.** The recording session's reading of the two messages. The rulings are the principal's; the wording here is not.

1. **Q1: option A.** Delivered dose never reaches the fleet, opted in or not. `NP-MOD-ID-001` §7.2 and the `DOSE-01` gate (`ci/test_shdr_schema.py`) stand unchanged. `NP-FW-NVRAM-001` §8.5's fleet column for `dose_accum` now says **never**, and its Rev 19 callout is resolved.
2. **Q2: option B.** Dose is kept **per wavelength** (660, 808 and 1064 nm), because a total per wavelength can be clinically useful to the individual user. It is the person's data, for their app (Map 2, the UHDR side) only, never SHDR. UC-214's sum is retired by this ruling.
3. **SHDR carries emitter use, as wear of the device:** a running total per emitter of how long it is driven, and a running total per emitter of the sessions in which it is switched on and off (the principal names on/off switching as the wear it shows). Not one figure per tile and not summed across wavelengths.
4. **Consent for SHDR data is the warranty owner's, never an individual user's** (`CLAUDE.md` §6.0; per helmet, `REQ-SHDR-08`).
5. **The working environment: temperature as time in 5 °F bands, initially.** The principal chose 5 °F for the detail it gives during an initial period, and expects that the bands may prove finer than needed once a first dataset has been analysed. They are SHDR running totals (time per band) under the warranty owner's consent. 5 °F has no derivation, so it has a register row (UC-215). **Humidity** is wanted if it is sensed. No humidity sensor exists today. The cost of adding temperature and humidity sensing is to be investigated: [#1086](https://github.com/stevehickman/NeuroPulse/issues/1086).

**The shape stays as ruled on 2026-11-23** (`docs/human-input/oi-priv-38-map3-journal-and-insertion-records.md` §6; `NP-FW-NVRAM-001` §8.5). Every new figure is a running total, with no per-session row, no ordinal and no clock. The app computes differences between downloads in Map 2, and D-18 keeps every per-window value out of the fleet. Everything new is **specified, not built**: `OI-NVRAM-23` (emitter use and dose per wavelength) and `OI-NVRAM-24` (the environment record).

**What follows from rules already locked [E].**

- **Dose per wavelength cannot be written to Config.** Config's key is NeurOne-derivable, so nothing of UHDR class is written there (`NP-FW-NVRAM-001` §3.2, D-3). Dose is the person's (ruling 2). So the per-wavelength totals need a home on the UHDR side: a record in the UHDR partition, or totals the app builds from the UHDR session files, which already carry dose per socket per wavelength (F2). The choice is `OI-NVRAM-23`'s. **The `dose_accum` sum built today sits in the Config row.** It is never uploaded and no unit has shipped, but under this ruling it is UHDR-class data in a store NeurOne can read. `OI-NVRAM-23` removes it from the Config row when the per-wavelength totals are built.
- **The §7 opt-in is already the warranty owner's.** `NP-MOD-ID-001` §7.5.1 binds it to the warranty owner, "consistent with the existing SHDR consent subject". Nothing in `NP-MOD-ID-001` §7 or `NP-FW-NVRAM-001` §8.5 makes any field, `throttle_count` included, depend on an individual user's opt-in. What the answer leaves open is which of the warranty owner's two consents gates the new fields: see (b) below, answered in §7.1 (either).
- **A tile's NTC sits against the scalp.** Its per-socket reading in a session is ruled UHDR, as scalp-contact temperature that marks a lit socket (F7, `docs/reference/data-architecture-detail.md` §5.1). A time-in-band total built from tile NTCs needs `CLAUDE.md` §5.1 rule 1's positive demonstration that it carries no user biology, or it must come from a source that does not touch the scalp (the hub NTCs, or an ambient NTC, `NP-ENV-001` §5). `OI-NVRAM-24` decides which.

**What "per emitter" can mean on this hardware [E].** A tile has 90 emitter sites (`NP-HW-HEXTILE-001` §4.2: T1-A has 45 at 660 nm and 45 at 808 nm; T1-C has 30 each at 660, 808 and 1064 nm). They are wired in series strings per channel (§8.1.1 there). The hub commands and meters a tile per channel (`CH_A`, `CH_B`, `CH_C`; `np_pbm_socket_telem.h`, `dose_J_cm2[3]`), never per site. So the finest "per emitter" the hub can count is **per channel, at most three per tile, one per wavelength**. That is how it is recorded. Counting per site would need hardware that switches each site on its own, and 90 × 8 = 720 B per row.

**Two readings put to the principal (`OI-NVRAM-25`), answered the same day: see §7.1.** As first put:

- **(a) Per emitter means per drive channel**, as above. A session count is the number of sessions in which the channel was driven, one switch-on and one switch-off each. It does not count the pulses of a pulsed protocol.
- **(b) Which warranty-owner consent gates the new SHDR fields.** One option is the warranty owner's SHDR consent, given at registration, per helmet (`SHDRUploader`, `REQ-SHDR-08`). The other is `NP-MOD-ID-001` §7.5's separate characterisation opt-in, which the warranty owner also grants. Two facts bear on it. §7.3 stops the §7 fields when its window closes, by a build-time constant. §7.5 holds that folding an 80-socket duty map into warranty registration would be bundled consent. Per-channel totals per module are coarser than that map, but a module usually stays in one socket. `throttle_count` rides on §7's opt-in today and is unchanged until this is answered. Nothing uploads either way: the hub side of the fleet upload is not built (`NP-FW-HUB-001` §2.5).

**Room in the Map 3 row [E].** The 12,288 B record leaves **19 B** per row (`NP-FW-NVRAM-001` §3.3.1.6, UC-213). Every count is 32-bit and stored whole (`OI-UINT16-01`):

| Fields | Bytes per row |
|---|---|
| On-time per channel, 3 × 4 | 12 |
| Sessions per channel, 3 × 4 | 12 |
| Dose per wavelength, 3 × 4, if it stayed in the row (it cannot, by D-3), in place of `dose_accum`'s 4 | +8 |
| Time per 5 °F band, if kept per module, 4 per band | 4 × N; N is not set |
| `dose_accum` removed from the Config row | −4 |

With dose moved off Config, emitter use alone needs 24 − 4 = **20 B against 19**, which is 1 B short. Any temperature band kept per module adds 4 B more for each band. With dose left in the row, the total would be 32 + 4N B. **Nothing fits the record as it stands.** A per-helmet temperature record would sit outside the row, at 4 × N B once. Nothing is resized here. The record length is D-36's and UC-213's, and `OI-NVRAM-23` decides how the fields get room.

**What would reopen it:** for Q1, a fleet model that needs delivered dose. For the 5 °F bands, the first dataset's analysis, as the principal said. For humidity, the outcome of #1086. For (a) and (b), see §7.1.

**What changed where (2026-11-26):** `NP-FW-NVRAM-001` §3.3.1.6, §8.1, §8.5, §11 (D-38) and §13; `NP-FW-HUB-001` §6.11 and `REQ-FWHUB-93`'s *Fails without it*; `NP-MOD-ID-001` §7.2; `docs/reference/data-architecture-detail.md` §5.1; `docs/status/unjustified-choices.md` (UC-213 updated, UC-214 retired, UC-215 added); open items `OI-NVRAM-22` (part (c) closed), and `OI-NVRAM-23`, `OI-NVRAM-24` and `OI-NVRAM-25` raised.

### 7.1 The answer to `OI-NVRAM-25` (the principal, 2026-11-26)

**The principal's words, verbatim:**

> (a) 'per emitter' means per drive channel; 'session' means a session in which the channel was driven, not individual pulses; (b) both consents cover it. Generally, characterisation opt-in is just a time limited version of consent that, once the characterisation period ends, changes consent from 'yes' to 'no'. Note that when that period ends and the value changes, the warranty owner should be prompted for general SHDR consent - because they may ave changed their mind at this point.  And yes, emitters in accessory ports are counted

**How it is recorded.** This is the recording session's reading. The rulings are the principal's; the wording is not.

- **(a) Per emitter means per drive channel.** A session count is the number of sessions in which that channel was driven. It does not count individual pulses.
- **Accessory ports are counted.** Emitters in accessory ports are counted per drive channel, as a tile's are: for example the intranasal probe's 660 and 808 nm channels on each side.
- **(b) Either consent admits the new SHDR fields.** The new fields are on-time per channel, sessions per channel and the 5 °F temperature bands. They flow while **either** of two consents is in force:
  - the warranty owner's general SHDR consent (per helmet, `REQ-SHDR-08`);
  - `NP-MOD-ID-001` §7.5's characterisation opt-in, while its window is open.

  The principal frames the characterisation opt-in as a time-limited form of the same consent, which turns from yes to no when the window ends.
- **The expiry prompt.** When the characterisation window ends and that consent turns to no, the warranty owner is asked about general SHDR consent for that helmet, because they may have changed their mind. The invariants this keeps:
  - The prompt is a fresh question. It starts at no consent, and silence is not a yes: a dismissal, a timeout or no answer grants nothing.
  - "Never asked" and "said stop" stay distinguishable (`CLAUDE.md` §6.0). The end of the window is recorded as its own state. It is not a revoke, and it is not never asked.
  - `REQ-SHDR-09` stands. Revoking SHDR consent ends characterisation enrolment, and a later grant does not bring the enrolment back. The prompt asks only for general SHDR consent and never re-enrols characterisation.
  - It concerns the warranty owner only. `SHDRUploader` stays independent of `ConsentStore` (`REQ-CONSENT-01`).

  Written as `REQ-SHDR-12` (`docs/reference/data-architecture-detail.md` §5.4), untested until it is built (`OI-NVRAM-26`).
- **`throttle_count` is read under "Generally".** "Generally, …" states the rule for the characterisation opt-in as a whole. So `throttle_count`, which rode on the §7 opt-in alone, now also flows under either consent. That is this session's reading of "generally", not the principal's words.

**What this reading does not reach, and why [E].** The words "generally" could also be read to cover the per-socket, per-session fields that `NP-MOD-ID-001` §7.2 admits (`emitter_on_seconds`, `thermal_seconds_over_threshold`, `throttle_event_count`, `peak_ntc_celsius`). They could also cover `NP-FW-EMMC-002` §H's accelerometer programme. Under §7.3 and §H.3.4 those fields stop at the window's end by a firmware gate, and §7.5 argues that admitting an 80-socket duty map under warranty registration would be bundled consent. Reading "generally" that widely would move a per-socket, per-session duty map to every consenting device. The principal's answer was given about the Map 3 totals, so this record applies it to them and to `throttle_count`, and leaves §7.2's per-socket set and §H as they are. Whether the ruling reaches them is put to the principal in `OI-NVRAM-27` (a).

**A conflict this exposes [E].** `REQ-SHDR-09` (a) refuses characterisation enrolment for a helmet whose general SHDR consent is not open, and (b) ends the enrolment when that consent is revoked. So a helmet inside its characterisation window always has general consent open as well. In practice "either consent" therefore means general consent. At expiry the owner already has general consent, so the prompt gives them a chance to reconsider. The answer does not say what an unanswered prompt does to the consent the owner already gave: leave it as it was, or let it lapse. Until the principal rules (`OI-NVRAM-27` (b)), `REQ-SHDR-12` makes an unanswered prompt change nothing. It records no grant and no revoke.

**Where this supersedes `NP-MOD-ID-001` [E].** For the Map 3 totals and `throttle_count`, the ruling supersedes two things in `NP-MOD-ID-001`: §7.3's stop at the window's end, and §7.5's bundled-consent argument. Both are kept in that document, with a note pointing to `NP-FW-NVRAM-001` §8.5 and D-39. They still govern §7.2's own per-socket set until `OI-NVRAM-27` (a) is answered.

**Room in the Map 3 row, accessory channels included [E].** The record has one row layout for tiles and accessory ports alike, so a row has to hold the most channels any module drives. Each channel takes 8 B (a 32-bit on-time and a 32-bit session count):

| Module | Drive channels | Emitter-use bytes |
|---|---|---|
| Tile (T1-C) | 3 (660, 808 and 1064 nm; `NP-HW-HEXTILE-001` §4.2) | 24 |
| Intranasal probe | 4 (two sides, 660 and 808 nm on each; `REQ-NASAL-04`, `cur_660` and `cur_808`) | 32 |
| 1170 nm deep PBM unit (T2) | 1 | 8 |
| Visual goggles | Not known. The 12-bit zone mask times 660 and 808 nm would be up to 24, but what the bits mean is open (`OI-FWHUB-35`), and Mode F adds an NIR walk | up to 192 |

The record leaves 19 B per row, and dropping `dose_accum` from Config frees 4 B. A row sized for the intranasal probe therefore needs 32 − 4 = **28 B against 19**. A row sized for the goggles' worst case needs 188 B. Each per-module temperature band adds 4 B. At 147 rows, a 4-channel row makes the record 147 × (64 − 4 + 32) + 4 = 13,528 B, which is already past 12,288. **Nothing fits.** Nothing is resized here. `OI-NVRAM-23` decides the layout, for example a separate record for the 19 port rows, or rows that differ in length by kind.

**Open items:** `OI-NVRAM-25` is closed. `OI-NVRAM-23` now covers accessory-port channels. `OI-NVRAM-26` (raised) builds the app's expiry prompt. `OI-NVRAM-27` (raised) asks the principal the two questions above.

**What changed where (2026-11-26, second change):** `NP-FW-NVRAM-001` §3.3.1.6, §8.1, §8.5, D-38, D-39 and §13; `NP-MOD-ID-001` §7.2, §7.3 and §7.5; `NP-FW-HUB-001` §6.11; `docs/reference/data-architecture-detail.md` §5.1 and `REQ-SHDR-12`.
