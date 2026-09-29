import Foundation

// Swift port of app/web/src/lib/wavelengthRules.ts (the reference),
// NP-NPPS-REF-001 Rev 17 §4.1a and §7a.
//
// A protocol states the wavelength its source used ("810nm"). These rules say
// which emitter channel, if any, may deliver it. A wavelength no rule accepts
// maps to NOTHING: never the nearest channel, never every channel. iOS reads
// only the shipped defaults; editing the rules is done in the web app for now,
// and `wavelength_rules` blocks in a script are accepted and ignored here.

enum NPPBMChannelElement: String, CaseIterable {
    case led660 = "led_660"
    case led808 = "led_808"
    case led1064 = "led_1064"
}

struct NPWavelengthChannelRule: Equatable {
    let element: NPPBMChannelElement
    /// Only breaks a tie between two accepting channels: nearest nominal wins.
    let nominalNm: Double
    let minNm: Double
    let maxNm: Double
}

struct NPWavelengthRules: Equatable {
    let name: String
    let channels: [NPWavelengthChannelRule]

    /// Mirrors protocols/predefined/00-wavelength-rules.npps: each channel's band in
    /// CLAUDE.md §3 widened 10 nm each side. An UNVALIDATED DEFAULT, not a safety parameter.
    static let `default` = NPWavelengthRules(
        name: "NeurOne default wavelength mapping",
        channels: [
            NPWavelengthChannelRule(element: .led660, nominalNm: 660, minNm: 650, maxNm: 680),
            NPWavelengthChannelRule(element: .led808, nominalNm: 808, minNm: 798, maxNm: 840),
            NPWavelengthChannelRule(element: .led1064, nominalNm: 1064, minNm: 1054, maxNm: 1074),
        ]
    )

    /// The three legacy channel names. They name channels, not a source's wavelength.
    static let legacy: [String: [NPPBMChannelElement]] = [
        "660_808nm": [.led660, .led808],
        "1064nm": [.led1064],
        "660_808_1064nm": [.led660, .led808, .led1064],
    ]

    /// The channel that delivers `nm`, or nil. Nearest nominal wins; a tie goes to the
    /// earlier channel (660, 808, 1064), so rule order never matters.
    func map(_ nm: Double) -> NPPBMChannelElement? {
        var best: NPWavelengthChannelRule?
        for element in NPPBMChannelElement.allCases {
            guard let rule = channels.first(where: { $0.element == element }),
                  nm >= rule.minNm, nm <= rule.maxNm else { continue }
            if let b = best, abs(nm - rule.nominalNm) >= abs(nm - b.nominalNm) { continue }
            best = rule
        }
        return best?.element
    }

    enum Refusal: Equatable { case invalid, unmapped }

    /// The channels a `wavelength` value drives, or the reason it drives none.
    func resolveChannels(_ value: String) -> Result<[NPPBMChannelElement], NPWavelengthRefusal> {
        if let legacy = Self.legacy[value] { return .success(legacy) }
        let digits = value.dropLast(2)
        guard value.hasSuffix("nm"),
              let first = digits.first, first.isASCII, first.isNumber,
              digits.allSatisfy({ ($0.isASCII && $0.isNumber) || $0 == "." }),
              digits.filter({ $0 == "." }).count <= 1, digits.last != ".",
              let nm = Double(digits),
              nm > 0 else {
            return .failure(NPWavelengthRefusal(value: value, reason: .invalid))
        }
        guard let element = map(nm) else {
            return .failure(NPWavelengthRefusal(value: value, reason: .unmapped))
        }
        return .success([element])
    }
}

/// Why a PBM block's wavelength cannot be delivered. Thrown by the session compiler.
struct NPWavelengthRefusal: Error, LocalizedError, Equatable {
    let value: String
    let reason: NPWavelengthRules.Refusal

    var errorDescription: String? {
        switch reason {
        case .invalid:
            return "PBM wavelength '\(value)' is not a wavelength: write one value such as \"810nm\"."
        case .unmapped:
            return "No emitter channel delivers \(value) under the wavelength rules in force. " +
                "Refused, not moved to the nearest channel."
        }
    }
}

/// A block the session wire cannot express. Refused rather than flattened.
struct NPUnsupportedTimingError: Error, LocalizedError, Equatable {
    var errorDescription: String? {
        "This protocol times a block with `start`, which the session wire cannot express yet. " +
            "Refused, not reshaped."
    }
}
