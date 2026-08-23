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
/// **Definition files load first.** `manifest.json` lists `00-zones.npps` and
/// `00-conditions.npps` before the protocols, and they are loaded in that order
/// so a protocol's zone and condition references resolve regardless of file
/// order (NP-NPPS-REF-001 §1.6). The parser gained `zone` and `condition`
/// top-level blocks in Rev 10; before that it raised on them and this loader
/// had to skip both files.
enum NPBundledProtocols {

    /// Bundle subdirectory the folder reference lands in.
    static let resourceSubdirectory = "predefined"

    private struct Manifest: Decodable {
        let zones: [String]?
        let conditions: [String]?
        let protocols: [String]?
        let composites: [String]?
    }

    /// Every file `manifest.json` lists, definitions first: zones, conditions,
    /// then protocols and composites.
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
            return (manifest.zones ?? []) + (manifest.conditions ?? [])
                + (manifest.protocols ?? []) + (manifest.composites ?? [])
        } catch {
            assertionFailure("predefined/manifest.json could not be decoded: \(error)")
            return []
        }
    }()

    /// Protocol and composite file names only — what a protocol library lists.
    static var protocolFiles: [String] {
        let defs = Set(definitionFiles)
        return manifestFiles.filter { !defs.contains($0) }
    }

    /// Zone and condition definition file names, in load order.
    static let definitionFiles: [String] = {
        guard let url = Bundle.main.url(
            forResource: "manifest", withExtension: "json", subdirectory: resourceSubdirectory
        ), let manifest = try? JSONDecoder().decode(Manifest.self, from: Data(contentsOf: url))
        else { return [] }
        return (manifest.zones ?? []) + (manifest.conditions ?? [])
    }()

    /// Everything parsed into one namespace, with zone and condition definitions
    /// resolved. Cross-reference errors are reported by
    /// `validateNamespaceReferences` rather than thrown, so one bad reference
    /// does not cost the caller the whole library.
    static let namespace: NPNamespace = {
        let entries = allContents.flatMap { content -> [NPProtocolEntry] in
            do {
                var lexer = NPPSLexer(content)
                var parser = NPPSParser(try lexer.tokenize())
                return try parser.parse()
            } catch {
                assertionFailure("bundled .npps failed to parse: \(error)")
                return []
            }
        }
        return buildNamespace(entries).namespace
    }()

    /// Raw NPPS text of every bundled file, definitions first.
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
