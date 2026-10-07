import SwiftUI

/// The acknowledgement screen for a protocol in the zone model's caution zone (docs/reference/safety-zones.md).
///
/// A caution is not a refusal: the compiler runs the protocol only when the request lists the id of every caution it
/// is in. Each caution is its own switch, because an id names one dose and an acknowledgement is of that dose, not of
/// the protocol. Nothing is stored: the ids go to `onAcknowledge` for one compile and are dropped (an acknowledgement
/// is a decision about the person, CLAUDE.md §5).
struct CautionAcknowledgementView: View {
    let protocolName: String
    let cautions: [NPZoneCaution]
    let onAcknowledge: ([String]) -> Void
    let onCancel: () -> Void

    @State private var acknowledged: Set<String> = []

    private var allAcknowledged: Bool {
        !cautions.isEmpty && cautions.allSatisfy { acknowledged.contains($0.ackId) }
    }

    var body: some View {
        NavigationStack {
            List {
                Section {
                    Text(String(format: String(localized: "ZONE_ACK_INTRO"), protocolName))
                        .font(.subheadline)
                }

                Section {
                    ForEach(cautions) { caution in
                        Toggle(isOn: binding(for: caution)) {
                            VStack(alignment: .leading, spacing: 4) {
                                Text(heading(for: caution)).font(.headline)
                                Text(dose(for: caution)).font(.subheadline)
                                Text("ZONE_ACK_CHECK_LABEL").font(.caption).foregroundColor(.secondary)
                            }
                        }
                        .accessibilityElement(children: .combine)
                    }
                } footer: {
                    Text(String(format: String(localized: "ZONE_ACK_PROGRESS"),
                                String(acknowledged.count), String(cautions.count)))
                }

                Section {
                    Text("ZONE_ACK_SAFETY_NOTE")
                    Text("ZONE_ACK_REASK_NOTE")
                    Text("ZONE_ACK_LOCAL_NOTE")
                }
                .font(.footnote)
                .foregroundColor(.secondary)
            }
            .navigationTitle(String(localized: "ZONE_ACK_TITLE"))
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .cancellationAction) {
                    Button(String(localized: "ZONE_ACK_CANCEL"), action: onCancel)
                }
                ToolbarItem(placement: .confirmationAction) {
                    Button(String(localized: "ZONE_ACK_CONFIRM")) { onAcknowledge(cautions.map(\.ackId)) }
                        .disabled(!allAcknowledged)
                }
            }
        }
        // Swiping the sheet away is a cancel, not an acknowledgement.
        .interactiveDismissDisabled()
    }

    private func binding(for caution: NPZoneCaution) -> Binding<Bool> {
        Binding(
            get: { acknowledged.contains(caution.ackId) },
            set: { on in
                if on { acknowledged.insert(caution.ackId) } else { acknowledged.remove(caution.ackId) }
            })
    }

    private func heading(for caution: NPZoneCaution) -> String {
        String(format: String(localized: "ZONE_ACK_ITEM_HEADING"),
               caution.modality?.displayName ?? "", NSLocalizedString(caution.axisNameKey, comment: ""))
    }

    private func dose(for caution: NPZoneCaution) -> String {
        String(format: String(localized: "ZONE_ACK_ITEM_DOSE"),
               Self.shown(caution.value), caution.unit, Self.shown(caution.caution))
    }

    /// Rounded for display only; the id keeps the exact dose.
    private static func shown(_ n: Double) -> String {
        let r = (n * 100).rounded() / 100
        return r == r.rounded() ? String(Int(r)) : String(r)
    }
}
