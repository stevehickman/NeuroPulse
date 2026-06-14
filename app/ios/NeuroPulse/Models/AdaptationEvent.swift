import Foundation

// Mirrors np_adapt_trigger_t in firmware/hub_control/include/np_adaptation_log.h
// Plain-language copy requires Privacy Lead sign-off (OI-PA-04) before shipping.
// Never add raw EEG or HRV float values to this file — trigger context is enum-only.

enum AdaptTrigger: UInt8, CaseIterable {
    // EEG band-power triggers
    case eegAlphaLow        = 0x00
    case eegAlphaHigh       = 0x01
    case eegThetaLow        = 0x02
    case eegThetaHigh       = 0x03
    case eegGammaLow        = 0x04
    case eegGammaHigh       = 0x05
    case eegBetaLow         = 0x06
    case eegBetaHigh        = 0x07
    // HRV triggers
    case hrvCoherenceLow    = 0x08
    case hrvCoherenceHigh   = 0x09
    case hrvRmssdLow        = 0x0A
    // Safety / device-condition triggers
    case impedanceChange    = 0x0B
    case doseLimitApproach  = 0x0C
    case thermalThrottle    = 0x0D
    // Session-lifecycle triggers
    case phaseTransition    = 0x0E
    case userPause          = 0x0F
    case sessionEndRamp     = 0x10

    // Approved plain-language descriptions — extend only with Privacy Lead sign-off (OI-PA-04).
    var plainLanguageDescription: String {
        switch self {
        case .eegAlphaLow:
            return "Brainwave activity in the calm-focus range dipped — stimulation frequency was increased to support focus."
        case .eegAlphaHigh:
            return "Brainwave activity in the calm-focus range was elevated — stimulation frequency was reduced to avoid over-stimulation."
        case .eegThetaLow:
            return "Brainwave activity in the memory-processing range dipped — stimulation was adjusted to support memory encoding."
        case .eegThetaHigh:
            return "Brainwave activity in the memory-processing range was elevated — stimulation was reduced."
        case .eegGammaLow:
            return "High-frequency brainwave activity (linked to focus and attention) dipped — stimulation frequency was stepped up."
        case .eegGammaHigh:
            return "High-frequency brainwave activity was elevated — stimulation frequency was stepped down to prevent fatigue."
        case .eegBetaLow:
            return "Alertness-linked brainwave activity dipped — stimulation was adjusted to maintain engagement."
        case .eegBetaHigh:
            return "Alertness-linked brainwave activity was elevated — stimulation was reduced to encourage relaxation."
        case .hrvCoherenceLow:
            // swiftlint:disable:next line_length
            return "Heart rhythm coherence fell below your personal target — the breathing pacer rate was adjusted to help restore coherence."
        case .hrvCoherenceHigh:
            // swiftlint:disable:next line_length
            return "Heart rhythm coherence was above target — the breathing pacer rate was eased to maintain the effect without over-correction."
        case .hrvRmssdLow:
            return "Heart rate variability fell below your baseline — stimulation intensity was reduced to lower physiological load."
        case .impedanceChange:
            return "Electrode contact quality changed during the session — stimulation was adjusted to account for the change."
        case .doseLimitApproach:
            return "The session was approaching its cumulative light-energy limit — output was reduced to stay within the safe dose."
        case .thermalThrottle:
            return "A device component reached its temperature limit — output was reduced temporarily to allow cooling."
        case .phaseTransition:
            return "The session moved to a new phase — stimulation parameters were updated for the new phase."
        case .userPause:
            return "The session was paused — all stimulation was reduced to standby levels."
        case .sessionEndRamp:
            return "The session was completing — all stimulation was gradually reduced to zero."
        }
    }
}

// One closed-loop adaptation event as decoded from the UHDR log.
// Mirrors np_adaptation_event_t — no raw biometric floats.
struct AdaptationEvent: Identifiable {
    // Stable identity derived from content — the same UHDR event decoded twice
    // produces the same id, so SwiftUI diffing is correct across re-renders.
    // Format: "<sessionOffsetMs>-<trigger.rawValue>-<modType>-<paramId>"
    var id: String { "\(sessionOffsetMs)-\(trigger.rawValue)-\(modType)-\(paramId)" }

    var sessionOffsetMs:  UInt32
    var trigger:          AdaptTrigger
    var modType:          UInt8       // np_hub_mod_type_t raw value
    var paramId:          UInt8
    var valueBeforeX100:  Int32       // parameter value before × 100
    var valueAfterX100:   Int32       // parameter value after × 100
    var confidencePct:    UInt8       // 0–100; 0xFF = not applicable

    var sessionOffset: TimeInterval { TimeInterval(sessionOffsetMs) / 1000.0 }
}

// Post-session summary handed to SessionHistoryView.
//
// Display-aggregated metrics only — no raw EEG/HRV waveform data. The optional
// coherence/RMSSD/impedance fields are the same values shown live during the
// session and are what SessionHistoryStore persists.
struct CompletedSessionSummary {
    var protocolName:      String
    var durationSeconds:   UInt32
    var adaptationEvents:  [AdaptationEvent]
    var completedAt:       Date

    var averageCoherenceScore: Float?  = nil   // 0.0–10.0; nil when no HRV modality
    var rmssdMilliseconds:     UInt16? = nil    // integer only
    var impedancePassCount:    Int     = 0      // 0–8 EEG electrodes passed
    var edfSessionID:          UInt32? = nil    // hub session ID for Mode 4 EDF download

    // Reconstruct a summary from a persisted history row. Adaptation events are
    // not persisted in the history store (they live in UHDR), so the detail view
    // opened from history shows the empty-state Adaptive Adjustments card.
    //
    // completedAt is reconstructed from sessionDay at start-of-day in local
    // timezone. Time-of-day is not available — the store holds day granularity
    // only (UHDR boundary; NP-PRIV-ANALYSIS-002 MEDIUM-07).
    init(record: SessionRecord) {
        self.protocolName          = record.protocolName
        self.durationSeconds       = UInt32(record.durationSeconds.rounded())
        self.adaptationEvents      = []
        self.completedAt           = SessionRecord.dayFormatter.date(from: record.sessionDay) ?? .distantPast
        self.averageCoherenceScore = record.averageCoherenceScore
        self.rmssdMilliseconds     = record.rmssdMilliseconds
        self.impedancePassCount    = record.impedancePassCount
        self.edfSessionID          = record.edfSessionID
    }

    init(protocolName: String,
         durationSeconds: UInt32,
         adaptationEvents: [AdaptationEvent],
         completedAt: Date,
         averageCoherenceScore: Float? = nil,
         rmssdMilliseconds: UInt16? = nil,
         impedancePassCount: Int = 0,
         edfSessionID: UInt32? = nil) {
        self.protocolName          = protocolName
        self.durationSeconds       = durationSeconds
        self.adaptationEvents      = adaptationEvents
        self.completedAt           = completedAt
        self.averageCoherenceScore = averageCoherenceScore
        self.rmssdMilliseconds     = rmssdMilliseconds
        self.impedancePassCount    = impedancePassCount
        self.edfSessionID          = edfSessionID
    }
}
