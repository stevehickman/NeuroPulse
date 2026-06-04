//
//  ConsumableTrackerTests.swift
//  NeuroPulseTests
//
//  Satisfies: ISC-154 (consumable reminder engine: threshold detection, safety-blocking gate,
//             priority-scoped snooze limits), ISC-157.
//
//  Subject under test: the consumable reminder logic in
//                      app/ios/NeuroPulse/Models/ConsumableInventory.swift
//                      (ConsumableState / ConsumableInventory / ConsumableKind), which is the exact
//                      logic ConsumableTracker drives.
//
//  WHY NOT ConsumableTracker DIRECTLY:
//  ConsumableTracker.init(gatt:) requires a NeuroPulseGATTManager, whose own init() constructs a
//  live CBCentralManager and whose `session` is `private(set)`. There is no seam to inject GATT
//  counts, so the tracker cannot be exercised deterministically in a unit test today. The decision
//  logic the tracker depends on lives entirely in ConsumableInventory/ConsumableState and is fully
//  tested here. Recommended refactor: extract a `ConsumableCountsProviding` protocol (one
//  `@Published var consumableSessionCounts: [UInt16]`) and inject it into ConsumableTracker so the
//  tracker's `sessionIsBlocked` / `snooze` / `recomputeReminders` can be tested against a mock.
//  These tests are written against the underlying model so they remain valid after that refactor.

import XCTest
@testable import NeuroPulse

final class ConsumableTrackerTests: XCTestCase {

    // MARK: - Helpers

    /// Build an inventory and apply hub GATT counts, mirroring ConsumableTracker.handleUpdatedCounts.
    private func inventory(counts: [UInt16]) -> ConsumableInventory {
        var inv = ConsumableInventory()
        inv.update(fromGATTCounts: counts)
        return inv
    }

    private func state(_ inv: ConsumableInventory, _ kind: ConsumableKind) -> ConsumableState {
        inv.states[kind.rawValue]
    }

    // Order: [intranasal, hydrogel, vns, audio]
    private func counts(intranasal: UInt16 = 0, hydrogel: UInt16 = 0,
                        vns: UInt16 = 0, audio: UInt16 = 0) -> [UInt16] {
        [intranasal, hydrogel, vns, audio]
    }

    // MARK: - testNoReminderBelowThreshold

    func testNoReminderBelowThreshold() {
        // Hydrogel: limit 45, lowThreshold 8 → 30 sessions used leaves 15 remaining (> 8): not low.
        // VNS: limit 30, lowThreshold 4 → 10 used leaves 20 remaining: not low.
        // Audio: limit 150, lowThreshold 20 → 50 used leaves 100 remaining: not low.
        // Intranasal: single-use; 0 used → not exceeded, not low.
        let inv = inventory(counts: counts(intranasal: 0, hydrogel: 30, vns: 10, audio: 50))

        XCTAssertFalse(state(inv, .electrodeHydrogel).isLow)
        XCTAssertFalse(state(inv, .vnsPads).isLow)
        XCTAssertFalse(state(inv, .audioCupFoam).isLow)

        // No safety-blocking consumable is exceeded → nothing blocks a session.
        XCTAssertTrue(inv.blockingReminders.isEmpty,
                      "With all counts below threshold, there must be no blocking reminders.")
    }

    // MARK: - testBlockingReminderAtThreshold

    func testBlockingReminderAtThreshold() {
        // Intranasal sleeves are single-use (limit 1) and safetyBlocking.
        // A count of 1 means the sleeve has been used → isExceeded → session blocked.
        let inv = inventory(counts: counts(intranasal: 1))

        let sleeve = state(inv, .intranasalSleeves)
        XCTAssertTrue(sleeve.isExceeded, "A used single-use sleeve must read as exceeded.")
        XCTAssertEqual(sleeve.kind.reminderPriority, .safetyBlocking)

        XCTAssertFalse(inv.blockingReminders.isEmpty,
                       "An exceeded safety-blocking consumable must produce a blocking reminder.")
        // This is the condition ConsumableTracker.sessionIsBlocked surfaces.
        let sessionIsBlocked = !inv.blockingReminders.isEmpty
        XCTAssertTrue(sessionIsBlocked,
                      "sessionIsBlocked must be true when intranasal sleeve is at its session limit.")
    }

    // MARK: - testSnoozeLimit_performance

    func testSnoozeLimit_performance() {
        // Hydrogel is performanceCritical → snooze max 3×.
        var s = ConsumableState(kind: .electrodeHydrogel, sessionCount: 44) // 1 remaining → low
        XCTAssertTrue(s.isLow)
        XCTAssertEqual(s.maxSnooze, 3)

        var applied = 0
        for _ in 0..<10 where s.canSnooze {
            s.snooze()
            applied += 1
        }
        XCTAssertEqual(applied, 3, "A performance-critical reminder must snooze at most 3 times.")
        XCTAssertEqual(s.snoozeCount, 3)
        XCTAssertFalse(s.canSnooze, "A 4th snooze must not be permitted.")
    }

    // MARK: - testSnoozeLimit_comfort

    func testSnoozeLimit_comfort() {
        // Audio cup foam is comfortLongevity → snooze max 5×.
        var s = ConsumableState(kind: .audioCupFoam, sessionCount: 149) // 1 remaining → low
        XCTAssertTrue(s.isLow)
        XCTAssertEqual(s.maxSnooze, 5)

        var applied = 0
        for _ in 0..<10 where s.canSnooze {
            s.snooze()
            applied += 1
        }
        XCTAssertEqual(applied, 5, "A comfort/longevity reminder must snooze at most 5 times.")
        XCTAssertFalse(s.canSnooze, "A 6th snooze must not be permitted.")
    }

    // MARK: - testSessionBlockedPreventsFourthPerformanceSnooze

    func testSessionBlockedPreventsFourthPerformanceSnooze() {
        // Performance-critical (hydrogel): after 3 snoozes the 4th must be refused outright.
        var s = ConsumableState(kind: .electrodeHydrogel, sessionCount: 44)
        s.snooze(); s.snooze(); s.snooze()
        XCTAssertEqual(s.snoozeCount, 3)

        XCTAssertFalse(s.canSnooze, "After 3 snoozes the consumable must report it cannot snooze.")
        s.snooze()  // attempt the 4th — guarded internally, must be a no-op
        XCTAssertEqual(s.snoozeCount, 3,
                       "A 4th snooze attempt must not increment the count beyond the cap.")

        // Safety-blocking consumables can never be snoozed at all.
        var sleeve = ConsumableState(kind: .intranasalSleeves, sessionCount: 1)
        XCTAssertFalse(sleeve.canSnooze, "A safety-blocking consumable must never be snoozeable.")
        sleeve.snooze()
        XCTAssertEqual(sleeve.snoozeCount, 0, "Snoozing a safety-blocking consumable must be a no-op.")
    }
}
