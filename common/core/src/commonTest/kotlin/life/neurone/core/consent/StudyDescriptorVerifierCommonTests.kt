@file:OptIn(ExperimentalEncodingApi::class)

package life.neurone.core.consent

import kotlin.io.encoding.Base64
import kotlin.io.encoding.ExperimentalEncodingApi
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNull
import life.neurone.core.models.ResearchCategory
import life.neurone.core.models.StudyDescriptor
import life.neurone.core.models.StudyDescriptorVerification
import life.neurone.core.models.UHDRElement
import life.neurone.core.platform.Ed25519

/**
 * `Ed25519StudyDescriptorVerifier` on every target that runs common tests: the browser (Node) and, on macOS CI, iOS
 * (`OI-UI-KMP-04`). The JVM suite (`StudyDescriptorSigningTests`) signs with the JDK; these sign with the core's own
 * Ed25519, the only signer those targets have, so the verifier is exercised end to end where it once could not check.
 * `RefusingStudyDescriptorVerifier` stays the default: nothing here wires a key.
 */
class StudyDescriptorVerifierCommonTests {

    private val seed = ByteArray(32) { (it + 1).toByte() }
    private val publicKey = Ed25519.publicKey(seed)

    private fun descriptor() = StudyDescriptor(
        studyId = "NP-STUDY-2026-014",
        studyTitle = "Sleep onset latency under 810 nm PBM",
        researchCategories = listOf(ResearchCategory.SLEEP, ResearchCategory.ATTENTION),
        requestedElements = setOf(UHDRElement.SESSION_TIMESTAMPS, UHDRElement.EEG_WAVEFORMS),
        kAnonymity = 10,
        dateRoundingDays = 7,
        issuedOnDay = "2026-09-14",
        signature = "",
    )

    private fun sign(d: StudyDescriptor, signer: ByteArray = seed): StudyDescriptor =
        d.copy(signature = Base64.encode(Ed25519.sign(signer, StudyDescriptorCanonicalForm.bytes(d))))

    // @verifies REQ-CONSENT-10
    @Test
    fun aGenuinelySignedDescriptorVerifiesAndCarriesItsHash() {
        val d = sign(descriptor())
        assertEquals(
            StudyDescriptorVerification.Verified(StudyDescriptorCanonicalForm.descriptorHash(d)),
            Ed25519StudyDescriptorVerifier(publicKey).verify(d),
        )
    }

    // @verifies REQ-CONSENT-10#a
    @Test
    fun aChangedFieldIsRejected() {
        val signed = sign(descriptor())
        assertEquals(
            StudyDescriptorVerification.Rejected,
            Ed25519StudyDescriptorVerifier(publicKey).verify(signed.copy(kAnonymity = 2)),
        )
    }

    // @verifies REQ-CONSENT-10
    @Test
    fun aDescriptorSignedByAnotherKeyIsRejected() {
        val other = sign(descriptor(), ByteArray(32) { 0x55 })
        assertEquals(StudyDescriptorVerification.Rejected, Ed25519StudyDescriptorVerifier(publicKey).verify(other))
    }

    // @verifies REQ-CONSENT-10
    @Test
    fun aMalformedSignatureIsRejectedNotUnavailable() {
        val verifier = Ed25519StudyDescriptorVerifier(publicKey)
        assertEquals(StudyDescriptorVerification.Rejected, verifier.verify(descriptor().copy(signature = "!!not base64!!")))
        assertEquals(StudyDescriptorVerification.Rejected, verifier.verify(descriptor().copy(signature = Base64.encode(ByteArray(10)))))
    }

    // @verifies REQ-CONSENT-10
    @Test
    fun anAbsentKeyYieldsNoVerifier() {
        assertNull(Ed25519StudyDescriptorVerifier.fromBase64(null))
    }
}
