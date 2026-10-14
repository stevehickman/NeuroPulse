# Building and testing NeurOne

A quick start. It lists the commands CI runs, so a green local run predicts a green PR. It does not
replace `docs/np_sw_ci_001.md` (NP-SW-CI-001), which records why the build is shaped this way and
what each gate does and does not prove. Where the two disagree, CI (`.github/workflows/`) wins.

All commands run from the repository root.

## What you need

| For | Tools |
|-----|-------|
| Firmware host tests | `cmake`, `ninja`, `gcc` |
| Firmware cross-builds | Arm GNU toolchain `arm-none-eabi-gcc` 13.2.Rel1 (CI pins it; the toolchain file requires 12.3 or newer) |
| Scripts, gates, web app | [Bun](https://bun.sh) |
| Web app NPPS core | Rust (`rustup`) for the WebAssembly build |
| Python checks | Python 3.11 |

## Firmware (`firmware/`)

The firmware is two safety classes. Class C is the safety MCU (STM32G071). Class B is everything
else (i.MX RT1062). Host tests cover both. They build for your machine, not the target.

### Host tests

```sh
cmake -B build/host-tests -G Ninja \
      -DCMAKE_BUILD_TYPE=Debug -DNP_BUILD_TESTS=ON -DCMAKE_CROSSCOMPILING=OFF \
      firmware
cmake --build build/host-tests
```

Configure `firmware`, not a subdirectory. The test registrations live in the `NP_BUILD_TESTS` branch
of `firmware/CMakeLists.txt`, so pointing cmake at a subdirectory builds binaries and registers zero
tests.

The Class C tests are the ones matching this regex. The same string is in the three workflows.

```sh
NP_SAFETY_TESTS='^np_(safety_spi_proto|charge_monitor|thermal_interlock|impedance_report|fault_latch_privacy|cardiac_interlock|hal_platform|nv_state|tier_identity|tier_identity_nokey|gpio_mgr)_tests$'

# Class B (the safety tests excluded)
ctest --test-dir build/host-tests -E "$NP_SAFETY_TESTS" --output-on-failure --no-tests=error

# Class C
ctest --test-dir build/host-tests -R "$NP_SAFETY_TESTS" --output-on-failure --no-tests=error
```

CI also asserts the number of tests selected (`ctest -N`), because `ctest` exits 0 on a shrunken
subset. If you add a test target, update the count, the partition and the floor in the same change:
`NP_CLASS_B_TEST_COUNT` or `NP_SAFETY_TEST_COUNT` in the workflows, `ci/host-test-partition.txt`
and `ci/host-test-floors.txt`. `bun scripts/check-ci-scope.ts` names whichever you missed.

### Sanitized and coverage-floor legs

CI runs the Class B tests a second time under ASan and UBSan, and a third time under `--coverage`.

```sh
SAN='-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer'
cmake -B build/host-tests-san -G Ninja -DCMAKE_BUILD_TYPE=Debug -DNP_BUILD_TESTS=ON \
      -DCMAKE_CROSSCOMPILING=OFF "-DCMAKE_C_FLAGS=$SAN" \
      "-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address,undefined" firmware
cmake --build build/host-tests-san
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1:abort_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
  ctest --test-dir build/host-tests-san -E "$NP_SAFETY_TESTS" --output-on-failure --no-tests=error

# Executed-line floors (every target must still execute at least its recorded lines)
cmake -B build/host-cov -G Ninja -DCMAKE_BUILD_TYPE=Debug -DNP_BUILD_TESTS=ON \
      -DCMAKE_CROSSCOMPILING=OFF -DCMAKE_C_FLAGS=--coverage -DCMAKE_EXE_LINKER_FLAGS=--coverage firmware
cmake --build build/host-cov
bun scripts/check-host-test-floors.ts build/host-cov --class B     # --class C for the safety MCU
```

### Cross-builds (need `arm-none-eabi-gcc`)

Each is a separate CMake project with its own toolchain file.

```sh
# Bootloader (i.MX RT1062)
cmake -B build/cross-boot -G Ninja -DCMAKE_TOOLCHAIN_FILE=$PWD/firmware/cmake/arm-none-eabi.cmake \
      -DCMAKE_BUILD_TYPE=Release firmware/bootloader
cmake --build build/cross-boot

# Main firmware (i.MX RT1062, links np_application.elf)
cmake -B build/cross -G Ninja -DCMAKE_TOOLCHAIN_FILE=$PWD/firmware/cmake/arm-none-eabi.cmake \
      -DCMAKE_BUILD_TYPE=Release firmware
cmake --build build/cross

# Safety MCU (STM32G071)
cmake -B build/safety-mcu -G Ninja -DCMAKE_TOOLCHAIN_FILE=$PWD/firmware/cmake/stm32g071.cmake \
      -DCMAKE_BUILD_TYPE=Release firmware/safety_mcu
cmake --build build/safety-mcu --target np_safety_mcu_objs   # every Class C TU, -Werror
cmake --build build/safety-mcu                               # the linked image
```

A green cross-build means everything compiled and linked with no unresolved symbols. It does not
mean the image runs (NP-SW-CI-001 §4.8).

## Web app (`app/web/`)

```sh
cd app/web && bun install --frozen-lockfile && cd ../..
bun scripts/sync-locales.ts          # generated locale files; git-ignored, needed before tsc
scripts/build-npps-wasm.sh           # NPPS core as WebAssembly; git-ignored, needed by tests and build
cd app/web
bun run tsc --noEmit
bun run vitest run
bun run build
```

`bun run dev` starts the Vite dev server. Never edit or commit the generated locale files. Strings
live in `locales/<bcp47>.json` (`docs/reference/localization.md`, CLAUDE.md §17).

The shared Rust NPPS core is tested separately:

```sh
cargo test --manifest-path common/Cargo.toml --workspace --locked
```

## Repository gates (`scripts/`)

Most gates run with `bun scripts/<name>.ts`. Each has a `--self-test` that proves it can fail, and CI
runs that first. Some useful ones:

| Gate | Checks |
|------|--------|
| `check-ci-scope.ts` | Workflow relevance lists and the host-test partition agree everywhere they are written |
| `check-section-refs.ts` | Every `§N` citation resolves |
| `check-locale-strings.ts` | Non-firmware code carries keys, not text (CLAUDE.md §17) |
| `req-trace.ts --check` | Every test names the requirement it verifies (CLAUDE.md §21) |
| `check-shared-placement.ts` | Shared files live in `app/NeurOneShared/` or `common/` (CLAUDE.md §20) |
| `check-unjustified-register.ts` | Marked provisional values are in the register (CLAUDE.md §19) |
| `check-doc-filenames.ts` | Document filenames are their serials |

The full list is the `tooling-ci.yml` workflow. `bun install --frozen-lockfile` at the root first.

## How CI decides what to run

Each workflow starts with a cheap `changes` job. It lists the files the change touched, matches
them against a relevance list with `scripts/ci-changed-scope.sh`, and the build jobs run only on a
match. A job that is skipped reports `skipped`, which satisfies a required check.

On a pull request, the diff base is the head of the last earlier run of the same workflow that
passed, falling back to the merge base. A push that touches nothing in scope therefore does not
rebuild what an earlier push already built and passed. Today only `firmware-cross-build.yml` does
this. Run `scripts/ci-changed-scope.sh --self-test` after editing the matcher.

`build-all.yml` has no relevance filter. It is the backstop for drift in the hand-maintained lists.
