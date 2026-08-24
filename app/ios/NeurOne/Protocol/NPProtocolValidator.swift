import Foundation

// MARK: - Validation result types

struct NPValidationIssue: Identifiable {
    var id = UUID()
    var severity: Severity
    var modality: NPModalityType?          // nil = protocol-level issue
    var parameterKey: String               // camelCase field name e.g. "intensityMilliamps"
    var parameterDisplayName: String       // e.g. "Intensity"
    var actualValueDescription: String     // e.g. "1.5 mA"
    var limitValueDescription: String      // e.g. "1.0 mA (Hardware Limit)"
    var limitSource: NPLimitSource
    var message: String

    enum Severity { case error, warning }
}

struct NPValidationResult {
    var issues: [NPValidationIssue] = []
    var isValid: Bool { !issues.contains { $0.severity == .error } }
    var hasWarnings: Bool { issues.contains { $0.severity == .warning } }
    var errors: [NPValidationIssue] { issues.filter { $0.severity == .error } }
    var warnings: [NPValidationIssue] { issues.filter { $0.severity == .warning } }

    mutating func addError(
        modality: NPModalityType? = nil,
        param: String,
        displayName: String,
        actual: String,
        limit: String,
        source: NPLimitSource,
        message: String
    ) {
        issues.append(NPValidationIssue(
            severity: .error,
            modality: modality,
            parameterKey: param,
            parameterDisplayName: displayName,
            actualValueDescription: actual,
            limitValueDescription: "\(limit) (\(source))",
            limitSource: source,
            message: message
        ))
    }

    mutating func addWarning(
        modality: NPModalityType? = nil,
        param: String,
        displayName: String,
        actual: String,
        limit: String,
        source: NPLimitSource,
        message: String
    ) {
        issues.append(NPValidationIssue(
            severity: .warning,
            modality: modality,
            parameterKey: param,
            parameterDisplayName: displayName,
            actualValueDescription: actual,
            limitValueDescription: "\(limit) (\(source))",
            limitSource: source,
            message: message
        ))
    }
}

// MARK: - NPProtocolValidator

struct NPProtocolValidator {
    let resolvedLimits: NPLimitsSet
    let sourceMap: NPLimitSourceMap

    // Convenience initialiser — if you only have limits without a source map
    init(resolvedLimits: NPLimitsSet, sourceMap: NPLimitSourceMap = NPLimitSourceMap()) {
        self.resolvedLimits = resolvedLimits
        self.sourceMap = sourceMap
    }

    // MARK: Public interface

    func validate(_ definition: NPProtocolDefinition) -> NPValidationResult {
        var result = NPValidationResult()
        let enabledModalities = definition.modalities.filter { $0.enabled }

        // Protocol-level checks
        if enabledModalities.isEmpty {
            result.issues.append(NPValidationIssue(
                severity: .error,
                modality: nil,
                parameterKey: "modalities",
                parameterDisplayName: "Modalities",
                actualValueDescription: "0",
                limitValueDescription: "≥1 (Hardware Limit)",
                limitSource: .hardware,
                message: "Protocol has no enabled modalities."
            ))
        }

        let totalDurationSeconds = definition.totalDurationSeconds

        if let dur = totalDurationSeconds {
            // Hard error: zero or negative duration is nonsensical (ISC-47).
            if dur <= 0 {
                result.addError(
                    param: "duration", displayName: "Duration",
                    actual: "\(dur)s", limit: "> 0s", source: .hardware,
                    message: "Session duration must be greater than zero."
                )
            } else if dur < 60 {
                result.addWarning(
                    param: "duration", displayName: "Duration",
                    actual: "\(dur)s", limit: "60s", source: .hardware,
                    message: "Session is very short (< 1 minute). Verify this is intentional."
                )
            }
            if dur > 7200 {
                result.addWarning(
                    param: "duration", displayName: "Duration",
                    actual: "\(dur / 60)m", limit: "120m", source: .hardware,
                    message: "Session is very long (> 2 hours). Verify this is intentional."
                )
            }
        }

        // Per-modality validation (pass total duration so charge-density and dose checks work)
        for block in enabledModalities {
            validateModality(block, totalDurationSeconds: totalDurationSeconds, into: &result)
        }

        // Cross-modality charge density check for tDCS (ISC-38).
        // Charge density (µC/cm²) = I(mA) × t(s) / A(cm²).
        // Area is estimated as NPHardwareLimits.tdcsDefaultElectrodeAreaCm2 per electrode position.
        if let dur = totalDurationSeconds, dur > 0 {
            for block in enabledModalities {
                if case .tdcs(let p) = block.params {
                    let numElectrodes = Double(p.electrodePairs.flatMap { $0 }.count)
                    guard numElectrodes > 0 else { continue }
                    let totalAreaCm2 = NPHardwareLimits.tdcsDefaultElectrodeAreaCm2 * numElectrodes
                    let chargeDensity = p.intensityMilliamps * Double(dur) / totalAreaCm2
                    if chargeDensity > NPHardwareLimits.tdcsMaxChargeDensityUCcm2 {
                        result.addError(
                            modality: .tdcs,
                            param: "chargeDensityUCcm2", displayName: "Charge Density",
                            actual: String(format: "%.1f µC/cm²", chargeDensity),
                            limit: "\(Int(NPHardwareLimits.tdcsMaxChargeDensityUCcm2)) µC/cm²",
                            source: .hardware,
                            message: String(format:
                                "Estimated tDCS charge density %.1f µC/cm² exceeds the " +
                                "40 µC/cm² safety ceiling. Reduce current or session duration.",
                                chargeDensity)
                        )
                    }
                }
            }
        }

        // Cross-modality checks
        let hasBES = enabledModalities.contains {
            if case .besTacs = $0.params { return true }
            return false
        }
        let hasTDCS = enabledModalities.contains {
            if case .tdcs = $0.params { return true }
            return false
        }
        if hasBES && hasTDCS {
            result.addWarning(
                param: "cross_modality", displayName: "Cross-modality",
                actual: "BES + tDCS active", limit: "Separate electrode paths", source: .hardware,
                message: "BES and tDCS active simultaneously — ensure electrode paths are non-overlapping to prevent current interaction."
            )
        }

        return result
    }

    @MainActor
    func validate(_ entry: NPProtocolEntry, resolving library: NPProtocolLibrary?) -> NPValidationResult {
        switch entry {
        case .single(let p):
            return validate(p)
        case .composite(let c):
            return validateComposite(c, library: library)
        case .limits, .zone, .condition:
            // Not protocols: limits are constraints, zones and conditions are
            // namespace definitions referenced by name and never run.
            return NPValidationResult()
        }
    }

    // MARK: Composite

    @MainActor
    private func validateComposite(_ c: NPCompositeProtocol, library: NPProtocolLibrary?) -> NPValidationResult {
        var result = NPValidationResult()
        if c.layers.isEmpty {
            result.issues.append(NPValidationIssue(
                severity: .error,
                modality: nil,
                parameterKey: "layers",
                parameterDisplayName: "Layers",
                actualValueDescription: "0",
                limitValueDescription: "≥1 (Hardware Limit)",
                limitSource: .hardware,
                message: "Composite protocol has no layers."
            ))
        }
        for layer in c.layers {
            if let proto = library?.allProtocols.first(where: { $0.name == layer.protocolName }),
               case .single(let def) = proto {
                let sub = validate(def)
                let prefixed = sub.issues.map { issue -> NPValidationIssue in
                    var i = issue
                    i.message = "[\(layer.protocolName)] \(i.message)"
                    return i
                }
                result.issues.append(contentsOf: prefixed)
            } else if library != nil {
                result.issues.append(NPValidationIssue(
                    severity: .error,
                    modality: nil,
                    parameterKey: "layer_ref",
                    parameterDisplayName: "Layer",
                    actualValueDescription: layer.protocolName,
                    limitValueDescription: "Known protocol (Hardware Limit)",
                    limitSource: .hardware,
                    message: "Layer references unknown protocol '\(layer.protocolName)'."
                ))
            }
        }
        return result
    }

    // MARK: Per-modality dispatch

    private func validateModality(
        _ block: NPProtocolModality,
        totalDurationSeconds: Int?,
        into result: inout NPValidationResult
    ) {
        switch block.params {
        case .pbmTranscranial(let p):
            validatePBMTranscranial(p, interval: block.interval,
                                    totalDurationSeconds: totalDurationSeconds, into: &result)
        case .pbmIntranasal(let p):     validatePBMIntranasal(p, interval: block.interval, into: &result)
        case .eegNeurofeedback(let p):  validateEEG(p, into: &result)
        case .besTacs(let p):           validateBESTacs(p, interval: block.interval, into: &result)
        case .tdcs(let p):              validateTDCS(p, interval: block.interval, into: &result)
        case .vnsHRV(let p):            validateVNS(p, interval: block.interval, into: &result)
        case .audioEntrainment(let p):  validateAudio(p, into: &result)
        case .visualStimulation(let p): validateVisual(p, interval: block.interval, into: &result)
        case .qeeg21ch:                 break // passive recording — no stimulation limits
        case .tms(let p):               validateTMS(p, into: &result)
        case .pbmDeep1170nm(let p):     validateDeepPBM(p, interval: block.interval, into: &result)
        case .clinicalTacs(let p):      validateClinicalTacs(p, interval: block.interval, into: &result)
        case .hdTdcs(let p):            validateHDTdcs(p, interval: block.interval, into: &result)
        case .cervicalVns(let p):       validateCervicalVns(p, interval: block.interval, into: &result)
        case .vibrotactile40hz(let p):  validateVibrotactile(p, into: &result)
        }
    }

    // MARK: Per-modality validators

    private func validatePBMTranscranial(
        _ p: NPPBMTranscranialParams,
        interval: NPIntervalConfig,
        totalDurationSeconds: Int?,
        into result: inout NPValidationResult
    ) {
        let m = NPModalityType.pbmTranscranial
        let lim = resolvedLimits.pbmTranscranial
        let srcs = sourceMap.pbmTranscranial

        // Targeting: the modality must resolve to at least one real socket.
        //
        // Checked here, before upload, rather than left to throw at wire-build
        // time: a mistyped zone name is an authoring error, and the clinician
        // needs it as a readable message next to the protocol they are editing.
        // `clinicianSelected` is exempt — it is unresolvable by design until the
        // operator picks sockets at session start.
        if p.target != .clinicianSelected {
            do {
                let mask = try p.resolveSocketMask()
                if mask.isEmpty {
                    result.addError(
                        modality: m, param: "target", displayName: "Target Zones",
                        actual: p.target.displayName, limit: "≥1 socket", source: .hardware,
                        message: "PBM transcranial target \(p.target.displayName) resolves to no sockets"
                            + " — a session would report a delivered dose while lighting nothing."
                    )
                }
            } catch {
                result.addError(
                    modality: m, param: "target", displayName: "Target Zones",
                    actual: p.target.displayName,
                    limit: "a named zone in 00-zones.npps", source: .hardware,
                    message: error.localizedDescription
                )
            }
        }

        // Hardware: duty cycle ≤ 25%
        if p.dutyCyclePercent > NPHardwareLimits.pbmDutyCycleMaxPercent {
            result.addError(
                modality: m, param: "dutyCyclePercent", displayName: "Duty Cycle",
                actual: "\(p.dutyCyclePercent)%",
                limit: "\(NPHardwareLimits.pbmDutyCycleMaxPercent)%",
                source: .hardware,
                message: "PBM duty cycle \(p.dutyCyclePercent)% exceeds firmware-enforced"
                    + " maximum of \(NPHardwareLimits.pbmDutyCycleMaxPercent)%."
            )
        }

        // Hardware: frequency cannot be negative
        if p.frequencyHz < 0 {
            result.addError(
                modality: m, param: "frequencyHz", displayName: "Frequency",
                actual: "\(p.frequencyHz) Hz", limit: "≥0 Hz", source: .hardware,
                message: "PBM frequency cannot be negative."
            )
        }

        // Dosage: max intensity
        if let maxI = lim?.maxIntensityPercent, p.intensityPercent > maxI {
            result.addError(
                modality: m, param: "intensityPercent", displayName: "Intensity",
                actual: "\(Int(p.intensityPercent))%",
                limit: "\(Int(maxI))%",
                source: srcs?.maxIntensityPercent ?? .global_,
                message: "PBM transcranial intensity \(Int(p.intensityPercent))% exceeds limit of \(Int(maxI))%."
            )
        }

        // Dosage: max frequency
        if let maxF = lim?.maxFrequencyHz, p.frequencyHz > maxF {
            result.addError(
                modality: m, param: "frequencyHz", displayName: "Frequency",
                actual: "\(formatHz(p.frequencyHz))",
                limit: "\(formatHz(maxF))",
                source: srcs?.maxFrequencyHz ?? .global_,
                message: "PBM frequency \(formatHz(p.frequencyHz)) exceeds limit of \(formatHz(maxF))."
            )
        }

        // Dosage: max duty cycle (configured limit, tighter than hardware 25%)
        if let maxDC = lim?.maxDutyCyclePercent, p.dutyCyclePercent > maxDC {
            result.addError(
                modality: m, param: "dutyCyclePercent", displayName: "Duty Cycle",
                actual: "\(p.dutyCyclePercent)%",
                limit: "\(maxDC)%",
                source: srcs?.maxDutyCyclePercent ?? .global_,
                message: "PBM duty cycle \(p.dutyCyclePercent)% exceeds configured limit of \(maxDC)%."
            )
        }

        // Dosage: max session dose J/cm² (ISC-47).
        // Estimated irradiance: CW path uses pbmCWMaxMWcm2; pulsed path scales by duty cycle.
        // Dose (J/cm²) = irradiance (mW/cm²) × intensity_fraction × duration (s) / 1000.
        if let maxDose = lim?.maxSessionDoseJCm2, let dur = totalDurationSeconds, dur > 0 {
            let peakMWcm2: Double = p.frequencyHz == 0
                ? NPHardwareLimits.pbmCWMaxMWcm2                                        // CW
                : NPHardwareLimits.pbmPulsedPeakMWcm2 * Double(p.dutyCyclePercent) / 100 // pulsed avg
            let estimatedDose = peakMWcm2 * (p.intensityPercent / 100.0) * Double(dur) / 1000.0
            if estimatedDose > maxDose {
                result.addError(
                    modality: m, param: "sessionDoseJCm2", displayName: "Session Dose",
                    actual: String(format: "%.1f J/cm²", estimatedDose),
                    limit: String(format: "%.1f J/cm²", maxDose),
                    source: srcs?.maxSessionDoseJCm2 ?? .global_,
                    message: String(format:
                        "Estimated PBM session dose %.1f J/cm² exceeds the configured limit of %.1f J/cm².",
                        estimatedDose, maxDose)
                )
            }
        }
    }

    private func validatePBMIntranasal(
        _ p: NPPBMIntranasalParams,
        interval: NPIntervalConfig,
        into result: inout NPValidationResult
    ) {
        let m = NPModalityType.pbmIntranasal
        let lim = resolvedLimits.pbmIntranasal
        let srcs = sourceMap.pbmIntranasal

        // Hardware: duty cycle ≤ 25%
        if p.dutyCyclePercent > NPHardwareLimits.pbmDutyCycleMaxPercent {
            result.addError(
                modality: m, param: "dutyCyclePercent", displayName: "Duty Cycle",
                actual: "\(p.dutyCyclePercent)%",
                limit: "\(NPHardwareLimits.pbmDutyCycleMaxPercent)%",
                source: .hardware,
                message: "Intranasal PBM duty cycle \(p.dutyCyclePercent)% exceeds firmware-enforced"
                    + " maximum of \(NPHardwareLimits.pbmDutyCycleMaxPercent)%."
            )
        }

        // Dosage: max intensity
        if let maxI = lim?.maxIntensityPercent, p.intensityPercent > maxI {
            result.addError(
                modality: m, param: "intensityPercent", displayName: "Intensity",
                actual: "\(Int(p.intensityPercent))%",
                limit: "\(Int(maxI))%",
                source: srcs?.maxIntensityPercent ?? .global_,
                message: "Intranasal PBM intensity \(Int(p.intensityPercent))% exceeds limit of \(Int(maxI))%."
            )
        }

        // Dosage: max session duration from interval
        if let maxDur = lim?.maxSessionDurationSeconds {
            let dur = interval.isContinuous ? nil : interval.intervalOnSeconds
            if let d = dur, d > maxDur {
                result.addError(
                    modality: m, param: "sessionDuration", displayName: "Session Duration",
                    actual: formatSeconds(d),
                    limit: formatSeconds(maxDur),
                    source: srcs?.maxSessionDurationSeconds ?? .global_,
                    message: "Intranasal PBM session duration \(formatSeconds(d)) exceeds limit of \(formatSeconds(maxDur))."
                )
            }
        }
    }

    private func validateEEG(
        _ p: NPEEGNeurofeedbackParams,
        into result: inout NPValidationResult
    ) {
        let m = NPModalityType.eegNeurofeedback
        let lim = resolvedLimits.eegNeurofeedback
        let srcs = sourceMap.eegNeurofeedback

        // Dosage: allowed bands whitelist
        if let allowedBands = lim?.allowedBands {
            let bandRaw = p.band.rawValue
            if !allowedBands.contains(bandRaw) {
                result.addError(
                    modality: m, param: "band", displayName: "EEG Band",
                    actual: p.band.displayName,
                    limit: allowedBands.joined(separator: ", "),
                    source: srcs?.allowedBands ?? .global_,
                    message: "EEG band '\(p.band.displayName)' is not in the allowed list: \(allowedBands.joined(separator: ", "))."
                )
            }
        }

        // Dosage: require closed loop
        if let requireCL = lim?.requireClosedLoop, requireCL, !p.closedLoopEnabled {
            result.addError(
                modality: m, param: "closedLoopEnabled", displayName: "Closed Loop",
                actual: "disabled",
                limit: "required",
                source: srcs?.requireClosedLoop ?? .global_,
                message: "EEG neurofeedback requires closed-loop mode to be enabled per current limits."
            )
        }
    }

    private func validateBESTacs(
        _ p: NPBESTacsParams,
        interval: NPIntervalConfig,
        into result: inout NPValidationResult
    ) {
        let m = NPModalityType.besTacs
        let lim = resolvedLimits.besTacs
        let srcs = sourceMap.besTacs

        // Hardware: intensity ≤ 1.0 mA
        if p.intensityMilliamps > NPHardwareLimits.besTacsMaxMilliamps {
            result.addError(
                modality: m, param: "intensityMilliamps", displayName: "Intensity",
                actual: "\(p.intensityMilliamps) mA",
                limit: "\(NPHardwareLimits.besTacsMaxMilliamps) mA",
                source: .hardware,
                message: "BES/tACS intensity \(p.intensityMilliamps) mA exceeds firmware-enforced"
                    + " maximum of \(NPHardwareLimits.besTacsMaxMilliamps) mA."
            )
        }

        // Hardware: frequency 0.5–40 Hz
        if p.frequencyHz < NPHardwareLimits.besTacsMinHz {
            result.addError(
                modality: m, param: "frequencyHz", displayName: "Frequency",
                actual: "\(formatHz(p.frequencyHz))",
                limit: "≥\(formatHz(NPHardwareLimits.besTacsMinHz))",
                source: .hardware,
                message: "BES/tACS frequency \(formatHz(p.frequencyHz)) is below minimum of \(formatHz(NPHardwareLimits.besTacsMinHz))."
            )
        }
        if p.frequencyHz > NPHardwareLimits.besTacsMaxHz {
            result.addError(
                modality: m, param: "frequencyHz", displayName: "Frequency",
                actual: "\(formatHz(p.frequencyHz))",
                limit: "≤\(formatHz(NPHardwareLimits.besTacsMaxHz))",
                source: .hardware,
                message: "BES/tACS frequency \(formatHz(p.frequencyHz)) exceeds firmware-enforced"
                    + " maximum of \(formatHz(NPHardwareLimits.besTacsMaxHz))."
            )
        }

        // Dosage: max intensity
        if let maxI = lim?.maxIntensityMilliamps, p.intensityMilliamps > maxI {
            result.addError(
                modality: m, param: "intensityMilliamps", displayName: "Intensity",
                actual: "\(p.intensityMilliamps) mA",
                limit: "\(maxI) mA",
                source: srcs?.maxIntensityMilliamps ?? .global_,
                message: "BES/tACS intensity \(p.intensityMilliamps) mA exceeds limit of \(maxI) mA."
            )
        }

        // Dosage: max frequency
        if let maxF = lim?.maxFrequencyHz, p.frequencyHz > maxF {
            result.addError(
                modality: m, param: "frequencyHz", displayName: "Frequency",
                actual: "\(formatHz(p.frequencyHz))",
                limit: "\(formatHz(maxF))",
                source: srcs?.maxFrequencyHz ?? .global_,
                message: "BES/tACS frequency \(formatHz(p.frequencyHz)) exceeds limit of \(formatHz(maxF))."
            )
        }

        // Dosage: min frequency
        if let minF = lim?.minFrequencyHz, p.frequencyHz < minF {
            result.addError(
                modality: m, param: "frequencyHz", displayName: "Frequency",
                actual: "\(formatHz(p.frequencyHz))",
                limit: "≥\(formatHz(minF))",
                source: srcs?.minFrequencyHz ?? .global_,
                message: "BES/tACS frequency \(formatHz(p.frequencyHz)) is below limit of \(formatHz(minF))."
            )
        }

        // Dosage: session duration
        if let maxDur = lim?.maxSessionDurationSeconds, !interval.isContinuous {
            if interval.intervalOnSeconds > maxDur {
                result.addError(
                    modality: m, param: "sessionDuration", displayName: "Session Duration",
                    actual: formatSeconds(interval.intervalOnSeconds),
                    limit: formatSeconds(maxDur),
                    source: srcs?.maxSessionDurationSeconds ?? .global_,
                    message: "BES/tACS interval duration"
                        + " \(formatSeconds(interval.intervalOnSeconds)) exceeds limit of \(formatSeconds(maxDur))."
                )
            }
        }
    }

    private func validateTDCS(
        _ p: NPTDCSParams,
        interval: NPIntervalConfig,
        into result: inout NPValidationResult
    ) {
        let m = NPModalityType.tdcs
        let lim = resolvedLimits.tdcs
        let srcs = sourceMap.tdcs

        // Hardware: intensity 0.1–2.0 mA
        if p.intensityMilliamps < NPHardwareLimits.tdcsMinMilliamps {
            result.addError(
                modality: m, param: "intensityMilliamps", displayName: "Intensity",
                actual: "\(p.intensityMilliamps) mA",
                limit: "≥\(NPHardwareLimits.tdcsMinMilliamps) mA",
                source: .hardware,
                message: "tDCS intensity \(p.intensityMilliamps) mA is below the minimum of \(NPHardwareLimits.tdcsMinMilliamps) mA."
            )
        }
        if p.intensityMilliamps > NPHardwareLimits.tdcsMaxMilliamps {
            result.addError(
                modality: m, param: "intensityMilliamps", displayName: "Intensity",
                actual: "\(p.intensityMilliamps) mA",
                limit: "\(NPHardwareLimits.tdcsMaxMilliamps) mA",
                source: .hardware,
                message: "tDCS intensity \(p.intensityMilliamps) mA exceeds firmware-enforced"
                    + " maximum of \(NPHardwareLimits.tdcsMaxMilliamps) mA."
            )
        }

        // Hardware: electrode pairs ≤ 3
        if p.electrodePairs.count > NPHardwareLimits.tdcsMaxElectrodePairs {
            result.addError(
                modality: m, param: "electrodePairs", displayName: "Electrode Pairs",
                actual: "\(p.electrodePairs.count)",
                limit: "\(NPHardwareLimits.tdcsMaxElectrodePairs)",
                source: .hardware,
                message: "tDCS has \(p.electrodePairs.count) electrode pairs,"
                    + " exceeding the maximum of \(NPHardwareLimits.tdcsMaxElectrodePairs)."
            )
        }

        // Dosage: max intensity
        if let maxI = lim?.maxIntensityMilliamps, p.intensityMilliamps > maxI {
            result.addError(
                modality: m, param: "intensityMilliamps", displayName: "Intensity",
                actual: "\(p.intensityMilliamps) mA",
                limit: "\(maxI) mA",
                source: srcs?.maxIntensityMilliamps ?? .global_,
                message: "tDCS intensity \(p.intensityMilliamps) mA exceeds limit of \(maxI) mA."
            )
        }

        // Dosage: session duration
        if let maxDur = lim?.maxSessionDurationSeconds, !interval.isContinuous, interval.intervalOnSeconds > maxDur {
            result.addError(
                modality: m, param: "sessionDuration", displayName: "Session Duration",
                actual: formatSeconds(interval.intervalOnSeconds),
                limit: formatSeconds(maxDur),
                source: srcs?.maxSessionDurationSeconds ?? .global_,
                message: "tDCS session duration \(formatSeconds(interval.intervalOnSeconds)) exceeds limit of \(formatSeconds(maxDur))."
            )
        }
    }

    private func validateVNS(
        _ p: NPVNSHRVParams,
        interval: NPIntervalConfig,
        into result: inout NPValidationResult
    ) {
        let m = NPModalityType.vnsHRV
        let lim = resolvedLimits.vnsHrv
        let srcs = sourceMap.vnsHrv

        // Hardware: intensity ≤ 2.0 mA
        if p.intensityMilliamps > NPHardwareLimits.vnsMaxMilliamps {
            result.addError(
                modality: m, param: "intensityMilliamps", displayName: "Intensity",
                actual: "\(p.intensityMilliamps) mA",
                limit: "\(NPHardwareLimits.vnsMaxMilliamps) mA",
                source: .hardware,
                message: "VNS intensity \(p.intensityMilliamps) mA exceeds firmware-enforced"
                    + " maximum of \(NPHardwareLimits.vnsMaxMilliamps) mA."
            )
        }

        // Hardware: frequency 1–25 Hz
        if p.frequencyHz < NPHardwareLimits.vnsMinHz {
            result.addError(
                modality: m, param: "frequencyHz", displayName: "Frequency",
                actual: "\(formatHz(p.frequencyHz))",
                limit: "≥\(formatHz(NPHardwareLimits.vnsMinHz))",
                source: .hardware,
                message: "VNS frequency \(formatHz(p.frequencyHz)) is below minimum of \(formatHz(NPHardwareLimits.vnsMinHz))."
            )
        }
        if p.frequencyHz > NPHardwareLimits.vnsMaxHz {
            result.addError(
                modality: m, param: "frequencyHz", displayName: "Frequency",
                actual: "\(formatHz(p.frequencyHz))",
                limit: "≤\(formatHz(NPHardwareLimits.vnsMaxHz))",
                source: .hardware,
                message: "VNS frequency \(formatHz(p.frequencyHz)) exceeds firmware-enforced"
                    + " maximum of \(formatHz(NPHardwareLimits.vnsMaxHz))."
            )
        }

        // Dosage: max intensity
        if let maxI = lim?.maxIntensityMilliamps, p.intensityMilliamps > maxI {
            result.addError(
                modality: m, param: "intensityMilliamps", displayName: "Intensity",
                actual: "\(p.intensityMilliamps) mA",
                limit: "\(maxI) mA",
                source: srcs?.maxIntensityMilliamps ?? .global_,
                message: "VNS intensity \(p.intensityMilliamps) mA exceeds limit of \(maxI) mA."
            )
        }

        // Dosage: max frequency
        if let maxF = lim?.maxFrequencyHz, p.frequencyHz > maxF {
            result.addError(
                modality: m, param: "frequencyHz", displayName: "Frequency",
                actual: "\(formatHz(p.frequencyHz))",
                limit: "\(formatHz(maxF))",
                source: srcs?.maxFrequencyHz ?? .global_,
                message: "VNS frequency \(formatHz(p.frequencyHz)) exceeds limit of \(formatHz(maxF))."
            )
        }

        // Dosage: session duration
        if let maxDur = lim?.maxSessionDurationSeconds, !interval.isContinuous, interval.intervalOnSeconds > maxDur {
            result.addError(
                modality: m, param: "sessionDuration", displayName: "Session Duration",
                actual: formatSeconds(interval.intervalOnSeconds),
                limit: formatSeconds(maxDur),
                source: srcs?.maxSessionDurationSeconds ?? .global_,
                message: "VNS session duration \(formatSeconds(interval.intervalOnSeconds)) exceeds limit of \(formatSeconds(maxDur))."
            )
        }

        // Dosage: allowed protocols whitelist
        if let allowed = lim?.allowedProtocols {
            let protoRaw = p.hrvProtocol.rawValue
            if !allowed.contains(protoRaw) {
                result.addError(
                    modality: m, param: "hrvProtocol", displayName: "HRV Protocol",
                    actual: p.hrvProtocol.displayName,
                    limit: allowed.joined(separator: ", "),
                    source: srcs?.allowedProtocols ?? .global_,
                    message: "HRV protocol '\(p.hrvProtocol.displayName)' is not in the allowed list."
                )
            }
        }
    }

    private func validateAudio(
        _ p: NPAudioEntrainmentParams,
        into result: inout NPValidationResult
    ) {
        let m = NPModalityType.audioEntrainment
        let lim = resolvedLimits.audioEntrainment
        let srcs = sourceMap.audioEntrainment

        // Dosage: max volume
        if let maxVol = lim?.maxVolumePercent, p.volumePercent > maxVol {
            result.addError(
                modality: m, param: "volumePercent", displayName: "Volume",
                actual: "\(Int(p.volumePercent))%",
                limit: "\(Int(maxVol))%",
                source: srcs?.maxVolumePercent ?? .global_,
                message: "Audio volume \(Int(p.volumePercent))% exceeds limit of \(Int(maxVol))%."
            )
        }

        // Dosage: max binaural beats Hz
        if let maxBB = lim?.maxBinauralBeatsHz, let bb = p.binauralBeatsHz, bb > maxBB {
            result.addError(
                modality: m, param: "binauralBeatsHz", displayName: "Binaural Beats",
                actual: "\(formatHz(bb))",
                limit: "\(formatHz(maxBB))",
                source: srcs?.maxBinauralBeatsHz ?? .global_,
                message: "Binaural beats \(formatHz(bb)) exceeds limit of \(formatHz(maxBB))."
            )
        }

        // Dosage: max isochronic tones Hz
        if let maxIT = lim?.maxIsochronicTonesHz, let it = p.isochronicTonesHz, it > maxIT {
            result.addError(
                modality: m, param: "isochronicTonesHz", displayName: "Isochronic Tones",
                actual: "\(formatHz(it))",
                limit: "\(formatHz(maxIT))",
                source: srcs?.maxIsochronicTonesHz ?? .global_,
                message: "Isochronic tones \(formatHz(it)) exceeds limit of \(formatHz(maxIT))."
            )
        }
    }

    private func validateVisual(
        _ p: NPVisualStimParams,
        interval: NPIntervalConfig,
        into result: inout NPValidationResult
    ) {
        let m = NPModalityType.visualStimulation
        let lim = resolvedLimits.visualStimulation
        let srcs = sourceMap.visualStimulation

        // Hardware: frequency ≤ 100 Hz
        if p.frequencyHz > NPHardwareLimits.visualMaxHz {
            result.addError(
                modality: m, param: "frequencyHz", displayName: "Frequency",
                actual: "\(formatHz(p.frequencyHz))",
                limit: "\(formatHz(NPHardwareLimits.visualMaxHz))",
                source: .hardware,
                message: "Visual stimulation frequency \(formatHz(p.frequencyHz)) exceeds firmware-enforced"
                    + " maximum of \(formatHz(NPHardwareLimits.visualMaxHz))."
            )
        }

        // Photoparoxysmal risk zone: 3–30 Hz
        // Default: warning. If blockHighRiskRange is true in limits: error.
        if p.frequencyHz >= NPHardwareLimits.visualHighRiskMinHz && p.frequencyHz <= NPHardwareLimits.visualHighRiskMaxHz {
            let blockRange = lim?.blockHighRiskRange ?? false
            let msg = "Visual stimulation at \(formatHz(p.frequencyHz)) is in the"
                + " photoparoxysmal risk zone"
                + " (\(Int(NPHardwareLimits.visualHighRiskMinHz))"
                + "–\(Int(NPHardwareLimits.visualHighRiskMaxHz)) Hz)."
                + " Clinician unlock required for this range per device safety policy."
            if blockRange {
                result.addError(
                    modality: m, param: "frequencyHz", displayName: "Frequency",
                    actual: "\(formatHz(p.frequencyHz))",
                    limit: "Outside \(Int(NPHardwareLimits.visualHighRiskMinHz))–\(Int(NPHardwareLimits.visualHighRiskMaxHz)) Hz",
                    source: srcs?.blockHighRiskRange ?? .global_,
                    message: msg
                )
            } else {
                result.addWarning(
                    modality: m, param: "frequencyHz", displayName: "Frequency",
                    actual: "\(formatHz(p.frequencyHz))",
                    limit: "Outside \(Int(NPHardwareLimits.visualHighRiskMinHz))–\(Int(NPHardwareLimits.visualHighRiskMaxHz)) Hz",
                    source: .hardware,
                    message: msg
                )
            }
        }

        // Dosage: max frequency
        if let maxF = lim?.maxFrequencyHz, p.frequencyHz > maxF {
            result.addError(
                modality: m, param: "frequencyHz", displayName: "Frequency",
                actual: "\(formatHz(p.frequencyHz))",
                limit: "\(formatHz(maxF))",
                source: srcs?.maxFrequencyHz ?? .global_,
                message: "Visual stimulation frequency \(formatHz(p.frequencyHz)) exceeds limit of \(formatHz(maxF))."
            )
        }

        // Dosage: min frequency
        if let minF = lim?.minFrequencyHz, p.frequencyHz < minF {
            result.addError(
                modality: m, param: "frequencyHz", displayName: "Frequency",
                actual: "\(formatHz(p.frequencyHz))",
                limit: "≥\(formatHz(minF))",
                source: srcs?.minFrequencyHz ?? .global_,
                message: "Visual stimulation frequency \(formatHz(p.frequencyHz)) is below limit of \(formatHz(minF))."
            )
        }

        // Dosage: allowed modes whitelist
        if let allowedModes = lim?.allowedModes {
            let modeRaw = p.mode.rawValue
            if !allowedModes.contains(modeRaw) {
                result.addError(
                    modality: m, param: "mode", displayName: "Visual Mode",
                    actual: p.mode.displayName,
                    limit: allowedModes.joined(separator: ", "),
                    source: srcs?.allowedModes ?? .global_,
                    message: "Visual mode '\(p.mode.displayName)' is not in the allowed list."
                )
            }
        }
    }

    private func validateTMS(
        _ p: NPTMSParams,
        into result: inout NPValidationResult
    ) {
        let m = NPModalityType.tms
        let lim = resolvedLimits.tms
        let srcs = sourceMap.tms

        // Dosage: max intensity % MT
        if let maxMT = lim?.maxIntensityPercentMT, p.intensityPercentMT > maxMT {
            result.addError(
                modality: m, param: "intensityPercentMT", displayName: "Intensity (%MT)",
                actual: "\(p.intensityPercentMT)% MT",
                limit: "\(maxMT)% MT",
                source: srcs?.maxIntensityPercentMT ?? .global_,
                message: "TMS intensity \(p.intensityPercentMT)% MT exceeds limit of \(maxMT)% MT."
            )
        }

        // Dosage: max pulses per session
        if let maxPulses = lim?.maxPulsesPerSession, p.pulseCount > maxPulses {
            result.addError(
                modality: m, param: "pulseCount", displayName: "Pulse Count",
                actual: "\(p.pulseCount) pulses",
                limit: "\(maxPulses) pulses",
                source: srcs?.maxPulsesPerSession ?? .global_,
                message: "TMS pulse count \(p.pulseCount) exceeds session limit of \(maxPulses) pulses."
            )
        }

        // Dosage: allowed protocols whitelist
        if let allowedProtos = lim?.allowedProtocols {
            let protoRaw = p.tmsProtocol.rawValue
            if !allowedProtos.contains(protoRaw) {
                result.addError(
                    modality: m, param: "tmsProtocol", displayName: "TMS Protocol",
                    actual: p.tmsProtocol.displayName,
                    limit: allowedProtos.joined(separator: ", "),
                    source: srcs?.allowedProtocols ?? .global_,
                    message: "TMS protocol '\(p.tmsProtocol.displayName)' is not in the allowed list."
                )
            }
        }

        // Dosage: allowed targets whitelist
        if let allowedTargets = lim?.allowedTargets {
            let targetRaw = p.target.rawValue
            if !allowedTargets.contains(targetRaw) {
                result.addError(
                    modality: m, param: "target", displayName: "TMS Target",
                    actual: p.target.displayName,
                    limit: allowedTargets.joined(separator: ", "),
                    source: srcs?.allowedTargets ?? .global_,
                    message: "TMS target '\(p.target.displayName)' is not in the allowed list."
                )
            }
        }

        // Warn: TMS at >120% MT is aggressive
        if p.intensityPercentMT > 120 {
            result.addWarning(
                modality: m, param: "intensityPercentMT", displayName: "Intensity (%MT)",
                actual: "\(p.intensityPercentMT)% MT",
                limit: "≤120% MT",
                source: .hardware,
                message: "TMS intensity \(p.intensityPercentMT)% MT is high (>120% MT). Verify this is prescribed."
            )
        }
    }

    private func validateDeepPBM(
        _ p: NPDeepPBM1170Params,
        interval: NPIntervalConfig,
        into result: inout NPValidationResult
    ) {
        let m = NPModalityType.pbmDeep1170nm
        let lim = resolvedLimits.pbmDeep1170nm
        let srcs = sourceMap.pbmDeep1170nm

        // Hardware: ≤ 1000 mW/cm²
        if p.intensityMWcm2 > NPHardwareLimits.deepPBMMaxMWcm2 {
            result.addError(
                modality: m, param: "intensityMWcm2", displayName: "Intensity",
                actual: "\(Int(p.intensityMWcm2)) mW/cm²",
                limit: "\(Int(NPHardwareLimits.deepPBMMaxMWcm2)) mW/cm²",
                source: .hardware,
                message: "Deep PBM 1170nm intensity \(Int(p.intensityMWcm2)) mW/cm²"
                    + " exceeds maximum of \(Int(NPHardwareLimits.deepPBMMaxMWcm2)) mW/cm²."
            )
        }

        // Hardware: duty cycle ≤ 25%
        if p.dutyCyclePercent > NPHardwareLimits.pbmDutyCycleMaxPercent {
            result.addError(
                modality: m, param: "dutyCyclePercent", displayName: "Duty Cycle",
                actual: "\(p.dutyCyclePercent)%",
                limit: "\(NPHardwareLimits.pbmDutyCycleMaxPercent)%",
                source: .hardware,
                message: "Deep PBM duty cycle \(p.dutyCyclePercent)% exceeds firmware-enforced"
                    + " maximum of \(NPHardwareLimits.pbmDutyCycleMaxPercent)%."
            )
        }

        // Dosage: max intensity
        if let maxI = lim?.maxIntensityMWcm2, p.intensityMWcm2 > maxI {
            result.addError(
                modality: m, param: "intensityMWcm2", displayName: "Intensity",
                actual: "\(Int(p.intensityMWcm2)) mW/cm²",
                limit: "\(Int(maxI)) mW/cm²",
                source: srcs?.maxIntensityMWcm2 ?? .global_,
                message: "Deep PBM 1170nm intensity \(Int(p.intensityMWcm2)) mW/cm² exceeds limit of \(Int(maxI)) mW/cm²."
            )
        }

        // Dosage: session duration
        if let maxDur = lim?.maxSessionDurationSeconds, !interval.isContinuous, interval.intervalOnSeconds > maxDur {
            result.addError(
                modality: m, param: "sessionDuration", displayName: "Session Duration",
                actual: formatSeconds(interval.intervalOnSeconds),
                limit: formatSeconds(maxDur),
                source: srcs?.maxSessionDurationSeconds ?? .global_,
                message: "Deep PBM session duration \(formatSeconds(interval.intervalOnSeconds)) exceeds limit of \(formatSeconds(maxDur))."
            )
        }
    }

    private func validateClinicalTacs(
        _ p: NPClinicalTacsParams,
        interval: NPIntervalConfig,
        into result: inout NPValidationResult
    ) {
        let m = NPModalityType.clinicalTacs
        let lim = resolvedLimits.clinicalTacs
        let srcs = sourceMap.clinicalTacs

        // Hardware: ≤ 4.0 mA
        if p.intensityMilliamps > NPHardwareLimits.clinicalTacsMaxMilliamps {
            result.addError(
                modality: m, param: "intensityMilliamps", displayName: "Intensity",
                actual: "\(p.intensityMilliamps) mA",
                limit: "\(NPHardwareLimits.clinicalTacsMaxMilliamps) mA",
                source: .hardware,
                message: "Clinical tACS intensity \(p.intensityMilliamps) mA exceeds firmware-enforced"
                    + " maximum of \(NPHardwareLimits.clinicalTacsMaxMilliamps) mA."
            )
        }

        // Dosage: max intensity
        if let maxI = lim?.maxIntensityMilliamps, p.intensityMilliamps > maxI {
            result.addError(
                modality: m, param: "intensityMilliamps", displayName: "Intensity",
                actual: "\(p.intensityMilliamps) mA",
                limit: "\(maxI) mA",
                source: srcs?.maxIntensityMilliamps ?? .global_,
                message: "Clinical tACS intensity \(p.intensityMilliamps) mA exceeds limit of \(maxI) mA."
            )
        }

        // Dosage: session duration
        if let maxDur = lim?.maxSessionDurationSeconds, !interval.isContinuous, interval.intervalOnSeconds > maxDur {
            result.addError(
                modality: m, param: "sessionDuration", displayName: "Session Duration",
                actual: formatSeconds(interval.intervalOnSeconds),
                limit: formatSeconds(maxDur),
                source: srcs?.maxSessionDurationSeconds ?? .global_,
                message: "Clinical tACS session duration"
                        + " \(formatSeconds(interval.intervalOnSeconds)) exceeds limit of \(formatSeconds(maxDur))."
            )
        }
    }

    private func validateHDTdcs(
        _ p: NPHDTdcsParams,
        interval: NPIntervalConfig,
        into result: inout NPValidationResult
    ) {
        let m = NPModalityType.hdTdcs
        let lim = resolvedLimits.hdTdcs
        let srcs = sourceMap.hdTdcs

        // Hardware: ≤ 2.0 mA per electrode
        if p.intensityMilliamps > NPHardwareLimits.hdTdcsMaxMilliampsPerElectrode {
            result.addError(
                modality: m, param: "intensityMilliamps", displayName: "Intensity",
                actual: "\(p.intensityMilliamps) mA",
                limit: "\(NPHardwareLimits.hdTdcsMaxMilliampsPerElectrode) mA",
                source: .hardware,
                message: "HD-tDCS intensity \(p.intensityMilliamps) mA/electrode exceeds"
                    + " Bikson lab safety limit of \(NPHardwareLimits.hdTdcsMaxMilliampsPerElectrode)"
                    + " mA/electrode for 3.5mm Ag/AgCl electrodes."
            )
        }

        // Dosage: max intensity
        if let maxI = lim?.maxIntensityMilliamps, p.intensityMilliamps > maxI {
            result.addError(
                modality: m, param: "intensityMilliamps", displayName: "Intensity",
                actual: "\(p.intensityMilliamps) mA",
                limit: "\(maxI) mA",
                source: srcs?.maxIntensityMilliamps ?? .global_,
                message: "HD-tDCS intensity \(p.intensityMilliamps) mA exceeds limit of \(maxI) mA."
            )
        }

        // Dosage: session duration
        if let maxDur = lim?.maxSessionDurationSeconds, !interval.isContinuous, interval.intervalOnSeconds > maxDur {
            result.addError(
                modality: m, param: "sessionDuration", displayName: "Session Duration",
                actual: formatSeconds(interval.intervalOnSeconds),
                limit: formatSeconds(maxDur),
                source: srcs?.maxSessionDurationSeconds ?? .global_,
                message: "HD-tDCS session duration \(formatSeconds(interval.intervalOnSeconds)) exceeds limit of \(formatSeconds(maxDur))."
            )
        }

        // Dosage: allowed montages whitelist
        if let allowedMontages = lim?.allowedMontages {
            let montageRaw = p.montage.rawValue
            if !allowedMontages.contains(montageRaw) {
                result.addError(
                    modality: m, param: "montage", displayName: "Montage",
                    actual: p.montage.displayName,
                    limit: allowedMontages.joined(separator: ", "),
                    source: srcs?.allowedMontages ?? .global_,
                    message: "HD-tDCS montage '\(p.montage.displayName)' is not in the allowed list."
                )
            }
        }
    }

    private func validateCervicalVns(
        _ p: NPCervicalVnsParams,
        interval: NPIntervalConfig,
        into result: inout NPValidationResult
    ) {
        let m = NPModalityType.cervicalVns
        let lim = resolvedLimits.cervicalVns
        let srcs = sourceMap.cervicalVns

        // Hardware: ≤ 2.0 mA (much more conservative than electroCore predicate ≤24 mA)
        if p.intensityMilliamps > NPHardwareLimits.cervicalVnsMaxMilliamps {
            result.addError(
                modality: m, param: "intensityMilliamps", displayName: "Intensity",
                actual: "\(p.intensityMilliamps) mA",
                limit: "\(NPHardwareLimits.cervicalVnsMaxMilliamps) mA",
                source: .hardware,
                message: "Cervical VNS intensity \(p.intensityMilliamps) mA exceeds firmware-enforced"
                    + " maximum of \(NPHardwareLimits.cervicalVnsMaxMilliamps) mA."
                    + " Cardiac interlock is always enforced by safety MCU regardless."
            )
        }

        // Hardware: frequency in VNS range 1–25 Hz
        if p.frequencyHz < NPHardwareLimits.vnsMinHz || p.frequencyHz > NPHardwareLimits.vnsMaxHz {
            result.addError(
                modality: m, param: "frequencyHz", displayName: "Frequency",
                actual: "\(formatHz(p.frequencyHz))",
                limit: "\(formatHz(NPHardwareLimits.vnsMinHz))–\(formatHz(NPHardwareLimits.vnsMaxHz))",
                source: .hardware,
                message: "Cervical VNS frequency \(formatHz(p.frequencyHz)) is outside the valid"
                    + " range of \(formatHz(NPHardwareLimits.vnsMinHz))–\(formatHz(NPHardwareLimits.vnsMaxHz))."
            )
        }

        // Dosage: max intensity
        if let maxI = lim?.maxIntensityMilliamps, p.intensityMilliamps > maxI {
            result.addError(
                modality: m, param: "intensityMilliamps", displayName: "Intensity",
                actual: "\(p.intensityMilliamps) mA",
                limit: "\(maxI) mA",
                source: srcs?.maxIntensityMilliamps ?? .global_,
                message: "Cervical VNS intensity \(p.intensityMilliamps) mA exceeds limit of \(maxI) mA."
            )
        }

        // Dosage: session duration
        if let maxDur = lim?.maxSessionDurationSeconds, !interval.isContinuous, interval.intervalOnSeconds > maxDur {
            result.addError(
                modality: m, param: "sessionDuration", displayName: "Session Duration",
                actual: formatSeconds(interval.intervalOnSeconds),
                limit: formatSeconds(maxDur),
                source: srcs?.maxSessionDurationSeconds ?? .global_,
                message: "Cervical VNS session duration"
                        + " \(formatSeconds(interval.intervalOnSeconds)) exceeds limit of \(formatSeconds(maxDur))."
            )
        }

        // Always note cardiac interlock is MCU-enforced
        result.addWarning(
            modality: m, param: "cardiacInterlock", displayName: "Cardiac Interlock",
            actual: "always on", limit: "non-overridable", source: .hardware,
            message: "Cervical VNS cardiac rhythm interlock is always enforced by the safety MCU."
                + " HR change >15 BPM within 5s will automatically stop stimulation."
        )
    }

    private func validateVibrotactile(
        _ p: NPVibrotactileParams,
        into result: inout NPValidationResult
    ) {
        let m = NPModalityType.vibrotactile40hz
        let lim = resolvedLimits.vibrotactile40hz
        let srcs = sourceMap.vibrotactile40hz

        // Hardware: intensity 0.6–1.2G
        if p.intensityG < NPHardwareLimits.vibrotactileMinG {
            result.addError(
                modality: m, param: "intensityG", displayName: "Intensity",
                actual: "\(p.intensityG) G",
                limit: "≥\(NPHardwareLimits.vibrotactileMinG) G",
                source: .hardware,
                message: "Vibrotactile intensity \(p.intensityG) G is below the minimum of"
                    + " \(NPHardwareLimits.vibrotactileMinG) G for effective entrainment."
            )
        }
        if p.intensityG > NPHardwareLimits.vibrotactileMaxG {
            result.addError(
                modality: m, param: "intensityG", displayName: "Intensity",
                actual: "\(p.intensityG) G",
                limit: "\(NPHardwareLimits.vibrotactileMaxG) G",
                source: .hardware,
                message: "Vibrotactile intensity \(p.intensityG) G exceeds DRV2605L"
                    + " driver maximum of \(NPHardwareLimits.vibrotactileMaxG) G."
            )
        }

        // Hardware: frequency should be 40 Hz (locked)
        if abs(p.frequencyHz - NPHardwareLimits.vibrotactileFrequencyHz) > NPHardwareLimits.vibrotactileFreqToleranceHz {
            result.addWarning(
                modality: m, param: "frequencyHz", displayName: "Frequency",
                actual: "\(formatHz(p.frequencyHz))",
                limit: "\(Int(NPHardwareLimits.vibrotactileFrequencyHz)) Hz ±\(NPHardwareLimits.vibrotactileFreqToleranceHz) Hz",
                source: .hardware,
                message: "Vibrotactile frequency \(formatHz(p.frequencyHz)) deviates from the"
                    + " firmware-locked 40 Hz ± 0.5 Hz target. Firmware will lock to 40 Hz regardless."
            )
        }

        // Dosage: max intensity
        if let maxG = lim?.maxIntensityG, p.intensityG > maxG {
            result.addError(
                modality: m, param: "intensityG", displayName: "Intensity",
                actual: "\(p.intensityG) G",
                limit: "\(maxG) G",
                source: srcs?.maxIntensityG ?? .global_,
                message: "Vibrotactile intensity \(p.intensityG) G exceeds limit of \(maxG) G."
            )
        }
    }

    // MARK: Formatting helpers

    private func formatHz(_ hz: Double) -> String {
        hz == Double(Int(hz)) ? "\(Int(hz)) Hz" : "\(hz) Hz"
    }

    private func formatSeconds(_ s: Int) -> String {
        if s < 60 { return "\(s)s" }
        let m = s / 60, sec = s % 60
        return sec == 0 ? "\(m)m" : "\(m)m \(sec)s"
    }
}
