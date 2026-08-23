import Foundation

/// The shipped protocol library, read from the `.npps` files in
/// `protocols/predefined/` — the single source of truth for every runtime.
///
/// The directory is copied into the app bundle at build time as a folder
/// reference in the Xcode project's Resources phase, so there is nothing to
/// keep in sync by hand. This type previously held the same protocols
/// transcribed into Swift string literals and had drifted twice over: it
/// carried 19 of the library's protocols, and 8 of those were missing the
/// `conditions` / `references` fields added in NP-NPPS-REF-001 Rev 2.
///
/// **Definition files are excluded.** `manifest.json` also lists
/// `00-zones.npps` and `00-conditions.npps`, but this parser has no `zone` or
/// `condition` top-level block — `NPProtocolEntry` is single/composite/limits
/// only. That is a standing gap against NP-NPPS-REF-001 Rev 2 (the Android
/// parser has it too; only the web parser implements them), not something this
/// loader should paper over, so it takes only the files the parser can read.
enum NPBundledProtocols {

    /// Bundle subdirectory the folder reference lands in.
    static let resourceSubdirectory = "predefined"

    private struct Manifest: Decodable {
        let protocols: [String]?
        let composites: [String]?
    }

    /// Protocol and composite file names from `manifest.json`, in manifest order.
    static let manifestFiles: [String] = {
        guard let url = Bundle.main.url(
            forResource: "manifest",
            withExtension: "json",
            subdirectory: resourceSubdirectory
        ) else {
            assertionFailure(
                "predefined/manifest.json is not in the app bundle — the protocols/predefined "
                + "folder reference is missing from the Resources build phase."
            )
            return []
        }
        do {
            let manifest = try JSONDecoder().decode(Manifest.self, from: Data(contentsOf: url))
            return (manifest.protocols ?? []) + (manifest.composites ?? [])
        } catch {
            assertionFailure("predefined/manifest.json could not be decoded: \(error)")
            return []
        }
    }()

    /// Raw NPPS text of every bundled protocol and composite file.
    static let allContents: [String] = {
        manifestFiles.compactMap { name in
            let stem = (name as NSString).deletingPathExtension
            guard let url = Bundle.main.url(
                forResource: stem,
                withExtension: "npps",
                subdirectory: resourceSubdirectory
            ) else {
                assertionFailure("predefined/\(name) is listed in manifest.json but is not in the app bundle.")
                return nil
            }
            return try? String(contentsOf: url, encoding: .utf8)
        }
    }()
}
