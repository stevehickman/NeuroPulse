import Foundation
import Combine
import CryptoKit
import Security

// SHDR upload to NeuroPulse fleet database.
// SHDR = System Health Data Record — device condition only, never user biology.
// Upload is gated on:
//   1. Hub notifies SHDR_UPLOAD_STATUS characteristic (0x01 = pending upload)
//   2. Warranty owner has consented to fleet telemetry (WarrantyAnalyticsGate.isOpen)
//   3. USB-C connection detected (hub reports power source)
// Per CLAUDE.md §5.1: SHDR is linked to device ID + opaque warranty token only — never user identity.
// Warranty consent is entirely independent of user research consent (ConsentStore).

enum SHDRUploadError: LocalizedError {
    case noData
    case networkError(Error)
    case serverRejected(Int)

    var errorDescription: String? {
        switch self {
        case .noData:                return "No SHDR data available to upload."
        case .networkError(let e):   return "Network error during SHDR upload: \(e.localizedDescription)"
        case .serverRejected(let c): return "Fleet server rejected SHDR upload (HTTP \(c))."
        }
    }
}

// INFRASTRUCTURE — fleet.neuropulse.internal
//
// Domain   : Internal hostname resolved via NeuroPulse private DNS.
//            No public registrar registration needed; provision in your private DNS zone.
//
// Purpose  : Accepts SHDR (System Health Data Record) binary uploads from the app.
//            SHDR is device-condition telemetry only — never user biology (§5.1 CLAUDE.md).
//
// Endpoint : POST /v1/shdr
//   Headers  : Content-Type: application/octet-stream
//              X-NP-Device-Token: <64-char hex, 256-bit opaque warranty token>
//   Body     : raw SHDR binary blob (read from shdr_staging.bin via hub CDC interface)
//   Response : 200 OK on success; 400 on bad token format; 5xx on server error
//
// TLS/pinning : This endpoint is SPKI-pinned (SHDRFleetPinningDelegate).
//   Pre-launch actions:
//   (1) Provision TLS certificate for fleet.neuropulse.internal.
//   (2) Derive SPKI SHA-256: openssl x509 -in cert.pem -pubkey -noout |
//         openssl pkey -pubin -outform der | openssl dgst -sha256 -binary | base64
//   (3) Replace the placeholder values in SHDRFleetPinningDelegate.pinnedHashes
//       with real hashes (primary key + at least one backup for zero-downtime rotation).
private let fleetEndpoint = URL(string: "https://fleet.neuropulse.internal/v1/shdr")!

// MARK: - SPKI certificate pinning (NP-PRIV-ANALYSIS-002 LOW-11)
//
// Prevents MITM on clinical networks by refusing any TLS handshake where the
// server leaf certificate's public key does not match a pinned SHA-256 hash.
//
// Algorithm: extract the leaf certificate's SubjectPublicKeyInfo (SPKI), compute
// its SHA-256, and compare against the pinned set. SPKI pinning (rather than
// full-certificate pinning) survives certificate renewal as long as the key pair
// is unchanged, avoiding unnecessary pinning failures at cert expiry.
//
// To derive a hash for a new certificate:
//   openssl x509 -in cert.pem -pubkey -noout \
//     | openssl pkey -pubin -outform der \
//     | openssl dgst -sha256 -binary \
//     | base64
//
// Pin at least two hashes (current key + one backup) to allow zero-downtime
// key rotation. Replace the placeholder values below before production deployment.

private final class SHDRFleetPinningDelegate: NSObject, URLSessionDelegate {

    // SHA-256 of the DER-encoded SPKI for fleet.neuropulse.internal.
    // PLACEHOLDER — replace with real hashes before launch. Zero hashes will
    // not match any real certificate, so the pinned session rejects all
    // connections until real hashes are installed.
    static let pinnedHashes: Set<Data> = {
        let placeholders = [
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=", // primary key — replace before launch
            "BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB=", // backup key  — replace before launch
        ]
        return Set(placeholders.compactMap { Data(base64Encoded: $0) })
    }()

    private let host: String
    private let pinnedHashes: Set<Data>

    init(host: String, pinnedHashes: Set<Data> = SHDRFleetPinningDelegate.pinnedHashes) {
        self.host = host
        self.pinnedHashes = pinnedHashes
    }

    func urlSession(
        _ session: URLSession,
        didReceive challenge: URLAuthenticationChallenge,
        completionHandler: @escaping (URLSession.AuthChallengeDisposition, URLCredential?) -> Void
    ) {
        guard
            challenge.protectionSpace.host == host,
            challenge.protectionSpace.authenticationMethod == NSURLAuthenticationMethodServerTrust,
            let serverTrust = challenge.protectionSpace.serverTrust
        else {
            completionHandler(.performDefaultHandling, nil)
            return
        }

        // Evaluate the chain through the OS trust store first.
        var error: CFError?
        guard SecTrustEvaluateWithError(serverTrust, &error) else {
            completionHandler(.cancelAuthenticationChallenge, nil)
            return
        }

        // Then pin the leaf certificate's SPKI.
        guard
            let chain = SecTrustCopyCertificateChain(serverTrust) as? [SecCertificate],
            let leaf = chain.first,
            let spkiHash = spkiSHA256(of: leaf),
            pinnedHashes.contains(spkiHash)
        else {
            completionHandler(.cancelAuthenticationChallenge, nil)
            return
        }

        completionHandler(.useCredential, URLCredential(trust: serverTrust))
    }

    // Compute the SHA-256 of the leaf certificate's SubjectPublicKeyInfo.
    // Returns nil if the key type is unrecognized or the representation is unavailable.
    private func spkiSHA256(of certificate: SecCertificate) -> Data? {
        guard
            let publicKey = SecCertificateCopyKey(certificate),
            let rawKey = SecKeyCopyExternalRepresentation(publicKey, nil) as Data?,
            let header = spkiHeader(for: publicKey)
        else { return nil }

        let spki = Data(header) + rawKey
        return Data(SHA256.hash(data: spki))
    }

    // DER SPKI header bytes that precede the raw key from SecKeyCopyExternalRepresentation.
    // For RSA: raw key is PKCS#1 DER (SEQUENCE { n, e }); header encodes SEQUENCE+AlgID+BIT STRING.
    // For EC:  raw key is uncompressed point (04 || X || Y); header encodes SEQUENCE+AlgID+BIT STRING.
    // Headers match TrustKit reference implementation.
    private func spkiHeader(for key: SecKey) -> [UInt8]? {
        guard
            let attrs = SecKeyCopyAttributes(key) as? [CFString: Any],
            let keyType = attrs[kSecAttrKeyType] as? String,
            let keySize = attrs[kSecAttrKeySizeInBits] as? Int
        else { return nil }

        let rsa  = kSecAttrKeyTypeRSA          as String
        let ec   = kSecAttrKeyTypeECSECPrimeRandom as String

        switch (keyType, keySize) {
        case (rsa, 2048):
            return [
                0x30, 0x82, 0x01, 0x22, 0x30, 0x0d, 0x06, 0x09,
                0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01,
                0x01, 0x05, 0x00, 0x03, 0x82, 0x01, 0x0f, 0x00,
            ]
        case (rsa, 4096):
            return [
                0x30, 0x82, 0x02, 0x22, 0x30, 0x0d, 0x06, 0x09,
                0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01,
                0x01, 0x05, 0x00, 0x03, 0x82, 0x02, 0x0f, 0x00,
            ]
        case (ec, 256):
            return [
                0x30, 0x59, 0x30, 0x13, 0x06, 0x07, 0x2a, 0x86,
                0x48, 0xce, 0x3d, 0x02, 0x01, 0x06, 0x08, 0x2a,
                0x86, 0x48, 0xce, 0x3d, 0x03, 0x01, 0x07, 0x03,
                0x42, 0x00,
            ]
        case (ec, 384):
            return [
                0x30, 0x76, 0x30, 0x10, 0x06, 0x07, 0x2a, 0x86,
                0x48, 0xce, 0x3d, 0x02, 0x01, 0x06, 0x05, 0x2b,
                0x81, 0x04, 0x00, 0x22, 0x03, 0x62, 0x00,
            ]
        default:
            return nil
        }
    }
}

// MARK: - Uploader

@MainActor
final class SHDRUploader: ObservableObject {

    @Published private(set) var isUploading = false
    @Published private(set) var lastUploadedAt: Date?
    @Published private(set) var lastError: SHDRUploadError?

    private let uploadTrigger: SHDRUploadTriggering
    private var cancellable: AnyCancellable?

    // Dedicated session with SPKI pinning — never use URLSession.shared for fleet uploads.
    private let session: URLSession

    init(gatt: SHDRUploadTriggering) {
        self.uploadTrigger = gatt
        let delegate = SHDRFleetPinningDelegate(host: "fleet.neuropulse.internal")
        self.session = URLSession(configuration: .ephemeral, delegate: delegate, delegateQueue: nil)
        observeUploadTrigger()
    }

    private func observeUploadTrigger() {
        cancellable = uploadTrigger.shdrUploadPendingPublisher
            .filter { $0 }
            .sink { [weak self] _ in
                Task { await self?.uploadIfConsented() }
            }
    }

    func uploadIfConsented() async {
        guard WarrantyAnalyticsGate.isOpen else { return }
        guard !isUploading else { return }
        do {
            try await upload()
        } catch {
            // Non-fatal — retry on next USB-C connection.
        }
    }

    func upload() async throws {
        isUploading = true
        lastError = nil
        defer { isUploading = false }

        // NP-FW-EMMC-002 Rev A §A: SHDR is linked to an opaque 256-bit TRNG warranty
        // token — never to any vendor-assigned or user-linked identifier.
        let warrantyToken = warrantyTokenFromKeychain()

        // In production the hub pushes the SHDR binary blob over USB-C bulk transfer;
        // the app reads it from a staging file dropped by the hub's CDC interface.
        let stagingURL = FileManager.default.urls(for: .documentDirectory, in: .userDomainMask)[0]
            .appendingPathComponent("shdr_staging.bin")
        // Exclude staging file from iCloud / iTunes backup — it is transient device
        // telemetry, not user data that needs to survive restores.
        try? (stagingURL as NSURL).setResourceValue(true, forKey: .isExcludedFromBackupKey)

        guard let shdrData = try? Data(contentsOf: stagingURL) else {
            throw SHDRUploadError.noData
        }

        var request = URLRequest(url: fleetEndpoint)
        request.httpMethod = "POST"
        request.setValue("application/octet-stream", forHTTPHeaderField: "Content-Type")
        request.setValue(warrantyToken, forHTTPHeaderField: "X-NP-Device-Token")
        request.httpBody = shdrData

        let (_, response): (Data, URLResponse)
        do {
            (_, response) = try await session.data(for: request)
        } catch {
            let err = SHDRUploadError.networkError(error)
            lastError = err
            throw err
        }

        if let http = response as? HTTPURLResponse, http.statusCode != 200 {
            let err = SHDRUploadError.serverRejected(http.statusCode)
            lastError = err
            throw err
        }

        try? FileManager.default.removeItem(at: stagingURL)
        lastUploadedAt = Date()
    }

    // MARK: - Warranty token (NP-FW-EMMC-002 Rev A §A)

    // Opaque 256-bit random token, generated once at first run and stored in the
    // Keychain with ThisDeviceOnly accessibility. Used as the SHDR fleet DB linkage
    // key — it is never joined to user identity.
    // Interim Keychain-generated token. Will be replaced by the 256-bit TRNG token
    // from the hub's GATT warrantyToken characteristic when hub firmware ships (OI-BLE-01).
    private static let warrantyTokenTag = "com.neuropulse.shdr.warranty-token"

    private func warrantyTokenFromKeychain() -> String {
        // kSecAttrAccount is part of the Keychain primary key for kSecClassGenericPassword
        // alongside kSecAttrService. Both must be present in read and write queries to
        // avoid ambiguous matches if another item ever shares the service string.
        // kSecAttrAccessible is included so the read query only matches an item with the
        // expected accessibility class — prevents a different-accessibility item from being
        // returned if one were ever written under the same service/account key pair.
        // This mirrors the write query below (same pattern as LOW-12 fix in SessionProtocolSigner).
        let query: [CFString: Any] = [
            kSecClass:              kSecClassGenericPassword,
            kSecAttrService:        Self.warrantyTokenTag,
            kSecAttrAccount:        "warranty-token",
            kSecAttrAccessible:     kSecAttrAccessibleAfterFirstUnlockThisDeviceOnly,
            kSecMatchLimit:         kSecMatchLimitOne,
            kSecReturnData:         true,
            kSecAttrSynchronizable: false
        ]
        var item: CFTypeRef?
        if SecItemCopyMatching(query as CFDictionary, &item) == errSecSuccess,
           let tokenData = item as? Data {
            return tokenData.map { String(format: "%02x", $0) }.joined()
        }

        // First run: generate a 32-byte random token.
        var bytes = [UInt8](repeating: 0, count: 32)
        guard SecRandomCopyBytes(kSecRandomDefault, bytes.count, &bytes) == errSecSuccess else {
            // SecRandomCopyBytes failure is extremely unlikely. Fall back to a
            // non-identifiable zero token rather than a linkable identifier.
            return String(repeating: "0", count: 64)
        }
        let tokenData = Data(bytes)
        let addQuery: [CFString: Any] = [
            kSecClass:              kSecClassGenericPassword,
            kSecAttrService:        Self.warrantyTokenTag,
            kSecAttrAccount:        "warranty-token",
            kSecValueData:          tokenData,
            kSecAttrAccessible:     kSecAttrAccessibleAfterFirstUnlockThisDeviceOnly,
            kSecAttrSynchronizable: false
        ]
        let status = SecItemAdd(addQuery as CFDictionary, nil)
        if status == errSecSuccess {
            return tokenData.map { String(format: "%02x", $0) }.joined()
        }
        if status == errSecDuplicateItem {
            // A concurrent call won the race and already wrote the token.
            // Re-read to get the persisted value rather than returning our
            // un-stored token (which would differ from theirs and break fleet
            // DB device-lifetime correlation).
            var retryItem: CFTypeRef?
            if SecItemCopyMatching(query as CFDictionary, &retryItem) == errSecSuccess,
               let retryData = retryItem as? Data {
                return retryData.map { String(format: "%02x", $0) }.joined()
            }
        }
        // Any other Keychain error: return un-stored token for this upload only.
        // The next upload will try again. Fleet DB may receive one orphaned record.
        return tokenData.map { String(format: "%02x", $0) }.joined()
    }
}
