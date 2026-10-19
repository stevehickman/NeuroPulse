import Foundation
@testable import NeurOne

// An isolated `ProtectedDefaults` for a test: its own temporary folder and its own legacy `UserDefaults` suite, so nothing a test
// writes reaches the real stores or another test.
func makeIsolatedProtectedDefaults() -> ProtectedDefaults {
    let directory = FileManager.default.temporaryDirectory
        .appendingPathComponent("ProtectedDefaultsTest-\(UUID().uuidString)", isDirectory: true)
    let legacy = UserDefaults(suiteName: "ProtectedDefaultsTest.\(UUID().uuidString)")!
    return ProtectedDefaults(directory: directory, legacy: legacy)
}
