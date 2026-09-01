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

---

