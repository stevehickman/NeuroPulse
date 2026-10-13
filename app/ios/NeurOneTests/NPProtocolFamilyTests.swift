//
//  NPProtocolFamilyTests.swift
//  NeurOneTests
//
//  The library's rules as the iOS library asks the shared core for them (SWR-PL-02/03/04/06, SWR-FM-03/05,
//  SWR-UP-02, SWR-HV-04). The same cases run on the Kotlin library, so the apps cannot drift apart.
//
//  Subject under test: NPProtocolLibrary (families extension), NPLibraryRules
//  (app/ios/NeurOne/Protocol/NPProtocolLibrary.swift, NPProtocolFamilies.swift)

import XCTest
@testable import NeurOne

@MainActor
final class NPProtocolFamilyTests: XCTestCase {

    private let stateKeys = [
        NPProtocolLibrary.usedFamiliesKey, NPProtocolLibrary.archivedFamiliesKey, NPProtocolLibrary.cloneSourcesKey,
    ]

    override func setUp() async throws {
        stateKeys.forEach { UserDefaults.standard.removeObject(forKey: $0) }
    }

    override func tearDown() async throws {
        let library = NPProtocolLibrary()
        for entry in library.userProtocols { library.delete(entry.id) }
        stateKeys.forEach { UserDefaults.standard.removeObject(forKey: $0) }
    }

    private func gamma(_ library: NPProtocolLibrary) throws -> NPProtocolEntry {
        try XCTUnwrap(library.bundledProtocols.first { $0.name == "Gamma Focus" })
    }

    private func clone(_ library: NPProtocolLibrary) throws -> NPProtocolEntry {
        let source = try gamma(library)
        return try XCTUnwrap(library.cloneFamily(source.id, copyName: protocolCopyName).first)
    }

    private func use(_ library: NPProtocolLibrary, _ entry: NPProtocolEntry) throws {
        let hash = try XCTUnwrap(library.contentHash(entry))
        XCTAssertEqual(library.recordUploadResult(entry, sentHash: hash, helmetStatus: "accepted", helmetHash: hash), .confirmed)
    }

    // @verifies SWR-FM-03
    func testACloneIsANewUnusedEditableFamily() throws {
        let library = NPProtocolLibrary()
        let source = try gamma(library)
        let copy = try clone(library)
        XCTAssertNotEqual(source.id, copy.id)
        XCTAssertNotEqual(source.familyKey, copy.familyKey)
        XCTAssertFalse(copy.isReadOnly)
        XCTAssertFalse(library.isUsed(copy))
        XCTAssertEqual(library.save(copy), .saved)
    }

    // @verifies SWR-PL-04
    func testAPredefinedProtocolIsNeverSavedOver() throws {
        let library = NPProtocolLibrary()
        let source = try gamma(library)
        XCTAssertEqual(library.save(source), .readOnly)
        XCTAssertEqual(library.delete(source.id), .readOnly)
    }

    // @verifies SWR-PL-02
    func testASaveIntoAUsedProtocolIsRefusedAndWritesNothing() throws {
        let library = NPProtocolLibrary()
        let copy = try clone(library)
        try use(library, copy)
        XCTAssertTrue(library.isUsed(copy))
        guard case .refusedFamilyUsed = library.save(copy) else { return XCTFail("a used protocol must refuse a save") }
    }

    // @verifies SWR-FM-05
    func testAUsedProtocolIsArchivedNotDeleted() throws {
        let library = NPProtocolLibrary()
        let copy = try clone(library)
        try use(library, copy)
        XCTAssertEqual(library.delete(copy.id), .refusedFamilyUsed)
        library.archiveFamily(copy)
        XCTAssertTrue(library.isArchived(copy))
        XCTAssertFalse(library.offeredProtocols.contains { $0.id == copy.id })
        library.unarchiveFamily(copy)
        XCTAssertTrue(library.offeredProtocols.contains { $0.id == copy.id })
    }

    // @verifies SWR-PL-06
    func testAnUnmodifiedCloneIsStoppedBeforeUpload() throws {
        let library = NPProtocolLibrary()
        let source = try gamma(library)
        let copy = try clone(library)
        XCTAssertEqual(library.uploadGate(copy, helmetHardwareVersion: 1), .unmodifiedClone(sourceId: source.id))
    }

    // @verifies SWR-HV-04
    func testAHelmetLayoutTheProtocolHasNoVersionForIsRefused() throws {
        let library = NPProtocolLibrary()
        let copy = try clone(library)
        guard case .hardwareMismatch = library.uploadGate(copy, helmetHardwareVersion: 99) else {
            return XCTFail("a helmet version the protocol has no layout for must be refused")
        }
    }

    // @verifies SWR-UP-02
    func testOnlyAConfirmedOrUnknownUploadMarksAFamilyUsed() throws {
        let library = NPProtocolLibrary()
        let copy = try clone(library)
        let hash = try XCTUnwrap(library.contentHash(copy))
        let wrong = String(repeating: "0", count: 64)
        XCTAssertEqual(
            library.recordUploadResult(copy, sentHash: hash, helmetStatus: "accepted", helmetHash: wrong),
            .confirmedFailed)
        XCTAssertEqual(
            library.recordUploadResult(copy, sentHash: hash, helmetStatus: nil, helmetHash: nil, refusedBeforeSend: true),
            .notSent)
        XCTAssertFalse(library.isUsed(copy))
        XCTAssertEqual(library.recordUploadResult(copy, sentHash: hash, helmetStatus: nil, helmetHash: nil), .unknown)
        XCTAssertTrue(library.isUsed(copy), "the helmet may hold it")
    }

    // @verifies SWR-WO-02
    func testTheUsedFlagSurvivesAReload() throws {
        let library = NPProtocolLibrary()
        let copy = try clone(library)
        try use(library, copy)
        let reloaded = NPProtocolLibrary()
        let again = try XCTUnwrap(reloaded.userProtocols.first { $0.id == copy.id })
        XCTAssertTrue(reloaded.isUsed(again))
    }
}
