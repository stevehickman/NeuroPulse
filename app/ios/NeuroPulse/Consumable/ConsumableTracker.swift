import Foundation
import Combine
import UserNotifications

// Measurement-triggered consumable reminder engine.
// Per CLAUDE.md §5.2: all reminders are measurement-triggered, not calendar-triggered.
// Reminder levels: safety-blocking (cannot dismiss), performance-critical (snooze ≤3×), comfort (≤5×).
// Every reminder includes the measured data that triggered it + one-tap order link.

@MainActor
final class ConsumableTracker: ObservableObject {

    @Published private(set) var inventory: ConsumableInventory = ConsumableInventory()
    @Published private(set) var activeReminders: [ConsumableReminder] = []
    @Published private(set) var blockingReminders: [ConsumableReminder] = []

    private var cancellable: AnyCancellable?
    private let countsProvider: ConsumableCountsProviding
    private let defaults: UserDefaults

    init(countsProvider: ConsumableCountsProviding, defaults: UserDefaults = .standard) {
        self.countsProvider = countsProvider
        self.defaults = defaults
        loadPersistedSnooze()   // must come before observeCounts — the publisher delivers its
        observeCounts()          // current value synchronously, which calls persistSnooze();
    }                            // loading first ensures snooze counts survive restart

    // MARK: - Count observation

    private func observeCounts() {
        cancellable = countsProvider.consumableCountsPublisher
            .removeDuplicates()
            .sink { [weak self] counts in
                self?.handleUpdatedCounts(counts)
            }
    }

    private func handleUpdatedCounts(_ counts: [UInt16]) {
        inventory.update(fromGATTCounts: counts)
        persistSnooze()
        recomputeReminders()
    }

    // MARK: - Reminder computation

    private func recomputeReminders() {
        var all: [ConsumableReminder] = []
        var blocking: [ConsumableReminder] = []

        for state in inventory.states where state.isLow {
            let reminder = ConsumableReminder(state: state)
            all.append(reminder)
            if state.kind.reminderPriority == .safetyBlocking && state.isExceeded {
                blocking.append(reminder)
            }
            if !state.canSnooze || state.snoozeCount == 0 {
                scheduleLocalNotification(for: reminder)
            }
        }

        activeReminders = all
        blockingReminders = blocking
    }

    // MARK: - User actions

    func snooze(consumableIndex: Int) {
        guard consumableIndex < inventory.states.count else { return }
        guard inventory.states[consumableIndex].canSnooze else { return }
        inventory.states[consumableIndex].snooze()
        persistSnooze()
        recomputeReminders()
    }

    func markReplaced(consumableIndex: Int) {
        guard consumableIndex < inventory.states.count else { return }
        inventory.states[consumableIndex].resetAfterReplacement()
        persistSnooze()
        recomputeReminders()
    }

    // MARK: - Session start gate

    var sessionIsBlocked: Bool { !blockingReminders.isEmpty }

    var sessionBlockReason: String? {
        guard let first = blockingReminders.first else { return nil }
        return "\(first.state.kind.displayName) requires replacement before starting a session. This is a single-use hygiene consumable."
    }

    // MARK: - Local notifications

    // Called from ConsumableView.onAppear so the dialog appears in context — after the user
    // can see why notifications are useful. Requesting at app init is premature and confusing.
    // (LOW-2 fix, NP-PRIV-ANALYSIS-003: permission deferred to first ConsumableView appearance.)
    func requestNotificationPermissionIfNeeded() {
        UNUserNotificationCenter.current().requestAuthorization(options: [.alert, .badge, .sound]) { _, _ in }
    }

    private func scheduleLocalNotification(for reminder: ConsumableReminder) {
        let content = UNMutableNotificationContent()
        // Generic title on lock screen — does not name the consumable or reveal device type.
        // Specific consumable detail is in body, revealed after device unlock on most configs.
        // (LOW-1 fix, NP-PRIV-ANALYSIS-003: prevents involuntary health-device disclosure.)
        content.title = "NeuroPulse"
        content.subtitle = "Action required — tap to view"
        content.body = reminder.headline
        content.sound = .default
        content.userInfo = ["consumableIndex": reminder.state.kind.rawValue]

        let trigger = UNTimeIntervalNotificationTrigger(timeInterval: 1, repeats: false)
        let id = "consumable-\(reminder.state.kind.rawValue)"
        let request = UNNotificationRequest(identifier: id, content: content, trigger: trigger)
        UNUserNotificationCenter.current().add(request, withCompletionHandler: nil)
    }

    // MARK: - Persistence (snooze counts survive app restart)

    private let snoozeKey = "np.consumable.snooze-counts"

    private func persistSnooze() {
        let counts = inventory.states.map(\.snoozeCount)
        defaults.set(counts, forKey: snoozeKey)
    }

    private func loadPersistedSnooze() {
        guard let counts = defaults.array(forKey: snoozeKey) as? [Int] else { return }
        for (idx, count) in counts.enumerated() where idx < inventory.states.count {
            inventory.states[idx].snoozeCount = count
        }
    }
}

// MARK: - Consumable reminder display model

struct ConsumableReminder: Identifiable {
    var id: Int { state.kind.rawValue }
    let state: ConsumableState

    var headline: String {
        if state.isExceeded { return "\(state.kind.displayName) — Replace Now" }
        return "\(state.kind.displayName) — \(state.sessionsRemaining) sessions remaining"
    }

    var notificationBody: String {
        "\(state.sessionsRemaining) sessions remaining. Tap to order — \(state.kind.packDisplayPrice)."
    }

    var priorityColor: String {
        switch state.kind.reminderPriority {
        case .safetyBlocking:      return "red"
        case .performanceCritical: return "orange"
        case .comfortLongevity:    return "yellow"
        }
    }

    // INFRASTRUCTURE — neuropulse.com
    //
    // Domain   : Primary NeuroPulse customer-facing domain. Register neuropulse.com.
    //
    // Purpose  : Deep-link into the consumables shop when a reminder fires.
    //            Opened in the user's default browser (Safari); no in-app webview.
    //
    // URL map  : /consumables/<Int> where Int = ConsumableKind.rawValue
    //   ConsumableKind is enum ConsumableKind: Int — rawValue is the integer case index:
    //     /consumables/0  →  Intranasal Sleeves    (30-pack, $19)
    //     /consumables/1  →  Electrode Hydrogel Tips (8-pack, $12–16)
    //     /consumables/2  →  VNS Clip Pads         (2-pack, $8)
    //     /consumables/3  →  Audio Cup Foam         (set,    $24)
    //
    //   NOTE: These paths are tied to enum integer positions. Do not reorder
    //   ConsumableKind cases or add cases before existing ones without also
    //   updating the shop URL routing. See docs/neuropulse_infra_001.md §5.
    //
    // Setup    : Ensure each path returns 200 and the correct product page before
    //            TestFlight beta. A 404 here breaks the one-tap reorder flow that
    //            is a primary consumable MRR driver (CLAUDE.md §2.3).
    var orderURL: URL? {
        URL(string: "https://neuropulse.com/consumables/\(state.kind.rawValue)")
    }
}
