import XCTest
@testable import NeurOne

/// Condition link policy — NP-COND-LINK-001 §3.
///
/// Driven by `npps/fixtures/condition_links.json`, the same vector file the
/// TypeScript, Kotlin, and C# suites run. A verdict that differs here from the
/// other platforms is a parity break, not a local test failure.
final class ConditionLinkPolicyTests: XCTestCase {

    private struct Vector: Decodable {
        let name: String
        let url: String
        let verdict: String
        let reason: String
        let host: String?
    }

    private struct VectorFile: Decodable {
        let vectors: [Vector]
    }

    /// Repo root derived from this source file's location:
    /// <root>/app/ios/NeurOneTests/ConditionLinkPolicyTests.swift
    private static var repoRoot: URL {
        URL(fileURLWithPath: #filePath)
            .deletingLastPathComponent()   // NeurOneTests
            .deletingLastPathComponent()   // ios
            .deletingLastPathComponent()   // app
            .deletingLastPathComponent()   // <root>
    }

    private static func loadVectors() throws -> [Vector] {
        let url = repoRoot
            .appendingPathComponent("npps")
            .appendingPathComponent("fixtures")
            .appendingPathComponent("condition_links.json")
        let data = try Data(contentsOf: url)
        return try JSONDecoder().decode(VectorFile.self, from: data).vectors
    }

    // MARK: - Shared cross-platform vectors

    func testSharedVectors() throws {
        let vectors = try Self.loadVectors()
        XCTAssertGreaterThan(vectors.count, 25, "shared vector file looks truncated")

        for v in vectors {
            let got = NPConditionLinkPolicy.evaluate(v.url)

            XCTAssertEqual(
                got.allowed, v.verdict == "allow",
                "verdict mismatch for \(v.name): \(v.url)"
            )
            XCTAssertEqual(
                got.reason.rawValue, v.reason,
                "reason mismatch for \(v.name): \(v.url)"
            )
            if let expectedHost = v.host {
                XCTAssertEqual(got.host, expectedHost, "host mismatch for \(v.name)")
            }
        }
    }

    func testBlockedVerdictsCarryNoURL() throws {
        for v in try Self.loadVectors() where v.verdict == "block" {
            let got = NPConditionLinkPolicy.evaluate(v.url)
            XCTAssertNil(got.url, "blocked link must not expose a URL: \(v.name)")
            XCTAssertNil(got.host, "blocked link must not expose a host: \(v.name)")
        }
    }

    func testAllowedVerdictsCarryTrimmedURL() throws {
        for v in try Self.loadVectors() where v.verdict == "allow" {
            let got = NPConditionLinkPolicy.evaluate(v.url)
            XCTAssertEqual(
                got.url,
                v.url.trimmingCharacters(in: .whitespacesAndNewlines),
                "allowed link must expose the trimmed URL: \(v.name)"
            )
        }
    }

    func testEveryBlockingReasonHasAMessage() throws {
        let reasons = Set(try Self.loadVectors().filter { $0.verdict == "block" }.map { $0.reason })
        for raw in reasons {
            let reason = NPLinkReason(rawValue: raw)
            XCTAssertNotNil(reason, "unknown reason in vectors: \(raw)")
            XCTAssertFalse(reason!.blockedMessage.isEmpty, "no message for \(raw)")
        }
        XCTAssertTrue(NPLinkReason.ok.blockedMessage.isEmpty)
    }

    // MARK: - The shipped registry

    func testBundledRegistryIsPopulated() {
        XCTAssertGreaterThan(NPBundledConditions.all.count, 20)
        XCTAssertEqual(
            NPBundledConditions.all.count,
            NPBundledConditions.byName.count,
            "duplicate condition names in the generated registry"
        )
    }

    func testEveryBundledLinkPassesPolicy() {
        let rejected = NPBundledConditions.all
            .filter { !NPConditionLinkPolicy.evaluate($0.link).allowed }
            .map { $0.name }

        XCTAssertEqual(rejected, [], "bundled conditions must all have openable links")
    }

    /// The generated table must not drift from the .npps registry. Guards the
    /// case where someone edits 00-conditions.npps without re-running
    /// scripts/sync-conditions.ts.
    func testGeneratedTableMatchesRegistrySource() throws {
        let registry = Self.repoRoot
            .appendingPathComponent("protocols")
            .appendingPathComponent("predefined")
            .appendingPathComponent("00-conditions.npps")
        let source = try String(contentsOf: registry, encoding: .utf8)

        let blockCount = source.components(separatedBy: "\ncondition ").count - 1
        XCTAssertEqual(
            NPBundledConditions.all.count, blockCount,
            "generated table is stale — run: bun scripts/sync-conditions.ts"
        )

        for condition in NPBundledConditions.all {
            XCTAssertTrue(
                source.contains(condition.link),
                "\(condition.name) link is not in the registry source — table is stale"
            )
        }
    }

    // MARK: - Resolution

    func testResolvePreservesOrderAndArity() {
        let names = ["Insomnia", "Not In Registry", "Migraine"]
        let resolved = NPBundledConditions.resolve(names)

        XCTAssertEqual(resolved.map(\.name), names)
        XCTAssertNotNil(resolved[0].definition)
        XCTAssertTrue(resolved[0].verdict.allowed)
        XCTAssertNil(resolved[1].definition)
        XCTAssertFalse(resolved[1].verdict.allowed)
        XCTAssertNotNil(resolved[2].definition)
    }

    func testResolveKnownConditionExposesCodeAndHost() {
        guard let resolved = NPBundledConditions.resolve(["Major Depressive Disorder"]).first else {
            return XCTFail("expected one resolved condition")
        }
        XCTAssertEqual(resolved.definition?.code, "6A70")
        XCTAssertEqual(resolved.verdict.host, "en.wikipedia.org")
    }

    // MARK: - Spot checks independent of the vector file

    func testJavaScriptSchemeIsRefused() {
        XCTAssertEqual(
            NPConditionLinkPolicy.evaluate("javascript:alert(1)").reason,
            .schemeNotHTTPS
        )
    }

    func testHostSpoofViaUserinfoIsRefused() {
        let v = NPConditionLinkPolicy.evaluate("https://en.wikipedia.org@evil.example.com/x")
        XCTAssertEqual(v.reason, .embeddedCredentials)
        XCTAssertNil(v.host, "must never surface a spoofable host")
    }
}
