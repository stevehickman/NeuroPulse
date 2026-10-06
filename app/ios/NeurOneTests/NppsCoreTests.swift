import XCTest
@testable import NeurOne

/// The iOS binding of the shared NPPS core (OI-NPPS-CORE-01), exercised through the linked static
/// library. The goldens are the web parser's and compiler's own output
/// (`bun scripts/gen-npps-parse-golden.ts`, `bun scripts/gen-hub-descriptor-golden.ts`); the Rust crate
/// and its C ABI are already diffed against them, so what this adds is the Swift layer: the bridge, the
/// buffer ownership, UTF-8 (the messages contain `—` and `²`) and the request builder. Every entry, byte
/// and message must match. These tests need the XCFramework scripts/build-npps-xcframework.sh builds.
final class NppsCoreTests: XCTestCase {

    // MARK: - Fixtures

    private static func root(file: StaticString = #filePath) -> URL {
        // .../<root>/app/ios/NeurOneTests/<this file>
        var url = URL(fileURLWithPath: "\(file)")
        for _ in 0..<4 { url.deleteLastPathComponent() }
        return url
    }

    private static func json(_ name: String) throws -> [String: Any] {
        let url = root().appendingPathComponent("app/NeurOneShared/TestData").appendingPathComponent(name)
        let object = try JSONSerialization.jsonObject(with: Data(contentsOf: url))
        return try XCTUnwrap(object as? [String: Any])
    }

    private func hex(_ data: Data) -> String { data.map { String(format: "%02x", $0) }.joined() }

    // MARK: - Parse

    private static let PARSE_KEYS = ["entries", "zones", "conditions", "wavelengthRules", "limits"]

    /// What the web parser's `parseNPPSFile` and `parseNPPSLimits` reduce to. The web parser keeps only the
    /// first `limits` block; the core reports every one.
    private func reduce(_ parsed: [String: Any]) -> NSDictionary {
        var out: [String: Any] = [:]
        for key in Self.PARSE_KEYS where key != "limits" { out[key] = parsed[key] ?? [] }
        out["limits"] = (parsed["limits"] as? [Any])?.first ?? NSNull()
        return out as NSDictionary
    }

    private func checkParse(_ name: String, _ source: String, _ expected: [String: Any], _ failures: inout [String]) {
        do {
            let data = try NppsCore.parse(source)
            if let error = expected["error"] as? String {
                failures.append("\(name): accepted what the web parser refuses: \(error)")
                return
            }
            let parsed = (try? JSONSerialization.jsonObject(with: data)) as? [String: Any] ?? [:]
            let want = expected.filter { Self.PARSE_KEYS.contains($0.key) }
            if !reduce(parsed).isEqual(to: want) { failures.append("\(name): the parse differs") }
        } catch let refusal as NppsCore.Refusal {
            guard let want = expected["error"] as? String else {
                failures.append("\(name): refused what the web parser accepts: \(refusal.message)")
                return
            }
            if refusal.message != want { failures.append("\(name): message differs\n  core: \(refusal.message)\n  web:  \(want)") }
        } catch {
            failures.append("\(name): \(error)")
        }
    }

    func testTheWholeShippedLibraryParsesAsTheWebParserDoes() throws {
        let files = try XCTUnwrap(Self.json("npps-parse-golden.json")["files"] as? [String: [String: Any]])
        XCTAssertGreaterThan(files.count, 60, "the golden covers the shipped library")
        var failures: [String] = []
        for (rel, expected) in files {
            let source = try String(contentsOf: Self.root().appendingPathComponent(rel), encoding: .utf8)
            checkParse(rel, source, expected, &failures)
        }
        XCTAssertTrue(failures.isEmpty, "\(failures.count) divergence(s):\n" + failures.joined(separator: "\n"))
    }

    func testTheRefusalAndAliasCorpusParsesAsTheWebParserDoes() throws {
        let cases = try XCTUnwrap(Self.json("npps-parse-golden.json")["cases"] as? [String: [String: Any]])
        var failures: [String] = []
        for (name, expected) in cases {
            checkParse(name, try XCTUnwrap(expected["source"] as? String), expected, &failures)
        }
        XCTAssertTrue(failures.isEmpty, "\(failures.count) divergence(s):\n" + failures.joined(separator: "\n"))
    }

    func testAnEmptySourceIsAnEmptyProtocolList() throws {
        let file = try JSONSerialization.jsonObject(with: NppsCore.parse("")) as? [String: Any]
        XCTAssertEqual((file?["entries"] as? [Any])?.count, 0)
    }

    // MARK: - Compile

    private func options(_ cases: [String: Any], withClinician: Bool) throws -> NppsCore.CompileOptions {
        let zones = try XCTUnwrap(cases["zones"] as? [String: [Int]])
        let clinician = try XCTUnwrap(cases["clinicianSockets"] as? [Int])
        let byte = try XCTUnwrap(cases["sessionUuidByte"] as? Int)
        return NppsCore.CompileOptions(
            zones: zones,
            clinicianSockets: withClinician ? clinician : nil,
            deviceSerial: nil,
            nowUnix: try XCTUnwrap(cases["compiledAt"] as? Int),
            sessionUUID: Data(repeating: UInt8(byte), count: 16),
            wavelengthRules: nil)
    }

    func testEveryGoldenDefinitionCompilesToTheWebCompilersBytes() throws {
        let cases = try Self.json("hub-descriptor-cases.json")
        let golden = try Self.json("hub-descriptor-golden.json")
        let defs = try XCTUnwrap(cases["cases"] as? [String: [String: Any]])
        XCTAssertGreaterThanOrEqual(defs.count, 22)
        for (name, entry) in defs {
            let def = try JSONSerialization.data(withJSONObject: try XCTUnwrap(entry["def"]))
            let blob = try NppsCore.compile(protocolJSON: def, options: try options(cases, withClinician: true))
            XCTAssertEqual(hex(blob), entry["hex"] as? String, name)
            XCTAssertEqual(hex(blob), golden[name] as? String, "\(name) (the file the other runtimes read)")
        }
    }

    func testEveryRefusalReadsAsTheWebCompilersDoes() throws {
        let cases = try Self.json("hub-descriptor-cases.json")
        let errors = try XCTUnwrap(cases["errors"] as? [String: [String: Any]])
        var refused = 0
        for (name, entry) in errors {
            guard let want = entry["message"] as? String else { continue }
            let with = !((entry["noClinicianSockets"] as? Bool) ?? false)
            let def = try JSONSerialization.data(withJSONObject: try XCTUnwrap(entry["def"]))
            XCTAssertThrowsError(
                try NppsCore.compile(protocolJSON: def, options: try options(cases, withClinician: with)), name
            ) { error in
                XCTAssertEqual((error as? NppsCore.Refusal)?.message, want, name)
            }
            refused += 1
        }
        XCTAssertGreaterThanOrEqual(refused, 15)
    }

    func testAShippedProtocolParsesThenCompiles() throws {
        let cases = try Self.json("hub-descriptor-cases.json")
        let url = Self.root().appendingPathComponent("protocols/predefined/01-gamma-focus.npps")
        let parsed = try XCTUnwrap(
            JSONSerialization.jsonObject(with: NppsCore.parse(String(contentsOf: url, encoding: .utf8))) as? [String: Any])
        let entries = try XCTUnwrap(parsed["entries"] as? [[String: Any]])
        let single = try XCTUnwrap(entries.first { ($0["kind"] as? String) == "single" })
        let proto = try JSONSerialization.data(withJSONObject: try XCTUnwrap(single["protocol"]))
        let blob = try NppsCore.compile(protocolJSON: proto, options: try options(cases, withClinician: true))
        XCTAssertEqual(blob[0], 0x50, "NP_HUB_PROTO_MAGIC low byte")
        XCTAssertGreaterThan(blob.count, 128)
        XCTAssertTrue(blob.suffix(64).allSatisfy { $0 == 0 }, "the signature slot is zeroed for the caller to fill")
    }

    func testAMalformedRequestIsRefusedNotCrashed() {
        let options = NppsCore.CompileOptions(
            zones: nil, clinicianSockets: nil, deviceSerial: nil, nowUnix: 0,
            sessionUUID: Data(repeating: 0, count: 16), wavelengthRules: nil)
        XCTAssertThrowsError(try NppsCore.compile(protocolJSON: Data("{}".utf8), options: options))
        XCTAssertThrowsError(try NppsCore.compile(
            protocolJSON: Data("{}".utf8),
            options: NppsCore.CompileOptions(
                zones: nil, clinicianSockets: nil, deviceSerial: nil, nowUnix: 0,
                sessionUUID: Data(), wavelengthRules: nil)))
    }

    // MARK: - Namespace

    func testNamespacesFoldAndValidateAsTheWebFunctionsDo() throws {
        let cases = try XCTUnwrap(Self.json("npps-parse-golden.json")["namespaces"] as? [String: [String: Any]])
        XCTAssertTrue(cases["library"] != nil && cases.count > 5)
        var failures: [String] = []
        for (name, expected) in cases {
            let sources: [String]
            if let paths = expected["paths"] as? [String] {
                sources = try paths.map { try String(contentsOf: Self.root().appendingPathComponent($0), encoding: .utf8) }
            } else {
                sources = try XCTUnwrap(expected["sources"] as? [String])
            }
            let files = try sources.map { try JSONSerialization.jsonObject(with: NppsCore.parse($0)) }
            let request = try JSONSerialization.data(withJSONObject: ["files": files])
            let got = try XCTUnwrap(JSONSerialization.jsonObject(with: NppsCore.namespace(requestJSON: request)) as? [String: Any])
            for key in ["entries", "zones", "conditions", "errors", "referenceErrors"] {
                if !((got[key] as? NSObject)?.isEqual(expected[key]) ?? false) { failures.append("\(name): \(key) differ") }
            }
        }
        XCTAssertTrue(failures.isEmpty, "\(failures.count) divergence(s):\n" + failures.joined(separator: "\n"))
    }

    // MARK: - Serialize and validate

    func testSerializeWritesTheShippedLibraryAsTheWebSerializerDid() throws {
        let files = try XCTUnwrap(Self.json("npps-serialize-golden.json")["files"] as? [String: [[String: Any]]])
        var failures: [String] = []
        var count = 0
        for (rel, items) in files {
            for item in items {
                count += 1
                let request = try JSONSerialization.data(withJSONObject: ["items": [try XCTUnwrap(item["item"])]])
                let got = String(decoding: try NppsCore.serialize(requestJSON: request), as: UTF8.self)
                if got != item["text"] as? String { failures.append("\(rel): differs") }
            }
        }
        XCTAssertGreaterThan(count, 100)
        XCTAssertTrue(failures.isEmpty, "\(failures.count) divergence(s):\n" + failures.joined(separator: "\n"))
    }

    func testSerializeRefusesAZoneItCannotWriteWithTheCoresMessage() throws {
        let zone: [String: Any] = ["kind": "zone", "zone": ["name": "Z", "sockets": [0, 999]]]
        let request = try JSONSerialization.data(withJSONObject: ["items": [zone]])
        XCTAssertThrowsError(try NppsCore.serialize(requestJSON: request)) { error in
            // The message holds an em dash: it must survive the trip through the static library unchanged.
            XCTAssertEqual((error as? NppsCore.Refusal)?.message,
                           "cannot serialize zone \"Z\": 0, 999 are not sockets on this helmet \u{2014} ids are whole numbers 1\u{2013}80")
        }
    }

    func testValidateReturnsLocaleKeysAndArguments() throws {
        var bes = NPBESTacsParams()
        bes.intensityMilliamps = 2
        let definition = NPProtocolDefinition(
            name: "P", timingMode: .duration(1200),
            modalities: [NPProtocolModality(params: .besTacs(bes))])
        let result = try NppsCore.validate(
            entry: .single(definition), limits: .unlimited, library: nil, zones: [:], limitSources: NPLimitSourceMap())
        XCTAssertEqual(result["isValid"] as? Bool, false)
        let issues = try XCTUnwrap(result["issues"] as? [[String: Any]])
        let message = try XCTUnwrap(issues.first?["message"] as? [String: Any])
        XCTAssertEqual(message["key"] as? String, "VALIDATE_MSG_BES_TACS_INTENSITYMILLIAMPS")
        XCTAssertEqual(message["args"] as? [String], ["2", "1"])
    }

    // MARK: - Limit resolution

    func testResolveLimitsTakesTheMostSpecificTierPerFieldAndNamesIt() throws {
        var global = NPLimitsSet(name: "G", level: .global)
        global.besTacs = NPBESTacsLimits(maxIntensityMilliamps: 1.0, maxFrequencyHz: 40, minFrequencyHz: 1)
        var helmet = NPLimitsSet(name: "H", level: .helmet)
        helmet.besTacs = NPBESTacsLimits(maxIntensityMilliamps: 0.8, maxFrequencyHz: 30)
        var individual = NPLimitsSet(name: "I", level: .individual)
        individual.besTacs = NPBESTacsLimits(maxIntensityMilliamps: 0.5)
        individual.tms = NPTMSLimits(maxPulsesPerSession: 100)

        let (limits, sources) = NPLimitsSet.resolve(global: global, helmet: helmet, individual: individual)
        XCTAssertEqual(limits.besTacs, NPBESTacsLimits(maxIntensityMilliamps: 0.5, maxFrequencyHz: 30, minFrequencyHz: 1))
        XCTAssertEqual(limits.tms?.maxPulsesPerSession, 100)
        XCTAssertNil(limits.tdcs, "no tier states a tdcs block")
        XCTAssertEqual(sources.besTacs?.maxIntensityMilliamps, .individual)
        XCTAssertEqual(sources.besTacs?.maxFrequencyHz, .helmet)
        XCTAssertEqual(sources.besTacs?.minFrequencyHz, .global_)
        XCTAssertEqual(sources.tms?.maxPulsesPerSession, .individual)
    }
}
