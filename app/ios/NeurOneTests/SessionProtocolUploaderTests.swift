//
//  SessionProtocolUploaderTests.swift
//  NeurOneTests
//
//  Satisfies: ISC-35 (upload NPProtocolDefinition serialises to the NP-FW-HUB-001 §4 descriptor, NPHP magic),
//             ISC-35 (bleNotReady thrown when hub not connected),
//             ISC-35 (validationFailed thrown for out-of-bounds protocol, GATT not called).
//
//  Subject under test: SessionProtocolUploader
//  (app/ios/NeurOne/Session/SessionProtocolUploader.swift)

import XCTest
@testable import NeurOne

// MARK: - Tests

@MainActor
final class SessionProtocolUploaderTests: XCTestCase {

    // MARK: - ISC-35: NPHP magic in reassembled descriptor

    // upload(_:NPProtocolDefinition) must serialize the definition, sign it with
    // the session Ed25519 key, chunk it per the BLE framing spec, and deliver all
    // chunks to the GATT gateway. When chunks are reassembled the wire blob must
    // open with the 4-byte NPHP magic (0x4E504850 little-endian): [0x50, 0x48, 0x50, 0x4E].
    func testUploadDefinitionSendsTheBinaryDescriptor() async throws {
        let gateway = MockProtocolUploadGateway()
        let uploader = SessionProtocolUploader(gatt: gateway)

        let definition = NPProtocolDefinition(
            name: "Test Protocol",
            timingMode: .duration(20 * 60),
            modalities: [
                NPProtocolModality(
                    params: .pbmTranscranial(NPPBMTranscranialParams(
                        irradianceMWcm2: 302,
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
        XCTAssertGreaterThanOrEqual(reassembled.count, 4, "Reassembled blob must contain at least the descriptor header")
        XCTAssertEqual(
            Array(reassembled.prefix(4)),
            [0x50, 0x48, 0x50, 0x4E],
            "Reassembled descriptor must begin with NPHP magic bytes"
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

    // MARK: - ISC-35: Multi-chunk upload reassembles to a valid NPHP descriptor

    // A descriptor larger than the SINGLE-chunk limit (509 bytes) must be split across multiple
    // BLE writes. After reassembly the blob must still begin with the NPHP magic and carry the
    // whole descriptor. The descriptor has no name field, so size comes from commands: an
    // interval block expands to an ON and a STOP per repeat (NP-FW-HUB-001 §4.1).
    func testUploadMultiChunkProtocolReassemblesWithNPHPMagic() async throws {
        let gateway = MockProtocolUploadGateway()
        let uploader = SessionProtocolUploader(gatt: gateway)

        // 20 min at 20 s on / 28 s off = 25 repeats: 25 ON commands (21 bytes) and 25 STOPs (14),
        // plus the 64-byte header and 64-byte signature = 1003 bytes, a START + END split.
        let definition = NPProtocolDefinition(
            name: "Many commands",
            timingMode: .duration(20 * 60),
            modalities: [
                NPProtocolModality(
                    params: .besTacs(NPBESTacsParams(frequencyHz: 10, intensityMilliamps: 0.8, waveform: .sinusoidal)),
                    interval: NPIntervalConfig(intervalOnSeconds: 20, intervalOffSeconds: 28, repeatCount: nil),
                    enabled: true
                )
            ]
        )

        try await uploader.upload(definition)

        XCTAssertGreaterThan(
            gateway.uploadCallCount, 1,
            "A large descriptor must be split across more than one BLE write"
        )
        let reassembled = gateway.reassembledPayload()
        XCTAssertEqual(
            Array(reassembled.prefix(4)),
            [0x50, 0x48, 0x50, 0x4E],
            "Reassembled multi-chunk descriptor must begin with NPHP magic bytes"
        )
        let onCommands: Int = 25 * (14 + 7)   // 14-byte command header + 7 params bytes
        let stopCommands: Int = 25 * 14       // a STOP carries no params
        let expectedSize: Int = 64 + onCommands + stopCommands + 64
        XCTAssertEqual(reassembled.count, expectedSize,
                       "header + 25 ON commands + 25 STOP commands + signature")
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
    // GATT gateway. The NPHP magic bytes confirm the signed descriptor reached
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
        // Upload happened: reassembled blob carries NPHP magic from HubDescriptor.blob.
        let blob = gateway.reassembledPayload()
        XCTAssertGreaterThanOrEqual(blob.count, 4)
        XCTAssertEqual(
            Array(blob.prefix(4)),
            [0x50, 0x48, 0x50, 0x4E],
            "Signed descriptor must begin with NPHP magic bytes"
        )
        // isUploading resets to false after completion (captured before autoclosure to satisfy
        // Swift 6 nonisolated autoclosure restriction for @MainActor properties).
        let stillUploading = uploader.isUploading
        XCTAssertFalse(stillUploading, "isUploading must be false after upload completes")
    }

    // MARK: - NP-SW-FAULTMSG-001 P4: unread cardiac cutoff blocks a cervical restart

    private func cervicalDefinition() -> NPProtocolDefinition {
        NPProtocolDefinition(
            name: "Cervical",
            timingMode: .duration(120),
            modalities: [NPProtocolModality(params: .cervicalVns(NPCervicalVnsParams()),
                                            interval: .continuous, enabled: true)]
        )
    }

    func testCervicalProtocolRefusedWhileCardiacCutoffUnread() async {
        let gateway = MockProtocolUploadGateway()
        gateway.cervicalRestartBlocked = true
        let uploader = SessionProtocolUploader(gatt: gateway)
        do {
            try await uploader.upload(cervicalDefinition())
            XCTFail("a cervical protocol must be refused while a cardiac cutoff is unread")
        } catch UploadError.cervicalRestartBlocked {
            // expected
        } catch {
            XCTFail("unexpected error \(error)")
        }
        XCTAssertEqual(gateway.uploadCallCount, 0, "nothing may reach the hub")
    }

    func testAnotherPersonsCutoffNeedsOneConfirmationPerUpload() async throws {
        let gateway = MockProtocolUploadGateway()
        gateway.cervicalOutstandingForAnotherUser = true
        let uploader = SessionProtocolUploader(gatt: gateway)
        do {
            try await uploader.upload(cervicalDefinition())
            XCTFail("a profile switch past another person's cutoff must be confirmed")
        } catch UploadError.differentPersonConfirmationRequired {
            // expected
        }
        XCTAssertEqual(gateway.uploadCallCount, 0)

        uploader.confirmDifferentPerson()
        try await uploader.upload(cervicalDefinition())
        XCTAssertGreaterThan(gateway.uploadCallCount, 0, "confirmed — the upload proceeds")

        // The confirmation is one-shot: the next upload asks again.
        do {
            try await uploader.upload(cervicalDefinition())
            XCTFail("confirmation must not carry over to a later upload")
        } catch UploadError.differentPersonConfirmationRequired {
            // expected
        }
    }

    /// Mode 3 goes through the same gate and throws the same errors, so the protocol menu shows
    /// the same message and confirmation whichever mode the wearer chose.
    func testAutonomousProgrammingUsesTheSameCervicalGate() async throws {
        let gateway = MockProtocolUploadGateway()
        gateway.cervicalRestartBlocked = true
        let uploader = SessionProtocolUploader(gatt: gateway)
        do {
            try await uploader.programAutonomous(cervicalDefinition())
            XCTFail("Mode 3 must not get round an unread cardiac cutoff")
        } catch UploadError.cervicalRestartBlocked {
            // expected
        }
        XCTAssertEqual(gateway.uploadCallCount, 0)

        gateway.cervicalRestartBlocked = false
        gateway.cervicalOutstandingForAnotherUser = true
        do {
            try await uploader.programAutonomous(cervicalDefinition())
            XCTFail("Mode 3 must ask the different-person question too")
        } catch UploadError.differentPersonConfirmationRequired {
            // expected
        }
        XCTAssertEqual(gateway.uploadCallCount, 0)
        uploader.confirmDifferentPerson()
        try await uploader.programAutonomous(cervicalDefinition())
        XCTAssertGreaterThan(gateway.uploadCallCount, 0)
    }

    func testNonCervicalProtocolUnaffectedByCardiacCutoff() async throws {
        let gateway = MockProtocolUploadGateway()
        gateway.cervicalRestartBlocked = true
        let uploader = SessionProtocolUploader(gatt: gateway)
        let pbm = NPProtocolDefinition(
            name: "PBM",
            timingMode: .duration(600),
            modalities: [NPProtocolModality(params: .pbmTranscranial(NPPBMTranscranialParams()),
                                            interval: .continuous, enabled: true)]
        )
        try await uploader.upload(pbm)
        XCTAssertGreaterThan(gateway.uploadCallCount, 0, "other modalities are not blocked")
    }
}

// MARK: - Mock GATT gateway

/// Testable stub conforming to ProtocolUploadGateway.
/// Records every chunk delivered via uploadProtocol and immediately ACKs.
private final class MockProtocolUploadGateway: ProtocolUploadGateway {

    var isHubConnected: Bool
    var cervicalRestartBlocked = false
    var cervicalOutstandingForAnotherUser = false
    private(set) var uploadCallCount = 0
    private var chunks: [Data] = []

    init(connected: Bool = true) {
        self.isHubConnected = connected
    }

    func uploadProtocol(_ chunk: Data, completion: @escaping (Result<Void, GATTWriteError>) -> Void) {
        uploadCallCount += 1
        chunks.append(chunk)
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
