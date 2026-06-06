# Analytics Consent Key Split — Design Spec

**Date:** 2026-06-05  
**Addresses:** Code-review findings 1, 2, 3 from NP-PRIV-ANALYSIS-002  
**PR target:** `privacy/analysis-and-repair-20260605`

---

## Problem

A single UserDefaults key (`np.onboarding.consent-accepted`) is doing two unrelated jobs:

| Job | Should it open the analytics gate? |
|---|---|
| User completed the consent onboarding flow (Done) | Yes |
| User pressed Skip on the consent onboarding flow | **No** |
| User re-consented via ConsentDashboardView | Yes — but this path never called `configure()` |
| User withdrew blanket research consent | **No** — analytics and research are independent |

Because one key covers all of these, three bugs follow:
1. **Skip opens the analytics gate** — `commitAndDismiss()` sets the key unconditionally.
2. **Research withdrawal revokes analytics** — `withdrawBlanketConsent()` calls `revokeAnalyticsConsent()`.
3. **Re-consent from dashboard breaks analytics** — `configure()` is only called from `presentNextOnboardingStep()`, not from the sheet dismiss path.

---

## Fix

### Two keys, distinct semantics

| Key | String | Set when | Clears | Controls |
|---|---|---|---|---|
| `np.onboarding.consent-shown` | (existing `@AppStorage` in `NeuroPulseApp`) | View appears | Never | Prevents re-presenting onboarding |
| `np.analytics.consent-granted` | **new** | User taps Done (not Skip) | Explicit analytics opt-out only | `AnalyticsGate.isOpen` |

`np.onboarding.consent-accepted` — **retired**. Neither of the two new keys replaces it exactly; `np.analytics.consent-granted` takes over its role in `AnalyticsGate.isOpen`.

---

## File changes

### `AnalyticsGate.swift`
- Rename constant: `consentAcceptedKey` → `analyticsConsentKey`
- New string value: `"np.analytics.consent-granted"` (was `"np.onboarding.consent-accepted"`)
- `isOpen` reads `analyticsConsentKey` (no logic change, just new key)
- `reset()` clears `analyticsConsentKey`

### `ConsentOnboardingView.swift`
- `commitAndDismiss()` gets parameter `grantAnalyticsConsent: Bool = true`
- Skip toolbar button calls `commitAndDismiss(grantAnalyticsConsent: false)`
- When `grantAnalyticsConsent == true`: write `analyticsConsentKey = true` **and** call `AnalyticsGate.configure()` — fixes finding 3 (re-consent from dashboard restarts SDK)
- When `grantAnalyticsConsent == false`: only `updateResearchConsent(draft)` and `isPresented = false`

### `ConsentStore.swift`
- `withdrawBlanketConsent()` — remove `revokeAnalyticsConsent()` call
- Comment explains: research-blanket consent and analytics consent are independent decisions; a future analytics opt-out toggle will call `revokeAnalyticsConsent()` directly
- `revokeAnalyticsConsent()` stays as public API, now clears `analyticsConsentKey`

### `NeuroPulseApp.swift`
- No logic change
- `AnalyticsGate.configure()` at end of `presentNextOnboardingStep()` is correct as-is: handles returning users where `isOpen == true` on launch; the call is idempotent if SDK is already running

### `AnalyticsGateTests.swift`
- Update `consentKey` local variable from `"np.onboarding.consent-accepted"` to `"np.analytics.consent-granted"`
- Update `testConsentAcceptedKeyMatchesConstant()` to reference `analyticsConsentKey`
- Add `testSkipDoesNotOpenAnalyticsGate()`: set the old key, verify `isOpen == false`
- Add `testWithdrawBlanketConsentDoesNotRevokeAnalytics()`: verify `isOpen` remains true after `withdrawBlanketConsent()`

---

## What does NOT change

- `NeuroPulseApp.consentOnboardingShown` / `np.onboarding.consent-shown` — untouched
- All four research consent layers (L1–L4) — untouched
- `AnalyticsGate.configure()`, `track()` logic — untouched
- `SHDRUploader`, `UHDRKeyManager`, `UHDRBackupScheduler` — untouched

---

## Non-goals (out of scope)

- Dedicated analytics opt-out toggle in Settings — `revokeAnalyticsConsent()` is wired and ready; the toggle UI is a separate task
- Migration of existing devices with `np.onboarding.consent-accepted = true` — pre-beta, no existing users to migrate
