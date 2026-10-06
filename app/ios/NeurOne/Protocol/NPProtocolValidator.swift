import Foundation

// Protocol validation on iOS: the shared NPPS core (common/npps-core, OI-NPPS-CORE-01), through the C ABI.
//
// This used to be a 1,600-line hand-written validator, the original of three (Android ported it; the web one
// lacked the session-duration, dose and interlock checks this one had, and this one lacked the layer-scale and
// TMS-with-electrical-stimulation checks the web one had). There is one validator now, and it is in Rust, and it
// has every check any of them had. What is left here is the mapping of these models to the core's shape
// (Session/NppsCoreMapping.swift) and the words: the core returns locale keys and arguments, never text, and
// `NPValidationText` resolves them with this app's own catalog.

// MARK: - Validation result types

struct NPValidationIssue: Identifiable {
    var id = UUID()
    var severity: Severity
    var modality: NPModalityType?          // nil = protocol-level issue
    var parameterKey: String               // camelCase field name e.g. "intensityMilliamps"
    var parameterDisplayName: String       // e.g. "Intensity"
    var actualValueDescription: String     // e.g. "1.5 mA"
    var limitValueDescription: String      // e.g. "1.0 mA (Hardware Limit)"
    var limitSource: NPLimitSource
    var message: String

    enum Severity { case error, warning }
}

struct NPValidationResult {
    var issues: [NPValidationIssue] = []
    var isValid: Bool { !issues.contains { $0.severity == .error } }
    var hasWarnings: Bool { issues.contains { $0.severity == .warning } }
    var errors: [NPValidationIssue] { issues.filter { $0.severity == .error } }
    var warnings: [NPValidationIssue] { issues.filter { $0.severity == .warning } }

    /// "1.0 mA (Hardware Limit)" — the limit value and where it came from.
    /// Composed through a key so a locale can reorder or re-punctuate the pair.
    static func limitLabel(_ limit: String, _ source: NPLimitSource) -> String {
        String(format: String(localized: "VALIDATE_LIMIT_WITH_SOURCE_APPLE"),
               limit, String(describing: source))
    }
}

// MARK: - Words

/// How a core message becomes text. The core returns a plain string (a value or a unit label) or
/// `{"key": "VALIDATE_…", "args": [message…]}`, where an argument can itself be a message (a layer's prefix wraps the
/// issue it prefixes). A key is resolved against this app's String Catalog, and `{0}` in the canonical text is the
/// catalog's `%1$@`, so the arguments are passed as strings in order.
enum NPValidationText {
    static func render(_ message: Any?) -> String {
        if let text = message as? String { return text }
        guard let object = message as? [String: Any], let key = object["key"] as? String else { return "" }
        let args = (object["args"] as? [Any] ?? []).map { render($0) }
        // `NSLocalizedString` returns the key itself for a key the catalog lacks, which is visible, not silent.
        let template = NSLocalizedString(key, comment: "")
        guard !args.isEmpty else { return template }
        return String(format: template, arguments: args.map { NSString(string: $0) as CVarArg })
    }
}

// MARK: - NPProtocolValidator

struct NPProtocolValidator {
    let resolvedLimits: NPLimitsSet
    let sourceMap: NPLimitSourceMap

    // Convenience initialiser — if you only have limits without a source map
    init(resolvedLimits: NPLimitsSet, sourceMap: NPLimitSourceMap = NPLimitSourceMap()) {
        self.resolvedLimits = resolvedLimits
        self.sourceMap = sourceMap
    }

    // MARK: Public interface

    func validate(_ definition: NPProtocolDefinition) -> NPValidationResult {
        run(.single(definition), library: nil)
    }

    @MainActor
    func validate(_ entry: NPProtocolEntry, resolving library: NPProtocolLibrary?) -> NPValidationResult {
        switch entry {
        case .single, .composite:
            // A composite's layers resolve against the library; with none, there is nothing to check them against.
            let singles = library.map { lib in
                lib.allProtocols.filter { if case .single = $0 { return true } else { return false } }
            }
            return run(entry, library: singles)
        case .limits, .zone, .condition:
            // Not protocols: limits are constraints, zones and conditions are
            // namespace definitions referenced by name and never run.
            return NPValidationResult()
        }
    }

    // MARK: The core

    private func run(_ entry: NPProtocolEntry, library: [NPProtocolEntry]?) -> NPValidationResult {
        var zones: [String: [Int]] = [:]
        for (name, zone) in NPZoneRegistry.zones { zones[name] = zone.sockets }
        do {
            let result = try NppsCore.validate(
                entry: entry, limits: resolvedLimits, library: library, zones: zones, limitSources: sourceMap)
            let issues = result["issues"] as? [[String: Any]] ?? []
            return NPValidationResult(issues: issues.map(Self.issue))
        } catch {
            return failure(error.localizedDescription)
        }
    }

    /// A validation that could not run is an error on the protocol, never a clean bill of health.
    private func failure(_ message: String) -> NPValidationResult {
        NPValidationResult(issues: [NPValidationIssue(
            severity: .error, modality: nil, parameterKey: "validation",
            parameterDisplayName: String(localized: "VALIDATE_PARAM_VALIDATION"),
            actualValueDescription: message, limitValueDescription: "", limitSource: .hardware, message: message
        )])
    }

    private static func issue(_ o: [String: Any]) -> NPValidationIssue {
        let source = source(of: o["limitSource"] as? String)
        return NPValidationIssue(
            severity: (o["severity"] as? String) == "error" ? .error : .warning,
            modality: (o["modality"] as? String).flatMap { NPModalityType(rawValue: $0) },
            parameterKey: o["parameterKey"] as? String ?? "",
            parameterDisplayName: NPValidationText.render(o["parameterName"]),
            actualValueDescription: NPValidationText.render(o["actualValueDescription"]),
            limitValueDescription: NPValidationResult.limitLabel(NPValidationText.render(o["limitValueDescription"]), source),
            limitSource: source,
            message: NPValidationText.render(o["message"])
        )
    }

    private static func source(of wire: String?) -> NPLimitSource {
        switch wire {
        case "hardware": return .hardware
        case "helmet": return .helmet
        case "individual": return .individual
        default: return .global_
        }
    }
}
