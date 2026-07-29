import AVFoundation
import UIKit

// Speaks zone-module insertion confirmations on THIS device's speaker.
//
// ── Why the app says this and not the helmet ─────────────────────────────────
//
// The headset has a bone-conduction transducer at the mastoid, and the original
// design (NP-FW-ZA-001 Rev A §7–8) announced "Frontal Left connected" through it
// the instant a module was seated. That confirmation could never be heard.
// Bone conduction only reaches a user whose head the transducer is pressed
// against, and seating a module requires the helmet OFF the head — a worn head
// is physically in the way of the socket. The cue fired into an empty room.
//
// The confirmation therefore belongs on whatever device the user is holding and
// looking at during setup. The firmware now reports insertions over the
// module-status characteristic (`np_zone_notify.h`); this speaks them.
//
// The headset's tone engine is not retired — it stays for announcements
// delivered while the helmet IS worn (`np_za_play_worn_cue`).

@MainActor
protocol ZoneModuleAnnouncing {
    /// Speak a module-insertion confirmation.
    func announce(_ message: String)
    /// Abandon anything still being spoken (leaving the step, disconnect).
    func cancel()
}

/// Production announcer.
///
/// Routes to VoiceOver when VoiceOver is running and to `AVSpeechSynthesizer`
/// otherwise. The split matters: VoiceOver owns the audio channel for its users,
/// and speaking over it both talks across the screen reader and says the same
/// thing twice. The rest of the app already posts `UIAccessibility` announcements
/// this way (see SessionView's coherence updates).
///
/// This is not a VoiceOver-only feature. The bone-conduction cue it replaces
/// existed for sighted users too — anyone setting up a helmet is looking at the
/// hardware in their hands, not at the phone.
@MainActor
final class ZoneModuleAnnouncer: ZoneModuleAnnouncing {

    private let synthesizer = AVSpeechSynthesizer()
    private let isVoiceOverRunning: () -> Bool

    /// - Parameter isVoiceOverRunning: injectable so tests can drive both
    ///   branches; defaults to the live UIKit query.
    init(isVoiceOverRunning: @escaping () -> Bool = { UIAccessibility.isVoiceOverRunning }) {
        self.isVoiceOverRunning = isVoiceOverRunning
    }

    func announce(_ message: String) {
        guard !message.isEmpty else { return }

        if isVoiceOverRunning() {
            UIAccessibility.post(notification: .announcement, argument: message)
            return
        }

        // Mix rather than interrupt: the user may be playing something, and a
        // setup confirmation does not warrant taking over their audio session.
        // A failure here is not worth surfacing — the setup list still shows the
        // socket turn green, so the confirmation is not the only feedback.
        try? AVAudioSession.sharedInstance().setCategory(
            .playback, mode: .spokenAudio, options: [.mixWithOthers, .duckOthers])
        try? AVAudioSession.sharedInstance().setActive(true)

        let utterance = AVSpeechUtterance(string: message)
        utterance.rate = AVSpeechUtteranceDefaultSpeechRate
        // Match the user's own language rather than assuming English — the app
        // ships localised (Localizable.xcstrings) and the string is localised too.
        utterance.voice = AVSpeechSynthesisVoice(language: Locale.current.identifier)
            ?? AVSpeechSynthesisVoice(language: "en-US")
        synthesizer.speak(utterance)
    }

    func cancel() {
        if synthesizer.isSpeaking {
            synthesizer.stopSpeaking(at: .immediate)
        }
    }
}
