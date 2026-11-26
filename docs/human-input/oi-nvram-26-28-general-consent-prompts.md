# OI-NVRAM-26 / OI-NVRAM-28: three choices the general-consent prompts leave to the principal

**Status:** DRAFT, not sent.
**GitHub issue:** none yet. Rule 7 of `docs/human-input/README.md` asks for one. The session that wrote this did not open one unasked.
**Sender:** the session that built the app side of `OI-NVRAM-26` and `OI-NVRAM-28` (a Claude agent), drafted 2026-11-26.
**Addressee:** the Principal, with the Privacy Lead.
**Owning open items:** `OI-NVRAM-26`, `OI-NVRAM-28` (`docs/status/pending-decisions.md`; `NP-FW-NVRAM-001` §13).
**Not a controlled document and not a design input** (`docs/human-input/README.md`). It has no serial, is not in `NP-DHF-001`, and writes no requirement (`CLAUDE.md` §18).

**How to read it.** **[E]** marks what is established, with the file that shows it. **[P]** marks what is proposed here, or built as an interim default until you rule.

---

## 1. What I need from you

| # | Question | Interim behaviour, built | Recommendation |
|---|----------|--------------------------|----------------|
| Q1 | **How are the added items worded?** Prompt 2 names each item the review gate adds "in plain words" (`REQ-SHDR-12` (b)). **A:** a plain list, one short phrase per item, written when the gate rules and checked by you with that update. **B:** a phrase per item plus one sentence on why predictive maintenance needs it. **C:** your own wording, supplied with the gate's result. | No item has words yet, because the gate has not ruled and the shipped set is the baseline. A prompt whose items the app cannot all name is not shown and not recorded. | **A**, and you sign off the phrases with the update that ships the set. |
| Q2 | **What happens if an update's content set is malformed?** That covers versions that do not rise by one from the baseline, a repeated or badly formed item id, or an item with no words. **A:** show nothing and change nothing, as silence would (fail closed). **B:** A, and also tell the owner that an update could not be read and to update the app. **C:** A, and report it in SHDR as a device-condition fault. | **A.** No prompt is due, an answer is refused, and the owner's consent and content level stay as they were. CI also checks the shipped history and the item words (`GeneralShdrConsentPromptsTests`, `GeneralConsentPromptTest`), so a malformed set should not ship. | **A.** Because the set is compiled into the app (§2), a malformed set is a build defect that CI catches, not a run-time event to explain to an owner. C would put an app-build fact into SHDR, which describes device condition. |
| Q3 | **Is prompt 2 also put to an owner who said stop?** The ruling says the notice is broadcast to all warranty owners, and each owner's app then shows prompt (2). **A:** yes, to every owner who has answered the reports question, yes or no. **B:** only to owners whose reports are on. | **A**, the plain reading of "all warranty owners". An owner never asked is not prompted. They are asked when they first turn reports on, with the set in force, and that grant answers prompt 2. | **A**, unless you read a renewed question to someone who said stop as solicitation. Then choose **B**. |

What to send back: the option for each question, a short reason (`CLAUDE.md` §18), anything that would reopen it, and your name, role and date.

## 2. What is built, and the one design choice made in building it

- **[E]** The consent state per helmet is in the warranty owner's store (`WarrantyOwnerRecord`, `common/core/.../analytics/WarrantyOwnerStore.kt`, line format v4). It has these fields:
  - `fleetDecided`: "never asked" is distinct from "said stop", and a record made by the stealth lock alone is not an answer.
  - `contentLevel`: the content-set version the owner said yes to.
  - `windowEnded`: its own state, distinct from a revoke and a withdrawal.
  - `windowAsked` and `contentAsked`: each prompt shows once.
- **[E]** `GeneralShdrConsentPrompts` decides when each prompt is due and applies the answer:
  - Agree grants at the version shown.
  - Keep, a dismissal or no answer changes nothing.
  - Stop revokes.
  - No answer enrols characterisation.
  - After a window has ended, `CharacterisationEnrolmentGate` refuses enrolment in the same programme.
- **[E]** The owner's page shows the prompt for the connected helmet (`GeneralConsentPrompt.kt`, `WarrantyOwnerScreen.kt`). Its text is in all eleven locales, in English until translated.
- **[P] How the gate's result is broadcast:** as a new entry in `GeneralShdrContent.history`, compiled into an app release with its item words. Every install of a release gets the same set. Nothing is fetched, no server learns who received it, and nothing names an owner. This is the plainest reading of "broadcast through an app update mechanism". If you meant a separately signed content file that the helmet can also verify, say so. That changes `OI-NVRAM-28` (1), not the prompts.

## 3. What is not built, and why it is not asked here

- **[E]** The window's end does not reach the app. The hub decides it in firmware (`np_accel_char_window_open`: a build constant and a record budget, `NP-FW-EMMC-002` §H.3.4) and publishes nothing the app can read. `CharacterisationEnrolmentGate.endWindow` exists and nothing calls it yet. Firmware has to supply this (`OI-NVRAM-26`). It is engineering work, not a question for you.
- **[E]** The helmet does not enforce the content level. The app records the version the owner agreed to, but the uploader sends the hub's record as delivered (`REQ-SHDR-03` (c)). Only the helmet can withhold a field above that version (`OI-NVRAM-28` (1), (3)).
