# Question for the principal: OI-PRIV-74, how a research extract shows it is genuine without naming the device

**Status:** ANSWERED 2026-11-23: Q1 yes; Q2 A, anonymous tokens (the principal). Originally DRAFT, not sent. Not a controlled document; see `docs/human-input/README.md`.
**GitHub issue:** #1025
**From:** NeurOne Software Engineering (`OI-PRIV-74` in `docs/status/pending-decisions.md`, filed in #1018; found while fixing `OI-PRIV-26` (NP-PRIV-ANALYSIS-004), #979)
**To:** the principal (the Privacy Lead and a cryptography reviewer may advise on Q2)
**Response needed:** a yes or no for Q1, an option for Q2 if Q1 is yes, a short rationale, anything that would reopen it, and your name, role and the date.

## 1. The question

`NP-FW-ANON-001` §7.2 wraps each research extract in an envelope that carries `device_warranty_token` and a
`device_signature` made with a per-device Ed25519 key. Your ruling on `OI-PRIV-67` (2026-11-20) says an extract is
stored by **study ID only** and that **no device identifier, token or serial travels with it**
(`data-architecture-detail.md` §5.3 step 4; `CLAUDE.md` §5.3). The token goes: that ruling already decided it, and
it needs nothing from you.

The signature stands for a real need the ruling did not address: **without one, the research store cannot tell an
extract a genuine NeurOne device made from one anybody made.** A per-device key meets that need but names the device.

Plain form: **must the research store be able to refuse a forged extract, and if so, by what means that does not
identify the device?**

## 2. What is known

- **Nothing is built.** No code builds the envelope: `firmware/anon/` has no envelope writer, and no extractor
  exists in the apps (`OI-CONSENT-07`). The question can be settled before anything is built.
- **The study's encryption key is effectively public.** Each extract's records are encrypted to the
  `study_public_key` in the signed descriptor (`NP-FW-ANON-001` §3.1, §7.2). Every research request is published to
  every subscribed device (`OI-PRIV-68`, `CLAUDE.md` §6.3), so anyone with the app or a copy of a request has that
  key. **Encryption keeps outsiders from reading an extract. It does not stop them writing one.**
- **The transport hides the sender.** Every extract goes through the Oblivious HTTP relay (`OI-PRIV-69`), so
  NeurOne never sees the sender's network address. The relay does not authenticate anything, and it should not.
- **What already holds without a device key:**
  - A person's extracts come only from the device that is current for them (`OI-PRIV-68`).
  - A study without tracking gets one extract per person, and the device keeps the record that stops a second
    (`OI-PRIV-70`).
  - The privacy budget is enforced on the device (`NP-FW-ANON-001` §8).
  - None of these needs the server to know the device. **All of them protect against an honest device repeating
    itself, and none protects against a forger.**
- **What a forger could do.** Upload any number of extracts that decrypt cleanly and look well formed. The harm falls
  on research quality, not on anyone's privacy: a study could be skewed, flooded or made to look as if it has more
  participants than it does. k-anonymity and differential privacy protect participants; they do nothing against
  poisoned input.
- **What a per-device key costs.** One key per device, the same in every study, lets NeurOne join every extract that
  device ever sent. If the key or its certificate is tied to the warranty token or serial, it also joins them to SHDR
  and, through the warranty database, to a registrant. You ruled that out in `OI-PRIV-67` Q1 (option A, over a
  per-study pseudonym), accepting the loss of a server-side duplicate check for stronger privacy.
- **Known inconsistency, not resolved here:** `NP-FW-ANON-001` §2 places the anonymisation engine in the hub
  firmware, while `CLAUDE.md` §5.3 says anonymisation happens in the app. Which one makes the extract decides which
  component holds whatever key Q2 chooses. The options below work for either.

## 3. Questions

**Q1. Must the research store be able to refuse an extract that did not come from a genuine NeurOne device?**

- **No.** The envelope carries no proof of origin. Researchers are told that the store accepts any well-formed
  extract, and their analysis has to tolerate fabricated or repeated input (outlier checks, plausibility bounds).
  *Cost:* forged or flooded studies are possible. Detecting them falls on each research team, and a skewed study
  could be published. *Gain:* nothing to build, and nothing about a device ever reaches NeurOne.
- **Yes.** Then pick a mechanism in Q2.

**Q2 (only if Q1 is yes). How is a genuine extract recognised?**

- **A. Anonymous tokens (Privacy Pass).** A genuine device proves itself to NeurOne's token issuer and receives a
  batch of tokens blind-signed by the issuer (IETF RFC 9474 blind RSA signatures; Privacy Pass, RFC 9576–9578). Each
  extract spends one token. The store checks that the issuer signed the token and that it has not been spent, and
  learns nothing else: blinding means even NeurOne, as both issuer and store, cannot match a spent token to the
  device it was issued to.
  - The device proves it is genuine only at issuance, never with an extract. That proof can be the hub's own key or
    the phone platform's app attestation, whichever the extract-maker (section 2, last point) can hold.
  - **Issuance is on a fixed schedule and in a fixed batch size for every subscribed device, whether it takes part
    in any study or not.** Otherwise the issuer would learn who is active in research, or which study they joined.
  - *Cost:* a token-issuer service with key rotation; a spent-token list in the store; blind-signature code written
    once in `common/npps-core` (`CLAUDE.md` §22), with any crate checked for its licence and added to
    `NP-SOUP-RUST-001`; a ceiling on extracts per device per period, set by the batch size; and token timing that
    must not give away which device spent which token (batches, not one token per extract).
  - *Gain:* a forger without a genuine device gets no tokens, and a flood is capped by the per-period batch.
- **B. Group signatures (anonymous attestation).** Each genuine device holds a member key of one group. A signature
  shows "some genuine device signed this" and not which one (direct anonymous attestation, as in TPM 2.0 and
  ISO/IEC 20008-2).
  - *Cost:* heavier cryptography with fewer audited implementations, written once in Rust and reached from every
    platform (`CLAUDE.md` §22). Revoking one compromised device is hard without losing anonymity. With no tokens
    there is no per-period cap, unless a linkable-per-study variant is used, which reintroduces the within-study link
    a study without tracking must not have.
  - *Gain:* no issuer service and no token traffic.
- **C. Keep a per-device signature, as §7.2 has it.** *Cost:* it contradicts your `OI-PRIV-67` ruling, so choosing it
  reverses that ruling. `CLAUDE.md` §5.3 ("no device identifier reaches the research store") and the `NP-IRB-001`
  exemption argument would both change. Listed for completeness only.

A per-study signing key derived on the device, like the `OI-PRIV-70` tracking identifier, is not offered for
studies without tracking: it links one person's releases within a study, which those studies must not allow. A study
that declares tracking already sends a per-study identifier, so for those studies the question is only
authenticity, and A or B answers it the same way.

## 4. Recommendation

**Q1: yes. Q2: A, anonymous tokens.**

- Because the study key reaches every subscribed device, forging an extract is easy, and research NeurOne hosts
  should not rest on each team spotting it.
- Tokens meet the need with standard, published protocols. They keep your `OI-PRIV-67` position (nothing in the
  store names a device), and they give a flood cap that group signatures do not.
- The price is an issuer service and a fixed issuance schedule. Both run before any study, so neither touches a
  participant's data.
- If the cost of building A is too high for the first studies, **Q1 "no" is a sound interim answer** while studies
  are few and researchers are told. It closes nothing permanently, and A can be added later without changing what
  an extract carries.

## 5. What each ruling changes

Nothing below is done until the ruling is recorded. Whatever the ruling, `device_warranty_token` is removed from
`NP-FW-ANON-001` §7.2 under `OI-PRIV-67`.

| Ruling | Changes |
|--------|---------|
| Q1 no | `NP-FW-ANON-001` §7.2 drops `device_signature`, and the envelope says what it does not prove. The researcher terms (`NP-IRB-001` or the study agreement) say extracts are unauthenticated. |
| Q2 A | `NP-FW-ANON-001` §7.2 carries a spent token in place of the signature; a new section for issuance (schedule, batch size, key rotation, what the issuer learns); the issuer and spent-list service in `NP-INFRA-001`; blind-signature code in `common/npps-core` and `NP-SOUP-RUST-001`; requirement rows with tests. |
| Q2 B | `NP-FW-ANON-001` §7.2 carries a group signature; member-key provisioning at manufacture or at app setup; a revocation plan; a cryptography review before choosing a scheme; code in `common/npps-core`. |
| Q2 C | Reverses `OI-PRIV-67`: `CLAUDE.md` §5.3, `data-architecture-detail.md` §5.3 step 4 and `NP-IRB-001` §5 change, with a decisions-log entry. |

## 6. What I need back

1. Q1 yes or no, and the option for Q2 if yes.
2. A short rationale (`CLAUDE.md` §18).
3. Any condition that would reopen it, for example the number of studies or a forgery seen.
4. Your name, role and the date.

## 7. Answer

**ANSWERED 2026-11-23 by the principal: Q1 yes; Q2 A, anonymous tokens.** No separate rationale or reopening condition was given; the answer takes the recommendation of section 4 as written. Recorded in `NP-FW-ANON-001` §7.2 and §7.3, `docs/reference/data-architecture-detail.md` §5.3 step 3, the `OI-PRIV-74` row and the decisions log.
