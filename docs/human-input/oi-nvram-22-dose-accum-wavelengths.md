# OI-NVRAM-22 (c): does Map 3's `dose_accum` reach the fleet at all, and is it one sum over a tile's wavelengths or one total per wavelength?

**Status:** DRAFT, not sent.
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
