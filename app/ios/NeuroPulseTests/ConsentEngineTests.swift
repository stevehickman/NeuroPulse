//
//  ConsentEngineTests.swift
//  NeuroPulseTests
//
//  Satisfies: ISC-153 (consent engine maps use cases to minimum-necessary UHDR elements and
//             surfaces the irreversibility notice + plain-language document), ISC-157.
//
//  Subject under test: ConsentEngine + ConsentDocument (app/ios/NeuroPulse/Consent/ConsentEngine.swift)
//                      backed by ClinicianUseCaseTier / UHDRElement (Models/ConsentModels.swift)

import XCTest
@testable import NeuroPulse

final class ConsentEngineTests: XCTestCase {

    // MARK: - Tier element coverage

    func testMonitorTierElements() {
        let elements = ClinicianUseCaseTier.monitor.uhdrElements
        XCTAssertEqual(
            elements,
            [.sessionTimestamps, .sessionDuration, .protocolParameters],
            "Monitor tier must expose ONLY adherence metadata — no biology."
        )
        // Explicitly confirm no EEG/HRV leak at the lowest tier.
        XCTAssertFalse(elements.contains(.eegWaveforms))
        XCTAssertFalse(elements.contains(.hrvTimeSeries))
    }

    func testAssessTierAddsEEG() {
        let elements = ClinicianUseCaseTier.assess.uhdrElements
        XCTAssertTrue(elements.contains(.eegWaveforms),
                      "Assess tier must add EEG waveform access.")
        // Assess still must not include HRV (that is Full Clinical only).
        XCTAssertFalse(elements.contains(.hrvTimeSeries),
                       "Assess tier must NOT include HRV — that is a Full Clinical element.")
        // Assess is a superset of Monitor.
        XCTAssertTrue(elements.isSuperset(of: ClinicianUseCaseTier.monitor.uhdrElements))
    }

    func testFullClinicalTierAddsHRV() {
        let elements = ClinicianUseCaseTier.fullClinical.uhdrElements
        XCTAssertTrue(elements.contains(.hrvTimeSeries),
                      "Full Clinical tier must add HRV time-series access.")
        XCTAssertTrue(elements.contains(.ppgOpticalSignal))
        XCTAssertTrue(elements.contains(.closedLoopEvents))
        // Full Clinical is a superset of Assess.
        XCTAssertTrue(elements.isSuperset(of: ClinicianUseCaseTier.assess.uhdrElements))
    }

    // MARK: - Use-case minimum-necessary union

    func testMinimumNecessaryUnion() {
        // adherence_monitoring (Monitor) + eeg_review (Assess) → union of both element sets.
        let selected: Set<String> = ["adherence_monitoring", "eeg_review"]
        let union = ConsentEngine.minimumNecessaryElements(for: selected)

        let adherence = ConsentEngine.useCaseLibrary.first { $0.id == "adherence_monitoring" }!.requiredElements
        let eegReview = ConsentEngine.useCaseLibrary.first { $0.id == "eeg_review" }!.requiredElements
        XCTAssertEqual(union, adherence.union(eegReview),
                       "Minimum-necessary set must be the exact union of selected use cases.")

        // Must include the EEG element contributed by the Assess use case.
        XCTAssertTrue(union.contains(.eegWaveforms))
        // Must NOT include HRV — neither selected use case requires it.
        XCTAssertFalse(union.contains(.hrvTimeSeries),
                       "Union must not over-grant elements no selected use case needs.")
    }

    func testMinimumNecessaryEmptySelection() {
        XCTAssertTrue(ConsentEngine.minimumNecessaryElements(for: []).isEmpty,
                      "No selected use cases must grant no elements.")
    }

    // MARK: - Irreversibility notice

    func testIrreverseibilityNoticePresent() {
        let notice = ConsentEngine.irreversibilityNotice
        XCTAssertFalse(notice.isEmpty, "Irreversibility notice must be non-empty.")
        XCTAssertTrue(
            notice.contains("withdrawing consent immediately and permanently stops"),
            "Irreversibility notice must contain the forward-effectiveness guarantee phrase verbatim."
        )
    }

    // MARK: - Consent document generation

    func testConsentDocumentGeneration() {
        let doc = ConsentEngine.consentDocument(
            clinicianName: "Dr. Neda Rashidi-Ranjbar",
            organisation: "St. Michael's Hospital",
            selectedUseCaseIDs: ["adherence_monitoring", "eeg_review"],
            tier: .assess
        )

        XCTAssertEqual(doc.clinicianName, "Dr. Neda Rashidi-Ranjbar",
                       "Generated document must carry the requested clinician name.")
        XCTAssertEqual(doc.tier, .assess)
        XCTAssertFalse(doc.plainLanguageSummary.isEmpty,
                       "Plain-language summary must be non-empty.")
        // Summary should name the clinician and organisation.
        XCTAssertTrue(doc.plainLanguageSummary.contains("Dr. Neda Rashidi-Ranjbar"))
        XCTAssertTrue(doc.plainLanguageSummary.contains("St. Michael's Hospital"))

        // The "cannot see" set must be the complement of the approved set — no overlap, full cover.
        XCTAssertTrue(doc.approvedElements.isDisjoint(with: doc.cannotAccessElements),
                      "Approved and cannot-access element sets must not overlap.")
        XCTAssertEqual(
            doc.approvedElements.union(doc.cannotAccessElements),
            Set(UHDRElement.allCases),
            "Approved ∪ cannot-access must cover every UHDR element exactly once."
        )
    }
}
