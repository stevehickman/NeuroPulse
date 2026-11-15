# Principal decisions needed before the factory step can be built: OI-UPG-08 (the tier-identity record) and OI-UPG-09 (the hardware version in it)

**Status:** DRAFT, ready to send; not yet sent (no addressee named). Not a controlled document; see `docs/human-input/README.md`.
**GitHub issue:** [#569](https://github.com/stevehickman/NeuroPulse/issues/569) (package 8)
**From:** NeurOne Systems Engineering (`REQ-UPG-04`, `NP-REG-UPG-001` §7.9; `OI-UPG-08`, `OI-UPG-09`, `OI-UPG-01`)
**To:** The principal, with Manufacturing, Security, Safety firmware and Quality (owners of `OI-UPG-08`). Names to be assigned.
**Response needed:** a decision on each question in section 4, a short rationale, conditions that would reopen it, and the decider's name, role and date.
**Why now:** issue #569 package 8 says the factory step needs a manufacturer, a programming fixture and a signing service, and none exists. This note separates what could be built without them (and now is) from what must wait, and lists what only the principal can decide. **Nothing here chooses a manufacturer, a key-custody scheme or a signing service, and nothing in the repo may act as if one has been chosen.**

## 1. The question

`REQ-UPG-04` says no unit leaves manufacture without a tier-identity record that the unit itself has verified. `NP-REG-UPG-001` §7.9 designs the eight-step procedure. Until it exists **every unit is T1, including every T2 unit** (the image carries an all-zero placeholder authority key, `OI-UPG-08`). Which of the decisions below do you take now, and which stay open until a manufacturer exists?

## 2. What is known

- **Firmware half (built):** SW01-M10 (`firmware/safety_mcu/src/np_tier_identity.c`) verifies a 72-byte record at OTP offset `0x40`: `NPTI`, record version `0x02`, byte 5 (hardware version in the upper nibble, tier in the lower), two reserved zero bytes, then an Ed25519 signature over `"NeurOne.TierId.2"` ‖ body ‖ the part's 96-bit UID. It reports tier, reason and hardware version in a 7-byte report. It contains no OTP write path.
- **Design (§7.9):** eight steps (read UID, check blank, check build record, compose and sign, program, verify on the unit, lock, record), in that order, each failure ending the unit's run.
- **Not decided (§7.9 "Not decided"):** manufacturer and fixture; where the signing service runs and who holds the key; the list of released hardware versions and who keeps it; how the board's revision is read; the exact read-out-protection setting; whether every unit is verified (the writer's reading: yes).

## 3. Scope: what can be built, what must wait

### (a) Built on a host, now, with no manufacturer

`common/npps-core/src/tier_record.rs` (a module of the existing crate, so no new crate; §22 puts cryptographic code in `common/npps-core`). Pure functions over bytes, no secret, **no signer** (signing exists only in the tests, with the test authority's seed):

| Step of §7.9 | What the module does | Inputs it takes rather than chooses |
|---|---|---|
| 2 | `check_blank`: the 72-byte window must be all `0xFF` | the bytes the fixture read |
| 3 | `check_build_record`: tier valid, version 1 to 15, on the released list, equal to the board's version | the released list; the board-revision map's result |
| 4 | `record_body`, `signing_message`, `assemble`: the exact bytes the authority signs, and the 72 bytes to program from the 64-byte signature it returns | the signature |
| 5 | `double_words`, `check_read_back`: nine 64-bit words, read back equal | the read-back bytes |
| 6 | `verify_record` (the firmware's verdict, in the firmware's order of reasons), `parse_report`, `accept`: the unit's own report must read `OK`, the build record's tier and its version | the authority public key; the report bytes |

Tests are pinned to signatures produced by the firmware's own Monocypher signer for the same seed, UID and bodies (byte for byte), to every single-bit flip of a record, to another unit's UID, another key, a missing domain string, and to the whole sequence on a host double of the unit. They are tagged `REQ-UPG-04`.

What this does **not** show: that the firmware, on silicon, reads OTP as the module assumes; that a fixture can program and read back; that read-out protection locks the window. Those need hardware.

Not built, though host-possible, because it would be design: a production-record writer (step 8: fields are listed in §7.9, but the store, its retention and who reads it are Quality's), and a command-line tool (it would need a signer and a key source, which are the decisions below).

### (b) Must wait for a manufacturer, a fixture or a signing service

1. **The signing service** (step 4): it has to exist, hold the authority key, and refuse a UID it did not read in this run (§7.9 step 1). The module composes the message and verifies the result; it cannot be the service.
2. **The key ceremony** (`OI-UPG-08` (i)): generating the tier-authority key pair, and putting the public key in the image in place of the all-zero placeholder (`NP_TIER_AUTHORITY_PUBKEY_INIT`). Until then the verdict is always T1, reason `NoAuthority`.
3. **The programming fixture** (steps 1, 2, 5, 7): reading the UID from the part, reading OTP, programming nine double words, resetting the unit, reading its report, setting read-out protection. This is the driver the module's inputs come from.
4. **Read-out protection** (step 7, `OI-UPG-08` (iv)): the exact setting for the STM32G071 and its effect on the OTP root-key read the safety image already needs. Also the `REQ-UPG-02` rewrite test (`NP-REG-UPG-001` §7.7), which needs silicon in the locked state.
5. **The board-revision map** (step 3): how the factory reads the board's revision from the unit or its marking, and the hardware design's map from revision to version.
6. **The first-article and process records** (`NP-FAI-001` is not the home, §7.9): the manufacturing procedure, the operator record and the sampling decision.

### (c) Decisions needed from the principal

See section 4.

## 4. Questions

1. **Key custody and the signing service.** Who holds the tier-authority private key, where does the signing service run, and who may ask it to sign? Options the record has not weighed: an offline ceremony with a per-batch signing run; an HSM-backed service at the manufacturer; an HSM-backed service operated by NeurOne. Cost of each: trust placed in the manufacturer's site, or an online NeurOne service on the production line. **Nothing is recommended here.**
2. **One public key or a short list in the image** (`OI-UPG-08` (i)). A list allows rotation, and a safety-image update is never automated OTA, so a lone key cannot be rotated for units already built. Cost of a list: more Class C surface (`NP-FMEA-001` §3.10) and a rule for which key signs which unit.
3. **Who maintains the list of released hardware versions**, and what releases a version (§7.8 says one is incremented with each significant change; at most 16, `UC-080`). The module takes the list as an argument; it needs a named owner and a record.
4. **How the board's revision reaches the step**, and who owns the revision-to-version map (§7.9 step 3, §7.8 open). Until it is decided the step has nothing to compare a build record's version against.
5. **Is every unit verified at step 6, or a sample?** The writer's reading is every unit, because the record cannot be corrected (§7.9). Confirm, or state the sampling plan.
6. **Disposition of a unit written with a wrong version** (§7.9): scrap, or rebuild into the version its record names if its board is compatible. Quality decides; the principal may want to rule that scrap is the only option.
7. **May the placeholder key be replaced by a production key before a silicon run of the lock?** Replacing it is what puts units into force as T2 or T1-signed, while the lock (step 7) is the only thing that stops a field write of the window. Confirm whether the key may go into an image before the lock has been shown to work on silicon.

## 5. What to send back

For each question: the decision, a short rationale (`CLAUDE.md` §18), conditions that would reopen it, and the decider's name, role and date. Record an answer in `docs/np_reg_upg_001.md` §7.9 and §9 (`OI-UPG-08`, `OI-UPG-09`) and in `docs/status/completed-decisions.md`, then set this file's status to `ANSWERED`.

## 6. What changes nothing

No unit is T2, and no T1 behaviour changes, until `OI-UPG-08` closes (`CLAUDE.md` §1). `OI-UPG-01`'s app half (software features unlocking only against the signed identity, F4 reaching every platform) is separate and unaffected.
