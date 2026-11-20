import Foundation
import Combine

// MARK: - NPLimitsStore

@MainActor
final class NPLimitsStore: ObservableObject {

    // MARK: Published state

    @Published var globalLimits: NPLimitsSet?
    @Published var helmetLimits: [String: NPLimitsSet] = [:]    // keyed by helmet serial
    @Published var individualLimits: [UUID: NPLimitsSet] = [:]  // keyed by profile id
    @Published var profiles: [NPIndividualProfile] = []
    @Published var activeProfileId: UUID? = nil
    @Published var activeHelmetSerial: String? = nil             // updated by GATT manager

    // MARK: Computed

    var activeProfile: NPIndividualProfile? {
        profiles.first { $0.id == activeProfileId }
    }

    /// Resolved limits for current context: individual > helmet > global, field by field.
    var resolvedLimits: NPLimitsSet {
        NPLimitsSet.resolve(
            global: globalLimits,
            helmet: activeHelmetSerial.flatMap { helmetLimits[$0] },
            individual: activeProfileId.flatMap { individualLimits[$0] }
        ).limits
    }

    /// Resolved limits plus their source map (which tier contributed each field).
    var resolvedLimitsWithSources: (limits: NPLimitsSet, sources: NPLimitSourceMap) {
        NPLimitsSet.resolve(
            global: globalLimits,
            helmet: activeHelmetSerial.flatMap { helmetLimits[$0] },
            individual: activeProfileId.flatMap { individualLimits[$0] }
        )
    }

    /// Active resolution chain description for UI display.
    var resolutionChainDescription: String {
        var parts: [String] = []
        if let profile = activeProfile {
            parts.append(String(format: String(localized: "LIMITS_CHAIN_INDIVIDUAL"), profile.name))
        }
        if let serial = activeHelmetSerial, helmetLimits[serial] != nil {
            parts.append(String(format: String(localized: "LIMITS_CHAIN_HELMET"), serial))
        }
        if globalLimits != nil { parts.append(String(localized: "LIMITS_CHAIN_GLOBAL")) }
        if parts.isEmpty { return String(localized: "LIMITS_CHAIN_NONE") }
        return String(format: String(localized: "LIMITS_CHAIN_ACTIVE"),
                      parts.joined(separator: " + "))
    }

    // MARK: Factory

    func makeValidator() -> NPProtocolValidator {
        let (limits, sources) = resolvedLimitsWithSources
        return NPProtocolValidator(resolvedLimits: limits, sourceMap: sources)
    }

    // MARK: Mutations

    func saveGlobalLimits(_ limits: NPLimitsSet) {
        var l = limits
        l.level = .global
        l.modifiedAt = Date()
        globalLimits = l
        persist()
    }

    func saveHelmetLimits(_ limits: NPLimitsSet, forHelmet serial: String) {
        var l = limits
        l.level = .helmet
        l.helmetId = serial
        l.modifiedAt = Date()
        helmetLimits[serial] = l
        persist()
    }

    func saveIndividualLimits(_ limits: NPLimitsSet, forProfile id: UUID) {
        var l = limits
        l.level = .individual
        l.individualId = id
        l.modifiedAt = Date()
        individualLimits[id] = l
        persist()
    }

    /// Keep `profile`. A note over the bound is refused (false) and nothing changes (REQ-SW03-32 (b)).
    @discardableResult
    func saveProfile(_ profile: NPIndividualProfile) -> Bool {
        guard profile.notes.utf16.count <= NPIndividualProfile.maxNotesLength else { return false }
        if let idx = profiles.firstIndex(where: { $0.id == profile.id }) {
            profiles[idx] = profile
        } else {
            profiles.append(profile)
        }
        persist()
        return true
    }

    func deleteProfile(_ id: UUID) {
        profiles.removeAll { $0.id == id }
        individualLimits.removeValue(forKey: id)
        if activeProfileId == id { activeProfileId = nil }
        persist()
    }

    func setActiveProfile(_ id: UUID?) {
        activeProfileId = id
        persist()
    }

    func updateActiveHelmet(serial: String?) {
        activeHelmetSerial = serial
        // No persist — this is a runtime connection state, not stored
    }

    // MARK: NPPS import/export

    /// The limits as the script a person sees, copies or sends: the share form, with no helmet serial and no individual
    /// id (SWR-WO-07 (f)). The store keeps limits as JSON, so this is never the stored form; saving a script back into
    /// a tier stamps the tier's own id.
    func exportLimitsAsNPPS(_ limits: NPLimitsSet) -> String {
        NPPSSerializer().shareLimits(limits)
    }

    func importLimitsFromNPPS(_ text: String) throws -> NPLimitsSet {
        var lexer = NPPSLexer(text)
        let tokens = try lexer.tokenize()
        var parser = NPPSParser(tokens)
        let entries = try parser.parse()
        guard let limitsEntry = entries.compactMap({ entry -> NPLimitsSet? in
            if case .limits(let l) = entry { return l }
            return nil
        }).first else {
            throw NPPSError(message: String(localized: "LIMITS_NO_LIMITS_BLOCK_FOUND_IN_SCRIPT"), line: 0)
        }
        return limitsEntry
    }

    // MARK: Persistence — JSON to Documents directory

    private static var globalLimitsURL: URL {
        URL.documentsDirectory.appending(path: "np_global_limits.json")
    }
    private static var helmetLimitsURL: URL {
        URL.documentsDirectory.appending(path: "np_helmet_limits.json")
    }
    private static var individualLimitsURL: URL {
        URL.documentsDirectory.appending(path: "np_individual_limits.json")
    }
    private static var profilesURL: URL {
        URL.documentsDirectory.appending(path: "np_profiles.json")
    }
    private static var activeProfileURL: URL {
        URL.documentsDirectory.appending(path: "np_active_profile.json")
    }

    private func makeEncoder() -> JSONEncoder {
        let e = JSONEncoder()
        e.dateEncodingStrategy = .iso8601
        e.outputFormatting = [.prettyPrinted, .sortedKeys]
        return e
    }

    private func makeDecoder() -> JSONDecoder {
        let d = JSONDecoder()
        d.dateDecodingStrategy = .iso8601
        return d
    }

    private func persist() {
        let encoder = makeEncoder()
        // Global limits
        if let gl = globalLimits, let data = try? encoder.encode(gl) {
            try? data.write(to: Self.globalLimitsURL, options: .atomic)
        } else if globalLimits == nil {
            try? FileManager.default.removeItem(at: Self.globalLimitsURL)
        }

        // Helmet limits — encode as array
        let helmetArray = Array(helmetLimits.values)
        if let data = try? encoder.encode(helmetArray) {
            try? data.write(to: Self.helmetLimitsURL, options: .atomic)
        }

        // Individual limits — encode as array
        let individualArray = Array(individualLimits.values)
        if let data = try? encoder.encode(individualArray) {
            try? data.write(to: Self.individualLimitsURL, options: .atomic)
        }

        // Profiles
        if let data = try? encoder.encode(profiles) {
            try? data.write(to: Self.profilesURL, options: .atomic)
        }

        // Active profile id
        struct ActiveProfileWrapper: Codable { var id: UUID? }
        if let data = try? encoder.encode(ActiveProfileWrapper(id: activeProfileId)) {
            try? data.write(to: Self.activeProfileURL, options: .atomic)
        }
    }

    private func load() {
        let decoder = makeDecoder()
        let fm = FileManager.default

        // Global limits
        if fm.fileExists(atPath: Self.globalLimitsURL.path),
           let data = try? Data(contentsOf: Self.globalLimitsURL) {
            globalLimits = try? decoder.decode(NPLimitsSet.self, from: data)
        }

        // Helmet limits — decode as array, re-key by helmetId
        if fm.fileExists(atPath: Self.helmetLimitsURL.path),
           let data = try? Data(contentsOf: Self.helmetLimitsURL),
           let arr = try? decoder.decode([NPLimitsSet].self, from: data) {
            helmetLimits = Dictionary(uniqueKeysWithValues: arr.compactMap { l in
                guard let serial = l.helmetId else { return nil }
                return (serial, l)
            })
        }

        // Individual limits — decode as array, re-key by individualId
        if fm.fileExists(atPath: Self.individualLimitsURL.path),
           let data = try? Data(contentsOf: Self.individualLimitsURL),
           let arr = try? decoder.decode([NPLimitsSet].self, from: data) {
            individualLimits = Dictionary(uniqueKeysWithValues: arr.compactMap { l in
                guard let uid = l.individualId else { return nil }
                return (uid, l)
            })
        }

        // Profiles
        if fm.fileExists(atPath: Self.profilesURL.path),
           let data = try? Data(contentsOf: Self.profilesURL) {
            profiles = ((try? decoder.decode([NPIndividualProfile].self, from: data)) ?? []).map { p in
                var kept = p
                kept.notes = NPIndividualProfile.cutNotes(p.notes)
                return kept
            }
            // What is stored is reduced to what a profile has (REQ-SW03-32 (a), (c)): decoding drops a field the profile
            // does not have (a date of birth, a creation time) and a note over the bound is cut, and the file is written
            // back so neither stays on the device. The encoder sorts its keys, so a file that needs no change encodes
            // to the same bytes.
            if !profiles.isEmpty, let kept = try? makeEncoder().encode(profiles), kept != data {
                try? kept.write(to: Self.profilesURL, options: .atomic)
            }
        }

        // Active profile id
        struct ActiveProfileWrapper: Codable { var id: UUID? }
        if fm.fileExists(atPath: Self.activeProfileURL.path),
           let data = try? Data(contentsOf: Self.activeProfileURL),
           let wrapper = try? decoder.decode(ActiveProfileWrapper.self, from: data) {
            activeProfileId = wrapper.id
        }
    }

    // MARK: Init

    init() {
        load()
    }
}
