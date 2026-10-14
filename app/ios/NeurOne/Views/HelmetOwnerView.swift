import SwiftUI

// The warranty owner's settings (SWR-WO-01, SWR-SS-05): the choices that apply to everyone who uses the helmet, as opposed
// to a person's own. The owner may be a clinic, an institution or one person at home, and the app does not track which
// (CLAUDE.md §6.0), so nothing here asks who is using it or holds anything about a person: the offered set is a set of
// protocol families, and the pause limit is a number of minutes. A choice made for one person is in that person's records.
// The screen cannot tell the owner from anyone else holding the phone; a change to the helmet's setting asks the person to
// confirm that it is for everyone who uses the helmet.

struct HelmetOwnerView: View {

    @EnvironmentObject private var library: NPProtocolLibrary
    @EnvironmentObject private var gatt: NeurOneGATTManager
    @Environment(\.dismiss) private var dismiss

    @State private var minutesText = ""
    @State private var invalidMinutes = false
    @State private var pendingSeconds: UInt32?
    // The library is not observable per family, so a bump re-reads it after a switch.
    @State private var refresh = 0

    /// One family of protocols, as the owner sees it: a name, how many versions it has and whether it has been used.
    private struct FamilyRow: Identifiable {
        let id: String
        let first: NPProtocolEntry
        let versions: Int
        let used: Bool
    }

    private var families: [FamilyRow] {
        _ = refresh
        let runnable = library.allProtocols.filter { $0.isRunnable }
        return Dictionary(grouping: runnable, by: { $0.familyKey }).map { key, members in
            FamilyRow(id: key, first: members[0], versions: members.count, used: members.contains { library.isUsed($0) })
        }
        .sorted { $0.first.name.localizedCaseInsensitiveCompare($1.first.name) == .orderedAscending }
    }

    var body: some View {
        NavigationStack {
            Form {
                Section { Text("WARRANTY_OWNER_INTRO").font(.callout) }
                offeredSection
                pauseLimitSection
            }
            .navigationTitle("WARRANTY_OWNER_TITLE")
            .toolbar {
                ToolbarItem(placement: .confirmationAction) { Button("COMMON_OK") { dismiss() } }
            }
            .alert("PAUSE_LIMIT_CONFIRM_TITLE", isPresented: Binding(
                get: { pendingSeconds != nil },
                set: { if !$0 { pendingSeconds = nil } }
            )) {
                Button("PAUSE_LIMIT_SAVE") {
                    if let seconds = pendingSeconds { gatt.writePauseLimit(seconds: seconds) }
                    pendingSeconds = nil
                }
                Button("COMMON_CANCEL", role: .cancel) { pendingSeconds = nil }
            } message: {
                Text("PAUSE_LIMIT_CONFIRM_BODY")
            }
        }
    }

    private var offeredSection: some View {
        Section {
            if families.isEmpty {
                Text("WARRANTY_OWNER_NO_PROTOCOLS")
            }
            ForEach(families) { row in
                Toggle(isOn: Binding(
                    get: { !library.isArchived(row.first) },
                    set: { offered in
                        if offered { library.unarchiveFamily(row.first) } else { library.archiveFamily(row.first) }
                        refresh += 1
                    }
                )) {
                    VStack(alignment: .leading) {
                        Text(row.first.name)
                        let detail = detailText(row)
                        if !detail.isEmpty { Text(detail).font(.caption).foregroundColor(.secondary) }
                    }
                }
            }
        } header: {
            Text("WARRANTY_OWNER_OFFERED_TITLE")
        } footer: {
            Text("WARRANTY_OWNER_OFFERED_BODY")
        }
    }

    private func detailText(_ row: FamilyRow) -> String {
        var parts: [String] = []
        if row.versions > 1 {
            parts.append(String(format: String(localized: "WARRANTY_OWNER_VARIANTS"), String(row.versions)))
        }
        if row.used { parts.append(String(localized: "WARRANTY_OWNER_USED")) }
        return parts.joined(separator: " · ")
    }

    @ViewBuilder
    private var pauseLimitSection: some View {
        Section {
            if gatt.connectionState != .connected {
                Text("PAUSE_LIMIT_NOT_CONNECTED")
            } else if !gatt.pauseLimitPublished {
                Text("PAUSE_LIMIT_NOT_PUBLISHED")
            } else {
                currentLimitText
                Button("PAUSE_LIMIT_READ_AGAIN") { gatt.requestPauseLimit() }
                TextField("PAUSE_LIMIT_INPUT_LABEL", text: $minutesText)
                    .keyboardType(.numberPad)
                    .onChange(of: minutesText) { _, new in
                        minutesText = new.filter(\.isNumber)
                        invalidMinutes = false
                    }
                if invalidMinutes { Text("PAUSE_LIMIT_INVALID").font(.caption).foregroundColor(.red) }
                Button("PAUSE_LIMIT_SAVE") {
                    if let minutes = UInt32(minutesText), let seconds = PauseLimit.seconds(ofMinutes: minutes) {
                        pendingSeconds = seconds
                    } else {
                        invalidMinutes = true
                    }
                }
            }
        } header: {
            Text("PAUSE_LIMIT_TITLE")
        } footer: {
            Text("PAUSE_LIMIT_BODY")
        }
    }

    @ViewBuilder
    private var currentLimitText: some View {
        if let seconds = gatt.pauseLimitSeconds {
            if seconds == PauseLimit.noLimitSeconds {
                Text("PAUSE_LIMIT_NO_LIMIT").font(.headline)
            } else {
                // A limit set from elsewhere may not be a whole number of minutes; the figure is rounded up so it is
                // never shown as shorter than it is.
                Text(String(format: String(localized: "PAUSE_LIMIT_CURRENT"), String((seconds + 59) / 60))).font(.headline)
            }
        }
    }
}
