import Foundation

// NPPS serialization on iOS: the shared NPPS core (common/npps-core, OI-NPPS-CORE-01), through the C ABI.
//
// The lexer and parser that used to live here are the shared core now (NPPSParser.swift), and so is the writer.
// This file used to be a 450-line hand-written serializer, one of four that drifted from the parser: it wrote `1h`
// (a unit the lexer does not have), left a limits-set name unescaped, and wrote a limits value with a unit the
// parser reads differently. There is one serializer now, and it is in Rust: what the text looks like, and what it
// refuses to write, is decided there and is the same on every runtime. What is left here is the mapping of these
// models to the core's shape (Session/NppsCoreMapping.swift).

// MARK: - NPPS Serializer

struct NPPSSerializer {

    /// The `.npps` text for `entry`, or throws the core's refusal for a model it cannot write (a zone holding an id
    /// that is not a socket, a PBM block that names no zone). Refusing is deliberate: the alternative is a file the
    /// parser then rejects, which loses the protocol on the next load.
    func write(_ entry: NPProtocolEntry) throws -> String {
        try write([entry])
    }

    /// Entries as one `.npps` file: blocks separated by a blank line.
    func write(_ entries: [NPProtocolEntry]) throws -> String {
        let request: [String: Any] = ["items": entries.map { $0.nppsCoreItem() }]
        let out = try NppsCore.serialize(requestJSON: JSONSerialization.data(withJSONObject: request))
        return String(bytes: out, encoding: .utf8) ?? ""
    }

    /// As `write`, for a caller that shows the text (the editor's script pane, an export): a model the core cannot
    /// write comes back as a comment line saying why, which the parser skips, instead of crashing the view.
    func serialize(_ entry: NPProtocolEntry) -> String {
        do {
            return try write(entry)
        } catch {
            return "# This script cannot be written: \(error.localizedDescription)"
        }
    }

    func serializeZone(_ z: NPZoneDefinition) -> String { serialize(.zone(z)) }

    func serializeCondition(_ c: NPConditionDefinition) -> String { serialize(.condition(c)) }

    func serializeLimits(_ limits: NPLimitsSet) -> String { serialize(.limits(limits)) }
}

// MARK: - Round-trip

/// Parse then write: what the app would save for this text.
func nppsRoundTrip(_ text: String) throws -> String {
    var lexer = NPPSLexer(text)
    let tokens = try lexer.tokenize()
    var parser = NPPSParser(tokens)
    let entries = try parser.parse()
    return try NPPSSerializer().write(entries)
}
