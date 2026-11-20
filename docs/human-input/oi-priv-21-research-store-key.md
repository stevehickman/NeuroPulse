# Question for the principal: OI-PRIV-21, what identifies a device in the research store

**Status:** DRAFT, not sent. Not a controlled document; see `docs/human-input/README.md`.
**GitHub issue:** none yet.
**From:** NeurOne Software Engineering (`OI-PRIV-21` in `docs/status/pending-decisions.md`; raised by the principal's ruling on `OI-PRIV-07` Q3, 2026-11-20)
**To:** the principal
**Response needed:** an option for Q1, and Q2 if it applies; a short rationale; anything that would reopen it; your name, role and the date.

## 1. The question

`CLAUDE.md` §5.3 step 4 (`data-architecture-detail.md` §5.3): *"NeurOne servers store extract keyed to study ID and device ID only … No linkage table exists that could re-identify users."* `NP-IRB-001` §5 and its Type B row say the same, and the IRB exemption argument leans on it.

Plain form: **what is the "device ID" in that key, and does keying by it re-create the (study, device) link that `OI-PRIV-07` kept out of SHDR?**

## 2. What is known

- **No document defines the device ID.** §5.3 step 4, `NP-IRB-001` §5 and the §94 row use the words and say nothing else. No extractor exists (`OI-CONSENT-07`), so no code or schema decides it.
- **Two device-linked identifiers exist, and a key that is either one joins:**
  - The SHDR fleet database is keyed by the opaque 32-byte `warranty_token` (`ci/shdr/shdr_fleet_schema.sql`, `devices`). It is the only permitted link between SHDR and the warranty database (`NP-FW-EMMC-002` §A.3).
  - The warranty registration database holds `serial_number` beside the registrant's name, email and address (`NP-FW-EMMC-002` §A.2).
  - So an extract keyed by the **token** joins the research store to SHDR (study to fleet record). One keyed by the **serial** joins it, through the warranty database, to a named person, who may be a clinic and not the wearer (`CLAUDE.md` §6.0).
- **Even a device ID that joins neither** lets NeurOne link every extract one device sends across all studies, which is a longitudinal link on top of a k-anonymised extract.
- **The research store's receipt is NeurOne's only record of extracts** after `OI-PRIV-07` (the audit trail is UHDR). Its key decides what NeurOne can reconcile and what it can learn.
- **The on-device budget** (`NP-FW-ANON-001` §8.1, ε at most 1.0 per study) is enforced on the device. The server does not need a device key to enforce it.
- **Assumed, not established:** that the upload leaves with the phone's or hub's network address visible to NeurOne. `OI-PRIV-05` rules on analytics events and the fleet path (Oblivious HTTP relay), and does not mention research extracts.

## 3. Questions

**Q1. What may identify a device in the research store?**

- **A. Nothing.** An extract carries study ID only. NeurOne keeps per-study counts. *Cost:* no per-device duplicate or lost-extract check on the server; the same device sending twice is indistinguishable from two devices. The on-device budget still stops over-contribution.
- **B. A per-study pseudonym.** The device derives `HMAC(device secret, study ID)`, stable within a study and unrelated across studies, and not the token, the serial or any value that appears in SHDR or the warranty database. The secret is held in UHDR, so NeurOne cannot compute it. *Cost:* a new on-device secret and derivation (cryptographic, so `common/npps-core`, `CLAUDE.md` §22); a restore on a new phone loses it, so that device counts as a new contributor; within one study NeurOne can still link that study's extracts from one device, which a dedup needs.
- **C. Keep a device ID as written (token or serial).** *Cost:* the joins above. §5.3's "no linkage table" would be false and the `NP-IRB-001` exemption argument would need review.

**Q2. Under A or B, what does §5.3 say about linkage?** Reword "no linkage table exists" to say what is and is not linked: per-study (A) or per-study-and-device (B) only; nothing joins to SHDR, the warranty database or a person; and the network address does not reach the research store (needs a ruling on the relay for extracts). **Do you rule that extracts also go through the relay of `OI-PRIV-05`, or leave the address question to its own item?**

## 4. Recommendation

**Q1: B. Q2: reword as stated; raise the relay-for-extracts question as its own open item.**

- A device key is needed for duplicate and loss checks, and a per-study pseudonym gives them without any join to SHDR, the warranty database or another study.
- A is the stronger privacy answer and costs reconciliation; B keeps the reconciliation NeurOne loses under `OI-PRIV-07` Q1 A, at the price of a within-study link.
- C fails the §5.1 reasoning the ruling on `OI-PRIV-07` used.

## 5. What each ruling changes

Nothing below is done until the ruling is recorded.

| Ruling | Changes |
|--------|---------|
| Q1 A or B | `CLAUDE.md` §5.3 step 4 text and `data-architecture-detail.md` §5.3; `NP-IRB-001` §5 and its Type B row; completed-decisions and revision-history entries; `ci` gate that refuses a token or serial in a research-store schema, once one exists. B adds the derivation to `common/npps-core` and a crate or API entry in `docs/np_soup_rust_001.md` if a crate is added. |
| Q1 C | A review of the `NP-IRB-001` exemption basis; §5.3 wording changes to say the link exists. |
| Q2 | A new open item for the extract transport address. |

## 6. What I need back

1. The option for Q1, and the ruling on Q2.
2. A short rationale for each (`CLAUDE.md` §18).
3. Any condition that would reopen it.
4. Your name, role and the date.
