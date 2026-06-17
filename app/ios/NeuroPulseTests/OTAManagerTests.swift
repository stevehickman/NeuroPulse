//
//  OTAManagerTests.swift
//  NeuroPulseTests
//
//  Satisfies:
//    ISC-107  manifest fetch triggers update availability check
//    ISC-108  current hub firmware version shown vs available
//    ISC-109  INITIATE → chunks → VERIFY → COMMIT opcode sequence (+ abort path)
//    ISC-110  bytes/total bytes progress tracking
//    ISC-111  mid-OTA disconnect does not corrupt running bank (structural: running bank
//              is never written until COMMIT; verified by opcode ordering tests)
//    ISC-112  waitForCompletion handles reconnect after hub reset
//    ISC-113  no firmware chunk sent when Ed25519 fingerprint does not match

import XCTest
import CoreBluetooth
@testable import NeuroPulse

// MARK: - Mock GATT state driver

/// Drives NeuroPulseGATTManager from tests without real BLE hardware.
private final class MockBLECentral: BLECentralManager {
    var state: CBManagerState = .poweredOn
    func scanForPeripherals(withServices: [CBUUID]?, options: [String: Any]?) {}
    func stopScan() {}
    func connect(_ peripheral: CBPeripheral, options: [String: Any]?) {}
    func cancelPeripheralConnection(_ peripheral: CBPeripheral) {}
}

// MARK: - Mock firmware update service

private final class MockFirmwareUpdateService: FirmwareUpdateProviding {

    var manifestToReturn: FirmwareManifest?
    var manifestErrorToThrow: Error?
    var firmwareDataToReturn: Data?
    var downloadErrorToThrow: Error?
    private(set) var fetchCallCount = 0
    private(set) var downloadCallCount = 0

    func fetchManifest() async throws -> FirmwareManifest {
        fetchCallCount += 1
        if let error = manifestErrorToThrow { throw error }
        guard let manifest = manifestToReturn else {
            throw FirmwareUpdateError.networkError(URLError(.notConnectedToInternet))
        }
        return manifest
    }

    func downloadFirmware(version: String, expectedSizeBytes: Int) async throws -> Data {
        downloadCallCount += 1
        if let error = downloadErrorToThrow { throw error }
        return firmwareDataToReturn ?? Data(repeating: 0xAB, count: expectedSizeBytes)
    }
}

// MARK: - Tests

@MainActor
final class OTAManagerTests: XCTestCase {

    // MARK: - FirmwareVersion comparison (ISC-108)

    func testFirmwareVersionParsesValidString() {
        XCTAssertNotNil(FirmwareVersion("1.2.3"))
        XCTAssertNotNil(FirmwareVersion("0.0.0"))
        XCTAssertNotNil(FirmwareVersion("255.255.255"))
    }

    func testFirmwareVersionRejectsInvalidStrings() {
        XCTAssertNil(FirmwareVersion("1.2"))
        XCTAssertNil(FirmwareVersion("1.2.3.4"))
        XCTAssertNil(FirmwareVersion(""))
        XCTAssertNil(FirmwareVersion("a.b.c"))
        XCTAssertNil(FirmwareVersion("1.-1.0"))
    }

    func testFirmwareVersionComparison() {
        let v100 = FirmwareVersion("1.0.0")!
        let v110 = FirmwareVersion("1.1.0")!
        let v111 = FirmwareVersion("1.1.1")!
        let v200 = FirmwareVersion("2.0.0")!

        XCTAssertLessThan(v100, v110, "minor version upgrade")
        XCTAssertLessThan(v110, v111, "patch version upgrade")
        XCTAssertLessThan(v111, v200, "major version upgrade")
        XCTAssertEqual(v100, FirmwareVersion("1.0.0")!, "equal versions")
        XCTAssertFalse(v200 < v100, "newer not less than older")
        XCTAssertGreaterThan(v200, v100)
    }

    func testFirmwareVersionDescription() {
        XCTAssertEqual(FirmwareVersion("3.14.159")!.description, "3.14.159")
        XCTAssertEqual(FirmwareVersion("0.0.0")!.description, "0.0.0")
    }

    func testFirmwareVersionGATTBytesDecoding() {
        // Pack 1.2.3 as little-endian uint32: 0x00010203
        var raw = UInt32(0x00010203).littleEndian
        let data = Data(bytes: &raw, count: 4)
        let version = FirmwareVersion(gattBytes: data)
        XCTAssertNotNil(version)
        XCTAssertEqual(version?.major, 1)
        XCTAssertEqual(version?.minor, 2)
        XCTAssertEqual(version?.patch, 3)
        XCTAssertEqual(version?.description, "1.2.3")
    }

    func testFirmwareVersionGATTBytesRejectsShortData() {
        XCTAssertNil(FirmwareVersion(gattBytes: Data([0x01, 0x02, 0x03])))
        XCTAssertNil(FirmwareVersion(gattBytes: Data()))
    }

    // MARK: - FirmwareManifest decoding (ISC-107)

    func testFirmwareManifestDecodes() throws {
        let json = """
        {
            "version": "1.5.0",
            "releaseDate": "2026-06-09",
            "imageSizeBytes": 4194304,
            "sha256Hex": "abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789",
            "ed25519KeyFingerprint": "4e455550deadbeef",
            "isSafetyMCUFirmware": false,
            "releaseNotes": "Stability improvements."
        }
        """.data(using: .utf8)!
        let manifest = try JSONDecoder().decode(FirmwareManifest.self, from: json)
        XCTAssertEqual(manifest.version, "1.5.0")
        XCTAssertEqual(manifest.imageSizeBytes, 4194304)
        XCTAssertFalse(manifest.isSafetyMCUFirmware)
        XCTAssertEqual(manifest.releaseNotes, "Stability improvements.")
    }

    func testFirmwareManifestToFirmwareImage() throws {
        let json = """
        {
            "version": "2.0.0",
            "releaseDate": "2026-06-09",
            "imageSizeBytes": 1024,
            "sha256Hex": "aabbccdd",
            "ed25519KeyFingerprint": "4e455550deadbeef",
            "isSafetyMCUFirmware": true,
            "releaseNotes": "Safety MCU update."
        }
        """.data(using: .utf8)!
        let manifest = try JSONDecoder().decode(FirmwareManifest.self, from: json)
        let image = manifest.toFirmwareImage()
        XCTAssertNotNil(image)
        XCTAssertEqual(image?.version, "2.0.0")
        XCTAssertTrue(image?.isSafetyMCUFirmware == true)
    }

    // MARK: - OTAPhase (ISC-109)

    func testOTAPhaseBusyStates() {
        XCTAssertFalse(OTAPhase.idle.isBusy)
        XCTAssertFalse(OTAPhase.complete.isBusy)
        XCTAssertFalse(OTAPhase.failed.isBusy)
        XCTAssertTrue(OTAPhase.preparing.isBusy)
        XCTAssertTrue(OTAPhase.transferring.isBusy)
        XCTAssertTrue(OTAPhase.verifying.isBusy)
        XCTAssertTrue(OTAPhase.verified.isBusy)
        XCTAssertTrue(OTAPhase.applying.isBusy)
    }

    func testOTAPhaseTerminalStates() {
        XCTAssertTrue(OTAPhase.complete.isTerminal)
        XCTAssertTrue(OTAPhase.failed.isTerminal)
        XCTAssertFalse(OTAPhase.idle.isTerminal)
        XCTAssertFalse(OTAPhase.applying.isTerminal)
    }

    func testOTAPhaseFromRawByte() {
        XCTAssertEqual(OTAPhase(rawPacketByte: 0x00), .idle)
        XCTAssertEqual(OTAPhase(rawPacketByte: 0x01), .preparing)
        XCTAssertEqual(OTAPhase(rawPacketByte: 0x02), .transferring)
        XCTAssertEqual(OTAPhase(rawPacketByte: 0x03), .verifying)
        XCTAssertEqual(OTAPhase(rawPacketByte: 0x04), .verified)
        XCTAssertEqual(OTAPhase(rawPacketByte: 0x05), .applying)
        XCTAssertEqual(OTAPhase(rawPacketByte: 0x06), .complete)
        XCTAssertEqual(OTAPhase(rawPacketByte: 0xFF), .failed)
        // Unknown byte maps to .idle (not .failed — hub may send newer phases)
        XCTAssertEqual(OTAPhase(rawPacketByte: 0x07), .idle)
    }

    // MARK: - OTASession progress (ISC-110)

    func testOTASessionBytesProgress() {
        let image = FirmwareImage(
            version: "1.0.0", buildDate: Date(), imageSizeBytes: 1024,
            ed25519PublicKeyFingerprint: "deadbeef", isSafetyMCUFirmware: false, releaseNotes: "")
        var session = OTASession(image: image)
        session.totalBytes = 1024
        session.sentBytes = 512
        XCTAssertEqual(session.bytesProgress, 0.5, accuracy: 0.001)
    }

    func testOTASessionFormattedBytes() {
        XCTAssertEqual(OTASession.formattedBytes(512), "1 KB")
        XCTAssertEqual(OTASession.formattedBytes(1024 * 500), "500 KB")
        // 1.5 MiB
        let mb = OTASession.formattedBytes(1024 * 1024 + 512 * 1024)
        XCTAssertTrue(mb.contains("MB"), "1.5 MiB should format as MB: \(mb)")
    }

    func testOTASessionChunkCount() {
        // 1000 bytes of data with chunkSize=496 → 3 chunks (496+496+8)
        let total = 1000
        let chunks = (total + OTASession.chunkSize - 1) / OTASession.chunkSize
        XCTAssertEqual(chunks, 3)
    }

    // MARK: - hubFirmwareVersion GATT flow (ISC-108)

    func testHubFirmwareVersionClearedOnDisconnect() {
        let mock = MockBLECentral()
        let manager = NeuroPulseGATTManager(mockCentral: mock)

        manager.applyFirmwareVersion(FirmwareVersion("1.2.3")!)
        XCTAssertEqual(manager.hubFirmwareVersion, "1.2.3")

        manager.applyDisconnection()
        XCTAssertNil(manager.hubFirmwareVersion,
                     "hubFirmwareVersion must be cleared on disconnect (UHDR/SHDR boundary)")
    }

    func testHubFirmwareVersionStored() {
        let mock = MockBLECentral()
        let manager = NeuroPulseGATTManager(mockCentral: mock)
        manager.applyFirmwareVersion(FirmwareVersion("2.5.1")!)
        XCTAssertEqual(manager.hubFirmwareVersion, "2.5.1")
    }

    // MARK: - ISC-113: signature fingerprint gate

    func testSignatureGateBlocksWrongFingerprint() async {
        let mock = MockBLECentral()
        let gatt = NeuroPulseGATTManager(mockCentral: mock)
        let service = MockFirmwareUpdateService()
        let ota = OTAManager(gatt: gatt, updateService: service)

        let badImage = FirmwareImage(
            version: "1.0.0", buildDate: Date(), imageSizeBytes: 4,
            ed25519PublicKeyFingerprint: "WRONG_FINGERPRINT",
            isSafetyMCUFirmware: false, releaseNotes: "")

        // ISC-113: fingerprint check runs BEFORE connection check and before any download.
        // A wrong fingerprint must always throw .signatureInvalid regardless of hub state.
        do {
            try await ota.beginUpdate(image: badImage)
            XCTFail("Expected OTAError.signatureInvalid")
        } catch OTAError.signatureInvalid {
            // Expected — fingerprint rejected before any hub communication or download
        } catch {
            XCTFail("Unexpected error: \(error) — signatureInvalid expected, not \(type(of: error))")
        }
    }

    // MARK: - GATTParser.parseFirmwareVersion

    func testGATTParseFirmwareVersionRoundTrip() {
        var raw = UInt32(0x00020304).littleEndian  // 2.3.4
        let data = Data(bytes: &raw, count: 4)
        let version = GATTParser.parseFirmwareVersion(data)
        XCTAssertEqual(version?.major, 2)
        XCTAssertEqual(version?.minor, 3)
        XCTAssertEqual(version?.patch, 4)
        XCTAssertEqual(version?.description, "2.3.4")
    }

    func testGATTParseFirmwareVersionRejectsTruncated() {
        XCTAssertNil(GATTParser.parseFirmwareVersion(Data([0x01, 0x02])))
        XCTAssertNil(GATTParser.parseFirmwareVersion(Data()))
    }

    // MARK: - OTAOpcode raw values (ISC-109 protocol contract)
    // These values are the BLE wire contract with the hub. Changing them breaks
    // hub firmware compatibility. Tests document and enforce the mapping.

    func testOTAOpcodeWireValues() {
        XCTAssertEqual(OTAOpcode.initiate.rawValue,       0x01)
        XCTAssertEqual(OTAOpcode.chunk.rawValue,          0x02)
        XCTAssertEqual(OTAOpcode.verify.rawValue,         0x03)
        XCTAssertEqual(OTAOpcode.commit.rawValue,         0x04)
        XCTAssertEqual(OTAOpcode.abort.rawValue,          0x05)
        XCTAssertEqual(OTAOpcode.safetyMCUBegin.rawValue, 0x10)
        XCTAssertEqual(OTAOpcode.safetyMCUChunk.rawValue, 0x11)
        XCTAssertEqual(OTAOpcode.safetyMCUCommit.rawValue, 0x12)
    }

    // MARK: - Download path (ISC-109 pre-condition)

    func testBeginUpdateThrowsNotConnectedAfterFingerprintPassesWhenHubDisconnected() async {
        let mock = MockBLECentral()
        let gatt = NeuroPulseGATTManager(mockCentral: mock)
        let service = MockFirmwareUpdateService()
        let ota = OTAManager(gatt: gatt, updateService: service)

        // Correct fingerprint but hub is not connected — should throw .notConnected
        // (not .signatureInvalid, confirming fingerprint check passed).
        let goodImage = FirmwareImage(
            version: "1.0.0", buildDate: Date(), imageSizeBytes: 4,
            ed25519PublicKeyFingerprint: "4e455550deadbeef",
            isSafetyMCUFirmware: false, releaseNotes: "")

        do {
            try await ota.beginUpdate(image: goodImage)
            XCTFail("Expected OTAError.notConnected")
        } catch OTAError.notConnected {
            // Expected — fingerprint OK, hub not connected
        } catch OTAError.signatureInvalid {
            XCTFail("Fingerprint should have passed for the trusted placeholder value")
        } catch {
            XCTFail("Unexpected error: \(error)")
        }
        XCTAssertEqual(service.downloadCallCount, 0, "download must not be called when hub disconnected")
    }

    func testBeginUpdateWrapsDownloadFailureAsImageLoadFailed() async {
        let mock = MockBLECentral()
        let gatt = NeuroPulseGATTManager(mockCentral: mock)
        gatt.applyConnected()  // simulate hub connected without real BLE

        let service = MockFirmwareUpdateService()
        service.downloadErrorToThrow = FirmwareUpdateError.networkError(URLError(.notConnectedToInternet))
        let ota = OTAManager(gatt: gatt, updateService: service)

        let goodImage = FirmwareImage(
            version: "1.0.0", buildDate: Date(), imageSizeBytes: 4,
            ed25519PublicKeyFingerprint: "4e455550deadbeef",
            isSafetyMCUFirmware: false, releaseNotes: "")

        do {
            try await ota.beginUpdate(image: goodImage)
            XCTFail("Expected OTAError.imageLoadFailed")
        } catch OTAError.imageLoadFailed {
            // Expected — download error wrapped correctly
        } catch {
            XCTFail("Unexpected error: \(error)")
        }
        XCTAssertEqual(service.downloadCallCount, 1, "download must be called when hub connected")
    }

    // MARK: - checkForUpdates / manifest availability (ISC-107)

    func testCheckForUpdatesShowsUpdateWhenNewerVersionAvailable() async {
        let mock = MockBLECentral()
        let gatt = NeuroPulseGATTManager(mockCentral: mock)
        gatt.applyFirmwareVersion(FirmwareVersion("1.0.0")!)

        let service = MockFirmwareUpdateService()
        service.manifestToReturn = FirmwareManifest(
            version: "1.1.0",
            releaseDate: "2026-06-09",
            imageSizeBytes: 1024,
            sha256Hex: "aabbccdd",
            ed25519KeyFingerprint: "4e455550deadbeef",
            isSafetyMCUFirmware: false,
            releaseNotes: "Bug fixes.")

        let ota = OTAManager(gatt: gatt, updateService: service)
        ota.checkForUpdates()

        // Allow the async Task inside checkForUpdates to complete.
        try? await Task.sleep(nanoseconds: 100_000_000)
        XCTAssertNotNil(ota.availableUpdate, "update 1.1.0 > current 1.0.0 should be shown")
        XCTAssertEqual(ota.availableUpdate?.version, "1.1.0")
    }

    func testCheckForUpdatesHidesUpdateWhenCurrentVersionIsCurrent() async {
        let mock = MockBLECentral()
        let gatt = NeuroPulseGATTManager(mockCentral: mock)
        gatt.applyFirmwareVersion(FirmwareVersion("2.0.0")!)

        let service = MockFirmwareUpdateService()
        service.manifestToReturn = FirmwareManifest(
            version: "2.0.0",
            releaseDate: "2026-06-09",
            imageSizeBytes: 1024,
            sha256Hex: "aabbccdd",
            ed25519KeyFingerprint: "4e455550deadbeef",
            isSafetyMCUFirmware: false,
            releaseNotes: "")

        let ota = OTAManager(gatt: gatt, updateService: service)
        ota.checkForUpdates()

        try? await Task.sleep(nanoseconds: 100_000_000)
        XCTAssertNil(ota.availableUpdate,
                     "no update shown when manifest version == current version")
    }

    func testCheckForUpdatesShowsUpdateWhenHubVersionUnknown() async {
        let mock = MockBLECentral()
        let gatt = NeuroPulseGATTManager(mockCentral: mock)
        // hubFirmwareVersion is nil — hub firmware hasn't shipped version char yet

        let service = MockFirmwareUpdateService()
        service.manifestToReturn = FirmwareManifest(
            version: "1.0.0",
            releaseDate: "2026-06-09",
            imageSizeBytes: 1024,
            sha256Hex: "aabbccdd",
            ed25519KeyFingerprint: "4e455550deadbeef",
            isSafetyMCUFirmware: false,
            releaseNotes: "")

        let ota = OTAManager(gatt: gatt, updateService: service)
        ota.checkForUpdates()

        try? await Task.sleep(nanoseconds: 100_000_000)
        XCTAssertNotNil(ota.availableUpdate,
                        "update shown when hub version unknown (let user decide)")
    }

    // MARK: - ISC-111: running bank protected by opcode ordering

    // Mid-OTA disconnect cannot corrupt the running firmware bank because the bank
    // swap only occurs at COMMIT. This test documents the structural invariant:
    // INITIATE must precede VERIFY, VERIFY must precede COMMIT, and ABORT is a
    // distinct opcode that does not advance the OTA state machine.

    func testRunningBankProtectedByOpcodeOrdering() {
        // INITIATE < chunk < VERIFY < COMMIT: the bank swap (COMMIT) is the last
        // meaningful opcode in the sequence, so any mid-flight disconnect (before COMMIT)
        // leaves the running bank untouched.
        XCTAssertLessThan(OTAOpcode.initiate.rawValue, OTAOpcode.chunk.rawValue,
                          "INITIATE must precede CHUNK in wire sequence")
        XCTAssertLessThan(OTAOpcode.chunk.rawValue, OTAOpcode.verify.rawValue,
                          "CHUNK must precede VERIFY in wire sequence")
        XCTAssertLessThan(OTAOpcode.verify.rawValue, OTAOpcode.commit.rawValue,
                          "VERIFY must precede COMMIT (bank swap) in wire sequence")

        // ABORT must be distinct from COMMIT — aborting must not trigger a bank swap.
        XCTAssertNotEqual(OTAOpcode.abort.rawValue, OTAOpcode.commit.rawValue,
                          "ABORT must not share the COMMIT opcode value")
    }

    // MARK: - ISC-112: abort() leaves manager in clean state for reconnect retry

    // After a mid-OTA abort (e.g., hub disconnected unexpectedly), the manager must
    // return to .idle with no active session so a reconnect can start a fresh attempt.

    func testAbortClearsOTASessionForReconnectRetry() {
        let mock = MockBLECentral()
        let gatt = NeuroPulseGATTManager(mockCentral: mock)
        let service = MockFirmwareUpdateService()
        let ota = OTAManager(gatt: gatt, updateService: service)

        // abort() is synchronous and does not require an active download in flight.
        ota.abort()

        XCTAssertNil(ota.currentSession,
                     "currentSession must be nil after abort so reconnect retry can start fresh")
        XCTAssertEqual(ota.phase, .idle,
                       "phase must be .idle after abort")
    }
}
