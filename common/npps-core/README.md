# neurone-npps-core — one NPPS implementation for every runtime

**Open item:** `OI-NPPS-CORE-01` (`docs/status/pending-decisions.md`). **Status:** v0; Android's, iOS's, Windows' and the web's compilers run on it.

NPPS had five hand-written parsers and four hand-written hub-descriptor compilers (web, simulator bundle,
iOS, Android, Windows). Each port drifted: the iOS tDCS parser read one field of four, iOS and Android read
`protocol` where the spec says `tms_protocol`, defaults differed per platform, and Windows had no parser at
all. The same `.npps` file must mean the same thing everywhere, so the meaning is written once, here.

## What is here

| Piece | File | State |
|---|---|---|
| Field table: every modality's fields, spellings, kinds, defaults | `../npps/fields.json` | v1; `common/lib/nppsFieldTable.test.ts` fails if `defaultParams()` or the parser drifts from it |
| Lexer | `src/lexer.rs` | port of `tokenize()` |
| Parser, `protocol` entries | `src/parser.rs` | port; modality knowledge is read from the table, not written in code |
| PBM wavelength rules | `src/wavelength.rs` | default rules only |
| Hub-descriptor compiler (NP-FW-HUB-001 §4) | `src/compiler.rs` | all 15 encoders, interval expansion, PBM tile merge |

## How it is verified

Not by reading it. `tests/differential.rs` and `tests/compiler.rs` diff the core against the web parser and
compiler, which remain the reference:

- `app/NeurOneShared/TestData/npps-parse-golden.json` (`bun scripts/gen-npps-parse-golden.ts`): the web
  parser's output for all 76 shipped `.npps` files and the shared fixtures, and 38 corpus cases covering the
  refusals and the alias, default and CW rules. Every entry and every **message** must match.
- `app/NeurOneShared/TestData/hub-descriptor-cases.json` (`bun scripts/gen-hub-descriptor-golden.ts`): the 22
  golden definitions, their zone namespace, and 16 refusals with the web compiler's message. Every byte and
  every message must match `hub-descriptor-golden.json`, which the Android, iOS and Windows tests already
  diff.

`web-ci.yml` regenerates both goldens, fails if either is stale, and runs `cargo test --locked`.

## Android binding

`../npps-jni` is a `cdylib` of two JNI functions over `src/api.rs` (`parse_json`, `compile_json`: the one
contract every binding wraps). `app/android/core/.../npps/NppsCore.kt` loads it and marshals; a refusal is
thrown as `IllegalArgumentException` carrying the core's message. `NppsCoreTests` pushes the shipped
library, the corpus and the 22 golden descriptors through the real native library from Kotlin (the messages
contain `—` and `²`, so UTF-8 marshalling is exercised) and requires every entry, byte and message to match
the web reference. `:core:test` builds the host library first (`cargo` must be on `PATH`; CI installs it).

**Android now compiles through the core.** `HubDescriptorCompiler` maps the Android models to the core's
shape (`NppsCoreMapping.kt`), calls `NppsCore.compile`, and signs the result; the Kotlin encoders are
deleted, so Android has no compiler of its own to drift. Its golden and refusal tests pass through the core.
`:app` packages the library for `arm64-v8a`, `armeabi-v7a`, `x86_64` and `x86` with `cargo-ndk`
(`buildNppsCoreNative`), and `android-ci.yml` fails if the APK lacks it. **That packaging could only be
written, not run, in the authoring environment (no NDK): CI is its first execution.** Not done: Android's
parser (`NPPSParser`) still reads `.npps` itself.

## iOS binding (and the C ABI Windows will share)

`../npps-ffi` is a C ABI over `src/api.rs`: `npps_parse_json`, `npps_compile_json`, `npps_free`, declared
in `include/neurone_npps.h` (hand-maintained, with a `module.modulemap` for Swift). `tests/ffi.rs` runs the
shipped library, the corpus and the 22 golden descriptors through those functions, and
`tests/c/run.sh` compiles a real C program against the header and the static library; both run in
`web-ci.yml`. For iOS, `scripts/build-npps-xcframework.sh` builds a static XCFramework (device arm64;
simulator arm64 + x86_64) into `app/ios/Frameworks/` (git-ignored); `app/ios/project.yml`'s `preGenCommand`
runs it before XcodeGen reads the tree and the app links it. `NppsCore.swift` marshals; `NppsCoreTests`
pushes the same goldens through it. **The XCFramework build, the Xcode linking and the Swift code could
only be written, not run, in the authoring environment (no Xcode, no Apple Rust targets): iOS CI is their
first execution.** **iOS now compiles through the core:** `HubDescriptorCompiler.swift` maps the iOS models to the core's shape
(`NppsCoreMapping.swift`), calls `NppsCore.compile` and signs; the Swift encoders are deleted. `NPPSParser` is
still the Swift port. Because the app's compiler now depends on the XCFramework, a build without it fails to link.

## Not yet in v0

- `composite`, `limits`, `zone`, `condition` and `wavelength_rules` blocks are skipped, not validated; the
  namespace (zone and condition resolution) and the serializer are not ported. The compiler takes the
  zone map as an argument.
- User wavelength rules are accepted by the compiler API but only the default rules are exercised.
- Bindings: Android (`../npps-jni`) and the C ABI (`../npps-ffi`, below) that iOS uses. The Windows
  P/Invoke wrapper over the same C ABI and web and the simulator (`wasm32`) are not written.
  **iOS and Windows cannot be built or tested from this environment** (their CI is the first execution). Android's,
  iOS's and Windows' compilers call their bindings, and the web's runs the same C ABI as WebAssembly
  (`scripts/build-npps-wasm.sh`, `app/web/src/lib/nppsCore.ts`); `hubCompiler.ts` is now a wrapper, so the web
  compiler is no longer an independent reference, and the wire layout is held by `scripts/check-hub-wire-format.ts`
  reading `compiler.rs` and decoding the core's output at the firmware's offsets.
- The validator (`protocolValidator`, per-platform today) is not ported.
- **Signing stays in each platform's keystore.** The compiler returns the blob with a zeroed 64-byte
  signature slot; the caller signs the raw region and fills it.

## Behaviour carried over unchanged, and why

The port is faithful, quirks included, so the differential test can pass and a later change is a visible
diff. Recorded under `OI-NPPS-CORE-01`: an unknown field is dropped silently; a field of the wrong type
falls back to its default; `zones: clinician_selected` keeps the default `zoneRefs`.

## Running it

```
cargo test --manifest-path common/npps-core/Cargo.toml --locked
```
