import CryptoKit
import Foundation

// What a study descriptor's signature covers, and the verifier that checks it —
// CLAUDE.md §5.3, NP-SW-PORTAL-API-001 §4a. Peer of Android
// `core/.../consent/StudyDescriptorSigning.kt`; the two encodings must agree byte for byte,
// because one signature is checked on either platform.

/// The bytes a study descriptor's signature covers.
///
/// §5.3 locks that descriptors are *cryptographically signed*, which is not implementable until
/// something says **what** is signed. This is that something, and it is the one schema decision
/// `NP-SW-PORTAL-API-001` takes: a verifier cannot exist without it, and two platforms cannot agree
/// without it being written down rather than inferred from whichever serializer each happens to use.
///
/// Three properties are load-bearing.
///
/// **Length-prefixed, not delimited.** `studyTitle` is server-supplied text, so it can contain any
/// byte including the separator. Joining fields with a delimiter would let a title carrying that
/// delimiter shift the field boundaries and produce the same bytes as a different descriptor — a
/// signature over an ambiguous encoding signs more than one message.
///
/// **Sets are sorted, lists are not re-ordered.** `requestedElements` is a `Set` with no inherent
/// order, so the byte sequence has to impose one. `researchCategories` is an array and keeps the
/// order the issuer sent.
///
/// **Dates are UTC days, not instants.** Android models this field as `"yyyy-MM-dd"` and iOS as a
/// `Date`, so the coarser of the two is the only representation both can produce. The consequence
/// is worth stating rather than discovering: **time of day is not covered by the signature**, so an
/// issuer must not rely on it to distinguish two descriptors — `studyID` is what does that.
enum StudyDescriptorCanonicalForm {

    static let version = "NP-STUDY-DESCRIPTOR-V1"

    /// `issuedAt` rendered the way Android already stores it. Fixed to UTC and to the POSIX
    /// calendar: a device's locale must not change what a signature is checked against.
    static func day(from date: Date) -> String {
        var calendar = Calendar(identifier: .gregorian)
        calendar.timeZone = TimeZone(secondsFromGMT: 0)!
        let c = calendar.dateComponents([.year, .month, .day], from: date)
        return String(format: "%04d-%02d-%02d", c.year!, c.month!, c.day!)
    }

    /// The exact bytes signed, and the bytes `descriptorHash` digests.
    static func bytes(for descriptor: StudyDescriptor) -> Data {
        var out = version + "\n"
        func field(_ value: String) {
            out += "\(value.utf8.count):\(value)\n"
        }
        field(descriptor.studyID)
        field(descriptor.studyTitle)
        field(descriptor.researchCategories.map(\.rawValue).joined(separator: ","))
        field(descriptor.requestedElements.map(\.rawValue).sorted().joined(separator: ","))
        field(String(descriptor.kAnonymity))
        field(String(descriptor.dateRoundingDays))
        field(day(from: descriptor.issuedAt))
        return Data(out.utf8)
    }

    /// SHA-256 of `bytes(for:)`, as `"sha256:<lowercase hex>"` — the §5.3 audit-trail hash.
    ///
    /// It digests the *signed* bytes rather than the parsed object, so the hash in the audit trail
    /// identifies exactly what was verified. Anything else would record a hash of the device's
    /// reading of a descriptor instead of the descriptor.
    static func descriptorHash(for descriptor: StudyDescriptor) -> String {
        let digest = SHA256.hash(data: bytes(for: descriptor))
        return "sha256:" + digest.map { String(format: "%02x", $0) }.joined()
    }
}

/// The real verifier: Ed25519 over `StudyDescriptorCanonicalForm`.
///
/// **It is not the default, and `OI-CONSENT-07` is not closed by its existing.** It cannot be
/// constructed without a public key, and there is no key to construct it with:
/// `NP-SW-PORTAL-API-001` §4(a) requires the verifying key to ship *with the app build*, pinned,
/// because a key fetched over the channel it authenticates proves only that one party sent both.
/// So `RefusingStudyDescriptorVerifier` remains what `ConsentStore` defaults to, and this is what a
/// build with a key configured would inject.
///
/// What it buys before that key exists: the canonical form has an executable definition rather than
/// a prose one, the iOS and Android encodings are pinned against each other by test, and whoever
/// supplies the key is supplying only a key.
struct Ed25519StudyDescriptorVerifier: StudyDescriptorVerifier {

    private let publicKey: Curve25519.Signing.PublicKey

    init(publicKeyRaw: Data) throws {
        publicKey = try Curve25519.Signing.PublicKey(rawRepresentation: publicKeyRaw)
    }

    /// Returns nil for a key that is absent or unusable, so a misconfigured build falls back to
    /// refusing rather than to a verifier that fails at the first descriptor. A key that cannot be
    /// parsed is the same situation as no key at all, and §5.3's floor is that no descriptor is
    /// admitted in either.
    init?(publicKeyBase64: String?) {
        guard let publicKeyBase64, !publicKeyBase64.isEmpty,
              let raw = Data(base64Encoded: publicKeyBase64),
              let key = try? Curve25519.Signing.PublicKey(rawRepresentation: raw)
        else { return nil }
        publicKey = key
    }

    func verify(_ descriptor: StudyDescriptor) -> StudyDescriptorVerification {
        // A malformed signature is `.rejected`, never `.unavailable`: `.unavailable` means the
        // device cannot check, and a device holding a key can. Collapsing them would let a forged
        // descriptor read as a configuration problem.
        guard let signature = Data(base64Encoded: descriptor.signature) else { return .rejected }

        guard publicKey.isValidSignature(
            signature,
            for: StudyDescriptorCanonicalForm.bytes(for: descriptor)
        ) else { return .rejected }

        return .verified(descriptorHash: StudyDescriptorCanonicalForm.descriptorHash(for: descriptor))
    }
}
