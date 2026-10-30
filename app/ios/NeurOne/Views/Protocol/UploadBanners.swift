import SwiftUI

/// The red banner the protocol menu puts at the top when an upload failed or the helmet refused it. It holds no state and
/// decides nothing: the message is what `ProtocolUploadScreen` set (`ProtocolUploadFlow.refusalMessage`), and Retry tells the
/// menu to send the same protocol again. A tier refusal never names a purchase (`NP-PWRSRC-001` §6.3).
struct UploadErrorBanner: View {
    let message: String
    let onRetry: () -> Void

    var body: some View {
        HStack(spacing: 8) {
            Image(systemName: "exclamationmark.triangle.fill")
                .foregroundColor(.white)
                .font(.caption)
            Text(message)
                .font(.caption)
                .foregroundColor(.white)
                .lineLimit(3)
                .fixedSize(horizontal: false, vertical: true)
            Spacer()
            Button(action: onRetry) {
                Text(String(localized: "SETUP_RETRY_BUTTON"))
                    .font(.caption.bold())
                    .foregroundColor(.white)
                    .padding(.horizontal, 10)
                    .padding(.vertical, 4)
                    .background(Color.white.opacity(0.25))
                    .clipShape(Capsule())
            }
        }
        .padding(.horizontal, 12)
        .padding(.vertical, 10)
        .background(Color.red)
        .transition(.move(edge: .top).combined(with: .opacity))
    }
}

/// The green "sent" banner at the bottom of the protocol menu, shown while the screen waits for the helmet's report.
struct UploadSentBanner: View {
    var body: some View {
        HStack(spacing: 8) {
            Image(systemName: "checkmark.circle.fill")
                .foregroundColor(.white)
            Text("PROTOCOL_MENU_PROTOCOL_SENT_TO_HUB")
                .font(.subheadline.weight(.medium))
                .foregroundColor(.white)
        }
        .padding(.horizontal, 16)
        .padding(.vertical, 10)
        .background(Color.green)
        .clipShape(Capsule())
        .padding(.bottom, 24)
        .transition(.move(edge: .bottom).combined(with: .opacity))
    }
}
