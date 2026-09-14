# Vendored littlefs — NeurOne integration notes

littlefs **v2.11.3**, the storage filesystem for the **SW-02 main processor**
(NXP i.MX RT1062, Cortex-M7 @ 600 MHz). IEC 62304 Class B SOUP — the formal
record is `VERSION`, NP-SW-001 §9.4, and the hazard analysis
`docs/np_soup_lfs_001.md` (NP-SOUP-LFS-001 Rev 2).

## Read this before relying on anything here

This directory closes **OI-LFS-01** (pin + vendor) and nothing else.

- **Nothing calls it yet.** The `OI-LOG-05..07` HAL seams in
  `firmware/hub_control/include/np_log_backend.h` are still unimplemented, and
  `np_littlefs` is linked into no image. The library builds and its
  configuration is tested; the filesystem is not mounted anywhere.
- **`OI-LFS-02` is open**: the IEC 62304 §7.1.2 anomaly evaluation against this
  tag, and a NeurOne power-loss injection test against claims `L-1…L-4`
  (falsified in both directions first, `NP-CONV-001` §8). Until it closes, the
  atomicity claims in `NP-FW-NVRAM-001` §4, `EMMC-FS-01` and `NP-FW-HUB-001`
  §6.5 stay **asserted, not verified**. A vendored directory is not a test.
- **The Class B classification is conditional on the caller**, not on this
  component — `NP-SOUP-LFS-001` §6.2 and `REQ-LFS-01`. Read it before quoting
  the class.

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
├── CMakeLists.txt         np_littlefs (cross) + np_lfs_config_tests (host)
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
ctest --test-dir build/host-tests -R np_lfs_config_tests --output-on-failure
```

Vendored SOUP is built with `-Wall -Wextra` but **not** `-Werror` — the same rule
the FreeRTOS and MCUX targets follow. Third-party sources are not first-party
code, and a warning in them is not a reason a NeurOne build cannot be produced.
