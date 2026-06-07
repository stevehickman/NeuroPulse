//
//  SessionDisplayTests.swift
//  NeuroPulseTests
//
//  Subject: SessionView display-logic helpers
//           (app/ios/NeuroPulse/Views/SessionView.swift)
//
//  Satisfies ISC-30: session-completed state shows "Download Session Data" button
//  that triggers EDFDownloader.requestDownload(sessionID:).
//
//  The download-button predicate and the edfSessionID extractor are pure static
//  functions on SessionView — no UIKit, no SwiftUI state, fully unit-testable.

import XCTest
@testable import NeuroPulse

final class SessionDisplayTests: XCTestCase {

    // MARK: - ISC-30: shouldShowSessionDownload predicate

    func testDownloadButtonRequiresCompletedStatus() {
        // All non-completed statuses must not show the download button, even with
        // a valid epoch.
        XCTAssertFalse(SessionView.shouldShowSessionDownload(status: .idle,    epoch: 1001))
        XCTAssertFalse(SessionView.shouldShowSessionDownload(status: .running, epoch: 1001))
        XCTAssertFalse(SessionView.shouldShowSessionDownload(status: .paused,  epoch: 1001))
    }

    func testDownloadButtonHiddenWhenEpochIsZero() {
        // epoch == 0 means the hub has not assigned a session ID. The app must not
        // offer a download for a session that has no hub-side record.
        XCTAssertFalse(
            SessionView.shouldShowSessionDownload(status: .completed, epoch: 0),
            "epoch 0 → no hub session ID → download must not be offered"
        )
    }

    func testDownloadButtonShownWhenCompletedAndEpochNonZero() {
        XCTAssertTrue(SessionView.shouldShowSessionDownload(status: .completed, epoch: 1))
        XCTAssertTrue(SessionView.shouldShowSessionDownload(status: .completed, epoch: 1001))
        XCTAssertTrue(SessionView.shouldShowSessionDownload(status: .completed, epoch: UInt32.max))
    }

    // MARK: - ISC-30: edfSessionID epoch mapping (feeds handleStatusChange history record)

    func testEDFSessionIDIsNilForZeroEpoch() {
        // epoch 0 must not be stored as Optional(0) in the session record — nil prevents
        // SessionHistoryView from showing a spurious Download button for that record.
        XCTAssertNil(
            SessionView.edfSessionID(from: 0),
            "epoch 0 must map to nil, not Optional(0)"
        )
    }

    func testEDFSessionIDPreservesNonZeroEpoch() {
        XCTAssertEqual(SessionView.edfSessionID(from: 1),          1)
        XCTAssertEqual(SessionView.edfSessionID(from: 1001),       1001)
        XCTAssertEqual(SessionView.edfSessionID(from: UInt32.max), UInt32.max)
    }
}
