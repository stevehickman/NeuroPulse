# NeuroPulse Android

Android version of the NeuroPulse iOS app (`app/ios/`). Kotlin + Jetpack
Compose, native only. System of record: [ISA.md](ISA.md).

## Modules

| Module | What | Requires |
|--------|------|----------|
| `core/` | Pure-JVM Kotlin: models, GATT UUIDs/parsers, consent store + engine models, analytics gates (research/warranty), engagement tier, session history (day-coarsened), consumable tracker, NPPS protocol engine, UHDR key derivation core | JDK 17 only |
| `app/`  | Android shell: Compose UI, BLE central + GATT manager, SHDR uploader (Keystore token + SPKI pinning), UHDR key manager (BiometricPrompt + Argon2id), manifest with backup exclusion | Android SDK 35 |

## Build

```bash
# Privacy-critical logic — no Android SDK needed:
gradle :core:test

# Full app (Android Studio or a machine with the Android SDK):
gradle :app:assembleDebug
```

If the default JVM is newer than the Kotlin Gradle plugin supports, point the
daemon at JDK 17 (see the comment in `gradle.properties`).

## Parity contract with iOS

- GATT UUIDs, wire formats, and opcodes are byte-identical to
  `app/ios/NeuroPulse/BLE/GATTCharacteristics.swift` — enforced by
  `ModelsParityTests`.
- All CLAUDE.md privacy invariants carry over: UHDR/SHDR separation, two
  consent subjects (research vs warranty — never share keys), prohibited
  analytics keys, `engagement_tier` coarsening, `sessionDay` timestamp
  coarsening, opaque SHDR device token, `allowBackup=false`.
- Strings will be generated from the canonical JSON i18n pipeline's Android
  XML adapter (extension point specified in the i18n sync script); the
  current `strings.xml` is a bootstrap skeleton.

## Known open items

- `:app:assembleDebug` verification requires an Android SDK machine
  (ISA ISC-8, NP-AND-BUILD-01).
- SPKI pins in `ShdrUploader` are placeholders — derive from production
  fleet-endpoint certificates before deploy.
- `org.signal:argon2` requires an NP-SW-001 SOUP table entry before beta.
- Analytics vendor not selected (NP-PRIV-AUDIT-001 HIGH-1) — `NoOpAnalyticsBackend`
  keeps the gate architecture exercised until the DPA is executed.
