# Question for the principal: OI-PRIV-27 (NP-PRIV-ANALYSIS-004), how a person starts a factory reset

**Status:** DRAFT, not sent. Not a controlled document; see `docs/human-input/README.md`.
**GitHub issue:** [#955](https://github.com/stevehickman/NeuroPulse/issues/955)
**From:** NeurOne Firmware Engineering (`OI-PRIV-27` (`NP-PRIV-ANALYSIS-004`) in `docs/status/pending-decisions.md`; firmware privacy review `NP-PRIV-ANALYSIS-004`, Finding H7)
**To:** the principal (a wire and design change); Firmware and Software to implement
**Response needed:** an option for Q1 and Q2, a yes or no on Q3, a short rationale, anything that would reopen it, and your name, role and the date.

## 1. The question

A factory reset now purges what it erases (eMMC SANITIZE after the erases, `REQ-SW02-13`, done in the change that wrote this brief). **Nothing can start one.** `np_factory_reset_execute()` has no caller outside its own tests. No GATT characteristic, no USB-C message and no app screen reaches it. `NP-FW-EMMC-002` §B.3 R-1 and R-2 say "the app issues a signed FACTORY_RESET command" and "the hub verifies the signature". Neither half exists, and no wire format was ever specified.

Plain form: **what is the command, who may send it, and how does the hub know it is genuine and fresh?**

This is a decision, not a detail. It adds a path that destroys a person's data on the device. It is a new wire message under `CLAUDE.md` §23, and it decides whose consent an erase needs under §5.1 and §6.0.

## 2. What is known

Checked on 2026-11-21 against `main` at `49d6c51`.

| Fact | Where |
|---|---|
| `np_factory_reset_execute()` is called only by `firmware/factory_reset/tests/`. `np_factory_reset_boot_check()` (finishing an interrupted reset) is called at bring-up. | `grep` over the tree; `firmware/hub_control/src/np_hub_control_main.c:599` |
| R-1/R-2 are comments: "caller already verified the Ed25519-signed FACTORY_RESET command", "this module accepts unconditionally". | `firmware/factory_reset/src/np_factory_reset.c` |
| R-4 assumes no session is active and does nothing. It is "enforced by the hub layer", and no hub layer exists for it. | same file |
| The only command path into the hub is the protocol upload (0x0008, BLE with link encryption; USB-C CDC with none). It is reassembled by `np_transport.c` and verified as a session descriptor. | `docs/np_fw_hub_001.md` §2.3, §2.5 |
| §B.2 says: user-initiated, "Reset Device" → "Transfer or Sell Device", current biometric plus a two-step confirmation. No app has this screen. The apps' "Erase this computer's data" erases the app's own store, not the helmet, and says so. | `docs/np_fw_emmc_002.md` §B.2; `locales/en.json` `SETTINGS_ERASE_DEVICE_*` |
| **The BIPA consent text already promises the reset:** EEG data is kept "until I request deletion or perform a factory reset, at which point it is permanently destroyed via hardware-level secure erase (eMMC SANITIZE)", and "use factory reset for full erasure". Until a reset can be started, that promise is not met on a shipped device. | `locales/en.json` `BIPA_STEP4_RELEASE_TEXT` |
| The warranty owner may be a clinic, institution or purchaser, and is not assumed to be the wearer. The UHDR owner is the user. | `CLAUDE.md` §5.1, §6.0 |
| "An erase is not a weakening": it destroys what the presence gate protected. | `CLAUDE.md` §5.1 |
| Erasing the safety MCU's cardiac log on reset is a separate ISO 14971 decision. It is not asked here. | `NP-PRIV-ANALYSIS-004` H5 |

## 3. Q1: the form of the command

| Option | What it is | Cost | Risk |
|---|---|---|---|
| **A. A signed reset blob on the existing upload path** | A new blob type beside the session descriptor, on 0x0008 and the same framing. It carries the device serial, a hub-issued nonce and the request, Ed25519-signed with the same infrastructure. Verified by `np_proto_verify`'s code, then dispatched to the reset instead of the runner. | Moderate: a blob type, a nonce read characteristic, a dispatcher branch, app and simulator support, tests. | One parser serves two kinds of blob, so a type confusion must be impossible by construction (distinct magic, verified before dispatch). |
| B. A dedicated GATT characteristic | A write-only reset characteristic with its own signed payload. | Similar; also a new UUID in the frozen table (`REQ-SW03-01`). | Two verification paths to keep identical. |
| C. USB-C only, with physical presence | The reset is accepted only over a cable. | Lower on BLE. | USB-C CDC has no encryption (§23 gap, `OI-SIMFW-70`), so this path is not conformant until its payload is encrypted. |

**Every option needs:**
- a nonce from the hub, so a recorded reset cannot be replayed on this device or any other;
- binding to the device serial;
- refusal while a session is active (R-4 made real);
- an encrypted path (§23).

## 4. Q2: who may authorize it

| Option | Meaning | Consequence |
|---|---|---|
| **A. The person, present (§B.2 as written)** | The app signs only after the person's biometric or passphrase (`docs/np_app_presence_001.md`) and the two-step confirmation. | Matches §5.1: the user owns UHDR, and only the user destroys it. A clinic that owns the warranty cannot reset a wearer's helmet without that wearer. |
| B. The person or the warranty owner | Either may reset. | A clinic can recover a device from a patient who has left. It also lets an institution destroy a person's health data without the person. That needs your ruling under §6.0. |
| C. A as default, plus an offline reset | A, plus a physical-presence reset on the helmet (for example a held button at boot). It needs no app, for a device whose owner lost their phone. | It destroys UHDR without the owner's credential. §5.1 says an erase is not a weakening, but the new owner of a stolen device could use it. It is a hardware and IFU change. |

## 5. Q3: the BIPA text

Until the reset ships, should the consent text keep saying "perform a factory reset"? Yes means it stays, and the gap is tracked here. No means Software changes it to what is true today (regulatory and legal review, `docs/reference/regulatory-strategy.md`).

## 6. Recommendation

- **Q1: A.** It uses the one framing and verifier the hub already has, needs no new frozen UUID, and is encrypted on BLE today. Accept it on BLE only until USB-C carries payload encryption. The nonce and serial binding are part of the format, not added later.
- **Q2: A.** The erase destroys the user's record, so it needs the user. The lost-phone case is real. I recommend raising option C as its own decision with the hardware, not folding it in here.
- **Q3: No.** Change the text to what is true until the reset ships. A consent text that promises an erase the product cannot perform is the kind of statement a regulator reads literally.

## 7. What does not wait for this

Done in the same change:
- the corrected command in `NP-FW-EMMC-002` §B.3;
- `np_emmc_sanitize()` in the bootloader, and the sequence in `np_emmc_sanitize.h` with host tests;
- R-7a, which purges UHDR, SHDR and Config after their erases (`REQ-SW02-13`).

Still open under `OI-PRIV-27` (`NP-PRIV-ANALYSIS-004`) whatever is decided here:
- JEDEC JESD84-B51 and the selected part's datasheet read against the command;
- the 240 s bound (`UC-202`);
- the application's eMMC transport behind the trapping HALs (`OI-RESET-01`).
