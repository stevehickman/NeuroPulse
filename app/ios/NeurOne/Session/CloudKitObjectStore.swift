import CloudKit
import Foundation

// The iCloud route for the migration and backup flows, through the native CloudKit framework (SWR-MG-11, issue #566,
// docs/np_sw_cloudstore_001.md §4b). It is the Swift counterpart of Kotlin's CloudKitObjectStore (common/cloudstore), which reaches
// the same container through CloudKit Web Services: both write one record per object, type `NpObject`, with the bytes in one Asset
// field `data`, and name the record by the same percent-encoding, so a record one wrote the other reads. Object bytes cross the
// `ObjectStore` role as hex, as on every platform but Kotlin.

enum CloudKitStoreError: Error, Equatable {
    case emptyObject
    case nameTooLong(String)
    case noAsset(String)
    case noResult(String)
}

/// The five calls the store needs of CloudKit, so its rules (names, hex, the claim's two outcomes) are tested without an iCloud
/// account. `CKDatabaseRecords` is the one real implementation.
protocol CloudKitRecords: Sendable {
    /// Create the record if none has this name. True when this call created it, false when one exists; anything else throws.
    func create(_ recordName: String, _ data: Data) async throws -> Bool
    /// Write the record, replacing any earlier one.
    func replace(_ recordName: String, _ data: Data) async throws
    /// Remove the record. Not an error when there is none.
    func remove(_ recordName: String) async throws
    /// The record's bytes, or nil when there is none.
    func fetch(_ recordName: String) async throws -> Data?
    /// The names of every record of the type.
    func names() async throws -> [String]
}

/// An `ObjectStore` over one iCloud container's private database. Whether `createIfAbsent` is arbitrated atomically by CloudKit is
/// **not established**: the documentation says a save with `ifServerRecordUnchanged` is refused with `serverRecordChanged` when the
/// server record has a newer change tag, and does not say what it does for a new record whose ID is already in use (UC-111). The
/// contract test against a real container (SWR-MG-11) is what would show it.
struct CloudKitObjectStore: ObjectStore {
    static let recordType = "NpObject"
    static let field = "data"
    /// A name that would pass this many characters once encoded is refused. The figure is from memory of CKRecord.ID, not read (UC-110).
    static let maxNameLength = 255

    let records: CloudKitRecords

    /// The private database of the container with this identifier (it begins `iCloud.`). The app needs the iCloud capability for it.
    static func privateDatabase(containerIdentifier: String) -> CloudKitObjectStore {
        CloudKitObjectStore(records: CKDatabaseRecords(database: CKContainer(identifier: containerIdentifier).privateCloudDatabase))
    }

    /// The record name for an object name: everything outside A-Z a-z 0-9 `-` `_` `.` `~` as `%XX` over the UTF-8 bytes, and a leading
    /// `_` too (a record name may not start with one, from memory). The same rule as Kotlin's `recordName`.
    static func recordName(_ name: String) throws -> String {
        let hex = Array("0123456789ABCDEF")
        var out = ""
        for (index, byte) in name.utf8.enumerated() {
            let unreserved = (byte >= 0x41 && byte <= 0x5A) || (byte >= 0x61 && byte <= 0x7A) || (byte >= 0x30 && byte <= 0x39) ||
                byte == 0x2D || byte == 0x2E || byte == 0x5F || byte == 0x7E
            if unreserved && !(index == 0 && byte == 0x5F) {
                out.append(Character(UnicodeScalar(byte)))
            } else {
                out.append("%")
                out.append(hex[Int(byte >> 4)])
                out.append(hex[Int(byte & 15)])
            }
        }
        guard out.count <= maxNameLength else { throw CloudKitStoreError.nameTooLong(name) }
        return out
    }

    /// The reverse of `recordName`; a name that is not valid percent-encoding stays as it is.
    static func objectName(_ recordName: String) -> String { recordName.removingPercentEncoding ?? recordName }

    private func bytes(_ hex: String) throws -> Data {
        let data = try hexData(hex)
        guard !data.isEmpty else { throw CloudKitStoreError.emptyObject }
        return data
    }

    func get(_ name: String) async throws -> String? {
        guard let data = try await records.fetch(try CloudKitObjectStore.recordName(name)) else { return nil }
        return hexString(data)
    }

    func put(_ name: String, _ data: String) async throws {
        try await records.replace(try CloudKitObjectStore.recordName(name), try bytes(data))
    }

    func createIfAbsent(_ name: String, _ data: String) async throws -> Bool {
        try await records.create(try CloudKitObjectStore.recordName(name), try bytes(data))
    }

    func delete(_ name: String) async throws {
        try await records.remove(try CloudKitObjectStore.recordName(name))
    }

    func list(_ prefix: String) async throws -> [String] {
        try await records.names().map(CloudKitObjectStore.objectName).filter { $0.hasPrefix(prefix) }.sorted()
    }

    /// CloudKit has no quota call for the private database in the pages read, so the free space is unknown.
    func freeSpace() async throws -> Int64? { nil }
}

/// `CloudKitRecords` over a `CKDatabase`. Not run against iCloud (no account here); `isExists` and `isUnknownItem` are the parts that
/// are tested.
final class CKDatabaseRecords: CloudKitRecords, @unchecked Sendable {
    private let database: CKDatabase

    init(database: CKDatabase) { self.database = database }

    /// `serverRecordChanged`: what a save with `ifServerRecordUnchanged` reports when the server's record is newer (Apple's savePolicy
    /// and error pages, read 2026-10-28), read here as "a record with this name exists".
    static func isExists(_ error: Error) -> Bool { (error as? CKError)?.code == .serverRecordChanged }

    static func isUnknownItem(_ error: Error) -> Bool { (error as? CKError)?.code == .unknownItem }

    private func stage(_ recordName: String, _ data: Data) throws -> (CKRecord, URL) {
        let file = FileManager.default.temporaryDirectory.appendingPathComponent("np-ck-\(UUID().uuidString)")
        try data.write(to: file)
        let record = CKRecord(recordType: CloudKitObjectStore.recordType, recordID: CKRecord.ID(recordName: recordName))
        record[CloudKitObjectStore.field] = CKAsset(fileURL: file)
        return (record, file)
    }

    func create(_ recordName: String, _ data: Data) async throws -> Bool {
        let (record, file) = try stage(recordName, data)
        defer { try? FileManager.default.removeItem(at: file) }
        let outcome = try await database.modifyRecords(
            saving: [record], deleting: [], savePolicy: .ifServerRecordUnchanged, atomically: false
        )
        guard let saved = outcome.saveResults[record.recordID] else { throw CloudKitStoreError.noResult(recordName) }
        switch saved {
        case .success: return true
        case .failure(let error):
            if CKDatabaseRecords.isExists(error) { return false }
            throw error
        }
    }

    func replace(_ recordName: String, _ data: Data) async throws {
        let (record, file) = try stage(recordName, data)
        defer { try? FileManager.default.removeItem(at: file) }
        let outcome = try await database.modifyRecords(saving: [record], deleting: [], savePolicy: .allKeys, atomically: false)
        guard let saved = outcome.saveResults[record.recordID] else { throw CloudKitStoreError.noResult(recordName) }
        _ = try saved.get()
    }

    func remove(_ recordName: String) async throws {
        let id = CKRecord.ID(recordName: recordName)
        let outcome = try await database.modifyRecords(saving: [], deleting: [id], atomically: false)
        guard let deleted = outcome.deleteResults[id] else { throw CloudKitStoreError.noResult(recordName) }
        do {
            try deleted.get()
        } catch where CKDatabaseRecords.isUnknownItem(error) {
            return
        }
    }

    func fetch(_ recordName: String) async throws -> Data? {
        do {
            let record = try await database.record(for: CKRecord.ID(recordName: recordName))
            guard let url = (record[CloudKitObjectStore.field] as? CKAsset)?.fileURL else { throw CloudKitStoreError.noAsset(recordName) }
            return try Data(contentsOf: url)
        } catch where CKDatabaseRecords.isUnknownItem(error) {
            return nil
        }
    }

    func names() async throws -> [String] {
        let query = CKQuery(recordType: CloudKitObjectStore.recordType, predicate: NSPredicate(value: true))
        var found: [String] = []
        var page = try await database.records(matching: query, desiredKeys: [])
        while true {
            for (id, result) in page.matchResults {
                _ = try result.get()
                found.append(id.recordName)
            }
            guard let cursor = page.queryCursor else { break }
            page = try await database.records(continuingMatchFrom: cursor, desiredKeys: [])
        }
        return found
    }
}
