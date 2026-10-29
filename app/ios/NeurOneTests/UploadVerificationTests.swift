//
//  UploadVerificationTests.swift
//  NeurOneTests
//
//  UPLOAD_CHECK as the app decodes it (SWR-UP-01) and the uploader's wait for it (SWR-UP-03). The same cases run on the
//  Kotlin app, so the two cannot drift apart.
//
//  Subject under test: UploadCheck, UploadVerdict, SessionProtocolUploader.verify
//  (app/ios/NeurOne/Models/UploadCheck.swift, app/ios/NeurOne/Session/SessionProtocolUploader.swift)

import XCTest
import CoreBluetooth
@testable import NeurOne

/// A hub whose reports are scripted: each request for the upload check delivers the next one.
private final class ScriptedUploadHub: ProtocolUploadGateway {
    var isHubConnected = true
    var uploadCheck: UploadCheck?
    private let reports: [UploadCheck?]
    private(set) var reads = 0
    private(set) var writes = 0

    init(_ reports: [UploadCheck?]) { self.reports = reports }

    func uploadProtocol(_ chunk: Data, completion: @escaping (Result<Void, GATTWriteError>) -> Void) {
        writes += 1
        completion(.success(()))
    }

    func requestUploadCheck() {
        let report = reports[min(reads, reports.count - 1)]
        reads += 1
        if let report { uploadCheck = report }
    }
}

final class UploadVerificationTests: XCTestCase {

    private func frame(status: UInt8, reason: UInt8, hash: [UInt8]) -> Data { Data([status, reason] + hash) }

    private let sent = UploadCheck.hash(of: Data("the exact bytes sent".utf8))
    private let other = UploadCheck.hash(of: Data("something else".utf8))

    private func report(_ status: UploadCheckStatus, reason: Int = 0, hash: String) -> UploadCheck {
        UploadCheck(status: status, reason: reason, receivedHash: hash)
    }

    // @verifies SWR-UP-01
    func testDecodesStatusReasonAndHash() throws {
        let hash = (0..<32).map { UInt8($0) }
        let c = try XCTUnwrap(UploadCheck(wire: frame(status: 1, reason: 0, hash: hash)))
        XCTAssertEqual(c.status, .accepted)
        XCTAssertEqual(c.reason, 0)
        XCTAssertEqual(c.receivedHash, "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f")
    }

    // @verifies SWR-UP-01
    func testTheHashOfTheSentBytesIsTheSha256InTheFormTheHubReports() {
        // FIPS 180-4: SHA-256("abc"), the vector the hub's own test uses.
        XCTAssertEqual(UploadCheck.hash(of: Data("abc".utf8)),
                       "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad")
    }

    // @verifies SWR-UP-01
    func testAMalformedReportIsDroppedWhole() {
        XCTAssertNil(UploadCheck(wire: Data(count: 33)))
        XCTAssertNil(UploadCheck(wire: Data(count: 35)))
        XCTAssertNil(UploadCheck(wire: frame(status: 9, reason: 0, hash: [UInt8](repeating: 0, count: 32))), "an unknown status")
    }

    // @verifies SWR-UP-02
    func testATierOrHardwareVersionRefusalIsNamedAndOtherRefusalsAreNot() {
        XCTAssertEqual(report(.refused, reason: 19, hash: sent).refusalKind, "tier")
        XCTAssertEqual(report(.refused, reason: 20, hash: sent).refusalKind, "tier")
        XCTAssertEqual(report(.refused, reason: 27, hash: sent).refusalKind, "hardware_version", "SWR-HV-08 mismatch")
        XCTAssertEqual(report(.refused, reason: 28, hash: sent).refusalKind, "hardware_version", "SWR-HV-08 not yet verified")
        XCTAssertNil(report(.accepted, reason: 27, hash: sent).refusalKind, "only a refusal has a refusal kind")
        XCTAssertNil(report(.refused, reason: 6, hash: sent).refusalKind, "a bad signature is a failure, not a tier")
        XCTAssertNil(report(.accepted, reason: 19, hash: sent).refusalKind, "only a refusal has a refusal kind")
    }

    // @verifies SWR-UP-03
    func testNoReportAnEmptyOneOrAPendingOneIsNoAnswer() {
        let none = UploadVerdict(helmetStatus: nil, helmetHash: nil, helmetRefusal: nil)
        XCTAssertEqual(UploadVerdict.of(nil), none)
        XCTAssertEqual(UploadVerdict.of(report(.none, hash: sent)), none)
        XCTAssertEqual(UploadVerdict.of(report(.pending, hash: sent)), none)
        XCTAssertEqual(UploadVerdict.of(report(.accepted, hash: sent)).helmetStatus, "accepted")
    }

    // @verifies SWR-HV-08
    func testAHardwareVersionRefusalKeepsItsReasonSoAScreenCanSayWhetherToTryAgain() {
        let mismatch = UploadVerdict.of(report(.refused, reason: 27, hash: sent))
        XCTAssertEqual(mismatch.helmetRefusal, "hardware_version")
        XCTAssertEqual(mismatch.helmetReason, 27)
        let notReady = UploadVerdict.of(report(.refused, reason: 28, hash: sent))
        XCTAssertEqual(notReady.helmetRefusal, "hardware_version")
        XCTAssertEqual(notReady.helmetReason, UploadCheck.reasonHardwareVersionUnverified)
        // Only a refusal has a reason; an accepted upload and no answer have none.
        XCTAssertNil(UploadVerdict.of(report(.accepted, hash: sent)).helmetReason)
        XCTAssertNil(UploadVerdict.of(nil).helmetReason)
    }

    // @verifies SWR-UP-01
    @MainActor
    func testAReportWithTheSentHashAndAcceptedIsPassedOn() async {
        let hub = ScriptedUploadHub([report(.accepted, hash: sent)])
        let verdict = await SessionProtocolUploader(gatt: hub).verify(sentHash: sent, timeoutNanos: 50_000_000, pollNanos: 5_000_000)
        XCTAssertEqual(verdict.helmetStatus, "accepted")
        XCTAssertEqual(verdict.helmetHash, sent)
    }

    // @verifies SWR-UP-03
    @MainActor
    func testAPendingReportIsReadAgainAndTheProtocolIsNeverSentAgain() async {
        let hub = ScriptedUploadHub([report(.pending, hash: sent), report(.pending, hash: sent), report(.accepted, hash: sent)])
        let verdict = await SessionProtocolUploader(gatt: hub).verify(sentHash: sent, timeoutNanos: 500_000_000, pollNanos: 5_000_000)
        XCTAssertEqual(verdict.helmetStatus, "accepted")
        XCTAssertEqual(hub.reads, 3, "read until the verdict")
        XCTAssertEqual(hub.writes, 0, "an unknown outcome is resolved by reading, never by a second write")
    }

    // @verifies SWR-UP-01
    @MainActor
    func testATruncatedUploadLeavesThePreviousReportAndItsHashDoesNotMatch() async {
        let hub = ScriptedUploadHub([report(.accepted, hash: other)])
        let verdict = await SessionProtocolUploader(gatt: hub).verify(sentHash: sent, timeoutNanos: 20_000_000, pollNanos: 5_000_000)
        XCTAssertEqual(verdict.helmetHash, other, "the app passes the hub's hash on; the library finds it differs from the sent one")
        XCTAssertEqual(hub.writes, 0)
    }

    // @verifies SWR-UP-03
    @MainActor
    func testAHubThatNeverAnswersIsUnknownNotResent() async {
        let hub = ScriptedUploadHub([nil])
        let verdict = await SessionProtocolUploader(gatt: hub).verify(sentHash: sent, timeoutNanos: 20_000_000, pollNanos: 5_000_000)
        XCTAssertNil(verdict.helmetStatus)
        XCTAssertNil(verdict.helmetHash)
        XCTAssertEqual(hub.writes, 0)
    }

    // @verifies SWR-UP-02
    @MainActor
    func testARefusalIsPassedOnWithItsKind() async {
        let hub = ScriptedUploadHub([report(.refused, reason: 20, hash: sent)])
        let verdict = await SessionProtocolUploader(gatt: hub).verify(sentHash: sent, timeoutNanos: 50_000_000, pollNanos: 5_000_000)
        XCTAssertEqual(verdict.helmetStatus, "refused")
        XCTAssertEqual(verdict.helmetRefusal, "tier")
    }

    // @verifies SWR-UP-01
    @MainActor
    func testTheManagerShowsTheReportAndAMalformedFrameOrADroppedLinkClearsIt() {
        let manager = NeurOneGATTManager(mockCentral: InertUploadCentral())
        manager.applyUploadCheck(frame(status: 1, reason: 0, hash: [UInt8](repeating: 7, count: 32)))
        XCTAssertEqual(manager.uploadCheck?.status, .accepted)
        manager.applyUploadCheck(Data(count: 5))
        XCTAssertNil(manager.uploadCheck)
        manager.applyUploadCheck(frame(status: 1, reason: 0, hash: [UInt8](repeating: 7, count: 32)))
        manager.applyDisconnection()
        XCTAssertNil(manager.uploadCheck)
    }
}

/// A central that does nothing: the manager is driven directly through `applyUploadCheck`.
private final class InertUploadCentral: BLECentralManager {
    var state: CBManagerState = .poweredOn
    func scanForPeripherals(withServices: [CBUUID]?, options: [String: Any]?) {}
    func stopScan() {}
    func connect(_ peripheral: CBPeripheral, options: [String: Any]?) {}
    func cancelPeripheralConnection(_ peripheral: CBPeripheral) {}
}
