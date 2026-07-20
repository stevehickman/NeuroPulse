# NPPS Shared Test Fixtures

Cross-platform test fixtures for validating that the iOS (Swift) and web (TypeScript) NPPS parsers produce identical output from identical input.

## Fixture format

Each fixture is a pair of files:

- `<name>.npps` -- the NPPS source input
- `<name>.expected.json` -- the normalized JSON output both parsers must produce

Error fixtures have only a `.npps` file and no `.expected.json`. These inputs are expected to fail parsing.

## Normalized JSON schema

### Single protocol (`"kind": "single"`)

```json
{
  "kind": "single",
  "protocol": {
    "name": "string",
    "id": "uuid-string (only when explicitly set in source)",
    "description": "string",
    "author": "string",
    "version": "string",
    "isReadOnly": false,
    "tags": ["string"],
    "timingMode": { "type": "duration", "seconds": 1200 },
    "modalityCount": 4,
    "modalities": [
      {
        "type": "modality_block_name",
        "enabled": true,
        "params": { "...alias-resolved camelCase keys...": "value" },
        "interval": { "intervalOnSeconds": 0, "intervalOffSeconds": 0 }
      }
    ]
  }
}
```

### Composite protocol (`"kind": "composite"`)

```json
{
  "kind": "composite",
  "protocol": {
    "name": "string",
    "id": "uuid-string",
    "conflictResolution": "merge",
    "layerCount": 2,
    "layers": [
      {
        "name": "string",
        "startSeconds": 0,
        "durationSeconds": 300,
        "intensityScale": 0.5
      }
    ]
  }
}
```

## Field alias conventions

NPPS source fields are snake_case shorthand. The expected JSON uses camelCase resolved names:

| NPPS source field | Expected JSON key |
|---|---|
| `intensity: 80%` | `intensityPercent`: 80 |
| `frequency: 40Hz` | `frequencyHz`: 40 |
| `duty_cycle: 25%` | `dutyCyclePercent`: 25 |
| `binaural_hz: 40Hz` | `binauralBeatsHz`: 40 |
| `carrier_hz: 440Hz` | `carrierHz`: 440 |
| `volume: 70%` | `volumePercent`: 70 |
| `emdr_cadence: 1Hz` | `emdrCadenceHz`: 1 |
| `closed_loop: true` | `closedLoopEnabled`: true |
| `intensity: 0.8mA` | `intensityMA`: 0.8 |
| `ramp: 30s` | `rampSeconds`: 30 |
| `breathing_rate: 6` | `breathingRateBPM`: 6 |
| `hrv_protocol: standalone` | `hrvProtocol`: "standalone" |
| `enable_mode_f: false` | `enableModeF`: false |
| `eeg_adaptive: false` | `eegAdaptive`: false |
| `bone_conduction_pacer: false` | `boneConductionPacer`: false |

## Duration conversion

- `20m` -> `{ "type": "duration", "seconds": 1200 }`
- `30m` -> `{ "type": "duration", "seconds": 1800 }`
- `300s` -> 300 (used in layer start/duration fields)

## Interval fields

- When `interval_on` and `interval_off` are absent, default to `{ "intervalOnSeconds": 0, "intervalOffSeconds": 0 }`.
- `repeat: until_end` is implicit when interval fields are present and is not represented in the JSON output.

## Fixtures

| Fixture | Tests |
|---|---|
| `gamma_focus` | 4 modalities, metadata, field aliases, compound ident (660_808nm), intervals, booleans |
| `all_t1_modalities` | All 8 T1 modalities, mA unit, electrode_pairs nested array, hrv_protocol, breathing_rate |
| `composite` | Composite kind, conflict_resolution, layer blocks, duration conversion (5m=300s) |
| `comments` | Full-line and inline # comments ignored, zero modalities |
| `error_old_format` | **Error fixture** -- old `protocol { name: "X" }` format, expected to fail |
| `compound_idents` | Tri-wavelength compound ident (660_808_1064nm) |
| `hyphenated_tags` | Hyphenated tags (wind-down, all-modalities, deep-sleep) |

## Condition link vectors

`condition_links.json` is a different shape from the parser fixtures above: it
holds behavioural vectors for the condition external-link policy
(NP-COND-LINK-001 §3) rather than parser input/output. It is consumed by **four**
suites, not two:

| Platform | Test |
|---|---|
| Web | `app/web/src/lib/conditionLink.test.ts` |
| Apple | `app/ios/NeurOneTests/ConditionLinkPolicyTests.swift` |
| Android | `app/android/core/src/test/kotlin/com/neurone/core/protocol/ConditionLinkPolicyTests.kt` |
| Windows | `app/windows/NeurOne/Protocol/NPConditionLinkPolicy.cs` (see §3 of the spec for the harness) |

Each vector is `{ name, url, verdict, reason, host? }` where `verdict` is
`allow` or `block` and `reason` is one of the seven values listed in the file's
`_reasons` key. Adding a vector is how the policy changes — all four
implementations must then agree, or the parity tests fail.

## How to consume in tests

### TypeScript (Vitest)

```typescript
import { readFileSync } from "fs";
import { resolve } from "path";

const fixturesDir = resolve(__dirname, "../../npps/fixtures");

function loadFixture(name: string) {
  const input = readFileSync(resolve(fixturesDir, `${name}.npps`), "utf-8");
  const expected = JSON.parse(
    readFileSync(resolve(fixturesDir, `${name}.expected.json`), "utf-8")
  );
  return { input, expected };
}

test("gamma_focus fixture", () => {
  const { input, expected } = loadFixture("gamma_focus");
  const result = parseNPPS(input);
  expect(normalizeResult(result)).toEqual(expected);
});
```

### Swift (XCTest)

```swift
func loadFixture(_ name: String) throws -> (input: String, expected: [String: Any]) {
    let fixturesURL = Bundle.module.url(forResource: "fixtures", withExtension: nil)!
    let npps = try String(contentsOf: fixturesURL.appendingPathComponent("\(name).npps"))
    let json = try Data(contentsOf: fixturesURL.appendingPathComponent("\(name).expected.json"))
    let expected = try JSONSerialization.jsonObject(with: json) as! [String: Any]
    return (npps, expected)
}
```
