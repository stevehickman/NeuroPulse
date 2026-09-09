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

    /// "1.0 mA (Hardware Limit)" — the limit value and where it came from.
    /// Composed through a key so a locale can reorder or re-punctuate the pair.
    static func limitLabel(_ limit: String, _ source: NPLimitSource) -> String {
        String(format: String(localized: "VALIDATE_LIMIT_WITH_SOURCE_APPLE"),
               limit, String(describing: source))
    }

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
            limitValueDescription: NPValidationResult.limitLabel(limit, source),
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
            limitValueDescription: NPValidationResult.limitLabel(limit, source),
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
                parameterDisplayName: String(localized: "VALIDATE_PARAM_MODALITIES"),
                actualValueDescription: "0",
                limitValueDescription: NPValidationResult.limitLabel(
                    String(localized: "VALIDATE_LIMIT_AT_LEAST_1"), .hardware),
                limitSource: .hardware,
                message: String(localized: "VALIDATE_MSG_GENERAL_MODALITIES")
            ))
        }

        let totalDurationSeconds = definition.totalDurationSeconds

        if let dur = totalDurationSeconds {
            // Hard error: zero or negative duration is nonsensical (ISC-47).
            if dur <= 0 {
                result.addError(
                    param: "duration", displayName: String(localized: "VALIDATE_PARAM_DURATION"),
                    actual: "\(dur)s", limit: "> 0s", source: .hardware,
                    message: String(localized: "VALIDATE_MSG_GENERAL_DURATION_3")
                )
            } else if dur < 60 {
                result.addWarning(
                    param: "duration", displayName: String(localized: "VALIDATE_PARAM_DURATION"),
                    actual: "\(dur)s", limit: "60s", source: .hardware,
                    message: String(localized: "VALIDATE_MSG_GENERAL_DURATION")
                )
            }
            if dur > 7200 {
                result.addWarning(
                    param: "duration", displayName: String(localized: "VALIDATE_PARAM_DURATION"),
                    actual: "\(dur / 60)m", limit: "120m", source: .hardware,
                    message: String(localized: "VALIDATE_MSG_GENERAL_DURATION_2")
                )
            }
        }

        // Per-modality validation (pass total duration so charge-density and dose checks work)
        for block in enabledModalities {
            validateModality(block, totalDurationSeconds: totalDurationSeconds, into: &result)
        }

        // Charge-density check for tDCS (ISC-38; model corrected by OI-CHARGE-04).
        // Charge density (µC/cm²) = I(mA) × t(s) / A(cm²), PER ELECTRODE.
        //
        // Two things changed here on 2026-09-09, and the second is the larger one:
        //
        //  1. The area is `p.electrodeAreaCm2` — declared by the protocol and carried to
        //     the safety MCU in the signed descriptor — not a global app-side assumption.
        //     tdcsDefaultElectrodeAreaCm2 is now only what a new block is seeded with.
        //  2. The denominator is ONE electrode's area, not the sum across the montage.
        //     The full session current passes through each electrode of a pair, so summing
        //     (35 × electrodeCount) modelled a current split that never happens and
        //     under-reported the density at every electrode: 2.8× permissive for one pair
        //     against the enforcer's 25 cm², 8.4× for three. Fixing only the constant
        //     would have left most of the gap in place.
        //
        // An area of 0 or less is reported by validateModality as an electrodeAreaCm2
        // error; skipping it here keeps one defect to one message instead of adding an
        // "Infinity µC/cm²" alongside it.
        if let dur = totalDurationSeconds, dur > 0 {
            for block in enabledModalities {
                if case .tdcs(let p) = block.params {
                    guard p.electrodeAreaCm2 > 0 else { continue }
                    let chargeDensity = p.intensityMilliamps * Double(dur) / p.electrodeAreaCm2
                    if chargeDensity > NPHardwareLimits.tdcsMaxChargeDensityUCcm2 {
                        result.addError(
                            modality: .tdcs,
                            param: "chargeDensityUCcm2", displayName: String(localized: "VALIDATE_PARAM_CHARGE_DENSITY"),
                            actual: String(format: "%.1f µC/cm²", chargeDensity),
                            limit: "\(Int(NPHardwareLimits.tdcsMaxChargeDensityUCcm2)) µC/cm²",
                            source: .hardware,
                            message: String(format: String(localized: "VALIDATE_MSG_TDCS_CHARGEDENSITY"),
                                            String(format: "%.1f", chargeDensity),
                                            String(Int(NPHardwareLimits.tdcsMaxChargeDensityUCcm2)))
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
                param: "cross_modality", displayName: String(localized: "VALIDATE_PARAM_CROSS_MODALITY"),
                actual: String(localized: "VALIDATE_ACTUAL_BES_TDCS_ACTIVE"),
                limit: String(localized: "VALIDATE_LIMIT_SEPARATE_PATHS"), source: .hardware,
                message: String(localized: "VALIDATE_MSG_GENERAL_CROSS_MODALITY_2")
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
                parameterDisplayName: String(localized: "VALIDATE_PARAM_LAYERS"),
                actualValueDescription: "0",
                limitValueDescription: NPValidationResult.limitLabel(
                    String(localized: "VALIDATE_LIMIT_AT_LEAST_1"), .hardware),
                limitSource: .hardware,
                message: String(localized: "VALIDATE_MSG_GENERAL_LAYERS")
            ))
        }
        for layer in c.layers {
            if let proto = library?.allProtocols.first(where: { $0.name == layer.protocolName }),
               case .single(let def) = proto {
                let sub = validate(def)
                let prefixed = sub.issues.map { issue -> NPValidationIssue in
                    var i = issue
                    i.message = String(format: String(localized: "VALIDATE_LAYER_PREFIX_APPLE"),
                                       layer.protocolName, i.message)
                    return i
                }
                result.issues.append(contentsOf: prefixed)
            } else if library != nil {
                result.issues.append(NPValidationIssue(
                    severity: .error,
                    modality: nil,
                    parameterKey: "layer_ref",
                    parameterDisplayName: String(localized: "VALIDATE_PARAM_LAYER"),
                    actualValueDescription: layer.protocolName,
                    limitValueDescription: NPValidationResult.limitLabel(
                        String(localized: "VALIDATE_LIMIT_KNOWN_PROTOCOL"), .hardware),
                    limitSource: .hardware,
                    message: String(format: String(localized: "VALIDATE_MSG_GENERAL_LAYER_REF"),
                                    layer.protocolName)
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
                        modality: m, param: "target", displayName: String(localized: "VALIDATE_PARAM_TARGET_ZONES"),
                        actual: p.target.displayName,
                        limit: String(localized: "VALIDATE_LIMIT_ONE_SOCKET"), source: .hardware,
                        message: String(
                            format: String(localized: "VALIDATE_MSG_GENERAL_TARGET_2"),
                            String(describing: p.target.displayName)
                        )
                    )
                }
            } catch {
                result.addError(
                    modality: m, param: "target", displayName: String(localized: "VALIDATE_PARAM_TARGET_ZONES"),
                    actual: p.target.displayName,
                    limit: String(localized: "VALIDATE_LIMIT_NAMED_ZONE"), source: .hardware,
                    message: error.localizedDescription
                )
            }
        }

        // Hardware: duty cycle ≤ 25%
        if p.dutyCyclePercent > NPHardwareLimits.pbmDutyCycleMaxPercent {
            result.addError(
                modality: m, param: "dutyCyclePercent", displayName: String(localized: "VALIDATE_PARAM_DUTY_CYCLE"),
                actual: "\(p.dutyCyclePercent)%",
                limit: "\(NPHardwareLimits.pbmDutyCycleMaxPercent)%",
                source: .hardware,
                message: String(
                    format: String(localized: "VALIDATE_MSG_GENERAL_DUTYCYCLEPERCENT_4"),
                    String(describing: p.dutyCyclePercent),
                    String(describing: NPHardwareLimits.pbmDutyCycleMaxPercent)
                )
            )
        }

        // Hardware: frequency cannot be negative
        if p.frequencyHz < 0 {
            result.addError(
                modality: m, param: "frequencyHz", displayName: String(localized: "VALIDATE_PARAM_FREQUENCY"),
                actual: "\(p.frequencyHz) Hz", limit: "≥0 Hz", source: .hardware,
                message: String(localized: "VALIDATE_MSG_PBM_TRANSCRANIAL_FREQUENCYHZ")
            )
        }

        // Dosage: max intensity
        if let maxI = lim?.maxIntensityPercent, p.intensityPercent > maxI {
            result.addError(
                modality: m, param: "intensityPercent", displayName: String(localized: "VALIDATE_PARAM_INTENSITY"),
                actual: "\(Int(p.intensityPercent))%",
                limit: "\(Int(maxI))%",
                source: srcs?.maxIntensityPercent ?? .global_,
                message: String(
                    format: String(localized: "VALIDATE_MSG_GENERAL_INTENSITYPERCENT_2"),
                    String(describing: Int(p.intensityPercent)),
                    String(describing: Int(maxI))
                )
            )
        }

        // Dosage: max frequency
        if let maxF = lim?.maxFrequencyHz, p.frequencyHz > maxF {
            result.addError(
                modality: m, param: "frequencyHz", displayName: String(localized: "VALIDATE_PARAM_FREQUENCY"),
                actual: "\(formatHz(p.frequencyHz))",
                limit: "\(formatHz(maxF))",
                source: srcs?.maxFrequencyHz ?? .global_,
                message: String(
                    format: String(localized: "VALIDATE_MSG_GENERAL_FREQUENCYHZ_13"),
                    String(describing: formatHz(p.frequencyHz)),
                    String(describing: formatHz(maxF))
                )
            )
        }

        // Dosage: max duty cycle (configured limit, tighter than hardware 25%)
        if let maxDC = lim?.maxDutyCyclePercent, p.dutyCyclePercent > maxDC {
            result.addError(
                modality: m, param: "dutyCyclePercent", displayName: String(localized: "VALIDATE_PARAM_DUTY_CYCLE"),
                actual: "\(p.dutyCyclePercent)%",
                limit: "\(maxDC)%",
                source: srcs?.maxDutyCyclePercent ?? .global_,
                message: String(
                    format: String(localized: "VALIDATE_MSG_GENERAL_DUTYCYCLEPERCENT_3"),
                    String(describing: p.dutyCyclePercent),
                    String(describing: maxDC)
                )
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
                    modality: m, param: "sessionDoseJCm2", displayName: String(localized: "VALIDATE_PARAM_SESSION_DOSE"),
                    actual: String(format: "%.1f J/cm²", estimatedDose),
                    limit: String(format: "%.1f J/cm²", maxDose),
                    source: srcs?.maxSessionDoseJCm2 ?? .global_,
                    message: String(format: String(localized: "VALIDATE_MSG_PBM_SESSION_DOSE"),
                                    String(format: "%.1f", estimatedDose),
                                    String(format: "%.1f", maxDose))
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
                modality: m, param: "dutyCyclePercent", displayName: String(localized: "VALIDATE_PARAM_DUTY_CYCLE"),
                actual: "\(p.dutyCyclePercent)%",
                limit: "\(NPHardwareLimits.pbmDutyCycleMaxPercent)%",
                source: .hardware,
                message: String(
                    format: String(localized: "VALIDATE_MSG_GENERAL_DUTYCYCLEPERCENT_2"),
                    String(describing: p.dutyCyclePercent),
                    String(describing: NPHardwareLimits.pbmDutyCycleMaxPercent)
                )
            )
        }

        // Dosage: max intensity
        if let maxI = lim?.maxIntensityPercent, p.intensityPercent > maxI {
            result.addError(
                modality: m, param: "intensityPercent", displayName: String(localized: "VALIDATE_PARAM_INTENSITY"),
                actual: "\(Int(p.intensityPercent))%",
                limit: "\(Int(maxI))%",
                source: srcs?.maxIntensityPercent ?? .global_,
                message: String(
                    format: String(localized: "VALIDATE_MSG_GENERAL_INTENSITYPERCENT"),
                    String(describing: Int(p.intensityPercent)),
                    String(describing: Int(maxI))
                )
            )
        }

        // Dosage: max session duration from interval
        if let maxDur = lim?.maxSessionDurationSeconds {
            let dur = interval.isContinuous ? nil : interval.intervalOnSeconds
            if let d = dur, d > maxDur {
                result.addError(
                    modality: m, param: "sessionDuration", displayName: String(localized: "VALIDATE_PARAM_SESSION_DURATION"),
                    actual: formatSeconds(d),
                    limit: formatSeconds(maxDur),
                    source: srcs?.maxSessionDurationSeconds ?? .global_,
                    message: String(
                        format: String(localized: "VALIDATE_MSG_GENERAL_SESSIONDURATION_8"),
                        String(describing: formatSeconds(d)),
                        String(describing: formatSeconds(maxDur))
                    )
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
                    modality: m, param: "band", displayName: String(localized: "VALIDATE_PARAM_EEG_BAND"),
                    actual: p.band.displayName,
                    limit: allowedBands.joined(separator: ", "),
                    source: srcs?.allowedBands ?? .global_,
                    message: String(
                        format: String(localized: "VALIDATE_MSG_GENERAL_BAND"),
                        String(describing: p.band.displayName),
                        String(describing: allowedBands.joined(separator: ", "))
                    )
                )
            }
        }

        // Dosage: require closed loop
        if let requireCL = lim?.requireClosedLoop, requireCL, !p.closedLoopEnabled {
            result.addError(
                modality: m, param: "closedLoopEnabled", displayName: String(localized: "VALIDATE_PARAM_CLOSED_LOOP"),
                actual: "disabled",
                limit: "required",
                source: srcs?.requireClosedLoop ?? .global_,
                message: String(localized: "VALIDATE_MSG_GENERAL_CLOSEDLOOPENABLED")
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
                modality: m, param: "intensityMilliamps", displayName: String(localized: "VALIDATE_PARAM_INTENSITY"),
                actual: "\(p.intensityMilliamps) mA",
                limit: "\(NPHardwareLimits.besTacsMaxMilliamps) mA",
                source: .hardware,
                message: String(
                    format: String(localized: "VALIDATE_MSG_GENERAL_INTENSITYMILLIAMPS_13"),
                    String(describing: p.intensityMilliamps),
                    String(describing: NPHardwareLimits.besTacsMaxMilliamps)
                )
            )
        }

        // Hardware: frequency 0.5–40 Hz
        if p.frequencyHz < NPHardwareLimits.besTacsMinHz {
            result.addError(
                modality: m, param: "frequencyHz", displayName: String(localized: "VALIDATE_PARAM_FREQUENCY"),
                actual: "\(formatHz(p.frequencyHz))",
                limit: "≥\(formatHz(NPHardwareLimits.besTacsMinHz))",
                source: .hardware,
                message: String(
                    format: String(localized: "VALIDATE_MSG_GENERAL_FREQUENCYHZ_12"),
                    String(describing: formatHz(p.frequencyHz)),
                    String(describing: formatHz(NPHardwareLimits.besTacsMinHz))
                )
            )
        }
        if p.frequencyHz > NPHardwareLimits.besTacsMaxHz {
            result.addError(
                modality: m, param: "frequencyHz", displayName: String(localized: "VALIDATE_PARAM_FREQUENCY"),
                actual: "\(formatHz(p.frequencyHz))",
                limit: "≤\(formatHz(NPHardwareLimits.besTacsMaxHz))",
                source: .hardware,
                message: String(
                    format: String(localized: "VALIDATE_MSG_GENERAL_FREQUENCYHZ_11"),
                    String(describing: formatHz(p.frequencyHz)),
                    String(describing: formatHz(NPHardwareLimits.besTacsMaxHz))
                )
            )
        }

        // Dosage: max intensity
        if let maxI = lim?.maxIntensityMilliamps, p.intensityMilliamps > maxI {
            result.addError(
                modality: m, param: "intensityMilliamps", displayName: String(localized: "VALIDATE_PARAM_INTENSITY"),
                actual: "\(p.intensityMilliamps) mA",
                limit: "\(maxI) mA",
                source: srcs?.maxIntensityMilliamps ?? .global_,
                message: String(
                    format: String(localized: "VALIDATE_MSG_GENERAL_INTENSITYMILLIAMPS_12"),
                    String(describing: p.intensityMilliamps),
                    String(describing: maxI)
                )
            )
        }

        // Dosage: max frequency
        if let maxF = lim?.maxFrequencyHz, p.frequencyHz > maxF {
            result.addError(
                modality: m, param: "frequencyHz", displayName: String(localized: "VALIDATE_PARAM_FREQUENCY"),
                actual: "\(formatHz(p.frequencyHz))",
                limit: "\(formatHz(maxF))",
                source: srcs?.maxFrequencyHz ?? .global_,
                message: String(
                    format: String(localized: "VALIDATE_MSG_GENERAL_FREQUENCYHZ_10"),
                    String(describing: formatHz(p.frequencyHz)),
                    String(describing: formatHz(maxF))
                )
            )
        }

        // Dosage: min frequency
        if let minF = lim?.minFrequencyHz, p.frequencyHz < minF {
            result.addError(
                modality: m, param: "frequencyHz", displayName: String(localized: "VALIDATE_PARAM_FREQUENCY"),
                actual: "\(formatHz(p.frequencyHz))",
                limit: "≥\(formatHz(minF))",
                source: srcs?.minFrequencyHz ?? .global_,
                message: String(
                    format: String(localized: "VALIDATE_MSG_GENERAL_FREQUENCYHZ_9"),
                    String(describing: formatHz(p.frequencyHz)),
                    String(describing: formatHz(minF))
                )
            )
        }

        // Dosage: session duration
        if let maxDur = lim?.maxSessionDurationSeconds, !interval.isContinuous {
            if interval.intervalOnSeconds > maxDur {
                result.addError(
                    modality: m, param: "sessionDuration", displayName: String(localized: "VALIDATE_PARAM_SESSION_DURATION"),
                    actual: formatSeconds(interval.intervalOnSeconds),
                    limit: formatSeconds(maxDur),
                    source: srcs?.maxSessionDurationSeconds ?? .global_,
                    message: String(
                        format: String(localized: "VALIDATE_MSG_GENERAL_SESSIONDURATION_7"),
                        String(describing: formatSeconds(interval.intervalOnSeconds)),
                        String(describing: formatSeconds(maxDur))
                    )
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
                modality: m, param: "intensityMilliamps", displayName: String(localized: "VALIDATE_PARAM_INTENSITY"),
                actual: "\(p.intensityMilliamps) mA",
                limit: "≥\(NPHardwareLimits.tdcsMinMilliamps) mA",
                source: .hardware,
                message: String(
                    format: String(localized: "VALIDATE_MSG_GENERAL_INTENSITYMILLIAMPS_11"),
                    String(describing: p.intensityMilliamps),
                    String(describing: NPHardwareLimits.tdcsMinMilliamps)
                )
            )
        }
        if p.intensityMilliamps > NPHardwareLimits.tdcsMaxMilliamps {
            result.addError(
                modality: m, param: "intensityMilliamps", displayName: String(localized: "VALIDATE_PARAM_INTENSITY"),
                actual: "\(p.intensityMilliamps) mA",
                limit: "\(NPHardwareLimits.tdcsMaxMilliamps) mA",
                source: .hardware,
                message: String(
                    format: String(localized: "VALIDATE_MSG_GENERAL_INTENSITYMILLIAMPS_10"),
                    String(describing: p.intensityMilliamps),
                    String(describing: NPHardwareLimits.tdcsMaxMilliamps)
                )
            )
        }

        // Hardware: electrode pairs ≤ 3
        if p.electrodePairs.count > NPHardwareLimits.tdcsMaxElectrodePairs {
            result.addError(
                modality: m, param: "electrodePairs", displayName: String(localized: "VALIDATE_PARAM_ELECTRODE_PAIRS"),
                actual: "\(p.electrodePairs.count)",
                limit: "\(NPHardwareLimits.tdcsMaxElectrodePairs)",
                source: .hardware,
                message: String(
                    format: String(localized: "VALIDATE_MSG_GENERAL_ELECTRODEPAIRS"),
                    String(describing: p.electrodePairs.count),
                    String(describing: NPHardwareLimits.tdcsMaxElectrodePairs)
                )
            )
        }

        // Hardware: declared per-electrode pad geometry (OI-CHARGE-04).
        // An undeclared area is an error, not a fallback: the hub refuses a zero area and
        // the safety MCU's geometry gate holds tDCS out of granted_mask, so a protocol
        // that reaches the device without one simply never stimulates.
        if !(p.electrodeAreaCm2 > 0) || p.electrodeAreaCm2 > NPHardwareLimits.tdcsMaxElectrodeAreaCm2 {
            result.addError(
                modality: m, param: "electrodeAreaCm2", displayName: String(localized: "VALIDATE_PARAM_ELECTRODE_AREA"),
                actual: "\(p.electrodeAreaCm2) cm²",
                limit: "0 < A ≤ \(NPHardwareLimits.tdcsMaxElectrodeAreaCm2) cm²",
                source: .hardware,
                message: String(
                    format: String(localized: "VALIDATE_MSG_TDCS_ELECTRODEAREA"),
                    String(describing: p.electrodeAreaCm2),
                    String(describing: NPHardwareLimits.tdcsMaxElectrodeAreaCm2)
                )
            )
        }

        // Dosage: max intensity
        if let maxI = lim?.maxIntensityMilliamps, p.intensityMilliamps > maxI {
            result.addError(
                modality: m, param: "intensityMilliamps", displayName: String(localized: "VALIDATE_PARAM_INTENSITY"),
                actual: "\(p.intensityMilliamps) mA",
                limit: "\(maxI) mA",
                source: srcs?.maxIntensityMilliamps ?? .global_,
                message: String(
                    format: String(localized: "VALIDATE_MSG_GENERAL_INTENSITYMILLIAMPS_9"),
                    String(describing: p.intensityMilliamps),
                    String(describing: maxI)
                )
            )
        }

        // Dosage: session duration
        if let maxDur = lim?.maxSessionDurationSeconds, !interval.isContinuous, interval.intervalOnSeconds > maxDur {
            result.addError(
                modality: m, param: "sessionDuration", displayName: String(localized: "VALIDATE_PARAM_SESSION_DURATION"),
                actual: formatSeconds(interval.intervalOnSeconds),
                limit: formatSeconds(maxDur),
                source: srcs?.maxSessionDurationSeconds ?? .global_,
                message: String(
                    format: String(localized: "VALIDATE_MSG_GENERAL_SESSIONDURATION_6"),
                    String(describing: formatSeconds(interval.intervalOnSeconds)),
                    String(describing: formatSeconds(maxDur))
                )
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
                modality: m, param: "intensityMilliamps", displayName: String(localized: "VALIDATE_PARAM_INTENSITY"),
                actual: "\(p.intensityMilliamps) mA",
                limit: "\(NPHardwareLimits.vnsMaxMilliamps) mA",
                source: .hardware,
                message: String(
                    format: String(localized: "VALIDATE_MSG_GENERAL_INTENSITYMILLIAMPS_8"),
                    String(describing: p.intensityMilliamps),
                    String(describing: NPHardwareLimits.vnsMaxMilliamps)
                )
            )
        }

        // Hardware: frequency 1–25 Hz
        if p.frequencyHz < NPHardwareLimits.vnsMinHz {
            result.addError(
                modality: m, param: "frequencyHz", displayName: String(localized: "VALIDATE_PARAM_FREQUENCY"),
                actual: "\(formatHz(p.frequencyHz))",
                limit: "≥\(formatHz(NPHardwareLimits.vnsMinHz))",
                source: .hardware,
                message: String(
                    format: String(localized: "VALIDATE_MSG_GENERAL_FREQUENCYHZ_8"),
                    String(describing: formatHz(p.frequencyHz)),
                    String(describing: formatHz(NPHardwareLimits.vnsMinHz))
                )
            )
        }
        if p.frequencyHz > NPHardwareLimits.vnsMaxHz {
            result.addError(
                modality: m, param: "frequencyHz", displayName: String(localized: "VALIDATE_PARAM_FREQUENCY"),
                actual: "\(formatHz(p.frequencyHz))",
                limit: "≤\(formatHz(NPHardwareLimits.vnsMaxHz))",
                source: .hardware,
                message: String(
                    format: String(localized: "VALIDATE_MSG_GENERAL_FREQUENCYHZ_7"),
                    String(describing: formatHz(p.frequencyHz)),
                    String(describing: formatHz(NPHardwareLimits.vnsMaxHz))
                )
            )
        }

        // Dosage: max intensity
        if let maxI = lim?.maxIntensityMilliamps, p.intensityMilliamps > maxI {
            result.addError(
                modality: m, param: "intensityMilliamps", displayName: String(localized: "VALIDATE_PARAM_INTENSITY"),
                actual: "\(p.intensityMilliamps) mA",
                limit: "\(maxI) mA",
                source: srcs?.maxIntensityMilliamps ?? .global_,
                message: String(
                    format: String(localized: "VALIDATE_MSG_GENERAL_INTENSITYMILLIAMPS_7"),
                    String(describing: p.intensityMilliamps),
                    String(describing: maxI)
                )
            )
        }

        // Dosage: max frequency
        if let maxF = lim?.maxFrequencyHz, p.frequencyHz > maxF {
            result.addError(
                modality: m, param: "frequencyHz", displayName: String(localized: "VALIDATE_PARAM_FREQUENCY"),
                actual: "\(formatHz(p.frequencyHz))",
                limit: "\(formatHz(maxF))",
                source: srcs?.maxFrequencyHz ?? .global_,
                message: String(
                    format: String(localized: "VALIDATE_MSG_GENERAL_FREQUENCYHZ_6"),
                    String(describing: formatHz(p.frequencyHz)),
                    String(describing: formatHz(maxF))
                )
            )
        }

        // Dosage: session duration
        if let maxDur = lim?.maxSessionDurationSeconds, !interval.isContinuous, interval.intervalOnSeconds > maxDur {
            result.addError(
                modality: m, param: "sessionDuration", displayName: String(localized: "VALIDATE_PARAM_SESSION_DURATION"),
                actual: formatSeconds(interval.intervalOnSeconds),
                limit: formatSeconds(maxDur),
                source: srcs?.maxSessionDurationSeconds ?? .global_,
                message: String(
                    format: String(localized: "VALIDATE_MSG_GENERAL_SESSIONDURATION_5"),
                    String(describing: formatSeconds(interval.intervalOnSeconds)),
                    String(describing: formatSeconds(maxDur))
                )
            )
        }

        // Dosage: allowed protocols whitelist
        if let allowed = lim?.allowedProtocols {
            let protoRaw = p.hrvProtocol.rawValue
            if !allowed.contains(protoRaw) {
                result.addError(
                    modality: m, param: "hrvProtocol", displayName: String(localized: "VALIDATE_PARAM_HRV_PROTOCOL"),
                    actual: p.hrvProtocol.displayName,
                    limit: allowed.joined(separator: ", "),
                    source: srcs?.allowedProtocols ?? .global_,
                    message: String(
                        format: String(localized: "VALIDATE_MSG_GENERAL_HRVPROTOCOL"),
                        String(describing: p.hrvProtocol.displayName)
                    )
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
                modality: m, param: "volumePercent", displayName: String(localized: "VALIDATE_PARAM_VOLUME"),
                actual: "\(Int(p.volumePercent))%",
                limit: "\(Int(maxVol))%",
                source: srcs?.maxVolumePercent ?? .global_,
                message: String(
                    format: String(localized: "VALIDATE_MSG_GENERAL_VOLUMEPERCENT"),
                    String(describing: Int(p.volumePercent)),
                    String(describing: Int(maxVol))
                )
            )
        }

        // Dosage: max binaural beats Hz
        if let maxBB = lim?.maxBinauralBeatsHz, let bb = p.binauralBeatsHz, bb > maxBB {
            result.addError(
                modality: m, param: "binauralBeatsHz", displayName: String(localized: "VALIDATE_PARAM_BINAURAL_BEATS"),
                actual: "\(formatHz(bb))",
                limit: "\(formatHz(maxBB))",
                source: srcs?.maxBinauralBeatsHz ?? .global_,
                message: String(
                    format: String(localized: "VALIDATE_MSG_GENERAL_BINAURALBEATSHZ"),
                    String(describing: formatHz(bb)),
                    String(describing: formatHz(maxBB))
                )
            )
        }

        // Dosage: max isochronic tones Hz
        if let maxIT = lim?.maxIsochronicTonesHz, let it = p.isochronicTonesHz, it > maxIT {
            result.addError(
                modality: m, param: "isochronicTonesHz", displayName: String(localized: "VALIDATE_PARAM_ISOCHRONIC_TONES"),
                actual: "\(formatHz(it))",
                limit: "\(formatHz(maxIT))",
                source: srcs?.maxIsochronicTonesHz ?? .global_,
                message: String(
                    format: String(localized: "VALIDATE_MSG_GENERAL_ISOCHRONICTONESHZ"),
                    String(describing: formatHz(it)),
                    String(describing: formatHz(maxIT))
                )
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
                modality: m, param: "frequencyHz", displayName: String(localized: "VALIDATE_PARAM_FREQUENCY"),
                actual: "\(formatHz(p.frequencyHz))",
                limit: "\(formatHz(NPHardwareLimits.visualMaxHz))",
                source: .hardware,
                message: String(
                    format: String(localized: "VALIDATE_MSG_GENERAL_FREQUENCYHZ_5"),
                    String(describing: formatHz(p.frequencyHz)),
                    String(describing: formatHz(NPHardwareLimits.visualMaxHz))
                )
            )
        }

        // Photoparoxysmal risk zone: 3–30 Hz
        // Default: warning. If blockHighRiskRange is true in limits: error.
        if p.frequencyHz >= NPHardwareLimits.visualHighRiskMinHz && p.frequencyHz <= NPHardwareLimits.visualHighRiskMaxHz {
            let blockRange = lim?.blockHighRiskRange ?? false
            let msg = String(format: String(localized: "VALIDATE_MSG_VISUAL_HIGH_RISK"),
                             formatHz(p.frequencyHz),
                             String(Int(NPHardwareLimits.visualHighRiskMinHz)),
                             String(Int(NPHardwareLimits.visualHighRiskMaxHz)))
            if blockRange {
                result.addError(
                    modality: m, param: "frequencyHz", displayName: String(localized: "VALIDATE_PARAM_FREQUENCY"),
                    actual: "\(formatHz(p.frequencyHz))",
                    limit: "Outside \(Int(NPHardwareLimits.visualHighRiskMinHz))–\(Int(NPHardwareLimits.visualHighRiskMaxHz)) Hz",
                    source: srcs?.blockHighRiskRange ?? .global_,
                    message: msg
                )
            } else {
                result.addWarning(
                    modality: m, param: "frequencyHz", displayName: String(localized: "VALIDATE_PARAM_FREQUENCY"),
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
                modality: m, param: "frequencyHz", displayName: String(localized: "VALIDATE_PARAM_FREQUENCY"),
                actual: "\(formatHz(p.frequencyHz))",
                limit: "\(formatHz(maxF))",
                source: srcs?.maxFrequencyHz ?? .global_,
                message: String(
                    format: String(localized: "VALIDATE_MSG_GENERAL_FREQUENCYHZ_4"),
                    String(describing: formatHz(p.frequencyHz)),
                    String(describing: formatHz(maxF))
                )
            )
        }

        // Dosage: min frequency
        if let minF = lim?.minFrequencyHz, p.frequencyHz < minF {
            result.addError(
                modality: m, param: "frequencyHz", displayName: String(localized: "VALIDATE_PARAM_FREQUENCY"),
                actual: "\(formatHz(p.frequencyHz))",
                limit: "≥\(formatHz(minF))",
                source: srcs?.minFrequencyHz ?? .global_,
                message: String(
                    format: String(localized: "VALIDATE_MSG_GENERAL_FREQUENCYHZ_3"),
                    String(describing: formatHz(p.frequencyHz)),
                    String(describing: formatHz(minF))
                )
            )
        }

        // Dosage: allowed modes whitelist
        if let allowedModes = lim?.allowedModes {
            let modeRaw = p.mode.rawValue
            if !allowedModes.contains(modeRaw) {
                result.addError(
                    modality: m, param: "mode", displayName: String(localized: "VALIDATE_PARAM_VISUAL_MODE"),
                    actual: p.mode.displayName,
                    limit: allowedModes.joined(separator: ", "),
                    source: srcs?.allowedModes ?? .global_,
                    message: String(
                        format: String(localized: "VALIDATE_MSG_GENERAL_MODE"),
                        String(describing: p.mode.displayName)
                    )
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
                modality: m, param: "intensityPercentMT", displayName: String(localized: "VALIDATE_PARAM_INTENSITY_MT"),
                actual: "\(p.intensityPercentMT)% MT",
                limit: "\(maxMT)% MT",
                source: srcs?.maxIntensityPercentMT ?? .global_,
                message: String(
                    format: String(localized: "VALIDATE_MSG_GENERAL_INTENSITYPERCENTMT_2"),
                    String(describing: p.intensityPercentMT),
                    String(describing: maxMT)
                )
            )
        }

        // Dosage: max pulses per session
        if let maxPulses = lim?.maxPulsesPerSession, p.pulseCount > maxPulses {
            result.addError(
                modality: m, param: "pulseCount", displayName: String(localized: "VALIDATE_PARAM_PULSE_COUNT"),
                actual: "\(p.pulseCount) pulses",
                limit: "\(maxPulses) pulses",
                source: srcs?.maxPulsesPerSession ?? .global_,
                message: String(
                    format: String(localized: "VALIDATE_MSG_GENERAL_PULSECOUNT"),
                    String(describing: p.pulseCount),
                    String(describing: maxPulses)
                )
            )
        }

        // Dosage: allowed protocols whitelist
        if let allowedProtos = lim?.allowedProtocols {
            let protoRaw = p.tmsProtocol.rawValue
            if !allowedProtos.contains(protoRaw) {
                result.addError(
                    modality: m, param: "tmsProtocol", displayName: String(localized: "VALIDATE_PARAM_TMS_PROTOCOL"),
                    actual: p.tmsProtocol.displayName,
                    limit: allowedProtos.joined(separator: ", "),
                    source: srcs?.allowedProtocols ?? .global_,
                    message: String(
                        format: String(localized: "VALIDATE_MSG_GENERAL_TMSPROTOCOL"),
                        String(describing: p.tmsProtocol.displayName)
                    )
                )
            }
        }

        // Dosage: allowed targets whitelist
        if let allowedTargets = lim?.allowedTargets {
            let targetRaw = p.target.rawValue
            if !allowedTargets.contains(targetRaw) {
                result.addError(
                    modality: m, param: "target", displayName: String(localized: "VALIDATE_PARAM_TMS_TARGET"),
                    actual: p.target.displayName,
                    limit: allowedTargets.joined(separator: ", "),
                    source: srcs?.allowedTargets ?? .global_,
                    message: String(
                        format: String(localized: "VALIDATE_MSG_GENERAL_TARGET"),
                        String(describing: p.target.displayName)
                    )
                )
            }
        }

        // Warn: TMS at >120% MT is aggressive
        if p.intensityPercentMT > 120 {
            result.addWarning(
                modality: m, param: "intensityPercentMT", displayName: String(localized: "VALIDATE_PARAM_INTENSITY_MT"),
                actual: "\(p.intensityPercentMT)% MT",
                limit: "≤120% MT",
                source: .hardware,
                message: String(
                    format: String(localized: "VALIDATE_MSG_GENERAL_INTENSITYPERCENTMT"),
                    String(describing: p.intensityPercentMT)
                )
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
                modality: m, param: "intensityMWcm2", displayName: String(localized: "VALIDATE_PARAM_INTENSITY"),
                actual: "\(Int(p.intensityMWcm2)) mW/cm²",
                limit: "\(Int(NPHardwareLimits.deepPBMMaxMWcm2)) mW/cm²",
                source: .hardware,
                message: String(
                    format: String(localized: "VALIDATE_MSG_GENERAL_INTENSITYMWCM2_2"),
                    String(describing: Int(p.intensityMWcm2)),
                    String(describing: Int(NPHardwareLimits.deepPBMMaxMWcm2))
                )
            )
        }

        // Hardware: duty cycle ≤ 25%
        if p.dutyCyclePercent > NPHardwareLimits.pbmDutyCycleMaxPercent {
            result.addError(
                modality: m, param: "dutyCyclePercent", displayName: String(localized: "VALIDATE_PARAM_DUTY_CYCLE"),
                actual: "\(p.dutyCyclePercent)%",
                limit: "\(NPHardwareLimits.pbmDutyCycleMaxPercent)%",
                source: .hardware,
                message: String(
                    format: String(localized: "VALIDATE_MSG_GENERAL_DUTYCYCLEPERCENT"),
                    String(describing: p.dutyCyclePercent),
                    String(describing: NPHardwareLimits.pbmDutyCycleMaxPercent)
                )
            )
        }

        // Dosage: max intensity
        if let maxI = lim?.maxIntensityMWcm2, p.intensityMWcm2 > maxI {
            result.addError(
                modality: m, param: "intensityMWcm2", displayName: String(localized: "VALIDATE_PARAM_INTENSITY"),
                actual: "\(Int(p.intensityMWcm2)) mW/cm²",
                limit: "\(Int(maxI)) mW/cm²",
                source: srcs?.maxIntensityMWcm2 ?? .global_,
                message: String(
                    format: String(localized: "VALIDATE_MSG_GENERAL_INTENSITYMWCM2"),
                    String(describing: Int(p.intensityMWcm2)),
                    String(describing: Int(maxI))
                )
            )
        }

        // Dosage: session duration
        if let maxDur = lim?.maxSessionDurationSeconds, !interval.isContinuous, interval.intervalOnSeconds > maxDur {
            result.addError(
                modality: m, param: "sessionDuration", displayName: String(localized: "VALIDATE_PARAM_SESSION_DURATION"),
                actual: formatSeconds(interval.intervalOnSeconds),
                limit: formatSeconds(maxDur),
                source: srcs?.maxSessionDurationSeconds ?? .global_,
                message: String(
                    format: String(localized: "VALIDATE_MSG_GENERAL_SESSIONDURATION_4"),
                    String(describing: formatSeconds(interval.intervalOnSeconds)),
                    String(describing: formatSeconds(maxDur))
                )
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
                modality: m, param: "intensityMilliamps", displayName: String(localized: "VALIDATE_PARAM_INTENSITY"),
                actual: "\(p.intensityMilliamps) mA",
                limit: "\(NPHardwareLimits.clinicalTacsMaxMilliamps) mA",
                source: .hardware,
                message: String(
                    format: String(localized: "VALIDATE_MSG_GENERAL_INTENSITYMILLIAMPS_6"),
                    String(describing: p.intensityMilliamps),
                    String(describing: NPHardwareLimits.clinicalTacsMaxMilliamps)
                )
            )
        }

        // Hardware: channel count, 1...21 — OI-TACS-01.
        // Until 2026-09-07 nothing checked this and the hub encoder silently clamped
        // an over-range count to the 16-bit wire mask. The mask now spans the
        // driver's full 21 channels, and an out-of-range count is reported rather
        // than quietly reduced.
        if p.channelCount < 1 || p.channelCount > NPHardwareLimits.clinicalTacsMaxChannels {
            result.addError(
                modality: m, param: "channelCount", displayName: String(localized: "VALIDATE_PARAM_CHANNEL_COUNT"),
                actual: "\(p.channelCount)",
                limit: "1–\(NPHardwareLimits.clinicalTacsMaxChannels)",
                source: .hardware,
                message: String(
                    format: String(localized: "VALIDATE_MSG_CLINICAL_TACS_CHANNELCOUNT"),
                    String(describing: p.channelCount),
                    String(describing: NPHardwareLimits.clinicalTacsMaxChannels)
                )
            )
        }

        // Dosage: max intensity
        if let maxI = lim?.maxIntensityMilliamps, p.intensityMilliamps > maxI {
            result.addError(
                modality: m, param: "intensityMilliamps", displayName: String(localized: "VALIDATE_PARAM_INTENSITY"),
                actual: "\(p.intensityMilliamps) mA",
                limit: "\(maxI) mA",
                source: srcs?.maxIntensityMilliamps ?? .global_,
                message: String(
                    format: String(localized: "VALIDATE_MSG_GENERAL_INTENSITYMILLIAMPS_5"),
                    String(describing: p.intensityMilliamps),
                    String(describing: maxI)
                )
            )
        }

        // Dosage: session duration
        if let maxDur = lim?.maxSessionDurationSeconds, !interval.isContinuous, interval.intervalOnSeconds > maxDur {
            result.addError(
                modality: m, param: "sessionDuration", displayName: String(localized: "VALIDATE_PARAM_SESSION_DURATION"),
                actual: formatSeconds(interval.intervalOnSeconds),
                limit: formatSeconds(maxDur),
                source: srcs?.maxSessionDurationSeconds ?? .global_,
                message: String(
                    format: String(localized: "VALIDATE_MSG_GENERAL_SESSIONDURATION_3"),
                    String(describing: formatSeconds(interval.intervalOnSeconds)),
                    String(describing: formatSeconds(maxDur))
                )
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
                modality: m, param: "intensityMilliamps", displayName: String(localized: "VALIDATE_PARAM_INTENSITY"),
                actual: "\(p.intensityMilliamps) mA",
                limit: "\(NPHardwareLimits.hdTdcsMaxMilliampsPerElectrode) mA",
                source: .hardware,
                message: String(
                    format: String(localized: "VALIDATE_MSG_GENERAL_INTENSITYMILLIAMPS_4"),
                    String(describing: p.intensityMilliamps),
                    String(describing: NPHardwareLimits.hdTdcsMaxMilliampsPerElectrode)
                )
            )
        }

        // Dosage: max intensity
        if let maxI = lim?.maxIntensityMilliamps, p.intensityMilliamps > maxI {
            result.addError(
                modality: m, param: "intensityMilliamps", displayName: String(localized: "VALIDATE_PARAM_INTENSITY"),
                actual: "\(p.intensityMilliamps) mA",
                limit: "\(maxI) mA",
                source: srcs?.maxIntensityMilliamps ?? .global_,
                message: String(
                    format: String(localized: "VALIDATE_MSG_GENERAL_INTENSITYMILLIAMPS_3"),
                    String(describing: p.intensityMilliamps),
                    String(describing: maxI)
                )
            )
        }

        // Dosage: session duration
        if let maxDur = lim?.maxSessionDurationSeconds, !interval.isContinuous, interval.intervalOnSeconds > maxDur {
            result.addError(
                modality: m, param: "sessionDuration", displayName: String(localized: "VALIDATE_PARAM_SESSION_DURATION"),
                actual: formatSeconds(interval.intervalOnSeconds),
                limit: formatSeconds(maxDur),
                source: srcs?.maxSessionDurationSeconds ?? .global_,
                message: String(
                    format: String(localized: "VALIDATE_MSG_GENERAL_SESSIONDURATION_2"),
                    String(describing: formatSeconds(interval.intervalOnSeconds)),
                    String(describing: formatSeconds(maxDur))
                )
            )
        }

        // Dosage: allowed montages whitelist
        if let allowedMontages = lim?.allowedMontages {
            let montageRaw = p.montage.rawValue
            if !allowedMontages.contains(montageRaw) {
                result.addError(
                    modality: m, param: "montage", displayName: String(localized: "VALIDATE_PARAM_MONTAGE"),
                    actual: p.montage.displayName,
                    limit: allowedMontages.joined(separator: ", "),
                    source: srcs?.allowedMontages ?? .global_,
                    message: String(
                        format: String(localized: "VALIDATE_MSG_GENERAL_MONTAGE"),
                        String(describing: p.montage.displayName)
                    )
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
                modality: m, param: "intensityMilliamps", displayName: String(localized: "VALIDATE_PARAM_INTENSITY"),
                actual: "\(p.intensityMilliamps) mA",
                limit: "\(NPHardwareLimits.cervicalVnsMaxMilliamps) mA",
                source: .hardware,
                message: String(
                    format: String(localized: "VALIDATE_MSG_GENERAL_INTENSITYMILLIAMPS_2"),
                    String(describing: p.intensityMilliamps),
                    String(describing: NPHardwareLimits.cervicalVnsMaxMilliamps)
                )
            )
        }

        // Hardware: frequency in VNS range 1–25 Hz
        if p.frequencyHz < NPHardwareLimits.vnsMinHz || p.frequencyHz > NPHardwareLimits.vnsMaxHz {
            result.addError(
                modality: m, param: "frequencyHz", displayName: String(localized: "VALIDATE_PARAM_FREQUENCY"),
                actual: "\(formatHz(p.frequencyHz))",
                limit: "\(formatHz(NPHardwareLimits.vnsMinHz))–\(formatHz(NPHardwareLimits.vnsMaxHz))",
                source: .hardware,
                message: String(
                    format: String(localized: "VALIDATE_MSG_GENERAL_FREQUENCYHZ_2"),
                    String(describing: formatHz(p.frequencyHz)),
                    String(describing: formatHz(NPHardwareLimits.vnsMinHz)),
                    String(describing: formatHz(NPHardwareLimits.vnsMaxHz))
                )
            )
        }

        // Dosage: max intensity
        if let maxI = lim?.maxIntensityMilliamps, p.intensityMilliamps > maxI {
            result.addError(
                modality: m, param: "intensityMilliamps", displayName: String(localized: "VALIDATE_PARAM_INTENSITY"),
                actual: "\(p.intensityMilliamps) mA",
                limit: "\(maxI) mA",
                source: srcs?.maxIntensityMilliamps ?? .global_,
                message: String(
                    format: String(localized: "VALIDATE_MSG_GENERAL_INTENSITYMILLIAMPS"),
                    String(describing: p.intensityMilliamps),
                    String(describing: maxI)
                )
            )
        }

        // Dosage: session duration
        if let maxDur = lim?.maxSessionDurationSeconds, !interval.isContinuous, interval.intervalOnSeconds > maxDur {
            result.addError(
                modality: m, param: "sessionDuration", displayName: String(localized: "VALIDATE_PARAM_SESSION_DURATION"),
                actual: formatSeconds(interval.intervalOnSeconds),
                limit: formatSeconds(maxDur),
                source: srcs?.maxSessionDurationSeconds ?? .global_,
                message: String(
                    format: String(localized: "VALIDATE_MSG_GENERAL_SESSIONDURATION"),
                    String(describing: formatSeconds(interval.intervalOnSeconds)),
                    String(describing: formatSeconds(maxDur))
                )
            )
        }

        // Always note cardiac interlock is MCU-enforced
        result.addWarning(
            modality: m, param: "cardiacInterlock", displayName: String(localized: "VALIDATE_PARAM_CARDIAC_INTERLOCK"),
            actual: String(localized: "VALIDATE_ACTUAL_ALWAYS_ON"),
            limit: String(localized: "VALIDATE_LIMIT_NON_OVERRIDABLE"), source: .hardware,
            message: String(localized: "VALIDATE_MSG_GENERAL_CARDIACINTERLOCK")
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
                modality: m, param: "intensityG", displayName: String(localized: "VALIDATE_PARAM_INTENSITY"),
                actual: "\(p.intensityG) G",
                limit: "≥\(NPHardwareLimits.vibrotactileMinG) G",
                source: .hardware,
                message: String(
                    format: String(localized: "VALIDATE_MSG_GENERAL_INTENSITYG_3"),
                    String(describing: p.intensityG),
                    String(describing: NPHardwareLimits.vibrotactileMinG)
                )
            )
        }
        if p.intensityG > NPHardwareLimits.vibrotactileMaxG {
            result.addError(
                modality: m, param: "intensityG", displayName: String(localized: "VALIDATE_PARAM_INTENSITY"),
                actual: "\(p.intensityG) G",
                limit: "\(NPHardwareLimits.vibrotactileMaxG) G",
                source: .hardware,
                message: String(
                    format: String(localized: "VALIDATE_MSG_GENERAL_INTENSITYG_2"),
                    String(describing: p.intensityG),
                    String(describing: NPHardwareLimits.vibrotactileMaxG)
                )
            )
        }

        // Hardware: frequency should be 40 Hz (locked)
        if abs(p.frequencyHz - NPHardwareLimits.vibrotactileFrequencyHz) > NPHardwareLimits.vibrotactileFreqToleranceHz {
            result.addWarning(
                modality: m, param: "frequencyHz", displayName: String(localized: "VALIDATE_PARAM_FREQUENCY"),
                actual: "\(formatHz(p.frequencyHz))",
                limit: "\(Int(NPHardwareLimits.vibrotactileFrequencyHz)) Hz ±\(NPHardwareLimits.vibrotactileFreqToleranceHz) Hz",
                source: .hardware,
                message: String(
                    format: String(localized: "VALIDATE_MSG_GENERAL_FREQUENCYHZ"),
                    String(describing: formatHz(p.frequencyHz))
                )
            )
        }

        // Dosage: max intensity
        if let maxG = lim?.maxIntensityG, p.intensityG > maxG {
            result.addError(
                modality: m, param: "intensityG", displayName: String(localized: "VALIDATE_PARAM_INTENSITY"),
                actual: "\(p.intensityG) G",
                limit: "\(maxG) G",
                source: srcs?.maxIntensityG ?? .global_,
                message: String(
                    format: String(localized: "VALIDATE_MSG_GENERAL_INTENSITYG"),
                    String(describing: p.intensityG),
                    String(describing: maxG)
                )
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
