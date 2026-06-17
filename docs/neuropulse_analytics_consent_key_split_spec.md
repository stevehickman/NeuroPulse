# Analytics Gate Architecture — Design Spec

**Date:** 2026-06-16 (original 2026-06-05; supersedes stale draft)  
**Addresses:** NP-PRIV-ANALYSIS-002 findings (PR #112) + terminology clarification  
**Status:** IMPLEMENTED — describes the current codebase

---

## Overview

NeuroPulse uses two distinct analytics gates, each controlling a different data flow and each
tied to a different consent subject. The ambiguous term "analytics consent" has been eliminated
and replaced with precise terms throughout all code and docs.

| Gate | Swift type | Consent subject | Data controlled | UserDefaults key |
|------|-----------|----------------|----------------|-----------------|
| Research analytics | `ResearchAnalyticsGate` | Device user (the person wearing the device) | PostHog SDK / app-level event tracking | `np.research.consent-granted` |
| Warranty analytics | `WarrantyAnalyticsGate` | Warranty owner (entity that registered the device — may be a clinic, NOT the user) | SHDR fleet telemetry (`SHDRUploader`) | `np.warranty.consent.granted` |

The two gates are **code-structurally independent**. `ResearchAnalyticsGate` and `WarrantyAnalyticsGate`
have no shared state. `SHDRUploader` has no reference to `ConsentStore` or `ResearchAnalyticsGate`.

---

## ResearchAnalyticsGate

**File:** `app/ios/NeuroPulse/Analytics/ResearchAnalyticsGate.swift`

Controls the PostHog SDK (app-level analytics). The gate opens when the user completes the
research consent onboarding flow and taps Done (not Skip).

```swift
static let researchAnalyticsKey = "np.research.consent-granted"
static var isOpen: Bool { UserDefaults.standard.bool(forKey: researchAnalyticsKey) }
```

### Onboarding flow

- **Done**: `ConsentOnboardingView` writes `researchAnalyticsKey = true` and calls
  `ResearchAnalyticsGate.configure()`.
- **Skip**: key is NOT written; `ResearchAnalyticsGate.isOpen` remains false.

### Blanket research consent withdrawal (L3)

`ConsentStore.withdrawBlanketResearchConsent()` calls `revokeResearchAnalytics()`, which:
1. Removes `researchAnalyticsKey` from UserDefaults (closes the gate).
2. Calls `ResearchAnalyticsGate.reset()` (tears down the PostHog SDK).

This is **by design** — blanket research withdrawal signals the user does not want any
data collection beyond basic device function. The SDK is torn down, not just gated.

### Partial research withdrawal

`setCategoryConsent()` and per-study withdrawal do NOT call `revokeResearchAnalytics()`.
The gate stays open; only data flows for the withdrawn category/study are blocked.

---

## WarrantyAnalyticsGate

**File:** `app/ios/NeuroPulse/Analytics/WarrantyAnalyticsGate.swift`

Controls SHDR fleet telemetry uploads. The gate opens when the warranty owner grants
consent at device registration.

```swift
static let warrantyConsentKey = "np.warranty.consent.granted"
static var isOpen: Bool { UserDefaults.standard.bool(forKey: warrantyConsentKey) }
```

`SHDRUploader` reads `WarrantyAnalyticsGate.isOpen` to gate uploads. It has no reference
to `ConsentStore` or `ResearchAnalyticsGate`.

**Warranty consent withdrawal has no effect on research analytics.**  
**Research consent withdrawal has no effect on SHDR uploads.**

---

## Key strings (unchanged from PR #112)

| Key | Purpose | Gate |
|-----|---------|------|
| `np.research.consent-granted` | Research analytics consent | `ResearchAnalyticsGate` |
| `np.warranty.consent.granted` | Warranty/SHDR consent | `WarrantyAnalyticsGate` |
| `np.onboarding.consent-shown` | Prevents re-presenting onboarding | (not a gate) |

`np.onboarding.consent-accepted` — retired (replaced by `np.research.consent-granted`).

---

## Files affected by 2026-06-16 terminology change

| File | Change |
|------|--------|
| `Analytics/AnalyticsGate.swift` | Now contains only `typealias AnalyticsGate = ResearchAnalyticsGate` |
| `Analytics/ResearchAnalyticsGate.swift` | New file — authoritative implementation (was `AnalyticsGate`) |
| `Analytics/WarrantyAnalyticsGate.swift` | New file — SHDR upload gate |
| `Consent/ConsentStore.swift` | `withdrawBlanketConsent()` → `withdrawBlanketResearchConsent()`; `revokeAnalyticsConsent()` → `revokeResearchAnalytics()` |
| `Views/ConsentOnboardingView.swift` | `AnalyticsGate` → `ResearchAnalyticsGate` |
| `NeuroPulseApp.swift` | `AnalyticsGate.configure()` → `ResearchAnalyticsGate.configure()` |
| `Data/SHDRUploader.swift` | Removed inline `warrantyConsentGranted` property; now reads `WarrantyAnalyticsGate.isOpen` |
| `NeuroPulseTests/AnalyticsGateTests.swift` | Full rewrite: subject is `ResearchAnalyticsGate`; blanket withdrawal test INVERTED (now correctly asserts gate closes + SDK tears down) |
| `NeuroPulseTests/SHDRUploaderTests.swift` | `setUp`/`tearDown` clear warranty key; `@MainActor` tests call `WarrantyAnalyticsGate.revoke()` |
| `NeuroPulseTests/ConsentStoreTests.swift` | Test names updated to `withdrawBlanketResearchConsent` |
| `app/ios/ISA.md` | Line 159: `withdrawBlanketConsent()` → `withdrawBlanketResearchConsent()` |

---

## Correction to original (2026-06-05) draft

The original draft of this spec incorrectly stated:

> *User withdrew blanket research consent → No — analytics and research are independent*
> *Fix: remove `revokeAnalyticsConsent()` from `withdrawBlanketConsent()`*

This was WRONG. The authoritative design (CLAUDE.md §6.0) is:

> *Blanket research consent withdrawal (L3) additionally revokes app analytics
> (`ResearchAnalyticsGate`); partial research withdrawals do not.*

The regression identified in code review 2026-06-16 restored this coupling.
The test `testWithdrawBlanketConsentDoesNotRevokeAnalytics` (which asserted the wrong behavior)
has been replaced with `testWithdrawBlanketResearchConsentRevokesResearchAnalytics`.
