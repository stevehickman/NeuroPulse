# NeuroPulse Firmware: Privacy Remediation Delta (NP-FW-EMMC-002 §B, §D, §E + OI-EMMC2-06)

## Context

NP-FW-EMMC-002 Rev A (2026-06-02) is a privacy remediation delta that patches four open items left unimplemented after the initial eMMC firmware spec (NP-FW-EMMC-001 Rev A):

- **§B** — Device factory reset procedure (R-1 through R-12), required for device transfer/resale. Blocks NP-PRIV-REM-001 STEP-02. Without this, a new owner's usage is associated with the previous owner's SHDR history.
- **§D** — Scratch partition AES-256-CTR encryption for the research anonymisation engine. Required before the anonymisation engine (NP-FW-ANON-001) can be built. Without this, decrypted UHDR data persists in plaintext on Scratch if power is lost mid-anonymisation.
- **§E** — EDF+ patient header policy: writer + validator. Blocks first EEG session recording implementation (OI-EMMC2-05). Without this, EDF+ files would contain real patient names/birthdate in cleartext.
- **OI-EMMC2-06** — No-join CI test for warranty_db × shdr_db. Blocks NP-PRIV-REM-001 STEP-01. Analogous to the SHDR accelerometer schema gate (OI-EMMC2-07, already passing in CI).

## Research Summary

**Firmware conventions** (from 7 existing modules):
- Module layout: `firmware/<module>/include/*.h` + `firmware/<module>/src/*.c` + optional `tests/*.c` + `CMakeLists.txt`
- Status codes: `np_status_t` from `firmware/bootloader/include/np_types.h` — reuse `NP_OK`, `NP_ERR_GENERIC`, etc. New modules define their own `np_<module>_status_t` where needed.
- Structs: `__attribute__((packed))` for wire/eMMC format
- Sensitive data zeroing: always `memset_explicit()` — never plain `memset()`
- SNVS: `NP_SNVS_LPGPR0` defined in `firmware/bootloader/include/np_config.h`. Need to add `NP_SNVS_LPGPR1` (reset_in_progress) and `NP_SNVS_LPGPR2` (anon_in_progress) defs.
- HAL stubs: unimplemented platform calls documented as `OI-<MODULE>-NN` open items

**CI conventions** (from `ci/test_shdr_schema.py` + `ci/shdr/shdr_fleet_schema.sql`):
- Python 3.11, argparse, pure regex/DDL parsing — no external dependencies
- Exit 0 = PASS, exit 1 = FAIL; `--verbose` flag; `--schema PATH` override
- Check functions return `list[Failure]`; `Failure` is a dataclass with `check_id`, `description`, `location`
- GitHub Actions workflow mirrors `shdr-schema-ci.yml`: push/PR on relevant paths, `ubuntu-latest`, Python 3.11
- The warranty schema SQL lives at `ci/warranty/warranty_registration_schema.sql`

**EDF+ spec** (from NP-FW-EMMC-002 §E.2):
- `local patient id`: `NP[first 14 hex chars of UHDR token]` + ` X X NeuroPulse_User` (total 80 bytes)
- `local recording id`: `Startdate DD-MMM-YYYY NeuroPulse X NeuroPulse_v[FW_VER]` (total 80 bytes)
- UHDR token = 32 bytes binary; first 14 → hex = 28 chars, but per §E.2 example it's 14 raw bytes → 28 hex chars, then "NP" prefix makes 30-char patient code. Re-check: per E.2 the example shows "NPa3f7b9c2d1e4f5" which is NP + 14 hex chars = 16 total. So 7 raw bytes → 14 hex chars.
- EDF header is 256 bytes: 8-byte version, 80-byte patient id, 80-byte recording id, 8-byte startdate, 8-byte starttime, 44-byte reserved, 8-byte num data records, 8-byte duration, 4-byte num signals.
- Host-compilable tests using `NP_BUILD_TESTS` flag (as per sloreta_hdtdcs CMakeLists pattern)

**Factory reset** (from §B.3):
- 12-step sequence R-1..R-12
- `reset_in_progress` flag → SNVS_LPGPR1
- Bootloader must detect and re-run SANITIZE on UHDR+SHDR+Config on next boot if flag set
- `np_emmc_sanitize()` HAL stub (CMD38 SANITIZE_START) already defined in `firmware/bootloader/src/np_emmc.c`

**Scratch encryption** (from §D):
- `np_anon_scratch_ctx_t` lives in SRAM only — never written to any partition
- AES-256-CTR: counter = nonce (16 bytes) XOR'd with block_offset — stub as HAL
- `anon_in_progress` flag → SNVS_LPGPR2
- Bootloader SANITIZE on Scratch if LPGPR2 flag set on boot

---

## Work Units

### Unit 1 — Factory reset firmware module
**Files:**
- `firmware/factory_reset/include/np_factory_reset_config.h` — SNVS_LPGPR1 bit defs, timeouts
- `firmware/factory_reset/include/np_factory_reset_types.h` — `np_reset_status_t`, `np_reset_state_t`
- `firmware/factory_reset/include/np_factory_reset.h` — public API (`np_factory_reset_execute`, `np_factory_reset_is_in_progress`, HAL stubs)
- `firmware/factory_reset/src/np_factory_reset.c` — 12-step sequence, SNVS_LPGPR1 power-loss resilience
- `firmware/factory_reset/CMakeLists.txt`
- `firmware/bootloader/include/np_config.h` — add `NP_SNVS_LPGPR1`, `NP_SNVS_LPGPR2`, `NP_SNVS_RESET_IN_PROGRESS`, `NP_SNVS_ANON_IN_PROGRESS` constants
- `firmware/bootloader/src/np_main.c` — add SNVS_LPGPR1 check at boot (re-run SANITIZE+zero on UHDR/SHDR/Config if flag set)

**Description:** New `np_factory_reset` static library implementing R-1..R-12 from NP-FW-EMMC-002 §B. TRNG calls, eMMC SANITIZE, partition zeroing, and new warranty token generation are all HAL-stubbed (OI-RESET-01..OI-RESET-05). Power-loss resilience via SNVS_LPGPR1. Bootloader boot-time detection of in-progress reset also added.

---

### Unit 2 — Scratch partition AES-256-CTR encryption
**Files:**
- `firmware/anon/include/np_anon_config.h` — block size, SNVS_LPGPR2 bit def
- `firmware/anon/include/np_anon_types.h` — `np_anon_status_t`
- `firmware/anon/include/np_anon_scratch.h` — `np_anon_scratch_ctx_t`, public API, HAL stubs
- `firmware/anon/src/np_anon_scratch.c` — init/write/read/complete with TRNG key gen, AES-256-CTR, memset_explicit, SANITIZE
- `firmware/anon/CMakeLists.txt`
- `firmware/bootloader/src/np_main.c` — add SNVS_LPGPR2 check at boot (Scratch SANITIZE if anon_in_progress set)

**Note:** Unit 2 also touches `np_main.c` (same file as Unit 1). These two units must be reviewed together for the bootloader edits. Each unit adds different SNVS checks; they don't conflict on lines but the worker should be aware of the parallel edit. The bootloader changes can be staged so each PR touches different contiguous blocks of `np_main.c`. Alternatively, the bootloader edits can be bundled with Unit 1 (the reset module) since the SNVS_LPGPR1/LPGPR2 defs depend on the same config header.

**To avoid conflict:** Unit 1 worker adds SNVS_LPGPR1 check to np_main.c; Unit 2 worker adds SNVS_LPGPR2 check. Each adds a separate `if` block after the OTA_PENDING check. The workers should note this in their PR descriptions.

**Description:** New `np_anon_scratch` static library implementing NP-FW-EMMC-002 §D. AES-256-CTR HAL-stubbed (OI-ANON-AES-01). TRNG HAL-stubbed (OI-ANON-AES-02). SNVS_LPGPR2 `anon_in_progress` flag for power-loss resilience.

---

### Unit 3 — EDF+ patient header writer and validator
**Files:**
- `firmware/edf/include/np_edf_config.h` — EDF header field offsets and widths per EDF+ spec §2.1.3
- `firmware/edf/include/np_edf_types.h` — `np_edf_status_t`, `np_edf_header_t` (256-byte packed struct)
- `firmware/edf/include/np_edf_writer.h` — `np_edf_write_header()`, `np_edf_validate_privacy_header()`, `np_edf_header_contains_real_name()`
- `firmware/edf/src/np_edf_writer.c` — write_header: hex conversion of UHDR token, field fill per §E.2
- `firmware/edf/src/np_edf_validator.c` — validate_privacy_header: checks sex='X', birthdate='X', patient name constraint
- `firmware/edf/tests/np_edf_tests.c` — host-compilable tests: generate 100 EDF+ headers with varied tokens, verify all pass validator; test known-bad headers fail; round-trip test (OI-EMMC2-05)
- `firmware/edf/CMakeLists.txt` — static lib + `NP_BUILD_TESTS` host test target

**Description:** New `np_edf` static library. The 256-byte EDF header struct maps the standard EDF+ layout. Writer fills fields per NP-FW-EMMC-002 §E.2: NP prefix + 14 hex chars from UHDR token as patient code, sex/birthdate/name = 'X'/'X'/'NeuroPulse_User', recording ID with date from session_ts and firmware version. Validator checks the three privacy fields. Host-side unit tests compilable with `-DNP_BUILD_TESTS`.

---

### Unit 4 — Warranty token no-join CI test (OI-EMMC2-06)
**Files:**
- `ci/warranty/warranty_registration_schema.sql` — minimal warranty DB schema (has `warranty_token BYTEA`, `registered_name`, `registered_email`, `registered_address`, `serial_number`, `registration_date`) — represents the warranty_db that must NOT be joinable with shdr_db
- `ci/test_warranty_nojoin.py` — Python 3.11 CI gate for OI-EMMC2-06; analogous structure to `ci/test_shdr_schema.py`
- `.github/workflows/warranty-nojoin-ci.yml` — GitHub Actions workflow

**Description:** New CI test confirming the no-join invariant between warranty_db and shdr_db. Checks:
- `NOJOIN-SHDR-01`: SHDR schema contains no FOREIGN KEY references to warranty_registration, registrant, or warranty_owner tables
- `NOJOIN-SHDR-02`: SHDR schema columns do not include name/email/address (would enable implicit join via PII)
- `NOJOIN-WARRANTY-01`: Warranty schema contains no references to SHDR table names (devices, pbm_zone_telemetry, fault_log, etc.)
- `TOKEN-BOTH-01`: `warranty_token` is BYTEA in both schemas
- A `--shdr-schema` and `--warranty-schema` path override (defaults to `ci/shdr/shdr_fleet_schema.sql` and `ci/warranty/warranty_registration_schema.sql`)

---

## E2E Test Recipe

**Unit 1 (factory reset) — skip e2e:** Cross-compiled ARM firmware; no host-executable path. Verify via: (1) code review, (2) confirm it compiles cleanly with the ARM toolchain if available, (3) read the implementation matches the 12 steps R-1..R-12 in NP-FW-EMMC-002 §B.3 exactly.

**Unit 2 (scratch encryption) — skip e2e:** Same rationale as Unit 1. Verify via code review against §D.2–§D.6. Confirm `memset_explicit` is used (not `memset`) for all key zeroing.

**Unit 3 (EDF+ writer/validator) — host unit tests:**
```bash
# From repo root
cmake -B build/edf-tests \
  firmware/edf \
  -DCMAKE_BUILD_TYPE=Debug \
  -DNP_BUILD_TESTS=ON
cmake --build build/edf-tests
./build/edf-tests/np_edf_tests
# Expected: 0 failures, output "PASS: 100/100 header generation tests"
```

**Unit 4 (no-join CI test):**
```bash
# Run the new no-join test
python3 ci/test_warranty_nojoin.py --verbose
# Expected: RESULT: PASS

# Also confirm existing SHDR test still passes
python3 ci/test_shdr_schema.py --verbose
# Expected: RESULT: PASS
```

---

## Worker Instructions (shared template)

Conventions all workers must follow:
- Firmware module naming: `firmware/<module>/include/np_<module>_*.h`, `src/np_<module>_*.c`
- Status enum: `NP_<MODULE>_OK = 0`, `NP_<MODULE>_ERR_* = negative ints`
- Include guards: `#ifndef NP_<MODULE>_<SUBSYSTEM>_H`
- Document reference comment at top of each file: `Document: NP-FW-EMMC-002 Rev A §<section>`
- Sensitive zeroing: ALWAYS `memset_explicit()`, NEVER `memset()`
- HAL stubs: declare `extern` function in header with an `/* OI-XX-NN — platform HAL stub */` comment; provide a no-op stub in `.c` for host build (`#ifdef NPTEST_HOST`)
- `__attribute__((packed))` on all wire/eMMC-format structs
- CMakeLists: static lib target + `if(NP_BUILD_TESTS)` host test target with `-DNPTEST_HOST -O0 -g`
- No heap. All buffers stack-allocated or `static`.
- SNVS registers: use the existing register-access pattern from `np_config.h` (volatile pointer dereference)
