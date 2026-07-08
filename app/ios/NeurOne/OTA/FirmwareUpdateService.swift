import Foundation
import CryptoKit

// Firmware manifest fetch and image download service.
//
// Privacy design (NP-PRIV-REM-001):
// • Manifest request contains NO device ID, user ID, firmware version, or any
//   user-correlated data. The server receives identical requests from all clients
//   and cannot distinguish between devices or users.
// • Download URL is always the canonical static path derived from version string.
//   No presigned, personalized, or expiring URLs — prevents per-device tracking.
// • User-Agent is a static, non-device-specific string.
// • SPKI certificate pinning on the firmware distribution domain prevents MITM
//   manifest substitution (e.g. suppressing security updates) even though the
//   hub independently verifies the Ed25519 image signature.

// MARK: - Service errors

enum FirmwareUpdateError: LocalizedError {
    case networkError(Error)
    case invalidResponse(Int)       // unexpected HTTP status code
    case malformedManifest(String)  // JSON decode failure
    case downloadFailed(Error)
    case downloadSizeMismatch(expected: Int, actual: Int)

    var errorDescription: String? {
        switch self {
        case .networkError(let e):          return "Network error: \(e.localizedDescription)"
        case .invalidResponse(let code):    return "Server returned HTTP \(code)"
        case .malformedManifest(let msg):   return "Manifest parse error: \(msg)"
        case .downloadFailed(let e):        return "Download failed: \(e.localizedDescription)"
        case .downloadSizeMismatch(let exp, let act):
            return "Download size mismatch: expected \(exp) bytes, got \(act)"
        }
    }
}

// MARK: - Protocol (enables mock injection in tests)

protocol FirmwareUpdateProviding: AnyObject {
    func fetchManifest() async throws -> FirmwareManifest
    func downloadFirmware(version: String, expectedSizeBytes: Int) async throws -> Data
}

// MARK: - Production implementation

final class FirmwareUpdateService: FirmwareUpdateProviding {

    static let shared = FirmwareUpdateService()

    // INFRASTRUCTURE — firmware.neurone.ai
    //
    // Domain   : Public subdomain of neurone.ai.
    //   (1) Register neurone.ai with your domain registrar.
    //   (2) Create a CNAME: firmware.neurone.ai → your CDN origin (e.g. CloudFront,
    //       Fastly, or a GCS/S3 bucket with a custom domain).
    //
    // Purpose  : OTA firmware manifest and signed image distribution.
    //
    // Manifest : GET https://firmware.neurone.ai/manifest.json
    //   Response body (JSON, FirmwareManifest schema):
    //     { "version": "<major>.<minor>.<patch>",
    //       "sizeBytes": <integer>,
    //       "releaseDate": "<YYYY-MM-DD>" }
    //
    // Images   : GET https://firmware.neurone.ai/firmware/main-<version>.npfw
    //   Body   : Ed25519-signed binary firmware image (NP-FW-EMMC-001 Rev A format).
    //            The hub independently verifies the Ed25519 signature before flashing.
    //
    // Privacy  : Static, non-personalized URLs — no device ID, user ID, or version
    //            number in the manifest request. All clients send identical requests.
    //
    // TLS/pinning : This endpoint is SPKI-pinned (FirmwarePinningDelegate).
    //   Pre-launch actions:
    //   (1) Obtain a TLS certificate for firmware.neurone.ai.
    //   (2) Derive SPKI SHA-256: openssl s_client -connect firmware.neurone.ai:443 \
    //         2>/dev/null | openssl x509 -noout -pubkey | \
    //         openssl pkey -pubin -outform DER | sha256sum
    //   (3) Replace placeholder values in pinnedSPKIHashes (primary + backup key).
    private static let manifestURL = URL(string: "https://firmware.neurone.ai/manifest.json")!
    private static func firmwareURL(for version: String) -> URL {
        // Version string is validated by FirmwareVersion before use.
        URL(string: "https://firmware.neurone.ai/firmware/main-\(version).npfw")!
    }

    // Placeholder SPKI hashes — replace with real values derived from production TLS
    // certificate chain before first public release. Two hashes allow key rotation
    // without a forced app update (primary + backup).
    // $ openssl s_client -connect firmware.neurone.ai:443 2>/dev/null | \
    //     openssl x509 -noout -pubkey | \
    //     openssl pkey -pubin -outform DER | sha256sum
    private static let pinnedSPKIHashes: Set<Data> = [
        Data(hexString: "0000000000000000000000000000000000000000000000000000000000000000")!,
        Data(hexString: "0000000000000000000000000000000000000000000000000000000000000001")!,
    ]

    private let session: URLSession

    init(session: URLSession? = nil) {
        if let session {
            self.session = session
        } else {
            let config = URLSessionConfiguration.ephemeral
            // Static, non-device-identifying User-Agent.
            config.httpAdditionalHeaders = ["User-Agent": "NeurOne-OTA/1.0"]
            config.timeoutIntervalForRequest = 30
            config.timeoutIntervalForResource = 300  // 5 min for firmware download
            self.session = URLSession(
                configuration: config,
                delegate: FirmwarePinningDelegate(pinnedHashes: FirmwareUpdateService.pinnedSPKIHashes),
                delegateQueue: nil
            )
        }
    }

    // MARK: - FirmwareUpdateProviding

    func fetchManifest() async throws -> FirmwareManifest {
        let (data, response) = try await session.data(from: Self.manifestURL)
        guard let http = response as? HTTPURLResponse else {
            throw FirmwareUpdateError.networkError(URLError(.badServerResponse))
        }
        guard http.statusCode == 200 else {
            throw FirmwareUpdateError.invalidResponse(http.statusCode)
        }
        do {
            let decoder = JSONDecoder()
            return try decoder.decode(FirmwareManifest.self, from: data)
        } catch {
            throw FirmwareUpdateError.malformedManifest(error.localizedDescription)
        }
    }

    func downloadFirmware(version: String, expectedSizeBytes: Int) async throws -> Data {
        guard FirmwareVersion(version) != nil else {
            throw FirmwareUpdateError.malformedManifest("Invalid version string: \(version)")
        }
        let url = Self.firmwareURL(for: version)
        let (data, response): (Data, URLResponse)
        do {
            (data, response) = try await session.data(from: url)
        } catch {
            throw FirmwareUpdateError.downloadFailed(error)
        }
        guard let http = response as? HTTPURLResponse, http.statusCode == 200 else {
            throw FirmwareUpdateError.networkError(URLError(.badServerResponse))
        }
        if data.count != expectedSizeBytes {
            throw FirmwareUpdateError.downloadSizeMismatch(
                expected: expectedSizeBytes, actual: data.count)
        }
        return data
    }
}

// MARK: - Certificate pinning delegate

private final class FirmwarePinningDelegate: NSObject, URLSessionDelegate {

    private let pinnedHashes: Set<Data>

    init(pinnedHashes: Set<Data>) {
        self.pinnedHashes = pinnedHashes
    }

    func urlSession(
        _ session: URLSession,
        didReceive challenge: URLAuthenticationChallenge,
        completionHandler: @escaping (URLSession.AuthChallengeDisposition, URLCredential?) -> Void
    ) {
        guard challenge.protectionSpace.authenticationMethod == NSURLAuthenticationMethodServerTrust,
              let serverTrust = challenge.protectionSpace.serverTrust else {
            completionHandler(.cancelAuthenticationChallenge, nil)
            return
        }

        // Evaluate OS trust store first.
        var error: CFError?
        guard SecTrustEvaluateWithError(serverTrust, &error) else {
            completionHandler(.cancelAuthenticationChallenge, nil)
            return
        }

        // Pin the leaf certificate's SPKI SHA-256 (iOS 15+).
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

    // SHA-256 of the leaf certificate's DER-encoded SubjectPublicKeyInfo.
    // Matches the hash produced by:
    //   openssl s_client -connect firmware.neurone.ai:443 2>/dev/null | \
    //     openssl x509 -noout -pubkey | \
    //     openssl pkey -pubin -outform DER | sha256sum
    private func spkiSHA256(of certificate: SecCertificate) -> Data? {
        guard
            let publicKey = SecCertificateCopyKey(certificate),
            let rawKey = SecKeyCopyExternalRepresentation(publicKey, nil) as Data?,
            let header = spkiDERHeader(for: publicKey)
        else { return nil }

        let spki = Data(header) + rawKey
        return Data(SHA256.hash(data: spki))
    }

    // DER SPKI header bytes prepended to SecKeyCopyExternalRepresentation output.
    // For RSA the raw key is PKCS#1 DER; for EC it is the uncompressed point (04 || X || Y).
    // Headers match TrustKit reference implementation.
    private func spkiDERHeader(for key: SecKey) -> [UInt8]? {
        guard
            let attrs = SecKeyCopyAttributes(key) as? [CFString: Any],
            let keyType = attrs[kSecAttrKeyType] as? String,
            let keySize = attrs[kSecAttrKeySizeInBits] as? Int
        else { return nil }

        let rsa = kSecAttrKeyTypeRSA as String
        let ec  = kSecAttrKeyTypeECSECPrimeRandom as String

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

// MARK: - Data hex init helper

private extension Data {
    init?(hexString: String) {
        guard hexString.count % 2 == 0 else { return nil }
        var data = Data(capacity: hexString.count / 2)
        var index = hexString.startIndex
        while index < hexString.endIndex {
            let next = hexString.index(index, offsetBy: 2)
            guard let byte = UInt8(hexString[index ..< next], radix: 16) else { return nil }
            data.append(byte)
            index = next
        }
        self = data
    }
}
