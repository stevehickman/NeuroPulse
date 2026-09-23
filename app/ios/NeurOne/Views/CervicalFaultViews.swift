import SwiftUI

/// Explains cervical VNS faults from sessions that ran without the app (NP-SW-FAULTMSG-001 P4).
///
/// Shown when the phone connects and the hub reports faults the wearer has not yet read.  In
/// Mode 3 the device could only blink its red LED; this is where the wearer learns which fault
/// it was and what to do.  Reading it releases nothing on the device — a cardiac cutoff stays
/// latched in the safety MCU until the wearer confirms resuming (CervicalResumeCard).
struct CervicalFaultSummaryView: View {
    let faults: [CervicalFaultRecord]
    let onAcknowledge: () -> Void

    var body: some View {
        VStack(alignment: .leading, spacing: 16) {
            Label("CVNS_FAULT_SUMMARY_TITLE", systemImage: "exclamationmark.triangle.fill")
                .font(.headline)
                .foregroundColor(.orange)
            Text("CVNS_FAULT_SUMMARY_INTRO")
                .font(.subheadline)
                .foregroundColor(.secondary)
                .fixedSize(horizontal: false, vertical: true)
            ScrollView {
                VStack(alignment: .leading, spacing: 12) {
                    ForEach(faults) { fault in
                        HStack(alignment: .top, spacing: 12) {
                            Image(systemName: fault.kind == .heartRateChange
                                  ? "heart.slash.fill" : "exclamationmark.circle")
                                .foregroundColor(fault.kind == .heartRateChange ? .red : .orange)
                                .accessibilityHidden(true)
                            Text(fault.message)
                                .fixedSize(horizontal: false, vertical: true)
                        }
                    }
                }
            }
            Button(action: onAcknowledge) {
                Text("CVNS_FAULT_ACKNOWLEDGE").frame(maxWidth: .infinity)
            }
            .buttonStyle(.borderedProminent)
        }
        .padding(24)
        .presentationDetents([.medium, .large])
        // The wearer must read it: a swipe does not count as acknowledgement.
        .interactiveDismissDisabled()
    }
}

/// Shown while the hub is waiting for the wearer to confirm resuming cervical stimulation after
/// a cardiac cutoff — during a running session, once the 30 s lockout has passed.  Confirming
/// sends the confirmation the hub requires (REQ-CVNS-09); the hub then re-checks the pads, and
/// only then does the safety MCU clear the cutoff.
struct CervicalResumeCard: View {
    let onConfirm: (@escaping (Bool) -> Void) -> Void

    @State private var askConfirm = false
    @State private var resultKey: String?

    var body: some View {
        VStack(alignment: .leading, spacing: 8) {
            Label("CVNS_RESUME_TITLE", systemImage: "heart.fill")
                .font(.headline)
                .foregroundColor(.red)
            Text("CVNS_RESUME_BODY")
                .font(.subheadline)
                .fixedSize(horizontal: false, vertical: true)
            if let key = resultKey {
                Text(String(localized: String.LocalizationValue(stringLiteral: key)))
                    .font(.footnote)
                    .foregroundColor(.secondary)
            }
            Button("CVNS_RESUME_BUTTON") { askConfirm = true }
                .buttonStyle(.bordered)
        }
        .padding()
        .frame(maxWidth: .infinity, alignment: .leading)
        .background(Color.red.opacity(0.08))
        .clipShape(RoundedRectangle(cornerRadius: 12))
        .confirmationDialog("CVNS_RESUME_CONFIRM_TITLE", isPresented: $askConfirm,
                            titleVisibility: .visible) {
            Button("CVNS_RESUME_BUTTON", role: .destructive) {
                onConfirm { accepted in
                    resultKey = accepted ? "CVNS_RESUME_SENT" : "CVNS_RESUME_REJECTED"
                }
            }
            Button("COMMON_CANCEL", role: .cancel) { }
        } message: {
            Text("CVNS_RESUME_CONFIRM_BODY")
        }
    }
}
