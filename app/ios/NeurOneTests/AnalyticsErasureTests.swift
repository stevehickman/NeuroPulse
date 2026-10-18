import XCTest
@testable import NeurOne

// A withdrawn person's pseudonymous analytics ID is kept, durably, until the vendor's erasure relay has answered for it
// (NP-APP-TELEMETRY-001 §5, REQ-SW03-19).

/// A relay that answers with a fixed outcome and records what it was asked.
@MainActor
final class FakeErasureRelay: AnalyticsErasureRelaying {
    var outcome: ErasureOutcome
    private(set) var asked: [String] = []
    var onErase: (() -> Void)?

    init(outcome: ErasureOutcome) { self.outcome = outcome }

    func erase(distinctID: String) async -> ErasureOutcome {
        asked.append(distinctID)
        onErase?()
        return outcome
    }
}

/// Answers `HTTPAnalyticsErasureRelay`'s requests from a closure, and keeps the last request for inspection.
final class StubURLProtocol: URLProtocol {
    static var handler: ((URLRequest) throws -> HTTPURLResponse)?
    static var lastRequest: URLRequest?
    static var lastBody: Data?

    override class func canInit(with request: URLRequest) -> Bool { true }
    override class func canonicalRequest(for request: URLRequest) -> URLRequest { request }

    override func startLoading() {
        Self.lastRequest = request
        if let stream = request.httpBodyStream {
            stream.open()
            defer { stream.close() }
            var data = Data()
            var buffer = [UInt8](repeating: 0, count: 1024)
            while stream.hasBytesAvailable {
                let n = stream.read(&buffer, maxLength: buffer.count)
                if n <= 0 { break }
                data.append(buffer, count: n)
            }
            Self.lastBody = data
        } else {
            Self.lastBody = request.httpBody
        }
        do {
            let response = try Self.handler!(request)
            client?.urlProtocol(self, didReceive: response, cacheStoragePolicy: .notAllowed)
            client?.urlProtocol(self, didLoad: Data())
            client?.urlProtocolDidFinishLoading(self)
        } catch {
            client?.urlProtocol(self, didFailWithError: error)
        }
    }

    override func stopLoading() {}
}

@MainActor
final class AnalyticsErasureTests: XCTestCase {

    private let consentKey = "np.research.consent-granted"
    private var dir: URL!
    private var legacyName: String!
    private var store: ProtectedDefaults!
    private var spy: SpyAnalyticsBackend!

    override func setUp() async throws {
        try await super.setUp()
        dir = FileManager.default.temporaryDirectory.appendingPathComponent("erasure-\(UUID().uuidString)")
        legacyName = "erasure-\(UUID().uuidString)"
        store = ProtectedDefaults(directory: dir, legacy: UserDefaults(suiteName: legacyName)!)
        spy = SpyAnalyticsBackend()
        ResearchAnalyticsGate._backend = spy
        ResearchAnalyticsGate._resetForTesting()
        UserDefaults.standard.removeObject(forKey: consentKey)
    }

    override func tearDown() async throws {
        ResearchAnalyticsGate._resetForTesting()
        ResearchAnalyticsGate._backend = PostHogAnalyticsBackend()
        ResearchAnalyticsGate._erasureQueue = .shared
        UserDefaults.standard.removeObject(forKey: consentKey)
        UserDefaults(suiteName: legacyName)?.removePersistentDomain(forName: legacyName)
        try? FileManager.default.removeItem(at: dir)
        StubURLProtocol.handler = nil
        StubURLProtocol.lastRequest = nil
        StubURLProtocol.lastBody = nil
        try await super.tearDown()
    }

    private func queue(_ relay: FakeErasureRelay) -> AnalyticsErasureQueue {
        AnalyticsErasureQueue(store: store, relay: relay)
    }

    // MARK: - The queue keeps an ID until the relay has answered

    // @verifies REQ-SW03-19#b
    func testEnqueueKeepsTheIDAcrossAnInstanceAndIgnoresDuplicatesAndEmpty() {
        let q = queue(FakeErasureRelay(outcome: .retryLater))
        q.enqueue("id-1")
        q.enqueue("id-1")
        q.enqueue("")
        q.enqueue("id-2")
        XCTAssertEqual(q.pending, ["id-1", "id-2"])
        // A new queue over the same store (a relaunch) still holds them.
        XCTAssertEqual(queue(FakeErasureRelay(outcome: .retryLater)).pending, ["id-1", "id-2"])
    }

    // @verifies REQ-SW03-19#b
    func testAnAcceptedIDLeavesTheQueue() async {
        let relay = FakeErasureRelay(outcome: .done)
        let q = queue(relay)
        q.enqueue("id-1")
        q.enqueue("id-2")
        await q.drain()
        XCTAssertEqual(relay.asked, ["id-1", "id-2"])
        XCTAssertEqual(q.pending, [])
    }

    // @verifies REQ-SW03-19#b
    func testARefusedIDLeavesTheQueueBecauseRetryingCannotChangeTheAnswer() async {
        let q = queue(FakeErasureRelay(outcome: .refused))
        q.enqueue("id-1")
        await q.drain()
        XCTAssertEqual(q.pending, [])
    }

    // @verifies REQ-SW03-19#b
    func testATransientFailureKeepsEveryIDAndStopsAfterTheFirst() async {
        let relay = FakeErasureRelay(outcome: .retryLater)
        let q = queue(relay)
        q.enqueue("id-1")
        q.enqueue("id-2")
        await q.drain()
        XCTAssertEqual(q.pending, ["id-1", "id-2"], "nothing is lost while the relay cannot be reached")
        XCTAssertEqual(relay.asked, ["id-1"], "the network is likely down for all of them, so the rest wait for the next try")

        relay.outcome = .done
        await q.drain()
        XCTAssertEqual(q.pending, [], "the next attempt, when the relay answers, clears them")
    }

    // @verifies REQ-SW03-19#b
    func testAnEmptyQueueMakesNoRelayCall() async {
        let relay = FakeErasureRelay(outcome: .done)
        await queue(relay).drain()
        XCTAssertTrue(relay.asked.isEmpty)
    }

    // MARK: - The gate captures the ID before the SDK resets it

    // @verifies REQ-SW03-19#a
    func testWithdrawalQueuesTheIDTheVendorHeldBeforeTheSDKMadeANewOne() {
        let relay = FakeErasureRelay(outcome: .retryLater)
        let q = queue(relay)
        ResearchAnalyticsGate._erasureQueue = q
        UserDefaults.standard.set(true, forKey: consentKey)
        ResearchAnalyticsGate.configure()
        spy.distinctID = "the-id-with-the-vendor"

        ResearchAnalyticsGate.reset()

        XCTAssertEqual(spy.resetCallCount, 1)
        XCTAssertEqual(spy.distinctID, "spy-distinct-id-after-reset", "the spy models the SDK making a new ID on reset")
        XCTAssertEqual(q.pending, ["the-id-with-the-vendor"], "the OLD ID is what must be erased, not the one reset() made")
    }

    // @verifies REQ-SW03-19#a
    func testWithdrawalAsksTheRelayStraightAway() {
        let relay = FakeErasureRelay(outcome: .done)
        let asked = expectation(description: "the relay is asked about the withdrawn ID")
        relay.onErase = { asked.fulfill() }
        ResearchAnalyticsGate._erasureQueue = queue(relay)
        UserDefaults.standard.set(true, forKey: consentKey)
        ResearchAnalyticsGate.configure()
        spy.distinctID = "id-to-erase"

        ResearchAnalyticsGate.reset()

        wait(for: [asked], timeout: 2)
        XCTAssertEqual(relay.asked, ["id-to-erase"])
    }

    // @verifies REQ-SW03-19#a
    func testNothingIsQueuedWhenTheSDKHeldNoID() {
        let q = queue(FakeErasureRelay(outcome: .retryLater))
        ResearchAnalyticsGate._erasureQueue = q
        UserDefaults.standard.set(true, forKey: consentKey)
        ResearchAnalyticsGate.configure()
        spy.distinctID = nil

        ResearchAnalyticsGate.reset()

        XCTAssertEqual(spy.resetCallCount, 1)
        XCTAssertEqual(q.pending, [])
    }

    // @verifies REQ-SW03-19#a
    func testResettingAGateThatWasNeverConfiguredQueuesNothing() {
        let q = queue(FakeErasureRelay(outcome: .retryLater))
        ResearchAnalyticsGate._erasureQueue = q
        ResearchAnalyticsGate.reset()
        XCTAssertEqual(spy.resetCallCount, 0)
        XCTAssertEqual(q.pending, [])
    }

    // MARK: - What the relay is sent

    private func stubbedRelay(_ status: Int) -> HTTPAnalyticsErasureRelay {
        let config = URLSessionConfiguration.ephemeral
        config.protocolClasses = [StubURLProtocol.self]
        StubURLProtocol.handler = { request in
            HTTPURLResponse(url: request.url!, statusCode: status, httpVersion: nil, headerFields: nil)!
        }
        return HTTPAnalyticsErasureRelay(session: URLSession(configuration: config))
    }

    // @verifies REQ-SW03-19#c
    func testTheRequestIsOnePOSTOfTheIDAndNothingElse() async throws {
        let outcome = await stubbedRelay(202).erase(distinctID: "abc-123")
        XCTAssertEqual(outcome, .done)
        let request = try XCTUnwrap(StubURLProtocol.lastRequest)
        XCTAssertEqual(request.httpMethod, "POST")
        XCTAssertEqual(request.url, HTTPAnalyticsErasureRelay.endpoint)
        XCTAssertEqual(request.value(forHTTPHeaderField: "Content-Type"), "application/json")
        let body = try XCTUnwrap(StubURLProtocol.lastBody)
        let json = try XCTUnwrap(JSONSerialization.jsonObject(with: body) as? [String: String])
        XCTAssertEqual(json, ["distinct_id": "abc-123"], "no account, device name, token or consent record travels with it")
        XCTAssertNil(request.value(forHTTPHeaderField: "Authorization"))
    }

    // @verifies REQ-SW03-19#b
    func testTheRelaysAnswersMapToDoneRefusedOrRetry() async {
        for (status, expected) in [(200, ErasureOutcome.done), (202, .done), (204, .done),
                                   (400, .refused), (403, .refused), (404, .refused), (422, .refused),
                                   (408, .retryLater), (429, .retryLater), (500, .retryLater), (503, .retryLater)] {
            let outcome = await stubbedRelay(status).erase(distinctID: "id")
            XCTAssertEqual(outcome, expected, "HTTP \(status)")
        }
    }

    // @verifies REQ-SW03-19#b
    func testANetworkErrorIsRetriedLater() async {
        let config = URLSessionConfiguration.ephemeral
        config.protocolClasses = [StubURLProtocol.self]
        StubURLProtocol.handler = { _ in throw URLError(.notConnectedToInternet) }
        let outcome = await HTTPAnalyticsErasureRelay(session: URLSession(configuration: config)).erase(distinctID: "id")
        XCTAssertEqual(outcome, .retryLater)
    }
}
