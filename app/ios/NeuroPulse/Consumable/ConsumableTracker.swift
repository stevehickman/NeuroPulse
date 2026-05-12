import Foundation
import Combine
import UserNotifications
import UIKit

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
    private let gatt: NeuroPulseGATTManager

    init(gatt: NeuroPulseGATTManager) {
        self.gatt = gatt
        observeGATT()
        requestNotificationPermission()
        loadPersistedSnooze()
    }

    // MARK: - GATT observation

    private func observeGATT() {
        cancellable = gatt.$session
            .map(\.consumableSessionCounts)
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

    // Returns true if a blocking consumable issue prevents session start.
    var sessionIsBlocked: Bool { !blockingReminders.isEmpty }

    var sessionBlockReason: String? {
        guard let first = blockingReminders.first else { return nil }
        return "\(first.state.kind.displayName) requires replacement before starting a session. This is a single-use hygiene consumable."
    }

    // MARK: - Local notifications

    private func requestNotificationPermission() {
        UNUserNotificationCenter.current().requestAuthorization(options: [.alert, .badge, .sound]) { _, _ in }
    }

    private func scheduleLocalNotification(for reminder: ConsumableReminder) {
        let content = UNMutableNotificationContent()
        content.title = "NeuroPulse: \(reminder.state.kind.displayName) Low"
        content.body = reminder.notificationBody
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
        UserDefaults.standard.set(counts, forKey: snoozeKey)
    }

    private func loadPersistedSnooze() {
        guard let counts = UserDefaults.standard.array(forKey: snoozeKey) as? [Int] else { return }
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

    var orderURL: URL? {
        // Production: deep-link to NeuroPulse store with consumable pre-selected.
        URL(string: "https://neuropulse.com/consumables/\(state.kind.rawValue)")
    }
}

