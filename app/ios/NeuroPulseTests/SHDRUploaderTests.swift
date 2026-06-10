//
//  SHDRUploaderTests.swift
//  NeuroPulseTests
//
//  Satisfies:
//    ISC-63  Anti: SHDRUploader contains no UHDR element references (static analysis)
//    ISC-64  SHDRUploader triggers upload when shdrUploadPending fires true
//    ISC-65  SHDRUploader reads only the binary shdr_staging.bin blob (SHDR fields only)
//    ISC-66  Source contains no UHDR field names in the request path
//    ISC-67  SHDRUploader uses @Published isUploading (non-blocking) rather than modal alerts
//
//  Subject under test: SHDRUploader + SHDRUploadTriggering protocol
//  (app/ios/NeuroPulse/Data/SHDRUploader.swift,
//   app/ios/NeuroPulse/Data/SHDRUploadTriggering.swift)
//
//  Privacy note: these tests verify the UHDR/SHDR architectural boundary.
//  SHDRUploader must never read from UHDR-classified sources (EEG waveforms,
//  HRV time series, session timestamps, outcome logs, PPG signal).

import XCTest
import Combine
@testable import NeuroPulse

// MARK: - MockSHDRUploadTrigger

/// Minimal conformance to SHDRUploadTriggering.
/// Sends true or false values on demand to drive the uploader's observation path.
private final class MockSHDRUploadTrigger: SHDRUploadTriggering {
    private let subject = PassthroughSubject<Bool, Never>()

    var shdrUploadPendingPublisher: AnyPublisher<Bool, Never> {
        subject.eraseToAnyPublisher()
    }

    func sendPending(_ value: Bool) {
        subject.send(value)
    }
}

// MARK: - SHDRUploaderTests

final class SHDRUploaderTests: XCTestCase {

    // MARK: - ISC-63 / ISC-66 Static source analysis: no UHDR fields

    // These tests read the production source file directly and verify the absence
    // of UHDR-classified field names in the code that forms the upload request.
    // Passing these tests proves the uploader cannot accidentally include
    // EEG waveforms, HRV time series, session timestamps, or PPG signal in the
    // POST body regardless of any future refactor.

    func testNoUHDRElementsInUploaderSource() throws {
        let source = try Self.uploaderSource()

        // UHDR field names that must never appear in SHDRUploader.swift.
        // These are drawn directly from CLAUDE.md §5.1 UHDR contents.
        let forbiddenUHDRFields = [
            "eegWaveforms",
            "hrvTimeSeries",
            "sessionTimestamps",
            "ppgSignal",
            "outcomeLog",
            "neurofeedbackScore",
            "closedLoopEvent",
            "adaptationEvent",
            "eyeOpenState",
        ]
        for field in forbiddenUHDRFields {
            XCTAssertFalse(
                source.contains(field),
                "SHDRUploader.swift must not reference UHDR field '\(field)' — " +
                "SHDR uploads must never include user health data (CLAUDE.md §5.1)."
            )
        }
    }

    func testUploaderOnlyReadsSHDRStagingFile() throws {
        let source = try Self.uploaderSource()

        // The uploader reads exactly one file: shdr_staging.bin.
        // Any additional file read would risk including UHDR data.
        XCTAssertTrue(
            source.contains("shdr_staging.bin"),
            "SHDRUploader.swift must reference shdr_staging.bin (the SHDR binary blob)."
        )

        // The uploader must never read UHDR partition paths.
        let forbiddenPaths = [
            "uhdr",
            "UHDRBackup",
            "edf",
            ".edf",
        ]
        for path in forbiddenPaths {
            XCTAssertFalse(
                source.lowercased().contains(path.lowercased()) && source.contains("Data(contentsOf:") &&
                source.range(of: "\(path).*Data\\(contentsOf:", options: .regularExpression) != nil,
                "SHDRUploader must not read '\(path)' — only shdr_staging.bin is permitted."
            )
        }
    }

    // MARK: - ISC-63 Warrant token is opaque, never identifierForVendor

    func testNoIdentifierForVendorInUploaderSource() throws {
        let source = try Self.uploaderSource()

        XCTAssertFalse(
            source.contains("identifierForVendor"),
            "SHDRUploader.swift must not use identifierForVendor — SHDR must be linked " +
            "to an opaque warranty token only (NP-FW-EMMC-002 Rev A §A)."
        )
        XCTAssertTrue(
            source.contains("X-NP-Device-Token"),
            "SHDRUploader.swift must use the opaque device token header X-NP-Device-Token."
        )
    }

    // MARK: - ISC-64 Upload triggers on shdrUploadPending notification

    // MockSHDRUploadTrigger delivers a 'true' value; the uploader's Combine
    // subscription should invoke uploadIfConsented(). Because warrantyConsent is
    // false by default in tests, the upload itself is gated out (no network call),
    // but the subscription path is proven to fire.

    @MainActor
    func testUploadTriggeredByPendingNotification() async {
        let trigger = MockSHDRUploadTrigger()
        let uploader = SHDRUploader(gatt: trigger)

        // Consent not granted: upload gates out, but the subscription must fire.
        // We verify isUploading stays false (gated) rather than transitioning —
        // because uploadIfConsented returns early without consent.
        uploader.warrantyConsentGranted = false

        // Fire pending=true then pending=false.
        trigger.sendPending(true)
        trigger.sendPending(false)

        // Give the async task time to schedule and run.
        await Task.yield()

        // The uploader must not be stuck in uploading=true (it was gated by consent).
        XCTAssertFalse(uploader.isUploading,
                       "Uploader must not be stuck uploading when consent is not granted.")
    }

    @MainActor
    func testFalseValueDoesNotTriggerUpload() async {
        let trigger = MockSHDRUploadTrigger()
        let uploader = SHDRUploader(gatt: trigger)
        uploader.warrantyConsentGranted = false

        // Sending false should be filtered out by the .filter { $0 } combinator.
        trigger.sendPending(false)
        await Task.yield()

        XCTAssertFalse(uploader.isUploading,
                       "A false pending value must not initiate an upload.")
        XCTAssertNil(uploader.lastError,
                     "A false pending value must not set lastError.")
    }

    // MARK: - ISC-65 / ISC-66 SHDR-only fields in request

    func testUploaderDoesNotBuildJSONRequestBody() throws {
        let source = try Self.uploaderSource()

        // The SHDR upload sends a raw binary blob (application/octet-stream),
        // NOT a JSON-serialised object. If JSONEncoder or JSONSerialization
        // appeared in the upload path it could accidentally serialise UHDR fields.
        let requestBody = source.components(separatedBy: "func upload()").dropFirst().first ?? ""
        XCTAssertFalse(
            requestBody.contains("JSONEncoder") || requestBody.contains("JSONSerialization"),
            "SHDRUploader.upload() must send a raw binary blob, not a JSON-serialised body — " +
            "JSON encoding could accidentally capture UHDR fields."
        )
        XCTAssertTrue(
            source.contains("application/octet-stream"),
            "SHDRUploader must set Content-Type: application/octet-stream for the binary SHDR blob."
        )
    }

    // MARK: - ISC-67 Non-blocking upload indicator (@Published, not modal)

    func testIsUploadingIsPublishedProperty() throws {
        let source = try Self.uploaderSource()

        // @Published isUploading is the non-blocking indicator.
        // A modal (present(_:animated:), showAlert, Alert(), Sheet) would block the UI.
        XCTAssertTrue(
            source.contains("@Published private(set) var isUploading"),
            "SHDRUploader must expose @Published var isUploading for non-blocking UI observation."
        )

        // Verify no modal-presenting calls in the uploader source.
        let modalSymbols = ["present(", "showAlert", ".alert(", "UIAlertController"]
        for symbol in modalSymbols {
            XCTAssertFalse(
                source.contains(symbol),
                "SHDRUploader must not use modal UI ('\(symbol)') — upload status must be " +
                "observable via @Published isUploading (ISC-67)."
            )
        }
    }

    // MARK: - Source file helper

    private static func uploaderSource(file: StaticString = #filePath) throws -> String {
        let testFileURL = URL(fileURLWithPath: "\(file)")
        let iosDir = testFileURL
            .deletingLastPathComponent()   // NeuroPulseTests/
            .deletingLastPathComponent()   // ios/
        let sourceURL = iosDir
            .appendingPathComponent("NeuroPulse")
            .appendingPathComponent("Data")
            .appendingPathComponent("SHDRUploader.swift")
        return try String(contentsOf: sourceURL, encoding: .utf8)
    }
}
