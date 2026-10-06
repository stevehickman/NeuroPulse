import XCTest
@testable import NeurOne

/// NP-NPPS-REF-001 Rev 18 §4.1b, §4.7: PBM irradiance and audio level are absolute, and every
/// PBM wavelength is its own block. Mirrors app/web/src/lib/npps.test.ts, the Android
/// NPPSAbsoluteQuantityTests and the shared fixtures in npps/fixtures.
final class NPAbsoluteQuantityTests: XCTestCase {

    private func parse(_ source: String) throws -> [NPProtocolEntry] {
        var lexer = NPPSLexer(source)
        var parser = NPPSParser(try lexer.tokenize())
        return try parser.parse()
    }

    private func proto(_ body: String) -> String {
        "protocol \"T\" {\n    duration: 5m\n\(body)\n}\n"
    }

    private func pbm(_ fields: String) -> String {
        "    pbm_transcranial {\n\(fields)\n    }"
    }

    private func assertRefused(_ body: String, contains fragment: String,
                               file: StaticString = #filePath, line: UInt = #line) {
        XCTAssertThrowsError(try parse(proto(body)), file: file, line: line) { error in
            XCTAssertTrue("\(error)".contains(fragment), "got: \(error)", file: file, line: line)
        }
    }

    func testReadsIrradianceWithItsUnit() throws {
        let entries = try parse(proto(pbm("wavelength: \"808nm\"\nirradiance: 36mW_cm2")))
        guard case .single(let p) = entries[0], case .pbmTranscranial(let params) = p.modalities[0].params else {
            return XCTFail("expected one PBM protocol")
        }
        XCTAssertEqual(params.irradianceMWcm2, 36)
        XCTAssertEqual(params.wavelength.rawValue, "808nm")
    }

    func testAcceptsTheCanonicalNameWithABareNumber() throws {
        let entries = try parse(proto(pbm("wavelength: \"808nm\"\nirradiance_mw_cm2: 36")))
        guard case .single(let p) = entries[0], case .pbmTranscranial(let params) = p.modalities[0].params else {
            return XCTFail("expected one PBM protocol")
        }
        XCTAssertEqual(params.irradianceMWcm2, 36)
    }

    func testRefusesPercentagesAndUnitlessDoses() {
        assertRefused(pbm("intensity: 80%\nwavelength: \"808nm\""), contains: "percentage of a baseline")
        assertRefused(pbm("intensity_percent: 80\nwavelength: \"808nm\""), contains: "percentage of a baseline")
        assertRefused(pbm("wavelength: \"808nm\"\nirradiance: 80%"), contains: "in % is refused")
        assertRefused(pbm("wavelength: \"808nm\"\nirradiance: 80"), contains: "needs its unit written")
        assertRefused(pbm("wavelength: \"808nm\"\nirradiance: 80mA"), contains: "takes mW_cm2, not mA")
    }

    func testRequiresIrradianceAndWavelengthRatherThanDefaultingThem() {
        assertRefused(pbm("wavelength: \"808nm\""), contains: "irradiance (e.g. irradiance: 300mW_cm2) is required")
        assertRefused(pbm("irradiance: 300mW_cm2"), contains: "wavelength is required")
    }

    func testRefusesTheRetiredCombinedWavelengthsAndNamesTheReplacement() {
        for wl in ["660_808nm", "660_808_1064nm"] {
            assertRefused(pbm("wavelength: \"\(wl)\"\nirradiance: 300mW_cm2"), contains: "one block per wavelength")
        }
    }

    func testIntranasalBlocksCarryAWavelengthAndAnIrradiance() throws {
        let entries = try parse(proto("    pbm_intranasal {\n        wavelength: \"660nm\"\n        irradiance: 25mW_cm2\n    }"))
        guard case .single(let p) = entries[0], case .pbmIntranasal(let params) = p.modalities[0].params else {
            return XCTFail("expected one intranasal protocol")
        }
        XCTAssertEqual(params.irradianceMWcm2, 25)
        XCTAssertEqual(params.wavelength.rawValue, "660nm")
        assertRefused("    pbm_intranasal {\n        intensity: 60%\n    }", contains: "percentage of a baseline")
    }

    func testReadsAudioInDbAndRefusesAPercentage() throws {
        func audio(_ body: String) -> String {
            "    audio_entrainment {\n        carrier_hz: 440Hz\n        \(body)\n    }"
        }
        let entries = try parse(proto(audio("volume: 72.5dB")))
        guard case .single(let p) = entries[0], case .audioEntrainment(let params) = p.modalities[0].params else {
            return XCTFail("expected one audio protocol")
        }
        XCTAssertEqual(params.volumeDb, 72.5)
        assertRefused(audio("volume: 70%"), contains: "in % is refused")
        assertRefused(audio("volume: 70"), contains: "needs its unit written")
        assertRefused(audio("volume_percent: 70"), contains: "percentage of a baseline")
        assertRefused(audio("binaural_hz: 10Hz"), contains: "is required and must be positive")
    }

    func testRefusesAPercentageCeilingInLimitsInsteadOfSkippingIt() throws {
        for mod in ["pbm_transcranial", "pbm_intranasal", "audio_entrainment"] {
            let text = "limits \"L\" {\n    level: global\n    \(mod) {\n        max_intensity: 80\n    }\n}\n"
            XCTAssertThrowsError(try parse(text)) { error in
                XCTAssertTrue("\(error)".contains("percentage ceiling and is retired"), "got: \(error)")
            }
        }
        let ok = try parse("limits \"L\" {\n    level: global\n    pbm_transcranial {\n        max_irradiance_mw_cm2: 250\n    }\n}\n")
        guard case .limits(let lim) = ok[0] else { return XCTFail("expected limits") }
        XCTAssertEqual(lim.pbmTranscranial?.maxIrradianceMWcm2, 250)
    }

    func testTheSessionBuilderCarriesAbsoluteValuesAndRefusesRetiredNames() throws {
        var transcranial = NPPBMTranscranialParams()
        transcranial.irradianceMWcm2 = 250
        var nasal = NPPBMIntranasalParams()
        nasal.irradianceMWcm2 = 40
        var audio = NPAudioEntrainmentParams()
        audio.volumeDb = 70
        let definition = NPProtocolDefinition(
            name: "abs",
            modalities: [
                NPProtocolModality(params: .pbmTranscranial(transcranial)),
                NPProtocolModality(params: .pbmIntranasal(nasal)),
                NPProtocolModality(params: .audioEntrainment(audio)),
            ]
        )
        // The wire carries a drive register, which the hub cannot read back as a percentage.
        // 250 mW/cm² at 808 nm: round(250 / 403 × 255) = 158 in the second current byte.
        let blob = try HubDescriptorCompiler.build(NPProtocolDefinition(
            name: "abs", modalities: [NPProtocolModality(params: .pbmTranscranial(transcranial))])).blob
        // header(64) + cmd_hdr(14) + socket mask(16) → params [freq, duty, cur_660, cur_808]
        XCTAssertEqual(Array(blob.subdata(in: 94..<98)), [20, 50, 0, 158])
        // Audio: 70 dB SPL → (70 − 40) / 0.5 = 60 % volume register.
        let audioBlob = try HubDescriptorCompiler.build(NPProtocolDefinition(
            name: "abs", modalities: [NPProtocolModality(params: .audioEntrainment(audio))])).blob
        XCTAssertEqual(audioBlob[64 + 14 + 5], 60)
        _ = nasal

        var retired = NPPBMTranscranialParams()
        retired.wavelength = NPPBMTranscranialParams.Wavelength(rawValue: "660_808nm")
        let bad = NPProtocolDefinition(name: "bad", modalities: [NPProtocolModality(params: .pbmTranscranial(retired))])
        XCTAssertThrowsError(try HubDescriptorCompiler.build(bad)) { error in
            XCTAssertTrue("\(error.localizedDescription)".contains("retired"))
        }
        var nasal1064 = NPPBMIntranasalParams()
        nasal1064.wavelength = .nm1064
        let probe = NPProtocolDefinition(name: "nasal", modalities: [NPProtocolModality(params: .pbmIntranasal(nasal1064))])
        XCTAssertThrowsError(try HubDescriptorCompiler.build(probe))
    }

    func testTheValidatorChecksThe400PeakDirectly() {
        var params = NPPBMTranscranialParams()
        params.irradianceMWcm2 = 450
        let definition = NPProtocolDefinition(name: "hot", modalities: [NPProtocolModality(params: .pbmTranscranial(params))])
        let result = NPProtocolValidator(resolvedLimits: .unlimited).validate(definition)
        XCTAssertTrue(result.errors.contains { $0.parameterKey == "irradianceMWcm2" })
    }

    // OI-SESPWR-03: `frequency: 0` is CW and CW has no duty cycle.
    func testRefusesCwWithADutyOtherThan100() {
        let doses = [
            "pbm_transcranial": "wavelength: \"808nm\"\nirradiance: 30mW_cm2",
            "pbm_intranasal": "wavelength: \"660nm\"\nirradiance: 30mW_cm2",
            "pbm_deep_1170nm": "intensity_mw_cm2: 500"
        ]
        for (modality, fields) in doses {
            assertRefused("    \(modality) {\n\(fields)\nfrequency: 0Hz\nduty_cycle: 25%\n    }",
                          contains: "continuous wave, which has no duty cycle")
        }
    }

    func testReadsCwAs100PercentWhetherTheDutyIsWrittenOrNot() throws {
        for extra in ["", "\nduty_cycle: 100%"] {
            let entries = try parse(proto(pbm("wavelength: \"808nm\"\nirradiance: 30mW_cm2\nfrequency: 0Hz\(extra)")))
            guard case .single(let p) = entries[0], case .pbmTranscranial(let params) = p.modalities[0].params else {
                return XCTFail("expected one PBM protocol")
            }
            XCTAssertEqual(params.dutyCyclePercent, 100)
        }
    }

    func testLeavesAPulsedBlockAlone() throws {
        let entries = try parse(proto(pbm("wavelength: \"808nm\"\nirradiance: 30mW_cm2\nfrequency: 40Hz\nduty_cycle: 25%")))
        guard case .single(let p) = entries[0], case .pbmTranscranial(let params) = p.modalities[0].params else {
            return XCTFail("expected one PBM protocol")
        }
        XCTAssertEqual(params.frequencyHz, 40)
        XCTAssertEqual(params.dutyCyclePercent, 25)
    }
}
