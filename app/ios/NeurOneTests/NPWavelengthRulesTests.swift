import XCTest
@testable import NeurOne

/// NP-NPPS-REF-001 Rev 17 §4.1a / §7a on iOS. Mirrors the web reference tests
/// (common/lib/wavelengthRules.test.ts) for the cases this runtime acts on:
/// the defaults, the refusal of an unmapped or invalid wavelength, that a script's
/// wavelength is carried exactly as written, and that `start` round-trips.
final class NPWavelengthRulesTests: XCTestCase {

    private let rules = NPWavelengthRules.default

    func testDefaultsMapTheSourceWavelengths() {
        XCTAssertEqual(rules.map(660), .led660)
        XCTAssertEqual(rules.map(810), .led808)
        XCTAssertEqual(rules.map(830), .led808)
        XCTAssertEqual(rules.map(1070), .led1064)
        XCTAssertNil(rules.map(640))
        XCTAssertNil(rules.map(850))
        XCTAssertNil(rules.map(1080))
    }

    func testRetiredNamesAreRefusedAnd1064IsAPlainWavelength() {
        for name in ["660_808nm", "660_808_1064nm"] {
            guard case .failure(let refusal) = rules.resolveChannels(name) else { return XCTFail("\(name) must be refused") }
            XCTAssertEqual(refusal.reason, .retired)
        }
        XCTAssertEqual(try rules.resolveChannels("1064nm").get(), [.led1064])
    }

    func testUnmappedAndInvalidAreRefused() {
        guard case .failure(let unmapped) = rules.resolveChannels("850nm") else { return XCTFail("850nm must be refused") }
        XCTAssertEqual(unmapped.reason, .unmapped)
        for bad in ["red", "810", "810 nm", ".5nm", "0nm", "810.nm", "8.1.0nm"] {
            guard case .failure(let invalid) = rules.resolveChannels(bad) else { return XCTFail("\(bad) must be refused") }
            XCTAssertEqual(invalid.reason, .invalid, bad)
        }
    }

    func testScriptWavelengthIsCarriedExactlyAndStartRoundTrips() throws {
        let script = """
        protocol "Series" {
            duration: 8m
            pbm_transcranial {
                wavelength: "810nm"
                irradiance: 250mW_cm2
                frequency: 0Hz
                duty_cycle: 100%
                zones: ["Frontal Right"]
                start: 4m
                interval_on: 4m
                interval_off: 0s
                repeat: 1
            }
        }
        wavelength_rules "Lab" {
            level: user
            channel "led_808" { nominal_nm: 808  min_nm: 798  max_nm: 860 }
        }
        """
        var lexer = NPPSLexer(script)
        var parser = NPPSParser(try lexer.tokenize())
        let entries = try parser.parse()
        XCTAssertEqual(entries.count, 1, "wavelength_rules is accepted and not an entry")
        guard case .single(let proto) = entries[0],
              case .pbmTranscranial(let p) = proto.modalities[0].params else {
            return XCTFail("expected one PBM protocol")
        }
        XCTAssertEqual(p.wavelength.rawValue, "810nm", "never replaced by a default")
        XCTAssertEqual(proto.modalities[0].interval.startOffsetSeconds, 240)
        XCTAssertFalse(p.wavelength.requiresSmartModule)
        XCTAssertTrue(NPPBMTranscranialParams.Wavelength(rawValue: "1070nm").requiresSmartModule)
    }

    func testWavelengthEncodesAsABareString() throws {
        let data = try JSONEncoder().encode(NPPBMTranscranialParams.Wavelength.nm808)
        XCTAssertEqual(String(data: data, encoding: .utf8), "\"808nm\"")
        let back = try JSONDecoder().decode(NPPBMTranscranialParams.Wavelength.self, from: Data("\"810nm\"".utf8))
        XCTAssertEqual(back.rawValue, "810nm")
    }
}
