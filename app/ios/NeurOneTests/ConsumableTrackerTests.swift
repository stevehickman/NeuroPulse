//
//  ConsumableTrackerTests.swift
//  NeurOneTests
//
//  Satisfies: ISC-154 (consumable reminder engine: threshold detection, safety-blocking gate,
//             priority-scoped snooze limits, tracker protocol injection seam, reactive updates).
//
//  Two test sections:
//  1. ConsumableInventoryTests — pure model logic (ConsumableState / ConsumableInventory / ConsumableKind).
//     These exercise the decision logic directly and remain valid regardless of tracker refactors.
//  2. ConsumableTrackerDirectTests — tests ConsumableTracker itself via the ConsumableCountsProviding
//     protocol injection seam. A MockCountsProvider (CurrentValueSubject-backed) eliminates the
//     NeurOneGATTManager dependency so the tracker's sessionIsBlocked gate, snooze propagation,
//     markReplaced propagation, and reactive count updates can all be covered.

import XCTest
import Combine
@testable import NeurOne

// MARK: - MockCountsProvider

/// Injects deterministic consumable counts into ConsumableTracker without a real GATT connection.
/// CurrentValueSubject delivers the initial value synchronously on subscription, so tracker state
/// is already fully computed by the time ConsumableTracker.init(_:) returns.
private final class MockCountsProvider: ConsumableCountsProviding {

    private let subject: CurrentValueSubject<[UInt16], Never>

    var consumableCountsPublisher: AnyPublisher<[UInt16], Never> {
        subject.eraseToAnyPublisher()
    }

    init(counts: [UInt16] = [0, 0, 0, 0]) {
        subject = CurrentValueSubject(counts)
    }

    func push(_ counts: [UInt16]) {
        subject.send(counts)
    }
}

// MARK: - Pure model tests (ISC-154, threshold / snooze logic)

final class ConsumableInventoryTests: XCTestCase {

    private func inventory(intranasal: UInt16 = 0, hydrogel: UInt16 = 0,
                           vns: UInt16 = 0, audio: UInt16 = 0) -> ConsumableInventory {
        var inv = ConsumableInventory()
        inv.update(fromGATTCounts: [intranasal, hydrogel, vns, audio])
        return inv
    }

    // MARK: Threshold detection

    func testNoReminderBelowThreshold() {
        // Hydrogel: limit 45, lowThreshold 8 → 30 used = 15 remaining → not low.
        // VNS:      limit 30, lowThreshold 4 → 10 used = 20 remaining → not low.
        // Audio:    limit 150, lowThreshold 20 → 50 used = 100 remaining → not low.
        let inv = inventory(hydrogel: 30, vns: 10, audio: 50)

        XCTAssertFalse(inv.states[ConsumableKind.electrodeHydrogel.rawValue].isLow)
        XCTAssertFalse(inv.states[ConsumableKind.vnsPads.rawValue].isLow)
        XCTAssertFalse(inv.states[ConsumableKind.audioCupFoam.rawValue].isLow)
        XCTAssertTrue(inv.blockingReminders.isEmpty,
                      "No blocking reminder expected when all counts are below threshold.")
    }

    func testBlockingReminderWhenIntranasalExceeded() {
        // Intranasal: single-use (limit 1), safetyBlocking.
        // A count of 1 means the sleeve has been used → isExceeded → session must be blocked.
        let inv = inventory(intranasal: 1)

        let sleeve = inv.states[ConsumableKind.intranasalSleeves.rawValue]
        XCTAssertTrue(sleeve.isExceeded, "count == limit must read as exceeded")
        XCTAssertEqual(sleeve.kind.reminderPriority, .safetyBlocking)
        XCTAssertFalse(inv.blockingReminders.isEmpty,
                       "Exceeded safety-blocking consumable must produce a blocking reminder.")
    }

    func testActiveRemindersReturnLowStates() {
        // Hydrogel: 44 used out of 45 limit → sessionsRemaining = 1 ≤ lowThreshold 8 → isLow.
        let inv = inventory(hydrogel: 44)
        XCTAssertTrue(inv.states[ConsumableKind.electrodeHydrogel.rawValue].isLow)
        XCTAssertTrue(inv.activeReminders.contains { $0.kind == .electrodeHydrogel })
    }

    // MARK: Snooze limits

    func testSnoozeLimit_performanceCritical() {
        // Hydrogel is performanceCritical → max 3 snoozes.
        var s = ConsumableState(kind: .electrodeHydrogel, sessionCount: 44)
        XCTAssertTrue(s.isLow)
        XCTAssertEqual(s.maxSnooze, 3)

        var applied = 0
        for _ in 0..<10 where s.canSnooze {
            s.snooze()
            applied += 1
        }
        XCTAssertEqual(applied, 3, "performanceCritical must allow at most 3 snoozes.")
        XCTAssertFalse(s.canSnooze, "4th snooze must be refused.")
    }

    func testSnoozeLimit_comfort() {
        // audioCupFoam is comfortLongevity → max 5 snoozes.
        var s = ConsumableState(kind: .audioCupFoam, sessionCount: 149)
        XCTAssertTrue(s.isLow)
        XCTAssertEqual(s.maxSnooze, 5)

        var applied = 0
        for _ in 0..<10 where s.canSnooze {
            s.snooze()
            applied += 1
        }
        XCTAssertEqual(applied, 5, "comfortLongevity must allow at most 5 snoozes.")
        XCTAssertFalse(s.canSnooze, "6th snooze must be refused.")
    }

    func testSafetyBlockingCannotBeSnooze_model() {
        var sleeve = ConsumableState(kind: .intranasalSleeves, sessionCount: 1)
        XCTAssertFalse(sleeve.canSnooze, "safetyBlocking must never be snoozeable.")
        sleeve.snooze()
        XCTAssertEqual(sleeve.snoozeCount, 0, "Snooze on safetyBlocking must be a no-op.")
    }

    func testFourthSnoozeAttemptIsNoOp_performanceCritical() {
        var s = ConsumableState(kind: .electrodeHydrogel, sessionCount: 44)
        s.snooze(); s.snooze(); s.snooze()
        XCTAssertEqual(s.snoozeCount, 3)
        s.snooze()  // 4th — must be a no-op
        XCTAssertEqual(s.snoozeCount, 3, "4th snooze must not increment the count.")
    }
}

// MARK: - ConsumableTracker direct tests (ISC-154, tracker layer)

/// Tests ConsumableTracker itself via MockCountsProvider injection.
/// ConsumableTracker is @MainActor so this test class must be @MainActor too.
///
/// Each test uses a fresh, isolated UserDefaults suite (UUID-named) to prevent
/// snooze persistence from leaking between tests. The suite is removed in tearDown.
@MainActor
final class ConsumableTrackerDirectTests: XCTestCase {

    private var testSuiteName: String!
    private var testDefaults: UserDefaults!

    override func setUp() {
        super.setUp()
        testSuiteName = "com.neurone.test.\(UUID().uuidString)"
        testDefaults = UserDefaults(suiteName: testSuiteName)!
    }

    override func tearDown() {
        testDefaults.removePersistentDomain(forName: testSuiteName)
        testDefaults = nil
        testSuiteName = nil
        super.tearDown()
    }

    private func makeTracker(counts: [UInt16] = [0, 0, 0, 0]) -> ConsumableTracker {
        ConsumableTracker(countsProvider: MockCountsProvider(counts: counts), defaults: testDefaults)
    }

    // MARK: sessionIsBlocked gate

    func testSessionNotBlockedAtZeroCounts() {
        let tracker = makeTracker()
        XCTAssertFalse(tracker.sessionIsBlocked,
                       "No consumable is exceeded at zero counts — session must not be blocked.")
        XCTAssertNil(tracker.sessionBlockReason)
    }

    func testSessionBlockedWhenIntranasalExceeded() {
        let tracker = ConsumableTracker(countsProvider: MockCountsProvider(counts: [1, 0, 0, 0]), defaults: testDefaults)
        XCTAssertTrue(tracker.sessionIsBlocked,
                      "Intranasal at session limit must block session start.")
        XCTAssertNotNil(tracker.sessionBlockReason)
    }

    func testSessionNotBlockedWhenOnlyHydrogelLow() {
        // Hydrogel is performanceCritical, not safetyBlocking — it does not block sessions.
        let tracker = ConsumableTracker(countsProvider: MockCountsProvider(counts: [0, 44, 0, 0]), defaults: testDefaults)
        XCTAssertFalse(tracker.sessionIsBlocked,
                       "performanceCritical consumable being low must not block session start.")
    }

    // MARK: Active and blocking reminders

    func testActiveRemindersEmptyAtZeroCounts() {
        let tracker = makeTracker()
        XCTAssertTrue(tracker.activeReminders.isEmpty)
        XCTAssertTrue(tracker.blockingReminders.isEmpty)
    }

    func testActiveRemindersPopulatedWhenHydrogelLow() {
        let tracker = ConsumableTracker(countsProvider: MockCountsProvider(counts: [0, 44, 0, 0]), defaults: testDefaults)
        XCTAssertFalse(tracker.activeReminders.isEmpty,
                       "Hydrogel at 44/45 must produce an active reminder.")
        XCTAssertEqual(tracker.activeReminders.first?.state.kind, .electrodeHydrogel)
    }

    func testBlockingReminderPresentWhenIntranasalExceeded() {
        let tracker = ConsumableTracker(countsProvider: MockCountsProvider(counts: [1, 0, 0, 0]), defaults: testDefaults)
        XCTAssertFalse(tracker.blockingReminders.isEmpty,
                       "Exceeded intranasal sleeve must appear in blockingReminders.")
        XCTAssertEqual(tracker.blockingReminders.first?.state.kind, .intranasalSleeves)
    }

    // MARK: Snooze forwarded through tracker

    func testSnoozeIncrements() {
        let tracker = ConsumableTracker(countsProvider: MockCountsProvider(counts: [0, 44, 0, 0]), defaults: testDefaults)
        let idx = ConsumableKind.electrodeHydrogel.rawValue
        tracker.snooze(consumableIndex: idx)
        XCTAssertEqual(tracker.inventory.states[idx].snoozeCount, 1)
    }

    func testSnoozeCapRespectedByTracker_performanceCritical() {
        let tracker = ConsumableTracker(countsProvider: MockCountsProvider(counts: [0, 44, 0, 0]), defaults: testDefaults)
        let idx = ConsumableKind.electrodeHydrogel.rawValue
        tracker.snooze(consumableIndex: idx)
        tracker.snooze(consumableIndex: idx)
        tracker.snooze(consumableIndex: idx)
        tracker.snooze(consumableIndex: idx)  // 4th — must be no-op
        XCTAssertEqual(tracker.inventory.states[idx].snoozeCount, 3,
                       "Tracker must cap snooze at 3 for performanceCritical.")
    }

    func testTrackerSafetyBlockingCannotBeSnooze() {
        let tracker = ConsumableTracker(countsProvider: MockCountsProvider(counts: [1, 0, 0, 0]), defaults: testDefaults)
        let idx = ConsumableKind.intranasalSleeves.rawValue
        tracker.snooze(consumableIndex: idx)
        XCTAssertEqual(tracker.inventory.states[idx].snoozeCount, 0,
                       "Snooze on safetyBlocking must be a no-op through the tracker.")
        XCTAssertTrue(tracker.sessionIsBlocked,
                      "Session must remain blocked after snooze attempt on safetyBlocking.")
    }

    // MARK: markReplaced resets through tracker

    func testMarkReplacedResetsSessionCount() {
        let tracker = ConsumableTracker(countsProvider: MockCountsProvider(counts: [0, 44, 0, 0]), defaults: testDefaults)
        let idx = ConsumableKind.electrodeHydrogel.rawValue
        tracker.markReplaced(consumableIndex: idx)
        XCTAssertEqual(tracker.inventory.states[idx].sessionCount, 0)
        XCTAssertFalse(tracker.inventory.states[idx].isLow)
        XCTAssertTrue(tracker.activeReminders.isEmpty)
    }

    func testMarkReplacedAlsoResetsSnoozeCount() {
        let tracker = ConsumableTracker(countsProvider: MockCountsProvider(counts: [0, 44, 0, 0]), defaults: testDefaults)
        let idx = ConsumableKind.electrodeHydrogel.rawValue
        tracker.snooze(consumableIndex: idx)
        tracker.markReplaced(consumableIndex: idx)
        XCTAssertEqual(tracker.inventory.states[idx].snoozeCount, 0)
    }

    // MARK: Reactive update on new counts from provider

    func testInventoryUpdatesWhenProviderPushesNewCounts() {
        let provider = MockCountsProvider()
        let tracker = ConsumableTracker(countsProvider: provider, defaults: testDefaults)

        XCTAssertFalse(tracker.sessionIsBlocked, "Initially clear.")

        provider.push([1, 0, 0, 0])  // Intranasal now exceeded — delivered synchronously

        XCTAssertTrue(tracker.sessionIsBlocked,
                      "Session must be blocked after provider pushes exceeded intranasal count.")
    }

    func testInventoryClears_afterReplacedCountPushed() {
        let provider = MockCountsProvider(counts: [1, 0, 0, 0])
        let tracker = ConsumableTracker(countsProvider: provider, defaults: testDefaults)
        XCTAssertTrue(tracker.sessionIsBlocked)

        // Hub pushes count of 0 (sleeve replaced at hub side) — tracker should react.
        provider.push([0, 0, 0, 0])

        XCTAssertFalse(tracker.sessionIsBlocked,
                       "Session must unblock when provider pushes a cleared intranasal count.")
    }

    // MARK: Out-of-bounds index guard

    func testSnoozeOutOfBoundsIndexIsNoOp() {
        let tracker = makeTracker()
        // Index 99 is well outside the 4-element states array — must not crash.
        tracker.snooze(consumableIndex: 99)
        XCTAssertTrue(tracker.activeReminders.isEmpty)
    }

    func testMarkReplacedOutOfBoundsIndexIsNoOp() {
        let tracker = makeTracker()
        tracker.markReplaced(consumableIndex: 99)
        XCTAssertTrue(tracker.activeReminders.isEmpty)
    }

    // MARK: sessionBlockReason content

    func testSessionBlockReasonMentionsConsumableName() {
        let tracker = ConsumableTracker(countsProvider: MockCountsProvider(counts: [1, 0, 0, 0]), defaults: testDefaults)
        let reason = tracker.sessionBlockReason
        XCTAssertNotNil(reason)
        let name = ConsumableKind.intranasalSleeves.displayName
        XCTAssertTrue(reason?.contains(name) == true,
                      "Block reason must name the specific consumable (got: \(reason ?? "nil"), expected to contain: \(name)).")
    }
}
