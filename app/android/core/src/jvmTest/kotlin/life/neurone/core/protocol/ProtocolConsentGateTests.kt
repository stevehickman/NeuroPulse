package life.neurone.core.protocol

import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertTrue

/**
 * ISC-90 parity (Android) — the EEG-consent gate that iOS applies in
 * NPProtocolLibrary.availability(for:). Verifies the pure-JVM decision:
 * EEG-dependent protocols are blocked until BIPA consent is granted; non-EEG
 * protocols are unaffected; composites resolve their members. Mirrors the iOS
 * EEGConsentGateTests one-for-one.
 */
class ProtocolConsentGateTests {

    // MARK: - Builders

    private fun eegProtocol() = NPProtocolEntry.Single(
        NPProtocolDefinition(
            name = "Test EEG Neurofeedback", hardwareVersion = 1,
            modalities = listOf(
                NPProtocolModality(
                    params = NPModalityParams.EegNeurofeedback(
                        NPEEGNeurofeedbackParams(closedLoopEnabled = true),
                    ),
                ),
            ),
        ),
    )

    private fun pbmProtocol() = NPProtocolEntry.Single(
        NPProtocolDefinition(
            name = "Test PBM Vascular", hardwareVersion = 1,
            modalities = listOf(
                NPProtocolModality(
                    params = NPModalityParams.PbmTranscranial(NPPBMTranscranialParams()),
                ),
            ),
        ),
    )

    private fun eegAdaptiveAudioProtocol() = NPProtocolEntry.Single(
        NPProtocolDefinition(
            name = "Test EEG-Adaptive Audio", hardwareVersion = 1,
            modalities = listOf(
                NPProtocolModality(
                    params = NPModalityParams.AudioEntrainment(
                        NPAudioEntrainmentParams(eegAdaptive = true),
                    ),
                ),
            ),
        ),
    )

    // MARK: - isEEGDependent predicate

    // @verifies REQ-ONBOARD-05#a
    @Test
    fun isEEGDependent_trueForEEGModality() {
        assertTrue(ProtocolConsentGate.isEEGDependent(eegProtocol()))
    }

    // @verifies REQ-ONBOARD-05#a
    @Test
    fun isEEGDependent_trueForEEGAdaptiveAudio() {
        assertTrue(
            ProtocolConsentGate.isEEGDependent(eegAdaptiveAudioProtocol()),
            "audio with eegAdaptive == true reads brainwave data to close the loop",
        )
    }

    // @verifies REQ-ONBOARD-05#b
    @Test
    fun isEEGDependent_falseForNonEEG() {
        assertFalse(ProtocolConsentGate.isEEGDependent(pbmProtocol()))
    }

    // @verifies REQ-ONBOARD-05
    @Test
    fun isEEGDependent_falseWhenEEGModalityDisabled() {
        val disabled = NPProtocolEntry.Single(
            NPProtocolDefinition(
                name = "Disabled EEG", hardwareVersion = 1,
                modalities = listOf(
                    NPProtocolModality(
                        params = NPModalityParams.EegNeurofeedback(NPEEGNeurofeedbackParams()),
                        enabled = false,
                    ),
                ),
            ),
        )
        assertFalse(ProtocolConsentGate.isEEGDependent(disabled))
    }

    // MARK: - isBlockedByBipa gate

    // @verifies REQ-ONBOARD-05#a
    @Test
    fun eegProtocol_blockedWhenConsentDeclined() {
        assertTrue(ProtocolConsentGate.isBlockedByBipa(eegProtocol(), consentGranted = false))
    }

    // @verifies REQ-ONBOARD-05#c
    @Test
    fun eegProtocol_allowedWhenConsentGranted() {
        assertFalse(ProtocolConsentGate.isBlockedByBipa(eegProtocol(), consentGranted = true))
    }

    // @verifies REQ-ONBOARD-05#b
    @Test
    fun nonEEGProtocol_unaffectedByConsentState() {
        assertFalse(ProtocolConsentGate.isBlockedByBipa(pbmProtocol(), consentGranted = false))
        assertFalse(ProtocolConsentGate.isBlockedByBipa(pbmProtocol(), consentGranted = true))
    }

    // MARK: - Composite resolution

    // @verifies REQ-ONBOARD-05#a
    @Test
    fun composite_blockedWhenAnyMemberEEGDependent() {
        val composite = NPProtocolEntry.Composite(
            NPCompositeProtocol(
                name = "Test Composite", hardwareVersion = 1,
                layers = listOf(
                    NPCompositeLayer(protocolName = "Test PBM Vascular"),
                    NPCompositeLayer(protocolName = "Test EEG Neurofeedback"),
                ),
            ),
        )
        val resolve: (String) -> NPProtocolDefinition? = { name ->
            when (name) {
                "Test EEG Neurofeedback" -> (eegProtocol() as NPProtocolEntry.Single).protocol
                "Test PBM Vascular" -> (pbmProtocol() as NPProtocolEntry.Single).protocol
                else -> null
            }
        }
        assertTrue(ProtocolConsentGate.isEEGDependent(composite, resolve))
        assertTrue(ProtocolConsentGate.isBlockedByBipa(composite, consentGranted = false, resolveSingle = resolve))
        assertFalse(ProtocolConsentGate.isBlockedByBipa(composite, consentGranted = true, resolveSingle = resolve))
    }

    // MARK: - Consent key + message resource parity with iOS

    // @verifies REQ-ONBOARD-05
    @Test
    fun messageResourceName_matchesIOSLocalizationKey() {
        // iOS uses SESSION_EEG_UNAVAILABLE_BODY; Android resolves this res name.
        assertEquals("session_eeg_unavailable_body", ProtocolConsentGate.EEG_UNAVAILABLE_MESSAGE_RES)
    }
}
