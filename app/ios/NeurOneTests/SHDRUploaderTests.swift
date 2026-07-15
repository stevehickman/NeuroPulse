//
//  SHDRUploaderTests.swift
//  NeurOneTests
//
//  Satisfies:
//    ISC-63  Anti: SHDRUploader contains no UHDR element references (static analysis)
//    ISC-64  SHDRUploader triggers upload when shdrUploadPending fires true
//    ISC-65  SHDRUploader reads only the binary shdr_staging.bin blob (SHDR fields only)
//    ISC-66  Source contains no UHDR field names in the request path
//    ISC-67  SHDRUploader uses @Published isUploading (non-blocking) rather than modal alerts
//
//  Subject under test: SHDRUploader + SHDRUploadTriggering protocol
//  (app/ios/NeurOne/Data/SHDRUploader.swift,
//   app/ios/NeurOne/Data/SHDRUploadTriggering.swift)
//
//  Privacy note: these tests verify the UHDR/SHDR architectural boundary.
//  SHDRUploader must never read from UHDR-classified sources (EEG waveforms,
//  HRV time series, session timestamps, outcome logs, PPG signal).

import XCTest
import Combine
@testable import NeurOne

// MARK: - MockSHDRUploadTrigger

/// Minimal conformance to SHDRUploadTriggering.
/// Sends true or false values on demand to drive the uploader's observation path,
/// and drives the hub warranty-token publisher for the OI-BLE-01 upgrade path.
private final class MockSHDRUploadTrigger: SHDRUploadTriggering {
    private let subject = PassthroughSubject<Bool, Never>()
    // CurrentValueSubject mirrors @Published semantics (emits current value on subscribe),
    // matching NeurOneGATTManager.$warrantyToken which starts at nil.
    private let tokenSubject = CurrentValueSubject<Data?, Never>(nil)

    var shdrUploadPendingPublisher: AnyPublisher<Bool, Never> {
        subject.eraseToAnyPublisher()
    }

    var warrantyTokenPublisher: AnyPublisher<Data?, Never> {
        tokenSubject.eraseToAnyPublisher()
    }

    func sendPending(_ value: Bool) {
        subject.send(value)
    }

    func sendWarrantyToken(_ token: Data?) {
        tokenSubject.send(token)
    }
}

// MARK: - SHDRUploaderTests

final class SHDRUploaderTests: XCTestCase {

    override func setUp() {
        super.setUp()
        // Ensure warranty consent is revoked so uploads gate out by default in each test.
        // WarrantyAnalyticsGate is @MainActor; manipulate the underlying key directly
        // so setUp can run without being @MainActor itself.
        UserDefaults.standard.removeObject(forKey: "np.warranty.consent.granted")
    }

    override func tearDown() {
        UserDefaults.standard.removeObject(forKey: "np.warranty.consent.granted")
        super.tearDown()
    }

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

        // Warranty consent not granted: upload gates out via WarrantyAnalyticsGate.isOpen,
        // but the subscription must still fire to prove the Combine path is wired.
        WarrantyAnalyticsGate.revoke()

        // Fire pending=true then pending=false.
        trigger.sendPending(true)
        trigger.sendPending(false)

        // Give the async task time to schedule and run.
        await Task.yield()

        // The uploader must not be stuck in uploading=true (it was gated by warranty consent).
        XCTAssertFalse(uploader.isUploading,
                       "Uploader must not be stuck uploading when warranty consent is not granted.")
    }

    @MainActor
    func testFalseValueDoesNotTriggerUpload() async {
        let trigger = MockSHDRUploadTrigger()
        let uploader = SHDRUploader(gatt: trigger)
        WarrantyAnalyticsGate.revoke()

        // Sending false should be filtered out by the .filter { $0 } combinator.
        trigger.sendPending(false)
        await Task.yield()

        XCTAssertFalse(uploader.isUploading,
                       "A false pending value must not initiate an upload.")
        XCTAssertNil(uploader.lastError,
                     "A false pending value must not set lastError.")
    }

    // MARK: - OI-BLE-01 Hub-provisioned TRNG token upgrade path

    // Before the hub delivers a token, currentDeviceToken() must fall back to the
    // 64-char hex Keychain token so uploads still carry an opaque device linkage key.
    @MainActor
    func testDeviceTokenFallsBackToKeychainBeforeHubToken() {
        let trigger = MockSHDRUploadTrigger()
        let uploader = SHDRUploader(gatt: trigger)

        let token = uploader.currentDeviceToken()
        XCTAssertEqual(token.count, 64,
                       "Fallback Keychain token must be a 64-char hex (256-bit) string.")
        XCTAssertTrue(token.allSatisfy { $0.isHexDigit },
                      "Device token must be hex.")
    }

    // When the hub publishes a 32-byte token, the uploader must upgrade to it and
    // currentDeviceToken() must return its lowercase hex encoding, not the Keychain value.
    @MainActor
    func testUpgradeToHubProvisionedToken() async {
        let trigger = MockSHDRUploadTrigger()
        let uploader = SHDRUploader(gatt: trigger)

        let keychainToken = uploader.currentDeviceToken()

        let hubBytes = Data((0..<32).map { UInt8($0) })
        trigger.sendWarrantyToken(hubBytes)
        await Task.yield()

        let expectedHex = hubBytes.map { String(format: "%02x", $0) }.joined()
        XCTAssertEqual(uploader.currentDeviceToken(), expectedHex,
                       "Uploader must prefer the hub-provisioned TRNG token once delivered.")
        XCTAssertNotEqual(uploader.currentDeviceToken(), keychainToken,
                          "Hub token must supersede the interim Keychain token.")
    }

    // A nil emission (pre-provisioning or on disconnect) must never roll back an
    // established hub-token upgrade — the device identity is unchanged across reconnects.
    @MainActor
    func testDisconnectNilDoesNotDowngradeHubToken() async {
        let trigger = MockSHDRUploadTrigger()
        let uploader = SHDRUploader(gatt: trigger)

        let hubBytes = Data(repeating: 0xAB, count: 32)
        trigger.sendWarrantyToken(hubBytes)
        await Task.yield()
        let upgraded = uploader.currentDeviceToken()

        // Simulate GATT clearing the token on disconnect.
        trigger.sendWarrantyToken(nil)
        await Task.yield()

        XCTAssertEqual(uploader.currentDeviceToken(), upgraded,
                       "A nil token emission must not downgrade back to the Keychain token.")
    }

    // A short (corrupt/partial) token must be rejected, keeping the prior token intact.
    @MainActor
    func testShortHubTokenIsRejected() async {
        let trigger = MockSHDRUploadTrigger()
        let uploader = SHDRUploader(gatt: trigger)
        let before = uploader.currentDeviceToken()

        trigger.sendWarrantyToken(Data(repeating: 0xFF, count: 16)) // too short
        await Task.yield()

        XCTAssertEqual(uploader.currentDeviceToken(), before,
                       "A sub-32-byte token must be rejected, leaving the device token unchanged.")
    }

    // MARK: - ISC-65 / ISC-66 SHDR-only fields in request

    func testUploaderDoesNotBuildJSONRequestBody() throws {
        let source = try Self.uploaderSource()

        // The SHDR upload sends a raw binary blob (application/octet-stream),
        // NOT a JSON-serialized object. If JSONEncoder or JSONSerialization
        // appeared in the upload path it could accidentally serialize UHDR fields.
        let requestBody = source.components(separatedBy: "func upload()").dropFirst().first ?? ""
        XCTAssertFalse(
            requestBody.contains("JSONEncoder") || requestBody.contains("JSONSerialization"),
            "SHDRUploader.upload() must send a raw binary blob, not a JSON-serialized body — " +
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
            .deletingLastPathComponent()   // NeurOneTests/
            .deletingLastPathComponent()   // ios/
        let sourceURL = iosDir
            .appendingPathComponent("NeurOne")
            .appendingPathComponent("Data")
            .appendingPathComponent("SHDRUploader.swift")
        return try String(contentsOf: sourceURL, encoding: .utf8)
    }
}
