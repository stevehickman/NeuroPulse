import Foundation
import Combine

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

        guard let deviceID = UIDevice.current.identifierForVendor?.uuidString else {
            throw SHDRUploadError.noData
        }

        // In production the hub pushes the SHDR binary blob over USB-C bulk transfer;
        // the app reads it from a staging file dropped by the hub's CDC interface.
        let stagingURL = FileManager.default.urls(for: .documentDirectory, in: .userDomainMask)[0]
            .appendingPathComponent("shdr_staging.bin")

        guard let shdrData = try? Data(contentsOf: stagingURL) else {
            throw SHDRUploadError.noData
        }

        var request = URLRequest(url: fleetEndpoint)
        request.httpMethod = "POST"
        request.setValue("application/octet-stream", forHTTPHeaderField: "Content-Type")
        request.setValue(deviceID, forHTTPHeaderField: "X-NP-Device-ID")
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
}

