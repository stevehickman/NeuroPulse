import XCTest
@testable import NeurOne

/// Parsing, serializing and validating the `zones:` field.
///
/// The bug these guard: the parser's `default: p.zones = .all` mapped every
/// selector it did not recognise onto "every module" — so
/// `zones: ["Frontal Right (excl. midline)"]`, the lateralized clinical-03
/// target, silently became a whole-vault session with no error anywhere.
final class NPPBMTargetScriptingTests: XCTestCase {

    // MARK: - Helpers

    private func parseTarget(zonesLine: String, extra: String = "") throws -> NPPBMTarget {
        let source = """
        protocol "Target Test" {
            id: "AABB0100-0000-0000-0000-000000000000"
            description: "Target parsing"
            author: "Test"
            version: "1.0"
            readonly: false
            duration: 10m
            pbm_transcranial {
                intensity: 80%
                frequency: 40Hz
                duty_cycle: 25%
                \(zonesLine)
        \(extra)
                wavelength: 660_808nm
            }
        }
        """
        var lexer = NPPSLexer(source)
        var parser = NPPSParser(try lexer.tokenize())
        let entries = try parser.parse()
        guard case .single(let proto)? = entries.first,
              case .pbmTranscranial(let params)? = proto.modalities.first?.params else {
            throw XCTSkip("protocol did not parse into a PBM transcranial modality")
        }
        return params.target
    }

    private func serializedZonesLine(for target: NPPBMTarget) -> String? {
        var params = NPPBMTranscranialParams()
        params.target = target
        let proto = NPProtocolDefinition(
            name: "Serialize Test",
            modalities: [NPProtocolModality(params: .pbmTranscranial(params))]
        )
        return NPPSSerializer().serialize(.single(proto))
            .split(separator: "\n")
            .map { $0.trimmingCharacters(in: .whitespaces) }
            .first { $0.hasPrefix("zones:") }
    }

    // MARK: - Parsing the forms that resolve

    func testNamedZoneArrayParsesAsNamedRefs() throws {
        let target = try parseTarget(zonesLine: #"zones: ["Frontal Right (excl. midline)"]"#)
        XCTAssertEqual(target, .named(["Frontal Right (excl. midline)"]))
    }

    func testLateralizedZoneNoLongerBecomesEveryModule() throws {
        // The regression this whole change exists for. Before: parsed as `.all`.
        let target = try parseTarget(zonesLine: #"zones: ["Frontal Right (excl. midline)"]"#)
        let mask = try target.resolve()
        XCTAssertEqual(mask.socketIDs, [3, 6, 9, 10, 14, 15, 18, 19])
        XCTAssertLessThan(mask.socketCount, SocketZones.socketCount,
                          "a lateralized target must not light the whole vault")
    }

    func testMultipleNamedZonesParse() throws {
        let target = try parseTarget(zonesLine: #"zones: ["Frontal Left", "Frontal Right"]"#)
        XCTAssertEqual(target, .named(["Frontal Left", "Frontal Right"]))
    }

    func testClinicianSelectedParses() throws {
        XCTAssertEqual(try parseTarget(zonesLine: "zones: clinician_selected"), .clinicianSelected)
    }

    // MARK: - Parsing the forms that are retained but do not resolve

    func testRetiredSelectorsParseAsRetired() throws {
        XCTAssertEqual(try parseTarget(zonesLine: "zones: all"),   .retiredSelector(.all))
        XCTAssertEqual(try parseTarget(zonesLine: "zones: front"), .retiredSelector(.front))
        XCTAssertEqual(try parseTarget(zonesLine: "zones: rear"),  .retiredSelector(.rear))
    }

    func testNumericZoneArrayParsesAsRetiredNumericWithoutRebasing() throws {
        // Kept EXACTLY as authored. The old parser subtracted 1, silently picking
        // one of the two contradictory documented bases.
        XCTAssertEqual(try parseTarget(zonesLine: "zones: [1, 2]"), .retiredNumeric([1, 2]))
    }

    func testCustomWithSiblingListParsesAsRetiredNumeric() throws {
        let target = try parseTarget(
            zonesLine: "zones: custom", extra: "        custom_zones: [3, 4, 5]"
        )
        XCTAssertEqual(target, .retiredNumeric([3, 4, 5]))
    }

    func testBareCustomParsesAsAnEmptyRetiredNumeric() throws {
        XCTAssertEqual(try parseTarget(zonesLine: "zones: custom"), .retiredNumeric([]))
    }

    // MARK: - The silent fallback is gone

    func testUnrecognisedSelectorThrowsInsteadOfDefaultingToAll() {
        XCTAssertThrowsError(try parseTarget(zonesLine: "zones: whole_head")) { error in
            let message = (error as? NPPSError)?.errorDescription ?? "\(error)"
            XCTAssertTrue(message.contains("whole_head"),
                          "the error must name the unrecognised selector: \(message)")
        }
    }

    func testEmptyZoneListThrows() {
        XCTAssertThrowsError(try parseTarget(zonesLine: "zones: []"))
    }

    func testMixedZoneListThrows() {
        XCTAssertThrowsError(try parseTarget(zonesLine: #"zones: ["Frontal", 2]"#))
    }

    // MARK: - Serializing round-trips every form as authored

    func testSerializerRoundTripsEveryForm() throws {
        let cases: [(NPPBMTarget, String)] = [
            (.named(["All"]),                          #"zones: ["All"]"#),
            (.named(["Frontal Left", "Frontal Right"]), #"zones: ["Frontal Left", "Frontal Right"]"#),
            (.clinicianSelected,                       "zones: clinician_selected"),
            (.retiredSelector(.all),                   "zones: all"),
            (.retiredSelector(.front),                 "zones: front"),
            (.retiredSelector(.rear),                  "zones: rear"),
            (.retiredNumeric([1, 2]),                  "zones: [1, 2]"),
            (.retiredNumeric([]),                      "zones: custom"),
        ]
        for (target, expected) in cases {
            XCTAssertEqual(serializedZonesLine(for: target), expected)
            // And back again, unchanged — serializing is not a migration step.
            XCTAssertEqual(try parseTarget(zonesLine: expected), target)
        }
    }

    // MARK: - Every shipped protocol targets something real

    func testAllBundledPBMProtocolsResolve() throws {
        for source in NPBundledProtocols.allContents {
            var lexer = NPPSLexer(source)
            var parser = NPPSParser(try lexer.tokenize())
            for entry in try parser.parse() {
                guard case .single(let proto) = entry else { continue }
                for modality in proto.modalities {
                    guard case .pbmTranscranial(let params) = modality.params else { continue }
                    if params.target == .clinicianSelected { continue }
                    XCTAssertNoThrow(
                        try params.resolveSocketMask(),
                        "shipped protocol \"\(proto.name)\" has an unresolvable PBM target "
                            + "(\(params.target.displayName))"
                    )
                }
            }
        }
    }

    // MARK: - Validator surfaces an unresolvable target before upload

    private func validate(_ target: NPPBMTarget) -> NPValidationResult {
        var params = NPPBMTranscranialParams()
        params.target = target
        return NPProtocolValidator(resolvedLimits: .unlimited).validate(
            NPProtocolDefinition(
                name: "Validation Test",
                modalities: [NPProtocolModality(params: .pbmTranscranial(params))]
            )
        )
    }

    func testValidatorAcceptsANamedZone() {
        XCTAssertTrue(validate(.named(["Frontal"])).isValid)
    }

    func testValidatorRejectsRetiredSelector() {
        let result = validate(.retiredSelector(.front))
        XCTAssertFalse(result.isValid)
        let message = result.errors.first?.message ?? ""
        XCTAssertTrue(message.contains("\"Frontal\""),
                      "the validation error must name the replacement zone: \(message)")
    }

    func testValidatorRejectsRetiredNumeric() {
        XCTAssertFalse(validate(.retiredNumeric([1, 2])).isValid)
    }

    func testValidatorRejectsUnknownZoneName() {
        let result = validate(.named(["Left Frontal"]))
        XCTAssertFalse(result.isValid)
        XCTAssertTrue((result.errors.first?.message ?? "").contains("Left Frontal"))
    }

    func testValidatorDoesNotRejectClinicianSelected() {
        // Unresolvable by design until the operator picks sockets; refusing it
        // here would make every clinician-selected protocol unauthorable.
        XCTAssertTrue(validate(.clinicianSelected).isValid)
    }

    // MARK: - Wire protocol

    func testWireProtocolCarriesTheResolvedMask() throws {
        var params = NPPBMTranscranialParams()
        params.target = .named(["Occipital Left"])
        let definition = NPProtocolDefinition(
            name: "Wire Test",
            modalities: [NPProtocolModality(params: .pbmTranscranial(params))]
        )
        let wire = try NPSessionProtocol(from: definition)
        XCTAssertEqual(wire.schemaVersion, 2,
                       "the blob shape changed; the hub must not read a v2 blob as v1")
        guard case .pbmTranscranial(let config)? = wire.modalities.first else {
            return XCTFail("expected a PBM transcranial modality on the wire")
        }
        XCTAssertEqual(config.socketMask.socketIDs, [72, 73, 74, 77, 78])
    }

    func testWireProtocolRefusesARetiredTarget() {
        var params = NPPBMTranscranialParams()
        params.target = .retiredSelector(.all)
        let definition = NPProtocolDefinition(
            name: "Wire Test",
            modalities: [NPProtocolModality(params: .pbmTranscranial(params))]
        )
        XCTAssertThrowsError(try NPSessionProtocol(from: definition))
    }
}
