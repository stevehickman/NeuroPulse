//
//  StudyDescriptorSigningTests.swift
//  NeurOneTests
//
//  Subject under test: StudyDescriptorCanonicalForm + Ed25519StudyDescriptorVerifier
//                      (app/ios/NeurOne/Consent/StudyDescriptorSigning.swift)
//
//  NP-SW-PORTAL-API-001 §4a. The canonical form is the one schema decision that document takes,
//  so it is pinned here by its bytes rather than described — these are the same bytes Android's
//  StudyDescriptorSigningTests.theCanonicalFormIsExactlyThis pins, and the two must not drift.

import CryptoKit
import XCTest
@testable import NeurOne

final class StudyDescriptorSigningTests: XCTestCase {

    private let privateKey = Curve25519.Signing.PrivateKey()

    private var verifier: Ed25519StudyDescriptorVerifier {
        get throws { try Ed25519StudyDescriptorVerifier(publicKeyRaw: privateKey.publicKey.rawRepresentation) }
    }

    /// 2026-09-14T00:00:00Z, so the day rendering is unambiguous.
    private let issued = Date(timeIntervalSince1970: 1_789_344_000)

    private func descriptor(
        studyID: String = "NP-STUDY-2026-014",
        studyTitle: String = "Sleep onset latency under 810 nm PBM",
        categories: [ResearchCategory] = [.sleep, .attention],
        elements: Set<UHDRElement> = [.sessionTimestamps, .eegWaveforms, .pbmDoseLogs],
        k: Int = 10,
        rounding: Int = 7,
        issuedAt: Date? = nil,
        signature: String = ""
    ) -> StudyDescriptor {
        StudyDescriptor(
            studyID: studyID, studyTitle: studyTitle, researchCategories: categories,
            requestedElements: elements, kAnonymity: k, dateRoundingDays: rounding,
            issuedAt: issuedAt ?? issued, signature: signature
        )
    }

    private func signed(_ d: StudyDescriptor) throws -> StudyDescriptor {
        var copy = d
        let sig = try privateKey.signature(for: StudyDescriptorCanonicalForm.bytes(for: d))
        copy.signature = sig.base64EncodedString()
        return copy
    }

    // MARK: - The canonical form itself

    /// The exact bytes. This is the contract Android's `StudyDescriptorCanonicalForm.bytes`
    /// reproduces character for character; describing it in prose on both sides is how two
    /// implementations drift.
    func testTheCanonicalFormIsExactlyThis() {
        let bytes = StudyDescriptorCanonicalForm.bytes(
            for: descriptor(
                studyID: "S1", studyTitle: "Trial", categories: [.sleep],
                elements: [.sessionDuration, .eegWaveforms], k: 10, rounding: 7
            )
        )
        XCTAssertEqual(
            String(decoding: bytes, as: UTF8.self),
            """
            NP-STUDY-DESCRIPTOR-V1
            2:S1
            5:Trial
            15:Sleep Disorders
            30:EEG Waveforms,Session Duration
            2:10
            1:7
            10:2026-09-14

            """
        )
    }

    /// The day is derived in UTC, not in the device's zone: a descriptor must not verify in one
    /// time zone and fail in another.
    func testTheDayIsRenderedInUTC() {
        // 2026-09-14T23:30:00Z — the previous day in any negative offset, the same day in UTC.
        let lateUTC = Date(timeIntervalSince1970: 1_789_428_600)
        XCTAssertEqual(StudyDescriptorCanonicalForm.day(from: lateUTC), "2026-09-14")
    }

    func testElementOrderDoesNotChangeTheBytes() {
        // Set has no inherent order; the encoding must impose one.
        let a = descriptor(elements: [.eegWaveforms, .outcomeLogs])
        let b = descriptor(elements: [.outcomeLogs, .eegWaveforms])
        XCTAssertEqual(StudyDescriptorCanonicalForm.bytes(for: a),
                       StudyDescriptorCanonicalForm.bytes(for: b))
    }

    /// Why fields are length-prefixed rather than delimited: `studyTitle` is server-supplied text,
    /// so it can contain the separator. Two different descriptors must never share bytes, or one
    /// signature covers both.
    func testATitleCarryingTheSeparatorCannotForgeAnotherDescriptor() {
        XCTAssertNotEqual(
            StudyDescriptorCanonicalForm.bytes(for: descriptor(studyID: "S1", studyTitle: "Trial")),
            StudyDescriptorCanonicalForm.bytes(for: descriptor(studyID: "S1", studyTitle: "Trial\n5:Other"))
        )
    }

    func testTheHashDigestsTheSignedBytes() {
        let d = descriptor()
        let hash = StudyDescriptorCanonicalForm.descriptorHash(for: d)
        XCTAssertTrue(hash.hasPrefix("sha256:"))
        XCTAssertEqual(hash.count, 71)  // "sha256:" + 64 hex
        // The signature field is not part of what is signed, so it cannot move the hash.
        XCTAssertEqual(hash, StudyDescriptorCanonicalForm.descriptorHash(for: descriptor(signature: "zzzz")))
    }

    // MARK: - The verifier

    func testAGenuinelySignedDescriptorVerifiesAndCarriesItsHash() throws {
        let d = try signed(descriptor())
        XCTAssertEqual(
            try verifier.verify(d),
            .verified(descriptorHash: StudyDescriptorCanonicalForm.descriptorHash(for: d))
        )
    }

    /// Every field is covered: tampering with any of them must break the signature.
    func testTamperingWithAnyCoveredFieldRejects() throws {
        let base = try signed(descriptor())
        let v = try verifier
        var cases: [StudyDescriptor] = []
        for mutate in [
            { (d: inout StudyDescriptor) in d.studyID = "NP-STUDY-2026-015" },
            { (d: inout StudyDescriptor) in d.studyTitle = "Something else" },
            { (d: inout StudyDescriptor) in d.researchCategories = [.depression] },
            { (d: inout StudyDescriptor) in d.requestedElements.insert(.hrvTimeSeries) },
            { (d: inout StudyDescriptor) in d.kAnonymity = 1 },
            { (d: inout StudyDescriptor) in d.dateRoundingDays = 0 },
            { (d: inout StudyDescriptor) in d.issuedAt = d.issuedAt.addingTimeInterval(86_400) },
        ] {
            var copy = base
            mutate(&copy)
            cases.append(copy)
        }
        for tampered in cases {
            XCTAssertEqual(v.verify(tampered), .rejected,
                           "tampering must reject: \(tampered.studyID)")
        }
    }

    func testADescriptorSignedByAnotherKeyIsRejected() throws {
        let other = Curve25519.Signing.PrivateKey()
        var d = descriptor()
        d.signature = try other.signature(
            for: StudyDescriptorCanonicalForm.bytes(for: d)
        ).base64EncodedString()
        XCTAssertEqual(try verifier.verify(d), .rejected)
    }

    /// A malformed signature is `.rejected`, never `.unavailable`. `.unavailable` means the device
    /// cannot check; a device holding a key can, and collapsing the two would let a forged
    /// descriptor read as a configuration problem.
    func testAMalformedSignatureIsRejectedNotUnavailable() throws {
        let v = try verifier
        XCTAssertEqual(v.verify(descriptor(signature: "")), .rejected)
        XCTAssertEqual(v.verify(descriptor(signature: "not base64!!")), .rejected)
        XCTAssertEqual(v.verify(descriptor(signature: Data(count: 8).base64EncodedString())), .rejected)
    }

    /// A build with no key, or a broken one, falls back to having no verifier at all — and so to
    /// `RefusingStudyDescriptorVerifier` — rather than to one that fails at the first descriptor.
    func testAnAbsentOrUnusableKeyYieldsNoVerifierAtAll() {
        XCTAssertNil(Ed25519StudyDescriptorVerifier(publicKeyBase64: nil))
        XCTAssertNil(Ed25519StudyDescriptorVerifier(publicKeyBase64: ""))
        XCTAssertNil(Ed25519StudyDescriptorVerifier(publicKeyBase64: "not base64!!"))
        XCTAssertNil(Ed25519StudyDescriptorVerifier(publicKeyBase64: Data(count: 16).base64EncodedString()))
        XCTAssertNotNil(Ed25519StudyDescriptorVerifier(
            publicKeyBase64: privateKey.publicKey.rawRepresentation.base64EncodedString()
        ))
    }

    /// The wire names a signature covers. They are `rawValue` on iOS — the persisted Codable
    /// value — and `wireName` on Android, and they must be the same strings or one platform
    /// rejects every descriptor the other accepts.
    func testWireNamesArePinned() {
        XCTAssertEqual(
            UHDRElement.allCases.map(\.rawValue).sorted(),
            ["Closed-Loop Adaptation Events", "EEG Waveforms", "Eye Open/Closed State",
             "HRV Time Series", "Neurofeedback Performance Scores", "PBM Dose (J/cm²) Per Zone",
             "PPG Optical Signal", "Protocol Parameters", "Session Duration",
             "Session Timestamps", "User-Entered Outcome Logs"]
        )
        XCTAssertEqual(
            ResearchCategory.allCases.map(\.rawValue).sorted(),
            ["Alzheimer's / Dementia", "Attention / ADHD", "Depression", "Healthy Ageing",
             "PTSD", "Parkinson's Disease", "Sleep Disorders", "Traumatic Brain Injury",
             "Visual Health"]
        )
    }
}
