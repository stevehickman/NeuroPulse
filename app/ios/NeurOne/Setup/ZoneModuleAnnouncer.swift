import UIKit

// Speaks zone-module insertion confirmations on THIS device.
//
// ── Why the app says this and not the helmet ─────────────────────────────────
//
// The headset has a bone-conduction transducer at the mastoid, and the original
// design (NP-FW-ZA-001 Rev A §7–8) announced "Frontal Left connected" through it
// the instant a module was seated. That confirmation could never be heard.
// Bone conduction requires the transducer in contact with the mastoid, and
// inserting a module requires the helmet OFF the head. The cue fired into an
// empty room. NP-HFE-002 §2.4 records this as retired at any socket count.
//
// The confirmation therefore belongs on whatever device the user is holding.
// This is the separate work item NP-HFE-002 §7.5 names — "redirecting
// insertion-time confirmation to the companion app" — and §7.5 binds it to
// §7.2's channel and grammar requirements, which is what this file implements.
//
// ── The channel is the platform screen reader. Not our own TTS. ──────────────
//
// NP-HFE-002 §7.2 rejects in-app bespoke TTS explicitly, and the reasoning is
// not stylistic. `AVSpeechSynthesizer` would:
//
//   - discard the user's own speech rate, voice and verbosity settings;
//   - discard their gesture settings; and, decisively,
//   - miss REFRESHABLE BRAILLE DISPLAY ROUTING, which §7.2 identifies as the
//     deaf-blind user's ONLY channel. A braille display is driven by VoiceOver;
//     audio we synthesise ourselves reaches it not at all.
//
// So this posts `UIAccessibility` announcements and nothing else. A sighted user
// with VoiceOver off is served by the socket list updating on screen and by the
// haptic below — not by us talking at them, which is what the retired design did
// in a different way.
//
// ── Grammar: the zone leads, the socket number does not ──────────────────────
//
// §7.2: "Raw socket numbers are never the primary form; they are available on
// demand as a detail." HFE-R-07 tests that no instruction requires a raw socket
// number to be actionable. So the spoken string leads with the zone name from
// `00-zones.npps`; the id stays visible in the UI for anyone who wants it.

@MainActor
protocol ZoneModuleAnnouncing {
    /// Announce a module-insertion confirmation.
    func announce(_ message: String)
    /// Abandon anything still pending (leaving the step, disconnect).
    func cancel()
}

/// Production announcer — platform screen reader plus haptic.
@MainActor
final class ZoneModuleAnnouncer: ZoneModuleAnnouncing {

    private let isVoiceOverRunning: () -> Bool
    private let haptic = UINotificationFeedbackGenerator()

    /// - Parameter isVoiceOverRunning: injectable so tests can drive both
    ///   branches; defaults to the live UIKit query.
    init(isVoiceOverRunning: @escaping () -> Bool = { UIAccessibility.isVoiceOverRunning }) {
        self.isVoiceOverRunning = isVoiceOverRunning
    }

    func announce(_ message: String) {
        guard !message.isEmpty else { return }

        // Haptic first, and unconditionally: §7.2 requires a non-audio,
        // non-visual acknowledgement of an accepted placement, which is the only
        // confirmation a deaf-blind user gets in real time while their hands are
        // on the helmet rather than on a braille display.
        haptic.notificationOccurred(.success)

        // The screen reader carries the words — and, through it, the braille
        // display. When VoiceOver is off there is deliberately no speech: the
        // list on screen is the sighted user's confirmation.
        guard isVoiceOverRunning() else { return }
        UIAccessibility.post(notification: .announcement, argument: message)
    }

    func cancel() {
        // Nothing to tear down: announcements are fire-and-forget into the
        // screen reader's own queue, and it owns their interruption policy.
        // Deliberately NOT calling any stop-speaking API — that would be us
        // reaching into a channel we do not own.
    }
}
