# Questions for the cryptography reviewer and the principal: OI-PRIV-35, the UHDR key length, the Argon2id memory and the unlock limit

**Status:** DRAFT, not sent. Not a controlled document; see `docs/human-input/README.md`.
**GitHub issue:** [#963](https://github.com/stevehickman/NeuroPulse/issues/963)
**From:** NeurOne Firmware Engineering (`OI-PRIV-35` (NP-PRIV-ANALYSIS-004) in `docs/status/pending-decisions.md`; firmware privacy review `docs/np_priv_analysis_004.md`, Findings M9 and M10)
**To:** the cryptography reviewer named by `OI-EMMC2-02` (Q1 to Q4); the principal (Q2's "is this a weakening" part, and Q5)
**Response needed:** an option for each question; a short rationale; anything that would reopen it; name, role and date.

## 1. The questions

1. **Q1, the XTS key.** The UHDR partition key (the UKMD) is 32 bytes and goes straight to the XTS mount. XTS-AES-256 takes 64. Which construction do we use?
2. **Q2, the Argon2id parameters.** `m = 65536` KiB (64 MiB) cannot run on this device. What are `m`, `t` and `p`, and (principal) is replacing a figure that never ran a weakening under CLAUDE.md §5.1?
3. **Q3, the back-off schedule.** An unlock attempt limit is now built with provisional figures (5 free attempts, then 30 s doubling to 24 h). Confirm or replace them.
4. **Q4, the counter's own protection.** The failed-attempt count lives in the Config partition. Should it be anchored in something an eMMC rewrite cannot reset?
5. **Q5 (principal), wipe or permanent lock.** Should any number of failures ever erase the UHDR partition or lock it for good? Nothing does today.

## 2. What is known (checked against the code, 2026-11-21)

### 2.1 The key is 32 bytes and is used directly

- `firmware/uhdr_key/include/np_uhdr_key_config.h`: `NP_UHDR_UKMD_LEN` = 32. Until this change its comment called it "the AES-256-XTS key length".
- `np_uhdr_key_unlock()` passes the 32 bytes to `np_uhdr_hal_mount_uhdr(ukmd)`. `NP-FW-EMMC-002` §C.2 says the UKMD is "used directly as the AES-256-XTS key".
- XTS-AES-256 takes Key1 ‖ Key2, 64 bytes (IEEE Std 1619; NIST SP 800-38E). 32 bytes used directly is therefore XTS-AES-128, or a 256-bit key with Key1 = Key2, a construction FIPS 140 validation guidance rejects.
- **Nothing consumes the key on target yet.** `np_uhdr_hal_mount_uhdr` traps in `firmware/platform/src/np_platform_stub.c`, so no partition has ever been encrypted under either reading. The fix costs nothing in migration if it is made before the seam is implemented.

### 2.2 What each Q1 option touches

| | A. Widen the UKMD to 64 bytes | B. Derive Key1 and Key2 from the 32-byte UKMD | C. Keep 32 bytes, declare XTS-AES-128 |
|---|---|---|---|
| On-flash record (`np_ukmd_record_t`, 192 B) | Changes: `ukmd_ciphertext` grows from 32 to 64 bytes, taking 32 of the 84 reserved bytes. The layout question in `OI-UHDRK-08` (84 or 44 reserved) becomes binding: at 44, 12 bytes would remain | Unchanged | Unchanged |
| GCM wrap | Wraps 64 bytes (the host stub supports only 32 and would be extended) | Unchanged | Unchanged |
| New primitive | None | A KDF at mount: HKDF-SHA-256 or HMAC in the CAAM, or Monocypher's keyed BLAKE2b (`firmware/crypto/vendor/monocypher`), with distinct labels for Key1 and Key2 | None |
| `ci/test_shdr_schema.py` KEYMAT-01 | Matches column names, not lengths: unaffected | Unaffected | Unaffected |
| Strength | 512-bit key material | Bounded by the 256-bit UKMD, which is the AES-256 key strength anyway | 128-bit AES. CLAUDE.md §4.1 and §5.1 say AES-256, so this lowers a stated protection and is the principal's decision, not the reviewer's |
| Spec edits | §C.2, §C.3, §C.5 ("the 32-byte wrapper") | §C.2, plus a sentence at §C.4 step 5 | §C.2, and CLAUDE.md §5.1 |

**Recommendation: B**, with a check that Key1 ≠ Key2 before the mount. It changes no stored format, uses a primitive the firmware already vendors or the CAAM provides, and keeps AES-256. A is also sound and is the choice if the reviewer prefers no derivation step on the data path. C is not recommended.

### 2.3 The Argon2id memory is larger than all the device's RAM

- `NP_UHDR_ARGON2ID_M_COST` = 65536 KiB (64 MiB), `t` = 4, `p` = 1 (`NP-FW-EMMC-002` §C.3, §C.4; inherited from `NP-FW-EMMC-001` Rev 1 §6).
- The i.MX RT1062 has **1 MB of on-chip SRAM** (`docs/reference/hardware-detail.md` §4.1). The review inferred that 64 MiB "may not fit the 32 MB" of external SDRAM listed there. It is worse: **there is no external SDRAM**. `NP-SW-CI-001` §4.13 (closing `OI-SWCI-46`) found no SDRAM part, no BOM line, no SEMC configuration and no linker region, and the linker script refuses anything placed in `.lpsdr4` (`firmware/application/linker/app_imxrt1062.ld`). `OI-SWCI-47` asks whether the 32 MB line in CLAUDE.md §4.1 is real.
- So 64 MiB is 64 times everything the processor has. Even the 32 MB part, if it existed, would hold half of it.
- What on-chip memory is free is a measurement, not a figure from this brief: `NP-SW-CI-001` §4.13 recorded 312,716 B free in the staging region and about 439 KB of unallocated DTCM on 2026-09-08. The KDF runs only at unlock, so its buffer can be shared with memory that is idle then. That has to be shown on the image.
- **Hardware binding changes what the memory cost buys.** `WKMD = hw_bind(Argon2id(credential, salt), HUK)` (`OI-UHDRK-09`): without the device's fused key the Argon2id output is useless, so the offline attack on an eMMC image needs the device. Argon2id's memory hardness was chosen against offline GPU attack. With the binding, the KDF's cost mostly sets the *online* rate, which the new back-off (§3) now limits as well. The binding is the control that must hold; the memory figure is the second line.
- **Why this has to be settled before any device is provisioned:** `NP_UHDR_ARGON2ID_M_COST_MIN` is the floor `np_uhdr_key_unlock()` enforces on a stored record. Once a record exists with a value, lowering the constant refuses that record, and lowering the floor is a weakening of a protection that has run (CLAUDE.md §5.1). Today no record has been minted on a device: `np_uhdr_hal_argon2id` traps on target.

### 2.4 Q2 options

| Option | Memory | What it costs | §5.1 |
|---|---|---|---|
| A. Add an external SDRAM part and keep 64 MiB | 64 MiB needs at least a 64 MB part, not the 32 MB listed | A hardware decision: BOM, layout, EMF, power (§4.5). Every cost figure is a floor (CLAUDE.md §2.1) and none is quoted here | Keeps the written figure |
| B. Fit the KDF on-chip and raise `t` to hold the time per attempt | A measured share of on-chip SRAM, of the order of 256 KiB | Far weaker memory hardness on its own; acceptable only because of the hardware binding (§2.3). `t` chosen from a measured time per attempt on the target | Replaces a figure that never ran. The principal rules whether that is a weakening |
| C. Run the memory-hard stretch on the phone, then the HUK bind and a small KDF on the device | 64 MiB on the phone, small on the device | The device-side guess rate then depends on the device KDF and the back-off. Changes the §C.4 split of work, and the transport has to carry a key-equivalent value (CLAUDE.md §23 covers it) | Keeps 64 MiB, moved |

**Recommendation: B**, unless A is wanted for other reasons, with the principal recording explicitly that replacing a parameter no device has run is a correction and not a weakening, and that the floor binds from the first provisioned record. Whichever is chosen, the time per attempt is measured on the target and written beside the figures (CLAUDE.md §18).

## 3. The unlock limit that is now built (Q3 and Q4)

`NP-FW-EMMC-002` §C.7, `REQ-EMMC2-01`, `firmware/uhdr_key/src/np_uhdr_key.c`, six host tests:

- Every attempt that reaches the KDF is charged to a persisted count **before** the credential is tested; a charge that cannot be persisted refuses the attempt untested.
- A successful authentication clears the count.
- After `NP_UHDR_UNLOCK_FREE_ATTEMPTS` (5) failures, an attempt made before `min(30 s × 2^(n−5), 24 h)` has elapsed is refused with `NP_UHDR_ERR_LOCKOUT`, without running the KDF and without being charged.
- The wait is counted in uptime from the later of boot and the last charged attempt (there is no RTC backup, CLAUDE.md §4.5). A power cycle restarts the wait; it never ends it.
- A missing or unreadable count refuses the attempt.

**Q3.** The 5 / 30 s / 24 h figures are PROVISIONAL (register row UC-203). NIST SP 800-63B-3 §5.2.2 bounds consecutive failures at 100 and does not state these. At the cap, a 4-digit PIN takes on the order of 10⁴ days to exhaust, a 6-digit one about 10⁶. **Recommendation:** set them from the device credential's real entropy (no PIN length rule for the device credential was found; `docs/np_app_presence_001.md` covers the phone's own screen lock) and the time an attacker may be assumed to hold the device, and write that beside them.

**Q4.** An attacker who removes the eMMC and rewrites the Config partition can reset the count. The Config key is re-derivable by NeurOne (`docs/np_fw_nvram_001.md` §3.2) but not, as far as the documents say, by a third party, so the rewrite needs that key as well. A counter in the SNVS monotonic counter or in OTP would remove the question; OTP fuses are finite. **Recommendation:** the reviewer rules whether Config is enough, given hardware binding already stops offline guessing; if not, an SNVS-anchored counter, raised as its own item.

## 4. Q5 for the principal: should failures ever erase or lock for good?

Nothing in §C.7 erases or permanently locks. An erase after N failures would destroy the person's data on the guesses of someone else (a child, a thief, a clinic colleague), and CLAUDE.md §5.1 treats an erase as a separate act the person chooses. A permanent lock without a recovery path is the same loss. **Recommendation: neither**, and the back-off stands alone. If the answer is yes, it is an ISO 14971 question too, since losing a session log can matter to the person's care.

## 5. What happens with each answer

- Q1 → the mount seam's contract (`np_uhdr_hal_mount_uhdr`) and §C.2 are rewritten before the seam is implemented; for A, also the record and the host GCM stub.
- Q2 → `NP_UHDR_ARGON2ID_*` and the floors change in one commit, with the measured time per attempt; §C.3 and §C.4 are corrected.
- Q3 → UC-203 moves to "Justified or retired" with the reasoning, or the figures change.
- Q4, Q5 → new items if the answer adds work.

`OI-PRIV-35` (NP-PRIV-ANALYSIS-004) stays open until Q1 to Q3 are ruled.
