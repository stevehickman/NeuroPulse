# Decision brief: OI-PRIV-32 (NP-PRIV-ANALYSIS-004): where the firmware rollback floor lives, and whether USB DFU is encrypted

**Status:** DRAFT, not sent. Not a controlled document; see `docs/human-input/README.md`.
**GitHub issue:** [#960](https://github.com/stevehickman/NeuroPulse/issues/960)
**From:** NeurOne Firmware Engineering (`docs/status/pending-decisions.md` `OI-PRIV-32` (NP-PRIV-ANALYSIS-004); `docs/np_priv_analysis_004.md` Findings M5 and M6)
**To:** the principal (Q3 is an exception to `CLAUDE.md` §23, which only the principal can make). Q1 and Q2 also need Firmware and the electrical owner of the i.MX RT1062 fuse map. No names are assigned.
**Response needed:** an option for each of Q1 to Q3, a short reason (`CLAUDE.md` §18), what would reopen it, and a name, role and date.

## 1. The question

1. **Q1.** Where is the monotonic minimum firmware version (the *rollback floor*) stored?
2. **Q2.** Is the floor compared with every release's version, or with a separate rollback index that only security fixes raise? The answer depends on Q1.
3. **Q3.** USB DFU recovery moves a firmware image over USB-C in plaintext. Should its payload be encrypted, or does it get a recorded exception to §23?

## 2. What the review found, re-checked against the code on 2026-11-21

Both findings hold as written.

- **M5, no anti-rollback.** `np_ota.c` built the Ed25519 message with `hdr.version` in it (lines 411–414 on `main` at `49d6c51`), and `np_signature.c` did the same. Nothing compared that version with the installed version or with any minimum. So any older image signed with the production key verified and installed. That covers both USB DFU (`np_dfu.c`, the `DFU_STATE_MANIFEST_SYNC` branch, `np_ota_verify_scratch()` then `np_ota_write_bank()`) and the OTA staging path.
- **M6, DFU is a built transfer path, unencrypted and unlisted.**
  - `np_dfu.c` writes each `DFU_DNLOAD` block to Scratch as it arrives (`np_emmc_write`, line 288). The whole image is verified only at the manifest step.
  - Nothing is encrypted, at the link or in the payload.
  - `NP-FW-HUB-001` §2.5, the §23 transfer table, did not list DFU.
  - DFU is **download only**. `DFU_REQUEST_UPLOAD` is defined, but no handler serves it (the request stalls), so no data leaves the device by this path.

## 3. What this change has already done (no decision was needed)

- **The rule is built** (`REQ-SW02-15`, `NP-SW-001` §5.2.2). The code is `firmware/bootloader/src/np_rollback.c`, called from both install verify paths in `np_ota.c`:
  - An image is admitted only once its signature verifies, and only if its signed version is at or above the floor. Equal is admitted, so the same build can be reinstalled.
  - A floor that cannot be read refuses every image.
  - The floor rises only when an image is confirmed healthy (`np_ota_confirm_boot`), never at staging. A candidate that fails its boot trial can therefore still roll back to the bank it replaced. The floor is never lowered.
  - `np_bootloader_rollback_tests` runs the real `np_ota.c` and `np_signature.c` against a fake eMMC and a fake floor store. It fails six ways when the check is removed.
- **The store sits behind a seam** (`np_rollback_store.h`).
  - **The device build links `np_rollback_store_unbuilt.c`. Its read fails, so the device installs no OTA update and accepts no DFU image until Q1 is answered.** That is the fail-closed reading, the same pattern as the production power governor that refuses every load (`OI-FWHUB-09`).
  - A store that always reports floor 0 would admit every older signed image while looking like enforcement. It was deliberately not written.
  - No unit exists yet, so this blocks nothing in the field. It does block the first bench OTA and DFU on real hardware.
- **DFU now has a row in §2.5** that says it is built, unencrypted, and not covered.

**What the rule does not do:**
- The normal boot of an already-confirmed bank (`np_main.c` `verify_bank_header`) does not check the floor. That bank passed the check when it was installed.
- Any store chosen under Q1 that sits in the eMMC user area is erased by a factory reset (`np_main.c` erases all of Config). That is one reason Q1's options are fuses or RPMB.

## 4. Q1: where the floor lives

| Option | What it is | Costs and limits |
|---|---|---|
| **A. On-chip OTP fuses (OCOTP)** | A fuse field on the i.MX RT1062, blown one bit per raise (thermometer code), read by the bootloader | Truly monotonic: no software can lower it, and it survives every erase and factory reset. **Finite:** a field of N bits allows N raises in the life of the unit, so it can only carry a rollback index (Q2 option b), not every release. Blowing fuses in the field needs a programming-voltage and power-integrity argument. **Whether the RT1062 has a free general-purpose fuse field, and how wide it is, was not checked here.** The electrical owner must confirm it against the fuse map |
| **B. eMMC RPMB** | An authenticated block in the eMMC's Replay Protected Memory Block, holding the floor and protected by the RPMB key and write counter | Unlimited raises, so it can carry the full version (Q2 option a). It lies outside the user area, so no factory reset or partition erase touches it. **Needs an RPMB authentication key** that is unique to the device, programmed once, and never readable by the application. That key is itself fuse-backed or derived from a device-unique secret, so it shares part of A's provisioning step. The RPMB driver is not written (`np_emmc.c` has no RPMB support) |
| **C. A raw sector in Config (interim only)** | The floor in an eMMC user-area sector next to the OTA state record | Cheap and buildable now. **Not monotonic** against anyone who runs code on the main processor, and **erased by every factory reset**, after which DFU accepts any older signed image again. It stops only an attacker who has nothing but DFU and has not reset the device. Not recommended, even as an interim, because it looks like a protection it is not |

**Recommendation: B, with its RPMB key provisioned in the same factory step that writes the tier identity record (`NP-REG-UPG-001` §7.9). If RPMB is unavailable on the chosen eMMC part, choose A with Q2 (b).** Until one is chosen, keep the fail-closed stub. Do not choose C.

## 5. Q2: what is compared

| Option | What it means | Fits |
|---|---|---|
| **(a) The full signed version (as built)** | Every confirmed release raises the floor | B. Unlimited raises |
| **(b) A rollback index** | A separate, signed field that release engineering raises only for a security fix. A non-security downgrade stays possible | A. Finite raises. **Changes the image header:** the index must be covered by the signature, and today the signature covers only `sha256 ‖ version ‖ size`. Either the message gains the index, or the index is carved out of `version` by convention (for example its top byte). A header change is a bootloader and signing-tool change |

**Recommendation: (a) with B. (b) only if Q1 lands on A.**

## 6. Q3: DFU and §23

§23: *"Every upload to, and every download from, the headset or the control hub is encrypted … no unencrypted path exists for … firmware."* Its two reasons are data privacy and hardening against malware injection. DFU today:

- **Privacy.** DFU carries a firmware image from a host to the device, and nothing back (§2). It carries no UHDR and no SHDR.
- **Injection.** The image is Ed25519-verified before any bank write, and is now also refused below the floor. Encryption would add secrecy of the image, not authenticity.

| Option | What it is | Costs |
|---|---|---|
| **A. Encrypt the DFU payload** | The image is encrypted, in transit and in Scratch, to a key the device holds and decrypted at the manifest step before verification | Meets §23 as written. **A key every unit holds (a fleet key) protects little:** extracted from one unit, it decrypts every image. A per-device key needs a host tool that wraps the image for each unit, plus a provisioning step and a key-management design. That overlaps the secure-channel design in `oi-simfw-71-secure-channel-design.md`, which DFU, running in the bootloader before any application or pairing exists, cannot use as is. Decryption code goes in the bootloader, under §22 (Rust in `common/npps-core`, or a recorded exception for the bootloader's C) |
| **B. A recorded exception for recovery DFU** | §23 records DFU as a known exception: firmware images only, download only, signed and floor-checked, no user data | No new crypto in the bootloader, and recovery stays simple. **Lowers nothing that exists today** (DFU has never been encrypted). But it is an exception to a locked rule, which only the principal grants, and it should carry conditions (below) |
| **C. Withdraw DFU from production units** | Fuse-disable or compile out DFU after manufacture. Recovery becomes a service-centre operation | Removes the path. **Costs field recovery:** a unit with both banks bad halts (`np_dfu.h` case (c)) and goes back to service |

**Recommendation: B, with these conditions written into the exception.**
1. DFU stays download only: no `DFU_UPLOAD` handler is ever added.
2. Every DFU image passes the Ed25519 check and the rollback floor before any bank write. Both are built.
3. DFU never moves anything but a signed firmware image. No configuration, calibration or user data ever goes over it.
4. The exception is revisited when the `OI-SIMFW-71` secure channel exists, or if image secrecy becomes a requirement (IP protection, or a regulator's view).

If the principal rejects B, choose A with per-device wrapping over C, because C removes field recovery.

## 7. What to send back

- Q1: A, B or C (or another store), and the electrical owner's confirmation of the fuse field or the RPMB part.
- Q2: (a) or (b).
- Q3: A, B (with or without the conditions above) or C.
- For each: a reason, what would reopen it, and the reviewer's name, role and date.

Once an answer is given, it is recorded in `OI-PRIV-32` (NP-PRIV-ANALYSIS-004), in `NP-FW-HUB-001` §2.5, and in `docs/status/completed-decisions.md`. Nothing in the repository acts on it before then.
