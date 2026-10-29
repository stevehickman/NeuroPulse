import Foundation

/// What the protocol menu shows and when it closes while a protocol is being sent: the "sent" banner, the error banner, the
/// "different person" question, and whether the screen dismisses or reports a Mode 3 protocol stored. The view only draws
/// these, so the behaviour is tested without rendering one (SWR-UP-02, SWR-HV-08, REQ-UPG-01).
@MainActor
final class ProtocolUploadScreen: ObservableObject {
    @Published var uploadError: String?
    @Published var showUploadSuccess = false
    /// Another person on this device has an outstanding cardiac cutoff: the cervical protocol needs the "different person"
    /// confirmation first.
    @Published var askDifferentPerson = false

    /// The Mode 2 wait for the helmet's report, which runs on its own so leaving the screen does not cancel it.
    private(set) var recording: Task<Void, Never>?

    /// A refusal from the helmet: the "sent" banner was true when it was shown, and this replaces it.
    func say(_ message: String) {
        showUploadSuccess = false
        uploadError = message
    }

    /// Send one protocol. `send` writes it and returns the SHA-256 of the bytes sent. `close` dismisses the screen and
    /// `programmed` tells the setup flow a Mode 3 protocol is stored.
    ///
    /// Mode 3 (`programMode`) closes the screen and reports the protocol stored only if the helmet holds it, so it waits for
    /// the helmet's report: after a refusal, for whatever reason, the message stays on screen and `programmed` is not called.
    /// No answer at all is the unknown outcome, treated as sent. Mode 2 shows "sent" for `bannerNanos`, then closes unless the
    /// helmet refused it in the meantime.
    func run(flow: ProtocolUploadFlow, entry: NPProtocolEntry, programMode: Bool,
             send: @escaping () async throws -> String,
             close: () -> Void, programmed: (() -> Void)?,
             bannerNanos: UInt64 = 2_000_000_000) async {
        do {
            if programMode {
                let stored = try await flow.program(entry: entry, send: send, say: { self.say($0) })
                if !stored { return }
                close()
                programmed?()
            } else {
                let sentHash = try await send()
                recording = Task { @MainActor in await flow.record(of: entry, sentHash: sentHash, say: { self.say($0) }) }
                showUploadSuccess = true
                try? await Task.sleep(nanoseconds: bannerNanos)
                showUploadSuccess = false
                // A refusal from the hub (SWR-HV-08) has put its message up by now: leave it on screen.
                if uploadError == nil { close() }
            }
        } catch UploadError.differentPersonConfirmationRequired {
            guard !Task.isCancelled else { return }
            askDifferentPerson = true
        } catch {
            guard !Task.isCancelled else { return }
            uploadError = error.localizedDescription
        }
    }
}
