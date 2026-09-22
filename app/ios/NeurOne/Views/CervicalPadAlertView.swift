import SwiftUI

/// Tells the wearer which cervical VNS gel pad failed, and where it is (OI-ACC-07).
///
/// Shown after the hub has already refused or stopped stimulation — the safety MCU has acted,
/// and dismissing this changes nothing on the device. The pads sit on the neck module, not in a
/// helmet socket, so the diagram is of the neck: it lights each side the hub reports a failing
/// pad on.
struct CervicalPadAlertView: View {
    let status: CervicalPadStatus
    let onAcknowledge: () -> Void

    var body: some View {
        VStack(spacing: 20) {
            Label("CVNS_PAD_ALERT_TITLE", systemImage: "exclamationmark.triangle.fill")
                .font(.headline)
                .foregroundColor(.orange)

            NeckPadDiagram(failingSides: status.failingSides)

            Text(status.alertMessage ?? "")
                .font(.body)
                .multilineTextAlignment(.center)
                .fixedSize(horizontal: false, vertical: true)

            Button(action: onAcknowledge) {
                Text("COMMON_OK").frame(maxWidth: .infinity)
            }
            .buttonStyle(.borderedProminent)
        }
        .padding(24)
        .presentationDetents([.medium])
    }
}

/// Front view of head and neck, drawn as the wearer sees themself in a mirror: their left
/// side is on the left of the screen, which is the side their hand reaches for. Each side is
/// labelled in words as well, so the diagram never depends on the reader guessing the view.
/// One pad marker per side; a side lights when the hub reports a failing pad there — both
/// pads of a unilateral montage share one side, and one marker stands for them.
struct NeckPadDiagram: View {
    let failingSides: Set<CervicalPadStatus.NeckSide>

    var body: some View {
        VStack(spacing: 6) {
            ZStack {
                Circle()
                    .stroke(Color.secondary, lineWidth: 2)
                    .frame(width: 72, height: 72)
                    .offset(y: -40)
                RoundedRectangle(cornerRadius: 8)
                    .stroke(Color.secondary, lineWidth: 2)
                    .frame(width: 44, height: 56)
                    .offset(y: 22)
                padMarker(.left).offset(x: -26, y: 14)
                padMarker(.right).offset(x: 26, y: 14)
            }
            .frame(width: 160, height: 140)

            HStack {
                sideLabel("CVNS_PAD_SIDE_LEFT", side: .left)
                Spacer()
                sideLabel("CVNS_PAD_SIDE_RIGHT", side: .right)
            }
            .frame(width: 150)

            Text("CVNS_PAD_DIAGRAM_CAPTION")
                .font(.caption2)
                .foregroundColor(.secondary)
        }
        // The message names the side in words; the drawing adds nothing a screen reader needs.
        .accessibilityHidden(true)
    }

    private func padMarker(_ side: CervicalPadStatus.NeckSide) -> some View {
        let failing = failingSides.contains(side)
        return RoundedRectangle(cornerRadius: 4)
            .fill(failing ? Color.red : Color.clear)
            .overlay(RoundedRectangle(cornerRadius: 4)
                .stroke(failing ? Color.red : Color.secondary, lineWidth: 2))
            .frame(width: 14, height: 22)
    }

    private func sideLabel(_ key: LocalizedStringKey, side: CervicalPadStatus.NeckSide) -> some View {
        let failing = failingSides.contains(side)
        return Text(key)
            .font(.caption.weight(failing ? .bold : .regular))
            .foregroundColor(failing ? .red : .secondary)
    }
}
