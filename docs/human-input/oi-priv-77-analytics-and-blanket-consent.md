# Question for the principal: OI-PRIV-77, may app analytics be on while blanket research consent (L3) is withdrawn?

**Status:** DRAFT, not sent. Not a controlled document; see `docs/human-input/README.md`.
**GitHub issue:** [#1062](https://github.com/stevehickman/NeuroPulse/issues/1062)
**From:** NeurOne Software Engineering (`OI-PRIV-77` in `docs/status/pending-decisions.md`; raised while repairing `NP-PRIV-ANALYSIS-006` F-09)
**To:** the principal
**Response needed:** an option for Q1, and for Q2 if Q1 is A; a short rationale; anything that would reopen it; your name, role and the date.

## 1. The question

Two rules point different ways.

- **`REQ-CONSENT-14`** (`docs/reference/consent-engine.md:256`): *"The research analytics gate is closed unless research
  consent was accepted."* **`CLAUDE.md` §6.0** (line 339) and **`REQ-CONSENT-02` (b)** (`consent-engine.md:244`):
  withdrawing blanket consent *"tears down research analytics"*. Read together, analytics belongs to research consent.
- **What analytics actually sends** (`docs/np_app_telemetry_001.md` §3) is app usage: the app's version, the phone's
  class, which of four screens is open, app error codes, under a random ID. No health data, no session, no protocol, no
  consent screen. That is not research data, and **`REQ-CONSENT-28`** (`consent-engine.md:269`) built the Settings switch
  as a consent of its own, with a disclosure, a confirmation and a recorded withdrawal, that does not read L3.

Plain form: **is sharing app usage information part of research consent, or a separate question?**

Nothing changes until you answer. **No app sends an analytics event yet** (`OI-PRIV-05`: nothing calls `track`), so
nothing is exposed while this is open.

## 2. What is known

All of this was read in the code on 2026-11-25.

### 2.1 What the switch does today

| Where | What it shows |
|-------|---------------|
| `app/NeurOneUI/shared/src/commonMain/kotlin/life/neurone/shared/ui/SettingsScreen.kt:415` to `:454` | `ResearchAnalyticsSection`: turning the switch on opens a dialog; only its confirm calls `ConsentStore.grantResearchAnalytics()`. Turning it off calls `revokeResearchAnalytics()`. Nothing reads L3. |
| `locales/en.json:55` | The switch is labelled **"Research analytics"**. |
| `locales/en.json:926` to `:928` | The dialog it opens is titled **"Share app usage information?"**, its button **"Share usage information"**, and its body says the data never includes health data, sessions or protocols. The label and the disclosure already describe two different things. |
| `common/core/src/commonMain/kotlin/life/neurone/core/consent/ConsentStore.kt:554` to `:573` | `grantResearchAnalytics` sets the flag and starts the backend; `revokeResearchAnalytics` marks the withdrawal day if it was on and tears the backend down. |
| `ConsentStore.kt:468` to `:470`, `:487`, `:519`, `:798` | An L3 withdrawal (by the commit path, the withdraw call made locked or unlocked, or a restored withdrawal) calls `revokeResearchAnalytics()`. |
| `common/core/src/commonMain/kotlin/life/neurone/core/analytics/ResearchAnalyticsGate.kt:7` to `:18`, `:31` | The gate covers analytics **and crash reporting**, and its header still says it opens when the person "completed the consent onboarding flow". Its key is `np.research.consent-granted`. |
| `grep grantResearchAnalytics app common` | The Settings dialog is the only caller. Onboarding no longer opens the gate (`REQ-CONSENT-27`, F-10). |

**So today:** a person who withdrew L3 can turn analytics on again after the disclosure, and their next L3 withdrawal
turns it off again. A person who never touched research can turn it on.

### 2.2 What else says how analytics relates to research

- `NP-APP-TELEMETRY-001` §5 (line 229): *"If a user withdraws blanket research consent (L3), the research analytics SDK
  must be de-initialised"*; partial withdrawals do not revoke it. §5's binding requirement is that no SDK starts before
  the consent flow is completed on first launch. Neither says that analytics needs L3 to start.
- `REQ-CONSENT-03` (`consent-engine.md:245`) and `scripts/check-consent-reachability.ts`: the analytics teardown fires on
  the true-to-false transition of L3 at `updateResearchConsent`. These guard the teardown, not the grant.
- `NP-APP-TELEMETRY-001` §3.3: no event is named for a consent, so analytics cannot report research choices whatever the
  answer here.
- `common/npps/migration-keys.json`: the flag sits in the `consent` group, so it is never carried by key, and the
  withdrawal mark is per install (`REQ-CONSENT-28` (b)).

### 2.3 Related open items

- **`OI-PRIV-59`** (`docs/human-input/oi-priv-59-one-install-several-wearers.md`): the analytics ID and gate are one per
  install. If that item goes per profile, so does this consent, whichever option is chosen here.
- **`OI-PRIV-05`**: the event list and the relay. Not touched by this item.

## 3. Questions

**Q1. What does analytics require?**

- **A. Its own consent, independent of L1 to L4 (recommended).** The switch is renamed away from "research" to match the
  dialog it already opens (for example "Share app usage information"), and stays where it is in Settings.
  `REQ-CONSENT-14` is reworded to say the gate is closed unless the person turned analytics on after the disclosure
  (`REQ-CONSENT-28`). `CLAUDE.md` §6.0's teardown sentence, `REQ-CONSENT-02` (b) and `NP-APP-TELEMETRY-001` §5 are
  reworded according to Q2.
  - *For the person:* one question with its answer in front of them: "may NeurOne see how the app is used?" It does not
    depend on anything they decided about research.
  - *Cost:* the reword in `CLAUDE.md` (a `## CLAUDE.md` fragment), `consent-engine.md` and `NP-APP-TELEMETRY-001`; one
    renamed string key in eleven locales; code names (`grantResearchAnalytics`, `ResearchAnalyticsGate`) can stay or be
    renamed. The stored key `np.research.consent-granted` can keep its name: renaming a stored key is a migration for no
    gain to the person.
- **B. Analytics needs blanket consent (L3).** The switch is offered only while L3 is on, and `REQ-CONSENT-14` says so.
  - *For the person:* they must accept a study-participation posture (blanket consent to research, with its
    irreversibility notice, `REQ-CONSENT-07`) before they may share app usage that the disclosure says is not research
    data. Explaining why one depends on the other means explaining something that is not true of the data.
  - *Cost:* a check on the switch and a row change; the label may keep "research". Consent bundling: one purpose is
    made conditional on accepting another, which the consent model elsewhere keeps apart (L2 is scope, L3 is posture,
    `REQ-CONSENT-05`). Whether that is lawful is for counsel; the brief does not claim it is not.
- **C. Analytics needs some research consent (any of L1 to L3).**
  - *For the person:* the hardest to explain: agreeing to be contacted about studies (L1), or to one research area (L2),
    would let them share app usage, and the screens would have to say which.
  - *Cost:* as B, with three conditions instead of one, and a rule for what happens when the last of them is withdrawn.

**Q2 (only if Q1 is A). Does an L3 withdrawal still turn analytics off?**

- **A. Yes, and the person can turn it on again (recommended).** Today's behaviour, made explicit. A blanket withdrawal is
  the person saying "stop", and stopping app usage sharing with it errs on their side; the withdrawal screen says that
  usage sharing was turned off too and can be turned on in Settings. `REQ-CONSENT-02` (b), `REQ-CONSENT-03` and the
  reachability gate stay as they are; `CLAUDE.md` §6.0 says "turns off app usage sharing" instead of "tears down research
  analytics". Cost: one sentence on the withdrawal screen, eleven locales.
- **B. No: the two are fully independent.** An L3 withdrawal leaves analytics alone. Cost: `REQ-CONSENT-02` (b),
  `REQ-CONSENT-03`, `NP-APP-TELEMETRY-001` §5 and the teardown calls at `ConsentStore.kt:468`, `:487`, `:519` and `:798` change,
  with their tests and the reachability gate. A person who withdrew everything they could see under "research" keeps
  sending usage events, which they may not expect.

## 4. Which option is easiest to explain to a user

**A.** It is one switch with its own disclosure, which already exists and already says "app usage information". B and C
each need a sentence explaining why sharing app usage depends on a research choice, when the disclosure says the data is
not research data. Q2 A adds one sentence at the point of withdrawal, where the person is already reading.

## 5. Recommendation

**Q1 A, Q2 A.** It matches what the events are (`NP-APP-TELEMETRY-001` §3) and what the switch was built as
(`REQ-CONSENT-28`), keeps the guard that a blanket "stop" stops everything not essential, and is the easiest to explain.
Nothing is sent today, so no person is affected by the change of wording.

## 6. What follows a ruling (not started)

**If Q1 A:**
1. `REQ-CONSENT-14` reworded and `REQ-CONSENT-28`'s "Not decided" note removed, so the two say the same thing; under
   Q2 B, `REQ-CONSENT-02` (b) and `-03` too (`CLAUDE.md` §18: each with what fails without it).
2. `CLAUDE.md` §6.0's sentence (a `## CLAUDE.md` fragment) and `NP-APP-TELEMETRY-001` §5.
3. The switch's label key renamed in all eleven locales (`CLAUDE.md` §17), marked for legal review with the existing
   three strings; under Q2 A, the withdrawal screen's sentence.
4. `ResearchAnalyticsGate`'s header comment corrected.
5. Tests with `@verifies` tags: the switch turns on with L3 withdrawn (A); under Q2 A the existing teardown tests stand.

**If Q1 B or C:**
1. `REQ-CONSENT-14` and `-28` state the condition; the switch reads the consent store and is shown disabled, with a
   reason, while the condition does not hold.
2. What happens to analytics already on when the condition stops holding (under B this is today's teardown).
3. Tests with `@verifies` tags, and the reachability gate extended to the grant.

Either way: an entry in `docs/status/completed-decisions.md`, and `OI-PRIV-77` closes when the two requirements and the
switch agree.

## 7. What I need back

- An option for Q1, and for Q2 if Q1 is A.
- A sentence of rationale, for the requirement row.
- Anything that would reopen it (for example, a new event that carries research data, which `NP-APP-TELEMETRY-001` §3
  would first have to permit).
- Your name, role and the date.
