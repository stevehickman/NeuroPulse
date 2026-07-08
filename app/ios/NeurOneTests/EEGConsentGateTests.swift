// EEGConsentGateTests.swift
//
// Verifies ISC-90 (second half): when the user declines the BIPA biometric
// written release, EEG-neurofeedback-dependent protocols are blocked in the
// protocol menu — and the Watch preset picker cannot bypass that block via the
// phone. SessionView's closed-loop-metrics half of ISC-90 is covered elsewhere.
//
// Subjects under test:
//   • NPProtocolDefinition.isEEGDependent (app/ios/NeurOne/Protocol/NPProtocolDefinition.swift)
//   • NPProtocolLibrary.availability(for:) BIPA gate (…/NPProtocolLibrary.swift)
//   • PhoneSessionManager.acceptedPreset(_:consentGranted:) (…/WatchBridge/PhoneSessionManager.swift)

import XCTest
@testable import NeurOne

final class EEGConsentGateTests: XCTestCase {

    private let acceptedKey = "np.onboarding.bipa-accepted"

    override func setUp() {
        super.setUp()
        UserDefaults.standard.removeObject(forKey: acceptedKey)
    }

    override func tearDown() {
        UserDefaults.standard.removeObject(forKey: acceptedKey)
        super.tearDown()
    }

    // MARK: - Builders

    /// An EEG-neurofeedback protocol (closed-loop EEG modality).
    private func eegProtocol() -> NPProtocolEntry {
        .single(NPProtocolDefinition(
            name: "Test EEG Neurofeedback",
            modalities: [
                NPProtocolModality(params: .eegNeurofeedback(
                    NPEEGNeurofeedbackParams(channels: .all, customChannels: nil,
                                             band: .alpha, closedLoopEnabled: true)))
            ]))
    }

    /// A non-EEG protocol (transcranial PBM only).
    private func pbmProtocol() -> NPProtocolEntry {
        .single(NPProtocolDefinition(
            name: "Test PBM Vascular",
            modalities: [
                NPProtocolModality(params: .pbmTranscranial(
                    NPPBMTranscranialParams(zones: .all, customZones: nil,
                                            wavelength: .base660_808nm,
                                            intensityPercent: 75, frequencyHz: 0,
                                            dutyCyclePercent: 25)))
            ]))
    }

    /// An audio-only protocol with EEG-adaptive coupling — also EEG-dependent.
    private func eegAdaptiveAudioProtocol() -> NPProtocolEntry {
        .single(NPProtocolDefinition(
            name: "Test EEG-Adaptive Audio",
            modalities: [
                NPProtocolModality(params: .audioEntrainment(
                    NPAudioEntrainmentParams(binauralBeatsHz: 20, isochronicTonesHz: nil,
                                             noiseType: .pink, carrierHz: 440,
                                             volumePercent: 60, eegAdaptive: true,
                                             boneConductionPacer: true)))
            ]))
    }

    // MARK: - ISC-1 / ISC-10  isEEGDependent predicate

    func testIsEEGDependent_trueForEEGModality() {
        XCTAssertTrue(eegProtocol().isEEGDependent)
    }

    func testIsEEGDependent_trueForEEGAdaptiveAudio() {
        XCTAssertTrue(eegAdaptiveAudioProtocol().isEEGDependent,
                      "audio with eegAdaptive == true reads brainwave data to close the loop")
    }

    func testIsEEGDependent_falseForNonEEG() {
        XCTAssertFalse(pbmProtocol().isEEGDependent)
    }

    // MARK: - ISC-6  availability gate blocks EEG protocols on declined consent

    @MainActor
    func testEEGProtocol_unavailableWhenConsentDeclined() {
        let library = NPProtocolLibrary()
        library.updateAvailability(tier: .t1, smartModules: false)   // hardware present
        library.updateEEGConsent(false)                              // consent declined

        let avail = library.availability(for: eegProtocol())
        XCTAssertFalse(avail.isAvailable)
        XCTAssertEqual(avail, .eegConsentRequired,
                       "EEG protocol must be blocked with the consent-specific reason, " +
                       "not a hardware-missing reason.")
    }

    // MARK: - ISC-4  the block carries the localized ISC-90 message

    @MainActor
    func testEEGConsentRequired_carriesLocalizedISC90Message() {
        let reason = ProtocolAvailability.eegConsentRequired.unavailableReason ?? ""
        XCTAssertTrue(reason.contains("brainwave data consent was not granted"),
                      "reason must be the localized SESSION_EEG_UNAVAILABLE_BODY string")
    }

    // MARK: - ISC-6  EEG protocol becomes available once consent granted

    @MainActor
    func testEEGProtocol_availableWhenConsentGranted() {
        let library = NPProtocolLibrary()
        library.updateAvailability(tier: .t1, smartModules: false)
        library.updateEEGConsent(true)

        XCTAssertTrue(library.availability(for: eegProtocol()).isAvailable)
    }

    // MARK: - ISC-7  non-EEG protocol is unaffected by consent state

    @MainActor
    func testNonEEGProtocol_unaffectedByConsentState() {
        let library = NPProtocolLibrary()
        library.updateAvailability(tier: .t1, smartModules: false)

        library.updateEEGConsent(false)
        XCTAssertTrue(library.availability(for: pbmProtocol()).isAvailable,
                      "a PBM-only protocol must remain available even without EEG consent")

        library.updateEEGConsent(true)
        XCTAssertTrue(library.availability(for: pbmProtocol()).isAvailable)
    }

    // MARK: - ISC-5  library seeds consent state from UserDefaults

    @MainActor
    func testLibrarySeedsConsentFromUserDefaults() {
        UserDefaults.standard.set(true, forKey: acceptedKey)
        let library = NPProtocolLibrary()
        XCTAssertTrue(library.eegConsentGranted,
                      "eegConsentGranted must seed from np.onboarding.bipa-accepted")
    }

    // MARK: - ISC-11  Watch preset gate on the phone

    func testAcceptedPreset_dropsEEGPresetsWithoutConsent() {
        // Gamma Clarity (0) and Alpha Calm (1) are EEG-adaptive.
        XCTAssertNil(PhoneSessionManager.acceptedPreset(0, consentGranted: false))
        XCTAssertNil(PhoneSessionManager.acceptedPreset(1, consentGranted: false))
    }

    func testAcceptedPreset_allowsSleepPresetWithoutConsent() {
        // Sleep Deep (2) is not EEG-adaptive — always allowed.
        XCTAssertEqual(PhoneSessionManager.acceptedPreset(2, consentGranted: false), 2)
    }

    func testAcceptedPreset_allowsAllPresetsWithConsent() {
        XCTAssertEqual(PhoneSessionManager.acceptedPreset(0, consentGranted: true), 0)
        XCTAssertEqual(PhoneSessionManager.acceptedPreset(1, consentGranted: true), 1)
        XCTAssertEqual(PhoneSessionManager.acceptedPreset(2, consentGranted: true), 2)
    }
}
