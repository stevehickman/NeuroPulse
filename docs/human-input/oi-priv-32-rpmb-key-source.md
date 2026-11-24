# Decision brief: OI-PRIV-32 (NP-PRIV-ANALYSIS-004): where each helmet's eMMC RPMB key comes from, who may program a blank RPMB, and whether the key code is C or Rust

**Status:** DRAFT, not sent. Not a controlled document; see `docs/human-input/README.md`.
**GitHub issue:** [#960](https://github.com/stevehickman/NeuroPulse/issues/960)
**From:** NeurOne Firmware Engineering (`docs/status/pending-decisions.md` `OI-PRIV-32` (NP-PRIV-ANALYSIS-004); `NP-REG-UPG-001` §7.10)
**To:** the principal (Q1 and Q2 are security-architecture choices; Q3 is a `CLAUDE.md` §22 question). Firmware, Manufacturing, Security and Service also need to see it. No names are assigned.
**Response needed:** an option for each of Q1 to Q3, a short reason (`CLAUDE.md` §18), what would reopen it, and a name, role and date.
**Follows:** `oi-priv-32-firmware-rollback-floor-and-dfu.md` §8, where the principal put the rollback floor in the eMMC's RPMB and left open *"how the RPMB key is provisioned and protected"*.

## 1. The questions

1. **Q1.** Where does each helmet's 32-byte RPMB key come from, and where is it kept so that the bootloader gets the same key at every boot?
2. **Q2.** Who may program the key into a blank RPMB after the unit has left the factory (a replaced eMMC, a repaired hub)?
3. **Q3.** Under Q1 A the bootloader derives the key. Is that derivation written in C in the bootloader, or in `no_std` Rust from `common/npps-core` (`CLAUDE.md` §22)? The same question for the RPMB MAC (HMAC-SHA256) is already `OI-SW02-01`, so Q3 asks only that the derivation follow whatever `OI-SW02-01` decides.

## 2. What is known

### 2.1 What RPMB needs

- Every RPMB write, and every read the reader wants to trust, carries an HMAC-SHA256 under a 32-byte key. The key is programmed into the eMMC once, by an *authentication key programming* request, and cannot be read back or changed (JESD84, as stated in the task; **JESD84 itself was not read**, because JEDEC requires an account).
- **The key crosses the eMMC bus in the clear** when it is programmed: it is a field of the request frame. Checked against OP-TEE's implementation (`optee_os` `core/tee/tee_rpmb_fs.c`, `struct rpmb_data_frame`, read 2026-11-23): 196 stuff bytes, then a 32-byte key/MAC field, then data, nonce, write counter, address, block count, result and type.
- **Anyone who holds the key can write any floor, including a lower one.** RPMB's write counter stops a replayed old write, not a new write signed with the key (first brief §4.1).
- **A blank part says so without authentication.** A write-counter read on a part with no key returns result `0x07`, *key not yet programmed* (OP-TEE `RPMB_RESULT_AUTH_KEY_NOT_PROGRAMMED`).

### 2.2 What the i.MX RT1062 offers, as far as could be checked

**Source used:** NXP's MCUXpresso SDK register header for this part, `MIMXRT1062.h`, and its DCP driver `fsl_dcp.h` / `fsl_dcp.c` (`github.com/nxp-mcuxpresso/mcux-sdk`, `main`, read 2026-11-23), and the SDK's RT1060 DCP example (`mcux-sdk-examples`, `evkmimxrt1060/driver_examples/dcp/dcp.c`). **The reference manual (IMXRT1060RM) could not be read:** `nxp.com` was refused by this environment's network proxy. **The security reference manual (IMXRT1060SRM) is distributed by NXP under NDA and was not read.** Register names and bit positions below are from the header; what a lock does beyond its one-line description there is unchecked.

| Claim | Where | Checked? |
|---|---|---|
| The **SW_GP2** fuse field is four 32-bit words (`SW_GP20`–`SW_GP23`, OTP Bank 5 words 1–4), so **128 bits** | `MIMXRT1062.h`, `OCOTP_Type`, offsets `0x690`–`0x6C0` | Yes (header) |
| SW_GP2 has a **write lock** (`OCOTP_LOCK` bit 21, `SW_GP2_LOCK`) and a **read lock** (bit 23, `SW_GP2_RLOCK`: *"the reading of this region's shadow register and OTP fuse word are blocked"*) | `MIMXRT1062.h`, `OCOTP_LOCK_*` | Yes (header) |
| The **DCP** (the on-chip AES engine) can use a key it is given in hardware, never through a register software reads: key select `0xFF` (*OTP key*) or `0xFE` (*unique key*) | `fsl_dcp.h` `kDCP_OtpKey`, `kDCP_OtpUniqueKey`; `fsl_dcp.c` packet `control1` | Yes (driver) |
| Which secret the DCP's *OTP key* is, is a mux: `IOMUXC_GPR_GPR10` bit 4 `DCPKEY_OCOTP_OR_KEYMUX`, **0 = the SNVS master key, 1 = SW_GP2**; `IOMUXC_GPR_GPR3` bit 4 `DCP_KEY_SEL` picks the low or high 128 bits of the SNVS master key | `MIMXRT1062.h` | Yes (header); the example's `DCP_OTPKeySelect()` sets exactly these bits |
| The mux has a **lock**: `GPR10` bit 20 `LOCK_DCPKEY_OCOTP_OR_KEYMUX`, *"Field is locked (read access only)"* | `MIMXRT1062.h` | Bit and description yes. **Whether the lock holds until reset, so that software cannot clear it, is unchecked** (reference manual) |
| The SNVS master key is the fused OTPMK, a zeroizable key or a combination (`SNVS_LPMKCR` `MASTER_KEY_SEL`, `SNVS_HPCOMR` `MKS_EN`); `SNVS_HPSR` has an `OTPMK_ZERO` status bit | `MIMXRT1062.h` | Yes (header) |
| The SDK's OCOTP register map has shadow registers for the SRK hash and SW_GP2 but **none for the OTPMK** | `MIMXRT1062.h`, `OCOTP_Type` | The absence, yes. **That the OTPMK is unreadable by software, and whether NXP or the customer programs it, is unchecked** |
| The DCP does AES-128 only | `NP-FW-EMMC-001` `EMMC-UHDR-06` (an internal document) | **Unchecked** against NXP |
| What the DCP's *unique key* (`0xFE`) is derived from on this part | — | **Unchecked** |
| Whether SW_GP2 is also what BEE or another boot feature uses as its key (which would collide with flash or image encryption, `OI-SEC-02` (c)) | — | **Unchecked** |
| The SDK example's comment speaks of *"[255:128] from ocotp key"*, a 256-bit OCOTP key, while the header gives SW_GP2 128 bits | `dcp.c` lines 39–42 against `MIMXRT1062.h` | **Contradiction not resolved.** This brief assumes 128 bits, the header's figure |
| Burning a fuse in the factory needs a supply and timing argument | first brief §4, option A | **Unchecked** (reference manual, datasheet) |

### 2.3 What the existing design already does with these parts

- **The SHDR key** is derived by NeurOne from a fleet-wide manufacturing root key and the device serial, injected into the Config partition at manufacture, and wrapped under the SNVS OTPMK (`NP-FW-EMMC-001` §7.3, `EMMC-SHDR-04` to `-07`). So **the application already uses the DCP with the SNVS master key** at every boot. That decides option D below.
- **The bootloader is not authenticated by the ROM.** Its image header sets `csf = 0` (HAB disabled, `firmware/bootloader/src/np_main.c:103`), and no document specifies HAB closed configuration or a debug-port lock for production (`OI-SEC-02`, open). **Whatever runs first on the RT1062 can derive or read any key the RT1062 holds.** No key source keeps the key from an attacker who can replace the bootloader. Every option below assumes `OI-SEC-02` (a) and (b) are done.
- **As built, the application raises the floor.** `np_ota_confirm_boot()` raises it (`np_ota.c:553-565`), and `np_ota.h` step 9 says the application calls it. `np_ota_verify_scratch()`, which reads the floor, is also application-side (step 3). An RPMB floor store under that code needs the key in the application. **So the key can be kept from the application only if the floor is read and raised by the bootloader alone**: the application records "confirmed", the bootloader raises the floor at the next boot before it hands over, and the application's own admission check reads a floor value the bootloader handed it (advisory; the bootloader's boot trial already checks the floor itself, `np_ota_handle_boot_verification`). The RPMB store is now built (`REQ-SW02-16`, `np_rollback_store_rpmb.c`, PR #1030) under the same application-side callers, so this is a change to the callers of that store, and is recommended under Q1.

### 2.4 How others do it

OP-TEE, the reference open-source trusted OS on Arm, derives its RPMB key on the device from the platform's hardware unique key and the eMMC's CID, with the CID's product-revision and CRC bytes masked because a firmware update of the eMMC changes them (`tee_rpmb_fs.c`, `tee_rpmb_key_gen()`, `huk_subkey_derive(HUK_SUBKEY_RPMB, cid…)`). It programs the key only when built with `CFG_RPMB_WRITE_KEY`, and its configuration says: *"Do NOT enable this in product builds, as doing so would allow the TEE to leak the RPMB key … useful in … RPMB key provisioning in a controlled environment (factory setup)"* (`mk/config.mk`, read 2026-11-23). It also has `CFG_RPMB_TESTKEY`, a hard-coded key for testing. **That is exactly the path this design forbids** (`REQ-RPMBK-01`).

Two lessons carried into the options:
1. **Bind the key to the part's CID.** A key captured on the bus while one part is programmed is then useless against any other part, and a replacement part gets a new key without anyone choosing one.
2. **Never program a blank part automatically.** An interposer that answers "key not programmed" would be handed the key. Programming happens only in a controlled step.

## 3. Q1: where the key comes from

| Option | What it is | Can the application get it? | Factory sees it? Key database? | Replaced eMMC | Cost |
|---|---|---|---|---|---|
| **A. Derived on the device from a fused device secret** | At the factory step the RT1062 draws 128 bits from its own TRNG, burns them into **SW_GP2**, then burns `SW_GP2_RLOCK` and `SW_GP2_LOCK`. At every boot the bootloader selects SW_GP2 as the DCP's OTP key (`GPR10` bit 4 = 1), derives `K = KDF(SW_GP2; "NeurOne.RPMB.1" ‖ CID with PRV and CRC masked)` through the DCP, then switches the mux back to the SNVS master key and sets `LOCK_DCPKEY_OCOTP_OR_KEYMUX` before the application runs | **No, if** the mux lock holds until reset (unchecked) and the bootloader alone reads and raises the floor (§2.3). With the read lock burnt, the application cannot read SW_GP2 through OCOTP; with the mux locked to the SNVS master key, its DCP calls reach the OTPMK, which the SHDR wrap needs, and never SW_GP2 | **No and no.** The secret is generated and burnt by the unit and never leaves it. No list of keys exists anywhere | The unit derives the new part's key itself (new CID, new key). Programming waits for Q2's authorisation; until then the blank part refuses every image | A device-side KDF and a CMAC or counter-mode construction over AES-128 (Q3). The key's strength is the secret's, **128 bits**. SW_GP2 is spent on this and unavailable to BEE (unchecked whether that matters, `OI-SEC-02` (c)). The fuse burn is a first for this product |
| **B. A random key generated at the factory and fused** | The station generates the key, programs it into the eMMC from outside, and burns it (or a secret from which it is derived) into RT1062 fuses | As A if it is in SW_GP2 behind the mux; a full 256-bit key does not fit SW_GP2 (128 bits), and the `GP3`/`GP4` fields have their own locks (`OCOTP_LOCK_GP3`, `GP4`, `GP4_RLOCK`) but no path to the DCP was found for them, so the bootloader would have to read them in software, and so could the application unless a read lock is set after boot (unchecked whether one can be) | **Yes, at the station, for every unit.** A database exists only if it is kept; nothing needs it, because the unit holds its key | The unit holds the key, so it can program the new part, under Q2. No CID binding unless derived as in A | Simpler device code (no KDF if the raw key is fused). A station that sees every key is a target while it runs, and its logs are a target after |
| **C. Re-derivable by NeurOne (the SHDR pattern)** | `K = HKDF(a fleet RPMB root key in NeurOne's HSM, device serial)`, programmed at the station, stored on the unit wrapped under the OTPMK in Config | **Yes**: the application unwraps Config keys through the same DCP path today (§2.3) | **Yes, and NeurOne can recompute every unit's key**: the HSM is a fleet-wide downgrade key | **Worse than A or B:** Config is erased by factory reset (`np_main.c`), so a reset unit loses its key and refuses every image until NeurOne re-provisions it | Reuses an existing pattern. One HSM breach lowers the floor on every unit. Not recommended |
| **D. Derived from the OTPMK through the SNVS master key** | As A, but with the mux left on the SNVS master key | **Yes.** The application uses that path for the SHDR key (§2.3), so it can derive the same RPMB key | No and no | As A | Rejected for the application reason. It would be acceptable only if the SHDR wrap moved off the OTPMK |

**None of these keeps the key from code that runs before the bootloader's lock**, so all of them depend on `OI-SEC-02` (secure boot and the debug-port lock).

**Data boundary.** In no option does the key, the secret or any record of them touch UHDR or user identity: the inputs are a fuse value, the eMMC CID and a label. Under C, the HSM holds a device-scoped secret, the same class as the SHDR root. The production record of §7.10 step 8 holds no key in any option.

**Recommendation: A.** It is the only option in which neither the factory nor NeurOne ever holds a key, no key database exists, a replaced eMMC needs nobody to look a key up, and a key leaked on the bus is worth one part. It needs three confirmations from the reference manual before it is built: that `LOCK_DCPKEY_OCOTP_OR_KEYMUX` holds until reset, that `SW_GP2_RLOCK` stops every software read path while leaving the DCP path, and that SW_GP2 is free (not wanted by BEE). **If the mux lock does not hold until reset, no RT1062 mechanism found keeps the key from a compromised application**, and the choice is A anyway (it still keeps the key off the factory and out of any database), with the residual risk recorded: a signed application that is exploited can lower the floor, and the registry of the first brief §4.1 detects and repairs that at the next connection.

## 4. Q2: who may program a blank RPMB after manufacture

A blank or wrong-key RPMB refuses every image (first brief §8, Q1 conditions). It must stay that way until a provisioning step runs, and that step must not happen on its own (§2.4, lesson 2).

| Option | What it is | Costs |
|---|---|---|
| **(a) A signed service authorisation** | Service obtains a message signed by a NeurOne service key, bound to the unit's ID and the new part's CID, carrying the floor to write (from the registry of the first brief §4.1, or the current release). The bootloader checks it, programs the key, writes the floor | Field repair without returning the unit. A new signing key to hold (custody like the tier authority's, `OI-UPG-08` (i)). The registry stays optional: without it, service uses the current release's version as the floor |
| **(b) Factory-mode fixture only** | The unit goes back to a service centre with the factory station, which runs §7.10 again | No new key. Costs a return for every eMMC replacement |
| **(c) Automatic** | The bootloader programs any blank part it finds | **Rejected.** An interposer posing as a blank part harvests the key (§2.4), and a fresh part would start at floor 0, which is the downgrade the floor exists to stop |

**Recommendation: (b) until a service network exists, then (a).** (b) needs nothing new. (a) is worth its key once eMMC replacement in the field is a real service.

## 5. Q3: C or Rust for the key derivation

**Since this brief was drafted, the RPMB store (PR #1030) raised `OI-SW02-01` for the MAC, with its own brief (`docs/human-input/oi-sw02-01-rpmb-mac-in-the-bootloader.md`).** The derivation needs a MAC or block-cipher construction of the same kind, so the two should be decided together. The options below are kept for the derivation; **the recommendation is that the derivation follow `OI-SW02-01`'s answer**, so the bootloader does not end up with its cryptography split across C and Rust.

`CLAUDE.md` §22 puts key derivation and message authentication in Rust in `common/npps-core`, and permits `no_std` Rust on the main processor (the principal, 2026-11-11). The bootloader is C today, and already does cryptography in C: Ed25519 through the vendored Monocypher, and its own SHA-256 in `np_signature.c`.

| Option | What it is | Costs |
|---|---|---|
| **(a) C in the bootloader, recorded as an exception** | The KDF (over the DCP) and HMAC-SHA256 in C beside `np_signature.c` | Matches what is there. A second implementation of HMAC-SHA256 beside the Rust one in `common/npps-core` (which the host model of §7.10 uses), so the two must be pinned to each other by test vectors |
| **(b) `no_std` Rust from `common/npps-core`, linked into the bootloader** | The KDF's software half and HMAC-SHA256 compiled for Cortex-M7 and called through the C ABI | One implementation, as §22 asks. A Rust cross-toolchain in the bootloader build, and a code-size figure nobody has measured (the bootloader's budget is in its linker script) |

**Recommendation: whatever `OI-SW02-01` decides for the MAC.** Either way the DCP calls stay in C, because they are register access, not cryptography.

## 6. What this change has built (no decision was needed)

- **The seam.** `firmware/bootloader/include/np_rpmb_key.h` declares `np_status_t np_rpmb_key_get(uint8_t key[32]);`, the signature the RPMB floor store (PR #1030) calls; that PR created the header, and this one adds the no-key-material-on-error clause and the `REQ-RPMBK-01` reference.
- **The device build fails closed.** `firmware/bootloader/src/np_rpmb_key_unbuilt.c` always returns an error and wipes the buffer. The floor store therefore cannot authenticate, the floor reads as an error, and every image is refused, as it is today. It is never replaced by a fixed, all-zero or build-time key (`REQ-RPMBK-01`).
- **The factory step's design and its requirements:** `NP-REG-UPG-001` §7.10, `REQ-RPMBK-01` to `-06`.
- **A host model of the programming sequence:** `common/npps-core/src/rpmb_provision.rs`. It refuses an all-zero or all-one key before sending anything; reads the part first; programs only a part that reports no key; never sends a second programming request; proves the key with an authenticated read whose MAC verifies and whose nonce comes back; refuses a part holding another key; accepts a re-run on a part already holding this key. Ten tests, against a simulated part.

## 7. What to send back

- Q1: A, B, C or D, and, if A, who confirms the three reference-manual points.
- Q2: (a), (b) or (c), now and later.
- Q3: follow `OI-SW02-01` (recommended), or (a) or (b) for the derivation alone.
- For each: a reason, what would reopen it, and a name, role and date.

Once answered, the answer is recorded in `OI-PRIV-32` (NP-PRIV-ANALYSIS-004), in `NP-REG-UPG-001` §7.10 and in `docs/status/completed-decisions.md`. Nothing in the repository acts on it before then: the key source stays the failing stub.
