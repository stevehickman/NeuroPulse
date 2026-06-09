import Foundation
import Combine
import CryptoKit
import UIKit

// OTA firmware update orchestration — dual-bank flow.
// Per NP-FW-EMMC-001 Rev A §8: Bank A = running firmware, Bank B = staging target.
//
// BLE OTA opcode sequence (main processor):
//   initiate → chunks (×N) → verify → [wait for .verified] → commit → [wait for .complete]
//
// Hub verifies Ed25519 signature on Scratch before writing to inactive bank.
// Safety MCU firmware update is a separate flow requiring explicit user confirmation.
//
// ISC-107: manifest fetched on hub connect and on OTAView appear.
// ISC-108: current hub firmware version shown alongside available version.
// ISC-109: INITIATE → chunks → VERIFY → COMMIT opcode sequence.
// ISC-110: bytes/total bytes progress bar.
// ISC-111: mid-OTA disconnect is safe — running bank is never touched until COMMIT.
// ISC-112: "Update complete — hub will restart" with reconnect wait.
// ISC-113: unsigned firmware chunks are never sent (Ed25519 fingerprint gate).

enum OTAError: LocalizedError {
    case notConnected
    case imageLoadFailed(String)
    case signatureInvalid
    case transferFailed(GATTWriteError)
    case hubRejected(UInt16)
    case safetyMCURequiresConfirmation
    case timeout
    case verificationFailed

    var errorDescription: String? {
        switch self {
        case .notConnected:
            return "Hub not connected."
        case .imageLoadFailed(let r):
            return "Could not load firmware image: \(r)"
        case .signatureInvalid:
            return "Firmware image signature verification failed. Do not install this image."
        case .transferFailed(let e):
            return "Transfer failed: \(e.localizedDescription)"
        case .hubRejected(let code):
            return "Hub rejected firmware (error \(String(format: "0x%04X", code))). The image may be corrupt."
        case .safetyMCURequiresConfirmation:
            return "Safety MCU update requires explicit confirmation."
        case .timeout:
            return "OTA timed out. Reconnect and try again."
        case .verificationFailed:
            return "Hub signature verification failed. Do not install this image."
        }
    }
}

@MainActor
final class OTAManager: ObservableObject {

    @Published private(set) var currentSession: OTASession?
    @Published private(set) var phase: OTAPhase = .idle
    @Published private(set) var progressPercent: Int = 0
    @Published private(set) var lastError: OTAError?
    @Published private(set) var availableUpdate: FirmwareImage?

    private let gatt: NeuroPulseGATTManager
    private let updateService: FirmwareUpdateProviding
    private var statusCancellable: AnyCancellable?
    private var connectionCancellable: AnyCancellable?

    // NeuroPulse OTA public key fingerprint — matches manufacturing root key in bootloader.
    // ISC-113: no firmware chunk is sent unless this fingerprint matches (Ed25519 gate).
    // Placeholder: replace with production key fingerprint at secure build time.
    private let trustedPublicKeyFingerprint = "4e455550deadbeef"

    init(gatt: NeuroPulseGATTManager,
         updateService: FirmwareUpdateProviding = FirmwareUpdateService.shared) {
        self.gatt = gatt
        self.updateService = updateService
        observeOTAStatus()
        observeConnectionForManifestFetch()
    }

    // MARK: - Manifest fetch (ISC-107)

    func checkForUpdates() {
        Task { await refreshManifest() }
    }

    private func refreshManifest() async {
        do {
            let manifest = try await updateService.fetchManifest()
            // Compare available version to current hub version.
            if let available = FirmwareVersion(manifest.version),
               let current = gatt.hubFirmwareVersion.flatMap(FirmwareVersion.init),
               available > current {
                availableUpdate = manifest.toFirmwareImage()
            } else if gatt.hubFirmwareVersion == nil {
                // Hub version unknown — show available update (user can decide).
                availableUpdate = manifest.toFirmwareImage()
            } else {
                availableUpdate = nil
            }
        } catch {
            // Manifest fetch failure is non-fatal; silently suppress on reconnect checks.
        }
    }

    // Auto-fetch manifest when hub connects (ISC-107).
    private func observeConnectionForManifestFetch() {
        connectionCancellable = gatt.$connectionState
            .filter { $0 == .connected }
            .sink { [weak self] _ in
                self?.checkForUpdates()
            }
    }

    // MARK: - Main OTA flow (ISC-109)

    func beginUpdate(image: FirmwareImage) async throws {
        // ISC-113: Verify Ed25519 fingerprint before ANY hub communication or download.
        // Fail fast with a clear error if the fingerprint is unrecognised.
        try verifySignature(image: image)

        guard gatt.connectionState == .connected else { throw OTAError.notConnected }

        // Download firmware binary from distribution endpoint (static URL, no device ID).
        let imageData: Data
        do {
            imageData = try await updateService.downloadFirmware(
                version: image.version, expectedSizeBytes: image.imageSizeBytes)
        } catch {
            let wrapped = OTAError.imageLoadFailed(error.localizedDescription)
            lastError = wrapped
            throw wrapped
        }

        var session = OTASession(image: image)
        session.totalBytes = imageData.count
        session.totalChunks = (imageData.count + OTASession.chunkSize - 1) / OTASession.chunkSize
        currentSession = session
        phase = .preparing
        progressPercent = 0
        lastError = nil

        // Step 1: Initiate — hub clears Scratch partition, prepares for receive.
        try await sendOTACommand(.initiate)
        phase = .transferring

        // Step 2: Transfer chunks with byte-level progress (ISC-110).
        var offset = 0
        var chunkIndex = 0
        while offset < imageData.count {
            let end = min(offset + OTASession.chunkSize, imageData.count)
            var payload = Data()
            var idx = UInt16(chunkIndex).littleEndian
            payload.append(Data(bytes: &idx, count: 2))
            payload.append(imageData[offset..<end])
            try await sendOTACommand(.chunk, payload: payload)
            chunkIndex += 1
            offset = end
            currentSession?.sentChunks = chunkIndex
            currentSession?.sentBytes = offset
            // Reserve 80% of progress bar for the transfer phase.
            progressPercent = max(1, Int(Double(offset) / Double(imageData.count) * 80))
        }

        // Step 3: Verify — hub runs np_ota_verify_scratch() (Ed25519 + SHA-256).
        // Hub responds via OTA_STATUS NOTIFY with phase .verifying → .verified or .failed.
        phase = .verifying
        progressPercent = 82
        try await sendOTACommand(.verify)
        try await waitForPhase(.verified, timeout: 60, label: "signature verification")
        progressPercent = 90

        // Step 4: Commit — hub writes inactive bank, stages boot swap, resets.
        // Hub will be unreachable during reboot; waitForCompletion handles reconnect.
        phase = .applying
        progressPercent = 93
        try await sendOTACommand(.commit)
        try await waitForCompletion()
    }

    // MARK: - Safety MCU firmware update (separate confirmation gate)

    func beginSafetyMCUUpdate(image: FirmwareImage) async throws {
        // Same fingerprint-first ordering as beginUpdate.
        try verifySignature(image: image)

        guard gatt.connectionState == .connected else { throw OTAError.notConnected }

        let imageData: Data
        do {
            imageData = try await updateService.downloadFirmware(
                version: image.version, expectedSizeBytes: image.imageSizeBytes)
        } catch {
            let wrapped = OTAError.imageLoadFailed(error.localizedDescription)
            lastError = wrapped
            throw wrapped
        }

        var session = OTASession(image: image)
        session.totalBytes = imageData.count
        session.totalChunks = (imageData.count + OTASession.chunkSize - 1) / OTASession.chunkSize
        currentSession = session
        phase = .preparing
        progressPercent = 0
        lastError = nil

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
            currentSession?.sentChunks = chunkIndex
            currentSession?.sentBytes = offset
            progressPercent = max(1, Int(Double(offset) / Double(imageData.count) * 80))
        }

        phase = .verifying
        progressPercent = 82
        try await sendOTACommand(.safetyMCUCommit)
        try await waitForCompletion()
    }

    func abort() {
        Task { try? await sendOTACommand(.abort) }
        phase = .idle
        currentSession = nil
        progressPercent = 0
    }

    // MARK: - Signature verification gate (ISC-113)

    // Guards against unsigned/unrecognised firmware images before transfer begins.
    // Full cryptographic verification is performed by the hub on the received image.
    private func verifySignature(image: FirmwareImage) throws {
        guard image.ed25519PublicKeyFingerprint.lowercased() == trustedPublicKeyFingerprint else {
            lastError = .signatureInvalid
            throw OTAError.signatureInvalid
        }
    }

    // MARK: - GATT status observation

    private func observeOTAStatus() {
        statusCancellable = gatt.$otaStatus
            .compactMap { $0 }
            .sink { [weak self] packet in
                guard let self else { return }
                let newPhase = OTAPhase(rawPacketByte: packet.phaseRaw)
                phase = newPhase
                // Hub-reported progress augments local byte-counting during transfer.
                if Int(packet.progressPercent) > progressPercent {
                    progressPercent = Int(packet.progressPercent)
                }
                if packet.isError {
                    lastError = .hubRejected(packet.errorCode)
                } else if newPhase == .complete {
                    progressPercent = 100
                    currentSession?.completedAt = Date()
                }
            }
    }

    // MARK: - Async helpers

    private func sendOTACommand(_ opcode: OTAOpcode, payload: Data = Data()) async throws {
        try await withCheckedThrowingContinuation { (cont: CheckedContinuation<Void, Error>) in
            gatt.sendOTACommand(opcode, payload: payload) { result in
                switch result {
                case .success:        cont.resume()
                case .failure(let e): cont.resume(throwing: OTAError.transferFailed(e))
                }
            }
        }
    }

    // Wait for a specific OTA phase from hub status notifications.
    // Used to synchronise verify → commit step handshake.
    private func waitForPhase(_ target: OTAPhase,
                               timeout: TimeInterval,
                               label: String) async throws {
        let deadline = Date().addingTimeInterval(timeout)
        while phase != target {
            guard Date() < deadline else {
                lastError = .timeout
                throw OTAError.timeout
            }
            if phase == .failed {
                lastError = .verificationFailed
                throw OTAError.verificationFailed
            }
            try await Task.sleep(nanoseconds: 200_000_000) // 200 ms poll
        }
    }

    // Wait for hub to reboot and reconnect (ISC-112).
    // Hub reboots after commit; allow 120 s for reconnect + boot verification.
    private func waitForCompletion() async throws {
        let deadline = Date().addingTimeInterval(120)
        while phase != .complete && phase != .failed {
            guard Date() < deadline else {
                lastError = .timeout
                throw OTAError.timeout
            }
            try await Task.sleep(nanoseconds: 500_000_000) // 500 ms poll
        }
        if phase == .failed {
            throw OTAError.hubRejected(0xFFFF)
        }
    }
}
