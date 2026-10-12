# Consent engine — screen rationale, POA workflow, research portal

> Relocated from CLAUDE.md §6.2 (the §6.2.1–§6.2.5 rationale, POA workflow and vulnerable-population
> disclosure) and §6.3 (Rev 40) to slim the always-loaded core. Content is verbatim; section numbers
> are unchanged. CLAUDE.md §6 keeps the two consent subjects (§6.0), the L1–L4 / S1–S2 map and the
> binding invariants — including that Select-all does not enable blanket consent and that the
> analytics teardown fires on a true→false transition at the store ingestion point.
>
> **Read this file when:** implementing or reviewing consent UI on either platform, changing
> `ConsentStore`, working on the research portal, or answering *why* the screens are shaped this
> way. `scripts/check-consent-reachability.ts` guards the defect §6.2.5 was written against.
>
> The clinician use-case subscription tiers (§6.1) are in `docs/reference/commercial-model.md`.

### 6.2 A priori research consent — rationale and workflows (detail)

**The four layers in full.** CLAUDE.md §6.2 carries the screen map and the binding invariants; the
complete per-layer table — including the L3 irreversibility notice as it is displayed — is here.

| Layer | Question | If yes | If no | Brand ambassador mechanism |
|-------|----------|--------|-------|--------------------------|
| L1 — Contact consent *(S1)* | Can we reach you about future research opportunities? | Provide contact method + frequency limit. POA holders upload POA (human review, 3 business days, jurisdiction-flagged, annual re-verification) | No contact. All features unchanged. | Being asked creates perceived agency → trust baseline |
| L2 — Category consent *(S2)* | Which research areas? (9 categories: AD/dementia, Depression, PTSD, TBI, Sleep, Attention, Parkinson's, Healthy ageing, Visual health) | Per-project contact for selected categories only. Each project is a fresh decision. **A Select-all affordance sets all nine; it does NOT enable L3** — see the auto-enable decision below. | Not contacted for that category. | Personal category choice deepens engagement |
| L3 — Blanket consent *(S2)* | Pre-approve all NeurOne-reviewed research? | Data included in all studies. **Still receives per-study engagement notifications** (not consent requests — maintains engagement, can opt out per-study). anonymization: k≥10, no IDs, no sub-weekly timestamps. **Irreversibility notice displayed whenever this layer's control is on:** "Once your anonymized data has been included in a published study, it cannot be individually withdrawn from that dataset. However, because NeurOne anonymises your data fresh from your device for each study, withdrawing consent immediately and permanently stops any further data flowing to any future dataset — including data from sessions that occurred before your withdrawal." | Per-category and per-project process applies. | Blanket patients kept engaged — not taken for granted |
| L4 — Results + community *(S1)* | Hear study results? Join suggestion portal? | Plain-language results notification per study (including null results) + paper link + "suggest next steps" link. Access to suggestion/voting/pledge portal. | No results contact, no portal. | Results notification is the highest-value brand moment |



#### 6.2.1 Why L1 and L4 share a screen

L1 and L4 were always the same question. L1 asks *may we contact you*; L4 asks *what about*. A
contact method is the shared precondition for all three delivery paths — per-study invitations
(L1+L2), per-study engagement notifications (L3), and results notifications (L4). Asking for
permission on one screen and topics on another was two screens for one decision. No consent
axis is merged here; only a contact method and the topics it is used for.

#### 6.2.2 Why L2 and L3 share a screen but not a control (locked)

**Selecting all nine L2 categories is NOT equivalent to L3 blanket consent.** They are
orthogonal axes:

- **L2 is scope** — which research areas.
- **L3 is posture** — ask-me-each-time versus pre-approved.

L2-all-categories means *"ask me about everything."* L3 means *"stop asking me."* The position
"everything, but ask me" is real — arguably the most engaged position a user can hold — and is
expressible only while both axes survive.

S2 therefore carries **two controls**: the nine category checkboxes with a Select-all
affordance, and a separately labelled blanket toggle. One step; both axes intact; both
withdrawal semantics distinct.

**What a full collapse would have cost, had it been adopted:** the "everything, but ask me"
position becomes unexpressible; L2's *each project is a fresh decision* property is destroyed
for any user wanting broad scope; and the two withdrawal semantics fuse. Category withdrawal
does not tear down research analytics and blanket withdrawal does (§6.0), so a single control
means a single withdrawal — either every category withdrawal starts tearing down analytics, or
blanket withdrawal stops. The latter is exactly the regression caught in review on 2026-06-16.

#### 6.2.3 Select-all does NOT auto-enable the blanket toggle (locked)

Ticking nine boxes expresses breadth of *interest*; the blanket toggle surrenders the *right to
be consulted*. Inferring the second from the first attributes to the user a decision they did
not make. Auto-enabling also fails silently in both directions and asymmetrically: a user
auto-escalated to blanket stops receiving consent requests they wanted, with no event to
notice, and if they later switch it off they get a research-analytics teardown they never asked
for. Per the conservative-claim rule, the option that asserts least and preserves the ability to
be asked wins.

The usability objection — that only a privacy engineer perceives the distinction — is answered
with **copy, not state**: enabling Select all surfaces an inline note ("You'll still be asked
before each individual study. To stop being asked, turn on the setting below"), and the blanket
toggle's label states what it changes in plain words rather than naming a tier.

#### 6.2.4 Presenting L4 first without it becoming an inducement

L4 offers results notification and the suggestion portal; both presuppose participation, so
showing them first risks reading as *here is what you get, now consent*. The framing follows the
precedent in `docs/np_mod_id_001.md` §7.5, where reciprocity is described as an honest exchange
rather than a bolt-on incentive, and §7.5.2's non-coercion invariant. Three binding copy rules:

1. **Conditional framing.** S1 says *"if your data ever contributes to a study."* The benefit is
   contingent on a decision the user has not yet been asked to make; a conditional cannot induce
   satisfaction of its own condition.
2. **The exchange is symmetric and stated as such.** A study that uses your data and never tells
   you what it found has taken something and returned nothing. Results notification is the other
   half of one transaction, not a reward for completing the first half. **Null results are named
   explicitly** — that is what separates a genuine exchange from marketing.
3. **Non-coercion, stated on the screen.** Opting into results or the portal grants **no** data
   access, and declining costs nothing. The reciprocity buys information, never participation.

#### 6.2.5 Fewer steps to grant must not mean coarser withdrawal (locked)

Withdrawal remains at study, category, and blanket granularity, with the §6.0 scoping rules
unchanged. Because S2 commits both L2 and L3 together, the blanket→analytics teardown is
enforced at the **store ingestion point** (`updateResearchConsent`) on a true→false *transition*
of blanket consent, not only in the explicitly-named `withdrawBlanketResearchConsent()`.
Guarding the transition rather than the value is what keeps a category-only edit from triggering
teardown — the same regression inverted.

**POA workflow:** POA holder uploads executed healthcare POA → human review 3 business days → jurisdiction flagging → scope limitation noted → annual re-verification. If patient regains capacity, all proxy consent decisions presented for ratification or revocation. Research contact goes to POA holder only.

**Vulnerable population disclosure:** At per-project consent time, explicitly state: "Once your anonymized data is included in a study, individual withdrawal is not possible from that dataset — this is a fundamental property of k-anonymized aggregate data and is required by Common Rule (45 CFR 46). However, because NeurOne anonymises your data fresh from your device for each new study, withdrawing consent immediately and permanently prevents any further data from flowing to any future dataset — including data from sessions that occurred before your withdrawal. Your historical sessions remain on your device under your sole control."

### 6.3 Research suggestion portal (three functions)

1. **Patient research agenda:** Patients submit study ideas in plain language, community votes ("interested"), comments, expresses participation intent. Top suggestions visible to researcher community.

2. **Pre-identified subject pool:** "Would participate" intent flag creates pre-screened, device-familiar, motivated cohort. Researcher portal shows willing participant count, geographic distribution, anonymized device usage profiles per suggestion. Solves researchers' hardest problem (recruitment = 40–60% of trial cost) before grant is written.

3. **Crowdfunding catalyst:** Pledges ($10–$100+) are intent, not charges. When researcher confirms pilot feasibility, formal campaign activates. Escrow held until target met; refunded if not. Released to institution research account. NeurOne contribution matching for strategic studies. Pilot data (even n=20–30) supports NIH SBIR/R21 application. Funders receive results notification + paper acknowledgement as "NeurOne Patient Research Fund contributors."

**Per-project contact workflow:**
1. NeurOne reviews study (use case library, minimum necessary data, IRB verification)
2. Eligible patient list generated by device ID + contact prefs only (no UHDR)
3. personalized invitation from NeurOne (not researcher) — personal tone, specific about study, explicit about what researchers CAN and CANNOT see
4. Patient decision: Yes / No / Ask a question (secure message to NeurOne liaison, 2 business day response). Invitation includes irreversibility notice: data already included in published studies cannot be individually removed; consent withdrawal blocks all future data flows from any time period.
5. Results notification closes loop for all who opted in (including null results). Users who later withdrew consent still receive results for studies they previously participated in — notification only, no new data.
6. Consent withdrawal effect: device immediately stops processing study descriptors; no further extracts generated or transmitted, for any data period including historical sessions.

**Device-side ingestion gate (added 2026-09-10, closes `OI-CONSENT-03`).** Steps 1–3 above are
NeurOne's; the device does not take them on trust. What crosses onto the device is the **signed
study descriptor** of §5.3 step 1 — study ID, title, categories, requested UHDR elements, `k`,
date-rounding interval, signature — and the invitation the user reads is *derived* from it here,
after the signature verifies. It used to be the other way round: `ConsentStore.addInvitation()` took
a finished invitation, "what they CANNOT see" prose and irreversibility notice included, from
whatever called it.

`ConsentEngine.admit()` decides whether a descriptor may become something the user sees. **The order
is load-bearing**, and each step traces to a locked line:

| # | Check | Refusal | Source |
|---|-------|---------|--------|
| 1 | Signature verifies | `verifierUnavailable` · `signatureInvalid` | §5.3 step 1. First, so nothing an unverified descriptor *claims* reaches a later step |
| 2 | `k ≥ 10` and date rounding `≥ 7` days | `anonymisationBelowFloor` | §5.3. The device is the only place these can be enforced, because §5.3 puts the anonymisation on the device — a study below the floor is not one a user may be *asked* about |
| 3a | Blanket consent not withdrawn | `researchConsentWithdrawn` | §6.0 — *withdrawal stops **ALL** research data flows*. Ahead of 3b because a withdrawn user usually still passes it: withdrawal does not un-tick the nine L2 categories |
| 3b | Any research consent at all | `noResearchConsent` | §6.0 |
| 4 | L1 contact consent | `noContactConsent` | §6.2.1 — a contact method is the precondition for **all three** delivery paths, so it gates the engagement notification too |
| 5 | Study not already answered, joined or withdrawn from | `studyAlreadyDecided` | §5.3, §6.3 step 6. Re-presenting a study is the device forgetting an answer the user gave |
| 6 | L3 on → engagement notification; else an L2 category must match | `categoryNotConsented` | §6.2.2 |

**Step 3a is the second place a boolean could not hold a decision.** `blanketConsentGranted ==
false` describes a user who never turned L3 on *and* a user who turned it off, and §6.0 treats
those differently: the second stopped everything. Withdrawal deliberately does not clear the L2
categories — destroying a stored preference is its own decision, and not this one — so without a
separate marker those nine stale checkboxes go on admitting studies as consent requests, which is
the flow §6.0 says stops. `ResearchConsentState.blanketConsentWithdrawnAt` records the true→false
transition. Three properties of it are load-bearing:

- **The store sets it, never a screen.** The consent UI commits a whole `ResearchConsentState` and
  cannot see a transition; a screen able to write this field could erase the record of the user
  having said stop by committing a stale snapshot.
- **It is guarded on the transition, not the value** — the same shape as §6.2.5's analytics
  teardown, and for the same reason. Withdrawing consent that was never granted stops nothing, and
  marking it would silently bar an L2 participant from studies they still consent to.
- **Re-granting L3 clears the condition, not the record.** `blanketConsentWithdrawn` is
  `blanketConsentWithdrawnAt != nil && !blanketConsentGranted`, so a fresh grant resumes admission
  while the withdrawal stays in the history.

The dashboards render three postures rather than two for the same reason: telling a withdrawn user
*"you are asked about each study"* while the gate refuses every study they are sent is the one
reading that is plainly false.

**Step 6 is why this needed a data-model change and not wiring.** L3 users "still receive per-study
*engagement* notifications, **not consent requests**" (§6.2), so one study reaches an L2 and an L3
user as two different objects whose **defaults on silence are opposite**: an unanswered consent
request means *not participating*; an unread engagement notification means *participating*, because
L3 already answered. `StudyInvitation.Posture` carries which. An engagement notification records
participation at ingestion, cannot be "accepted", and its decline is a withdrawal — §6.2's *can opt
out per-study*, routed through the same `withdrawFromStudy` the dashboard already uses.

**What the invitation may say.** `cannotLearn` is the complement of the approved element set,
computed on-device — step 3 requires the invitation to be explicit about what researchers cannot
see, and taking that half as prose from the party asking for access lets that party write it and
lets it drift from the first half. The irreversibility notice is the §6.2 copy from a locale key.
`studyTitle` is the one field that stays server-supplied text: it names a specific study the way a
part number names a part (CLAUDE.md §17).

**Step 4's third response exists** — `askQuestionAboutInvitation()` leaves the invitation open,
because asking is not deciding, the rule §6.1's `askQuestionAboutExpansion` already held.

**What is not built:** the transport. No descriptor fetch, no signing key, no outbound channel for
the question (`OI-CONSENT-07`). The inbound end is nevertheless **closed by default**, not merely
uncalled: `StudyDescriptorVerifier`'s only implementation refuses everything, so a transport cannot
be wired up without also supplying §5.3's signature check.

---

---

### 6.4 Requirements

*(Added 2026-10-11.)* Each row states a rule CLAUDE.md §5, §6 or this file already makes binding; the ID only makes
it citable, and adds nothing to what the rule requires. A test names the row it verifies with
`@verifies REQ-CONSENT-NN` (`scripts/req-trace.ts`). Where a row lists (a), (b), (c), a test claims a part
with `@verifies REQ-CONSENT-NN#b`. Every row binds both platforms.

| ID | Requirement | What fails if it is not met | Traced to | Verification |
|---|---|---|---|---|
| **`REQ-CONSENT-01`** | **The two consent subjects are independent.** (a) `SHDRUploader` has no reference to `ConsentStore`. (b) Revoking either consent leaves the other as it was. (c) A clinic that registers a warranty has not consented for any patient; each patient on a clinic device has their own `ConsentStore` | A warranty owner's registration would stand in for a wearer's consent, or revoking research consent would silence device-health upload (or the reverse) | CLAUDE.md §6.0 | Test. (a) is a source property, checked on the uploader's source |
| **`REQ-CONSENT-02`** | **Withdrawal has the scope that was withdrawn.** (a) Withdrawing a study or a category stops only that flow and does not tear down research analytics. (b) Withdrawing blanket consent (L3) stops all research flows and tears down research analytics | If category withdrawal tore analytics down, every edit of a category would; if blanket withdrawal did not, "stop" would leave research analytics running | CLAUDE.md §6.0, §6.2; this file §6.2.2, §6.2.5 | Test |
| **`REQ-CONSENT-03`** | **The analytics teardown fires on the true→false transition of blanket consent, at `updateResearchConsent`.** (a) A commit that turns blanket consent off tears analytics down. (b) A category-only edit does not. (c) A commit that leaves blanket consent on does not | S2 commits L2 and L3 together, so a guard on the value fires on every category edit, and a guard only in `withdrawBlanketResearchConsent()` is bypassed by the commit path. That was the regression caught in review on 2026-06-16 | CLAUDE.md §6.2; this file §6.2.5; `scripts/check-consent-reachability.ts` | Test, plus the reachability gate |
| **`REQ-CONSENT-04`** | **"Never asked" and "said stop" stay distinguishable, and the store alone records the second.** (a) `blanketConsentWithdrawnAt` is set by the store on a true→false transition, never by a screen. (b) A category-only edit does not set it. (c) A stale commit cannot clear it. (d) Withdrawing consent that was never granted does not set it. (e) While withdrawn it outranks the stale L2 ticks: studies those categories would admit are refused. (f) Re-granting L3 resumes admission and keeps the record | A boolean cannot tell a user who never turned L3 on from one who turned it off. Without the marker, nine stale L2 ticks keep admitting studies as consent requests after the user said stop; a screen able to write it could erase the record by committing a stale snapshot; marking a never-granted withdrawal would bar an L2 participant from studies they still consent to | CLAUDE.md §6.0; this file §6.3 (device-side ingestion gate) | Test |
| **`REQ-CONSENT-05`** | **L2 is scope and L3 is posture, and they stay separate.** (a) Select-all sets all nine L2 categories and never enables L3. (b) "Everything, but ask me" (all nine categories, no blanket) is representable. (c) Blanket consent with no category is representable. (d) All four layers are independently settable | Ticking nine boxes expresses breadth of interest; the toggle surrenders the right to be consulted. Inferring the second from the first attributes to the user a decision they did not make, and fails silently in both directions | CLAUDE.md §6.2; this file §6.2.2, §6.2.3 | Test |
| **`REQ-CONSENT-06`** | **Onboarding is two screens whose controls follow the layers.** S1 carries L4 and L1; S2 carries L2 and L3. S2 has two controls, the category checkboxes with Select-all and a separately labelled blanket toggle | A single control means a single withdrawal: either every category withdrawal starts tearing analytics down, or blanket withdrawal stops | CLAUDE.md §6.2; this file §6.2.1, §6.2.2 | Test of the step count; inspection of the screen composition |
| **`REQ-CONSENT-07`** | **L3 carries an irreversibility notice whenever it is on, and per-project consent repeats it for vulnerable populations.** The notice is the §6.2 copy from a locale key | A user who grants blanket consent without being told that already-included anonymized data cannot be individually removed has not given informed consent (45 CFR 46) | CLAUDE.md §6.2; this file §6.2, vulnerable population disclosure | Test that the notice is present; inspection of the copy |
| **`REQ-CONSENT-08`** | **Presenting L4 first is not an inducement.** (a) S1 uses conditional framing ("if your data ever contributes to a study"). (b) Null results are named explicitly. (c) The screen states that opting into results or the portal grants no data access and that declining costs nothing | "Here is what you get, now consent" reads as a reward for consenting; a conditional cannot induce satisfaction of its own condition | CLAUDE.md §6.2; this file §6.2.4; `docs/np_mod_id_001.md` §7.5.2 | Inspection of the S1 copy in `locales/` |
| **`REQ-CONSENT-09`** | **The device does not trust the server: `ConsentEngine.admit()` checks a signed study descriptor in a fixed order, and a failure stops the study.** (a) The signature verifies first, so nothing an unverified descriptor claims reaches a later step. (b) `k` ≥ 10 and date rounding ≥ 7 days. (c) Blanket consent has not been withdrawn. (d) The user has some research consent. (e) L1 contact consent is on, for both delivery paths. (f) The study is not already answered, joined or withdrawn from. (g) With L3 on the study is an engagement notification, otherwise an L2 category must match | A forged or unverified descriptor shows the user an invitation derived from claims nobody checked; a study below the anonymization floor is not one a user may be asked about, and the device is the only place the floor can be enforced; re-presenting a study is the device forgetting an answer | CLAUDE.md §5.3, §6.3; this file §6.3 (device-side ingestion gate) | Test |
| **`REQ-CONSENT-10`** | **The descriptor's signature covers what the user is shown, and the gate is closed by default.** (a) Altering any covered field makes verification fail. (b) A descriptor signed by another key is rejected. (c) An absent or unusable key yields no verifier, and with no verifier every descriptor is refused. (d) The canonical form is one byte sequence per descriptor, independent of element order and unable to be forged by a separator inside the title | A signature that leaves a field uncovered lets the party asking for access change it; a transport wired up without the signature check must not admit anything (`OI-CONSENT-07`) | CLAUDE.md §5.3, §6.3; this file §6.3 | Test |
| **`REQ-CONSENT-11`** | **A consent request and an engagement notification are different objects.** (a) An unanswered consent request means not participating; an unread engagement notification means participating. (b) An engagement notification records participation at ingestion and cannot be accepted. (c) Declining one is a withdrawal, routed through `withdrawFromStudy`. (d) Asking a question about an invitation leaves it open | Silence means opposite things in each: one posture handled as the other makes an L3 user's opt-out a decline, or an L2 user a participant | CLAUDE.md §6.2; this file §6.3 | Test |
| **`REQ-CONSENT-12`** | **What an invitation says a researcher cannot see is computed on-device as the complement of what the descriptor asked for.** It is never taken as prose from the party asking | The party asking for access would write the half of the invitation that limits it, and it would drift from the half that requests it | CLAUDE.md §6.3; this file §6.3 | Test |
| **`REQ-CONSENT-13`** | **Withdrawal permanently blocks future extracts for every data period, including sessions that predate it.** (a) After blanket withdrawal the device stops processing study descriptors, whatever their issue date, categories or elements. (b) After withdrawing from a study, that study is never processed again. (c) No further extract is generated or transmitted, for any data period. (d) Results notifications for studies already joined continue and carry no new data. **Partly built:** the admission gate (a, b) exists; extract generation, transmission and results notifications (c, d) do not (`OI-CONSENT-07`) | Withdrawing would stop only future sessions and leave history flowing, which the irreversibility notice promises does not happen | CLAUDE.md §5.3; this file §6.3 (per-project contact workflow, step 6) | Test |
| **`REQ-CONSENT-14`** | **The research analytics gate is closed unless research consent was accepted.** An absent or false consent key, an onboarding screen having been shown, and skipping onboarding all leave it closed; with it closed the analytics backend is not called | Analytics that run before or without consent are research data flowing without research consent | CLAUDE.md §6.0, §6.2 | Test |
| **`REQ-CONSENT-15`** | **A clinician selects use cases, never data elements, and the system derives the minimum.** (a) The elements granted are the union of those the selected use cases need and never more. (b) A selection above the tier is clamped, never honoured. (c) The research tier yields no elements whatever is selected. (d) The user is given a plain-language statement of what the clinician can and cannot learn | A clinician able to pick elements can over-reach the minimum necessary, and a grant derived from the tier alone gives access nobody chose | CLAUDE.md §6.1 | Test |
| **`REQ-CONSENT-16`** | **Retroactive and prospective access are always separate decisions.** A grant's reach in time is scoped by its own decision and is never derived from its tier. (a) The two decisions are recorded separately. (b) A forward-only grant leaves earlier sessions out of reach. (c) A grant that predates the workflow keeps its tier meaning | Granting access to the future would grant the past with it, or the reverse | CLAUDE.md §6.1 | Test |
| **`REQ-CONSENT-17`** | **A proxy consents only on verified authority, and the proxy's decisions stay revisable.** (a) A POA holder uploads the executed healthcare POA. (b) A human reviews it, within 3 business days. (c) The review flags the jurisdiction. (d) The review notes any limit on the POA's scope. (e) The POA is re-verified annually. (f) If the patient regains capacity, every proxy consent decision is presented to the patient for ratification or revocation. (g) Research contact goes to the POA holder only. **Not built:** nothing in the app implements this workflow | A proxy acting without verified authority consents for a patient who has not; a proxy decision the patient cannot revisit outlives the incapacity that justified it; contact reaching the patient as well as the proxy defeats the proxy arrangement | CLAUDE.md §6.2 (L1: "POA holders upload POA"); this file §6.2 (POA workflow). The 3-business-day and annual figures are the workflow's own: `UC-081` | Test, once built. Today no test exists and the row is expected to read untested |
