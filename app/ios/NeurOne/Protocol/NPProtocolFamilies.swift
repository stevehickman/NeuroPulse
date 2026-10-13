import Foundation

// The protocol library's rules as the app asks them (SWR-PL-02/03/06, SWR-UP-02, SWR-WO-01/02/03, SWR-FM-03…07,
// SWR-HV-04). Nothing is decided here: every answer is the shared core's (common/npps-core/src/library.rs, reached
// through NppsCore.library), so the five apps cannot disagree. This file is the typed shape of the answers, the entry
// summaries the core is given, and the calls. NPProtocolLibrary keeps the state and the screens read it.

// MARK: - Answers

/// What a save met.
enum SaveResult: Equatable {
    case saved
    /// A predefined protocol: never saved over; the way to change it is a clone.
    case readOnly
    /// The protocol's family has been used: nothing was saved, and a clone of the whole family is offered
    /// (SWR-PL-02, SWR-FM-03).
    case refusedFamilyUsed(familyId: String, memberIds: [String])
    /// A variant for this hardware version already exists in the family (SWR-FM-06).
    case refusedVariantExists
}

/// What a delete met.
enum DeleteResult: Equatable {
    case deleted
    case readOnly
    case notFound
    /// A used family is archived, never deleted (SWR-FM-05).
    case refusedFamilyUsed
}

/// What an upload meets before any byte is sent. A refusal is a local decision: nothing is sent and nothing is marked used.
enum UploadGate: Equatable {
    case allow
    /// The protocol has no layout for the helmet's hardware version (SWR-HV-04). The counts say how the first of its
    /// layouts overlaps the helmet's; they are nil while the helmet's layout is not in the table.
    case hardwareMismatch(helmetVersion: Int, protocolVersions: [Int], sharedSockets: Int?,
                          onlyProtocolSockets: Int?, onlyHelmetSockets: Int?)
    /// The clone's content is its source's (SWR-PL-06): upload the source, edit the clone, or cancel.
    case unmodifiedClone(sourceId: UUID)
}

/// The outcomes of an upload that was sent, and the two refusals that are not failed uploads (SWR-UP-02).
enum UploadOutcome: String {
    case confirmed
    case unknown
    case confirmedFailed = "confirmed_failed"
    case notSent = "not_sent"
    case notAFailedUpload = "not_a_failed_upload"
}

// MARK: - Entry helpers

extension NPProtocolEntry {

    /// A protocol's or composite's family id; nil for the other kinds and for a protocol that states none.
    var familyId: String? {
        switch self {
        case .single(let p):    return p.familyId
        case .composite(let c): return c.familyId
        default:                return nil
        }
    }

    /// The key of the family an entry belongs to: its family id, or its own id when it has none (a family of one).
    var familyKey: String { familyId ?? id.uuidString }

    /// A protocol or a composite: the entries a person runs, as opposed to the zone, condition and limits definitions.
    var isRunnable: Bool {
        switch self {
        case .single, .composite: return true
        default:                  return false
        }
    }

    /// The entry with its id and family replaced, as a clone is made; the other kinds are returned unchanged.
    func withIdentity(id newId: UUID, familyId newFamily: String?) -> NPProtocolEntry {
        switch self {
        case .single(var p):
            p.id = newId; p.familyId = newFamily; p.isPredefined = false; p.isReadOnly = false
            return .single(p)
        case .composite(var c):
            c.id = newId; c.familyId = newFamily; c.isPredefined = false; c.isReadOnly = false
            return .composite(c)
        default:
            return self
        }
    }

    func withName(_ newName: String) -> NPProtocolEntry {
        switch self {
        case .single(var p):    p.name = newName; return .single(p)
        case .composite(var c): c.name = newName; return .composite(c)
        default:                return self
        }
    }

    fileprivate var versionText: String {
        switch self {
        case .single(let p):    return p.version
        case .composite(let c): return c.version
        default:                return ""
        }
    }
}
