package life.neurone.core.consent

import life.neurone.core.models.StudyDescriptor
import life.neurone.core.models.StudyDescriptorVerification
import life.neurone.core.models.StudyDescriptorVerifier
import java.security.KeyFactory
import java.security.MessageDigest
import java.security.Signature
import java.security.spec.X509EncodedKeySpec
import java.util.Base64

/**
 * The bytes a study descriptor's signature covers — NP-SW-PORTAL-API-001 §4a.
 *
 * §5.3 locks that descriptors are *cryptographically signed*, which is not implementable until
 * something says **what** is signed. This is that something, and it is the one schema decision the
 * spec takes: a verifier cannot exist without it, and two platforms cannot agree without it being
 * written down rather than inferred from whichever serializer each happens to use.
 *
 * Three properties are load-bearing.
 *
 * **Length-prefixed, not delimited.** `studyTitle` is server-supplied text (the one field that
 * stays text), so it can contain any byte including the separator. Joining fields with a delimiter
 * would let a title carrying that delimiter shift the field boundaries and produce the same bytes
 * as a different descriptor — a signature over an ambiguous encoding signs more than one message.
 *
 * **Sets are sorted, lists are not re-ordered.** `requestedElements` is a set with no inherent
 * order, so the byte sequence has to impose one or the same descriptor signs differently depending
 * on iteration order. `researchCategories` is a list and keeps its order, because that is what the
 * issuer sent.
 *
 * **Dates are UTC days, not instants.** iOS models `issuedAt` as a `Date` and Android models
 * `issuedOnDay` as `"yyyy-MM-dd"`, so the coarser of the two is the only representation both can
 * produce. The consequence is worth stating rather than discovering: **time of day is not covered
 * by the signature**, so an issuer must not rely on it to distinguish two descriptors — `studyId`
 * is what does that.
 *
 * Version prefix first, so a later form can exist without a v1 signature accidentally verifying
 * against it.
 */
object StudyDescriptorCanonicalForm {

    const val VERSION = "NP-STUDY-DESCRIPTOR-V1"

    /** The exact bytes signed, and the bytes [descriptorHash] digests. */
    fun bytes(descriptor: StudyDescriptor): ByteArray {
        val out = StringBuilder()
        fun field(value: String) {
            val encoded = value.toByteArray(Charsets.UTF_8)
            out.append(encoded.size).append(':').append(value).append('\n')
        }
        out.append(VERSION).append('\n')
        field(descriptor.studyId)
        field(descriptor.studyTitle)
        field(descriptor.researchCategories.joinToString(",") { it.wireName })
        field(descriptor.requestedElements.map { it.wireName }.sorted().joinToString(","))
        field(descriptor.kAnonymity.toString())
        field(descriptor.dateRoundingDays.toString())
        field(descriptor.issuedOnDay)
        return out.toString().toByteArray(Charsets.UTF_8)
    }

    /**
     * SHA-256 of [bytes], as `"sha256:<lowercase hex>"` — the §5.3 audit-trail hash.
     *
     * It digests the *signed* bytes rather than the parsed object, so the hash in the audit trail
     * identifies exactly what was verified. Anything else would record a hash of the device's
     * reading of a descriptor instead of the descriptor.
     */
    fun descriptorHash(descriptor: StudyDescriptor): String {
        val digest = MessageDigest.getInstance("SHA-256").digest(bytes(descriptor))
        return "sha256:" + digest.joinToString("") { "%02x".format(it) }
    }
}

/**
 * The real verifier: Ed25519 over [StudyDescriptorCanonicalForm].
 *
 * **It is not the default, and `OI-CONSENT-07` is not closed by its existing.** It cannot be
 * constructed without a public key, and there is no key to construct it with: NP-SW-PORTAL-API-001
 * §4(a) requires the verifying key to ship *with the app build*, pinned, because a key fetched over
 * the channel it authenticates proves only that one party sent both. So
 * [life.neurone.core.models.RefusingStudyDescriptorVerifier] remains what `ConsentStore` defaults
 * to, and this class is what a build with a key configured would inject.
 *
 * What it buys before that key exists: the canonical form has an executable definition rather than
 * a prose one, the iOS and Android encodings are pinned against each other by test, and whoever
 * supplies the key is supplying only a key.
 *
 * Ed25519 rather than ECDSA: deterministic signatures (no per-signature entropy to get wrong on the
 * signing side), small keys, and available on the JDK since 15 — `:core` is pure-JVM by design and
 * may not reach for an Android provider.
 */
class Ed25519StudyDescriptorVerifier(
    /**
     * The raw 32-byte Ed25519 public key — the same bytes iOS passes to CryptoKit's
     * `rawRepresentation`. **One key format across both platforms**, deliberately: CryptoKit takes
     * only the raw point and the JDK takes only SubjectPublicKeyInfo, so if each side accepted what
     * its own library wanted, "the NeurOne study-signing key" would be two different artifacts and
     * a build could ship the wrong one to one platform. The DER wrapper is a fixed 12-byte prefix
     * for this algorithm, so the conversion belongs here rather than in whoever holds the key.
     */
    publicKeyRaw: ByteArray,
) : StudyDescriptorVerifier {

    private val publicKey = KeyFactory.getInstance("Ed25519")
        .generatePublic(X509EncodedKeySpec(ED25519_SPKI_PREFIX + publicKeyRaw))

    companion object {
        /**
         * SubjectPublicKeyInfo header for Ed25519 (RFC 8410 §4): SEQUENCE, AlgorithmIdentifier
         * with OID 1.3.101.112 and absent parameters, then a 33-byte BIT STRING with zero unused
         * bits. Fixed for the algorithm, so a raw key is a complete SPKI once prefixed.
         */
        private val ED25519_SPKI_PREFIX = byteArrayOf(
            0x30, 0x2a, 0x30, 0x05, 0x06, 0x03, 0x2b, 0x65, 0x70, 0x03, 0x21, 0x00,
        )

        /** Raw public key length for Ed25519. */
        private const val RAW_KEY_BYTES = 32

        /**
         * Returns null for a key that is absent or unusable, so a misconfigured build falls back to
         * refusing rather than to a verifier that throws at the first descriptor. A key that cannot
         * be parsed is the same situation as no key at all, and §5.3's floor is that no descriptor
         * is admitted in either.
         */
        fun fromBase64(publicKeyRawBase64: String?): Ed25519StudyDescriptorVerifier? {
            if (publicKeyRawBase64.isNullOrBlank()) return null
            return runCatching {
                val raw = Base64.getDecoder().decode(publicKeyRawBase64)
                require(raw.size == RAW_KEY_BYTES)
                Ed25519StudyDescriptorVerifier(raw)
            }.getOrNull()
        }
    }

    override fun verify(descriptor: StudyDescriptor): StudyDescriptorVerification {
        val signature = runCatching { Base64.getDecoder().decode(descriptor.signature) }.getOrNull()
            ?: return StudyDescriptorVerification.Rejected

        // A malformed signature is `Rejected`, never `Unavailable`: `Unavailable` means the device
        // cannot check, and a device holding a key can. Collapsing them would let a forged
        // descriptor read as a configuration problem.
        val ok = runCatching {
            Signature.getInstance("Ed25519").run {
                initVerify(publicKey)
                update(StudyDescriptorCanonicalForm.bytes(descriptor))
                verify(signature)
            }
        }.getOrDefault(false)

        return if (ok) {
            StudyDescriptorVerification.Verified(
                StudyDescriptorCanonicalForm.descriptorHash(descriptor),
            )
        } else {
            StudyDescriptorVerification.Rejected
        }
    }
}
