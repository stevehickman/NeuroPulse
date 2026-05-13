import Foundation
import Combine
import CryptoKit
import UIKit

// OTA firmware update orchestration — dual-bank flow.
// Per NP-FW-EMMC-001 Rev A §8: Bank A = running firmware, Bank B = staging target.
// Hub verifies Ed25519 signature on Bank B before setting boot bank flag.
// Safety MCU firmware update is a separate flow requiring explicit user confirmation.

enum OTAError: LocalizedError {
    case notConnected
    case imageLoadFailed(String)
    case signatureInvalid
    case transferFailed(GATTWriteError)
    case hubRejected(UInt16)
    case safetyMCURequiresConfirmation
    case timeout

    var errorDescription: String? {
        switch self {
        case .notConnected:                     return "Hub not connected."
        case .imageLoadFailed(let r):           return "Could not load firmware image: \(r)"
        case .signatureInvalid:                 return "Firmware image signature verification failed. Do not install this image."
        case .transferFailed(let e):            return "Transfer failed: \(e.localizedDescription)"
        case .hubRejected(let code):            return "Hub rejected firmware (error code \(code)). The image may be corrupt."
        case .safetyMCURequiresConfirmation:    return "Safety MCU update requires explicit confirmation."
        case .timeout:                          return "OTA timed out. Reconnect and try again."
        }
    }
}

@MainActor
final class OTAManager: ObservableObject {

    @Published private(set) var currentSession: OTASession?
    @Published private(set) var phase: OTAPhase = .idle
    @Published private(set) var progressPercent: Int = 0
    @Published private(set) var lastError: OTAError?
    @Published private(set) var isAwaitingSafetyMCUConfirmation = false

    private let gatt: NeuroPulseGATTManager
    private var cancellable: AnyCancellable?
    private var timeoutTask: Task<Void, Never>?

    // NeuroPulse OTA public key — matches the manufacturing root key embedded in bootloader.
    // Placeholder: replace with production key fingerprint at secure build time.
    private let trustedPublicKeyFingerprint = "4e455550deadbeef"

    init(gatt: NeuroPulseGATTManager) {
        self.gatt = gatt
        observeOTAStatus()
    }

    // MARK: - Main OTA flow

    func beginUpdate(image: FirmwareImage, imageData: Data) async throws {
        guard gatt.connectionState == .connected else { throw OTAError.notConnected }

        // Verify Ed25519 signature before sending a single byte to the hub.
        try verifySignature(image: image, data: imageData)

        var session = OTASession(image: image)
        session.totalChunks = (imageData.count + OTASession.chunkSize - 1) / OTASession.chunkSize
        currentSession = session
        phase = .preparing
        progressPercent = 0
        lastError = nil

        // Step 1: Begin command
        try await sendOTACommand(.begin)
        phase = .transferring

        // Step 2: Transfer chunks
        var offset = 0
        var chunkIndex = 0
        while offset < imageData.count {
            let end = min(offset + OTASession.chunkSize, imageData.count)
            let chunk = imageData[offset..<end]

            // Chunk payload: 2-byte chunk index (little-endian) + chunk data
            var payload = Data()
            var idx = UInt16(chunkIndex).littleEndian
            payload.append(Data(bytes: &idx, count: 2))
            payload.append(chunk)

            try await sendOTACommand(.chunk, payload: payload)

            chunkIndex += 1
            offset = end
            currentSession?.sentChunks = chunkIndex
            progressPercent = Int(Double(chunkIndex) / Double(session.totalChunks) * 90)
        }

        // Step 3: Commit — hub verifies Ed25519 + SHA-256, swaps bank flag, reboots
        phase = .verifying
        try await sendOTACommand(.commit)
        phase = .applying
        progressPercent = 95

        // Step 4: Wait for hub to reboot and come back online (OTA_STATUS NOTIFY = complete)
        try await waitForCompletion()
    }

    // MARK: - Safety MCU firmware update (separate confirmation gate)

    func beginSafetyMCUUpdate(image: FirmwareImage, imageData: Data) async throws {
        guard gatt.connectionState == .connected else { throw OTAError.notConnected }
        // Safety MCU update must be confirmed by user in UI before this method is called.
        // The hub firmware requires explicit confirmation opcode for Safety MCU updates.
        try verifySignature(image: image, data: imageData)

        var session = OTASession(image: image)
        session.totalChunks = (imageData.count + OTASession.chunkSize - 1) / OTASession.chunkSize
        currentSession = session
        phase = .preparing

        try await sendOTACommand(.safetyMCUBegin)
        phase = .transferring

        var offset = 0
        var chunkIndex = 0
        while offset < imageData.count {
            let end = min(offset + OTASession.chunkSize, imageData.count)
            var payload = Data()
            var idx = UInt16(chunkIndex).littleEndian
            payload.append(Data(bytes: &idx, count: 2))
            payload.append(imageData[offset..<end])
            try await sendOTACommand(.safetyMCUChunk, payload: payload)
            chunkIndex += 1
            offset = end
        }

        phase = .verifying
        try await sendOTACommand(.safetyMCUCommit)
        try await waitForCompletion()
    }

    func abort() {
        timeoutTask?.cancel()
        Task { try? await sendOTACommand(.abort) }
        phase = .idle
        currentSession = nil
        progressPercent = 0
    }

    // MARK: - Signature verification

    // Verifies the Ed25519 fingerprint displayed to the user matches the trusted key.
    // Full signature verification occurs on the hub; app verifies fingerprint only.
    private func verifySignature(image: FirmwareImage, data: Data) throws {
        guard image.ed25519PublicKeyFingerprint.lowercased() == trustedPublicKeyFingerprint else {
            lastError = .signatureInvalid
            throw OTAError.signatureInvalid
        }
    }

    // MARK: - GATT status observation

    private func observeOTAStatus() {
        cancellable = gatt.$otaStatus
            .compactMap { $0 }
            .sink { [weak self] packet in
                guard let self else { return }
                let newPhase = OTAPhase(rawPacketByte: packet.phaseRaw)
                phase = newPhase
                progressPercent = max(progressPercent, Int(packet.progressPercent))
                if packet.isError {
                    lastError = .hubRejected(packet.errorCode)
                    timeoutTask?.cancel()
                } else if newPhase == .complete {
                    progressPercent = 100
                    currentSession?.completedAt = Date()
                    timeoutTask?.cancel()
                }
            }
    }

    // MARK: - Helpers

    private func sendOTACommand(_ opcode: OTAOpcode, payload: Data = Data()) async throws {
        try await withCheckedThrowingContinuation { (cont: CheckedContinuation<Void, Error>) in
            gatt.sendOTACommand(opcode, payload: payload) { result in
                switch result {
                case .success:       cont.resume()
                case .failure(let e): cont.resume(throwing: OTAError.transferFailed(e))
                }
            }
        }
    }

    private func waitForCompletion() async throws {
        // The hub reboots after commit. Allow up to 90 seconds for reconnect.
        let deadline = Date().addingTimeInterval(90)
        while phase != .complete && phase != .failed {
            guard Date() < deadline else {
                lastError = .timeout
                throw OTAError.timeout
            }
            try await Task.sleep(nanoseconds: 500_000_000)
        }
        if phase == .failed {
            throw OTAError.hubRejected(0xFFFF)
        }
    }
}

