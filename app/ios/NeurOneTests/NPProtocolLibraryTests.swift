//
//  NPProtocolLibraryTests.swift
//  NeurOneTests
//
//  Satisfies:
//    ISC-39  NPProtocolLibrary loads every predefined NPPS protocol template
//            without parse errors; all are read-only. The expected count comes
//            from protocols/predefined/manifest.json, not a literal — the
//            bundle is the shipped .npps library itself, copied in by the
//            Resources build phase.
//
//  Subject under test: NPProtocolLibrary + NPBundledProtocols
//  (app/ios/NeurOne/Protocol/NPProtocolLibrary.swift,
//   app/ios/NeurOne/Protocol/NPBundledProtocols.swift)

import XCTest
@testable import NeurOne

final class NPProtocolLibraryTests: XCTestCase {

    // MARK: - ISC-39 Every predefined template loads without errors

    @MainActor
    // @verifies REQ-NPPS-18
    func testBundledProtocolCountMatchesManifest() {
        let library = NPProtocolLibrary()
        // protocolFiles, not manifestFiles: the manifest also lists the zone and
        // condition definition files, which load into the namespace but are not
        // library items.
        let expected = NPBundledProtocols.protocolFiles.count
        XCTAssertGreaterThan(
            expected, 0,
            "manifest.json listed no protocols — the protocols/predefined folder reference " +
            "is missing from the Resources build phase."
        )
        XCTAssertEqual(
            library.bundledProtocols.count, expected,
            "bundledProtocols must contain one entry per protocol and composite in " +
            "protocols/predefined/manifest.json. A lower count indicates a parse failure " +
            "in loadBundledProtocols()."
        )
    }

    @MainActor
    // @verifies SWR-PL-04
    func testAllBundledProtocolsAreReadOnly() {
        let library = NPProtocolLibrary()
        for entry in library.bundledProtocols {
            XCTAssertTrue(
                entry.isReadOnly,
                "Bundled protocol '\(entry.name)' must be read-only — " +
                "predefined templates must be immutable (NPPS readonly: true field)."
            )
            XCTAssertFalse(
                library.canEdit(entry),
                "canEdit must return false for bundled protocol '\(entry.name)'."
            )
            XCTAssertFalse(
                library.canDelete(entry),
                "canDelete must return false for bundled protocol '\(entry.name)'."
            )
        }
    }

    @MainActor
    // @verifies REQ-NPPS-18
    func testAllBundledProtocolNamesAreUnique() {
        let library = NPProtocolLibrary()
        let names = library.bundledProtocols.map { $0.name }
        let uniqueNames = Set(names)
        XCTAssertEqual(
            names.count, uniqueNames.count,
            "All bundled protocol names must be unique — " +
            "duplicate names indicate a copy-paste error in NPBundledProtocols."
        )
    }

    @MainActor
    // @verifies REQ-NPPS-18
    func testAllBundledProtocolIDsAreUnique() {
        let library = NPProtocolLibrary()
        let ids = library.bundledProtocols.map { $0.id }
        let uniqueIDs = Set(ids)
        XCTAssertEqual(
            ids.count, uniqueIDs.count,
            "All bundled protocol UUIDs must be unique — duplicate IDs would break " +
            "lookup and persistence logic."
        )
    }

    // MARK: - ISC-39 Specific expected protocol names

    // Verify that each expected predefined template is present by name.
    // This guards against accidentally removing a template or misspelling its name
    // in NPBundledProtocols.allContents.

    @MainActor
    // @verifies REQ-NPPS-18
    func testExpectedSingleProtocolNamesPresent() {
        let library = NPProtocolLibrary()
        let names = Set(library.bundledProtocols.map { $0.name })

        let expectedNames = [
            "Gamma Focus",
            "Alpha Calm",
            "Deep Sleep",
            "Memory Boost",
            "Anxiety Relief",
            "Flow State",
            "Vascular Baseline",
            "ADHD Focus",
            "Retinal Health",
            "PTSD EMDR Support",
            "HRV Coherence Training",
            "HRV + taVNS Synchronised",
            "Focus Prime",
            "Gamma + Theta Coupled",
            "Full T1 Immersive",
        ]
        for name in expectedNames {
            XCTAssertTrue(
                names.contains(name),
                "Expected bundled protocol '\(name)' not found in library. " +
                "Check NPBundledProtocols.allContents and the NPPS 'name' field."
            )
        }
    }

    @MainActor
    // @verifies REQ-NPPS-18
    func testExpectedCompositeProtocolNamesPresent() {
        let library = NPProtocolLibrary()
        let names = Set(library.bundledProtocols.map { $0.name })

        let expectedCompositeNames = [
            "Full Multi-Modal RCT",
            "Sleep Optimisation Stack",
            "Calm to Focus",
            "Sleep Wind-Down",
        ]
        for name in expectedCompositeNames {
            XCTAssertTrue(
                names.contains(name),
                "Expected composite protocol '\(name)' not found in library. " +
                "Check NPBundledProtocols.allContents."
            )
        }
    }

    // MARK: - ISC-39 allContents matches the manifest

    // @verifies REQ-NPPS-18
    func testBundledProtocolsSourceListCount() {
        XCTAssertEqual(
            NPBundledProtocols.allContents.count, NPBundledProtocols.manifestFiles.count,
            "NPBundledProtocols.allContents must load every file manifest.json lists, " +
            "definitions included. A shortfall means a listed .npps file is missing from " +
            "the app bundle."
        )
    }

    // Each entry in allContents must be non-empty and contain a protocol or composite block.
    // @verifies REQ-NPPS-18
    func testAllBundledProtocolStringsAreNonEmpty() {
        for (index, content) in NPBundledProtocols.allContents.enumerated() {
            let trimmed = content.trimmingCharacters(in: .whitespacesAndNewlines)
            XCTAssertFalse(
                trimmed.isEmpty,
                "NPBundledProtocols.allContents[\(index)] must not be empty."
            )
            // Definition files are in here too: manifest.json lists 00-zones.npps
            // and 00-conditions.npps first so references resolve regardless of
            // file order (NP-NPPS-REF-001 §1.6).
            //
            // A file may open with a comment header — most of the shipped ones do
            // — so find the first line that is neither blank nor a comment before
            // looking for the keyword.
            let firstCode = trimmed
                .split(separator: "\n", omittingEmptySubsequences: false)
                .map { $0.trimmingCharacters(in: .whitespaces) }
                .first { !$0.isEmpty && !$0.hasPrefix("#") } ?? ""
            let startsWithTopLevelBlock = ["protocol ", "composite ", "zone ", "condition ", "limits "]
                .contains { firstCode.hasPrefix($0) }
            XCTAssertTrue(
                startsWithTopLevelBlock,
                "NPBundledProtocols.allContents[\(index)] must open with a top-level block " +
                "keyword. Got: '\(firstCode.prefix(40))…'"
            )
        }
    }

    // MARK: - Read-only enforcement: save/delete must be silently ignored

    @MainActor
    // @verifies SWR-PL-04
    func testSaveBundledProtocolIsIgnored() {
        let library = NPProtocolLibrary()
        guard let first = library.bundledProtocols.first else {
            XCTFail("Expected at least one bundled protocol.")
            return
        }
        let countBefore = library.bundledProtocols.count
        library.save(first)
        XCTAssertEqual(
            library.bundledProtocols.count, countBefore,
            "Saving a bundled (read-only) protocol must not change bundledProtocols count."
        )
        XCTAssertEqual(
            library.userProtocols.count, 0,
            "Saving a bundled protocol must not append it to userProtocols."
        )
    }

    @MainActor
    // @verifies SWR-PL-04
    func testDeleteBundledProtocolIsIgnored() {
        let library = NPProtocolLibrary()
        guard let first = library.bundledProtocols.first else {
            XCTFail("Expected at least one bundled protocol.")
            return
        }
        let countBefore = library.bundledProtocols.count
        library.delete(first.id)
        XCTAssertEqual(
            library.bundledProtocols.count, countBefore,
            "Deleting a bundled (read-only) protocol must not remove it from bundledProtocols."
        )
    }

    // MARK: - Values that must be quoted (NP-NPPS-REF-001 §2, Rev 6)

    // These three tests used to guard the OPPOSITE behaviour: that the lexer
    // read `660_808nm` and `wind-down` as bare tokens. Rev 6 removed both rules
    // so that every value is a plain identifier, a number, a boolean or a
    // quoted string — each of which maps onto a JSON scalar — and migrated the
    // shipped library. The guard is now that the quoted form works and the bare
    // form is refused with a message naming the fix.

    private func parseEntries(_ source: String) throws -> [NPProtocolEntry] {
        var lexer = NPPSLexer(source)
        var parser = NPPSParser(try lexer.tokenize())
        return try parser.parse()
    }

    private func wavelengthProtocol(_ value: String) -> String {
        """
        protocol "Wavelength Test" {
            hardware_version: 1
            version: "1.0"
            duration: 10m
            pbm_transcranial {
                wavelength: \(value)
                irradiance: 200mW_cm2
            }
        }
        """
    }

    // @verifies REQ-NPPS-03
    func testQuotedWavelengthsParse() throws {
        for wl in ["660nm", "808nm", "1064nm"] {
            let entries = try parseEntries(wavelengthProtocol("\"\(wl)\""))
            XCTAssertEqual(entries.count, 1, "quoted wavelength \"\(wl)\" must parse")
        }
    }

    /// Rev 18: the combined channel names are retired, and the refusal names the blocks that
    /// replace them.
    // @verifies REQ-NPPS-03#a
    func testRetiredCombinedWavelengthsAreRefused() {
        for wl in ["660_808nm", "660_808_1064nm"] {
            XCTAssertThrowsError(try parseEntries(wavelengthProtocol("\"\(wl)\""))) { error in
                let text = "\(error)"
                XCTAssertTrue(text.contains("retired") && text.contains("one block per wavelength"), text)
            }
        }
    }

    // @verifies REQ-NPPS-10#a
    func testUnquotedCompoundWavelengthIsRefused() {
        for wl in ["660_808nm", "660_808_1064nm"] {
            XCTAssertThrowsError(try parseEntries(wavelengthProtocol(wl)),
                                 "unquoted \(wl) must not parse") { error in
                XCTAssertTrue(
                    "\(error)".contains("must be quoted"),
                    "the error should name the fix, got: \(error)"
                )
            }
        }
    }

    // @verifies REQ-NPPS-10#a
    func testUnquotedHyphenatedTagIsRefused() {
        let input = """
        protocol "Hyphen Tag Test" {
            hardware_version: 1
            version: "1.0"
            tags: [sleep, wind-down, recovery]
            duration: 10m
        }
        """
        XCTAssertThrowsError(try parseEntries(input), "an unquoted hyphenated tag must not parse")
    }

    // @verifies REQ-NPPS-10#a
    func testQuotedHyphenatedTagsParse() throws {
        let input = """
        protocol "Hyphen Tag Test" {
            hardware_version: 1
            version: "1.0"
            tags: [sleep, "wind-down", "all-modalities", recovery]
            duration: 10m
        }
        """
        let entries = try parseEntries(input)
        XCTAssertEqual(entries.count, 1)
        guard case .single(let proto)? = entries.first else {
            return XCTFail("expected a single protocol")
        }
        XCTAssertEqual(proto.tags, ["sleep", "wind-down", "all-modalities", "recovery"])
    }

    // MARK: - Serializer round-trip: hyphenated tags must be quoted on emit

    // Guards against serialize→reparse silently splitting "wind-down" into
    // ["wind", "down"] by ensuring the serializer quotes tags containing hyphens.
    // @verifies REQ-NPPS-11
    func testHyphenatedTagRoundTripPreservesValue() throws {
        let input = """
        protocol "Hyphen Round Trip" {
            hardware_version: 1
            id: "AABB0004-0000-0000-0000-000000000000"
            name: "Hyphen Round Trip"
            description: "Round-trip test"
            author: "Test"
            version: "1.0"
            readonly: false
            tags: [sleep, "wind-down", recovery]
            duration: 10m
        }
        """
        let roundTripped = try nppsRoundTrip(input)
        // After serialize, re-lex and re-parse to get the final tags.
        var lexer2 = NPPSLexer(roundTripped)
        let tokens2 = try lexer2.tokenize()
        var parser2 = NPPSParser(tokens2)
        let entries2 = try parser2.parse()
        guard case .single(let proto) = entries2.first else {
            XCTFail("Round-tripped entry must be a .single protocol.")
            return
        }
        XCTAssertTrue(
            proto.tags.contains("wind-down"),
            "Round-trip must preserve hyphenated tag 'wind-down' as a single value — " +
            "serializer must quote tags containing non-ident characters (serializer regression)."
        )
        XCTAssertFalse(
            proto.tags.contains("wind") && proto.tags.contains("down"),
            "'wind-down' must not be split into separate 'wind' and 'down' tags after round-trip."
        )
    }

    // MARK: - allProtocols combines bundled + user protocols

    @MainActor
    // @verifies REQ-NPPS-18
    func testAllProtocolsIncludesBundledProtocols() {
        let library = NPProtocolLibrary()
        XCTAssertEqual(
            library.allProtocols.count,
            library.bundledProtocols.count + library.userProtocols.count,
            "allProtocols must equal bundledProtocols + userProtocols."
        )
        // All bundled protocols must appear in allProtocols.
        let allIDs = Set(library.allProtocols.map { $0.id })
        for entry in library.bundledProtocols {
            XCTAssertTrue(
                allIDs.contains(entry.id),
                "Bundled protocol '\(entry.name)' must appear in allProtocols."
            )
        }
    }
}
