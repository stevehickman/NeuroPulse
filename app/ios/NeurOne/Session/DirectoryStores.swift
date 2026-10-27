import Foundation

// A folder on disk as the app's two storage roles (SWR-MG-06, SWR-MG-10, issue #569 package 6k): `DirectoryObjectStore` is the
// route for a local computer, a USB drive or a shared folder, and `DirectoryFileStore` is the app's own files for the old
// phone's clear. They use the same file names and raw bytes as Kotlin's (DirectoryStores.kt) and C#'s (DirectoryStores.cs), so
// a folder one platform wrote another can read. Object bytes cross the `ObjectStore` role as hex, as on every platform but
// Kotlin, and are raw on disk.

enum DirectoryStoreError: Error {
    case unsafeName(String)
    case outsideFolder(String)
    case badHex
}

func hexString(_ data: Data) -> String { data.map { String(format: "%02x", $0) }.joined() }

func hexData(_ hex: String) throws -> Data {
    guard hex.count % 2 == 0 else { throw DirectoryStoreError.badHex }
    var bytes = [UInt8]()
    var index = hex.startIndex
    while index < hex.endIndex {
        let next = hex.index(index, offsetBy: 2)
        guard let byte = UInt8(hex[index..<next], radix: 16) else { throw DirectoryStoreError.badHex }
        bytes.append(byte)
        index = next
    }
    return Data(bytes)
}

/// An `ObjectStore` over one folder: each object is a file named by the object. A claim is created with a hard link from a fully
/// written temporary file, so the operating system arbitrates it (exactly one of two simultaneous creates succeeds) and a reader
/// never sees a half-written claim (SWR-MG-07). A folder on a network share is only as atomic as that share's hard links.
struct DirectoryObjectStore: ObjectStore {
    static let tempPrefix = ".tmp-"
    let root: URL

    init(root: URL) throws {
        try FileManager.default.createDirectory(at: root, withIntermediateDirectories: true)
        self.root = root
    }

    /// A name is one path segment of ASCII letters, digits, dot, dash and underscore, and does not start with a dot.
    static func isSafeName(_ name: String) -> Bool {
        let allowed = Set("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789._-")
        return !name.isEmpty && name.count <= 255 && !name.hasPrefix(".") && name.allSatisfy { allowed.contains($0) }
    }

    private func url(_ name: String) throws -> URL {
        guard DirectoryObjectStore.isSafeName(name) else { throw DirectoryStoreError.unsafeName(name) }
        return root.appendingPathComponent(name)
    }

    private func writeTemp(_ data: Data) throws -> URL {
        let temp = root.appendingPathComponent(DirectoryObjectStore.tempPrefix + UUID().uuidString)
        try data.write(to: temp)
        return temp
    }

    func get(_ name: String) async throws -> String? {
        let file = try url(name)
        guard FileManager.default.fileExists(atPath: file.path) else { return nil }
        return hexString(try Data(contentsOf: file))
    }

    func put(_ name: String, _ data: String) async throws {
        let file = try url(name)
        let temp = try writeTemp(try hexData(data))
        defer { try? FileManager.default.removeItem(at: temp) }
        if FileManager.default.fileExists(atPath: file.path) {
            _ = try FileManager.default.replaceItemAt(file, withItemAt: temp)
        } else {
            try FileManager.default.moveItem(at: temp, to: file)
        }
    }

    func createIfAbsent(_ name: String, _ data: String) async throws -> Bool {
        let file = try url(name)
        let temp = try writeTemp(try hexData(data))
        defer { try? FileManager.default.removeItem(at: temp) }
        do {
            try FileManager.default.linkItem(at: temp, to: file)
            return true
        } catch let error as CocoaError where error.code == .fileWriteFileExists {
            return false
        }
    }

    func delete(_ name: String) async throws {
        let file = try url(name)
        if FileManager.default.fileExists(atPath: file.path) { try FileManager.default.removeItem(at: file) }
    }

    func list(_ prefix: String) async throws -> [String] {
        try FileManager.default.contentsOfDirectory(atPath: root.path)
            .filter { $0.hasPrefix(prefix) && !$0.hasPrefix(DirectoryObjectStore.tempPrefix) }
            .sorted()
    }

    func freeSpace() async throws -> Int64? {
        let values = try root.resourceValues(forKeys: [.volumeAvailableCapacityForImportantUsageKey])
        return values.volumeAvailableCapacityForImportantUsage
    }
}

/// A `FileStore` over one folder tree: `list` reports every regular file under it with its path relative to the folder (forward
/// slashes), and no path may leave the folder. `write` overwrites in place and `sync` forces the file to storage.
struct DirectoryFileStore: FileStore {
    let root: URL

    init(root: URL) { self.root = root.standardizedFileURL }

    private func resolve(_ path: String) throws -> URL {
        // An absolute path would otherwise be read as relative to the folder; the other platforms refuse it, and so does this.
        let file = root.appendingPathComponent(path).standardizedFileURL
        guard !path.hasPrefix("/"), file.path.hasPrefix(root.path + "/") else { throw DirectoryStoreError.outsideFolder(path) }
        return file
    }

    func list() async throws -> [StoredFile] {
        guard let walker = FileManager.default.enumerator(
            at: root, includingPropertiesForKeys: [.isRegularFileKey, .fileSizeKey]
        ) else { return [] }
        // The enumerator may report /private/var/... for a folder given as /var/..., so both ends are resolved before comparing.
        let base = root.resolvingSymlinksInPath().path
        var found: [StoredFile] = []
        for case let url as URL in walker {
            let values = try url.resourceValues(forKeys: [.isRegularFileKey, .fileSizeKey])
            guard values.isRegularFile == true else { continue }
            let relative = String(url.resolvingSymlinksInPath().path.dropFirst(base.count + 1))
            found.append(StoredFile(path: relative, size: Int64(values.fileSize ?? 0)))
        }
        return found.sorted { $0.path < $1.path }
    }

    func write(_ path: String, offset: Int64, bytes: Data) async throws {
        let handle = try FileHandle(forUpdating: try resolve(path))
        defer { try? handle.close() }
        try handle.seek(toOffset: UInt64(offset))
        try handle.write(contentsOf: bytes)
    }

    func sync(_ path: String) async throws {
        let handle = try FileHandle(forUpdating: try resolve(path))
        defer { try? handle.close() }
        try handle.synchronize()
    }

    func delete(_ path: String) async throws {
        let file = try resolve(path)
        if FileManager.default.fileExists(atPath: file.path) { try FileManager.default.removeItem(at: file) }
    }
}
