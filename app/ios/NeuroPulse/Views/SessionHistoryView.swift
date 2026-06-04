import SwiftUI

// Session History screen — displays past session records stored in the UHDR partition.
// Includes AdaptiveAdjustmentsCard per session (NP-APP-ROADMAP-001 Rev B §9.4).

struct SessionHistoryView: View {

    var body: some View {
        NavigationStack {
            List {
                Section {
                    Text("No sessions recorded yet.")
                        .foregroundColor(.secondary)
                        .font(.body)
                }
            }
            .navigationTitle("Session History")
        }
    }
}

#Preview {
    SessionHistoryView()
}
