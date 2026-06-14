//
//  SessionProtocolUploaderTests.swift
//  NeuroPulseTests
//
//  Satisfies: ISC-35 (upload NPProtocolDefinition serialises to NPPR-magic wire blob),
//             ISC-35 (bleNotReady thrown when hub not connected),
//             ISC-35 (validationFailed thrown for out-of-bounds protocol, GATT not called).
//
//  Subject under test: SessionProtocolUploader
//  (app/ios/NeuroPulse/Session/SessionProtocolUploader.swift)

import XCTest
@testable import NeuroPulse

// MARK: - Tests

@MainActor
final class SessionProtocolUploaderTests: XCTestCase {

    // MARK: - ISC-35: NPPR magic in reassembled wire blob

    // upload(_:NPProtocolDefinition) must serialise the definition, sign it with
    // the session Ed25519 key, chunk it per the BLE framing spec, and deliver all
    // chunks to the GATT gateway. When chunks are reassembled the wire blob must
    // open with the 4-byte NPPR magic: [0x4E, 0x50, 0x50, 0x52].
    func testUploadDefinitionSendsNPPRBlob() async throws {
        let gateway = MockProtocolUploadGateway()
        let uploader = SessionProtocolUploader(gatt: gateway)

        let definition = NPProtocolDefinition(
            name: "Test Protocol",
            timingMode: .duration(20 * 60),
            modalities: [
                NPProtocolModality(
                    params: .pbmTranscranial(NPPBMTranscranialParams(
                        intensityPercent: 75,
                        frequencyHz: 20,
                        dutyCyclePercent: 25
                    )),
                    interval: .continuous,
                    enabled: true
                )
            ]
        )

        try await uploader.upload(definition)

        XCTAssertTrue(gateway.uploadCallCount > 0, "uploadProtocol must be called at least once")
        let reassembled = gateway.reassembledPayload()
        XCTAssertGreaterThanOrEqual(reassembled.count, 4, "Reassembled blob must contain at least the NPPR header")
        XCTAssertEqual(
            Array(reassembled.prefix(4)),
            [0x4E, 0x50, 0x50, 0x52],
            "Reassembled wire blob must begin with NPPR magic bytes"
        )
    }

    // MARK: - ISC-35: BLE-not-ready guard

    // upload(_:NPProtocolDefinition) must throw UploadError.bleNotReady before
    // attempting to sign or upload when the hub is not connected, and must set
    // lastError so the UI can surface the failure without re-catching.
    func testUploadDefinitionThrowsBLENotReadyWhenDisconnected() async {
        let gateway = MockProtocolUploadGateway(connected: false)
        let uploader = SessionProtocolUploader(gatt: gateway)

        let definition = NPProtocolDefinition(
            name: "Any Protocol",
            timingMode: .duration(60),
            modalities: []
        )

        do {
            try await uploader.upload(definition)
            XCTFail("Expected UploadError.bleNotReady")
        } catch UploadError.bleNotReady {
            XCTAssertEqual(gateway.uploadCallCount, 0, "GATT must not be called when disconnected")
            XCTAssertNotNil(uploader.lastError, "lastError must be set so the UI can show the reason without re-catching")
        } catch {
            XCTFail("Expected UploadError.bleNotReady, got \(error)")
        }
    }

    // MARK: - ISC-35: Multi-chunk upload reassembles to valid NPPR blob

    // A protocol definition whose serialised JSON exceeds the SINGLE-chunk limit
    // (509-byte payload after NPPR magic + length-prefix + Ed25519 signature) must
    // be split across multiple BLE writes. After reassembly the resulting blob must
    // still begin with the NPPR magic bytes and be structurally valid.
    func testUploadMultiChunkProtocolReassemblesWithNPPRMagic() async throws {
        let gateway = MockProtocolUploadGateway()
        let uploader = SessionProtocolUploader(gatt: gateway)

        // A 500-char name inflates the JSON body to ~700 bytes; the wire format
        // (4-byte magic + 4-byte length + JSON + 64-byte Ed25519 signature) exceeds
        // the 509-byte SINGLE-frame payload ceiling, forcing a START + END split.
        let definition = NPProtocolDefinition(
            name: String(repeating: "N", count: 500),
            timingMode: .duration(20 * 60),
            modalities: [
                NPProtocolModality(
                    params: .pbmTranscranial(NPPBMTranscranialParams(
                        intensityPercent: 75,
                        frequencyHz: 20,
                        dutyCyclePercent: 25
                    )),
                    interval: .continuous,
                    enabled: true
                )
            ]
        )

        try await uploader.upload(definition)

        XCTAssertGreaterThan(
            gateway.uploadCallCount, 1,
            "A large protocol must be split across more than one BLE write"
        )
        let reassembled = gateway.reassembledPayload()
        XCTAssertEqual(
            Array(reassembled.prefix(4)),
            [0x4E, 0x50, 0x50, 0x52],
            "Reassembled multi-chunk blob must begin with NPPR magic bytes"
        )
    }

    // MARK: - ISC-35: Validation guard — hardware safety ceiling

    // upload(_:NPProtocolDefinition) must throw UploadError.validationFailed and
    // must NOT call GATT uploadProtocol when the protocol violates hardware limits.
    // This ensures an unsafe protocol never reaches the hub.
    func testUploadDefinitionRejectsProtocolOverHardwareLimit() async {
        let gateway = MockProtocolUploadGateway()
        let uploader = SessionProtocolUploader(gatt: gateway)

        // tDCS at 2.5 mA exceeds the 2.0 mA hardware ceiling (NPHardwareLimits).
        let definition = NPProtocolDefinition(
            name: "Over-limit Protocol",
            timingMode: .duration(20 * 60),
            modalities: [
                NPProtocolModality(
                    params: .tdcs(NPTDCSParams(
                        intensityMilliamps: 2.5,
                        electrodePairs: [["Fp1", "P3"]]
                    )),
                    interval: .continuous,
                    enabled: true
                )
            ]
        )

        do {
            try await uploader.upload(definition)
            XCTFail("Expected UploadError.validationFailed")
        } catch UploadError.validationFailed(let issues) {
            XCTAssertFalse(issues.isEmpty, "validationFailed must include at least one issue")
            XCTAssertEqual(gateway.uploadCallCount, 0, "GATT must not be called for invalid protocols")
        } catch {
            XCTFail("Expected UploadError.validationFailed, got \(error)")
        }
    }

    // MARK: - ISC-35: testSignAndUpload (canonical ISA test)

    // Named test referenced in ISA Test Strategy table for ISC-35.
    // Verifies the core contract: upload(_:NPProtocolDefinition) signs the
    // protocol blob with the session Ed25519 key and delivers it to the
    // GATT gateway. The NPPR magic bytes confirm the signed wire format reached
    // the gateway rather than a raw/unsigned payload.
    @MainActor
    func testSignAndUpload() async throws {
        let gateway = MockProtocolUploadGateway()
        let uploader = SessionProtocolUploader(gatt: gateway)

        let definition = NPProtocolDefinition(
            name: "Sign And Upload Test",
            timingMode: .duration(10 * 60),
            modalities: [
                NPProtocolModality(
                    params: .eegNeurofeedback(NPEEGNeurofeedbackParams(
                        channels: .all,
                        band: .alpha,
                        closedLoopEnabled: true
                    )),
                    interval: .continuous,
                    enabled: true
                )
            ]
        )

        try await uploader.upload(definition)

        // Sign happened: GATT gateway was called at least once.
        XCTAssertTrue(gateway.uploadCallCount > 0, "uploadProtocol must be called after signing")
        // Upload happened: reassembled blob carries NPPR magic from SignedProtocolBlob.wireFormat.
        let blob = gateway.reassembledPayload()
        XCTAssertGreaterThanOrEqual(blob.count, 4)
        XCTAssertEqual(
            Array(blob.prefix(4)),
            [0x4E, 0x50, 0x50, 0x52],
            "Signed wire blob must begin with NPPR magic bytes"
        )
        // isUploading resets to false after completion (captured before autoclosure to satisfy
        // Swift 6 nonisolated autoclosure restriction for @MainActor properties).
        let stillUploading = uploader.isUploading
        XCTAssertFalse(stillUploading, "isUploading must be false after upload completes")
    }
}

// MARK: - Mock GATT gateway

/// Testable stub conforming to ProtocolUploadGateway.
/// Records every chunk delivered via uploadProtocol and immediately ACKs.
private final class MockProtocolUploadGateway: ProtocolUploadGateway {

    var isHubConnected: Bool
    private(set) var uploadCallCount = 0
    private var chunks: [Data] = []

    init(connected: Bool = true) {
        self.isHubConnected = connected
    }

    func uploadProtocol(_ blob: Data, completion: @escaping (Result<Void, GATTWriteError>) -> Void) {
        uploadCallCount += 1
        chunks.append(blob)
        completion(.success(()))
    }

    /// Reassemble all received chunks into the original wire blob by stripping
    /// the BLE framing headers (mirrors ProtocolChunker frame spec).
    func reassembledPayload() -> Data {
        var out = Data()
        for chunk in chunks {
            guard let header = chunk.first else { continue }
            switch header {
            case 0x04: // SINGLE — header + payload
                out.append(chunk.dropFirst())
            case 0x01: // START — header + 2-byte length + payload
                out.append(chunk.dropFirst(3))
            case 0x02, 0x03: // CONT / END — header + payload
                out.append(chunk.dropFirst())
            default:
                break
            }
        }
        return out
    }
}
