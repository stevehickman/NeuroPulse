import Foundation
import Combine

// MARK: - Protocol Availability

enum ProtocolAvailability: Equatable {
    case available
    case unavailable(missingModalities: [NPModalityType])
    /// EEG-neurofeedback protocol blocked because the user declined the BIPA
    /// biometric written release (ISC-90). Distinct from a hardware shortfall so
    /// the message can point the user at consent, not at a missing module.
    case eegConsentRequired

    var isAvailable: Bool {
        if case .available = self { return true }
        return false
    }

    var unavailableReason: String? {
        switch self {
        case .available:
            return nil
        case .unavailable(let missing):
            if missing.isEmpty { return String(localized: "PROTOCOL_AVAIL_NO_DEVICE") }
            let names = missing.map { $0.displayName }.joined(separator: ", ")
            return String(format: String(localized: "PROTOCOL_AVAIL_REQUIRES"), names)
        case .eegConsentRequired:
            // Localized ISC-90 message — shared with SessionView's EEG-unavailable card.
            return String(localized: "SESSION_EEG_UNAVAILABLE_BODY")
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
    @Published var validationResults: [UUID: NPValidationResult] = [:]

    /// Whether the user has granted the BIPA biometric written release. EEG-dependent
    /// protocols are blocked until this is true (ISC-90). Seeded from the same
    /// `np.onboarding.bipa-accepted` UserDefaults key the views read via `@AppStorage`;
    /// views push live changes in via `updateEEGConsent(_:)`.
    @Published var eegConsentGranted: Bool =
        UserDefaults.standard.bool(forKey: "np.onboarding.bipa-accepted")

    // MARK: Family state (SWR-FM-03…07, SWR-WO-02)

    /// Families that have been used: one bit per family, set by the first confirmed or unknown upload of any variant,
    /// never cleared (SWR-WO-02). Stored as family ids and nothing else.
    @Published private(set) var usedFamilies: Set<String> = []
    /// Families the warranty owner archived: kept, no longer offered (SWR-WO-01, SWR-FM-05).
    @Published private(set) var archivedFamilies: Set<String> = []
    /// For each clone, the protocol it was made from (SWR-PL-06). Provenance only.
    private(set) var cloneSources: [String: String] = [:]

    static let usedFamiliesKey = "np.used-families"
    static let archivedFamiliesKey = "np.archived-families"
    static let cloneSourcesKey = "np.clone-sources"

    // MARK: Bundled (read-only) protocols loaded from embedded .npps content

    private(set) var bundledProtocols: [NPProtocolEntry] = []

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
        loadBundledProtocols()
        loadFromDisk()
        loadFamilyState()
        publishZoneNamespace()
    }

    /// Duplicate zone/condition names between the shipped library and the
    /// user's own scripts. Empty in the normal case; a non-empty list means
    /// those names are undefined and any protocol referencing one will not
    /// resolve.
    private(set) var zoneNamespaceErrors: [String] = []

    /// Fold the user's own entries into the namespace zones resolve against.
    ///
    /// A user may define a `zone` block in their own script (NP-NPPS-REF-001 §8
    /// *User-defined zones*), and it has to resolve exactly as a shipped zone
    /// does. It reaches `NPZoneRegistry` the same way a shipped one does: parsed
    /// out of NPPS text, never authored anywhere else.
    ///
    /// A user zone colliding with a shipped name is a duplicate definition, so
    /// §1.6 applies unchanged — the name binds to neither and the collision is
    /// an error. That is deliberate: silently letting either one win would make
    /// which sockets get dosed depend on load order.
    private func publishZoneNamespace() {
        // `bundledProtocols` is deliberately runnable entries only, so the zone
        // and condition blocks are not in it. Take the bundled namespace's full
        // entry list instead — the definitions are exactly what is needed here.
        let build = buildNamespace(NPBundledProtocols.namespace.entries + userProtocols)
        NPZoneRegistry.use(build.namespace)
        zoneNamespaceErrors = build.errors
    }

    // MARK: Computed all-protocols list

    var allProtocols: [NPProtocolEntry] {
        bundledProtocols + userProtocols
    }

    // MARK: Availability

    func availability(for entry: NPProtocolEntry) -> ProtocolAvailability {
        // BIPA gate (ISC-90) runs before the hardware/tier checks: an EEG-dependent
        // protocol is blocked on declined consent regardless of what is connected, so
        // the user sees the actionable "brainwave data consent" message.
        if !eegConsentGranted, isEEGDependent(entry) {
            return .eegConsentRequired
        }

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

    /// Resolves EEG-dependency for any entry, expanding composites against the
    /// library — a composite is EEG-dependent if ANY member protocol is (ISC-2).
    func isEEGDependent(_ entry: NPProtocolEntry) -> Bool {
        if case .composite(let comp) = entry {
            return comp.layers.contains { layer in
                findSingle(named: layer.protocolName)?.isEEGDependent ?? false
            }
        }
        return entry.isEEGDependent
    }

    /// Push the current BIPA consent state (from `np.onboarding.bipa-accepted`) into
    /// the library so protocol availability reflects it live. Called by the views on
    /// appear and on change (ISC-8).
    func updateEEGConsent(_ granted: Bool) {
        if eegConsentGranted != granted { eegConsentGranted = granted }
    }

    // MARK: Validation integration

    /// Revalidates all protocols using the current resolved limits from limitsStore.
    func revalidateAll(using limitsStore: NPLimitsStore) {
        let validator = limitsStore.makeValidator()
        var results: [UUID: NPValidationResult] = [:]
        for entry in allProtocols {
            results[entry.id] = validator.validate(entry, resolving: self)
        }
        validationResults = results
    }

    /// Returns the cached validation result for an entry, computing it on demand if missing.
    func validationResult(for entry: NPProtocolEntry, limitsStore: NPLimitsStore) -> NPValidationResult {
        if let cached = validationResults[entry.id] { return cached }
        let validator = limitsStore.makeValidator()
        let result = validator.validate(entry, resolving: self)
        validationResults[entry.id] = result
        return result
    }

    /// Returns (errors, warnings) counts for a protocol entry, or (0, 0) if not validated.
    func issueCount(for entry: NPProtocolEntry) -> (errors: Int, warnings: Int) {
        guard let result = validationResults[entry.id] else { return (0, 0) }
        return (result.errors.count, result.warnings.count)
    }

    // MARK: Read-only guards

    /// Returns true if the entry can be edited by the user.
    func canEdit(_ entry: NPProtocolEntry) -> Bool {
        !entry.isReadOnly
    }

    /// Returns true if the entry can be deleted by the user.
    func canDelete(_ entry: NPProtocolEntry) -> Bool {
        !entry.isReadOnly
    }

    // MARK: Mutations

    /// Save `entry`. The rules are the core's (SWR-PL-02/03, SWR-FM-03/06): a predefined protocol is never saved over; a
    /// protocol of a used family is refused, nothing is written, and a clone of the whole family is offered; an unused
    /// one saves in place. A new protocol joining a used family is a new variant, allowed only for a hardware version
    /// the family has no variant for.
    @discardableResult
    func save(_ entry: NPProtocolEntry) -> SaveResult {
        if bundledProtocols.contains(where: { $0.id == entry.id }) { return .readOnly }
        let id = entry.id
        if let idx = userProtocols.firstIndex(where: { $0.id == id }) {
            let check = NPLibraryRules.saveCheck(familySummaries(withVersions: false), id: id.uuidString)
            if (check["allowed"] as? Bool) != true {
                return .refusedFamilyUsed(familyId: (check["familyId"] as? String) ?? entry.familyKey,
                                          memberIds: (check["members"] as? [String]) ?? [])
            }
            userProtocols[idx] = entry
        } else {
            if entry.isRunnable, entry.familyId != nil, usedFamilies.contains(entry.familyKey) {
                let version = NPLibraryRules.supportedVersions(entry).first ?? 1
                let check = NPLibraryRules.addVariantCheck(familySummaries(withVersions: true),
                                                           familyId: entry.familyKey, hardwareVersion: version)
                if (check["allowed"] as? Bool) != true { return .refusedVariantExists }
            }
            userProtocols.append(entry)
        }
        saveToDisk()
        publishZoneNamespace()
        return .saved
    }

    /// Delete a user protocol. A used family is archived, never deleted (SWR-FM-05).
    @discardableResult
    func delete(_ id: UUID) -> DeleteResult {
        if bundledProtocols.contains(where: { $0.id == id }) { return .readOnly }
        guard userProtocols.contains(where: { $0.id == id }) else { return .notFound }
        let check = NPLibraryRules.deleteCheck(familySummaries(withVersions: false), id: id.uuidString)
        if (check["allowed"] as? Bool) != true { return .refusedFamilyUsed }
        userProtocols.removeAll { $0.id == id }
        cloneSources[id.uuidString] = nil
        saveToDisk()
        saveFamilyState()
        publishZoneNamespace()
        return .deleted
    }

    // MARK: Families (SWR-FM-03…07, SWR-PL-02)

    /// Whether `entry`'s family has been used. The use of any variant marks the family (SWR-FM-03).
    func isUsed(_ entry: NPProtocolEntry) -> Bool { usedFamilies.contains(entry.familyKey) }

    private func familySummaries(withVersions: Bool) -> [[String: Any]] {
        NPLibraryRules.summaries(allProtocols, used: usedFamilies, withVersions: withVersions)
    }

    /// Clone the whole family `id` belongs to: a new family from the moment it is made, a copy of every variant, none of
    /// them used, fully editable, the source recorded for provenance only (SWR-FM-03). Each copy's name is the source's
    /// with a copy suffix added, made unique, because a protocol is referred to by name.
    @discardableResult
    func cloneFamily(_ id: UUID, copyName: (String) -> String) -> [NPProtocolEntry] {
        guard let entry = allProtocols.first(where: { $0.id == id && $0.isRunnable }) else { return [] }
        let members = allProtocols.filter { $0.isRunnable && $0.familyKey == entry.familyKey }
        let newFamily = UUID().uuidString
        let newIds = members.map { _ in UUID() }
        // The core checks the count, that the family is new and that every id is new, before anything is made.
        _ = NPLibraryRules.cloneFamily(familySummaries(withVersions: false), id: id.uuidString,
                                       newFamilyId: newFamily, newIds: newIds.map(\.uuidString))
        var taken = Set(allProtocols.map(\.name))
        var copies: [NPProtocolEntry] = []
        for (member, newId) in zip(members, newIds) {
            let name = uniqueCopyName(member.name, taken: taken, copyName: copyName)
            taken.insert(name)
            var copy = member.duplicated(newName: name)
            copy = copy.withIdentity(id: newId, familyId: newFamily)
            copies.append(copy)
        }
        for (member, copy) in zip(members, copies) {
            userProtocols.append(copy)
            cloneSources[copy.id.uuidString] = member.id.uuidString
        }
        saveToDisk()
        saveFamilyState()
        publishZoneNamespace()
        return copies
    }

    /// The edit that a used family refused, kept: clone the whole family, then save `edited` over its own copy. The
    /// copy's id, family and read-only state are the clone's; the content is the person's edit, and the name is theirs
    /// when they changed it, else the clone's copy name. Returns the saved copy, or nil when `edited` is not in the library.
    func saveAsClone(_ edited: NPProtocolEntry, copyName: (String) -> String) -> NPProtocolEntry? {
        guard let original = allProtocols.first(where: { $0.id == edited.id && $0.isRunnable }) else {
            return saveAsNewFamily(edited, copyName: copyName)
        }
        let copies = cloneFamily(edited.id, copyName: copyName)
        guard let copy = copies.first(where: { cloneSources[$0.id.uuidString] == edited.id.uuidString }) else { return nil }
        let renamed = edited.name != original.name
        let merged = edited.withIdentity(id: copy.id, familyId: copy.familyId)
            .withName(renamed ? edited.name : copy.name)
        save(merged) // the copy is a new, unused family, so this saves in place
        return merged
    }

    /// An edit that is not in the library yet and joined a used family's line: kept as a new family of its own, never
    /// as a variant of the used one.
    private func saveAsNewFamily(_ edited: NPProtocolEntry, copyName: (String) -> String) -> NPProtocolEntry? {
        guard edited.isRunnable else { return nil }
        let taken = Set(allProtocols.map(\.name))
        let name = taken.contains(edited.name) ? uniqueCopyName(edited.name, taken: taken, copyName: copyName) : edited.name
        let merged = edited.withIdentity(id: UUID(), familyId: UUID().uuidString).withName(name)
        save(merged)
        return merged
    }

    private func uniqueCopyName(_ base: String, taken: Set<String>, copyName: (String) -> String) -> String {
        var candidate = copyName(base)
        var n = 2
        while taken.contains(candidate) {
            candidate = copyName("\(base) \(n)")
            n += 1
        }
        return candidate
    }

    /// The variants of `entry`'s family that fit a helmet of `helmetVersion`, latest first; empty means the person
    /// decides whether to create one (SWR-HV-04).
    func variantsFor(_ entry: NPProtocolEntry, helmetVersion: Int) -> [NPProtocolEntry] {
        let ids = NPLibraryRules.variantFor(familySummaries(withVersions: true), familyId: entry.familyKey, helmetVersion: helmetVersion)
        return ids.compactMap { id in allProtocols.first { $0.id.uuidString == id } }
    }

    // MARK: Archive and the offered list (SWR-WO-01, SWR-FM-05)

    /// Archive the family of `entry`: it drops out of the offered list and every variant is kept.
    func archiveFamily(_ entry: NPProtocolEntry) {
        if archivedFamilies.insert(entry.familyKey).inserted { saveFamilyState() }
    }

    func unarchiveFamily(_ entry: NPProtocolEntry) {
        if archivedFamilies.remove(entry.familyKey) != nil { saveFamilyState() }
    }

    func isArchived(_ entry: NPProtocolEntry) -> Bool { archivedFamilies.contains(entry.familyKey) }

    /// The warranty owner's set: every protocol whose family is not archived.
    var offeredProtocols: [NPProtocolEntry] {
        let offered = NPLibraryRules.offered(familySummaries(withVersions: false), archivedFamilies: archivedFamilies)
        return allProtocols.filter { !$0.isRunnable || offered.contains($0.id.uuidString) }
    }

    // MARK: Upload (SWR-PL-06, SWR-HV-04, SWR-UP-02)

    /// `entry`'s identity as the core defines it, or nil for a kind that has none.
    func contentHash(_ entry: NPProtocolEntry) -> String? { NPLibraryRules.contentHash(entry) }

    /// What an upload of `entry` to a helmet of `helmetHardwareVersion` meets before any byte is sent. The hardware
    /// refusal comes first, then the unmodified clone; an allowed upload is then sent and its result passed to
    /// `recordUploadResult`.
    func uploadGate(_ entry: NPProtocolEntry, helmetHardwareVersion: Int) -> UploadGate {
        guard let hash = contentHash(entry) else { return .allow }
        let source = cloneSources[entry.id.uuidString].flatMap { sid in allProtocols.first { $0.id.uuidString == sid } }
        let answer = NPLibraryRules.uploadGate(helmetVersion: helmetHardwareVersion,
                                               protocolVersions: NPLibraryRules.supportedVersions(entry),
                                               contentHash: hash, sourceHash: source.flatMap { contentHash($0) })
        switch answer["reason"] as? String {
        case "hardware_mismatch":
            let diff = NPLibraryRules.overlap(answer)
            return .hardwareMismatch(
                helmetVersion: helmetHardwareVersion,
                protocolVersions: (answer["protocolVersions"] as? [Any])?.compactMap { ($0 as? NSNumber)?.intValue } ?? [],
                sharedSockets: diff?.shared,
                onlyProtocolSockets: diff?.onlyProtocol,
                onlyHelmetSockets: diff?.onlyHelmet)
        case "unmodified_clone":
            if let source { return .unmodifiedClone(sourceId: source.id) }
            return .allow
        default:
            return .allow
        }
    }

    /// Record how an upload that was sent ended. Confirmed and unknown mark `entry`'s family used; a confirmed failure,
    /// a refusal before sending and a helmet refusal for hardware version, unmodified clone or tier do not (SWR-UP-02).
    /// `helmetStatus` is the hub's UPLOAD_CHECK verdict, "accepted" or "refused", or nil when it never answered.
    func recordUploadResult(_ entry: NPProtocolEntry, sentHash: String, helmetStatus: String?, helmetHash: String?,
                            helmetRefusal: String? = nil, refusedBeforeSend: Bool = false) -> UploadOutcome {
        let answer = NPLibraryRules.uploadResult(sentHash: sentHash, helmetStatus: helmetStatus, helmetHash: helmetHash,
                                                 helmetRefusal: helmetRefusal, refusedBeforeSend: refusedBeforeSend)
        if (answer["markUsed"] as? Bool) == true { markUsed(entry) }
        return (answer["outcome"] as? String).flatMap(UploadOutcome.init(rawValue:)) ?? .unknown
    }

    /// The used flag is one bit per family and is never cleared (SWR-WO-02).
    private func markUsed(_ entry: NPProtocolEntry) {
        if usedFamilies.insert(entry.familyKey).inserted { saveFamilyState() }
    }

    // MARK: Family state persistence

    private func loadFamilyState() {
        let defaults = UserDefaults.standard
        // A used record is the family id and nothing else; a stored value the core does not recognise is dropped.
        let used = (defaults.array(forKey: Self.usedFamiliesKey) as? [String]) ?? []
        usedFamilies = Set(used.filter { NPLibraryRules.isValidUsedRecord(NPLibraryRules.usedRecord(familyId: $0)) })
        archivedFamilies = Set((defaults.array(forKey: Self.archivedFamiliesKey) as? [String]) ?? [])
        cloneSources = (defaults.dictionary(forKey: Self.cloneSourcesKey) as? [String: String]) ?? [:]
    }

    private func saveFamilyState() {
        let defaults = UserDefaults.standard
        defaults.set(Array(usedFamilies).sorted(), forKey: Self.usedFamiliesKey)
        defaults.set(Array(archivedFamilies).sorted(), forKey: Self.archivedFamiliesKey)
        defaults.set(cloneSources, forKey: Self.cloneSourcesKey)
    }

    // MARK: Script export / import

    /// The script for `entry`, carrying the used flag when its family is used (SWR-WO-03). The flag is not part of the
    /// content hash.
    func exportScript(_ entry: NPProtocolEntry) -> String {
        NPPSSerializer().serialize(entry, used: isUsed(entry))
    }

    /// Read the first entry of `text`. An entry that arrives flagged used makes its family used, and one that arrives
    /// unflagged but whose content is already used here stays used; an import never clears the flag (SWR-WO-03).
    func importScript(_ text: String) throws -> NPProtocolEntry {
        var lexer = NPPSLexer(text)
        let tokens = try lexer.tokenize()
        var parser = NPPSParser(tokens)
        let entries = try parser.parse()
        guard let first = entries.first else {
            throw NPPSError(message: String(localized: "LIBRARY_NO_PROTOCOL_FOUND_IN_SCRIPT"), line: 0)
        }
        var flagged = false
        if let parsed = try? JSONSerialization.jsonObject(with: NppsCore.parse(text)) as? [String: Any],
           let raw = (parsed["entries"] as? [[String: Any]])?.first,
           let body = (raw["protocol"] ?? raw["composite"]) as? [String: Any] {
            flagged = (body["isUsed"] as? Bool) == true
        }
        let usedHashes = allProtocols.filter { isUsed($0) }.compactMap { contentHash($0) }
        let merged = NPLibraryRules.mergeImport(usedHashes: usedHashes,
                                                incoming: [NPImportedEntry(id: first.id.uuidString, hash: contentHash(first), used: flagged)])
        if merged[first.id.uuidString] == true { markUsed(first) }
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

    // MARK: Bundled protocol loading

    /// The shipped protocols and composites. Zone and condition definitions load
    /// into the same namespace (NP-NPPS-REF-001 §1.6) but are not library items:
    /// they are referenced by name, never run, so they are filtered out here
    /// rather than shown in a protocol list.
    private func loadBundledProtocols() {
        bundledProtocols = NPBundledProtocols.namespace.runnableEntries
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
