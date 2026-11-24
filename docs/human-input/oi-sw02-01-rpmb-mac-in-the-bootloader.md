# Decision brief: OI-SW02-01: how the C bootloader computes the RPMB MAC (HMAC-SHA256)

**Status:** DRAFT, not sent. Not a controlled document; see `docs/human-input/README.md`.
**GitHub issue:** [#960](https://github.com/stevehickman/NeuroPulse/issues/960) (shared with `OI-PRIV-32` (NP-PRIV-ANALYSIS-004), which this item unblocks)
**From:** NeurOne Firmware Engineering (`docs/status/pending-decisions.md` `OI-SW02-01`; `NP-SW-001` `REQ-SW02-16`)
**To:** the principal (`CLAUDE.md` §22 is the principal's rule, and a choice of where a cryptographic primitive lives is a design decision under it). Firmware for the build. No name is assigned.
**Response needed:** an option, a short reason (`CLAUDE.md` §18), what would reopen it, and a name, role and date.

## 1. The question

The firmware rollback floor now lives in the eMMC's RPMB (`OI-PRIV-32`, the principal, 2026-11-23). Every RPMB frame is authenticated with **HMAC-SHA256** under the 32-byte RPMB key: the bootloader computes it on every request it signs and checks it on every response. The frame protocol and the store are built and host-tested (`firmware/bootloader/src/np_rpmb.c`, `np_rollback_store_rpmb.c`, `REQ-SW02-16`). The HMAC itself is a seam, `np_rpmb_hmac_sha256()` in `firmware/bootloader/include/np_rpmb_mac.h`, and **the device build links a stub that fails**. So the floor is unreadable and the device installs no OTA or DFU image (fail closed).

**Where should the bootloader's HMAC-SHA256 come from?**

## 2. Why it needs a decision

- `CLAUDE.md` §22: *"Real-time and cryptographic work is written in Rust, once, in `common/npps-core`"*, and `engineering-rules.md` lists **message authentication** and **hashing** as cryptographic. It also says the main processor's firmware is C, *"with `no_std` Rust permitted there"* (the principal, 2026-11-11).
- The bootloader runs on the main processor and is C. It **already** carries cryptographic C written before that rule: a first-party SHA-256 (`np_sha256()` in `firmware/bootloader/src/np_signature.c`, used to hash every firmware image) and vendored Monocypher 4.0.2 for Ed25519 (`NP-SW-001` §9.4). Neither has a recorded §22 exception or an open item.
- Adding HMAC in C is about 30 lines on top of that SHA-256. That is the easy path, and it is exactly a new C implementation of a primitive, which §22's detail calls a design decision to record first. So this change built everything else and stopped here.

## 3. Options

| Option | What it is | Costs and limits |
|---|---|---|
| **A. HMAC in the bootloader's C, on its existing SHA-256** | RFC 2104 over `np_sha256()`, in `firmware/bootloader/`; checked against RFC 4231 vectors in a host test (the host test already does exactly this for its oracle) | Smallest change, no new toolchain, fits the 48 KiB bootloader easily. **It is an exception to §22**, and it leans on a SHA-256 that is itself an unrecorded exception. If chosen, the exception should name both, and say when they move (for example, if Rust enters the bootloader for any other reason) |
| **B. `no_std` Rust from `common/npps-core`, linked into the bootloader** | The same HMAC-SHA256 the apps use, compiled for `thumbv7em-none-eabihf` as a static library behind the C seam | Meets §22 as written: one implementation, everywhere. **Costs:** a Rust cross toolchain in the firmware build and CI (`firmware-cross-build.yml`), a `no_std` build of the core's hash and MAC (the crates it uses must build without `std`, or a `no_std` subset is split out), a C ABI for the bootloader, size inside the 48 KiB budget to be measured, and each crate in `docs/np_soup_rust_001.md`. The bootloader would then hold two SHA-256 implementations (C for images, Rust for RPMB) until the image hash also moves |
| **C. The i.MX RT1062 DCP hash engine** | The on-chip Data Co-Processor computes SHA-256; HMAC is built on it in C (the two hash passes and the key pads) | Hardware hashing is "what only the platform can do" under §22's detail, but the HMAC construction around it is still a C primitive, so it does not avoid the question; it narrows it. Needs a DCP driver (not written), and the DCP and its errata are a new dependency of the boot path |

**Recommendation: B if the Rust toolchain is wanted in the main-processor firmware anyway (the 2026-11-11 permission suggests it is coming); otherwise A, recorded as a §22 exception that covers the bootloader's existing SHA-256 too, with a trigger to move both to Rust.** Not C on its own: it adds a driver and does not remove the C construction.

Whichever is chosen, the seam stays, and its contract does not change: an error from the primitive refuses every image.

## 4. What is not affected

- No protection is lowered: until this is decided the device refuses every image, which is stricter than today's behaviour on any built unit (none exists).
- The RPMB key source and its factory programming are another change (`np_rpmb_key.h`).
- The nonce entropy source is `OI-SW02-03`: reading the hardware TRNG is the platform's part under §22 and needs only a driver.

## 5. What to send back

- A, B or C (or another source), with a reason, what would reopen it, and a name, role and date.
- If A: whether the exception also covers the bootloader's existing C SHA-256 and Monocypher, and its revisit trigger.

Once answered, the answer is recorded in `OI-SW02-01` and in `docs/status/completed-decisions.md`, and the device build's `np_rpmb_mac_unbuilt.c` is replaced. Nothing in the repository acts on it before then.
