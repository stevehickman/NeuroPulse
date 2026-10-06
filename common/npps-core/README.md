# neurone-npps-core — one NPPS implementation for every runtime

**Open item:** `OI-NPPS-CORE-01` (`docs/status/pending-decisions.md`). **Status:** v0; every app's parser, serializer, validator and hub-descriptor compiler (Android, iOS, Windows, web) and the simulator run on it (the Windows and iOS bindings are written and unrun here: no .NET or Xcode).

NPPS had five hand-written parsers and four hand-written hub-descriptor compilers (web, simulator bundle,
iOS, Android, Windows). Each port drifted: the iOS tDCS parser read one field of four, iOS and Android read
`protocol` where the spec says `tms_protocol`, defaults differed per platform, and Windows had no parser at
all. The same `.npps` file must mean the same thing everywhere, so the meaning is written once, here.

## What is here

| Piece | File | State |
|---|---|---|
| Field table: every modality's fields, spellings, kinds, defaults | `../npps/fields.json` | v1; `common/lib/nppsFieldTable.test.ts` fails if `defaultParams()` or the parser drifts from it |
| Lexer | `src/lexer.rs` | port of `tokenize()` |
| Parser: `protocol`, `composite`, `zone`, `condition`, `wavelength_rules`, `limits` blocks | `src/parser.rs`, `src/parser/blocks.rs` | port; modality knowledge is read from the table, not written in code |
| Namespace: fold files, duplicate names, cross-references | `src/api.rs` (`namespace_json`) | port of `buildNamespace` and `validateNamespaceReferences` |
| PBM wavelength rules | `src/wavelength.rs` | default rules only |
| Hub-descriptor compiler (NP-FW-HUB-001 §4) | `src/compiler.rs` | all 15 encoders, interval expansion, PBM tile merge |
| Serializer: models to `.npps` text | `src/serialize.rs` | every block (`protocol`, `composite`, `zone`, `condition`, `wavelength_rules`, `limits`); the inverse of the parser, held to it by a round trip |
| Validator: hardware ceilings, dosage limits, charge density, cross-modality | `src/validate.rs`, `../npps/hardware-limits.json` | the union of the web, iOS and Android checks; returns locale keys and arguments, never text |

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

`../npps-jni` is a `cdylib` of JNI functions over `src/api.rs` (`parse_json`, `namespace_json`, `compile_json`, `serialize_json`,
`validate_json`: the one contract every binding wraps). `app/android/core/.../npps/NppsCore.kt` loads it and marshals; a refusal is
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

## Serializer and validator

`tests/serialize.rs` holds the serializer to `app/NeurOneShared/TestData/npps-serialize-golden.json`, the web
serializer's own output for every block of the shipped library (frozen when it moved to the core), and to the parser
by a round trip: what the core writes, the core reads back unchanged. It differs from the web writer in two places,
both of which were bugs: it writes a duration of whole hours in minutes (the web wrote `1h`, a unit the lexer does not
have, so the file did not load), and it writes a `limits` ceiling under the key the parser reads (the web snake-cased
the property name into `max_irradiance_m_wcm2`, which the parser drops, so a ceiling written by the app vanished on the
next load). A model it cannot write is refused with a message rather than written into a file the parser rejects: a
zone holding an id that is not a socket, a named PBM target with no zone.

`tests/validate.rs` holds the validator to `npps-validate-golden.json`, the web validator's English output over the
library and 560 cases that violate each check, after resolving the core's locale keys against `locales/en.json` the way
each app's `t()` does. **The core returns no text** (CLAUDE.md §17): an issue carries keys and positional arguments, a
number already rendered the way JavaScript renders it, and the app resolves them. The three validators had drifted, so
the core has the union of their checks (`tests/validate_union.rs`): the web one lacked the per-modality session-duration
limits, the PBM session dose, the CW-duty contradiction, the zero-duration error, the TMS above 120 % warning, the
cervical-VNS frequency range and interlock note, the vibrotactile frequency warning, the deep-PBM ceiling and the
zone-resolution check (when the caller gives the namespace); the mobile ones lacked the layer intensity scale and the
TMS-with-electrical-stimulation note. A configured limit is attributed to the level of the resolved set, or to the tier
a per-field `limitSources` map names (iOS's `NPLimitSourceMap`); the web said `global` for all of them.
`common/lib/hardwareLimits.test.ts` fails if the ceilings the validator reads differ from the editors' copies.

## Not yet in v0

- The compiler takes the zone map as an argument, and the apps fill it from the namespace.
- Bindings: Android (`../npps-jni`), the C ABI (`../npps-ffi`) that iOS and Windows link, and the same C ABI
  built to `wasm32` for the web and the simulator (`scripts/build-npps-wasm.sh`, `common/lib/nppsCore.ts`).
  **iOS and Windows cannot be built or tested from this environment** (their CI is the first execution).
  Every app's parser and compiler call their binding. `common/lib/nppsParser.ts` and
  `app/web/src/lib/hubCompiler.ts` are thin wrappers now, so the web parser and compiler are no longer an
  independent reference: the goldens are the core's own output, and the wire layout is held by
  `scripts/check-hub-wire-format.ts` reading `compiler.rs` and decoding the core's output at the firmware's
  offsets.
- The editors' own copies of the hardware ceilings (slider ranges) and the three-tier limit resolution
  (`resolveLimits`) are still per platform.
- **Signing stays in each platform's keystore.** The compiler returns the blob with a zeroed 64-byte
  signature slot; the caller signs the raw region and fills it.

## Where the native parsers differed from the reference

Moving Android and iOS onto the core removed behaviour their own parsers had and the web parser (the reference)
did not: an unknown `limits` sub-block was ignored (now refused: a dropped ceiling is a ceiling never applied),
a condition with no `link` was refused (now parsed with an empty link, which the link policy will not open),
an empty protocol name was refused (now parsed as written), the Android and iOS serializers wrote `0.9G` for
vibrotactile intensity (a form the reference refuses; they write `0.9` now), `helmet_id` no longer sets the limits
level, and the refusal messages are the web parser's. The Android and iOS tests that pinned those were updated.

## Behaviour carried over unchanged, and why

The port is faithful, quirks included, so the differential test can pass and a later change is a visible
diff. Recorded under `OI-NPPS-CORE-01`: an unknown field is dropped silently; a field of the wrong type
falls back to its default; `zones: clinician_selected` keeps the default `zoneRefs`.

## Running it

```
cargo test --manifest-path common/npps-core/Cargo.toml --locked
```
