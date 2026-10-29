//
//  ProtocolUploadFlowTests.swift
//  NeurOneTests
//
//  What the protocol screen does once a protocol is sent: record how the upload ended, say why the hub refused it, and let
//  Mode 3 report "stored" only when the hub holds the protocol (SWR-UP-02, SWR-UP-03, SWR-HV-08, REQ-UPG-01). The screen
//  holds no logic of its own for this, so it is tested here without a view. The same cases run on the Kotlin app's screen
//  through the shared causes.
//
//  Subject under test: ProtocolUploadFlow (app/ios/NeurOne/Session/ProtocolUploadFlow.swift)

import XCTest
@testable import NeurOne

@MainActor
final class ProtocolUploadFlowTests: XCTestCase {

    private let stateKeys = [
        NPProtocolLibrary.usedFamiliesKey, NPProtocolLibrary.archivedFamiliesKey, NPProtocolLibrary.cloneSourcesKey,
    ]
    private let sent = UploadCheck.hash(of: Data("the exact bytes sent".utf8))
    private let other = UploadCheck.hash(of: Data("an earlier upload".utf8))

    override func setUp() async throws {
        stateKeys.forEach { ProtectedDefaults.standard.removeObject(forKey: $0) }
    }

    override func tearDown() async throws {
        stateKeys.forEach { ProtectedDefaults.standard.removeObject(forKey: $0) }
    }

    private func check(_ status: UploadCheckStatus, reason: Int = 0, hash: String) -> UploadCheck {
        UploadCheck(status: status, reason: reason, receivedHash: hash)
    }

    private struct Rig {
        let flow: ProtocolUploadFlow
        let library: NPProtocolLibrary
        let entry: NPProtocolEntry
    }

    /// A flow over a hub that answers with `report` every time it is asked.
    private func rig(_ report: UploadCheck?) throws -> Rig {
        let library = NPProtocolLibrary()
        let entry = try XCTUnwrap(library.bundledProtocols.first { $0.name == "Gamma Focus" })
        let uploader = SessionProtocolUploader(gatt: ScriptedUploadHub([report]))
        return Rig(flow: ProtocolUploadFlow(uploader: uploader, library: library), library: library, entry: entry)
    }

    /// `verify` waits up to ten seconds for an answer, so a hub that never answers is not run through `record` here.
    private func recorded(_ rig: Rig) async -> (cause: HelmetRefusalCause?, said: [String]) {
        var said: [String] = []
        let cause = await rig.flow.record(of: rig.entry, sentHash: sent) { said.append($0) }
        return (cause, said)
    }

    // @verifies SWR-HV-08
    func testEveryCauseHasItsOwnMessageThatIsNotTheKey() {
        let causes: [HelmetRefusalCause] = [
            .tier, .hardwareVersion, .notReady, .signature, .wrongDevice, .formatVersion, .damaged, .malformed,
            .sessionActive, .bootRefused, .other,
        ]
        let messages = causes.map { ProtocolUploadFlow.refusalMessage(for: $0) }
        XCTAssertEqual(Set(messages).count, causes.count, "two causes share one message")
        for message in messages {
            XCTAssertFalse(message.isEmpty)
            XCTAssertFalse(message.hasPrefix("PROTOCOL_MENU_"), "a message that is still its key is a missing string")
        }
    }

    // @verifies SWR-UP-02 SWR-HV-08
    func testARefusalForThisUploadIsSaidAndMarksNothingUsed() async throws {
        let rig = try rig(check(.refused, reason: 19, hash: sent))
        let result = await recorded(rig)
        XCTAssertEqual(result.cause, .tier)
        XCTAssertEqual(result.said, [ProtocolUploadFlow.refusalMessage(for: .tier)])
        XCTAssertFalse(rig.library.isUsed(rig.entry), "a tier refusal is not a use")
    }

    // @verifies SWR-UP-02
    func testARefusalReportForAnEarlierUploadIsNotThisOnesAndSaysNothing() async throws {
        let rig = try rig(check(.refused, reason: 6, hash: other))
        let result = await recorded(rig)
        XCTAssertNil(result.cause)
        XCTAssertTrue(result.said.isEmpty, "an old refusal must not be shown for the current attempt")
    }

    // @verifies SWR-UP-02
    func testAnAcceptedUploadSaysNothingAndMarksTheFamilyUsed() async throws {
        let rig = try rig(check(.accepted, hash: sent))
        let result = await recorded(rig)
        XCTAssertNil(result.cause)
        XCTAssertTrue(result.said.isEmpty)
        XCTAssertTrue(rig.library.isUsed(rig.entry))
    }

    // @verifies SWR-UP-02 SWR-HV-08
    func testModeThreeIsNotStoredAfterAnyRefusal() async throws {
        // One reason from every group, including the ones that are not about the tier or the hardware version.
        for reason in [19, 20, 27, 28, 6, 7, 5, 4, 2, 13, 25, 99] {
            let rig = try rig(check(.refused, reason: reason, hash: sent))
            var said: [String] = []
            let stored = try await rig.flow.program(entry: rig.entry, send: { self.sent }) { said.append($0) }
            XCTAssertFalse(stored, "reason \(reason): Mode 3 must not report the protocol stored")
            XCTAssertEqual(said.count, 1, "reason \(reason): the person is told why")
        }
    }

    // @verifies SWR-UP-02
    func testModeThreeIsStoredWhenTheHubAcceptsIt() async throws {
        let rig = try rig(check(.accepted, hash: sent))
        var said: [String] = []
        let stored = try await rig.flow.program(entry: rig.entry, send: { self.sent }) { said.append($0) }
        XCTAssertTrue(stored)
        XCTAssertTrue(said.isEmpty)
    }

    // @verifies SWR-UP-02
    func testModeThreeDoesNotWaitForTheHubWhenTheSendFails() async throws {
        let rig = try rig(check(.refused, reason: 19, hash: sent))
        struct Boom: Error {}
        do {
            _ = try await rig.flow.program(entry: rig.entry, send: { throw Boom() }) { _ in XCTFail("nothing was sent") }
            XCTFail("the send's error must reach the screen")
        } catch is Boom {
            XCTAssertFalse(rig.library.isUsed(rig.entry), "nothing was sent, so nothing was used")
        }
    }
}
