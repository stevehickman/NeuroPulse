# Privacy Analysis — Consumable Tracker Feature
**Document:** NP-PRIV-ANALYSIS-003 Rev C  
**Date:** 2026-06-08  
**Scope:** `ConsumableCountsProviding.swift` (new), `ConsumableTracker.swift` (modified), `ConsumableInventory.swift` (modified), `NeuroPulseApp.swift` (modified call site)  
**Jurisdictions in scope:** All (US federal HIPAA/HBNR/FTC Act §5; US state BIPA IL, MHMD WA, CCPA/CPRA CA; EU/EEA GDPR; UK GDPR + DPA 2018; Canada PIPEDA; global equivalents). Consistent with NP-PRIV-001 Rev B and NP-PRIV-ANALYSIS-002.

---

## Privacy Analysis and Repair

### Summary

The consumable tracker refactor introduces no new UHDR data flows and correctly classifies all data elements as SHDR device-wear metrics per CLAUDE.md §5.1. The `ConsumableCountsProviding` protocol abstraction is a genuine privacy-by-design improvement: it structurally limits the tracker to only the 4-integer consumable count field, preventing future accidental access to EEG, HRV, or other UHDR session data. Two LOW findings were identified — both relate to lock-screen notification content and notification permission timing — and both have been resolved in Rev B. One finding surfaced by code review (snooze persistence wipe on launch, a correctness bug with a secondary privacy implication) was fixed before this analysis was written.

**0 Critical / 0 High / 0 Medium / 2 Low findings — all resolved (Rev B, 2026-06-08). GATT routing invariant test suite added (Rev C, 2026-06-08).** Jurisdictions in scope: All.

---

### Findings

#### [LOW — RESOLVED] Lock-screen notification content reveals NeuroPulse device ownership
**Where:** `ConsumableTracker.swift:97–106` (`scheduleLocalNotification(for:)`)  
**Category:** Pure privacy failure  
**Issue:** The notification `title` is `"NeuroPulse: Intranasal Sleeves Low"` and `body` is `"0 sessions remaining. Tap to order — $19 / 30-pack."` Both fields appear on the iOS lock screen without device unlock. A household member or anyone who sees the device screen learns: (1) the user owns a NeuroPulse brain-stimulation wearable, (2) which consumable they're using. For most users this is benign. For users who have not disclosed their brain health device use to family members, the lock screen becomes an involuntary disclosure surface. Under GDPR Art. 9 the "relating to health" test can encompass data from which health-adjacent inferences are drawn. Under Washington MHMD (RCW 70.372), behavioral data derived from a health device may be covered.

**Reference:** Privacy Pattern "Selective Disclosure"; GDPR Art. 9 (special-category health data); Washington MHMD RCW 70.372.

**Remediation:** Apply two-layer notification content: use a generic lock-screen title (`"NeuroPulse — Action Required"`) that does not name the consumable or reveal device type, and let the full consumable-specific content (`headline`, `notificationBody`) appear only in the expanded notification view (which requires device unlock on most iOS configurations). Implementation:
```swift
content.title = "NeuroPulse"
content.subtitle = "Action required — tap to view"
content.body = reminder.headline            // revealed after unlock
content.userInfo = ["consumableIndex": reminder.state.kind.rawValue]
```
This matches the approach used by period-tracking and HIV medication apps, which show only an untyped prompt on the lock screen.

**Resolution (Rev B):** Implemented in `ConsumableTracker.scheduleLocalNotification(for:)`. `content.title = "NeuroPulse"`, `content.subtitle = "Action required — tap to view"`, `content.body = reminder.headline`. Inline comment documents the privacy intent. Committed 2026-06-08.

---

#### [LOW — RESOLVED] Notification permission requested before consent onboarding completes
**Where:** `ConsumableTracker.swift:23` (`requestNotificationPermission()` in `init()`); `NeuroPulseApp.swift:37` (`ConsumableTracker(countsProvider:)` in `App.init()`)  
**Category:** Pure privacy failure  
**Issue:** `ConsumableTracker.init()` calls `UNUserNotificationCenter.current().requestAuthorization()` during `NeuroPulseApp.init()`, before the `.onAppear` block runs and before the age gate / BIPA / research consent screens appear. iOS notification permission is a system-level dialog; granting it is explicit user consent given to iOS, not to NeuroPulse. However, the dialog surfaces before onboarding is complete and may create a confusing "permission before context" experience. Under GDPR ePrivacy Art. 5(3) interpretation, accessing the device for notification delivery requires prior informed consent — the system dialog provides that consent, but best practice is to request it in context (after explaining why notifications are needed), not at cold launch.

**Reference:** GDPR ePrivacy Art. 5(3); iOS Human Interface Guidelines ("Request permission in context"); NP-APP-TELEMETRY-001 Rev B §5 (SDK init gate precedent).

**Remediation:** Defer `requestNotificationPermission()` to the first time the user views `ConsumableView` (in-context request), where the benefit of notifications is visible. Alternatively, move the permission request to the end of the consent onboarding flow with a short in-app explanation slide: "Allow notifications so NeuroPulse can remind you when consumables need replacement." Remove the call from `ConsumableTracker.init()`. The tracker continues to function correctly without notification permission; the `UNUserNotificationCenter.add()` call silently no-ops for unauthorised requests.

**Resolution (Rev B):** `requestNotificationPermission()` removed from `ConsumableTracker.init()`. Replaced by `requestNotificationPermissionIfNeeded()` (internal call semantics unchanged, method is now `internal` not `private` so the view can call it). `ConsumableView.body` calls `tracker.requestNotificationPermissionIfNeeded()` in `.onAppear`. Inline comments document the intent in both files. Committed 2026-06-08.

---

### What looks good

**UHDR/SHDR boundary is structurally enforced.** `ConsumableCountsProviding` exposes only `consumableSessionCounts: [UInt16]` — it cannot return EEG waveforms, HRV time series, session timestamps, or any UHDR field even if a future developer modifies `NeuroPulseGATTManager`. The protocol acts as a data-minimisation compile-time gate. This is exactly the "User-data confinement" pattern.

**Consumable session counts are correctly SHDR.** The 4 session-count integers represent device wear (how many sessions an electrode tip or foam pad has run) — they are a property of the *device's condition*, not user biology. This is explicitly classified as SHDR in CLAUDE.md §5.1 ("consumable session counts → SHDR"). No re-classification needed.

**`orderURL` is not user-identifying.** `URL(string: "https://neuropulse.com/consumables/\(state.kind.rawValue)")` embeds a static enum rawValue (Int 0–3) — a product category, not a user ID, device ID, or any data derived from user biology. No ISC-9 or privacy concern.

**Snooze counts in `UserDefaults.standard` are low-sensitivity.** Snooze count data (4 integers) represents UI interaction state: how many times the user has dismissed a reminder per consumable type. This data does not reveal health status, usage times, or session frequency — only that the user has snoozed a reminder some number of times. It is appropriate to back up via iCloud (it improves UX on device migration) and does not meet the "user health data" bar that triggers UHDR handling. No misclassification.

**No network calls in tracker scope.** The entire `ConsumableTracker` and `ConsumableInventory` stack makes zero `URLSession` or network calls. All data stays on device: GATT → tracker state → `UserDefaults` → local notifications. No transmission to NeuroPulse or third-party servers.

**Notification `userInfo` uses only `consumableIndex` (Int 0–3).** The notification payload does not include session counts, device IDs, or user-identifying data. A push notification server (if ever introduced) would receive no user health data in this payload.

**Protocol abstraction prevents future scope creep.** The `ConsumableCountsProviding` protocol boundary means any future change that adds new data to `SessionState` cannot accidentally reach `ConsumableTracker` without a deliberate protocol extension. This is proactive privacy architecture (Cavoukian Principle 1).

---

### What you couldn't review

- **Hub-side SHDR classification.** Whether the hub firmware correctly routes `consumableStatus` data to the SHDR partition (not UHDR) is out of scope for this iOS-only review. The CLAUDE.md specification is explicit, but hub firmware conformance should be verified when hub source is reviewed.
- **`NeuroPulseGATTManager.$session.consumableSessionCounts` full upstream path.** This review confirmed the field is used but did not trace its full origin from the GATT byte parser through `SessionState` to confirm no UHDR data is mixed in upstream. If `consumableSessionCounts` were accidentally populated from a UHDR GATT characteristic, the tracker would unknowingly handle UHDR data. Recommend a GATT parser test that asserts `consumableSessionCounts` is populated only from the `CONSUMABLE_STATUS` characteristic UUID.
- **Push notification infrastructure.** The tracker currently schedules only local notifications. If a future version adds push notifications (e.g., cloud-triggered reorder reminders), the notification content and permission scope would need re-analysis.

---

### Recommended next steps

1. ~~**[LOW, 30 min]** Change notification lock-screen title to a generic `"NeuroPulse"` string; move consumable-specific text to `content.body`.~~ **DONE** — `ConsumableTracker.scheduleLocalNotification(for:)`, committed 2026-06-08.

2. ~~**[LOW, 1 hr]** Defer `requestNotificationPermission()` from `ConsumableTracker.init()` to `ConsumableView.onAppear`.~~ **DONE** — `ConsumableView.onAppear` + `requestNotificationPermissionIfNeeded()`, committed 2026-06-08.

3. ~~**[Before first external beta]** Add a `GATTParserTests` test asserting that `GATTParser.parseConsumableStatus` is the only code path that populates `SessionState.consumableSessionCounts` — preventing upstream UHDR data from being routed to this field accidentally.~~ **DONE** — Four tests added in `NeuroPulseTests/GATTParserTests.swift` (MARK: CONSUMABLE_STATUS routing isolation): `testConsumableCountsUnchangedByAllUHDRCharacteristicUpdates`, `testConsumableCountsOnlyUpdatedByParseConsumableStatus`, `testConsumableStatusUUIDDistinctFromAllUHDRCharacteristicUUIDs`, `testWatchBridgeConsumableCountsSourcedFromCorrectKey`. Closes the "what you couldn't review" gap. Committed 2026-06-08.

All findings resolved. All open items resolved (Rev C, 2026-06-08). No remaining blocking items.

---

*All code-level findings in NP-PRIV-ANALYSIS-003 are LOW severity. No Critical, High, or Medium findings. No blocking items for App Store submission or external beta beyond those already tracked in NP-PRIV-REM-001.*
