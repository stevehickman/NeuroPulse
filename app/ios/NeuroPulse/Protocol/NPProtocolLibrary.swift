import Foundation
import Combine

// MARK: - Protocol Availability

enum ProtocolAvailability: Equatable {
    case available
    case unavailable(missingModalities: [NPModalityType])

    var isAvailable: Bool {
        if case .available = self { return true }
        return false
    }

    var unavailableReason: String? {
        switch self {
        case .available:
            return nil
        case .unavailable(let missing):
            if missing.isEmpty { return "No device connected." }
            let names = missing.map { $0.displayName }.joined(separator: ", ")
            return "Requires: \(names)"
        }
    }
}

// MARK: - NPProtocolLibrary

@MainActor
final class NPProtocolLibrary: ObservableObject {

    // MARK: Published state

    @Published var userProtocols: [NPProtocolEntry] = []
    @Published var availableModalities: Set<NPModalityType> = NPModalityType.t1Set
    @Published var has1064SmartModules: Bool = false
    @Published var connectedDeviceTier: DeviceTier = .none

    // MARK: Device tier

    enum DeviceTier: Equatable {
        case none
        case t1
        case t2
    }

    // MARK: Persistence URL

    private static var persistenceURL: URL {
        URL.documentsDirectory.appending(path: "np_user_protocols.json")
    }

    // MARK: Init

    init() {
        loadFromDisk()
    }

    // MARK: Computed all-protocols list

    var allProtocols: [NPProtocolEntry] {
        NPPredefinedProtocols.all + userProtocols
    }

    // MARK: Availability

    func availability(for entry: NPProtocolEntry) -> ProtocolAvailability {
        guard connectedDeviceTier != .none else {
            return .unavailable(missingModalities: [])
        }

        // For composite protocols, resolve through the library
        if case .composite(let comp) = entry {
            var missingNames: Set<NPModalityType> = []
            for layer in comp.layers {
                if let proto = findSingle(named: layer.protocolName) {
                    let sub = availability(for: .single(proto))
                    if case .unavailable(let m) = sub { missingNames.formUnion(m) }
                }
            }
            return missingNames.isEmpty ? .available : .unavailable(missingModalities: Array(missingNames))
        }

        // Single protocol
        let required = entry.requiredModalityTypes

        // Check 1064nm smart module requirement
        var missing: [NPModalityType] = []
        if case .single(let proto) = entry, proto.requires1064SmartModule(), !has1064SmartModules {
            // Not strictly a missing modality but we flag the PBM modality
            // We treat it as unavailable pbmTranscranial (smart module variant)
            missing.append(.pbmTranscranial)
        }

        for modality in required {
            if !isModalityAvailable(modality) {
                if !missing.contains(modality) {
                    missing.append(modality)
                }
            }
        }

        return missing.isEmpty ? .available : .unavailable(missingModalities: missing)
    }

    private func isModalityAvailable(_ modality: NPModalityType) -> Bool {
        switch modality.tier {
        case .t1:
            return connectedDeviceTier == .t1 || connectedDeviceTier == .t2
        case .t2:
            return connectedDeviceTier == .t2
        case .accessory:
            // Accessories always show as provisionally available
            return true
        }
    }

    private func findSingle(named name: String) -> NPProtocolDefinition? {
        for entry in allProtocols {
            if case .single(let proto) = entry, proto.name == name { return proto }
        }
        return nil
    }

    // MARK: Mutations

    func save(_ entry: NPProtocolEntry) {
        let id = entry.id
        if let idx = userProtocols.firstIndex(where: { $0.id == id }) {
            userProtocols[idx] = entry
        } else {
            userProtocols.append(entry)
        }
        saveToDisk()
    }

    func delete(_ id: UUID) {
        userProtocols.removeAll { $0.id == id }
        saveToDisk()
    }

    // MARK: Script export / import

    func exportScript(_ entry: NPProtocolEntry) -> String {
        NPPSSerializer().serialize(entry)
    }

    func importScript(_ text: String) throws -> NPProtocolEntry {
        var lexer = NPPSLexer(text)
        let tokens = try lexer.tokenize()
        var parser = NPPSParser(tokens)
        let entries = try parser.parse()
        guard let first = entries.first else {
            throw NPPSError(message: "No protocol found in script", line: 0)
        }
        return first
    }

    // MARK: Device availability update

    func updateAvailability(tier: DeviceTier, smartModules: Bool) {
        connectedDeviceTier = tier
        has1064SmartModules = smartModules
        switch tier {
        case .none:
            availableModalities = []
        case .t1:
            availableModalities = NPModalityType.t1Set
        case .t2:
            availableModalities = NPModalityType.t2Set
        }
        if smartModules {
            availableModalities.insert(.pbmTranscranial) // already in, but belt+braces
        }
    }

    // MARK: Persistence

    private func loadFromDisk() {
        let url = Self.persistenceURL
        guard FileManager.default.fileExists(atPath: url.path) else { return }
        do {
            let data = try Data(contentsOf: url)
            let decoder = JSONDecoder()
            decoder.dateDecodingStrategy = .iso8601
            userProtocols = try decoder.decode([NPProtocolEntry].self, from: data)
        } catch {
            // If loading fails (e.g. schema migration), start with empty user protocols
            userProtocols = []
        }
    }

    private func saveToDisk() {
        let url = Self.persistenceURL
        do {
            let encoder = JSONEncoder()
            encoder.dateEncodingStrategy = .iso8601
            encoder.outputFormatting = [.prettyPrinted, .sortedKeys]
            let data = try encoder.encode(userProtocols)
            try data.write(to: url, options: .atomic)
        } catch {
            // Silently fail; SHDR could log this in a real implementation
        }
    }
}
