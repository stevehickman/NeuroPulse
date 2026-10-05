//
//  HubDescriptorCompilerTests.swift
//  NeurOneTests
//
//  OI-AND-WIRE-01: the iOS compiler writes the descriptor of NP-FW-HUB-001 §4, byte for byte what
//  app/web/src/lib/hubCompiler.ts writes. hub-descriptor-golden.json is the web compiler's output
//  (bun scripts/gen-hub-descriptor-golden.ts), with the session UUID pinned to 0xAB and the compile
//  time to 1700000000. Android's HubDescriptorCompilerTests diffs the same file.
//
//  Subject under test: HubDescriptorCompiler (app/ios/NeurOne/Session/HubDescriptorCompiler.swift)

import XCTest
@testable import NeurOne

final class HubDescriptorCompilerTests: XCTestCase {

    // MARK: - Golden vectors

    private static func golden(file: StaticString = #filePath) throws -> [String: String] {
        // .../app/ios/NeurOneTests/<this file> → .../app/android/core/src/test/resources/
        var url = URL(fileURLWithPath: "\(file)")
        for _ in 0..<3 { url.deleteLastPathComponent() }   // → app/
        url.appendPathComponent("android/core/src/test/resources/hub-descriptor-golden.json")
        let data = try Data(contentsOf: url)
        return try JSONDecoder().decode([String: String].self, from: data)
    }

    private func hex(_ d: Data) -> String { d.map { String(format: "%02x", $0) }.joined() }

    private func build(_ d: NPProtocolDefinition, serial: Data? = nil, clinician: [Int]? = [7, 3]) throws -> HubDescriptor {
        try HubDescriptorCompiler.build(d, deviceSerial: serial, clinicianSockets: clinician,
                                        now: Date(timeIntervalSince1970: 1_700_000_000),
                                        sessionUUID: [UInt8](repeating: 0xAB, count: 16))
    }

    private func check(_ name: String, _ d: NPProtocolDefinition,
                       file: StaticString = #filePath, line: UInt = #line) throws {
        let golden = try Self.golden()
        XCTAssertEqual(hex(try build(d).blob), golden[name], name, file: file, line: line)
    }

    private func def(_ secs: Int, _ mods: NPProtocolModality...) -> NPProtocolDefinition {
        NPProtocolDefinition(name: "g", timingMode: .duration(secs), modalities: mods)
    }

    private func mod(_ p: NPModalityParams, _ i: NPIntervalConfig = .continuous) -> NPProtocolModality {
        NPProtocolModality(params: p, interval: i)
    }

    private func pbm(_ wl: String, _ irr: Double) -> NPProtocolModality {
        var p = NPPBMTranscranialParams()
        p.target = .named(["Frontal"])
        p.wavelength = NPPBMTranscranialParams.Wavelength(rawValue: wl)
        p.irradianceMWcm2 = irr
        p.frequencyHz = 40
        p.dutyCyclePercent = 25
        return mod(.pbmTranscranial(p))
    }

    func testPBMChannels() throws {
        try check("pbm660", def(1200, pbm("660nm", 200)))
        try check("pbm808", def(1200, pbm("808nm", 322)))
        try check("pbm1064", def(1200, pbm("1064nm", 28)))
    }

    func testParallelWavelengthsMergeIntoOneTileCommand() throws {
        try check("pbmMerged", def(600, pbm("660nm", 100), pbm("808nm", 200)))
    }

    func testClinicianSelectedSockets() throws {
        var p = NPPBMTranscranialParams()
        p.target = .clinicianSelected
        p.wavelength = .nm808
        p.irradianceMWcm2 = 100
        p.frequencyHz = 0
        p.dutyCyclePercent = 100
        try check("pbmClin", def(600, mod(.pbmTranscranial(p))))
    }

    func testT1Modalities() throws {
        var nasal = NPPBMIntranasalParams()
        nasal.wavelength = .nm808; nasal.irradianceMWcm2 = 60; nasal.frequencyHz = 40; nasal.dutyCyclePercent = 25
        try check("nasal", def(600, mod(.pbmIntranasal(nasal))))

        try check("eegAll", def(600, mod(.eegNeurofeedback(NPEEGNeurofeedbackParams()))))
        var eeg = NPEEGNeurofeedbackParams()
        eeg.channels = .custom; eeg.customChannels = ["Fp1", "P4"]; eeg.closedLoopEnabled = false
        try check("eegCustom", def(600, mod(.eegNeurofeedback(eeg))))

        var bes = NPBESTacsParams()
        bes.frequencyHz = 10; bes.intensityMilliamps = 0.8
        try check("bes", def(600, mod(.besTacs(bes))))

        var tdcs = NPTDCSParams()
        tdcs.intensityMilliamps = 1; tdcs.electrodePairs = [["P3", "P4"]]; tdcs.electrodeAreaCm2 = 25.5
        try check("tdcs", def(600, mod(.tdcs(tdcs))))

        var vns = NPVNSHRVParams()
        vns.hrvProtocol = .tavnsSync
        try check("vns", def(600, mod(.vnsHRV(vns))))

        var bin = NPAudioEntrainmentParams()
        bin.binauralBeatsHz = 20; bin.isochronicTonesHz = nil; bin.noiseType = nil
        bin.volumeDb = 75; bin.boneConductionPacer = false
        try check("audioBin", def(600, mod(.audioEntrainment(bin))))
        var pink = NPAudioEntrainmentParams()
        pink.binauralBeatsHz = nil; pink.isochronicTonesHz = nil; pink.noiseType = .pink
        pink.volumeDb = 60; pink.eegAdaptive = false
        try check("audioPink", def(600, mod(.audioEntrainment(pink))))

        var visual = NPVisualStimParams()
        visual.emdrCadenceHz = 1.5
        try check("visual", def(600, mod(.visualStimulation(visual))))
    }

    func testT2Modalities() throws {
        var q = NPqEEG21chParams(); q.reference = .average
        try check("qeeg", def(600, mod(.qeeg21ch(q))))
        var tms = NPTMSParams(); tms.tmsProtocol = .iTBS; tms.target = .m1_r; tms.pulseCount = 600
        try check("tms", def(600, mod(.tms(tms))))
        try check("deep", def(600, mod(.pbmDeep1170nm(NPDeepPBM1170Params()))))
        var ct = NPClinicalTacsParams(); ct.channelCount = 21; ct.waveform = .square
        try check("ctacs", def(600, mod(.clinicalTacs(ct))))
        var hd = NPHDTdcsParams(); hd.target = .dlpfc_r; hd.montage = .bilateral4x1
        try check("hdtdcs", def(600, mod(.hdTdcs(hd))))
        try check("cvns", def(600, mod(.cervicalVns(NPCervicalVnsParams()))))
        var vib = NPVibrotactileParams(); vib.syncToVisual = false
        try check("vibro", def(600, mod(.vibrotactile40hz(vib))))
        XCTAssertTrue(try build(def(600, mod(.cervicalVns(NPCervicalVnsParams())))).isT2)
    }

    func testIntervalsExpandToOnAndStopAndBlockStartShiftsTheSchedule() throws {
        var bes = NPBESTacsParams()
        bes.frequencyHz = 10; bes.intensityMilliamps = 0.8; bes.waveform = .square
        var tdcs = NPTDCSParams()
        tdcs.intensityMilliamps = 1; tdcs.electrodePairs = [["F3", "F4"]]; tdcs.rampSeconds = 45; tdcs.electrodeAreaCm2 = 10
        try check("interval", def(100,
            mod(.besTacs(bes), NPIntervalConfig(intervalOnSeconds: 20, intervalOffSeconds: 10, repeatCount: nil)),
            mod(.tdcs(tdcs), NPIntervalConfig(intervalOnSeconds: 0, intervalOffSeconds: 0, repeatCount: nil,
                                              startOffsetSeconds: 30))))
    }

    // MARK: - Signing, serial, flags

    func testSignatureCoversTheRawRegionAndLandsInTheLast64Bytes() throws {
        var seen: Data?
        let d = try HubDescriptorCompiler.compile(def(600, pbm("808nm", 100)), sign: { region in
            seen = region
            return (Data(repeating: 7, count: 64), "fp")
        })
        XCTAssertEqual(seen?.count, d.blob.count - 64)
        XCTAssertEqual(seen, d.blob.prefix(d.blob.count - 64))
        XCTAssertTrue(d.blob.suffix(64).allSatisfy { $0 == 7 })
        XCTAssertEqual(d.publicKeyFingerprint, "fp")
    }

    func testAWrongLengthSignatureIsRefused() {
        XCTAssertThrowsError(try HubDescriptorCompiler.compile(def(600, pbm("808nm", 100)),
                                                              sign: { _ in (Data(count: 63), "fp") }))
    }

    func testDeviceSerialIsTheReplayGuard() throws {
        let serial = Data((1...32).map { UInt8($0) })
        let blob = try build(def(600, pbm("808nm", 100)), serial: serial).blob
        XCTAssertEqual(blob.subdata(in: 28..<60), serial)
    }

    func testHeaderMagicAndAutonomousFlag() throws {
        let plain = try build(def(600, pbm("808nm", 100))).blob
        XCTAssertEqual(Array(plain.prefix(4)), [0x50, 0x48, 0x50, 0x4E], "NPHP, little-endian")
        XCTAssertEqual(plain[6], 0)
        let auto = try HubDescriptorCompiler.build(def(600, pbm("808nm", 100)), autonomous: true).blob
        XCTAssertEqual(auto[6], 0b10)
    }

    // MARK: - Refusals

    func testRefusalsNameTheProblemAndNeverReshape() {
        XCTAssertThrowsError(try build(def(600, pbm("808nm", 500))))      // above the 403 full scale
        XCTAssertThrowsError(try build(def(600, pbm("1064nm", 100))))     // above 28
        XCTAssertThrowsError(try build(def(600, pbm("660_808nm", 100)))) { e in
            XCTAssertTrue("\(e.localizedDescription)".contains("retired"))
        }
        XCTAssertThrowsError(try build(def(600)))                          // nothing enabled
        XCTAssertThrowsError(try build(def(60, mod(.besTacs(NPBESTacsParams()),
            NPIntervalConfig(intervalOnSeconds: 0, intervalOffSeconds: 0, repeatCount: nil, startOffsetSeconds: 60)))))
        var unknown = NPPBMTranscranialParams()
        unknown.target = .named(["No Such Zone"])
        XCTAssertThrowsError(try build(def(600, mod(.pbmTranscranial(unknown)))))
        XCTAssertThrowsError(try build(def(600, mod(.pbmTranscranial({
            var p = NPPBMTranscranialParams(); p.target = .clinicianSelected; return p
        }()))), clinician: nil))

        // Two PBM blocks on one tile that differ in duty are not one stimulus.
        var other = NPPBMTranscranialParams()
        other.target = .named(["Frontal"]); other.wavelength = .nm660
        other.irradianceMWcm2 = 100; other.frequencyHz = 40; other.dutyCyclePercent = 10
        XCTAssertThrowsError(try build(def(600, pbm("808nm", 100), mod(.pbmTranscranial(other)))))

        // More than 64 commands is refused, not truncated: the tail of an interval protocol is its STOPs.
        XCTAssertThrowsError(try build(def(3600, mod(.besTacs(NPBESTacsParams()),
            NPIntervalConfig(intervalOnSeconds: 10, intervalOffSeconds: 10, repeatCount: nil)))))
    }

    func testTheProbeRefusalReadsLikeUnmapped() {
        var nasal = NPPBMIntranasalParams()
        nasal.wavelength = .nm1064
        XCTAssertThrowsError(try build(def(600, mod(.pbmIntranasal(nasal))))) { e in
            XCTAssertEqual(e.localizedDescription,
                           "No emitter channel delivers 1064nm under the wavelength rules in force. " +
                           "Refused, not moved to the nearest channel.")
        }
    }

    // MARK: - Resolved mask on the wire (was testWireProtocolCarriesTheResolvedMask)

    func testTheTargetBlockCarriesTheResolvedMask() throws {
        var p = NPPBMTranscranialParams()
        p.target = .named(["Occipital Left"])
        let blob = try build(def(600, mod(.pbmTranscranial(p)))).blob
        // header(64) + cmd_hdr(14) → target[16]
        let mask = Array(blob.subdata(in: 78..<94))
        XCTAssertEqual(blob[64 + 12], 0x01, "target_kind = socket mask")
        XCTAssertEqual(blob[64 + 13], 16, "target_len")
        var ids: [Int] = []
        for (i, byte) in mask.enumerated() { for bit in 0..<8 where byte & UInt8(1 << bit) != 0 { ids.append(i * 8 + bit + 1) } }
        XCTAssertEqual(ids, [72, 73, 74, 77, 78])
    }
}
