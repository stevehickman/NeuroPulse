import Foundation

// Mode 4: EDF+ session data download.
// When the user reconnects via USB-C, the hub streams EDF+ files + parameter logs.
// The app requests a specific session by ID via the EDF_REQUEST GATT characteristic;
// actual data arrives over the USB-C bulk transfer channel (exposed as a local file URL
// by the hub's USB-C mass storage or CDC interface).

// EDF+ is the standard format for EEG/biosignal archiving (CLAUDE.md §4.6, Mode 4).

enum EDFDownloadError: LocalizedError {
    case notConnected
    case alreadyDownloading
    case requestFailed(Error)
    case noDataReceived
    case parseError(String)

    var errorDescription: String? {
        switch self {
        case .notConnected:          return "Hub not connected. Reconnect via USB-C to download session data."
        case .alreadyDownloading:    return "A download is already in progress."
        case .requestFailed(let e):  return "Download request failed: \(e.localizedDescription)"
        case .noDataReceived:        return "No session data received from hub."
        case .parseError(let s):     return "EDF+ parse error: \(s)"
        }
    }
}

struct EDFHeader {
    var version: String             // "0       "
    var patientID: String           // anonymized device ID (never user name)
    var recordingID: String
    var startDate: String           // dd.mm.yy
    var startTime: String           // hh.mm.ss
    var headerBytes: Int
    var reserved: String            // "EDF+C" or "EDF+D"
    var numberOfRecords: Int
    var durationPerRecord: Double   // seconds
    var numberOfSignals: Int
}

struct EDFSignal {
    var label: String               // e.g. "EEG Fp1" or "HRV RR"
    var transducerType: String
    var physicalDimension: String   // e.g. "uV", "ms"
    var physicalMin: Double
    var physicalMax: Double
    var digitalMin: Int
    var digitalMax: Double
    var prefiltering: String
    var samplesPerRecord: Int
    var samples: [Double] = []
}

struct EDFSession: Identifiable {
    var id: UUID = UUID()
    var sessionID: UInt32
    var downloadedAt: Date = Date()
    var header: EDFHeader
    var signals: [EDFSignal]
    var parameterLog: [String: String]   // key-value from hub parameter log
    var localURL: URL?                   // path in app's Documents/UHDR directory
}

// MARK: - Downloader

@MainActor
final class EDFDownloader: ObservableObject {

    @Published private(set) var isDownloading = false
    @Published private(set) var downloadedSessions: [EDFSession] = []
    @Published private(set) var lastError: EDFDownloadError?

    private let gatt: NeurOneGATTManager
    private let uhdrDirectory: URL

    init(gatt: NeurOneGATTManager) {
        self.gatt = gatt
        let docs = FileManager.default.urls(for: .documentDirectory, in: .userDomainMask)[0]
        uhdrDirectory = docs.appendingPathComponent("UHDR", isDirectory: true)
        try? FileManager.default.createDirectory(at: uhdrDirectory, withIntermediateDirectories: true)
        // EDF+ files contain raw EEG/HRV waveforms (UHDR) — must not sync to iCloud/iTunes.
        try? (uhdrDirectory as NSURL).setResourceValue(true, forKey: .isExcludedFromBackupKey)
    }

    // Request EDF+ download for a session by ID.
    // The hub will queue the file for USB-C transfer; the app polls the UHDR directory.
    func requestDownload(sessionID: UInt32) async throws -> EDFSession {
        guard gatt.connectionState == .connected else { throw EDFDownloadError.notConnected }
        // Reject concurrent callers — two downloads of the same session would append
        // duplicate entries to downloadedSessions.
        guard !isDownloading else { throw EDFDownloadError.alreadyDownloading }
        isDownloading = true
        lastError = nil
        defer { isDownloading = false }

        try await withCheckedThrowingContinuation { (cont: CheckedContinuation<Void, Error>) in
            gatt.requestEDFDownload(sessionID: sessionID) { result in
                switch result {
                case .success:        cont.resume()
                case .failure(let e): cont.resume(throwing: EDFDownloadError.requestFailed(e))
                }
            }
        }

        // Poll for the file to appear in the UHDR directory (hub delivers via USB-C bulk transfer).
        let expectedFilename = "session_\(sessionID).edf"
        let expectedURL = uhdrDirectory.appendingPathComponent(expectedFilename)
        let timeout = Date().addingTimeInterval(30)

        while !FileManager.default.fileExists(atPath: expectedURL.path) {
            guard Date() < timeout else { throw EDFDownloadError.noDataReceived }
            try Task.checkCancellation()
            try await Task.sleep(nanoseconds: 500_000_000)  // 500ms poll
        }

        // Exclude the landed file from iCloud/iTunes backup — UHDR (raw waveforms).
        try? (expectedURL as NSURL).setResourceValue(true, forKey: .isExcludedFromBackupKey)

        let session = try parseEDFFile(at: expectedURL, sessionID: sessionID)
        // Deduplicate: a re-download of an existing session must not create a second entry.
        if !downloadedSessions.contains(where: { $0.sessionID == sessionID }) {
            downloadedSessions.append(session)
        }
        return session
    }

    // MARK: - EDF+ parser (header only for now; full signal parsing at waveform review feature)

    private func parseEDFFile(at url: URL, sessionID: UInt32) throws -> EDFSession {
        let data = try Data(contentsOf: url)
        guard data.count >= 256 else { throw EDFDownloadError.parseError("File too short for EDF header") }

        func field(_ offset: Int, _ length: Int) -> String {
            String(bytes: data[offset..<(offset+length)], encoding: .ascii)?
                .trimmingCharacters(in: .whitespaces) ?? ""
        }

        let version           = field(0, 8)
        let patientID         = field(8, 80)
        let recordingID       = field(88, 80)
        let startDate         = field(168, 8)
        let startTime         = field(176, 8)
        let headerBytes       = Int(field(184, 8)) ?? 0
        let reserved          = field(192, 44)
        let numberOfRecords   = Int(field(236, 8)) ?? 0
        let durationPerRecord = Double(field(244, 8)) ?? 1.0
        let nSignals          = Int(field(252, 4)) ?? 0

        let header = EDFHeader(
            version: version, patientID: patientID, recordingID: recordingID,
            startDate: startDate, startTime: startTime, headerBytes: headerBytes,
            reserved: reserved, numberOfRecords: numberOfRecords,
            durationPerRecord: durationPerRecord, numberOfSignals: nSignals
        )

        // Signal header parsing (labels only — full sample parsing deferred to waveform view)
        var signals: [EDFSignal] = []
        if data.count >= headerBytes && nSignals > 0 {
            let labelOffset = 256
            for i in 0..<nSignals {
                let label = field(labelOffset + i * 16, 16)
                let physDim = field(labelOffset + nSignals * (16 + 80 + 8) + i * 8, 8)
                signals.append(EDFSignal(
                    label: label, transducerType: "", physicalDimension: physDim,
                    physicalMin: -32768, physicalMax: 32767, digitalMin: -32768, digitalMax: 32767,
                    prefiltering: "", samplesPerRecord: 0
                ))
            }
        }

        return EDFSession(
            sessionID: sessionID, header: header, signals: signals,
            parameterLog: [:], localURL: url
        )
    }
}
