# Question for the principal: OI-PRIV-59, one consent store, one history and one biometric release per install, while an install holds several profiles

**Status:** DRAFT, not sent. Not a controlled document; see `docs/human-input/README.md`.
**GitHub issue:** [#1061](https://github.com/stevehickman/NeuroPulse/issues/1061)
**From:** NeurOne Software Engineering (`OI-PRIV-59` in `docs/status/pending-decisions.md`; `NP-PRIV-ANALYSIS-006` F-03, `NP-PRIV-ANALYSIS-007` Finding M3)
**To:** the principal, with the App Lead
**Response needed:** an option for Q1, and for Q2 and Q3 if Q1 is A; a short rationale; anything that would reopen it; your name, role and the date.

## 1. The question

`CLAUDE.md` §6.0 says: *"Each patient on a clinic device has their own `ConsentStore`."* The shared app lets one install
hold several people's profiles, and tells the helmet which one is active, but keeps one research consent, one biometric
release, one analytics identity and one session history for the whole install.

Plain form: **is a phone or tablet one person's, or can it be several people's? If several, what does each person get
that is their own?**

Nothing changes until you answer. No unit has shipped (`CLAUDE.md`: pre-tooling), so no real person's consent has been
applied to anyone else yet.

## 2. What is known

All of this was read in the code on 2026-11-25.

### 2.1 The install holds several people

| Where | What it shows |
|-------|---------------|
| `common/core/src/commonMain/kotlin/life/neurone/core/protocol/NPLimitsStore.kt:36` | `NPIndividualProfile`: a random `id`, a `name` "how the person choosing a profile recognises it", an `activeUserTag`, a per-person `stealth` choice. |
| `NPLimitsStore.kt:101`, `:102` | `np.limits.profiles` holds a list of them; `np.limits.active-profile` names the one in use. |
| `app/NeurOneUI/shared/src/commonMain/kotlin/life/neurone/shared/AppServices.kt:428` to `:441` | The active profile's tag is sent to the helmet (`gattManager.activeUser`), so the helmet keeps per-person cardiac scope (`REQ-FAULTMSG-01`). Choosing a profile changes it. |

### 2.2 Everything consent-shaped is one per install

| Store | Where | What one per install means |
|-------|-------|----------------------------|
| Research consent L1 to L4, study participations and withdrawals, clinician grants | `AppServices.kt:236` builds one `ConsentStore`; its keys are fixed strings (`ConsentStore.kt:87` to `:96`) | One person's blanket (L3) grant admits every profile's data to a study; one person's withdrawal withdraws everyone. A clinician grant made by one person reaches the history of all. |
| Biometric (BIPA) written release | One key, `np.onboarding.bipa-accepted` (`app/NeurOneUI/shared/.../ui/NeurOneApp.kt:53`); read at `SessionTab.kt:40` and `SettingsScreen.kt:81`; written at `SettingsScreen.kt:154`, `:162`, `:179` | The first person's release lets EEG be recorded for every later profile, none of whom signed. |
| Session history | `AppServices.kt:342` builds one `SessionHistoryStore`; `SessionRecord` (`SessionHistoryStore.kt:50`) has no profile field | Every profile sees every other's sessions, protocols, durations, coherence and RMSSD. Records cannot be told apart after the fact. |
| Analytics identity and gate | `AppServices.kt:223` one `ResearchAnalyticsGate`; `PostHogAnalyticsBackend.kt:88` one `np.analytics.distinct-id` | Several people's events merge under one pseudonym; one person's withdrawal rotates the ID for all (Finding M3). |

### 2.3 The locked design already assumes several people per install

Three rulings take option (b) off the table unless they are reopened:

1. **`CLAUDE.md` §6.0** (line 334): each patient on a clinic device has their own `ConsentStore`.
2. **`OI-PRIV-68`, 2026-11-21** (`CLAUDE.md` §6.3; `docs/reference/data-architecture-detail.md:103`, `:105`): *"A device
   subscribes only while it is the current device for at least one of its users: one subscription per device however
   many users it holds (a clinic device has one)."* The device runs `ConsentEngine.admit()` *"against that user's
   consents"*, and each person has their own current-device key, carried by a migration.
3. **`OI-PRIV-34`, 2026-11-23 and 2026-11-24** (`docs/human-input/oi-priv-34-several-users-one-helmet.md` §8 to §10;
   `docs/np_fw_emmc_002.md` §C.8): helmets are shared and each person's UHDR is sealed from every other's. The unlock
   credential is the person's phone, **with a passphrase typed on the clinic's tablet as the fallback** (C2). The
   profile cap of 24 rests on *"profiles are loaded from the app as needed"* (`np_fw_emmc_002.md:474`, `UC-204`), that
   is, a clinic's app holds more patients than the helmet holds at once.

### 2.4 Migration, backup and the presence gate are built for one person

- **Migration keys** (`common/npps/migration-keys.json`). `np.session.history` and `np.limits.profiles` are single
  keys in `carried` (lines 5, 6). The consent keys are in `consent` (line 8): grants never move, only withdrawal
  records (`SWR-MG-04`). `np.analytics.distinct-id` is `perInstall` and `deviceTier` (lines 33, 56).
  `np.onboarding.bipa-accepted` is still `unsorted` (line 50): not moved and not dropped by `SWR-MG-01`.
- **One bundle, one key** (`docs/np_sw_001.md:461`, `SWR-MG-02`). The bundle is sealed under one recovery code or
  passphrase that the user holds. Whoever holds it opens every profile's history and limits in it.
- **One clear** (`SWR-MG-09`, `np_sw_001.md:468`; `SWR-MG-17`, `:476`). A device-replacement clear forgets every key the
  lists name. On a shared install one wearer's move off the phone erases every other wearer's data too (`OI-PRIV-63`
  (3) already notes this).
- **One presence gate** (`docs/np_app_presence_001.md` §1 ruling 1, §3). One gated data key per install, bound to the
  device credential (biometric or passcode). On a clinic tablet that credential is the clinic's, so it opens every
  patient's consent and history alike. Switching profile is a selection, not an authentication: today anyone holding the
  unlocked tablet can choose another profile. On the helmet the same point was ruled the other way: the active-user tag
  may **select** a profile but never **opens** one (`np_fw_emmc_002.md` §C.8.2, `OI-PRIV-29`).
- **`OI-PRIV-19`** (open) lists `ConsentStore`, `NPLimitsStore` and `ResearchAnalyticsGate` among the stores that read
  once and write whole values under the gate. Any rework of those stores under Q1 A has to be done to `REQ-PRES-01` (d)
  at the same time.

### 2.5 BIPA

`NP-PRIV-NOTICE-001` (line 201) treats EEG as biometric information under BIPA (740 ILCS 14) and applies those
protections to every user. `NP-PRIV-AUDIT-001` (line 87, 88) cites §15(b)(1) to (3): a written release from **the
subject** (or their legally authorized representative) before collection. A release given by one person cannot be a
release for another. Under one flag per install the second wearer's EEG is collected without their release. Either
option below cures this; the cure differs (§4). Counsel review of the release copy is still open (`OI-PA-03`); nothing
here is a legal opinion.

### 2.6 How much is live

- Nothing sends analytics (`OI-PRIV-05`, "nothing calls `track` yet").
- No research extract is made, and the only shipped `StudyDescriptorVerifier` refuses everything (`OI-CONSENT-07`).
- No migration route is built end to end (`SWR-MG-01` status: no app moves anything yet).
- The history, the release flag and the profile list are live in the shared app on every host.

So every option can be taken before any consent has been relied on.

## 3. Who is affected

| Case | Today | Q1 A (per profile) | Q1 B (one wearer per install) |
|------|-------|--------------------|-------------------------------|
| Clinic tablet, many patients, most bring their own phone | Each patient's own phone is one wearer. The tablet's single consent covers every patient run from it. | The tablet keeps a profile per patient it runs; each patient's consent, release and history are their own. | Patients with phones are unaffected. A patient without a phone has no install of their own and cannot be a wearer on the tablet, against the C2 fallback of `OI-PRIV-34`. |
| Clinic, patients without phones | All share one consent and one history. | Each patient has a profile; with Q2 B each opens it with their own passphrase. | One device per patient, or the clinic cannot run them. |
| Household, two people, one tablet | Each sees the other's history; one release covers both. | Each has their own. | Each needs their own phone or tablet with the app. |
| One person, one phone | No problem. | No visible change (one profile). | No change. |
| Warranty owner's records | Per helmet, separate (`REQ-SHDR-08`, `-10`). | Unchanged. | Unchanged. |

## 4. Questions

**Q1. Is an install one wearer's, or several people's?**

- **A. Several people's, each with their own state (recommended).** Research consent (L1 to L4, participations,
  withdrawals, clinician grants), the biometric release, the session history and the analytics identity are keyed by
  profile. Choosing a profile chooses its consent and its history; with no profile chosen the research and analytics
  gates are closed and no session is recorded to anyone. The suggestion portal's author token and the current-device
  key of `OI-PRIV-68` are per profile.
  - *Clinic:* meets §6.0 as written, the `OI-PRIV-68` design and the C2 fallback of `OI-PRIV-34`.
  - *Migration:* a bundle carries each profile's state apart, and a person can move **their own** profile to their own
    phone without taking anyone else's (the same journey the principal asked for on the helmet, `OI-PRIV-34` Q2).
    `migration-keys.json` stays a list of fixed keys if each key's value becomes a map by profile id (the gate,
    `scripts/check-migration-keys.ts`, cannot list keys built from an id). `SWR-MG-01`, `-04`, `-09` change: a clear can
    be of one profile, and the whole-install clear stays the erase. Q2 decides whether each profile also has its own
    bundle key.
  - *Presence gate:* the gate stays one per install unless Q2 B is chosen. Under Q2 A it separates the people's
    **decisions** but not their **confidentiality**: the install's credential holder can read every profile.
  - *BIPA:* one release per profile, asked when the profile is first used for an EEG session, recorded against that
    profile; a session for a profile without one does not record EEG.
  - *Cost:* the largest. Five stores change shape (`ConsentStore`, `SessionHistoryStore`, the release flag and
    `NPProtocolLibrary.updateEEGConsent`, `ResearchAnalyticsGate` and the PostHog ID, `ResearchSuggestionStore`), the
    consent and settings screens gain a profile context, the migration core and its tests change in each language, and
    `NP-SW-001` gains a rule. `OI-PRIV-19`'s rework of the same stores is best done in the same change.
- **B. One wearer per install.** The app holds at most one individual profile, says so on the profile screen, and a
  clinic uses one install per patient. The stores stay as they are.
  - *Clinic:* a patient without a phone of their own cannot be run, or the clinic keeps a device per patient. Reverses
    the C2 fallback (`OI-PRIV-34` Q3a) and the premise of `UC-204` that the app loads profiles onto the helmet as needed.
  - *Locked text:* `CLAUDE.md` §6.0 (the clinic sentence) and §6.3 / `data-architecture-detail.md` §5.3 step 1a (several
    users per device) change, a `## CLAUDE.md` revision.
  - *Migration:* no change; the one bundle and the one clear are already right for one person. `bipa-accepted` still
    needs sorting into a list.
  - *Presence gate:* already right: the device credential is the one wearer's.
  - *BIPA:* one release per install is one release per subject, if the install truly has one wearer. Nothing stops a
    second person wearing the helmet from the same phone; the profile screen's notice is the control.
  - *Cost:* small. The profile list is limited to one individual, existing extra profiles have to be dealt with, and the
    notice needs eleven locales.

**Q2 (only if Q1 is A). Is each profile's state sealed under its own key?**

- **A. No: one install key, separated by profile in the app.** Each person's consent applies only to them and each
  sees only their own history in the app, but the presence gate's one credential, and the one bundle key, open all of
  it. Cost: the least of the A family. It leaves a clinic tablet's operator able to read every patient's history, which
  is wider than `CLAUDE.md` §5.1's *"per-element, per-use-case, time-limited, audited, revocable"* clinician access.
- **B. Yes: each profile is sealed under a key only that person's credential opens (recommended).** The pattern of
  `OI-PRIV-34` on the helmet, applied to the app: a profile is opened by its person, on their own phone by the device
  credential and on a shared install by a passphrase of their own (`REQ-PRES-06`'s fallback). Choosing a profile asks
  for it. Each profile's backup is sealed under that person's own recovery code or passphrase, so the clinic can keep
  every patient's backup and read none of it. Cost: a second key layer in `:core` and the Rust at-rest code
  (`CLAUDE.md` §22), a per-profile unlock screen, and a per-profile backup key; it is the most work. Protection only goes
  up (`CLAUDE.md` §5.1), so Q2 A can be built first and B later, never the reverse.

**Q3 (only if Q1 is A). On upgrade, what becomes of the state that exists now?**

The existing consent, release and history cannot be attributed to a profile: nothing recorded who gave or ran them.

- **A. Attribute only what is safe (recommended).** Every **withdrawal** in the shared record is applied to every
  profile, because a withdrawal must never turn into "never asked" (§6.0); grants and the release are not attributed and
  each profile is asked afresh, as grants are after a migration (`SWR-MG-04`, issue #569). The history goes to the
  profile if the install has exactly one; otherwise it is kept apart, shown to no profile, and the install holder may
  assign it to a profile or delete it from Settings. Cost: one screen; a release already given is asked again.
- **B. Give everything to the active profile.** Cost: another person's history and grants may land on the wrong person,
  which is the defect this item is about, kept once.
- **C. Discard the history and ask again for everything.** Cost: a person's history is erased; an erase is allowed
  (`CLAUDE.md` §5.1) but loses data the person may want.

No unit has shipped, so Q3 affects only development and test installs today. It is asked so the rule exists before one
ships.

## 5. Recommendation

**Q1 A, Q2 B, Q3 A.**

Q1 A because B reverses three rulings made in the last week (§2.3) and drops the clinic patient without a phone, the
case the principal named when ruling `OI-PRIV-34`. Q2 B because, on the helmet, the principal ruled that each person's
UHDR is sealed from every other's and that a selection never opens a profile; an app that holds the same people's
history under one clinic credential would undo that one step away from the helmet. Q2 A is a reasonable first step if
the cost of B has to be spread, provided B follows.

## 6. What follows a ruling (not started)

**If Q1 A:**
1. A requirement row in `NP-SW-001` first (`CLAUDE.md` §18, §21). What fails without it: one wearer's consent or
   release is applied to another; a wearer sees another's history. Trace: `CLAUDE.md` §6.0, §6.3; BIPA §15(b).
2. The five stores keyed by profile; the gates closed with no profile chosen; the screens given a profile context;
   their locale keys in all eleven locales (`CLAUDE.md` §17).
3. `migration-keys.json`, `migration.rs`, `migration_flow.rs`, `SWR-MG-01`, `-04`, `-09`, the per-profile move and clear;
   the `OI-PRIV-19` rework of the same stores; `OI-PRIV-63` (3) is then a per-profile clear.
4. Under Q2 B: the per-profile key and backup key, designed with the cryptography review (`OI-EMMC2-02`), in
   `common/npps-core`.
5. Tests with `@verifies` tags; `check-migration-keys.ts`, `check-consent-reachability.ts`, `:core:jvmTest`.

**If Q1 B:**
1. `CLAUDE.md` §6.0 and §6.3 and `data-architecture-detail.md` §5.3 step 1a reworded (a `## CLAUDE.md` fragment);
   `OI-PRIV-34`'s C2 fallback and `UC-204` revisited.
2. `NPLimitsStore` limited to one individual profile, with the notice; a rule in `NP-SW-001`.
3. `np.onboarding.bipa-accepted` sorted in `migration-keys.json`.

Either way: an entry in `docs/status/completed-decisions.md`, and the warranty owner's store stays per helmet and
separate (`REQ-SHDR-08`, `-10`).

## 7. What I need back

- An option for Q1, and for Q2 and Q3 if Q1 is A.
- A sentence of rationale for each, for the requirement row.
- Anything that would reopen it (for example, a clinic deployment model where every patient has a phone).
- Your name, role and the date.
