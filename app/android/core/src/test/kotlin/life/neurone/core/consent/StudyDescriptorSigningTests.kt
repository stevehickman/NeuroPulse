package life.neurone.core.consent

import life.neurone.core.models.ResearchCategory
import life.neurone.core.models.StudyDescriptor
import life.neurone.core.models.StudyDescriptorVerification
import life.neurone.core.models.UHDRElement
import java.security.KeyPairGenerator
import java.security.Signature
import java.util.Base64
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNotEquals
import kotlin.test.assertNull
import kotlin.test.assertTrue

/**
 * NP-SW-PORTAL-API-001 §4a. The canonical form is the one schema decision the spec takes, so it
 * is pinned here by its bytes rather than described.
 */
class StudyDescriptorSigningTests {

    private val keyPair = KeyPairGenerator.getInstance("Ed25519").generateKeyPair()

    /** The raw 32 bytes, extracted from the JDK's SPKI encoding the way a key holder would. */
    private val rawPublicKey: ByteArray =
        keyPair.public.encoded.copyOfRange(keyPair.public.encoded.size - 32, keyPair.public.encoded.size)

    private fun descriptor(
        studyId: String = "NP-STUDY-2026-014",
        studyTitle: String = "Sleep onset latency under 810 nm PBM",
        categories: List<ResearchCategory> = listOf(ResearchCategory.SLEEP, ResearchCategory.ATTENTION),
        elements: Set<UHDRElement> = setOf(
            UHDRElement.SESSION_TIMESTAMPS, UHDRElement.EEG_WAVEFORMS, UHDRElement.PBM_DOSE_LOGS,
        ),
        k: Int = 10,
        rounding: Int = 7,
        day: String = "2026-09-14",
        signature: String = "",
    ) = StudyDescriptor(
        studyId = studyId,
        studyTitle = studyTitle,
        researchCategories = categories,
        requestedElements = elements,
        kAnonymity = k,
        dateRoundingDays = rounding,
        issuedOnDay = day,
        signature = signature,
    )

    private fun sign(d: StudyDescriptor): StudyDescriptor {
        val sig = Signature.getInstance("Ed25519").run {
            initSign(keyPair.private)
            update(StudyDescriptorCanonicalForm.bytes(d))
            sign()
        }
        return d.copy(signature = Base64.getEncoder().encodeToString(sig))
    }

    // ── The canonical form itself ────────────────────────────────────────────

    /**
     * The exact bytes, spelled out. This is the contract iOS's
     * `StudyDescriptorCanonicalForm.bytes(for:)` has to reproduce character for character, and
     * describing it in prose on both sides is how two implementations drift.
     */
    @Test
    fun theCanonicalFormIsExactlyThis() {
        val bytes = StudyDescriptorCanonicalForm.bytes(
            descriptor(
                studyId = "S1",
                studyTitle = "Trial",
                categories = listOf(ResearchCategory.SLEEP),
                elements = setOf(UHDRElement.SESSION_DURATION, UHDRElement.EEG_WAVEFORMS),
                k = 10,
                rounding = 7,
                day = "2026-09-14",
            ),
        )
        assertEquals(
            "NP-STUDY-DESCRIPTOR-V1\n" +
                "2:S1\n" +
                "5:Trial\n" +
                "15:Sleep Disorders\n" +
                "30:EEG Waveforms,Session Duration\n" +
                "2:10\n" +
                "1:7\n" +
                "10:2026-09-14\n",
            bytes.decodeToString(),
        )
    }

    /** The signature must not depend on which order a Set happened to iterate in. */
    @Test
    fun elementOrderDoesNotChangeTheBytes() {
        val a = descriptor(elements = linkedSetOf(UHDRElement.EEG_WAVEFORMS, UHDRElement.OUTCOME_LOGS))
        val b = descriptor(elements = linkedSetOf(UHDRElement.OUTCOME_LOGS, UHDRElement.EEG_WAVEFORMS))
        assertEquals(
            StudyDescriptorCanonicalForm.bytes(a).decodeToString(),
            StudyDescriptorCanonicalForm.bytes(b).decodeToString(),
        )
    }

    /**
     * The reason fields are length-prefixed rather than delimited: `studyTitle` is server-supplied
     * text, so it can contain the separator. Two different descriptors must never produce the same
     * bytes, or one signature covers both.
     */
    @Test
    fun aTitleCarryingTheSeparatorCannotForgeAnotherDescriptor() {
        val honest = descriptor(studyId = "S1", studyTitle = "Trial")
        val smuggled = descriptor(studyId = "S1", studyTitle = "Trial\n5:Other")
        assertNotEquals(
            StudyDescriptorCanonicalForm.bytes(honest).decodeToString(),
            StudyDescriptorCanonicalForm.bytes(smuggled).decodeToString(),
        )
    }

    @Test
    fun theHashDigestsTheSignedBytes() {
        val d = descriptor()
        val hash = StudyDescriptorCanonicalForm.descriptorHash(d)
        assertTrue(hash.startsWith("sha256:"))
        assertEquals(71, hash.length)  // "sha256:" + 64 hex
        // The signature field is not part of what is signed, so it cannot move the hash.
        assertEquals(hash, StudyDescriptorCanonicalForm.descriptorHash(d.copy(signature = "zzzz")))
    }

    // ── The verifier ─────────────────────────────────────────────────────────

    @Test
    fun aGenuinelySignedDescriptorVerifiesAndCarriesItsHash() {
        val d = sign(descriptor())
        val result = Ed25519StudyDescriptorVerifier(rawPublicKey).verify(d)
        assertEquals(
            StudyDescriptorVerification.Verified(StudyDescriptorCanonicalForm.descriptorHash(d)),
            result,
        )
    }

    /** Every field is covered: tampering with any of them must break the signature. */
    @Test
    fun tamperingWithAnyCoveredFieldRejects()  {
        val signed = sign(descriptor())
        val verifier = Ed25519StudyDescriptorVerifier(rawPublicKey)
        val tampered = listOf(
            signed.copy(studyId = "NP-STUDY-2026-015"),
            signed.copy(studyTitle = "Something else"),
            signed.copy(researchCategories = listOf(ResearchCategory.DEPRESSION)),
            signed.copy(requestedElements = signed.requestedElements + UHDRElement.HRV_TIME_SERIES),
            signed.copy(kAnonymity = 1),
            signed.copy(dateRoundingDays = 0),
            signed.copy(issuedOnDay = "2026-09-15"),
        )
        for (d in tampered) {
            assertEquals(
                StudyDescriptorVerification.Rejected,
                verifier.verify(d),
                "tampering must reject: $d",
            )
        }
    }

    /**
     * Lowering k below §5.3's floor is the case the ingestion gate's ORDER exists for. The
     * signature catches it first, so the descriptor is refused as forged rather than as
     * out-of-range — nothing an unverified descriptor claims influences a later step.
     */
    @Test
    fun aDescriptorSignedByAnotherKeyIsRejected() {
        val other = KeyPairGenerator.getInstance("Ed25519").generateKeyPair()
        val d = descriptor()
        val sig = Signature.getInstance("Ed25519").run {
            initSign(other.private)
            update(StudyDescriptorCanonicalForm.bytes(d))
            sign()
        }
        assertEquals(
            StudyDescriptorVerification.Rejected,
            Ed25519StudyDescriptorVerifier(rawPublicKey)
                .verify(d.copy(signature = Base64.getEncoder().encodeToString(sig))),
        )
    }

    /**
     * A malformed signature is Rejected, never Unavailable. Unavailable means the device cannot
     * check; a device holding a key can, and collapsing the two would let a forged descriptor read
     * as a configuration problem.
     */
    @Test
    fun aMalformedSignatureIsRejectedNotUnavailable() {
        val verifier = Ed25519StudyDescriptorVerifier(rawPublicKey)
        assertEquals(StudyDescriptorVerification.Rejected, verifier.verify(descriptor(signature = "")))
        assertEquals(
            StudyDescriptorVerification.Rejected,
            verifier.verify(descriptor(signature = "not base64!!")),
        )
        assertEquals(
            StudyDescriptorVerification.Rejected,
            verifier.verify(descriptor(signature = Base64.getEncoder().encodeToString(ByteArray(8)))),
        )
    }

    /** A build with no key, or a broken one, falls back to refusing rather than to a thrower. */
    @Test
    fun anAbsentOrUnusableKeyYieldsNoVerifierAtAll() {
        assertNull(Ed25519StudyDescriptorVerifier.fromBase64(null))
        assertNull(Ed25519StudyDescriptorVerifier.fromBase64(""))
        assertNull(Ed25519StudyDescriptorVerifier.fromBase64("   "))
        assertNull(Ed25519StudyDescriptorVerifier.fromBase64("not base64!!"))
        // Right encoding, wrong length.
        assertNull(Ed25519StudyDescriptorVerifier.fromBase64(Base64.getEncoder().encodeToString(ByteArray(16))))
        assertTrue(
            Ed25519StudyDescriptorVerifier.fromBase64(
                Base64.getEncoder().encodeToString(rawPublicKey),
            ) != null,
        )
    }

    /**
     * The wire names are the identity a signature covers, so they are pinned here. Changing one
     * invalidates every descriptor already issued, which should be a decision and not a rename.
     * They match iOS `UHDRElement.rawValue` / `ResearchCategory.rawValue` exactly.
     */
    @Test
    fun wireNamesMatchTheIosRawValues() {
        assertEquals(
            listOf(
                "Closed-Loop Adaptation Events", "EEG Waveforms", "Eye Open/Closed State",
                "HRV Time Series", "Neurofeedback Performance Scores", "PBM Dose (J/cm²) Per Zone",
                "PPG Optical Signal", "Protocol Parameters", "Session Duration",
                "Session Timestamps", "User-Entered Outcome Logs",
            ),
            UHDRElement.entries.map { it.wireName }.sorted(),
        )
        assertEquals(
            listOf(
                "Alzheimer's / Dementia", "Attention / ADHD", "Depression", "Healthy Ageing",
                "PTSD", "Parkinson's Disease", "Sleep Disorders", "Traumatic Brain Injury",
                "Visual Health",
            ),
            ResearchCategory.entries.map { it.wireName }.sorted(),
        )
    }
}
