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

    // MARK: - Research tier

    func testResearchTierElementsAreEmpty() {
        // Research tier is IRB-defined per study; the base tier grants nothing.
        XCTAssertTrue(
            ClinicianUseCaseTier.research.uhdrElements.isEmpty,
            "Research tier must return an empty element set — elements are specified per-study via signed descriptor."
        )
    }

    // MARK: - hrv_outcomes use case

    func testHRVOutcomesUseCaseIsInLibrary() {
        XCTAssertNotNil(
            ConsentEngine.useCaseLibrary.first { $0.id == "hrv_outcomes" },
            "hrv_outcomes use case must exist in the use case library."
        )
    }

    func testMinimumNecessaryHRVOutcomes_containsFullClinicalElements() {
        let elements = ConsentEngine.minimumNecessaryElements(for: ["hrv_outcomes"])
        // Full Clinical–tier elements that hrv_outcomes must grant.
        XCTAssertTrue(elements.contains(.hrvTimeSeries))
        XCTAssertTrue(elements.contains(.ppgOpticalSignal))
        XCTAssertTrue(elements.contains(.closedLoopEvents))
        XCTAssertTrue(elements.contains(.outcomeLogs))
        // Must also inherit all Monitor / Assess elements.
        XCTAssertTrue(elements.contains(.sessionTimestamps))
        XCTAssertTrue(elements.contains(.eegWaveforms))
        XCTAssertTrue(elements.contains(.pbmDoseLogs))
    }

    func testMinimumNecessaryAllThreeUseCases_equalUnionOfAll() {
        let allIDs: Set<String> = ["adherence_monitoring", "eeg_review", "hrv_outcomes"]
        let union = ConsentEngine.minimumNecessaryElements(for: allIDs)
        // All three together must equal the hrv_outcomes superset (it already contains the others).
        let hrvOnly = ConsentEngine.minimumNecessaryElements(for: ["hrv_outcomes"])
        XCTAssertEqual(union, hrvOnly,
                       "Selecting all three use cases must equal selecting hrv_outcomes alone (it is the superset).")
    }

    func testAllUseCasesHaveNonEmptyElements() {
        for useCase in ConsentEngine.useCaseLibrary {
            XCTAssertFalse(
                useCase.requiredElements.isEmpty,
                "Use case '\(useCase.id)' must require at least one UHDR element."
            )
        }
    }

    // MARK: - Irreversibility notice

    func testIrreversibilityNoticePresent() {
        let notice = ConsentEngine.irreversibilityNotice
        XCTAssertFalse(notice.isEmpty, "Irreversibility notice must be non-empty.")
        XCTAssertTrue(
            notice.contains("withdrawing consent immediately and permanently stops"),
            "Irreversibility notice must contain the forward-effectiveness guarantee phrase verbatim."
        )
    }

    // MARK: - UHDRElement.minimumTier (ISC-137)

    func testMinimumTierMonitorElements() {
        let monitorElements: [UHDRElement] = [.sessionTimestamps, .sessionDuration, .protocolParameters]
        for element in monitorElements {
            XCTAssertEqual(
                element.minimumTier, .monitor,
                "\(element.rawValue) should be accessible at Monitor tier."
            )
        }
    }

    func testMinimumTierAssessElements() {
        let assessElements: [UHDRElement] = [.eegWaveforms, .neurofeedbackScores, .pbmDoseLogs]
        for element in assessElements {
            XCTAssertEqual(
                element.minimumTier, .assess,
                "\(element.rawValue) should require at least Assess tier."
            )
        }
    }

    func testMinimumTierFullClinicalElements() {
        let fullClinicalElements: [UHDRElement] = [
            .hrvTimeSeries, .ppgOpticalSignal, .closedLoopEvents, .outcomeLogs, .eyeStateLogs
        ]
        for element in fullClinicalElements {
            XCTAssertEqual(
                element.minimumTier, .fullClinical,
                "\(element.rawValue) should require Full Clinical tier."
            )
        }
    }

    // ISC-137 anti-pattern: no tier must contain an element below that element's minimumTier.
    func testISC137_noTierContainsElementBelowItsMinimumTier() {
        let tierRank: [ClinicianUseCaseTier: Int] = [
            .monitor: 0, .assess: 1, .fullClinical: 2, .research: 3
        ]
        for tier in ClinicianUseCaseTier.allCases where tier != .research {
            for element in tier.uhdrElements {
                let grantRank = tierRank[tier]!
                let minimumRank = tierRank[element.minimumTier]!
                XCTAssertLessThanOrEqual(
                    minimumRank, grantRank,
                    "Tier \(tier.rawValue) grants \(element.rawValue) but its minimumTier is \(element.minimumTier.rawValue) — over-grant detected."
                )
            }
        }
    }

    // ISC-82 anti-pattern: minimumNecessaryElements never returns elements absent from selected use cases.
    func testISC82_minimumNecessaryNeverOverGrants() {
        let allIDs = Set(ConsentEngine.useCaseLibrary.map(\.id))
        let allExpected = ConsentEngine.useCaseLibrary
            .reduce(into: Set<UHDRElement>()) { $0.formUnion($1.requiredElements) }
        let returned = ConsentEngine.minimumNecessaryElements(for: allIDs)
        // Every returned element must be present in at least one use case.
        for element in returned {
            XCTAssertTrue(
                allExpected.contains(element),
                "\(element.rawValue) returned but is not required by any use case — over-grant."
            )
        }
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
