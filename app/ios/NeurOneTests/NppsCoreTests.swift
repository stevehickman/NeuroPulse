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

    /// What the web parser's entry list reduces to: protocols and the composites.
    private func reduce(_ entries: [[String: Any]]) -> [[String: Any]] {
        entries.filter { ($0["kind"] as? String) == "single" || ($0["what"] as? String) == "composite" }
    }

    private func checkParse(_ name: String, _ source: String, _ expected: [String: Any], _ failures: inout [String]) {
        do {
            let data = try NppsCore.parse(source)
            if let error = expected["error"] as? String {
                failures.append("\(name): accepted what the web parser refuses: \(error)")
                return
            }
            let entries = (try? JSONSerialization.jsonObject(with: data)) as? [[String: Any]] ?? []
            let want = expected["entries"] as? [[String: Any]] ?? []
            if !(reduce(entries) as NSArray).isEqual(to: want) { failures.append("\(name): entries differ") }
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
        let entries = try JSONSerialization.jsonObject(with: NppsCore.parse("")) as? [Any]
        XCTAssertEqual(entries?.count, 0)
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
        let entries = try XCTUnwrap(
            JSONSerialization.jsonObject(with: NppsCore.parse(String(contentsOf: url, encoding: .utf8))) as? [[String: Any]])
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
}
