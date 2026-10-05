import Foundation
import CryptoKit

// The hub session descriptor (OI-AND-WIRE-01).
//
// There is ONE wire format: the binary descriptor of NP-FW-HUB-001 §4, which
// firmware/hub_control/src/np_protocol.c parses and app/web/src/lib/hubCompiler.ts writes.
// This is a port of that compiler (and of Android's HubDescriptorCompiler.kt). Where this and
// §4 disagree, §4 is right and this file is wrong (REQ-FWHUB-08).
// NeurOneTests/HubDescriptorCompilerTests.swift diffs this compiler's output against
// app/android/core/src/test/resources/hub-descriptor-golden.json, the web compiler's own bytes.
//
// The hub has no JSON parser. The `NPPR` JSON blob this app used to send was a format no hub
// could read.
//
// Layout (little-endian, packed):
//   [64]  header   magic u32 | version u16 | flags u8 | cmd_count u8 | uuid[16] |
//                  compiled_at_unix u32 | serial[32] | session_duration_ms u32
//   [N]   commands cmd_hdr(14) + target[target_len] + params[params_len]
//   [64]  Ed25519 signature over everything before it (the raw region, not a digest)

/// A compiled descriptor. `blob` is the whole wire image; the hub verifies it as is.
struct HubDescriptor {
    let blob: Data
    let sessionUUID: Data
    let isT2: Bool
    let cmdCount: Int
    /// First 8 bytes of the signing public key, hex, for hub key pinning. Empty when unsigned.
    let publicKeyFingerprint: String
}

/// A request the descriptor cannot express or the hardware cannot reach. Refused, never reshaped.
struct NPHubCompileError: Error, LocalizedError, Equatable {
    let message: String
    var errorDescription: String? { message }
}

enum HubDescriptorCompiler {

    // Mirrors np_hub_config.h; scripts/check-hub-wire-format.ts pins the web compiler's copy.
    static let magic: UInt32 = 0x4E50_4850
    static let version: UInt16 = 1
    static let uuidLen = 16
    static let serialLen = 32
    static let sigLen = 64
    static let cmdMax = 64
    static let headerLen = 64
    static let cmdHdrLen = 14
    static let socketMaskBytes = NPSocketMask.byteCount
    static let flagT2Tier: UInt8 = 1 << 0      // NP_PROTO_FLAG_T2_TIER (app-computed, carries no authority)
    static let flagAutonomous: UInt8 = 1 << 1  // NP_PROTO_FLAG_AUTONOMOUS (Mode 3); no firmware reader yet

    private static let targetSlot: UInt8 = 0x00
    private static let targetSocketMask: UInt8 = 0x01
    private static let slotNone: UInt8 = 0xFF
    private static let slotFirstValid = 5
    private static let slotMax = 19

    private static let slotEEG = 5, slotAudio = 6, slotVisual = 7, slotVnsHrv = 8, slotIntranasal = 9
    private static let slotCvns = 10, slotQeeg = 11, slotTms = 12, slotPbm1170 = 13, slotClinTacs = 14
    private static let slotHdTdcs = 15, slotVibrotactile = 16, slotBesTacs = 17, slotTdcs = 18

    private static let modPbmBase: UInt8 = 0x01, modPbmSmart: UInt8 = 0x02, modIntranasal: UInt8 = 0x03
    private static let modEEG: UInt8 = 0x04, modBesTacs: UInt8 = 0x05, modTdcs: UInt8 = 0x06
    private static let modVnsHrv: UInt8 = 0x07, modAudio: UInt8 = 0x08, modVisual: UInt8 = 0x09
    private static let modCvns: UInt8 = 0x0A, modQeeg: UInt8 = 0x0B, modTms: UInt8 = 0x0C
    private static let modPbm1170: UInt8 = 0x0D, modClinTacs: UInt8 = 0x0E, modHdTdcs: UInt8 = 0x0F
    private static let modVibrotactile: UInt8 = 0x10

    // MARK: - Public entry points

    /// Compile and sign. Ed25519 covers the raw signed region (NP-FW-HUB-001 §4.1), and the
    /// signature fills the last 64 bytes.
    ///
    /// - Parameters:
    ///   - deviceSerial: the 32-byte replay guard the hub checks (§4.2). Omitted only on the
    ///     bench: a hub with a provisioned serial refuses the descriptor (OI-AND-WIRE-02).
    ///   - clinicianSockets: operator-chosen 1-based socket ids for `clinician_selected` targets.
    static func compile(
        _ definition: NPProtocolDefinition,
        deviceSerial: Data? = nil,
        clinicianSockets: [Int]? = nil,
        autonomous: Bool = false,
        sign: (Data) throws -> (signature: Data, fingerprint: String) = SessionProtocolSigner.sign
    ) throws -> HubDescriptor {
        try signed(try build(definition, deviceSerial: deviceSerial, clinicianSockets: clinicianSockets,
                             autonomous: autonomous), using: sign)
    }

    /// Fill the trailing signature of an unsigned descriptor. Split from `compile` so the caller can
    /// tell a compile refusal from a signing failure.
    static func signed(
        _ unsigned: HubDescriptor,
        using sign: (Data) throws -> (signature: Data, fingerprint: String) = SessionProtocolSigner.sign
    ) throws -> HubDescriptor {
        let region = Data(unsigned.blob.prefix(unsigned.blob.count - sigLen))
        let result = try sign(region)
        guard result.signature.count == sigLen else {
            throw NPHubCompileError(message: "Ed25519 signature must be \(sigLen) bytes, got \(result.signature.count).")
        }
        return HubDescriptor(blob: region + result.signature, sessionUUID: unsigned.sessionUUID,
                             isT2: unsigned.isT2, cmdCount: unsigned.cmdCount,
                             publicKeyFingerprint: result.fingerprint)
    }

    /// The descriptor with a zeroed signature: the web compiler's output, which the golden test diffs.
    /// `now` and `sessionUUID` are injectable so the bytes are reproducible.
    static func build(
        _ definition: NPProtocolDefinition,
        deviceSerial: Data? = nil,
        clinicianSockets: [Int]? = nil,
        autonomous: Bool = false,
        now: Date = Date(),
        sessionUUID: [UInt8]? = nil
    ) throws -> HubDescriptor {
        let sessionDurationMs: Int
        switch definition.timingMode {
        case .duration(let s): sessionDurationMs = s * 1000
        case .intervalCount: sessionDurationMs = 0   // runs until the repeats are exhausted
        }

        var cmds: [Cmd] = []
        var isT2 = false
        for (blockIndex, modality) in definition.modalities.enumerated() where modality.enabled {
            if isT2Params(modality.params) { isT2 = true }
            let generated = try buildCommands(modality.params, modality.interval,
                                              sessionDurationMs: sessionDurationMs,
                                              clinicianSockets: clinicianSockets)
            for c in generated {
                c.block = blockIndex
                cmds.append(c)
                // Refuse, never truncate: a dropped tail is a missing modality, or a missing STOP.
                if cmds.count > cmdMax {
                    throw NPHubCompileError(message:
                        "Protocol exceeds maximum command count (\(cmdMax)). " +
                        "Reduce interval repeats or the number of enabled modalities.")
                }
            }
        }

        try mergeOverlappingPbm(&cmds, sessionDurationMs: sessionDurationMs)
        if cmds.isEmpty {
            throw NPHubCompileError(message: "Protocol produces no commands — no enabled modalities")
        }

        // The hub needs start order; ties keep declaration order (explicit index: sorted() is not documented stable).
        let sorted = cmds.enumerated()
            .sorted { ($0.element.startMs, $0.offset) < ($1.element.startMs, $1.offset) }
            .map(\.element)
        let targets = try sorted.map { try serializeTarget($0.target) }

        var out = [UInt8]()
        let uuid = sessionUUID ?? (0..<uuidLen).map { _ in UInt8.random(in: 0...255) }
        precondition(uuid.count == uuidLen)
        var serial = [UInt8](repeating: 0, count: serialLen)
        if let s = deviceSerial { for (i, b) in s.prefix(serialLen).enumerated() { serial[i] = b } }

        out.appendLE(magic)
        out.appendLE(version)
        out.append((isT2 ? flagT2Tier : 0) | (autonomous ? flagAutonomous : 0))
        out.append(UInt8(truncatingIfNeeded: sorted.count))
        out.append(contentsOf: uuid)
        out.appendLE(UInt32(truncatingIfNeeded: Int(now.timeIntervalSince1970)))
        out.append(contentsOf: serial)
        out.appendLE(UInt32(truncatingIfNeeded: sessionDurationMs))
        precondition(out.count == headerLen)

        for (i, c) in sorted.enumerated() {
            let t = targets[i]
            out.append(c.modType)
            out.append(t.slotID)
            out.appendLE(UInt32(truncatingIfNeeded: c.startMs))
            out.appendLE(UInt32(truncatingIfNeeded: c.durationMs))
            out.appendLE(UInt16(truncatingIfNeeded: c.params.count))
            out.append(t.kind)
            out.append(UInt8(truncatingIfNeeded: t.block.count))
            out.append(contentsOf: t.block)
            out.append(contentsOf: c.params)
        }
        out.append(contentsOf: [UInt8](repeating: 0, count: sigLen))

        return HubDescriptor(blob: Data(out), sessionUUID: Data(uuid), isT2: isT2,
                             cmdCount: sorted.count, publicKeyFingerprint: "")
    }

    // MARK: - Command generation

    private static func buildCommands(
        _ mp: NPModalityParams, _ interval: NPIntervalConfig,
        sessionDurationMs: Int, clinicianSockets: [Int]?
    ) throws -> [Cmd] {
        let enc = try encode(mp, clinicianSockets: clinicianSockets)

        // A block's `start` shifts its whole schedule (NP-NPPS-REF-001 §5).
        let offsetMs = (interval.startOffsetSeconds ?? 0) * 1000
        if sessionDurationMs > 0 && offsetMs >= sessionDurationMs {
            throw NPHubCompileError(message:
                "A block starts at \(Double(offsetMs) / 1000)s, at or after the session's end " +
                "(\(Double(sessionDurationMs) / 1000)s). It would never run; remove it or lengthen the session.")
        }

        if interval.isContinuous {
            return [Cmd(modType: enc.modType, target: enc.target, startMs: offsetMs, durationMs: 0, params: enc.params)]
        }

        let onMs = interval.intervalOnSeconds * 1000
        let periodMs = onMs + interval.intervalOffSeconds * 1000
        let maxRepeats = interval.repeatCount
            ?? (sessionDurationMs > 0 ? Int((Double(sessionDurationMs) / Double(periodMs)).rounded(.up)) : 1)

        var out: [Cmd] = []
        var i = 0
        while i < maxRepeats {
            let startMs = offsetMs + i * periodMs
            if sessionDurationMs > 0 && startMs >= sessionDurationMs { break }
            out.append(Cmd(modType: enc.modType, target: enc.target, startMs: startMs, durationMs: onMs, params: enc.params))
            let stopMs = startMs + onMs
            if sessionDurationMs == 0 || stopMs < sessionDurationMs {
                // The stop carries the SAME target as the on: a stop that reached other
                // sockets would leave the difference running.
                out.append(Cmd(modType: enc.modType, target: enc.target, startMs: stopMs, durationMs: 0, params: []))
            }
            if out.count > cmdMax { break }   // the caller refuses; do not run an unbounded expansion
            i += 1
        }
        return out
    }

    // MARK: - Parallel PBM blocks on one tile

    private static let pbmCurOffsets: [UInt8: [Int]] = [
        modPbmBase: [2, 3],        // cur_a (660), cur_b (808)
        modPbmSmart: [2, 3, 4],    // cur_a, cur_b, cur_c (1064); ch_mask at 5
        modIntranasal: [3, 4],     // cur_660, cur_808; side at 0
    ]

    private static func isPbmOn(_ c: Cmd) -> Bool {
        (c.modType == modPbmBase || c.modType == modPbmSmart || c.modType == modIntranasal) && !c.params.isEmpty
    }

    /// What a command lights: tile sockets, or for the probe its one slot (offset clear of the socket range).
    private static func socketsOf(_ c: Cmd) -> [Int] {
        switch c.target {
        case .sockets(let ids): return ids
        case .slot(let s): return [10_000 + s]
        }
    }

    /// Two blocks that overlap on a socket are MERGED when they have the same window, sockets,
    /// frequency and duty and drive different channels; otherwise the protocol is REFUSED. A tile
    /// takes one frequency and one duty for all its channels, and delivering one block or
    /// averaging both would be a stimulus nobody authored (CLAUDE.md §3).
    private static func mergeOverlappingPbm(_ cmds: inout [Cmd], sessionDurationMs: Int) throws {
        func endOf(_ c: Cmd) -> Double {
            if c.durationMs > 0 { return Double(c.startMs + c.durationMs) }
            return sessionDurationMs > 0 ? Double(sessionDurationMs) : .infinity
        }

        var drop = Set<ObjectIdentifier>()
        for i in cmds.indices {
            let a = cmds[i]
            guard isPbmOn(a), !drop.contains(ObjectIdentifier(a)) else { continue }
            var j = i + 1
            while j < cmds.count {
                defer { j += 1 }
                let b = cmds[j]
                guard isPbmOn(b), !drop.contains(ObjectIdentifier(b)), a.block != b.block else { continue }
                let sa = Set(socketsOf(a))
                let sbList = socketsOf(b)
                let shared = sbList.filter { sa.contains($0) }
                let overlapInTime = Double(a.startMs) < endOf(b) && Double(b.startMs) < endOf(a)
                if shared.isEmpty || !overlapInTime { continue }

                let sameSockets = shared.count == sa.count && sbList.count == sa.count
                let sameWindow = a.startMs == b.startMs && a.durationMs == b.durationMs
                let (fo, dutyOff) = a.modType == modIntranasal ? (1, 2) : (0, 1)
                let sameTiming = a.modType == b.modType
                    && a.params[fo] == b.params[fo] && a.params[dutyOff] == b.params[dutyOff]
                let curs = pbmCurOffsets[a.modType] ?? []
                let disjoint = a.modType == b.modType && curs.allSatisfy { a.params[$0] == 0 || b.params[$0] == 0 }

                if !(sameSockets && sameWindow && sameTiming && disjoint) {
                    let list = shared.prefix(6).map(String.init).joined(separator: ", ") + (shared.count > 6 ? ", …" : "")
                    throw NPHubCompileError(message:
                        "Two PBM blocks overlap on socket\(shared.count == 1 ? "" : "s") \(list) " +
                        "from \(Double(max(a.startMs, b.startMs)) / 1000)s. A tile takes one frequency, " +
                        "one duty and one schedule for all its channels, so blocks sharing a tile must " +
                        "match in timing, frequency and duty and drive different wavelengths. " +
                        "The protocol is refused rather than reshaped.")
                }
                var merged = a.params
                for k in curs { merged[k] = a.params[k] != 0 ? a.params[k] : b.params[k] }
                if a.modType == modPbmSmart { merged[5] = a.params[5] | b.params[5] }
                a.params = merged
                drop.insert(ObjectIdentifier(b))
                // b's stop, if it has one, matches a's (same sockets, same end).
                if let bStop = cmds.first(where: {
                    $0.block == b.block && $0.params.isEmpty && $0.modType == b.modType
                        && Double($0.startMs) == endOf(b)
                }) { drop.insert(ObjectIdentifier(bStop)) }
            }
        }
        cmds.removeAll { drop.contains(ObjectIdentifier($0)) }
    }

    // MARK: - Targets

    private struct SerializedTarget { let kind: UInt8; let slotID: UInt8; let block: [UInt8] }

    private static func serializeTarget(_ t: Target) throws -> SerializedTarget {
        switch t {
        case .slot(let s):
            if s < slotFirstValid || s >= slotMax {
                throw NPHubCompileError(message:
                    "Slot \(s) is not addressable: slots below \(slotFirstValid) are the " +
                    "retired zone-module slots (cranial targets use sockets), and \(slotMax) is " +
                    "the end of the slot domain.")
            }
            return SerializedTarget(kind: targetSlot, slotID: UInt8(s), block: [])
        case .sockets(let ids):
            // An empty target is a session that reports a delivered dose while lighting nothing.
            if ids.isEmpty {
                throw NPHubCompileError(message: "Command targets no sockets — resolve the zone to at least one socket.")
            }
            // NPSocketMask is the app's only socket-number-to-bit conversion (NUMBER-1).
            let mask = try NPSocketMask(sockets: ids, source: "the command target")
            return SerializedTarget(kind: targetSocketMask, slotID: slotNone, block: mask.bytes)
        }
    }

    // MARK: - Parameter encoding

    private static func encode(_ mp: NPModalityParams, clinicianSockets: [Int]?) throws -> Encoded {
        switch mp {
        case .pbmTranscranial(let p):
            // Throws on an unknown zone or a missing clinician selection: a substituted target is wrong-site stimulation.
            let target = Target.sockets(try p.resolveSocketMask(clinicianSockets: clinicianSockets).socketIDs)
            let duty = dutyReg(p.dutyCyclePercent)
            let fc = freqCode(p.frequencyHz)
            // One wavelength per block: the rules pick the channel, every other channel is commanded 0.
            let ch = try oneChannel(p.wavelength.rawValue, allowed: NPPBMChannelElement.allCases)
            let cur = try PbmDrive.irradianceToRegister(p.irradianceMWcm2, fullScale: PbmDrive.pbmFullScaleMWcm2(ch), channel: ch.rawValue)
            switch ch {
            case .led660:  return Encoded(modType: modPbmBase, target: target, params: [fc, duty, cur, 0])
            case .led808:  return Encoded(modType: modPbmBase, target: target, params: [fc, duty, 0, cur])
            case .led1064: return Encoded(modType: modPbmSmart, target: target, params: [fc, duty, 0, 0, cur, 0x04])
            }

        case .pbmIntranasal(let p):
            let ch = try oneChannel(p.wavelength.rawValue, allowed: [.led660, .led808])
            let cur = try PbmDrive.irradianceToRegister(
                p.irradianceMWcm2, fullScale: PbmDrive.intranasalFullScaleMWcm2, channel: "the intranasal probe")
            return Encoded(modType: modIntranasal, target: .slot(slotIntranasal),
                           params: [0x00, freqCode(p.frequencyHz), dutyReg(p.dutyCyclePercent),
                                    ch == .led660 ? cur : 0, ch == .led808 ? cur : 0])

        case .eegNeurofeedback(let p):
            var chMask: Int
            switch p.channels {
            case .all: chMask = 0xFF
            case .front: chMask = 0x03
            case .central: chMask = 0x3C
            case .custom: chMask = 0xFF
            }
            if p.channels == .custom, let custom = p.customChannels {
                let labels = ["Fp1", "Fp2", "F3", "F4", "C3", "C4", "P3", "P4"]
                chMask = custom.reduce(0) { m, ch in
                    if let i = labels.firstIndex(of: ch) { return m | (1 << i) }
                    return m
                }
            }
            // gain=24×, notch=60 Hz, ref=linked_ear; adaptive_out 3 = drive both PBM and audio.
            return Encoded(modType: modEEG, target: .slot(slotEEG),
                           params: [u8(chMask), 6, 2, 0, p.closedLoopEnabled ? 0x03 : 0x00])

        case .besTacs(let p):
            // 7 bytes, not 8: the struct is packed and the driver checks the length exactly.
            var b = [UInt8]()
            b.append(0)
            b.appendLE(u16(min(jsRound(p.frequencyHz * 1000), 0xFFFF)))
            b.appendLE(u16(min(jsRound(p.intensityMilliamps * 1000), 1000)))
            b.append(p.waveform == .square ? 1 : 0)
            b.append(0)
            return Encoded(modType: modBesTacs, target: .slot(slotBesTacs), params: b)

        case .tdcs(let p):
            var b = [UInt8]()
            b.append(UInt8(electrodePair(p.electrodePairs.first)))
            b.appendLE(u16(min(jsRound(p.intensityMilliamps * 1000), 2000)))
            b.append(0)
            b.appendLE(u16(max(p.rampSeconds, 30)))   // firmware enforces ≥30 s
            // OI-CHARGE-04: FLOORED, never rounded — the safety MCU derives 40 µC/cm² × area,
            // so rounding up would hand it a limit above the true ceiling.
            b.appendLE(u16(min(Int((p.electrodeAreaCm2 * 1000).rounded(.down)), 0xFFFF)))
            return Encoded(modType: modTdcs, target: .slot(slotTdcs), params: b)

        case .vnsHRV(let p):
            let proto: UInt8
            switch p.hrvProtocol {
            case .standalone: proto = 0
            case .combinedPBM: proto = 1
            case .tavnsSync: proto = 2
            case .eegBiofeedback: proto = 3
            }
            var b = [UInt8]()
            b.append(0)
            b.appendLE(u16(min(jsRound(p.frequencyHz * 1000), 25000)))
            b.appendLE(u16(min(jsRound(p.intensityMilliamps * 1000), 2000)))
            b.append(contentsOf: [0, 1, 1, proto])
            return Encoded(modType: modVnsHrv, target: .slot(slotVnsHrv), params: b)

        case .audioEntrainment(let p):
            var mode = 4   // off
            var beatMhz = 0
            if let bin = p.binauralBeatsHz { mode = 0; beatMhz = jsRound(bin * 1000) }
            else if let iso = p.isochronicTonesHz { mode = 1; beatMhz = jsRound(iso * 1000) }
            else if p.noiseType == .pink { mode = 2 }
            else if p.noiseType == .brown { mode = 3 }
            var b = [UInt8]()
            b.append(UInt8(mode))
            b.appendLE(u16(Int(min(p.carrierHz, 65535))))
            b.appendLE(u16(min(beatMhz, 0xFFFF)))
            b.append(UInt8(try PbmDrive.dbToVolumePercent(p.volumeDb)))
            b.append(p.boneConductionPacer ? 1 : 0)
            b.append(p.eegAdaptive ? 1 : 0)
            return Encoded(modType: modAudio, target: .slot(slotAudio), params: b)

        case .visualStimulation(let p):
            let mode: Int
            switch p.mode {
            case .binocular: mode = 0
            case .emdr: mode = 1
            case .retinalPBM: mode = 2
            case .modeF: mode = 3
            }
            return Encoded(modType: modVisual, target: .slot(slotVisual),
                           params: [UInt8(mode), u8(min(jsRound(p.frequencyHz), 100)), 50, 0x02, 0xFF, 0x0F,
                                    u8(min(jsRound(p.emdrCadenceHz * 10), 255)), p.enableModeF ? 1 : 0,
                                    mode == 0 ? 1 : 0])

        case .qeeg21ch(let p):
            let ref: UInt8
            switch p.reference {
            case .linkedEar: ref = 0
            case .cz: ref = 1
            case .average: ref = 2
            }
            var b = [UInt8]()
            b.appendLE(UInt32(0x1FFFFF))
            b.append(contentsOf: [6, 2, ref, p.sloretaEnabled ? 1 : 0])
            return Encoded(modType: modQeeg, target: .slot(slotQeeg), params: b)

        case .tms(let p):
            let proto: UInt8
            switch p.tmsProtocol {
            case .rTMS: proto = 0
            case .TBS: proto = 1
            case .iTBS: proto = 2
            }
            let isTbs = p.tmsProtocol != .rTMS
            let interTrain = isTbs ? 200 : jsRound(1000.0 / max(p.frequencyHz, 0.1))
            var b = [UInt8]()
            b.append(proto)
            b.append(targetIndex(p.target))
            b.appendLE(u16(min(jsRound(p.frequencyHz * 1000), 0xFFFF)))
            b.append(u8(min(p.intensityPercentMT, 255)))
            b.appendLE(u16(min(p.pulseCount, 0xFFFF)))
            b.appendLE(u16(min(interTrain, 0xFFFF)))
            b.append(isTbs ? 50 : 0)
            return Encoded(modType: modTms, target: .slot(slotTms), params: b)

        case .pbmDeep1170nm(let p):
            var b = [UInt8]()
            b.appendLE(u16(min(jsRound(p.intensityMWcm2), 1000)))
            b.append(freqCode(p.frequencyHz))
            b.append(dutyReg(p.dutyCyclePercent))
            b.append(0)
            return Encoded(modType: modPbm1170, target: .slot(slotPbm1170), params: b)

        case .clinicalTacs(let p):
            // The first channelCount channels; bits 5–7 of the extension byte stay clear (OI-TACS-01).
            let n = min(max(p.channelCount, 0), NPHardwareLimits.clinicalTacsMaxChannels)
            let fullMask = n == 0 ? 0 : (1 << n) - 1
            let wf: UInt8
            switch p.waveform {
            case .sinusoidal: wf = 0
            case .square: wf = 1
            case .triangular: wf = 2
            }
            var b = [UInt8]()
            b.appendLE(u16(min(jsRound(p.frequencyHz * 1000), 0xFFFF)))
            b.appendLE(u16(min(jsRound(p.intensityMilliamps * 1000), 4000)))
            b.append(u8(fullMask & 0xFF))
            b.append(u8((fullMask >> 8) & 0xFF))
            b.append(u8((fullMask >> 16) & 0x1F))
            b.append(wf)
            return Encoded(modType: modClinTacs, target: .slot(slotClinTacs), params: b)

        case .hdTdcs(let p):
            let montage: UInt8
            switch p.montage {
            case .ring4x1: montage = 0
            case .bilateral4x1: montage = 1
            case .standard2el: montage = 2
            }
            var b = [UInt8]()
            b.append(targetIndex(p.target))
            b.append(montage)
            b.appendLE(u16(min(jsRound(p.intensityMilliamps * 1000), 2000)))
            b.appendLE(UInt16(30))   // ramp_s; firmware enforces ≥30 s
            return Encoded(modType: modHdTdcs, target: .slot(slotHdTdcs), params: b)

        case .cervicalVns(let p):
            var b = [UInt8]()
            b.append(0)
            b.appendLE(u16(min(jsRound(p.frequencyHz * 1000), 25000)))
            b.appendLE(u16(min(jsRound(p.intensityMilliamps * 1000), 2000)))
            b.appendLE(UInt16(0))    // pulse_width_us 0 → firmware default 250 µs
            b.appendLE(UInt16(10))   // ramp_s; firmware enforces ≥10 s
            b.append(1)              // baseline_req: the cardiac interlock is required
            return Encoded(modType: modCvns, target: .slot(slotCvns), params: b)

        case .vibrotactile40hz(let p):
            let clamped = max(0.6, min(1.2, p.intensityG))
            let gain = jsRound((clamped - 0.6) / 0.6 * 127)
            var b = [UInt8]()
            b.append(u8(gain))
            b.append((p.syncToAudio ? 0x01 : 0) | (p.syncToVisual ? 0x02 : 0))
            b.appendLE(UInt16(40000))   // 40 Hz locked
            return Encoded(modType: modVibrotactile, target: .slot(slotVibrotactile), params: b)
        }
    }

    /// The one emitter channel that delivers `wavelength`, or a refusal that names why.
    private static func oneChannel(_ wavelength: String, allowed: [NPPBMChannelElement]) throws -> NPPBMChannelElement {
        switch NPWavelengthRules.default.resolveChannels(wavelength) {
        case .failure(let refusal): throw refusal
        case .success(let els):
            guard let ch = els.first, allowed.contains(ch) else {
                throw NPWavelengthRefusal(value: wavelength, reason: .unmapped)
            }
            return ch
        }
    }

    private static func electrodePair(_ pair: [String]?) -> Int {
        guard let pair, pair.count >= 2 else { return 0 }
        let a = pair[0].uppercased(), b = pair[1].uppercased()
        switch (a, b) {
        case ("F3", "F4"), ("F4", "F3"): return 0
        case ("P3", "P4"), ("P4", "P3"): return 1
        case ("FZ", "PZ"), ("PZ", "FZ"): return 2
        default: return 0
        }
    }

    private static func targetIndex(_ t: NPTMSParams.TMSTarget) -> UInt8 {
        switch t {
        case .dlpfc_l: return 0
        case .dlpfc_r: return 1
        case .vlpfc_l: return 2
        case .acc: return 3
        case .mpfc: return 4
        case .m1_l: return 5
        case .m1_r: return 6
        }
    }

    private static func isT2Params(_ p: NPModalityParams) -> Bool {
        switch p {
        case .qeeg21ch, .tms, .pbmDeep1170nm, .clinicalTacs, .hdTdcs, .cervicalVns: return true
        default: return false
        }
    }

    // MARK: - Helpers

    private struct Encoded { let modType: UInt8; let target: Target; let params: [UInt8] }

    private enum Target { case slot(Int); case sockets([Int]) }

    private final class Cmd {
        let modType: UInt8
        let target: Target
        let startMs: Int
        let durationMs: Int
        var params: [UInt8]
        var block = -1
        init(modType: UInt8, target: Target, startMs: Int, durationMs: Int, params: [UInt8]) {
            self.modType = modType; self.target = target
            self.startMs = startMs; self.durationMs = durationMs; self.params = params
        }
    }

    /// JavaScript's Math.round — half rounds up — so this encodes what hubCompiler.ts encodes.
    private static func jsRound(_ x: Double) -> Int {
        guard x.isFinite else { return 0 }
        return Int(max(-1e9, min(1e9, (x + 0.5).rounded(.down))))
    }
    private static func freqCode(_ hz: Double) -> UInt8 { hz <= 0 ? 0 : u8(jsRound(hz) & 0xFF) }
    private static func dutyReg(_ pct: Int) -> UInt8 { u8(min(pct * 2, 0x32)) }
    private static func u8(_ v: Int) -> UInt8 { UInt8(truncatingIfNeeded: v) }
    private static func u16(_ v: Int) -> UInt16 { UInt16(truncatingIfNeeded: v) }
}

// MARK: - Little-endian append

private extension Array where Element == UInt8 {
    mutating func appendLE(_ v: UInt16) { append(UInt8(v & 0xFF)); append(UInt8(v >> 8)) }
    mutating func appendLE(_ v: UInt32) { for s in stride(from: 0, to: 32, by: 8) { append(UInt8((v >> UInt32(s)) & 0xFF)) } }
}

// MARK: - Absolute dose → drive register

/// Port of app/web/src/lib/pbmDrive.ts, which owns the figures: change them there and here
/// together (register rows UC-064 to UC-066, docs/status/unjustified-choices.md). A request the
/// hardware cannot reach is REFUSED, never clamped (CLAUDE.md §3).
enum PbmDrive {
    /// Peak irradiance at the scalp each channel delivers at full drive (register 255), mW/cm².
    static func pbmFullScaleMWcm2(_ ch: NPPBMChannelElement) -> Double {
        switch ch {
        case .led660, .led808: return 403
        case .led1064: return 28
        }
    }
    /// PROVISIONAL PLACEHOLDER (UC-065): the probe has no owning specification (OI-ART-04).
    static let intranasalFullScaleMWcm2 = 100.0
    /// PROVISIONAL PLACEHOLDER (UC-066, OI-AUDIOHW-01).
    static let audioDbAtZero = 40.0
    static let audioDbPerPercent = 0.5

    private static let eps = 1e-9

    static func irradianceToRegister(_ irradiance: Double, fullScale: Double, channel: String) throws -> UInt8 {
        guard irradiance.isFinite, irradiance > 0 else {
            throw NPHubCompileError(message: "PBM irradiance must be positive, got \(irradiance) mW/cm².")
        }
        if irradiance > fullScale * (1 + eps) {
            throw NPHubCompileError(message:
                "PBM irradiance \(irradiance) mW/cm² exceeds what \(channel) delivers at full " +
                "drive (\(fullScale) mW/cm²). The protocol is refused, not reduced to fit.")
        }
        let reg = Int((irradiance / fullScale * 255 + 0.5).rounded(.down))
        return UInt8(max(1, min(255, reg)))
    }

    static func dbToVolumePercent(_ db: Double) throws -> Int {
        let pct = (db - audioDbAtZero) / audioDbPerPercent
        guard db.isFinite, pct >= -eps, pct <= 100 + eps else {
            throw NPHubCompileError(message:
                "Audio level \(db) dB SPL is outside the range the drive can deliver " +
                "(\(Int(audioDbAtZero))–\(Int(audioDbAtZero + 100 * audioDbPerPercent)) dB SPL). " +
                "The protocol is refused, not reduced to fit.")
        }
        return max(0, min(100, Int((pct + 0.5).rounded(.down))))
    }
}

// MARK: - Refusals (NP-NPPS-REF-001 §4.1a)
//
// Compiler diagnostics, English like hubCompiler.ts's on the web.

/// Why a PBM block's wavelength cannot be delivered. Thrown by the compiler.
struct NPWavelengthRefusal: Error, LocalizedError, Equatable {
    let value: String
    let reason: NPWavelengthRules.Refusal

    var errorDescription: String? {
        switch reason {
        case .retired:
            return NPWavelengthRules.retiredMessage(value)
        case .invalid:
            return "PBM wavelength '\(value)' is not a wavelength: write one value such as \"810nm\"."
        case .unmapped:
            return "No emitter channel delivers \(value) under the wavelength rules in force. " +
                "Refused, not moved to the nearest channel."
        }
    }
}

extension NPWavelengthRules {
    /// The refusal text for a retired name, naming the blocks that replace it.
    static func retiredMessage(_ value: String) -> String {
        let blocks = (retired[value] ?? []).map { "\"\($0)\"" }.joined(separator: " and ")
        return "wavelength \"\(value)\" is retired: it welded independent emitters into one block. "
            + "Write one block per wavelength (\(blocks)), each with its own irradiance."
    }
}
