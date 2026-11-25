# OI-PRIV-62 (5): keeping a change made while the presence gate is locked across the end of the process

**Status:** DRAFT, not sent.
**GitHub issue:** none yet. Rule 7 of `docs/human-input/README.md` asks for one; the session that wrote this did not open an issue unasked. The finding is M11 of `NP-PRIV-ANALYSIS-007` (pull request [stevehickman/NeuroPulse#1006](https://github.com/stevehickman/NeuroPulse/pull/1006)).
**Sender:** the session that worked on `OI-PRIV-62` (a Claude agent), drafted 2026-11-22.
**Addressee:** the Principal, with Software Engineering and the Privacy Lead.
**Owning open item:** `OI-PRIV-62` part (5) (`docs/status/pending-decisions.md`). It overlaps `OI-PRIV-19` (how a held change is applied at the unlock).
**Not a controlled document and not a design input** (`docs/human-input/README.md`): no serial, not in `NP-DHF-001`, and it writes no requirement (`CLAUDE.md` §18). **Nothing here is built.**

**How to read it.** **[E]** is established, with the file that shows it. **[P]** is proposed here. **[choice]** is a figure with no derivation; if adopted it gets a row in `docs/status/unjustified-choices.md` (`CLAUDE.md` §19).

---

## 1. What I need from you

| # | Question | Recommendation |
|---|----------|----------------|
| Q1 | Should a change made while the gate is locked survive the end of the process (a crash, the system killing the app, the person closing it)? A: keep it on disk, sealed so that nobody can read it until the person is present; B: keep it in memory as today, and tell the person, when they make the change, that it takes effect only once they unlock; C: refuse changes while locked, except a removal and the analytics teardown that already act at once. | **A for a consent withdrawal and the other changes `REQ-PRES-01` already holds; B as the stop-gap until A is built.** |
| Q2 | If A: may the held changes be sealed to a **public key whose private half is sealed under the gated key** (the core already has HPKE for the analytics relay, `ohttp.rs`)? The app can then write while locked without being able to read what it wrote. | **Yes** |
| Q3 | If A: what does a held change say on disk while the person is away? It is ciphertext, but its existence, its size and its key name show that the person changed something while locked (for example, that a consent record changed). Is that acceptable, or must the file be padded to a fixed size and kept present whether or not anything is held? | **Fixed size, always present** (`CLAUDE.md` §5.1 rule 2: a pattern that shows a sensitive predicate leaks it) |

What to send back: the option for each, a short reason (`CLAUDE.md` §18), anything that would reopen it, and your name, role and date.

## 2. The problem

- **[E]** While the gate is locked, a write to a UHDR key is held in a `LinkedHashMap` in `TieredKeyValueStore` and applied at the next unlock (`common/core/src/commonMain/kotlin/life/neurone/core/common/TieredKeyValueStore.kt`). Nothing is written to disk, by design: the gated key cannot be read, so the value cannot be sealed, and writing it plain would put UHDR in the clear.
- **[E]** `REQ-PRES-01` (b) says the held writes are applied "at the next unlock in the same run, so none is lost (a withdrawal of consent made while locked must survive, `CLAUDE.md` §6.0)". The same file's header says a write is never silently dropped. **If the process ends before the unlock, every held write is lost, silently.** On a phone the system ends a backgrounded app routinely.
- **[E]** The principal ruled on 2026-11-21 that a change made while locked is an operation replayed on the sealed value at the unlock, never a whole value (`REQ-PRES-01`), and that changes which would widen what is shared are refused while locked. So what is held is already narrow: withdrawals, removals of a grant, and the other operations `OI-PRIV-19` lists.
- **[E]** A removal, and the analytics teardown a withdrawal brings, act on disk at once and are not affected.

## 3. The options

**A. Sealed to a gated public key.** [P] At the first unlock the app makes an HPKE key pair (`ohttp.rs` already does the KEM and AEAD in the core, `CLAUDE.md` §22), keeps the public key in the device tier and seals the private key under the gated key. A held operation is sealed to the public key and appended to a host file; at the next unlock the private key opens the file, the operations are replayed in order (the `OI-PRIV-19` merge), and the file is cleared. *Costs:* a new persistent format and its migration; a second sealed record per install; the file's presence and size leak that something was changed (Q3); replay must be idempotent, because a crash between replay and clear replays twice. *Gives:* a withdrawal made locked survives anything short of an erase.

**B. Memory only, and the person is told.** [P] The screen that takes a held change says, as it is made, that it takes effect when the person next unlocks and is lost if the app is closed first. *Costs:* a withdrawal can still be lost; the person must act on the notice. *Gives:* no new format; small. It is the "or the person is told they did not" half of `OI-PRIV-62`'s closing condition.

**C. Refuse while locked.** [P] Only removals and the teardown act while locked; everything else waits for an unlock. *Costs:* `REQ-PRES-01` (b) and the principal's 2026-11-21 ruling are reversed for withdrawals; a person who wants to withdraw must first unlock, which is a barrier to saying stop (`CLAUDE.md` §6.0, §6.2). Not recommended.

## 4. What is assumed

- That the HPKE in `ohttp.rs` can be reused for a local seal (it is built for one-shot requests to the relay; a local use needs its own `info` string and key lifetime). Not checked.
- That a fixed-size file (Q3) can hold every operation `OI-PRIV-19` allows while locked. The size would be a **[choice]**.

## 5. Until it is answered

`OI-PRIV-62` part (5) stays open, `NP-APP-PRESENCE-001` §3 says held writes are memory-only, and no host offers the gate (`OFFER_PRESENCE_GATE` stays false), as `OI-PRIV-62` and `OI-PRIV-19` already require.
