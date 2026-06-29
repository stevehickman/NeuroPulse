import SwiftUI

// Consumable inventory and reminder engine UI.
// Measurement-triggered: session counts come from hub GATT, not user input.
// Blocking reminders prevent session start.

struct ConsumableView: View {

    @EnvironmentObject private var tracker: ConsumableTracker

    var body: some View {
        NavigationStack {
            List {
                if !tracker.blockingReminders.isEmpty {
                    blockingSection
                }
                inventorySection
            }
            .listStyle(.insetGrouped)
            .navigationTitle(String(localized: "TAB_SUPPLIES"))
            .onAppear {
                // Request notification permission here — in context, where the user can see
                // why reminders are useful — rather than at app launch before onboarding.
                // (LOW-2 fix, NP-PRIV-ANALYSIS-003.)
                tracker.requestNotificationPermissionIfNeeded()
            }
        }
    }

    // MARK: - Sections

    private var blockingSection: some View {
        Section {
            ForEach(tracker.blockingReminders) { reminder in
                ConsumableAlertRow(reminder: reminder, tracker: tracker, isBlocking: true)
            }
        } header: {
            Label("CONSUMABLE_SECTION_BLOCKING", systemImage: "exclamationmark.triangle.fill")
                .foregroundColor(.red)
        } footer: {
            Text("CONSUMABLE_SECTION_BLOCKING_FOOTER")
                .font(.caption)
        }
    }

    private var inventorySection: some View {
        Section("CONSUMABLE_SECTION_INVENTORY") {
            ForEach(tracker.inventory.states) { state in
                ConsumableStatusRow(state: state, tracker: tracker)
            }
        }
    }
}

// MARK: - Row components

struct ConsumableStatusRow: View {
    let state: ConsumableState
    let tracker: ConsumableTracker

    var body: some View {
        VStack(alignment: .leading, spacing: 8) {
            HStack {
                Circle()
                    .fill(statusColor)
                    .frame(width: 10, height: 10)
                    .accessibilityHidden(true)
                Text(state.kind.displayName)
                    .font(.subheadline.bold())
                    .fixedSize(horizontal: false, vertical: true)
                Spacer()
                Text(state.kind.packDisplayPrice)
                    .font(.caption)
                    .foregroundColor(.secondary)
            }

            HStack(spacing: 4) {
                ProgressView(value: Double(state.sessionCount), total: Double(state.kind.sessionLimit))
                    .progressViewStyle(.linear)
                    .tint(statusColor)
                Text(String(format: String(localized: "CONSUMABLE_SESSIONS_LEFT_FORMAT"), state.sessionsRemaining))
                    .font(.caption2)
                    .foregroundColor(.secondary)
                    .frame(width: 44, alignment: .trailing)
            }

            if state.isLow {
                HStack(spacing: 8) {
                    if let url = ConsumableReminder(state: state).orderURL {
                        Link(destination: url) {
                            Label("CONSUMABLE_ORDER_NOW", systemImage: "cart")
                                .font(.caption)
                        }
                        .buttonStyle(.bordered)
                        .controlSize(.small)
                    }
                    if state.canSnooze {
                        Button(String(format: String(localized: "CONSUMABLE_SNOOZE_FORMAT"), state.maxSnooze - state.snoozeCount)) {
                            tracker.snooze(consumableIndex: state.kind.rawValue)
                        }
                        .font(.caption)
                        .buttonStyle(.bordered)
                        .controlSize(.small)
                    }
                    Spacer()
                    Button("CONSUMABLE_MARK_REPLACED") {
                        tracker.markReplaced(consumableIndex: state.kind.rawValue)
                    }
                    .font(.caption)
                    .buttonStyle(.borderedProminent)
                    .controlSize(.small)
                }
            }
        }
        .padding(.vertical, 4)
    }

    private var statusColor: Color {
        if state.isExceeded { return .red }
        if state.isLow      { return .orange }
        return .green
    }
}

struct ConsumableAlertRow: View {
    let reminder: ConsumableReminder
    let tracker: ConsumableTracker
    let isBlocking: Bool

    var body: some View {
        VStack(alignment: .leading, spacing: 6) {
            Text(reminder.headline)
                .font(.subheadline.bold())
                .foregroundColor(isBlocking ? .red : .primary)
                .fixedSize(horizontal: false, vertical: true)

            Text(
                String(
                    format: String(localized: "CONSUMABLE_SESSIONS_USED_FORMAT"),
                    reminder.state.sessionCount,
                    reminder.state.kind.sessionLimit
                )
            )
                .font(.caption)
                .foregroundColor(.secondary)
                .fixedSize(horizontal: false, vertical: true)

            HStack {
                if let url = reminder.orderURL {
                    Link(destination: url) {
                        Label(
                            String(
                                format: String(localized: "CONSUMABLE_ORDER_PACK_FORMAT"),
                                reminder.state.kind.packDisplayPrice
                            ),
                            systemImage: "cart.fill"
                        )
                        .font(.caption)
                    }
                    .buttonStyle(.borderedProminent)
                    .controlSize(.small)
                }
                Spacer()
                Button("CONSUMABLE_MARK_REPLACED") {
                    tracker.markReplaced(consumableIndex: reminder.state.kind.rawValue)
                }
                .buttonStyle(.bordered)
                .controlSize(.small)
            }
        }
    }
}
