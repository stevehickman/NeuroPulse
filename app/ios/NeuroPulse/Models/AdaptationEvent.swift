import Foundation

// Adaptation event record — NP-APP-ROADMAP-001 Rev B §9.4, STEP-33.
// Persisted to UHDR; displayed in Session History via AdaptiveAdjustmentsCard.
// Plain-language trigger enum satisfies GDPR Art. 13(2)(f) transparency obligation.

struct AdaptationEvent: Identifiable, Codable {

    enum Trigger: String, Codable, CaseIterable {
        case eegBandShift       = "EEG frequency response"
        case hrvCoherence       = "Heart-rate variability coherence"
        case thermalThrottle    = "Temperature management"
        case impedanceChange    = "Electrode contact change"
        case userPaused         = "Session paused"
    }

    let id: UUID
    let timestamp: Date
    let modality: String
    let trigger: Trigger
    let parameterChanged: String
    let previousValue: Double
    let newValue: Double
    let unit: String

    init(id: UUID = UUID(),
         timestamp: Date = Date(),
         modality: String,
         trigger: Trigger,
         parameterChanged: String,
         previousValue: Double,
         newValue: Double,
         unit: String) {
        self.id = id
        self.timestamp = timestamp
        self.modality = modality
        self.trigger = trigger
        self.parameterChanged = parameterChanged
        self.previousValue = previousValue
        self.newValue = newValue
        self.unit = unit
    }
}
