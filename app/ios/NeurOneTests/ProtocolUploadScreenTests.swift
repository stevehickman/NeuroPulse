//
//  ProtocolUploadScreenTests.swift
//  NeurOneTests
//
//  What the protocol menu shows while a protocol is sent and when it closes: the "sent" banner, the error banner, the
//  "different person" question, and whether a Mode 3 protocol is reported stored (SWR-UP-02, SWR-HV-08, REQ-UPG-01). The
//  view only draws this state, so it is tested on the model the view observes. How the view draws it is not tested here.
//
//  Subject under test: ProtocolUploadScreen (app/ios/NeurOne/Session/ProtocolUploadScreen.swift)

import XCTest
@testable import NeurOne

@MainActor
final class ProtocolUploadScreenTests: XCTestCase {

    private let stateKeys = [
        NPProtocolLibrary.usedFamiliesKey, NPProtocolLibrary.archivedFamiliesKey, NPProtocolLibrary.cloneSourcesKey,
    ]
    private let sent = UploadCheck.hash(of: Data("the exact bytes sent".utf8))

    /// Longer than one poll of the hub (`verify` reads every half second), so the report is in before the banner ends.
    private let banner: UInt64 = 1_200_000_000

    override func setUp() async throws {
        stateKeys.forEach { ProtectedDefaults.standard.removeObject(forKey: $0) }
    }

    override func tearDown() async throws {
        stateKeys.forEach { ProtectedDefaults.standard.removeObject(forKey: $0) }
    }

    @MainActor
    private final class Rig {
        let screen = ProtocolUploadScreen()
        let flow: ProtocolUploadFlow
        let entry: NPProtocolEntry
        let sent: String
        var closed = 0
        var programmed = 0

        init(flow: ProtocolUploadFlow, entry: NPProtocolEntry, sent: String) {
            self.flow = flow
            self.entry = entry
            self.sent = sent
        }

        /// One send, with the banner shown for `bannerNanos`. `send` defaults to writing the bytes `sent` hashes.
        func run(programMode: Bool, bannerNanos: UInt64, send: (() async throws -> String)? = nil) async {
            let sentHash = sent
            await screen.run(flow: flow, entry: entry, programMode: programMode,
                             send: send ?? { sentHash },
                             close: { self.closed += 1 }, programmed: { self.programmed += 1 },
                             bannerNanos: bannerNanos)
            await screen.recording?.value
        }
    }

    private func rig(_ report: UploadCheck?) throws -> Rig {
        let library = NPProtocolLibrary()
        let entry = try XCTUnwrap(library.bundledProtocols.first { $0.name == "Gamma Focus" })
        let uploader = SessionProtocolUploader(gatt: ScriptedUploadHub([report]))
        return Rig(flow: ProtocolUploadFlow(uploader: uploader, library: library), entry: entry, sent: sent)
    }

    private func report(_ status: UploadCheckStatus, reason: Int = 0) -> UploadCheck {
        UploadCheck(status: status, reason: reason, receivedHash: sent)
    }

    // @verifies SWR-UP-02
    func testModeTwoShowsSentThenClosesWhenTheHelmetAccepts() async throws {
        let rig = try rig(report(.accepted))
        let running = Task { @MainActor in await rig.run(programMode: false, bannerNanos: self.banner) }
        try await Task.sleep(nanoseconds: 100_000_000)
        XCTAssertTrue(rig.screen.showUploadSuccess, "the sent banner is up while the screen waits")
        await running.value
        XCTAssertFalse(rig.screen.showUploadSuccess)
        XCTAssertNil(rig.screen.uploadError)
        XCTAssertEqual(rig.closed, 1)
    }

    // @verifies SWR-UP-02 SWR-HV-08 REQ-UPG-01
    func testModeTwoStaysOpenWithTheReasonWhenTheHelmetRefuses() async throws {
        let rig = try rig(report(.refused, reason: 19))
        await rig.run(programMode: false, bannerNanos: banner)
        XCTAssertEqual(rig.closed, 0, "a refusal keeps the screen open to show why")
        XCTAssertFalse(rig.screen.showUploadSuccess)
        XCTAssertEqual(rig.screen.uploadError, ProtocolUploadFlow.refusalMessage(for: .tier))
    }

    // @verifies SWR-UP-02 SWR-HV-08
    func testModeThreeIsReportedStoredAndClosesWhenTheHelmetAccepts() async throws {
        let rig = try rig(report(.accepted))
        await rig.run(programMode: true, bannerNanos: banner)
        XCTAssertEqual(rig.closed, 1)
        XCTAssertEqual(rig.programmed, 1)
        XCTAssertNil(rig.screen.uploadError)
    }

    // @verifies SWR-UP-02 SWR-HV-08 REQ-UPG-01
    func testModeThreeIsNeitherClosedNorReportedStoredAfterARefusal() async throws {
        for reason in [19, 27, 28, 6, 13] {
            let rig = try rig(report(.refused, reason: reason))
            await rig.run(programMode: true, bannerNanos: banner)
            XCTAssertEqual(rig.closed, 0, "reason \(reason)")
            XCTAssertEqual(rig.programmed, 0, "reason \(reason): the setup flow must not be told the protocol is stored")
            XCTAssertNotNil(rig.screen.uploadError, "reason \(reason)")
        }
    }

    // @verifies DI-SAFE-09 REQ-CVNS-09#f
    func testTheDifferentPersonQuestionIsAskedAndNothingElseHappens() async throws {
        let rig = try rig(report(.accepted))
        await rig.run(programMode: false, bannerNanos: 1, send: { throw UploadError.differentPersonConfirmationRequired })
        XCTAssertTrue(rig.screen.askDifferentPerson)
        XCTAssertNil(rig.screen.uploadError)
        XCTAssertFalse(rig.screen.showUploadSuccess)
        XCTAssertEqual(rig.closed, 0)
    }

    // @verifies SWR-UP-02
    func testAFailedSendIsShownAndTheScreenStaysOpen() async throws {
        let rig = try rig(report(.accepted))
        await rig.run(programMode: true, bannerNanos: 1, send: { throw UploadError.bleNotReady })
        XCTAssertEqual(rig.screen.uploadError, UploadError.bleNotReady.localizedDescription)
        XCTAssertEqual(rig.closed, 0)
        XCTAssertEqual(rig.programmed, 0)
    }

    // @verifies SWR-UP-02
    func testASendThatWasCancelledShowsNothing() async throws {
        let rig = try rig(report(.accepted))
        let screen = rig.screen
        let running = Task { @MainActor in
            await screen.run(flow: rig.flow, entry: rig.entry, programMode: false,
                             send: { try await Task.sleep(nanoseconds: 10_000_000_000); return "" },
                             close: {}, programmed: nil, bannerNanos: 1)
        }
        try await Task.sleep(nanoseconds: 50_000_000)
        running.cancel()
        await running.value
        XCTAssertNil(screen.uploadError, "a selection replaced by another must not leave an error behind")
        XCTAssertFalse(screen.askDifferentPerson)
    }

    // @verifies SWR-HV-08
    func testARefusalReplacesTheSentBanner() throws {
        let screen = ProtocolUploadScreen()
        screen.showUploadSuccess = true
        screen.say("no")
        XCTAssertFalse(screen.showUploadSuccess)
        XCTAssertEqual(screen.uploadError, "no")
    }
}
