import Foundation
import Combine
import Security

// SHDR upload to NeuroPulse fleet database.
// SHDR = System Health Data Record — device condition only, never user biology.
// Upload is gated on:
//   1. Hub notifies SHDR_UPLOAD_STATUS characteristic (0x01 = pending upload)
//   2. User has accepted warranty consent (first-run)
//   3. USB-C connection detected (hub reports power source)
// Per CLAUDE.md §5.1: SHDR is linked to device ID + warranty owner ID only — never user identity.

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

// Endpoint placeholder — replace with production fleet analytics URL at launch.
private let fleetEndpoint = URL(string: "https://fleet.neuropulse.internal/v1/shdr")!

@MainActor
final class SHDRUploader: ObservableObject {

    @Published private(set) var isUploading = false
    @Published private(set) var lastUploadedAt: Date?
    @Published private(set) var lastError: SHDRUploadError?

    private let gatt: NeuroPulseGATTManager
    private let warrantyConsentKey = "np.warranty.consent.granted"
    private var cancellable: AnyCancellable?

    init(gatt: NeuroPulseGATTManager) {
        self.gatt = gatt
        observeUploadTrigger()
    }

    var warrantyConsentGranted: Bool {
        get { UserDefaults.standard.bool(forKey: warrantyConsentKey) }
        set { UserDefaults.standard.set(newValue, forKey: warrantyConsentKey) }
    }

    private func observeUploadTrigger() {
        cancellable = gatt.$shdrUploadPending
            .filter { $0 }
            .sink { [weak self] _ in
                Task { await self?.uploadIfConsented() }
            }
    }

    func uploadIfConsented() async {
        guard warrantyConsentGranted else { return }
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
        // token — never to identifierForVendor or any user-linked identifier.
        // identifierForVendor is app-bundle-scoped but is still a linkable identifier
        // that could correlate SHDR across data sources.
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
            (_, response) = try await URLSession.shared.data(for: request)
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
    // Keychain with ThisDeviceOnly accessibility. This replaces identifierForVendor
    // as the SHDR fleet DB linkage key — it is never joined to user identity.
    // Production: replace with the 256-bit TRNG token provisioned by the hub at
    // first pairing and delivered over the GATT warranty characteristic.
    private static let warrantyTokenTag = "com.neuropulse.shdr.warranty-token"

    private func warrantyTokenFromKeychain() -> String {
        // kSecAttrAccount is part of the Keychain primary key for kSecClassGenericPassword
        // alongside kSecAttrService. Both must be present in read and write queries to
        // avoid ambiguous matches if another item ever shares the service string.
        let query: [CFString: Any] = [
            kSecClass:              kSecClassGenericPassword,
            kSecAttrService:        Self.warrantyTokenTag,
            kSecAttrAccount:        "warranty-token",
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

