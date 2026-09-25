//
//  NPProtocolValidatorTests.swift
//  NeurOneTests
//
//  Satisfies: ISC-151 (protocol validator rejects out-of-bounds stimulation parameters),
//             ISC-157 (test target imports no production analytics/crash-reporting module).
//
//  Subject under test: NPProtocolValidator (app/ios/NeurOne/Protocol/NPProtocolValidator.swift)
//
//  Validator coverage (all gaps now closed — ISC-37, ISC-38, ISC-47):
//    (a) DC per-session charge density (mC/cm²) — implemented 2026-06-04, model corrected
//        2026-09-09 by OI-CHARGE-04, ceiling re-sourced 2026-09-15 by OI-CHARGE-05.
//        Formula: I(mA) × t(s) / A(cm²) where A is the area of ONE electrode, declared by
//        the protocol as NPTDCSParams.electrodeAreaCm2 and carried to the safety MCU in the
//        signed descriptor. Ceiling: NPHardwareLimits.tdcsMaxSessionChargeDensityMCcm2 (150),
//        renamed from tdcsMaxChargeDensityUCcm2 (40) — the old name's unit was wrong by
//        1000× and its missing period hid that a per-PHASE pulsed figure was being applied
//        to a DC session dose. The comparison is >=, matching the safety MCU's comparator.
//        Until the correction this divided by tdcsDefaultElectrodeAreaCm2 × the count of every
//        electrode in the montage — an app-side assumption the enforcer never saw, and a sum
//        that under-reported the density at each electrode (70 cm² for one pair against the
//        MCU's 25). The pre-flight was 2.8× more permissive than the thing that stops the
//        session; these tests now pin the per-electrode model.
//    (b) PBM session dose (J/cm²) — implemented 2026-06-04. Estimated dose formula:
//        peakMWcm2 × intensityFraction × duration(s) / 1000. CW: pbmCWMaxMWcm2; pulsed: scaled
//        by duty cycle. Checked against NPPBMTranscranialLimits.maxSessionDoseJCm2 when set.
//    (c) Zero-duration hard rejection — implemented 2026-06-04. dur ≤ 0 → .error (was .warning).

import XCTest
@testable import NeurOne

final class NPProtocolValidatorTests: XCTestCase {

    // MARK: - Helpers

    /// Validator with no configured dosage limits — only hardware ceilings apply.
    private func hardwareOnlyValidator() -> NPProtocolValidator {
        NPProtocolValidator(resolvedLimits: .unlimited)
    }

    private func protocolWith(
        _ params: NPModalityParams,
        interval: NPIntervalConfig = .continuous,
        durationSeconds: Int = 20 * 60
    ) -> NPProtocolDefinition {
        NPProtocolDefinition(
            name: "Test Protocol",
            timingMode: .duration(durationSeconds),
            modalities: [NPProtocolModality(params: params, interval: interval, enabled: true)]
        )
    }

    // MARK: - testValidProtocolAccepted

    func testValidProtocolAccepted() {
        let pbm = NPPBMTranscranialParams(
            irradianceMWcm2: 302,
            frequencyHz: 20,
            dutyCyclePercent: 25
        )
        let def = protocolWith(.pbmTranscranial(pbm))
        let result = hardwareOnlyValidator().validate(def)

        XCTAssertTrue(result.isValid, "A minimal in-bounds PBM protocol must validate as success.")
        XCTAssertTrue(result.errors.isEmpty, "Valid protocol must produce no errors.")
    }

    // MARK: - testCurrentOverLimitRejected_tDCS

    func testCurrentOverLimitRejected_tDCS() {
        // Hardware ceiling for tDCS is 2.0 mA.
        let tdcs = NPTDCSParams(intensityMilliamps: 2.5, electrodePairs: [["Fp1", "P3"]])
        let def = protocolWith(.tdcs(tdcs))
        let result = hardwareOnlyValidator().validate(def)

        XCTAssertFalse(result.isValid, "tDCS current above 2 mA must be rejected.")
        XCTAssertTrue(
            result.errors.contains { $0.modality == .tdcs && $0.parameterKey == "intensityMilliamps" },
            "Rejection must cite the tDCS intensity parameter."
        )
    }

    // MARK: - testCurrentOverLimitRejected_BES

    func testCurrentOverLimitRejected_BES() {
        // Hardware ceiling for BES/tACS is 1.0 mA.
        let bes = NPBESTacsParams(frequencyHz: 20, intensityMilliamps: 1.5, waveform: .sinusoidal)
        let def = protocolWith(.besTacs(bes))
        let result = hardwareOnlyValidator().validate(def)

        XCTAssertFalse(result.isValid, "BES current above 1 mA must be rejected.")
        XCTAssertTrue(
            result.errors.contains { $0.modality == .besTacs && $0.parameterKey == "intensityMilliamps" },
            "Rejection must cite the BES intensity parameter."
        )
    }

    // MARK: - testChargeDensityOverLimitRejected (intended-behavior spec)

    func testChargeDensityOverLimitRejected() {
        // 2.0 mA × 3600s / 35 cm² (ONE electrode) = 205.7 mC/cm² — over 150.
        let tdcs = NPTDCSParams(intensityMilliamps: 2.0, electrodePairs: [["Fp1", "P3"]])
        let def = protocolWith(.tdcs(tdcs), interval: .continuous, durationSeconds: 60 * 60)
        let result = hardwareOnlyValidator().validate(def)

        XCTAssertTrue(
            result.errors.contains { $0.parameterKey.lowercased().contains("charge") },
            "Charge density over 150 mC/cm² must be rejected with a charge-density error."
        )
    }

    // MARK: - testChargeDensityBorderlineValid (intended-behavior spec)

    func testChargeDensityBorderlineValid() {
        // Just under the ceiling: 1.4 mA × 3749s / 35 cm² = 149.96 mC/cm².
        //
        // This fixture used to be 2.0 mA at the default 20-minute duration and asserted
        // valid, which it was only because the retired summed-area model divided by 70 cm².
        // OI-CHARGE-04 rebuilt it at the boundary it always claimed to test; OI-CHARGE-05
        // moved the boundary (40 → 150 mC/cm², now with a derivation behind it) and made
        // the comparison inclusive, because the safety MCU trips at >= and an app that
        // accepted exactly the ceiling would sign a session the device cuts.
        let tdcs = NPTDCSParams(intensityMilliamps: 1.4, electrodePairs: [["Fp1", "P3"]],
                                electrodeAreaCm2: 35.0)
        let def = protocolWith(.tdcs(tdcs), durationSeconds: 3749)
        let result = hardwareOnlyValidator().validate(def)

        XCTAssertTrue(
            result.isValid,
            "A tDCS protocol just under the charge-density ceiling must be accepted."
        )
    }

    /// The enforcer trips AT the ceiling, so the pre-flight must too (OI-CHARGE-05).
    func testChargeDensityAtCeilingRejected() {
        // 1.4 mA × 3750s / 35 cm² = exactly 150.0 mC/cm².
        let tdcs = NPTDCSParams(intensityMilliamps: 1.4, electrodePairs: [["Fp1", "P3"]],
                                electrodeAreaCm2: 35.0)
        let result = hardwareOnlyValidator().validate(
            protocolWith(.tdcs(tdcs), durationSeconds: 3750))
        XCTAssertTrue(
            result.errors.contains { $0.parameterKey == "chargeDensityMCcm2" },
            "Exactly at the ceiling must be rejected — np_charge_monitor.c trips at >=."
        )
    }

    /// The clinical consequence OI-CHARGE-05 (d) weighed, pinned so a later change to the
    /// ceiling cannot quietly take it away again. At the retired 40 the routine protocol
    /// was unavailable and 13 of the 14 shipped predefined tDCS protocols exceeded it.
    func testRoutineTwoMilliampTwentyMinuteProtocolAccepted() {
        // 2 mA × 1200s / 35 cm² = 68.6 mC/cm² — the single most common protocol in the
        // literature and the median of docs/tdcs_database_full.csv.
        let tdcs = NPTDCSParams(intensityMilliamps: 2.0, electrodePairs: [["F3", "F4"]],
                                electrodeAreaCm2: 35.0)
        XCTAssertFalse(
            hardwareOnlyValidator().validate(protocolWith(.tdcs(tdcs), durationSeconds: 1200))
                .errors.contains { $0.parameterKey == "chargeDensityMCcm2" },
            "2 mA × 20 min on a 35 cm² pad must be available."
        )
    }

    // MARK: - OI-CHARGE-04: the model, not just the number

    /// Density is per electrode, so adding pairs must never make a protocol pass that a
    /// single pair fails. The summed-area model got 2× more permissive with each pair.
    func testChargeDensityDoesNotRelaxWithMoreElectrodePairs() {
        // 1.0 mA × 6000s / 35 cm² = 171.4 mC/cm² — over 150 whatever the montage.
        // The retired summed model divided the pair's two pads (70 cm²), got 85.7, and
        // accepted it.
        for pairs in [[["F3", "F4"]],
                      [["F3", "F4"], ["P3", "P4"]],
                      [["F3", "F4"], ["P3", "P4"], ["Fz", "Pz"]]] {
            let tdcs = NPTDCSParams(intensityMilliamps: 1.0, electrodePairs: pairs,
                                    electrodeAreaCm2: 35.0)
            let def = protocolWith(.tdcs(tdcs), durationSeconds: 6000)
            let result = hardwareOnlyValidator().validate(def)
            XCTAssertTrue(
                result.errors.contains { $0.parameterKey == "chargeDensityMCcm2" },
                "\(pairs.count) pair(s): over-ceiling density must be rejected regardless of montage size."
            )
        }
    }

    /// A smaller declared pad is stricter — which is the whole point of declaring it.
    func testSmallerDeclaredElectrodeAreaIsStricter() {
        // 1.0 mA × 4000s = 4000 mC. On 35 cm²: 114.3 mC/cm², accepted. On 25 cm²: 160.0, rejected.
        let big = NPTDCSParams(intensityMilliamps: 1.0, electrodePairs: [["F3", "F4"]],
                               electrodeAreaCm2: 35.0)
        let small = NPTDCSParams(intensityMilliamps: 1.0, electrodePairs: [["F3", "F4"]],
                                 electrodeAreaCm2: 25.0)
        XCTAssertFalse(
            hardwareOnlyValidator().validate(protocolWith(.tdcs(big), durationSeconds: 4000))
                .errors.contains { $0.parameterKey == "chargeDensityMCcm2" },
            "35 cm² pad at 114.3 mC/cm² must be accepted."
        )
        XCTAssertTrue(
            hardwareOnlyValidator().validate(protocolWith(.tdcs(small), durationSeconds: 4000))
                .errors.contains { $0.parameterKey == "chargeDensityMCcm2" },
            "25 cm² pad at 160.0 mC/cm² must be rejected."
        )
    }

    /// An undeclared area is an error, not a fallback: the hub refuses a zero area and the
    /// safety MCU's geometry gate holds tDCS off, so accepting it hands the user a session
    /// that silently never stimulates. And it must not ALSO produce an infinite density.
    func testUndeclaredElectrodeAreaRejected() {
        let tdcs = NPTDCSParams(intensityMilliamps: 1.0, electrodePairs: [["F3", "F4"]],
                                electrodeAreaCm2: 0)
        let result = hardwareOnlyValidator().validate(protocolWith(.tdcs(tdcs)))
        XCTAssertTrue(
            result.errors.contains { $0.parameterKey == "electrodeAreaCm2" },
            "A zero electrode area must be rejected as a geometry error."
        )
        XCTAssertFalse(
            result.errors.contains { $0.parameterKey == "chargeDensityMCcm2" },
            "A zero area must not also report an infinite charge density — one defect, one message."
        )
    }

    /// Above what the uint16 milli-cm² wire field can carry.
    func testUnencodableElectrodeAreaRejected() {
        let tdcs = NPTDCSParams(intensityMilliamps: 1.0, electrodePairs: [["F3", "F4"]],
                                electrodeAreaCm2: 70.0)
        XCTAssertTrue(
            hardwareOnlyValidator().validate(protocolWith(.tdcs(tdcs)))
                .errors.contains { $0.parameterKey == "electrodeAreaCm2" },
            "An area beyond the uint16 milli-cm² wire field must be rejected."
        )
    }

    // MARK: - testChargeDensityBorderlineInvalid (intended-behavior spec)

    func testChargeDensityBorderlineInvalid() {
        // 2.0 mA × 3600s / 35 cm² = 205.7 mC/cm² — over 150, must be rejected.
        let tdcs = NPTDCSParams(intensityMilliamps: 2.0, electrodePairs: [["Fp1", "P3"]])
        let def = protocolWith(.tdcs(tdcs), durationSeconds: 60 * 60)
        let result = hardwareOnlyValidator().validate(def)

        XCTAssertTrue(
            result.errors.contains { $0.parameterKey.lowercased().contains("charge") },
            "Charge density over the 150 mC/cm² ceiling must be rejected."
        )
    }

    // MARK: - OI-CHARGE-05 (b): per-phase charge density for charge-balanced modalities

    /// The session-cumulative DC dose must NEVER be applied to a charge-balanced
    /// modality: net delivered charge is ~zero by construction, so integrating |I| over a
    /// session measures nothing physical. The safety MCU did exactly that until
    /// 2026-09-15, which would have tripped every one of these in 0.4–1.0 s.
    func testChargeBalancedModalityHasNoSessionDose() {
        let bes = NPBESTacsParams(frequencyHz: 10, intensityMilliamps: 1.0, waveform: .sinusoidal)
        for seconds in [3600, 36000] {
            let result = hardwareOnlyValidator().validate(
                protocolWith(.besTacs(bes), durationSeconds: seconds))
            XCTAssertFalse(
                result.errors.contains { $0.parameterKey == "chargeDensityMCcm2" },
                "\(seconds)s of tACS must raise no session-dose error — the waveform is charge balanced."
            )
        }
    }

    /// Each pulsed modality passes at its rated worst case.
    func testPulsedModalitiesPassAtRatedMaximum() {
        // VNS: 2 mA × 250 µs = 0.5 µC on a 0.5 cm² clip pad = 1.0 µC/cm², 2.5% of ceiling.
        let vns = NPVNSHRVParams(frequencyHz: 25, intensityMilliamps: 2.0)
        XCTAssertFalse(
            hardwareOnlyValidator().validate(protocolWith(.vnsHRV(vns)))
                .errors.contains { $0.parameterKey == "phaseChargeDensityUCcm2" },
            "VNS at its rated 2 mA / 250 µs must pass."
        )

        // Cervical VNS: 0.5 µC on a 2 cm² collar pad = 0.25 µC/cm².
        let cvns = NPCervicalVnsParams(frequencyHz: 25, intensityMilliamps: 2.0)
        XCTAssertFalse(
            hardwareOnlyValidator().validate(protocolWith(.cervicalVns(cvns)))
                .errors.contains { $0.parameterKey == "phaseChargeDensityUCcm2" },
            "Cervical VNS at its rated 2 mA must pass."
        )

        // tACS at the band bottom, where phase charge is LARGEST: 1 mA at 0.5 Hz is a
        // 1 s half-period. Sinusoidal, so 2/π × 1000 µC = 636.6 µC ÷ 25 cm² = 25.5 µC/cm².
        let tacs = NPBESTacsParams(frequencyHz: 0.5, intensityMilliamps: 1.0, waveform: .sinusoidal)
        XCTAssertFalse(
            hardwareOnlyValidator().validate(protocolWith(.besTacs(tacs)))
                .errors.contains { $0.parameterKey == "phaseChargeDensityUCcm2" },
            "1 mA sinusoidal tACS at the 0.5 Hz band bottom must pass."
        )
    }

    /// The 2/π factor is load-bearing, not a refinement. Identical amplitude and phase
    /// duration; only the waveform differs. A square wave delivers the full I × T
    /// (1000 µC ÷ 25 cm² = 40.0 µC/cm², exactly the ceiling, refused); the sinusoid
    /// delivers 2/π of it and passes. The safety MCU applies the same factor.
    func testSineFactorAppliesOnlyToSinusoids() {
        let square = NPBESTacsParams(frequencyHz: 0.5, intensityMilliamps: 1.0, waveform: .square)
        XCTAssertTrue(
            hardwareOnlyValidator().validate(protocolWith(.besTacs(square)))
                .errors.contains { $0.parameterKey == "phaseChargeDensityUCcm2" },
            "A 1 mA square wave at 0.5 Hz is 40.0 µC/cm² per phase and must be rejected."
        )
        let sine = NPBESTacsParams(frequencyHz: 0.5, intensityMilliamps: 1.0, waveform: .sinusoidal)
        XCTAssertFalse(
            hardwareOnlyValidator().validate(protocolWith(.besTacs(sine)))
                .errors.contains { $0.parameterKey == "phaseChargeDensityUCcm2" },
            "The same settings as a sinusoid are 25.5 µC/cm² and must be accepted."
        )
    }

    /// Lower frequency means a longer phase means more charge per phase.
    func testPhaseChargeGetsStricterAsFrequencyFalls() {
        let slow = NPBESTacsParams(frequencyHz: 0.4, intensityMilliamps: 1.0, waveform: .square)
        XCTAssertTrue(
            hardwareOnlyValidator().validate(protocolWith(.besTacs(slow)))
                .errors.contains { $0.parameterKey == "phaseChargeDensityUCcm2" },
            "0.4 Hz square at 1 mA is a 1.25 s phase = 50 µC/cm² and must be rejected."
        )
        let fast = NPBESTacsParams(frequencyHz: 1.0, intensityMilliamps: 1.0, waveform: .square)
        XCTAssertFalse(
            hardwareOnlyValidator().validate(protocolWith(.besTacs(fast)))
                .errors.contains { $0.parameterKey == "phaseChargeDensityUCcm2" },
            "1 Hz halves the phase and must be accepted."
        )
    }

    // MARK: - testZeroDurationRejected (intended-behavior spec)

    func testZeroDurationRejected() {
        let pbm = NPPBMTranscranialParams(irradianceMWcm2: 302, frequencyHz: 20, dutyCyclePercent: 25)
        let def = protocolWith(.pbmTranscranial(pbm), durationSeconds: 0)
        let result = hardwareOnlyValidator().validate(def)

        XCTAssertFalse(result.isValid, "A protocol with 0-second duration must be rejected.")
        XCTAssertTrue(
            result.errors.contains { $0.parameterKey == "duration" },
            "Zero-duration rejection must cite the duration parameter."
        )
    }

    // MARK: - testDoseOverLimitRejected (intended-behavior spec)

    func testDoseOverLimitRejected() {
        // 200 mW/cm² CW (continuous) × 3600 s / 1000 = 720 J/cm² — over the 10 J/cm² limit.
        var limits = NPLimitsSet(name: "Dose-capped", level: .global)
        limits.pbmTranscranial = NPPBMTranscranialLimits(maxSessionDoseJCm2: 10.0)
        let validator = NPProtocolValidator(resolvedLimits: limits)

        let pbm = NPPBMTranscranialParams(irradianceMWcm2: 200, frequencyHz: 0, dutyCyclePercent: 100)
        let def = protocolWith(.pbmTranscranial(pbm), durationSeconds: 60 * 60)
        let result = validator.validate(def)

        XCTAssertTrue(
            result.errors.contains { $0.modality == .pbmTranscranial && $0.parameterKey.lowercased().contains("dose") },
            "PBM dose exceeding the configured J/cm² limit must be rejected."
        )
    }

    // MARK: - Bonus: configured dosage limit (no production gap) — confirms the limit path works

    func testConfiguredIntensityLimitRejected() {
        var limits = NPLimitsSet(name: "Capped", level: .global)
        limits.pbmTranscranial = NPPBMTranscranialLimits(maxIrradianceMWcm2: 200)
        let validator = NPProtocolValidator(resolvedLimits: limits)

        let pbm = NPPBMTranscranialParams(irradianceMWcm2: 322, frequencyHz: 20, dutyCyclePercent: 25)
        let def = protocolWith(.pbmTranscranial(pbm))
        let result = validator.validate(def)

        XCTAssertFalse(result.isValid, "Irradiance above the configured dosage limit must be rejected.")
        XCTAssertTrue(
            result.errors.contains { $0.parameterKey == "irradianceMWcm2" },
            "Rejection must cite the irradiance parameter."
        )
    }

    // MARK: - OI-HEXTILE-25: R-4 is two ceilings on the on-state irradiance

    func testPulsedIrradianceOverPeakCeilingRejected() {
        let ok = NPPBMTranscranialParams(irradianceMWcm2: 400, frequencyHz: 40, dutyCyclePercent: 25)
        XCTAssertFalse(hardwareOnlyValidator().validate(protocolWith(.pbmTranscranial(ok)))
            .errors.contains { $0.parameterKey == "irradianceMWcm2" }, "400 mW/cm² pulsed is at the peak ceiling.")
        var over = ok
        over.irradianceMWcm2 = 401
        XCTAssertTrue(hardwareOnlyValidator().validate(protocolWith(.pbmTranscranial(over)))
            .errors.contains { $0.parameterKey == "irradianceMWcm2" }, "401 mW/cm² pulsed must be rejected.")
    }

    func testCWIrradianceOverContinuousCeilingRejectedAndDutyIgnored() {
        let over = NPPBMTranscranialParams(irradianceMWcm2: 322, frequencyHz: 0, dutyCyclePercent: 100)
        let r = hardwareOnlyValidator().validate(protocolWith(.pbmTranscranial(over)))
        XCTAssertTrue(r.errors.contains { $0.parameterKey == "irradianceMWcm2" }, "322 mW/cm² CW exceeds 200.")
        XCTAssertFalse(r.errors.contains { $0.parameterKey == "dutyCyclePercent" },
                       "CW is continuous: 100 % is not a duty error.")
    }
}
