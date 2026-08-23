import Foundation

// MARK: - NPPS Error

struct NPPSError: Error, LocalizedError {
    let message: String
    let line: Int

    var errorDescription: String? { "Line \(line): \(message)" }
}

// MARK: - NPPS Token

enum NPPSToken: Equatable {
    case keyword(String)
    case ident(String)
    case string(String)
    case number(Double)
    case numberWithUnit(Double, String)
    case bool(Bool)
    case lbrace
    case rbrace
    case lbracket
    case rbracket
    case colon
    case comma
    case eof
}

// MARK: - NPPS Lexer

struct NPPSLexer {
    private let source: [Character]
    private var pos: Int = 0
    private var line: Int = 1

    private static let keywords: Set<String> = ["protocol", "composite", "layer", "limits", "zone", "condition"]
    private static let units: Set<String> = ["Hz", "mA", "G", "mW_cm2", "m", "s", "h", "%"]

    init(_ text: String) {
        self.source = Array(text)
    }

    mutating func tokenize() throws -> [(NPPSToken, Int)] {
        var tokens: [(NPPSToken, Int)] = []
        while pos < source.count {
            skipWhitespace()
            guard pos < source.count else { break }
            let ch = source[pos]
            let tokenLine = line

            if ch == "#" {
                // Skip comment to end of line
                while pos < source.count && source[pos] != "\n" { pos += 1 }
                continue
            }

            if ch == "\n" {
                line += 1
                pos += 1
                continue
            }

            if ch == "{" { tokens.append((.lbrace, tokenLine));   pos += 1; continue }
            if ch == "}" { tokens.append((.rbrace, tokenLine));   pos += 1; continue }
            if ch == "[" { tokens.append((.lbracket, tokenLine)); pos += 1; continue }
            if ch == "]" { tokens.append((.rbracket, tokenLine)); pos += 1; continue }
            if ch == ":" { tokens.append((.colon, tokenLine));    pos += 1; continue }
            if ch == "," { tokens.append((.comma, tokenLine));    pos += 1; continue }

            if ch == "\"" {
                let s = try readString(tokenLine: tokenLine)
                tokens.append((.string(s), tokenLine))
                continue
            }

            if ch.isNumber || (ch == "-" && pos + 1 < source.count && source[pos + 1].isNumber) {
                let (tok) = try readNumber(tokenLine: tokenLine)
                tokens.append((tok, tokenLine))
                continue
            }

            if ch.isLetter || ch == "_" {
                let ident = readIdent()
                if ident == "true" {
                    tokens.append((.bool(true), tokenLine))
                } else if ident == "false" {
                    tokens.append((.bool(false), tokenLine))
                } else if Self.keywords.contains(ident) {
                    tokens.append((.keyword(ident), tokenLine))
                } else {
                    tokens.append((.ident(ident), tokenLine))
                }
                continue
            }

            // Unknown character — skip
            pos += 1
        }
        tokens.append((.eof, line))
        return tokens
    }

    private mutating func skipWhitespace() {
        while pos < source.count {
            let ch = source[pos]
            if ch == " " || ch == "\t" || ch == "\r" {
                pos += 1
            } else {
                break
            }
        }
    }

    private mutating func readString(tokenLine: Int) throws -> String {
        pos += 1 // skip opening "
        var result = ""
        while pos < source.count {
            let ch = source[pos]
            if ch == "\"" { pos += 1; return result }
            if ch == "\\" && pos + 1 < source.count {
                pos += 1
                switch source[pos] {
                case "n": result.append("\n")
                case "t": result.append("\t")
                case "\"": result.append("\"")
                case "\\": result.append("\\")
                default: result.append(source[pos])
                }
                pos += 1
                continue
            }
            result.append(ch)
            pos += 1
        }
        throw NPPSError(message: "Unterminated string", line: tokenLine)
    }

    private mutating func readNumber(tokenLine: Int) throws -> NPPSToken {
        var numStr = ""
        if pos < source.count && source[pos] == "-" { numStr.append("-"); pos += 1 }
        while pos < source.count && (source[pos].isNumber || source[pos] == ".") {
            numStr.append(source[pos])
            pos += 1
        }
        guard let value = Double(numStr) else {
            throw NPPSError(message: "Invalid number: \(numStr)", line: tokenLine)
        }
        // Check for unit suffix (no space between number and unit in: 20Hz, 1.5mA, 25%, 2m, 30s, 1h, 0.9G, 500mW_cm2)
        let unitStart = pos
        var unitStr = ""
        while pos < source.count && (source[pos].isLetter || source[pos].isNumber || source[pos] == "_") {
            unitStr.append(source[pos])
            pos += 1
        }
        if !unitStr.isEmpty && Self.units.contains(unitStr) {
            return .numberWithUnit(value, unitStr)
        } else if !unitStr.isEmpty {
            // A digit-leading token that is not a number with a known unit is not
            // a value. This used to lex "660_808nm" as an ident when the next
            // char was '_' — which meant "1064nm" (no underscore) fell through
            // and silently split into the number 1064 plus a stray ident, and
            // hyphenated values like "wind-down" and "10-20" never lexed at all.
            // All such values are now quoted, which makes them ordinary JSON
            // strings (NP-NPPS-REF-001 §2, Rev 6).
            throw NPPSError(
                message: "Unquoted value '\(numStr)\(unitStr)' — values that start with a digit "
                       + "and are not a plain number must be quoted, e.g. \"\(numStr)\(unitStr)\"",
                line: tokenLine)
        } else {
            pos = unitStart
            return .number(value)
        }
    }

    private mutating func readIdent() -> String {
        var result = ""
        while pos < source.count && (source[pos].isLetter || source[pos].isNumber || source[pos] == "_") {
            result.append(source[pos])
            pos += 1
        }
        return result
    }
}

// MARK: - NPPS Parser

struct NPPSParser {
    private var tokens: [(NPPSToken, Int)]
    private var pos: Int = 0

    init(_ tokens: [(NPPSToken, Int)]) {
        self.tokens = tokens
    }

    // MARK: Entry point

    mutating func parse() throws -> [NPProtocolEntry] {
        var entries: [NPProtocolEntry] = []
        while currentToken != .eof {
            if case .keyword(let kw) = currentToken {
                if kw == "protocol" {
                    entries.append(.single(try parseProtocol()))
                } else if kw == "composite" {
                    entries.append(.composite(try parseComposite()))
                } else if kw == "limits" {
                    entries.append(.limits(try parseLimitsBlock()))
                } else if kw == "zone" {
                    entries.append(.zone(try parseZoneBlock()))
                } else if kw == "condition" {
                    entries.append(.condition(try parseConditionBlock()))
                } else {
                    throw NPPSError(message: "Unexpected keyword: \(kw)", line: currentLine)
                }
            } else {
                throw NPPSError(
                    message: "Expected 'protocol', 'composite', 'limits', 'zone', or 'condition'",
                    line: currentLine
                )
            }
        }
        return entries
    }

    // MARK: Zone block (NP-NPPS-REF-001 §8)

    /// `zone "Name" { sockets: [1, 2, 3]  types: [led_660]  exclude_types: false }`
    ///
    /// `sockets` is the defining field and is treated as a SET: duplicates
    /// collapse and the result is sorted, so a bilateral protocol referencing
    /// both hemisphere zones of a lobe addresses their shared midline socket
    /// once rather than twice.
    mutating func parseZoneBlock() throws -> NPZoneDefinition {
        let ln = currentLine
        try expectKeyword("zone")
        let name = try parseString()
        try expect(.lbrace)

        var zone = NPZoneDefinition(name: name, sockets: [])
        while currentToken != .rbrace && currentToken != .eof {
            guard case .ident(let key) = currentToken else {
                throw NPPSError(message: "Expected field name in zone block", line: currentLine)
            }
            advance()
            guard currentToken == .colon else {
                throw NPPSError(message: "Unexpected token after '\(key)' in zone block", line: currentLine)
            }
            advance()
            switch key {
            case "id":
                zone.id = try parseString()
                zone.isPredefined = true
            case "description":  zone.description = try parseString()
            case "sockets":      zone.sockets = try parseSocketList(zoneName: name)
            case "types":        zone.types = try parseTagList()
            case "exclude_types":
                if case .bool(let b) = currentToken { zone.excludeTypes = b; advance() } else { skipValue() }
            default:             skipValue()
            }
        }
        try expect(.rbrace)
        if name.isEmpty { throw NPPSError(message: "Zone name cannot be empty", line: ln) }
        return zone
    }

    /// A socket id is a plain whole number naming a socket on this helmet.
    /// Anything else — a fraction, a boolean, an out-of-range id — is reported
    /// with the offending value rather than silently coerced, because a wrong
    /// socket id means light lands somewhere the author did not choose.
    private mutating func parseSocketList(zoneName: String) throws -> [Int] {
        let ln = currentLine
        let raw = try parseFieldValue()
        guard case .array(let items) = raw else {
            throw NPPSError(
                message: "zone \"\(zoneName)\": sockets must be a list, e.g. sockets: [1, 2, 3]",
                line: ln
            )
        }
        var invalid: [String] = []
        var ids: Set<Int> = []
        for item in items {
            guard let d = item.asDouble, d == d.rounded(), d.isFinite else {
                invalid.append(NPPSParser.describe(item))
                continue
            }
            let id = Int(d)
            if NPSocketID.isValid(id) { ids.insert(id) } else { invalid.append(String(id)) }
        }
        if !invalid.isEmpty {
            let isAre = invalid.count == 1 ? "is not a socket" : "are not sockets"
            throw NPPSError(
                message: "zone \"\(zoneName)\": \(invalid.joined(separator: ", ")) \(isAre) on this "
                    + "helmet — ids are whole numbers \(NPSocketID.rangeLabel) "
                    + "(\(SocketZones.socketCount) sockets, numbered from \(NPSocketID.numberingBase))",
                line: ln
            )
        }
        return ids.sorted()
    }

    private static func describe(_ v: NPPSFieldValue) -> String {
        switch v {
        case .string(let s):            return "\"\(s)\""
        case .ident(let s):             return s
        case .bool(let b):              return String(b)
        case .array:                    return "a nested list"
        case .number(let n):            return n == n.rounded() ? String(Int(n)) : String(n)
        case .numberWithUnit(let n, let u):
            return (n == n.rounded() ? String(Int(n)) : String(n)) + u
        }
    }

    // MARK: Condition block (NP-NPPS-REF-001 §9)

    /// `condition "Name" { link: "https://…"  code: "6A70" }` — `link` is required.
    mutating func parseConditionBlock() throws -> NPConditionDefinition {
        let ln = currentLine
        try expectKeyword("condition")
        let name = try parseString()
        try expect(.lbrace)

        var link: String?
        var id: String?
        var code: String?
        var description: String?

        while currentToken != .rbrace && currentToken != .eof {
            guard case .ident(let key) = currentToken else {
                throw NPPSError(message: "Expected field name in condition block", line: currentLine)
            }
            advance()
            guard currentToken == .colon else {
                throw NPPSError(
                    message: "Unexpected token after '\(key)' in condition block", line: currentLine
                )
            }
            advance()
            switch key {
            case "link":        link = try parseString()
            case "id":          id = try parseString()
            case "code":        code = try parseString()
            case "description": description = try parseString()
            default:            skipValue()
            }
        }
        try expect(.rbrace)
        if name.isEmpty { throw NPPSError(message: "Condition name cannot be empty", line: ln) }
        guard let resolvedLink = link, !resolvedLink.isEmpty else {
            throw NPPSError(message: "condition \"\(name)\": 'link' is required", line: ln)
        }
        return NPConditionDefinition(
            name: name, link: resolvedLink, id: id, code: code, description: description
        )
    }

    // MARK: Limits block

    mutating func parseLimitsBlock() throws -> NPLimitsSet {
        let ln = currentLine
        try expectKeyword("limits")
        let name = try parseString()
        try expect(.lbrace)

        var limitsSet = NPLimitsSet(name: name, level: .global)

        while currentToken != .rbrace && currentToken != .eof {
            guard case .ident(let key) = currentToken else {
                throw NPPSError(message: "Expected field name in limits block", line: currentLine)
            }
            advance()

            if currentToken == .colon {
                advance()
                switch key {
                case "level":
                    let val = try parseIdentOrString()
                    switch val {
                    case "global":     limitsSet.level = .global
                    case "helmet":     limitsSet.level = .helmet
                    case "individual": limitsSet.level = .individual
                    default: break
                    }
                case "helmet_id":
                    limitsSet.helmetId = try parseString()
                    limitsSet.level = .helmet
                case "individual_id":
                    let uuidStr = try parseString()
                    limitsSet.individualId = UUID(uuidString: uuidStr)
                    limitsSet.level = .individual
                case "description":
                    limitsSet.description = try parseString()
                default:
                    skipValue()
                }
            } else if currentToken == .lbrace {
                // Modality limits sub-block
                try parseLimitsSubBlock(key: key, into: &limitsSet)
            } else {
                throw NPPSError(message: "Expected ':' or '{' after '\(key)' in limits block", line: currentLine)
            }
        }

        try expect(.rbrace)
        guard !name.isEmpty else {
            throw NPPSError(message: "Limits name cannot be empty", line: ln)
        }
        return limitsSet
    }

    private mutating func parseLimitsSubBlock(key: String, into limitsSet: inout NPLimitsSet) throws {
        try expect(.lbrace)
        var fields: [String: NPPSFieldValue] = [:]
        while currentToken != .rbrace && currentToken != .eof {
            guard case .ident(let fieldKey) = currentToken else {
                throw NPPSError(message: "Expected field name in limits sub-block", line: currentLine)
            }
            advance()
            try expect(.colon)
            let value = try parseFieldValue()
            fields[fieldKey] = value
        }
        try expect(.rbrace)

        switch key {
        case "pbm_transcranial":
            var lim = NPPBMTranscranialLimits()
            if let v = fields["max_intensity"]?.asPercent   { lim.maxIntensityPercent = v }
            if let v = fields["max_frequency"]?.asHz        { lim.maxFrequencyHz = v }
            if let v = fields["max_duty_cycle"]?.asPercent  { lim.maxDutyCyclePercent = Int(v) }
            if let v = fields["max_session_dose"]?.asDouble { lim.maxSessionDoseJCm2 = v }
            if let v = fields["max_daily_dose"]?.asDouble   { lim.maxDailyDoseJCm2 = v }
            limitsSet.pbmTranscranial = lim

        case "pbm_intranasal":
            var lim = NPPBMIntranasalLimits()
            if let v = fields["max_intensity"]?.asPercent           { lim.maxIntensityPercent = v }
            if let v = fields["max_session_dose"]?.asDouble         { lim.maxSessionDoseJCm2 = v }
            if let v = fields["max_session_duration"]?.asTime       { lim.maxSessionDurationSeconds = v }
            limitsSet.pbmIntranasal = lim

        case "eeg_neurofeedback":
            var lim = NPEEGNeurofeedbackLimits()
            if let v = fields["allowed_bands"] {
                if case .array(let items) = v {
                    lim.allowedBands = items.compactMap { item -> String? in
                        if case .ident(let s) = item { return s }
                        if case .string(let s) = item { return s }
                        return nil
                    }
                }
            }
            if let v = fields["require_closed_loop"]?.asBool { lim.requireClosedLoop = v }
            limitsSet.eegNeurofeedback = lim

        case "bes_tacs":
            var lim = NPBESTacsLimits()
            if let v = fields["max_intensity"]?.asMilliamps      { lim.maxIntensityMilliamps = v }
            if let v = fields["max_frequency"]?.asHz             { lim.maxFrequencyHz = v }
            if let v = fields["min_frequency"]?.asHz             { lim.minFrequencyHz = v }
            if let v = fields["max_session_duration"]?.asTime    { lim.maxSessionDurationSeconds = v }
            if let v = fields["max_sessions_per_day"]?.asDouble  { lim.maxSessionsPerDay = Int(v) }
            limitsSet.besTacs = lim

        case "tdcs":
            var lim = NPTDCSLimits()
            if let v = fields["max_intensity"]?.asMilliamps      { lim.maxIntensityMilliamps = v }
            if let v = fields["max_session_duration"]?.asTime    { lim.maxSessionDurationSeconds = v }
            if let v = fields["max_sessions_per_day"]?.asDouble  { lim.maxSessionsPerDay = Int(v) }
            limitsSet.tdcs = lim

        case "vns_hrv":
            var lim = NPVNSHRVLimits()
            if let v = fields["max_intensity"]?.asMilliamps      { lim.maxIntensityMilliamps = v }
            if let v = fields["max_frequency"]?.asHz             { lim.maxFrequencyHz = v }
            if let v = fields["max_session_duration"]?.asTime    { lim.maxSessionDurationSeconds = v }
            if let v = fields["allowed_protocols"] {
                if case .array(let items) = v {
                    lim.allowedProtocols = items.compactMap { item -> String? in
                        if case .ident(let s) = item { return s }
                        if case .string(let s) = item { return s }
                        return nil
                    }
                }
            }
            limitsSet.vnsHrv = lim

        case "audio_entrainment":
            var lim = NPAudioEntrainmentLimits()
            if let v = fields["max_intensity"]?.asPercent        { lim.maxVolumePercent = v }
            if let v = fields["max_binaural_beats"]?.asHz        { lim.maxBinauralBeatsHz = v }
            if let v = fields["max_isochronic_tones"]?.asHz      { lim.maxIsochronicTonesHz = v }
            limitsSet.audioEntrainment = lim

        case "visual_stimulation":
            var lim = NPVisualStimLimits()
            if let v = fields["max_frequency"]?.asHz           { lim.maxFrequencyHz = v }
            if let v = fields["min_frequency"]?.asHz           { lim.minFrequencyHz = v }
            if let v = fields["block_high_risk_range"]?.asBool { lim.blockHighRiskRange = v }
            if let v = fields["allowed_modes"] {
                if case .array(let items) = v {
                    lim.allowedModes = items.compactMap { item -> String? in
                        if case .ident(let s) = item { return s }
                        if case .string(let s) = item { return s }
                        return nil
                    }
                }
            }
            limitsSet.visualStimulation = lim

        case "tms":
            var lim = NPTMSLimits()
            if let v = fields["max_intensity_pct_mt"]?.asDouble   { lim.maxIntensityPercentMT = Int(v) }
            if let v = fields["max_pulses_per_session"]?.asDouble { lim.maxPulsesPerSession = Int(v) }
            if let v = fields["max_pulses_per_day"]?.asDouble     { lim.maxPulsesPerDay = Int(v) }
            if let v = fields["max_sessions_per_week"]?.asDouble  { lim.maxSessionsPerWeek = Int(v) }
            if let v = fields["allowed_protocols"] {
                if case .array(let items) = v {
                    lim.allowedProtocols = items.compactMap { item -> String? in
                        if case .ident(let s) = item { return s }
                        if case .string(let s) = item { return s }
                        return nil
                    }
                }
            }
            if let v = fields["allowed_targets"] {
                if case .array(let items) = v {
                    lim.allowedTargets = items.compactMap { item -> String? in
                        if case .ident(let s) = item { return s }
                        if case .string(let s) = item { return s }
                        return nil
                    }
                }
            }
            limitsSet.tms = lim

        case "pbm_deep_1170nm":
            var lim = NPDeepPBMLimits()
            if let v = fields["max_intensity"]?.asDouble          { lim.maxIntensityMWcm2 = v }
            if let v = fields["max_session_duration"]?.asTime     { lim.maxSessionDurationSeconds = v }
            limitsSet.pbmDeep1170nm = lim

        case "clinical_tacs":
            var lim = NPClinicalTacsLimits()
            if let v = fields["max_intensity"]?.asMilliamps       { lim.maxIntensityMilliamps = v }
            if let v = fields["max_session_duration"]?.asTime     { lim.maxSessionDurationSeconds = v }
            limitsSet.clinicalTacs = lim

        case "hd_tdcs":
            var lim = NPHDTdcsLimits()
            if let v = fields["max_intensity"]?.asMilliamps       { lim.maxIntensityMilliamps = v }
            if let v = fields["max_session_duration"]?.asTime     { lim.maxSessionDurationSeconds = v }
            if let v = fields["allowed_montages"] {
                if case .array(let items) = v {
                    lim.allowedMontages = items.compactMap { item -> String? in
                        if case .ident(let s) = item { return s }
                        if case .string(let s) = item { return s }
                        return nil
                    }
                }
            }
            limitsSet.hdTdcs = lim

        case "cervical_vns":
            var lim = NPCervicalVnsLimits()
            if let v = fields["max_intensity"]?.asMilliamps       { lim.maxIntensityMilliamps = v }
            if let v = fields["max_session_duration"]?.asTime     { lim.maxSessionDurationSeconds = v }
            limitsSet.cervicalVns = lim

        case "vibrotactile_40hz":
            var lim = NPVibrotactileLimits()
            if let v = fields["max_intensity"]?.asDouble          { lim.maxIntensityG = v }
            if let v = fields["max_session_duration"]?.asTime     { lim.maxSessionDurationSeconds = v }
            limitsSet.vibrotactile40hz = lim

        default:
            break // Forward compatibility: unknown modality limits blocks are silently ignored
        }
    }

    // MARK: Protocol

    mutating func parseProtocol() throws -> NPProtocolDefinition {
        let ln = currentLine
        try expectKeyword("protocol")
        let name = try parseString()
        try expect(.lbrace)

        var id: UUID = UUID()
        var description = ""
        var author = "NeurOne"
        var version = "1.0"
        var tags: [String] = []
        var conditions: [String] = []
        var references: [NPProtocolReference] = []
        var timingMode: NPProtocolDefinition.TimingMode = .duration(20 * 60)
        var modalities: [NPProtocolModality] = []
        var isReadOnly = false

        while currentToken != .rbrace && currentToken != .eof {
            guard case .ident(let key) = currentToken else {
                throw NPPSError(message: "Expected field name or modality block", line: currentLine)
            }
            advance()
            if currentToken == .colon {
                // Field: key: value
                advance()
                switch key {
                case "id":
                    let uuidStr = try parseString()
                    if let parsed = UUID(uuidString: uuidStr) { id = parsed }
                case "readonly":
                    if case .bool(let b) = currentToken { isReadOnly = b; advance() }
                case "description":
                    description = try parseString()
                case "author":
                    author = try parseString()
                case "version":
                    version = try parseString()
                case "tags":
                    tags = try parseTagList()
                case "conditions":
                    conditions = try parseTagList()
                case "references":
                    references = try parseReferenceList()
                case "duration":
                    let secs = try parseTimeValue()
                    timingMode = .duration(secs)
                case "interval_count":
                    if case .number(let n) = currentToken {
                        timingMode = .intervalCount(Int(n))
                        advance()
                    } else if case .numberWithUnit(let n, _) = currentToken {
                        timingMode = .intervalCount(Int(n))
                        advance()
                    } else {
                        throw NPPSError(message: "Expected number for interval_count", line: currentLine)
                    }
                default:
                    // Unknown field — skip value
                    skipValue()
                }
            } else if currentToken == .lbrace {
                // Typed modality block (new format: pbm_transcranial { ... })
                let mod = try parseModalityBlock(name: key)
                modalities.append(mod)
            } else {
                throw NPPSError(message: "Expected ':' or '{' after '\(key)'", line: currentLine)
            }
        }

        try expect(.rbrace)

        guard !name.isEmpty else {
            throw NPPSError(message: "Protocol name cannot be empty", line: ln)
        }

        var proto = NPProtocolDefinition(
            name: name,
            description: description,
            author: author,
            version: version,
            tags: tags,
            createdAt: Date(),
            modifiedAt: Date(),
            isPredefined: isReadOnly,
            isReadOnly: isReadOnly,
            timingMode: timingMode,
            modalities: modalities,
            conditions: conditions,
            references: references
        )
        proto.id = id
        return proto
    }

    // MARK: Composite

    mutating func parseComposite() throws -> NPCompositeProtocol {
        let ln = currentLine
        try expectKeyword("composite")
        let name = try parseString()
        try expect(.lbrace)

        var id: UUID = UUID()
        var description = ""
        var author = "NeurOne"
        var version = "1.0"
        var tags: [String] = []
        var conditions: [String] = []
        var references: [NPProtocolReference] = []
        var conflictResolution: NPCompositeProtocol.ConflictResolution = .merge
        var layers: [NPCompositeLayer] = []
        var isReadOnly = false

        while currentToken != .rbrace && currentToken != .eof {
            // Handle "layer" keyword (emitted as .keyword("layer") by lexer)
            if case .keyword(let kw) = currentToken, kw == "layer" {
                advance() // consume "layer"
                let layer = try parseLayerBlock()
                layers.append(layer)
                continue
            }

            guard case .ident(let key) = currentToken else {
                throw NPPSError(message: "Expected field name in composite block", line: currentLine)
            }
            advance()
            if case .colon = currentToken {
                advance()
                switch key {
                case "id":
                    let uuidStr = try parseString()
                    if let parsed = UUID(uuidString: uuidStr) { id = parsed }
                case "readonly":
                    if case .bool(let b) = currentToken { isReadOnly = b; advance() }
                case "description":
                    description = try parseString()
                case "author":
                    author = try parseString()
                case "version":
                    version = try parseString()
                case "tags":
                    tags = try parseTagList()
                case "conditions":
                    conditions = try parseTagList()
                case "references":
                    references = try parseReferenceList()
                case "conflict_resolution":
                    let val = try parseIdentOrString()
                    switch val {
                    case "merge":      conflictResolution = .merge
                    case "sequential": conflictResolution = .sequential
                    case "override":   conflictResolution = .override
                    default: break
                    }
                default:
                    skipValue()
                }
            } else {
                throw NPPSError(message: "Unexpected token after '\(key)' in composite block", line: currentLine)
            }
        }

        try expect(.rbrace)

        guard !name.isEmpty else {
            throw NPPSError(message: "Composite name cannot be empty", line: ln)
        }

        var comp = NPCompositeProtocol(
            name: name,
            description: description,
            author: author,
            version: version,
            tags: tags,
            createdAt: Date(),
            modifiedAt: Date(),
            isPredefined: isReadOnly,
            isReadOnly: isReadOnly,
            layers: layers,
            conflictResolution: conflictResolution,
            conditions: conditions,
            references: references
        )
        comp.id = id
        return comp
    }

    // MARK: Layer block (for composite)
    // Called after the "layer" keyword has been consumed. Next token is the protocol name string.

    mutating func parseLayerBlock() throws -> NPCompositeLayer {
        let protocolName = try parseString()
        try expect(.lbrace)

        var startOffsetSeconds = 0
        var durationSeconds: Int? = nil
        var intensityScale: Double = 1.0

        while currentToken != .rbrace && currentToken != .eof {
            guard case .ident(let key) = currentToken else {
                throw NPPSError(message: "Expected field name in layer block", line: currentLine)
            }
            advance()
            try expect(.colon)
            switch key {
            case "start":
                startOffsetSeconds = try parseTimeValue()
            case "duration":
                durationSeconds = try parseTimeValue()
            case "intensity_scale":
                intensityScale = try parseDoubleValue()
            default:
                skipValue()
            }
        }

        try expect(.rbrace)

        return NPCompositeLayer(
            protocolName: protocolName,
            startOffsetSeconds: startOffsetSeconds,
            durationSeconds: durationSeconds,
            intensityScale: intensityScale
        )
    }

    // MARK: Modality block

    mutating func parseModalityBlock(name: String) throws -> NPProtocolModality {
        // Captured before the block is consumed: buildParams reports errors, and
        // by the time it runs `currentLine` has moved past the closing brace.
        let blockLine = currentLine
        try expect(.lbrace)

        // Parse all key-value pairs into a dictionary for easier dispatch
        var fields: [String: NPPSFieldValue] = [:]
        while currentToken != .rbrace && currentToken != .eof {
            guard case .ident(let key) = currentToken else {
                throw NPPSError(message: "Expected field name in modality block", line: currentLine)
            }
            advance()
            try expect(.colon)
            let value = try parseFieldValue()
            fields[key] = value
        }
        try expect(.rbrace)

        // Parse interval config
        let intervalOn  = fields["interval_on"].flatMap  { $0.asTime }   ?? 0
        let intervalOff = fields["interval_off"].flatMap { $0.asTime }   ?? 0
        let repeatRaw   = fields["repeat"]
        var repeatCount: Int? = nil
        if let rv = repeatRaw {
            if case .ident(let s) = rv, s == "until_end" { repeatCount = nil }
            else if case .number(let n) = rv { repeatCount = Int(n) }
            else if case .numberWithUnit(let n, _) = rv { repeatCount = Int(n) }
        }
        let interval = NPIntervalConfig(
            intervalOnSeconds: intervalOn,
            intervalOffSeconds: intervalOff,
            repeatCount: repeatCount
        )

        let params = try buildParams(name: name, fields: fields, line: blockLine)
        return NPProtocolModality(params: params, interval: interval, enabled: true)
    }

    // MARK: Params builder

    /// Parse a `zones:` value into a PBM target.
    ///
    /// Exactly two authored forms, and nothing else parses:
    ///
    ///   `zones: ["Frontal", ...]`   -> named zones from 00-zones.npps
    ///   `zones: clinician_selected` -> the operator picks sockets at run time
    ///
    /// There is deliberately NO default branch and no retired-form handling. The
    /// previous parser mapped every unrecognised selector to `.all` — so
    /// `zones: ["Frontal Right (excl. midline)"]`, the lateralized clinical-03
    /// target, silently became "every module": a wrong-site stimulation path that
    /// produced no error anywhere. Anything this function does not recognise is a
    /// parse error, which is the only safe reading of an unknown target.
    private func parsePBMTarget(_ value: NPPSFieldValue, line: Int) throws -> NPPBMTarget {
        switch value {
        case .ident("clinician_selected"):
            return .clinicianSelected

        case .array(let items):
            guard !items.isEmpty else {
                throw NPPSError(
                    message: "zones: [] names no zone — a session cannot target nothing.",
                    line: line
                )
            }
            let names = items.compactMap { item -> String? in
                if case .string(let s) = item { return s }
                return nil
            }
            guard names.count == items.count else {
                throw NPPSError(
                    message: "zones must be a list of quoted zone names, e.g. "
                        + "zones: [\"Frontal Left\", \"Frontal Right\"].",
                    line: line
                )
            }
            return .named(names)

        case .ident(let selector):
            // Quote what was actually written. The author needs to find the token
            // in their file, and an unrecognised identifier here is most often a
            // typo in `clinician_selected` or a zone name that lost its quotes.
            throw NPPSError(
                message: "Unknown zone selector '\(selector)'. zones must be a list of quoted "
                    + "zone names (e.g. zones: [\"Frontal\"]) or the identifier clinician_selected.",
                line: line
            )

        default:
            throw NPPSError(
                message: "zones must be a list of quoted zone names (e.g. zones: [\"Frontal\"]) "
                    + "or the identifier clinician_selected.",
                line: line
            )
        }
    }

    private func buildParams(
        name: String, fields: [String: NPPSFieldValue], line: Int
    ) throws -> NPModalityParams {
        switch name {
        case "pbm_transcranial":
            var p = NPPBMTranscranialParams()
            if let v = fields["intensity"]?.asPercent { p.intensityPercent = v }
            if let v = fields["frequency"]?.asHz { p.frequencyHz = v }
            if let v = fields["duty_cycle"]?.asPercent { p.dutyCyclePercent = Int(v) }
            if let v = fields["zones"] {
                p.target = try parsePBMTarget(v, line: line)
            }
            if let v = fields["wavelength"]?.asIdent {
                switch v {
                case "660_808nm":      p.wavelength = .base660_808nm
                case "1064nm":         p.wavelength = .smart1064nm
                case "660_808_1064nm": p.wavelength = .tri660_808_1064
                default:               p.wavelength = .base660_808nm
                }
            }
            return .pbmTranscranial(p)

        case "pbm_intranasal":
            var p = NPPBMIntranasalParams()
            if let v = fields["intensity"]?.asPercent  { p.intensityPercent = v }
            if let v = fields["frequency"]?.asHz       { p.frequencyHz = v }
            if let v = fields["duty_cycle"]?.asPercent { p.dutyCyclePercent = Int(v) }
            return .pbmIntranasal(p)

        case "eeg_neurofeedback":
            var p = NPEEGNeurofeedbackParams()
            if let v = fields["band"]?.asIdent {
                switch v {
                case "delta":      p.band = .delta
                case "theta":      p.band = .theta
                case "alpha":      p.band = .alpha
                case "beta":       p.band = .beta
                case "gamma":      p.band = .gamma
                case "alpha_theta": p.band = .alphaTheta
                case "gamma_theta": p.band = .gammaTheta
                default: break
                }
            }
            if let v = fields["channels"] {
                if case .ident(let s) = v {
                    switch s {
                    case "all":     p.channels = .all
                    case "front":   p.channels = .front
                    case "central": p.channels = .central
                    default:        p.channels = .all
                    }
                } else if case .array(let items) = v {
                    p.channels = .custom
                    p.customChannels = items.compactMap { if case .string(let s) = $0 { return s }; return nil }
                }
            }
            if let v = fields["closed_loop"]?.asBool { p.closedLoopEnabled = v }
            return .eegNeurofeedback(p)

        case "bes_tacs":
            var p = NPBESTacsParams()
            if let v = fields["frequency"]?.asHz          { p.frequencyHz = v }
            if let v = fields["intensity"]?.asMilliamps   { p.intensityMilliamps = v }
            if let v = fields["waveform"]?.asIdent {
                switch v {
                case "sinusoidal": p.waveform = .sinusoidal
                case "square":     p.waveform = .square
                case "triangular": p.waveform = .triangular
                default: break
                }
            }
            return .besTacs(p)

        case "tdcs":
            var p = NPTDCSParams()
            if let v = fields["intensity"]?.asMilliamps { p.intensityMilliamps = v }
            return .tdcs(p)

        case "vns_hrv":
            var p = NPVNSHRVParams()
            if let v = fields["frequency"]?.asHz        { p.frequencyHz = v }
            if let v = fields["intensity"]?.asMilliamps { p.intensityMilliamps = v }
            if let v = fields["hrv_protocol"]?.asIdent {
                switch v {
                case "standalone":       p.hrvProtocol = .standalone
                case "tavns_sync":       p.hrvProtocol = .tavnsSync
                case "eeg_biofeedback":  p.hrvProtocol = .eegBiofeedback
                case "combined_pbm":     p.hrvProtocol = .combinedPBM
                default: break
                }
            }
            if let v = fields["breathing_rate"]?.asDouble { p.resonanceBreathingRate = v }
            return .vnsHRV(p)

        case "audio_entrainment":
            var p = NPAudioEntrainmentParams()
            if let v = fields["binaural_hz"]?.asHz  { p.binauralBeatsHz = v }
            if let v = fields["isochronic_hz"]?.asHz { p.isochronicTonesHz = v }
            if let v = fields["noise"]?.asIdent {
                switch v {
                case "pink":  p.noiseType = .pink
                case "brown": p.noiseType = .brown
                case "none":  p.noiseType = nil
                default: break
                }
            }
            if let v = fields["carrier_hz"]?.asHz   { p.carrierHz = v }
            if let v = fields["volume"]?.asPercent  { p.volumePercent = v }
            if let v = fields["eeg_adaptive"]?.asBool          { p.eegAdaptive = v }
            if let v = fields["bone_conduction_pacer"]?.asBool  { p.boneConductionPacer = v }
            return .audioEntrainment(p)

        case "visual_stimulation":
            var p = NPVisualStimParams()
            if let v = fields["frequency"]?.asHz { p.frequencyHz = v }
            if let v = fields["mode"]?.asIdent {
                switch v {
                case "binocular":  p.mode = .binocular
                case "emdr":       p.mode = .emdr
                case "retinal_pbm": p.mode = .retinalPBM
                case "mode_f":     p.mode = .modeF
                default: break
                }
            }
            if let v = fields["emdr_cadence"]?.asHz  { p.emdrCadenceHz = v }
            // Distinct from the `mode: mode_f` VALUE parsed just above.
            if let v = fields["enable_mode_f"]?.asBool { p.enableModeF = v }
            return .visualStimulation(p)

        case "qeeg_21ch":
            var p = NPqEEG21chParams()
            if let v = fields["sloreta_enabled"]?.asBool { p.sloretaEnabled = v }
            if let v = fields["reference"]?.asIdent {
                switch v {
                case "linked_ear": p.reference = .linkedEar
                case "cz":         p.reference = .cz
                case "average":    p.reference = .average
                default: break
                }
            }
            return .qeeg21ch(p)

        case "tms":
            var p = NPTMSParams()
            if let v = fields["frequency"]?.asHz          { p.frequencyHz = v }
            if let v = fields["intensity_percent_mt"]?.asDouble { p.intensityPercentMT = Int(v) }
            if let v = fields["pulse_count"]?.asDouble    { p.pulseCount = Int(v) }
            if let v = fields["target"]?.asIdent {
                if let t = NPTMSParams.TMSTarget(rawValue: v.uppercased()) { p.target = t }
            }
            if let v = fields["protocol"]?.asIdent {
                switch v {
                case "rTMS": p.tmsProtocol = .rTMS
                case "TBS":  p.tmsProtocol = .TBS
                case "iTBS": p.tmsProtocol = .iTBS
                default: break
                }
            }
            return .tms(p)

        case "pbm_deep_1170nm":
            var p = NPDeepPBM1170Params()
            if let v = fields["intensity"]?.asDouble    { p.intensityMWcm2 = v }
            if let v = fields["frequency"]?.asHz        { p.frequencyHz = v }
            if let v = fields["duty_cycle"]?.asPercent  { p.dutyCyclePercent = Int(v) }
            return .pbmDeep1170nm(p)

        case "clinical_tacs":
            var p = NPClinicalTacsParams()
            if let v = fields["frequency"]?.asHz        { p.frequencyHz = v }
            if let v = fields["intensity"]?.asMilliamps { p.intensityMilliamps = v }
            if let v = fields["channel_count"]?.asDouble { p.channelCount = Int(v) }
            return .clinicalTacs(p)

        case "hd_tdcs":
            var p = NPHDTdcsParams()
            if let v = fields["intensity"]?.asMilliamps { p.intensityMilliamps = v }
            if let v = fields["montage"]?.asIdent {
                switch v {
                case "ring_4x1":                   p.montage = .ring4x1
                case "bilateral_4x1":              p.montage = .bilateral4x1
                case "standard_2_electrode":       p.montage = .standard2el
                default: break
                }
            }
            if let v = fields["target"]?.asIdent {
                if let t = NPTMSParams.TMSTarget(rawValue: v.uppercased()) { p.target = t }
            }
            return .hdTdcs(p)

        case "cervical_vns":
            var p = NPCervicalVnsParams()
            if let v = fields["frequency"]?.asHz        { p.frequencyHz = v }
            if let v = fields["intensity"]?.asMilliamps { p.intensityMilliamps = v }
            return .cervicalVns(p)

        case "vibrotactile_40hz":
            var p = NPVibrotactileParams()
            if let v = fields["intensity_g"]?.asDouble  { p.intensityG = v }
            if let v = fields["sync_to_audio"]?.asBool     { p.syncToAudio = v }
            if let v = fields["sync_to_visual"]?.asBool    { p.syncToVisual = v }
            return .vibrotactile40hz(p)

        default:
            throw NPPSError(message: "Unknown modality: \(name)", line: 0)
        }
    }

    // MARK: Field value type

    enum NPPSFieldValue: Equatable {
        case number(Double)
        case numberWithUnit(Double, String)
        case string(String)
        case ident(String)
        case bool(Bool)
        case array([NPPSFieldValue])

        var asDouble: Double? {
            switch self {
            case .number(let n): return n
            case .numberWithUnit(let n, _): return n
            default: return nil
            }
        }

        var asHz: Double? {
            switch self {
            case .numberWithUnit(let n, let u) where u == "Hz": return n
            case .number(let n): return n
            default: return nil
            }
        }

        var asMilliamps: Double? {
            switch self {
            case .numberWithUnit(let n, let u) where u == "mA": return n
            case .number(let n): return n
            default: return nil
            }
        }

        var asPercent: Double? {
            switch self {
            case .numberWithUnit(let n, let u) where u == "%": return n
            case .number(let n): return n
            default: return nil
            }
        }

        var asTime: Int? {
            switch self {
            case .numberWithUnit(let n, "m"): return Int(n * 60)
            case .numberWithUnit(let n, "s"): return Int(n)
            case .numberWithUnit(let n, "h"): return Int(n * 3600)
            case .number(let n): return Int(n)
            default: return nil
            }
        }

        var asBool: Bool? {
            if case .bool(let b) = self { return b }
            return nil
        }

        var asIdent: String? {
            switch self {
            case .ident(let s):  return s
            case .string(let s): return s
            default:             return nil
            }
        }
    }

    // MARK: Field value parser

    mutating func parseFieldValue() throws -> NPPSFieldValue {
        switch currentToken {
        case .number(let n):
            advance()
            return .number(n)
        case .numberWithUnit(let n, let u):
            advance()
            return .numberWithUnit(n, u)
        case .string(let s):
            advance()
            return .string(s)
        case .bool(let b):
            advance()
            return .bool(b)
        case .ident(let s):
            advance()
            return .ident(s)
        case .lbracket:
            advance()
            var items: [NPPSFieldValue] = []
            while currentToken != .rbracket && currentToken != .eof {
                items.append(try parseFieldValue())
                if currentToken == .comma { advance() }
            }
            try expect(.rbracket)
            return .array(items)
        default:
            throw NPPSError(message: "Expected a value", line: currentLine)
        }
    }

    // MARK: Helpers

    private var currentToken: NPPSToken { tokens[pos].0 }
    private var currentLine: Int { tokens[pos].1 }

    mutating private func advance() {
        if pos < tokens.count - 1 { pos += 1 }
    }

    mutating func expect(_ token: NPPSToken) throws {
        if currentToken == token {
            advance()
        } else {
            throw NPPSError(message: "Expected \(token) but got \(currentToken)", line: currentLine)
        }
    }

    mutating private func expectKeyword(_ kw: String) throws {
        if case .keyword(let k) = currentToken, k == kw {
            advance()
        } else {
            throw NPPSError(message: "Expected keyword '\(kw)'", line: currentLine)
        }
    }

    mutating private func parseString() throws -> String {
        switch currentToken {
        case .string(let s): advance(); return s
        case .ident(let s): advance(); return s
        default:
            throw NPPSError(message: "Expected string", line: currentLine)
        }
    }

    mutating private func parseIdentOrString() throws -> String {
        return try parseString()
    }

    /// A `references` value: an array whose elements are each a bare URL/path
    /// string, or a `[label, url]` pair (NP-NPPS-REF-001 §2).
    mutating private func parseReferenceList() throws -> [NPProtocolReference] {
        let raw = try parseFieldValue()
        guard case .array(let items) = raw else { return [] }
        return items.compactMap { item in
            if case .array(let pair) = item {
                guard pair.count >= 2, let url = pair[1].asIdent else { return nil }
                return NPProtocolReference(url: url, label: pair[0].asIdent)
            }
            guard let url = item.asIdent else { return nil }
            return NPProtocolReference(url: url)
        }
    }

    mutating private func parseTagList() throws -> [String] {
        try expect(.lbracket)
        var tags: [String] = []
        while currentToken != .rbracket && currentToken != .eof {
            switch currentToken {
            case .ident(let s):  tags.append(s); advance()
            case .string(let s): tags.append(s); advance()
            default: advance()
            }
            if currentToken == .comma { advance() }
        }
        try expect(.rbracket)
        return tags
    }

    mutating func parseTimeValue() throws -> Int {
        let fv = try parseFieldValue()
        if let t = fv.asTime { return t }
        throw NPPSError(message: "Expected a time value (e.g. 20m, 30s, 1h)", line: currentLine)
    }

    mutating func parseDoubleValue() throws -> Double {
        let fv = try parseFieldValue()
        if let d = fv.asDouble { return d }
        throw NPPSError(message: "Expected a numeric value", line: currentLine)
    }

    mutating func parsePercentValue() throws -> Double? {
        let fv = try parseFieldValue()
        return fv.asPercent
    }

    private mutating func skipValue() {
        // Skip a single value (primitive or array)
        if case .lbracket = currentToken {
            advance()
            var depth = 1
            while currentToken != .eof && depth > 0 {
                if case .lbracket = currentToken { depth += 1 }
                if case .rbracket = currentToken { depth -= 1 }
                advance()
            }
        } else {
            advance()
        }
    }
}

// MARK: - NPPS Serializer

struct NPPSSerializer {

    func serialize(_ entry: NPProtocolEntry) -> String {
        switch entry {
        case .single(let proto):
            return serializeProtocol(proto)
        case .composite(let comp):
            return serializeComposite(comp)
        case .limits(let lim):
            return serializeLimits(lim)
        case .zone(let zone):
            return serializeZone(zone)
        case .condition(let cond):
            return serializeCondition(cond)
        }
    }

    /// A bare URL, or a `[label, url]` pair when the entry was labelled.
    private func serializeReference(_ r: NPProtocolReference) -> String {
        guard let label = r.label else { return "\"\(escape(r.url))\"" }
        return "[\"\(escape(label))\", \"\(escape(r.url))\"]"
    }

    /// Escape a value for a double-quoted NPPS string.
    private func escape(_ value: String) -> String {
        value.replacingOccurrences(of: "\\", with: "\\\\")
             .replacingOccurrences(of: "\"", with: "\\\"")
    }

    // MARK: Zone / Condition (NP-NPPS-REF-001 §8, §9)

    func serializeZone(_ z: NPZoneDefinition) -> String {
        var lines: [String] = ["zone \"\(escape(z.name))\" {"]
        if let id = z.id { lines.append("    id: \"\(escape(id))\"") }
        if let d = z.description { lines.append("    description: \"\(escape(d))\"") }
        // Canonical membership on the way out as well as in: sorted and deduped,
        // so two equal zones serialize identically.
        let sockets = Array(Set(z.sockets)).sorted().map(String.init).joined(separator: ", ")
        lines.append("    sockets: [\(sockets)]")
        if let types = z.types, !types.isEmpty {
            lines.append("    types: [\(types.joined(separator: ", "))]")
        }
        if z.excludeTypes { lines.append("    exclude_types: true") }
        lines.append("}")
        return lines.joined(separator: "\n")
    }

    func serializeCondition(_ c: NPConditionDefinition) -> String {
        var lines: [String] = ["condition \"\(escape(c.name))\" {"]
        if let id = c.id { lines.append("    id: \"\(escape(id))\"") }
        lines.append("    link: \"\(escape(c.link))\"")
        if let code = c.code { lines.append("    code: \"\(escape(code))\"") }
        if let d = c.description { lines.append("    description: \"\(escape(d))\"") }
        lines.append("}")
        return lines.joined(separator: "\n")
    }

    func serializeLimits(_ limits: NPLimitsSet) -> String {
        var lines: [String] = []
        lines.append("limits \"\(limits.name)\" {")
        lines.append("    level: \(limits.level.rawValue)")
        if let hid = limits.helmetId { lines.append("    helmet_id: \"\(hid)\"") }
        if let iid = limits.individualId { lines.append("    individual_id: \"\(iid.uuidString)\"") }
        if !limits.description.isEmpty { lines.append("    description: \"\(limits.description)\"") }
        lines.append("")

        if let lim = limits.pbmTranscranial {
            lines.append("    pbm_transcranial {")
            if let v = lim.maxIntensityPercent   { lines.append("        max_intensity: \(Int(v))%") }
            if let v = lim.maxFrequencyHz         { lines.append("        max_frequency: \(formatHz(v))") }
            if let v = lim.maxDutyCyclePercent    { lines.append("        max_duty_cycle: \(v)%") }
            if let v = lim.maxSessionDoseJCm2     { lines.append("        max_session_dose: \(v)") }
            if let v = lim.maxDailyDoseJCm2       { lines.append("        max_daily_dose: \(v)") }
            lines.append("    }")
        }
        if let lim = limits.pbmIntranasal {
            lines.append("    pbm_intranasal {")
            if let v = lim.maxIntensityPercent        { lines.append("        max_intensity: \(Int(v))%") }
            if let v = lim.maxSessionDoseJCm2         { lines.append("        max_session_dose: \(v)") }
            if let v = lim.maxSessionDurationSeconds  { lines.append("        max_session_duration: \(formatTime(v))") }
            lines.append("    }")
        }
        if let lim = limits.eegNeurofeedback {
            lines.append("    eeg_neurofeedback {")
            if let v = lim.allowedBands       { lines.append("        allowed_bands: [\(v.joined(separator: ", "))]") }
            if let v = lim.requireClosedLoop  { lines.append("        require_closed_loop: \(v)") }
            lines.append("    }")
        }
        if let lim = limits.besTacs {
            lines.append("    bes_tacs {")
            if let v = lim.maxIntensityMilliamps      { lines.append("        max_intensity: \(v)mA") }
            if let v = lim.maxFrequencyHz             { lines.append("        max_frequency: \(formatHz(v))") }
            if let v = lim.minFrequencyHz             { lines.append("        min_frequency: \(formatHz(v))") }
            if let v = lim.maxSessionDurationSeconds  { lines.append("        max_session_duration: \(formatTime(v))") }
            if let v = lim.maxSessionsPerDay          { lines.append("        max_sessions_per_day: \(v)") }
            lines.append("    }")
        }
        if let lim = limits.tdcs {
            lines.append("    tdcs {")
            if let v = lim.maxIntensityMilliamps      { lines.append("        max_intensity: \(v)mA") }
            if let v = lim.maxSessionDurationSeconds  { lines.append("        max_session_duration: \(formatTime(v))") }
            if let v = lim.maxSessionsPerDay          { lines.append("        max_sessions_per_day: \(v)") }
            lines.append("    }")
        }
        if let lim = limits.vnsHrv {
            lines.append("    vns_hrv {")
            if let v = lim.maxIntensityMilliamps      { lines.append("        max_intensity: \(v)mA") }
            if let v = lim.maxFrequencyHz             { lines.append("        max_frequency: \(formatHz(v))") }
            if let v = lim.maxSessionDurationSeconds  { lines.append("        max_session_duration: \(formatTime(v))") }
            if let v = lim.allowedProtocols           { lines.append("        allowed_protocols: [\(v.joined(separator: ", "))]") }
            lines.append("    }")
        }
        if let lim = limits.audioEntrainment {
            lines.append("    audio_entrainment {")
            if let v = lim.maxVolumePercent       { lines.append("        max_intensity: \(Int(v))%") }
            if let v = lim.maxBinauralBeatsHz     { lines.append("        max_binaural_beats: \(formatHz(v))") }
            if let v = lim.maxIsochronicTonesHz   { lines.append("        max_isochronic_tones: \(formatHz(v))") }
            lines.append("    }")
        }
        if let lim = limits.visualStimulation {
            lines.append("    visual_stimulation {")
            if let v = lim.maxFrequencyHz         { lines.append("        max_frequency: \(formatHz(v))") }
            if let v = lim.minFrequencyHz         { lines.append("        min_frequency: \(formatHz(v))") }
            if let v = lim.allowedModes           { lines.append("        allowed_modes: [\(v.joined(separator: ", "))]") }
            if let v = lim.blockHighRiskRange     { lines.append("        block_high_risk_range: \(v)") }
            lines.append("    }")
        }
        if let lim = limits.tms {
            lines.append("    tms {")
            if let v = lim.maxIntensityPercentMT  { lines.append("        max_intensity_pct_mt: \(v)") }
            if let v = lim.maxPulsesPerSession    { lines.append("        max_pulses_per_session: \(v)") }
            if let v = lim.maxPulsesPerDay        { lines.append("        max_pulses_per_day: \(v)") }
            if let v = lim.maxSessionsPerWeek     { lines.append("        max_sessions_per_week: \(v)") }
            if let v = lim.allowedProtocols       { lines.append("        allowed_protocols: [\(v.joined(separator: ", "))]") }
            if let v = lim.allowedTargets         { lines.append("        allowed_targets: [\(v.joined(separator: ", "))]") }
            lines.append("    }")
        }
        if let lim = limits.pbmDeep1170nm {
            lines.append("    pbm_deep_1170nm {")
            if let v = lim.maxIntensityMWcm2          { lines.append("        max_intensity: \(v)mW_cm2") }
            if let v = lim.maxSessionDurationSeconds   { lines.append("        max_session_duration: \(formatTime(v))") }
            lines.append("    }")
        }
        if let lim = limits.clinicalTacs {
            lines.append("    clinical_tacs {")
            if let v = lim.maxIntensityMilliamps       { lines.append("        max_intensity: \(v)mA") }
            if let v = lim.maxSessionDurationSeconds   { lines.append("        max_session_duration: \(formatTime(v))") }
            lines.append("    }")
        }
        if let lim = limits.hdTdcs {
            lines.append("    hd_tdcs {")
            if let v = lim.maxIntensityMilliamps       { lines.append("        max_intensity: \(v)mA") }
            if let v = lim.maxSessionDurationSeconds   { lines.append("        max_session_duration: \(formatTime(v))") }
            if let v = lim.allowedMontages             { lines.append("        allowed_montages: [\(v.joined(separator: ", "))]") }
            lines.append("    }")
        }
        if let lim = limits.cervicalVns {
            lines.append("    cervical_vns {")
            if let v = lim.maxIntensityMilliamps       { lines.append("        max_intensity: \(v)mA") }
            if let v = lim.maxSessionDurationSeconds   { lines.append("        max_session_duration: \(formatTime(v))") }
            lines.append("    }")
        }
        if let lim = limits.vibrotactile40hz {
            lines.append("    vibrotactile_40hz {")
            if let v = lim.maxIntensityG               { lines.append("        max_intensity: \(v)G") }
            if let v = lim.maxSessionDurationSeconds   { lines.append("        max_session_duration: \(formatTime(v))") }
            lines.append("    }")
        }

        lines.append("}")
        return lines.joined(separator: "\n")
    }

    private func serializeProtocol(_ proto: NPProtocolDefinition) -> String {
        var lines: [String] = []
        lines.append("protocol \"\(proto.name)\" {")
        lines.append("    id: \"\(proto.id.uuidString)\"")
        if !proto.description.isEmpty {
            lines.append("    description: \"\(proto.description)\"")
        }
        if proto.author != "NeurOne" {
            lines.append("    author: \"\(proto.author)\"")
        }
        lines.append("    version: \"\(proto.version)\"")
        if proto.isReadOnly {
            lines.append("    readonly: true")
        }
        if !proto.tags.isEmpty {
            lines.append("    tags: [\(proto.tags.map { serializeTag($0) }.joined(separator: ", "))]")
        }
        if !proto.conditions.isEmpty {
            let cs = proto.conditions.map { "\"\(escape($0))\"" }.joined(separator: ", ")
            lines.append("    conditions: [\(cs)]")
        }
        if !proto.references.isEmpty {
            let rs = proto.references.map { serializeReference($0) }.joined(separator: ", ")
            lines.append("    references: [\(rs)]")
        }
        switch proto.timingMode {
        case .duration(let s):
            lines.append("    duration: \(formatTime(s))")
        case .intervalCount(let n):
            lines.append("    interval_count: \(n)")
        }
        lines.append("")
        for mod in proto.modalities {
            if !mod.enabled { lines.append("    # (disabled)") }
            lines.append(contentsOf: serializeModality(mod).map { "    \($0)" })
            lines.append("")
        }
        lines.append("}")
        return lines.joined(separator: "\n")
    }

    private func serializeModality(_ mod: NPProtocolModality) -> [String] {
        var lines: [String] = []
        lines.append("\(mod.modalityType.rawValue) {")
        lines.append(contentsOf: serializeParams(mod.params).map { "    \($0)" })
        if !mod.interval.isContinuous {
            lines.append("    interval_on: \(formatTime(mod.interval.intervalOnSeconds))")
            lines.append("    interval_off: \(formatTime(mod.interval.intervalOffSeconds))")
            if let rc = mod.interval.repeatCount {
                lines.append("    repeat: \(rc)")
            } else {
                lines.append("    repeat: until_end")
            }
        }
        lines.append("}")
        return lines
    }

    private func serializeParams(_ params: NPModalityParams) -> [String] {
        switch params {
        case .pbmTranscranial(let p):
            var lines: [String] = []
            lines.append("intensity: \(Int(p.intensityPercent))%")
            lines.append("frequency: \(formatHz(p.frequencyHz))")
            if p.frequencyHz > 0 {
                lines.append("duty_cycle: \(p.dutyCyclePercent)%")
            }
            switch p.target {
            case .named(let names):
                let refs = names.map { "\"\($0)\"" }.joined(separator: ", ")
                lines.append("zones: [\(refs)]")
            case .clinicianSelected:
                lines.append("zones: clinician_selected")
            }
            lines.append("wavelength: \"\(p.wavelength.rawValue)\"")
            return lines

        case .pbmIntranasal(let p):
            return [
                "intensity: \(Int(p.intensityPercent))%",
                "frequency: \(formatHz(p.frequencyHz))",
                "duty_cycle: \(p.dutyCyclePercent)%"
            ]

        case .eegNeurofeedback(let p):
            var lines: [String] = []
            lines.append("band: \(p.band.rawValue)")
            switch p.channels {
            case .all:     lines.append("channels: all")
            case .front:   lines.append("channels: front")
            case .central: lines.append("channels: central")
            case .custom:
                let chs = (p.customChannels ?? []).map { "\"\($0)\"" }.joined(separator: ", ")
                lines.append("channels: [\(chs)]")
            }
            lines.append("closed_loop: \(p.closedLoopEnabled)")
            return lines

        case .besTacs(let p):
            return [
                "frequency: \(formatHz(p.frequencyHz))",
                "intensity: \(p.intensityMilliamps)mA",
                "waveform: \(p.waveform.rawValue)"
            ]

        case .tdcs(let p):
            let pairs = p.electrodePairs.map { pair in
                "[\(pair.map { "\"\($0)\"" }.joined(separator: ", "))]"
            }.joined(separator: ", ")
            return [
                "intensity: \(p.intensityMilliamps)mA",
                "electrode_pairs: [\(pairs)]",
                "ramp: \(p.rampSeconds)s  # hardware-enforced"
            ]

        case .vnsHRV(let p):
            var lines: [String] = []
            lines.append("frequency: \(formatHz(p.frequencyHz))")
            lines.append("intensity: \(p.intensityMilliamps)mA")
            lines.append("hrv_protocol: \(p.hrvProtocol.rawValue)")
            lines.append("breathing_rate: \(p.resonanceBreathingRate)")
            return lines

        case .audioEntrainment(let p):
            var lines: [String] = []
            if let bb = p.binauralBeatsHz { lines.append("binaural_hz: \(formatHz(bb))") }
            if let it = p.isochronicTonesHz { lines.append("isochronic_hz: \(formatHz(it))") }
            if let nt = p.noiseType { lines.append("noise: \(nt.rawValue)") } else { lines.append("noise: none") }
            lines.append("carrier_hz: \(formatHz(p.carrierHz))")
            lines.append("volume: \(Int(p.volumePercent))%")
            lines.append("eeg_adaptive: \(p.eegAdaptive)")
            lines.append("bone_conduction_pacer: \(p.boneConductionPacer)")
            return lines

        case .visualStimulation(let p):
            var lines: [String] = []
            lines.append("frequency: \(formatHz(p.frequencyHz))")
            lines.append("mode: \(p.mode.rawValue)")
            if p.mode == .emdr { lines.append("emdr_cadence: \(formatHz(p.emdrCadenceHz))") }
            if p.enableModeF { lines.append("enable_mode_f: true") }
            return lines

        case .qeeg21ch(let p):
            return [
                "montage: \(p.montage.rawValue)",
                "sloreta_enabled: \(p.sloretaEnabled)",
                "reference: \(p.reference.rawValue)"
            ]

        case .tms(let p):
            return [
                "protocol: \(p.tmsProtocol.rawValue)",
                "frequency: \(formatHz(p.frequencyHz))",
                "intensity_percent_mt: \(p.intensityPercentMT)%",
                "target: \(p.target.rawValue)",
                "pulse_count: \(p.pulseCount)"
            ]

        case .pbmDeep1170nm(let p):
            return [
                "intensity: \(p.intensityMWcm2)mW_cm2",
                "frequency: \(formatHz(p.frequencyHz))",
                "duty_cycle: \(p.dutyCyclePercent)%"
            ]

        case .clinicalTacs(let p):
            return [
                "frequency: \(formatHz(p.frequencyHz))",
                "intensity: \(p.intensityMilliamps)mA",
                "channel_count: \(p.channelCount)",
                "waveform: \(p.waveform.rawValue)"
            ]

        case .hdTdcs(let p):
            return [
                "target: \(p.target.rawValue)",
                "montage: \(p.montage.rawValue)",
                "intensity: \(p.intensityMilliamps)mA"
            ]

        case .cervicalVns(let p):
            return [
                "frequency: \(formatHz(p.frequencyHz))",
                "intensity: \(p.intensityMilliamps)mA",
                "# cardiac interlock always enforced by safety MCU"
            ]

        case .vibrotactile40hz(let p):
            return [
                "frequency: \(formatHz(p.frequencyHz))  # locked at 40Hz",
                "intensity_g: \(p.intensityG)G",
                "sync_to_audio: \(p.syncToAudio)",
                "sync_to_visual: \(p.syncToVisual)"
            ]
        }
    }

    private func serializeComposite(_ comp: NPCompositeProtocol) -> String {
        var lines: [String] = []
        lines.append("composite \"\(comp.name)\" {")
        lines.append("    id: \"\(comp.id.uuidString)\"")
        if !comp.description.isEmpty {
            lines.append("    description: \"\(comp.description)\"")
        }
        if comp.author != "NeurOne" {
            lines.append("    author: \"\(comp.author)\"")
        }
        lines.append("    version: \"\(comp.version)\"")
        if comp.isReadOnly {
            lines.append("    readonly: true")
        }
        if !comp.tags.isEmpty {
            lines.append("    tags: [\(comp.tags.map { serializeTag($0) }.joined(separator: ", "))]")
        }
        if !comp.conditions.isEmpty {
            let cs = comp.conditions.map { "\"\(escape($0))\"" }.joined(separator: ", ")
            lines.append("    conditions: [\(cs)]")
        }
        if !comp.references.isEmpty {
            let rs = comp.references.map { serializeReference($0) }.joined(separator: ", ")
            lines.append("    references: [\(rs)]")
        }
        lines.append("    conflict_resolution: \(comp.conflictResolution.rawValue)")
        lines.append("")
        for layer in comp.layers {
            lines.append("    layer \"\(layer.protocolName)\" {")
            lines.append("        start: \(formatTime(layer.startOffsetSeconds))")
            if let dur = layer.durationSeconds {
                lines.append("        duration: \(formatTime(dur))")
            }
            if abs(layer.intensityScale - 1.0) > 0.001 {
                lines.append("        intensity_scale: \(layer.intensityScale)")
            }
            lines.append("    }")
            lines.append("")
        }
        lines.append("}")
        return lines.joined(separator: "\n")
    }

    // MARK: Formatting helpers

    private func formatTime(_ seconds: Int) -> String {
        if seconds == 0 { return "0s" }
        if seconds % 3600 == 0 { return "\(seconds / 3600)h" }
        if seconds % 60 == 0   { return "\(seconds / 60)m" }
        return "\(seconds)s"
    }

    private func formatHz(_ hz: Double) -> String {
        if hz == 0 { return "0Hz  # CW" }
        if hz == Double(Int(hz)) { return "\(Int(hz))Hz" }
        return "\(hz)Hz"
    }

    // Emit a tag as a bare ident when all characters are letters, digits, or underscores.
    // Quote it otherwise (e.g. "wind-down") so that a serialize→reparse round-trip
    // preserves the original string without silent splitting on hyphens.
    private func serializeTag(_ tag: String) -> String {
        // A bare identifier may not start with a digit: a tag like "1064nm" is
        // all letters and digits but must still be quoted to re-parse (Rev 6).
        let isBareIdent = !tag.isEmpty
            && (tag.first!.isLetter || tag.first! == "_")
            && tag.allSatisfy { $0.isLetter || $0.isNumber || $0 == "_" }
        return isBareIdent ? tag : "\"\(tag.replacingOccurrences(of: "\"", with: "\\\""))\""
    }
}

// MARK: - Round-trip

func nppsRoundTrip(_ text: String) throws -> String {
    var lexer = NPPSLexer(text)
    let tokens = try lexer.tokenize()
    var parser = NPPSParser(tokens)
    let entries = try parser.parse()
    let serializer = NPPSSerializer()
    return entries.map { serializer.serialize($0) }.joined(separator: "\n\n")
}
