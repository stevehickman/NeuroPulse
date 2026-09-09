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
//    (a) Charge density (µC/cm²) — implemented 2026-06-04, model corrected 2026-09-09 by
//        OI-CHARGE-04. Formula: I(mA) × t(s) / A(cm²) where A is the area of ONE electrode,
//        declared by the protocol as NPTDCSParams.electrodeAreaCm2 and carried to the safety
//        MCU in the signed descriptor. Ceiling: NPHardwareLimits.tdcsMaxChargeDensityUCcm2 (40).
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
            intensityPercent: 75,
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
        // 2.0 mA × 3600s / 35 cm² (ONE electrode) = 205.7 µC/cm² — over 40.
        let tdcs = NPTDCSParams(intensityMilliamps: 2.0, electrodePairs: [["Fp1", "P3"]])
        let def = protocolWith(.tdcs(tdcs), interval: .continuous, durationSeconds: 60 * 60)
        let result = hardwareOnlyValidator().validate(def)

        XCTAssertTrue(
            result.errors.contains { $0.parameterKey.lowercased().contains("charge") },
            "Charge density over 40 µC/cm² must be rejected with a charge-density error."
        )
    }

    // MARK: - testChargeDensityBorderlineValid (intended-behavior spec)

    func testChargeDensityBorderlineValid() {
        // The ceiling is inclusive: 1.4 mA × 1000s / 35 cm² = exactly 40.0 µC/cm².
        //
        // This fixture used to be 2.0 mA at the default 20-minute duration and asserted
        // valid, which it was only because the retired summed-area model divided by 70 cm²
        // (34.3 µC/cm²). Per electrode that same protocol is 68.6 µC/cm² — over the ceiling,
        // and it is the safety MCU that was going to say so, mid-session. The test now
        // constructs the boundary it always claimed to be testing.
        let tdcs = NPTDCSParams(intensityMilliamps: 1.4, electrodePairs: [["Fp1", "P3"]],
                                electrodeAreaCm2: 35.0)
        let def = protocolWith(.tdcs(tdcs), durationSeconds: 1000)
        let result = hardwareOnlyValidator().validate(def)

        XCTAssertTrue(
            result.isValid,
            "A tDCS protocol exactly at the inclusive charge-density ceiling must be accepted."
        )
    }

    // MARK: - OI-CHARGE-04: the model, not just the number

    /// Density is per electrode, so adding pairs must never make a protocol pass that a
    /// single pair fails. The summed-area model got 2× more permissive with each pair.
    func testChargeDensityDoesNotRelaxWithMoreElectrodePairs() {
        // 1.0 mA × 1500s / 35 cm² = 42.9 µC/cm² — over 40 whatever the montage.
        for pairs in [[["F3", "F4"]],
                      [["F3", "F4"], ["P3", "P4"]],
                      [["F3", "F4"], ["P3", "P4"], ["Fz", "Pz"]]] {
            let tdcs = NPTDCSParams(intensityMilliamps: 1.0, electrodePairs: pairs,
                                    electrodeAreaCm2: 35.0)
            let def = protocolWith(.tdcs(tdcs), durationSeconds: 1500)
            let result = hardwareOnlyValidator().validate(def)
            XCTAssertTrue(
                result.errors.contains { $0.parameterKey == "chargeDensityUCcm2" },
                "\(pairs.count) pair(s): over-ceiling density must be rejected regardless of montage size."
            )
        }
    }

    /// A smaller declared pad is stricter — which is the whole point of declaring it.
    func testSmallerDeclaredElectrodeAreaIsStricter() {
        // 1.0 mA × 1100s = 1100 µC. On 35 cm²: 31.4 µC/cm², accepted. On 25 cm²: 44.0, rejected.
        let big = NPTDCSParams(intensityMilliamps: 1.0, electrodePairs: [["F3", "F4"]],
                               electrodeAreaCm2: 35.0)
        let small = NPTDCSParams(intensityMilliamps: 1.0, electrodePairs: [["F3", "F4"]],
                                 electrodeAreaCm2: 25.0)
        XCTAssertFalse(
            hardwareOnlyValidator().validate(protocolWith(.tdcs(big), durationSeconds: 1100))
                .errors.contains { $0.parameterKey == "chargeDensityUCcm2" },
            "35 cm² pad at 31.4 µC/cm² must be accepted."
        )
        XCTAssertTrue(
            hardwareOnlyValidator().validate(protocolWith(.tdcs(small), durationSeconds: 1100))
                .errors.contains { $0.parameterKey == "chargeDensityUCcm2" },
            "25 cm² pad at 44.0 µC/cm² must be rejected."
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
            result.errors.contains { $0.parameterKey == "chargeDensityUCcm2" },
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
        // 2.0 mA × 3600s / 35 cm² = 205.7 µC/cm² — over 40, must be rejected.
        let tdcs = NPTDCSParams(intensityMilliamps: 2.0, electrodePairs: [["Fp1", "P3"]])
        let def = protocolWith(.tdcs(tdcs), durationSeconds: 60 * 60)
        let result = hardwareOnlyValidator().validate(def)

        XCTAssertTrue(
            result.errors.contains { $0.parameterKey.lowercased().contains("charge") },
            "Charge density over the 40 µC/cm² ceiling must be rejected."
        )
    }

    // MARK: - testZeroDurationRejected (intended-behavior spec)

    func testZeroDurationRejected() {
        let pbm = NPPBMTranscranialParams(intensityPercent: 75, frequencyHz: 20, dutyCyclePercent: 25)
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
        // 100% CW intensity = 200 mW/cm² × 3600s / 1000 = 720 J/cm² — over the 10 J/cm² limit.
        var limits = NPLimitsSet(name: "Dose-capped", level: .global)
        limits.pbmTranscranial = NPPBMTranscranialLimits(maxSessionDoseJCm2: 10.0)
        let validator = NPProtocolValidator(resolvedLimits: limits)

        let pbm = NPPBMTranscranialParams(intensityPercent: 100, frequencyHz: 0, dutyCyclePercent: 25)
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
        limits.pbmTranscranial = NPPBMTranscranialLimits(maxIntensityPercent: 50)
        let validator = NPProtocolValidator(resolvedLimits: limits)

        let pbm = NPPBMTranscranialParams(intensityPercent: 80, frequencyHz: 20, dutyCyclePercent: 25)
        let def = protocolWith(.pbmTranscranial(pbm))
        let result = validator.validate(def)

        XCTAssertFalse(result.isValid, "Intensity above the configured dosage limit must be rejected.")
        XCTAssertTrue(
            result.errors.contains { $0.parameterKey == "intensityPercent" },
            "Rejection must cite the intensity parameter."
        )
    }
}
