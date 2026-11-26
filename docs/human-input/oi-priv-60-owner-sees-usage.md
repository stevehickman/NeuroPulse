# OI-PRIV-60 (2): may a clinic that owns a helmet see which protocols its wearers have run?

**Status:** DRAFT, not sent.
**GitHub issue:** none yet. Rule 7 of `docs/human-input/README.md` asks for one; the session that wrote this did not open an issue unasked. The finding is F-04 of `NP-PRIV-ANALYSIS-006`.
**Sender:** the session that worked on `OI-PRIV-60` (a Claude agent), drafted 2026-11-25.
**Addressee:** the Principal, with the Privacy Lead.
**Owning open item:** `OI-PRIV-60` part (2) (`docs/status/pending-decisions.md`).
**Not a controlled document and not a design input** (`docs/human-input/README.md`): no serial, not in `NP-DHF-001`, and it writes no requirement (`CLAUDE.md` §18).

**How to read it.** **[E]** is established, with the file that shows it. **[P]** is proposed here.

---

## 1. What I need from you

| # | Question | Recommendation |
|---|----------|----------------|
| Q1 | Should the Helmet owner page ever show which protocol families have been run? **A:** never; a clinic that needs to know what a wearer ran asks the wearer for a clinician grant (`CLAUDE.md` §6.1), which already exists for that purpose. **B:** yes, when the wearer has consented, as a clinician use case. **C:** yes, as a count across the helmet's wearers, with no wearer named. | **A** |
| Q2 | Only if B or C: whose consent, recorded where, and what does the page show for a wearer who did not consent? | — |

What to send back: the option for each, a short reason (`CLAUDE.md` §18), anything that would reopen it, and your name, role and date.

## 2. The problem

- **[E]** The Helmet owner page used to mark each protocol family "used" (`members.any { library.isUsed(it) }` in `app/NeurOneUI/shared/src/commonMain/kotlin/life/neurone/shared/ui/WarrantyOwnerScreen.kt`).
- **[E]** Which protocols a person has run says something about that person, so it is User Health Data Record (UHDR) data (`CLAUDE.md` §5.1, the defining test). The warranty owner is not assumed to be the wearer and may be a clinic (`CLAUDE.md` §6.0).
- **[E]** The marker has been removed while this question is open (`OI-PRIV-60`; `NP-SW-001` `SWR-WO-01`). Nothing about usage is shown to the owner today.
- **[E]** The used flag is one bit per family, kept in the protocol library on the phone (`SWR-WO-02`, `np.used-families`). It names no helmet and no person: it means "run from this phone's library, on some helmet, by someone". So it cannot be narrowed to one wearer's consent, or to one helmet, as it is stored.
- **[E]** A clinician reaches a wearer's UHDR through a use case the wearer grants: per element, per use case, time-limited, audited and revocable (`CLAUDE.md` §5.1, §6.1). Clinicians choose use cases, never data elements.

## 3. The options

**A. Never on the owner page.** [P] The owner page stays about the helmet. A clinic that needs to know what a wearer ran is acting as a clinician and asks that wearer for a grant, through the existing consent path. *Costs:* a clinic sees usage only through a grant. *Gives:* nothing new to build; the owner page holds nothing about a person, which is what its header already says; the warranty owner and the clinician stay separate roles, as `CLAUDE.md` §6.0 and §6.1 already make them.

**B. On the owner page, with the wearer's consent.** [P] The page shows a family as run when a wearer who consented ran it. *Costs:* the used flag would need to be recorded per wearer and per helmet, which `SWR-WO-02` forbids (it carries no device, user, place, count or time, because those say something about a person). The owner page cannot tell who is holding the phone. It would be a second consent path beside §6.1 for the same data. Not recommended.

**C. A count across wearers.** [P] *Costs:* on a helmet with one wearer, or a count of one, the count names the person. It would need the same per-wearer record as B, plus a minimum group size. Not recommended.

## 4. What is assumed

- That a clinic which owns a helmet and also treats the wearer can act as a clinician under §6.1 when it needs clinical data. Not checked against any clinic's workflow.
- That no regulatory or service reason needs the owner to see usage. None was found in `docs/reference/service-network.md` or `docs/reference/durability-maintenance.md`; a usage count for wear would be helmet data (SHDR) from the helmet, not the protocol library's flag.

## 5. What changes with each answer

- **A:** `OI-PRIV-60` closes; nothing else changes.
- **B or C:** a design for a per-wearer usage record, a change to `SWR-WO-02`, and a new clinician use case under `OI-CONSENT-08`, before anything is shown.
