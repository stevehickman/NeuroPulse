//
//  UploadBannersTests.swift
//  NeurOneTests
//
//  What the protocol menu's banners draw (SWR-UP-02, SWR-HV-08, REQ-UPG-01). `ProtocolUploadScreenTests` covers when each
//  banner is up and when the screen closes; these cover what a banner shows once it is up: the helmet's reason, word for
//  word, and a Retry that sends the same protocol again. They read the rendered view tree with ViewInspector (a test-only
//  package, NP-SOUP-NATIVE-001). Colours, spacing and animation are not asserted: a snapshot of them would fail on every
//  design change and say nothing about what the person is told.
//
//  Subject under test: UploadErrorBanner, UploadSentBanner (app/ios/NeurOne/Views/Protocol/UploadBanners.swift)

import SwiftUI
import ViewInspector
import XCTest
@testable import NeurOne

@MainActor
final class UploadBannersTests: XCTestCase {

    // @verifies SWR-HV-08
    func testTheErrorBannerShowsEveryRefusalMessageWordForWord() throws {
        let causes: [HelmetRefusalCause] = [
            .tier, .hardwareVersion, .notReady, .signature, .wrongDevice, .formatVersion, .damaged, .malformed,
            .sessionActive, .bootRefused, .other, .unmatched,
        ]
        for cause in causes {
            let message = ProtocolUploadFlow.refusalMessage(for: cause)
            let sut = UploadErrorBanner(message: message, onRetry: {})
            XCTAssertEqual(try sut.inspect().find(ViewType.Text.self).string(), message, "\(cause)")
        }
    }

    // @verifies REQ-UPG-01
    func testTheTierRefusalOnScreenNamesNoPurchase() throws {
        // NP-PWRSRC-001 §6.3: the refusal says what the helmet cannot run and never what to buy. The words below are English;
        // in another locale this passes without checking, which is the same limit the strings have until they are translated.
        let shown = try UploadErrorBanner(message: ProtocolUploadFlow.refusalMessage(for: .tier), onRetry: {})
            .inspect().find(ViewType.Text.self).string().lowercased()
        for word in ["buy", "purchase", "upgrade", "price", "pro ", "$"] {
            XCTAssertFalse(shown.contains(word), "the tier refusal must not name a purchase: \(word)")
        }
    }

    // @verifies SWR-UP-02
    func testRetryOnTheErrorBannerCallsBackOnceAndShowsItsOwnLabel() throws {
        var retried = 0
        let sut = UploadErrorBanner(message: "the helmet said no", onRetry: { retried += 1 })
        try sut.inspect().find(button: String(localized: "SETUP_RETRY_BUTTON")).tap()
        XCTAssertEqual(retried, 1)
    }

    // @verifies SWR-UP-02
    func testTheSentBannerSaysSentAndNothingElse() throws {
        let sut = UploadSentBanner()
        let texts = try sut.inspect().findAll(ViewType.Text.self).map { try $0.string() }
        XCTAssertEqual(texts, [String(localized: "PROTOCOL_MENU_PROTOCOL_SENT_TO_HUB")])
    }
}
