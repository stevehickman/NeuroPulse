# Vendored littlefs — NeurOne integration notes

littlefs **v2.11.3**, the storage filesystem for the **SW-02 main processor**
(NXP i.MX RT1062, Cortex-M7 @ 600 MHz). IEC 62304 Class B SOUP — the formal
record is `VERSION`, NP-SW-001 §9.4, and the hazard analysis
`docs/np_soup_lfs_001.md` (NP-SOUP-LFS-001 Rev 3).

## Read this before relying on anything here

This directory closes **OI-LFS-01** (pin + vendor). **OI-LFS-02** — the §7.1.2
anomaly evaluation and the power-loss test — is closed too, but not by anything
in this directory: it is closed by `NP-SOUP-LFS-001` §11 and §12 and by
`firmware/hub_control/tests/np_lfs_powerloss_tests.c`.

- **Nothing calls it yet.** The `OI-LOG-05..07` HAL seams in
  `firmware/hub_control/include/np_log_backend.h` are still unimplemented, and
  `np_littlefs` is linked into no image. The library builds, its configuration
  is tested, and the power-loss suite mounts it over a **test** block device;
  the device's own storage stack does not exist.
- **`L-1…L-4` hold against the `struct lfs_config` contract, not against the
  eMMC.** 456 interrupted runs, 0 violations (`NP-SOUP-LFS-001` §12) — but the
  injector interrupts the contract at `prog_size` granularity, and the medium is
  managed flash behind an XTS layer whose tear and erase semantics are not the
  raw-flash semantics littlefs is written against. `OI-LFS-07` needs hardware.
  `NP-FW-NVRAM-001` §4, `EMMC-FS-01` and `NP-FW-HUB-001` §6.5 may be relied on
  exactly as far as §12.5 says and no further.
- **Two caller obligations came out of the anomaly evaluation and are not met.**
  `OI-LFS-08` (upstream #1210 silently drops a file whose data keeps a valid
  CRC; the worst instance is `ukmd.rec`, whose loss makes a user's UHDR
  permanently unmountable) and `OI-LFS-09` (validate stored values by content on
  every read — `lfs_stat()` is not a durability check, and a read error can
  leave stale bytes in the cache that read back as success).
- **The Class B classification is conditional on the caller**, not on this
  component — `NP-SOUP-LFS-001` §6.2 and `REQ-LFS-01`. Read it before quoting
  the class. §11.4 finds the same thing about the anomaly profile.

## Why vendored (not a submodule)

`NP-SW-CI-001` §9: vendor SDKs in-tree in all cases, never fetch at build time.
A build that reaches the network is not reproducible, and the exact bytes that
go into a release have to be part of the design record. Same pattern as
`firmware/vendor/freertos/`, `firmware/vendor/mcux_sdk/` and
`firmware/crypto/vendor/monocypher/`.

## Layout

```
vendor/littlefs/
├── VERSION                SOUP record — tag, per-file SHA-256, configuration,
│                          and what this vendoring does NOT establish
├── LICENSE.md             BSD-3-Clause (upstream)
├── README-NEURONE.md      this file
├── CMakeLists.txt         np_littlefs (cross + host).  Both host suites —
│                          np_lfs_config_tests and np_lfs_powerloss_tests — are
│                          declared in firmware/hub_control, where the
│                          configuration and the instance live
├── lfs.c  lfs.h           the filesystem and its public API
└── lfs_util.c  lfs_util.h utilities; lfs_util.h is the configuration seam
```

## The configuration is **not** here

Exactly as with FreeRTOS, the configuration is first-party code owned by the hub
control program, so there is one source of truth for it:

- `firmware/hub_control/include/np_lfs_config.h` — the build-time configuration
  (`LFS_NO_MALLOC`, `LFS_THREADSAFE`, retargeted `LFS_ASSERT`, logging off),
  reached through the single compile definition `-DLFS_DEFINES=np_lfs_config.h`.
- `firmware/hub_control/include/np_lfs_instance.h` + `src/np_lfs_instance.c` —
  the `EMMC-FS-01` Config-partition instance parameters and
  `np_lfs_config_validate()`, the mount-time check that `NP-SOUP-LFS-001` §7.3
  asks for.
- `firmware/hub_control/tests/np_lfs_config_tests.c` — asserts both against the
  `VERSION` record, so a configuration change that contradicts the SOUP record
  fails a test rather than a review.
- `firmware/hub_control/tests/np_lfs_powerloss_tests.c` + `np_lfs_powerbd.c` —
  the `OI-LFS-02` power-loss sweep and NeurOne's own injecting block device.
  Upstream's `bd/lfs_emubd.*` does the same job and is deliberately not
  vendored: see `VERSION`.

**One compile definition, not six.** `LFS_THREADSAFE` changes the layout of
`struct lfs_config`, so a translation unit that saw a different set of defines
from the one `lfs.c` was built with would disagree about a struct rather than
fail to link. Putting every define inside the `LFS_DEFINES` header, and exporting
that one definition `PUBLIC` from the `np_littlefs` target, is what makes that
impossible rather than unlikely.

## Building

Cross (part of the firmware super-project):

```
cmake -B build/firmware -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi.cmake
cmake --build build/firmware --target np_littlefs
```

Host tests (no ARM toolchain):

```
cmake -B build/host-tests -DNP_BUILD_TESTS=ON
cmake --build build/host-tests
ctest --test-dir build/host-tests -R 'np_lfs_' --output-on-failure
```

That selects both suites: `np_lfs_config_tests` (SOUP hashes, build
configuration, `EMMC-FS-01` instance, 19 rejected perturbations) and
`np_lfs_powerloss_tests` (claims `L-1…L-4`; ~1.3 s, 456 interrupted runs plus
the falsification sweeps).

Vendored SOUP is built with `-Wall -Wextra` but **not** `-Werror` — the same rule
the FreeRTOS and MCUX targets follow. Third-party sources are not first-party
code, and a warning in them is not a reason a NeurOne build cannot be produced.
