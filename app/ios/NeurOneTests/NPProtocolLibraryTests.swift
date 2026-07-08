//
//  NPProtocolLibraryTests.swift
//  NeurOneTests
//
//  Satisfies:
//    ISC-39  NPProtocolLibrary loads all 19 predefined NPPS protocol templates
//            without parse errors; bundledProtocols.count == 19; all are read-only.
//
//  Subject under test: NPProtocolLibrary + NPBundledProtocols
//  (app/ios/NeurOne/Protocol/NPProtocolLibrary.swift,
//   app/ios/NeurOne/Protocol/NPBundledProtocols.swift)

import XCTest
@testable import NeurOne

final class NPProtocolLibraryTests: XCTestCase {

    // MARK: - ISC-39 All 19 predefined templates load without errors

    @MainActor
    func testBundledProtocolCountIs19() {
        let library = NPProtocolLibrary()
        XCTAssertEqual(
            library.bundledProtocols.count, 19,
            "bundledProtocols must contain exactly 19 entries — 15 single-protocol " +
            "and 4 composite templates (CLAUDE.md §3, NPBundledProtocols.allContents). " +
            "A lower count indicates a parse failure in loadBundledProtocols()."
        )
    }

    @MainActor
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
            "HRV + taVNS Synchronized",
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
    func testExpectedCompositeProtocolNamesPresent() {
        let library = NPProtocolLibrary()
        let names = Set(library.bundledProtocols.map { $0.name })

        let expectedCompositeNames = [
            "Full Multi-Modal RCT",
            "Sleep Optimization Stack",
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

    // MARK: - ISC-39 allContents source list has 19 entries

    func testBundledProtocolsSourceListCount() {
        XCTAssertEqual(
            NPBundledProtocols.allContents.count, 19,
            "NPBundledProtocols.allContents must list exactly 19 protocol strings."
        )
    }

    // Each entry in allContents must be non-empty and contain a protocol or composite block.
    func testAllBundledProtocolStringsAreNonEmpty() {
        for (index, content) in NPBundledProtocols.allContents.enumerated() {
            let trimmed = content.trimmingCharacters(in: .whitespacesAndNewlines)
            XCTAssertFalse(
                trimmed.isEmpty,
                "NPBundledProtocols.allContents[\(index)] must not be empty."
            )
            let hasProtocolOrComposite =
                trimmed.hasPrefix("protocol ") || trimmed.hasPrefix("composite ")
            XCTAssertTrue(
                hasProtocolOrComposite,
                "NPBundledProtocols.allContents[\(index)] must start with 'protocol' " +
                "or 'composite' keyword. Got: '\(trimmed.prefix(30))…'"
            )
        }
    }

    // MARK: - Read-only enforcement: save/delete must be silently ignored

    @MainActor
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

    // MARK: - Lexer regression: compound digit-underscore idents (e.g. 660_808nm)

    // Guards against the lexer splitting "660_808nm" into two tokens.
    // Previously, "660_808nm" was split into .number(660) + .ident("_808nm") which
    // caused the parser to see "_808nm" as a spurious field key and throw.
    func testLexerCompoundDigitIdentRoundTrip() throws {
        let input = """
        protocol "Wavelength Test" {
            id: "AABB0001-0000-0000-0000-000000000000"
            name: "Wavelength Test"
            description: "Regression test"
            author: "Test"
            version: "1.0"
            readonly: false
            duration: 10m
            pbm_transcranial {
                enabled: true
                wavelength: 660_808nm
                intensity: 200mW_cm2
            }
        }
        """
        var lexer = NPPSLexer(input)
        let tokens = try lexer.tokenize()
        var parser = NPPSParser(tokens)
        let entries = try parser.parse()
        XCTAssertEqual(entries.count, 1,
            "Protocol with 'wavelength: 660_808nm' must parse as exactly one entry — " +
            "compound digit-underscore idents must not split mid-parse (lexer regression).")
    }

    // Verifies 660_808_1064nm also lexes as a single compound ident.
    func testLexerTriWavelengthCompoundIdent() throws {
        let input = """
        protocol "Tri Wavelength Test" {
            id: "AABB0002-0000-0000-0000-000000000000"
            name: "Tri Wavelength Test"
            description: "Regression test"
            author: "Test"
            version: "1.0"
            readonly: false
            duration: 10m
            pbm_transcranial {
                enabled: true
                wavelength: 660_808_1064nm
                intensity: 200mW_cm2
            }
        }
        """
        var lexer = NPPSLexer(input)
        let tokens = try lexer.tokenize()
        var parser = NPPSParser(tokens)
        let entries = try parser.parse()
        XCTAssertEqual(entries.count, 1,
            "Protocol with 'wavelength: 660_808_1064nm' must parse as exactly one entry.")
    }

    // MARK: - Lexer regression: hyphenated tags must not throw

    // Guards against the lexer treating '-' after an ident as a negative-number
    // prefix, which caused Double("-") → throw, silently dropping protocols with
    // tags like "wind-down" or "all-modalities".
    func testHyphenatedTagsDoNotThrow() throws {
        let input = """
        protocol "Hyphen Tag Test" {
            id: "AABB0003-0000-0000-0000-000000000000"
            name: "Hyphen Tag Test"
            description: "Regression test"
            author: "Test"
            version: "1.0"
            readonly: false
            tags: [sleep, wind-down, all-modalities, recovery]
            duration: 10m
        }
        """
        var lexer = NPPSLexer(input)
        let tokens = try lexer.tokenize()
        var parser = NPPSParser(tokens)
        let entries = try parser.parse()
        XCTAssertEqual(entries.count, 1,
            "Protocol with hyphenated tags (e.g. 'wind-down') must parse without error — " +
            "'-' after an ident must not be treated as a negative-number prefix (lexer regression).")
    }

    // MARK: - Serializer round-trip: hyphenated tags must be quoted on emit

    // Guards against serialize→reparse silently splitting "wind-down" into
    // ["wind", "down"] by ensuring the serializer quotes tags containing hyphens.
    func testHyphenatedTagRoundTripPreservesValue() throws {
        let input = """
        protocol "Hyphen Round Trip" {
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
