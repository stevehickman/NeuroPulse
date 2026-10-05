// HubDescriptorCompiler.cs — the hub session descriptor (OI-AND-WIRE-01).
//
// There is ONE wire format: the binary descriptor of NP-FW-HUB-001 §4, which
// firmware/hub_control/src/np_protocol.c parses and app/web/src/lib/hubCompiler.ts writes. This
// is a port of that compiler (and of Android's HubDescriptorCompiler.kt and iOS's
// HubDescriptorCompiler.swift). Where this and §4 disagree, §4 is right and this file is wrong
// (REQ-FWHUB-08). NeurOne.Tests/HubDescriptorCompilerTests.cs diffs this compiler's output against
// app/NeurOneShared/TestData/hub-descriptor-golden.json, the web compiler's own bytes.
//
// The hub has no JSON parser. The `NPPR` JSON blob this app used to send was a format no hub
// could read.
//
// Layout (little-endian, packed):
//   [64]  header   magic u32 | version u16 | flags u8 | cmd_count u8 | uuid[16] |
//                  compiled_at_unix u32 | serial[32] | session_duration_ms u32
//   [N]   commands cmd_hdr(14) + target[target_len] + params[params_len]
//   [64]  Ed25519 signature over everything before it (the raw region, not a digest)

using System.Security.Cryptography;
using NeurOne.Session;

namespace NeurOne.Protocol;

/// A compiled descriptor. `Blob` is the whole wire image; the hub verifies it as is.
sealed class HubDescriptor
{
    public required byte[] Blob { get; init; }
    public required byte[] SessionUuid { get; init; }
    public required bool IsT2 { get; init; }
    public required int CmdCount { get; init; }
    /// First 8 bytes of the signing public key, hex, for hub key pinning. Empty when unsigned.
    public string PublicKeyFingerprint { get; init; } = "";
}

/// A request the descriptor cannot express or the hardware cannot reach. Refused, never reshaped.
sealed class HubCompileException(string message) : Exception(message);

static class HubDescriptorCompiler
{
    // Mirrors np_hub_config.h; scripts/check-hub-wire-format.ts pins the web compiler's copy.
    public const uint Magic = 0x4E504850;
    public const ushort Version = 1;
    public const int UuidLen = 16;
    public const int SerialLen = 32;
    public const int SigLen = 64;
    public const int CmdMax = 64;
    public const int HeaderLen = 64;
    public const int CmdHdrLen = 14;
    public const int SocketMaskBytes = 16;
    public const byte FlagT2Tier = 1 << 0;      // NP_PROTO_FLAG_T2_TIER (app-computed, carries no authority)
    public const byte FlagAutonomous = 1 << 1;  // NP_PROTO_FLAG_AUTONOMOUS (Mode 3); no firmware reader yet

    private const byte TargetSlot = 0x00;
    private const byte TargetSocketMask = 0x01;
    private const byte SlotNone = 0xFF;
    private const int SlotFirstValid = 5;
    private const int SlotMax = 19;

    private const int SlotEeg = 5, SlotAudio = 6, SlotVisual = 7, SlotVnsHrv = 8, SlotIntranasal = 9,
        SlotCvns = 10, SlotQeeg = 11, SlotTms = 12, SlotPbm1170 = 13, SlotClinTacs = 14,
        SlotHdTdcs = 15, SlotVibrotactile = 16, SlotBesTacs = 17, SlotTdcs = 18;

    private const byte ModPbmBase = 0x01, ModPbmSmart = 0x02, ModIntranasal = 0x03, ModEeg = 0x04,
        ModBesTacs = 0x05, ModTdcs = 0x06, ModVnsHrv = 0x07, ModAudio = 0x08, ModVisual = 0x09,
        ModCvns = 0x0A, ModQeeg = 0x0B, ModTms = 0x0C, ModPbm1170 = 0x0D, ModClinTacs = 0x0E,
        ModHdTdcs = 0x0F, ModVibrotactile = 0x10;

    /// Peak irradiance at the scalp each channel delivers at full drive (register 255), mW/cm².
    /// Port of app/web/src/lib/pbmDrive.ts, which owns the figures: change them there and here
    /// together (register rows UC-064 to UC-066, docs/status/unjustified-choices.md).
    private static double PbmFullScaleMwCm2(PbmChannelElement ch) => ch == PbmChannelElement.Led1064 ? 28 : 403;
    /// PROVISIONAL PLACEHOLDER (UC-065): the probe has no owning specification (OI-ART-04).
    private const double IntranasalFullScaleMwCm2 = 100;
    /// PROVISIONAL PLACEHOLDER (UC-066, OI-AUDIOHW-01).
    private const double AudioDbAtZero = 40;
    private const double AudioDbPerPercent = 0.5;
    private const double Eps = 1e-9;

    // ── public entry points ──────────────────────────────────────────────────

    /// Compile and sign. Ed25519 covers the raw signed region and fills the last 64 bytes.
    /// `deviceSerial` is the 32-byte replay guard the hub checks (§4.2); omitted only on the bench,
    /// where a hub with a provisioned serial refuses the descriptor (OI-AND-WIRE-02).
    /// `clinicianSockets` are operator-chosen 1-based socket ids for `clinician_selected` targets.
    public static HubDescriptor Compile(
        NPProtocolDefinition definition,
        byte[]? deviceSerial = null,
        IReadOnlyList<int>? clinicianSockets = null,
        bool autonomous = false,
        Func<byte[], (byte[] Signature, string Fingerprint)>? sign = null)
        => Signed(Build(definition, deviceSerial, clinicianSockets, autonomous), sign);

    /// Fill the trailing signature of an unsigned descriptor. Split from Compile so a caller can
    /// tell a compile refusal from a signing failure.
    public static HubDescriptor Signed(HubDescriptor unsigned, Func<byte[], (byte[] Signature, string Fingerprint)>? sign = null)
    {
        var region = unsigned.Blob[..^SigLen];
        var (signature, fingerprint) = (sign ?? SessionProtocolSigner.Sign)(region);
        if (signature.Length != SigLen)
            throw new HubCompileException($"Ed25519 signature must be {SigLen} bytes, got {signature.Length}.");
        var blob = new byte[unsigned.Blob.Length];
        region.CopyTo(blob, 0);
        signature.CopyTo(blob, region.Length);
        return new HubDescriptor
        {
            Blob = blob, SessionUuid = unsigned.SessionUuid, IsT2 = unsigned.IsT2,
            CmdCount = unsigned.CmdCount, PublicKeyFingerprint = fingerprint
        };
    }

    /// The descriptor with a zeroed signature: the web compiler's output, which the golden test diffs.
    /// `now` and `sessionUuid` are injectable so the bytes are reproducible.
    public static HubDescriptor Build(
        NPProtocolDefinition definition,
        byte[]? deviceSerial = null,
        IReadOnlyList<int>? clinicianSockets = null,
        bool autonomous = false,
        DateTimeOffset? now = null,
        byte[]? sessionUuid = null)
    {
        // interval_count protocols run until the repeats are exhausted: no session end.
        long sessionDurationMs = definition.Timing == NPProtocolDefinition.TimingMode.Duration
            ? (long)definition.TimingValue * 1000
            : 0;

        var cmds = new List<Cmd>();
        var isT2 = false;
        for (var blockIndex = 0; blockIndex < definition.Modalities.Length; blockIndex++)
        {
            var modality = definition.Modalities[blockIndex];
            if (!modality.Enabled) continue;
            if (IsT2Params(modality.Params)) isT2 = true;
            foreach (var c in BuildCommands(modality.Params, modality.Interval, sessionDurationMs, clinicianSockets))
            {
                c.Block = blockIndex;
                cmds.Add(c);
                // Refuse, never truncate: a dropped tail is a missing modality, or a missing STOP.
                if (cmds.Count > CmdMax)
                    throw new HubCompileException(
                        $"Protocol exceeds maximum command count ({CmdMax}). " +
                        "Reduce interval repeats or the number of enabled modalities.");
            }
        }

        MergeOverlappingPbm(cmds, sessionDurationMs);
        if (cmds.Count == 0)
            throw new HubCompileException("Protocol produces no commands — no enabled modalities");

        // The hub needs start order; ties keep declaration order (OrderBy is a stable sort).
        var sorted = cmds.OrderBy(c => c.StartMs).ToList();
        var targets = sorted.Select(c => SerializeTarget(c.Target)).ToList();

        var uuid = sessionUuid ?? RandomNumberGenerator.GetBytes(UuidLen);
        if (uuid.Length != UuidLen) throw new ArgumentException($"session UUID must be {UuidLen} bytes", nameof(sessionUuid));
        var serial = new byte[SerialLen];
        if (deviceSerial is not null) Array.Copy(deviceSerial, serial, Math.Min(deviceSerial.Length, SerialLen));

        var w = new Writer();
        w.U32(Magic);
        w.U16(Version);
        w.U8((byte)((isT2 ? FlagT2Tier : 0) | (autonomous ? FlagAutonomous : 0)));
        w.U8(unchecked((byte)sorted.Count));
        w.Bytes(uuid);
        w.U32(unchecked((uint)(now ?? DateTimeOffset.UtcNow).ToUnixTimeSeconds()));
        w.Bytes(serial);
        w.U32(unchecked((uint)sessionDurationMs));
        if (w.Length != HeaderLen) throw new InvalidOperationException("header layout drifted from NP-FW-HUB-001 §4.2");

        for (var i = 0; i < sorted.Count; i++)
        {
            var c = sorted[i];
            var t = targets[i];
            w.U8(c.ModType);
            w.U8(t.SlotId);
            w.U32(unchecked((uint)c.StartMs));
            w.U32(unchecked((uint)c.DurationMs));
            w.U16(unchecked((ushort)c.Params.Length));
            w.U8(t.Kind);
            w.U8(unchecked((byte)t.Block.Length));
            w.Bytes(t.Block);
            w.Bytes(c.Params);
        }
        w.Bytes(new byte[SigLen]);

        return new HubDescriptor { Blob = w.ToArray(), SessionUuid = uuid, IsT2 = isT2, CmdCount = sorted.Count };
    }

    // ── command generation ───────────────────────────────────────────────────

    private static List<Cmd> BuildCommands(
        NPModalityParams mp, NPIntervalConfig interval, long sessionDurationMs, IReadOnlyList<int>? clinicianSockets)
    {
        var enc = Encode(mp, clinicianSockets);

        // A block's `start` shifts its whole schedule (NP-NPPS-REF-001 §5).
        long offsetMs = (long)interval.StartOffsetSeconds * 1000;
        if (sessionDurationMs > 0 && offsetMs >= sessionDurationMs)
            throw new HubCompileException(
                $"A block starts at {offsetMs / 1000.0}s, at or after the session's end " +
                $"({sessionDurationMs / 1000.0}s). It would never run; remove it or lengthen the session.");

        if (interval.IsContinuous)
            return [new Cmd(enc.ModType, enc.Target, offsetMs, 0, enc.Params)];

        long onMs = (long)interval.IntervalOnSeconds * 1000;
        long periodMs = onMs + (long)interval.IntervalOffSeconds * 1000;
        long maxRepeats = interval.RepeatCount
            ?? (sessionDurationMs > 0 ? (long)Math.Ceiling((double)sessionDurationMs / periodMs) : 1);

        var output = new List<Cmd>();
        for (long i = 0; i < maxRepeats; i++)
        {
            long startMs = offsetMs + i * periodMs;
            if (sessionDurationMs > 0 && startMs >= sessionDurationMs) break;
            output.Add(new Cmd(enc.ModType, enc.Target, startMs, onMs, enc.Params));
            long stopMs = startMs + onMs;
            if (sessionDurationMs == 0 || stopMs < sessionDurationMs)
                // The stop carries the SAME target as the on: a stop that reached other sockets
                // would leave the difference running.
                output.Add(new Cmd(enc.ModType, enc.Target, stopMs, 0, []));
            if (output.Count > CmdMax) break;   // the caller refuses; do not run an unbounded expansion
        }
        return output;
    }

    // ── parallel PBM blocks on one tile ──────────────────────────────────────

    /// Channel-current byte offsets inside each PBM params struct (NP-FW-HUB-001 §4.6).
    private static int[] PbmCurOffsets(byte modType) => modType switch
    {
        ModPbmBase => [2, 3],        // cur_a (660), cur_b (808)
        ModPbmSmart => [2, 3, 4],    // cur_a, cur_b, cur_c (1064); ch_mask at 5
        ModIntranasal => [3, 4],     // cur_660, cur_808; side at 0
        _ => []
    };

    private static bool IsPbmOn(Cmd c) =>
        (c.ModType is ModPbmBase or ModPbmSmart or ModIntranasal) && c.Params.Length > 0;

    /// What a command lights: tile sockets, or for the probe its one slot (offset clear of the socket range).
    private static int[] SocketsOf(Cmd c) => c.Target switch
    {
        SocketsTarget s => s.Ids,
        SlotTarget s => [10_000 + s.Slot],
        _ => []
    };

    /// Two blocks that overlap on a socket are MERGED when they have the same window, sockets,
    /// frequency and duty and drive different channels; otherwise the protocol is REFUSED. A tile
    /// takes one frequency and one duty for all its channels, and delivering one block or
    /// averaging both would be a stimulus nobody authored (CLAUDE.md §3).
    private static void MergeOverlappingPbm(List<Cmd> cmds, long sessionDurationMs)
    {
        double EndOf(Cmd c) => c.DurationMs > 0 ? c.StartMs + c.DurationMs
            : sessionDurationMs > 0 ? sessionDurationMs : double.PositiveInfinity;

        var drop = new HashSet<Cmd>();
        for (var i = 0; i < cmds.Count; i++)
        {
            var a = cmds[i];
            if (!IsPbmOn(a) || drop.Contains(a)) continue;
            for (var j = i + 1; j < cmds.Count; j++)
            {
                var b = cmds[j];
                if (!IsPbmOn(b) || drop.Contains(b) || a.Block == b.Block) continue;
                var sa = new HashSet<int>(SocketsOf(a));
                var sb = SocketsOf(b);
                var shared = sb.Where(sa.Contains).ToArray();
                var overlapInTime = a.StartMs < EndOf(b) && b.StartMs < EndOf(a);
                if (shared.Length == 0 || !overlapInTime) continue;

                var sameSockets = shared.Length == sa.Count && sb.Length == sa.Count;
                var sameWindow = a.StartMs == b.StartMs && a.DurationMs == b.DurationMs;
                var (fo, dutyOff) = a.ModType == ModIntranasal ? (1, 2) : (0, 1);
                var sameTiming = a.ModType == b.ModType
                    && a.Params[fo] == b.Params[fo] && a.Params[dutyOff] == b.Params[dutyOff];
                var curs = PbmCurOffsets(a.ModType);
                var disjoint = a.ModType == b.ModType && curs.All(k => a.Params[k] == 0 || b.Params[k] == 0);

                if (!(sameSockets && sameWindow && sameTiming && disjoint))
                    throw new HubCompileException(
                        $"Two PBM blocks overlap on socket{(shared.Length == 1 ? "" : "s")} " +
                        $"{string.Join(", ", shared.Take(6))}{(shared.Length > 6 ? ", …" : "")} " +
                        $"from {Math.Max(a.StartMs, b.StartMs) / 1000.0}s. A tile takes one frequency, " +
                        "one duty and one schedule for all its channels, so blocks sharing a tile must " +
                        "match in timing, frequency and duty and drive different wavelengths. " +
                        "The protocol is refused rather than reshaped.");

                var merged = (byte[])a.Params.Clone();
                foreach (var k in curs) merged[k] = a.Params[k] != 0 ? a.Params[k] : b.Params[k];
                if (a.ModType == ModPbmSmart) merged[5] = (byte)(a.Params[5] | b.Params[5]);
                a.Params = merged;
                drop.Add(b);
                // b's stop, if it has one, matches a's (same sockets, same end).
                var bStop = cmds.FirstOrDefault(c => c.Block == b.Block && c.Params.Length == 0
                    && c.ModType == b.ModType && c.StartMs == EndOf(b));
                if (bStop is not null) drop.Add(bStop);
            }
        }
        cmds.RemoveAll(drop.Contains);
    }

    // ── targets ──────────────────────────────────────────────────────────────

    private sealed record SerializedTarget(byte Kind, byte SlotId, byte[] Block);

    private static SerializedTarget SerializeTarget(CmdTarget t)
    {
        switch (t)
        {
            case SlotTarget s:
                if (s.Slot < SlotFirstValid || s.Slot >= SlotMax)
                    throw new HubCompileException(
                        $"Slot {s.Slot} is not addressable: slots below {SlotFirstValid} are the " +
                        $"retired zone-module slots (cranial targets use sockets), and {SlotMax} is " +
                        "the end of the slot domain.");
                return new SerializedTarget(TargetSlot, (byte)s.Slot, []);
            case SocketsTarget s:
                // An empty target is a session that reports a delivered dose while lighting nothing.
                if (s.Ids.Length == 0)
                    throw new HubCompileException("Command targets no sockets — resolve the zone to at least one socket.");
                return new SerializedTarget(TargetSocketMask, SlotNone, SocketBitmap(s.Ids));
            default:
                throw new HubCompileException("Unknown command target.");
        }
    }

    /// 1-based NPPS socket ids → the firmware's 0-based LSB-first bitmap; duplicates set one bit.
    private static byte[] SocketBitmap(IEnumerable<int> ids)
    {
        var mask = new byte[SocketMaskBytes];
        foreach (var id in ids)
        {
            if (!SocketLattice.IsValid(id))
                throw new HubCompileException(
                    $"Socket {id} is not a socket on this helmet (valid ids are {SocketLattice.RangeLabel}). " +
                    "Check the zone's socket list.");
            var bit = id - SocketLattice.NumberingBase;
            mask[bit >> 3] |= (byte)(1 << (bit & 7));
        }
        return mask;
    }

    // ── parameter encoding ───────────────────────────────────────────────────

    private static Encoded Encode(NPModalityParams mp, IReadOnlyList<int>? clinicianSockets)
    {
        switch (mp)
        {
            case NPModalityParams.PbmTranscranial m:
            {
                var p = m.P;
                // Throws on an unknown zone or a missing clinician selection: a substituted target is wrong-site stimulation.
                var target = new SocketsTarget(p.Target.ResolveSockets(clinicianSockets));
                var duty = DutyReg(p.DutyCyclePercent);
                var fc = FreqCode(p.FrequencyHz);
                // One wavelength per block: the rules pick the channel, every other channel is commanded 0.
                var ch = OneChannel(p.Wavelength, [PbmChannelElement.Led660, PbmChannelElement.Led808, PbmChannelElement.Led1064]);
                var cur = IrradianceToRegister(p.IrradianceMwCm2, PbmFullScaleMwCm2(ch), ch switch
                {
                    PbmChannelElement.Led660 => "led_660",
                    PbmChannelElement.Led808 => "led_808",
                    _ => "led_1064"
                });
                return ch switch
                {
                    PbmChannelElement.Led660 => new Encoded(ModPbmBase, target, [fc, duty, cur, 0]),
                    PbmChannelElement.Led808 => new Encoded(ModPbmBase, target, [fc, duty, 0, cur]),
                    _ => new Encoded(ModPbmSmart, target, [fc, duty, 0, 0, cur, 0x04]),
                };
            }

            case NPModalityParams.PbmIntranasal m:
            {
                var p = m.P;
                var ch = OneChannel(p.Wavelength, [PbmChannelElement.Led660, PbmChannelElement.Led808]);
                var cur = IrradianceToRegister(p.IrradianceMwCm2, IntranasalFullScaleMwCm2, "the intranasal probe");
                return new Encoded(ModIntranasal, new SlotTarget(SlotIntranasal),
                    [0x00, FreqCode(p.FrequencyHz), DutyReg(p.DutyCyclePercent),
                     ch == PbmChannelElement.Led660 ? cur : (byte)0, ch == PbmChannelElement.Led808 ? cur : (byte)0]);
            }

            case NPModalityParams.EegNeurofeedback m:
            {
                var p = m.P;
                var chMask = p.Channels switch
                {
                    EegNeurofeedbackParams.ChannelSelection.All => 0xFF,
                    EegNeurofeedbackParams.ChannelSelection.Front => 0x03,
                    EegNeurofeedbackParams.ChannelSelection.Central => 0x3C,
                    _ => 0xFF,
                };
                if (p.Channels == EegNeurofeedbackParams.ChannelSelection.Custom && p.CustomChannels is not null)
                {
                    string[] labels = ["Fp1", "Fp2", "F3", "F4", "C3", "C4", "P3", "P4"];
                    chMask = p.CustomChannels.Aggregate(0, (acc, ch) =>
                    {
                        var i = Array.IndexOf(labels, ch);
                        return i >= 0 ? acc | (1 << i) : acc;
                    });
                }
                // gain=24×, notch=60 Hz, ref=linked_ear; adaptive_out 3 = drive both PBM and audio.
                return new Encoded(ModEeg, new SlotTarget(SlotEeg),
                    [U8(chMask), 6, 2, 0, p.ClosedLoopEnabled ? (byte)0x03 : (byte)0x00]);
            }

            case NPModalityParams.BesTacs m:
            {
                var p = m.P;
                // 7 bytes, not 8: the struct is packed and the driver checks the length exactly.
                var w = new Writer();
                w.U8(0);
                w.U16(U16(Math.Min(JsRound(p.FrequencyHz * 1000), 0xFFFF)));
                w.U16(U16(Math.Min(JsRound(p.IntensityMilliamps * 1000), 1000)));
                w.U8(p.WaveformType == BesTacsParams.Waveform.Square ? (byte)1 : (byte)0);
                w.U8(0);
                return new Encoded(ModBesTacs, new SlotTarget(SlotBesTacs), w.ToArray());
            }

            case NPModalityParams.Tdcs m:
            {
                var p = m.P;
                var w = new Writer();
                w.U8(ElectrodePair(p.ElectrodePairs.FirstOrDefault()));
                w.U16(U16(Math.Min(JsRound(p.IntensityMilliamps * 1000), 2000)));
                w.U8(0);
                w.U16(U16(Math.Max(p.RampSeconds, 30)));   // firmware enforces ≥30 s
                // OI-CHARGE-04: FLOORED, never rounded — the safety MCU derives 40 µC/cm² × area,
                // so rounding up would hand it a limit above the true ceiling.
                w.U16(U16((int)Math.Min(Math.Floor(p.ElectrodeAreaCm2 * 1000), 0xFFFF)));
                return new Encoded(ModTdcs, new SlotTarget(SlotTdcs), w.ToArray());
            }

            case NPModalityParams.VnsHrv m:
            {
                var p = m.P;
                var proto = p.HrvProtocolMode switch
                {
                    VnsHrvParams.HrvProtocol.Standalone => 0,
                    VnsHrvParams.HrvProtocol.CombinedPbm => 1,
                    VnsHrvParams.HrvProtocol.TavnsSync => 2,
                    _ => 3,   // EegBiofeedback
                };
                var w = new Writer();
                w.U8(0);
                w.U16(U16(Math.Min(JsRound(p.FrequencyHz * 1000), 25000)));
                w.U16(U16(Math.Min(JsRound(p.IntensityMilliamps * 1000), 2000)));
                w.U8(0); w.U8(1); w.U8(1); w.U8((byte)proto);
                return new Encoded(ModVnsHrv, new SlotTarget(SlotVnsHrv), w.ToArray());
            }

            case NPModalityParams.AudioEntrainment m:
                return EncodeAudio(m.P);

            case NPModalityParams.VisualStimulation m:
            {
                var p = m.P;
                var mode = p.Mode switch
                {
                    VisualStimParams.VisualMode.Binocular => 0,
                    VisualStimParams.VisualMode.Emdr => 1,
                    VisualStimParams.VisualMode.RetinalPbm => 2,
                    _ => 3,   // ModeF
                };
                return new Encoded(ModVisual, new SlotTarget(SlotVisual),
                    [(byte)mode, U8(Math.Min(JsRound(p.FrequencyHz), 100)), 50, 0x02, 0xFF, 0x0F,
                     U8(Math.Min(JsRound(p.EmdrCadenceHz * 10), 255)), p.EnableModeF ? (byte)1 : (byte)0,
                     mode == 0 ? (byte)1 : (byte)0]);
            }

            case NPModalityParams.QEeg21ch m:
            {
                var p = m.P;
                var w = new Writer();
                w.U32(0x1FFFFF);
                w.U8(6); w.U8(2);
                w.U8((byte)(int)p.ReferenceMode);   // linked_ear 0, cz 1, average 2
                w.U8(p.SloretaEnabled ? (byte)1 : (byte)0);
                return new Encoded(ModQeeg, new SlotTarget(SlotQeeg), w.ToArray());
            }

            case NPModalityParams.Tms m:
            {
                var p = m.P;
                var isTbs = p.Protocol != TmsParams.TmsProtocol.RTms;
                var interTrain = isTbs ? 200 : JsRound(1000.0 / Math.Max(p.FrequencyHz, 0.1));
                var w = new Writer();
                w.U8((byte)(int)p.Protocol);   // rTMS 0, TBS 1, iTBS 2
                w.U8((byte)(int)p.Target);
                w.U16(U16(Math.Min(JsRound(p.FrequencyHz * 1000), 0xFFFF)));
                w.U8(U8(Math.Min(p.IntensityPercentMt, 255)));
                w.U16(U16(Math.Min(p.PulseCount, 0xFFFF)));
                w.U16(U16(Math.Min(interTrain, 0xFFFF)));
                w.U8(isTbs ? (byte)50 : (byte)0);
                return new Encoded(ModTms, new SlotTarget(SlotTms), w.ToArray());
            }

            case NPModalityParams.PbmDeep1170nm m:
            {
                var p = m.P;
                var w = new Writer();
                w.U16(U16(Math.Min(JsRound(p.IntensityMwCm2), 1000)));
                w.U8(FreqCode(p.FrequencyHz));
                w.U8(DutyReg(p.DutyCyclePercent));
                w.U8(0);
                return new Encoded(ModPbm1170, new SlotTarget(SlotPbm1170), w.ToArray());
            }

            case NPModalityParams.ClinicalTacs m:
            {
                var p = m.P;
                // The first channelCount channels; bits 5–7 of the extension byte stay clear (OI-TACS-01).
                var n = Math.Min(Math.Max(p.ChannelCount, 0), 21);   // NPHardwareLimits.clinicalTacsMaxChannels
                var fullMask = n == 0 ? 0 : (1 << n) - 1;
                var wf = p.WaveformType switch
                {
                    BesTacsParams.Waveform.Sinusoidal => 0,
                    BesTacsParams.Waveform.Square => 1,
                    _ => 2,
                };
                var w = new Writer();
                w.U16(U16(Math.Min(JsRound(p.FrequencyHz * 1000), 0xFFFF)));
                w.U16(U16(Math.Min(JsRound(p.IntensityMilliamps * 1000), 4000)));
                w.U8(U8(fullMask & 0xFF));
                w.U8(U8((fullMask >> 8) & 0xFF));
                w.U8(U8((fullMask >> 16) & 0x1F));
                w.U8((byte)wf);
                return new Encoded(ModClinTacs, new SlotTarget(SlotClinTacs), w.ToArray());
            }

            case NPModalityParams.HdTdcs m:
            {
                var p = m.P;
                var w = new Writer();
                w.U8((byte)(int)p.Target);
                w.U8((byte)(int)p.MontageMode);   // ring_4x1 0, bilateral_4x1 1, standard_2_electrode 2
                w.U16(U16(Math.Min(JsRound(p.IntensityMilliamps * 1000), 2000)));
                w.U16(30);   // ramp_s; firmware enforces ≥30 s
                return new Encoded(ModHdTdcs, new SlotTarget(SlotHdTdcs), w.ToArray());
            }

            case NPModalityParams.CervicalVns m:
            {
                var p = m.P;
                var w = new Writer();
                w.U8(0);
                w.U16(U16(Math.Min(JsRound(p.FrequencyHz * 1000), 25000)));
                w.U16(U16(Math.Min(JsRound(p.IntensityMilliamps * 1000), 2000)));
                w.U16(0);    // pulse_width_us 0 → firmware default 250 µs
                w.U16(10);   // ramp_s; firmware enforces ≥10 s
                w.U8(1);     // baseline_req: the cardiac interlock is required
                return new Encoded(ModCvns, new SlotTarget(SlotCvns), w.ToArray());
            }

            case NPModalityParams.Vibrotactile40hz m:
            {
                var p = m.P;
                var clamped = Math.Max(0.6, Math.Min(1.2, p.IntensityG));
                var gain = JsRound((clamped - 0.6) / 0.6 * 0x7F);
                var w = new Writer();
                w.U8(U8(gain));
                w.U8((byte)((p.SyncToAudio ? 0x01 : 0) | (p.SyncToVisual ? 0x02 : 0)));
                w.U16(40000);   // 40 Hz locked
                return new Encoded(ModVibrotactile, new SlotTarget(SlotVibrotactile), w.ToArray());
            }

            default:
                throw new HubCompileException($"Unknown modality type in hub compiler: {mp.GetType().Name}");
        }
    }

    private static Encoded EncodeAudio(AudioEntrainmentParams p)
    {
        var mode = 4;   // off
        var beatMhz = 0;
        if (p.BinauralBeatsHz is { } bin) { mode = 0; beatMhz = JsRound(bin * 1000); }
        else if (p.IsochronicTonesHz is { } iso) { mode = 1; beatMhz = JsRound(iso * 1000); }
        else if (p.NoiseTypeMode == AudioEntrainmentParams.NoiseType.Pink) mode = 2;
        else if (p.NoiseTypeMode == AudioEntrainmentParams.NoiseType.Brown) mode = 3;

        var w = new Writer();
        w.U8((byte)mode);
        w.U16(U16((int)Math.Min(p.CarrierHz, 65535)));
        w.U16(U16(Math.Min(beatMhz, 0xFFFF)));
        w.U8((byte)DbToVolumePercent(p.VolumeDb));
        w.U8(p.BoneConductionPacer ? (byte)1 : (byte)0);
        w.U8(p.EegAdaptive ? (byte)1 : (byte)0);
        return new Encoded(ModAudio, new SlotTarget(SlotAudio), w.ToArray());
    }

    /// The one emitter channel that delivers `wavelength`, or a refusal that names why.
    private static PbmChannelElement OneChannel(string wavelength, PbmChannelElement[] allowed)
    {
        var channels = WavelengthRulesEngine.ResolveChannels(wavelength, WavelengthRulesEngine.Default, out var refusal);
        if (channels is null)
            throw new HubCompileException(refusal switch
            {
                "retired" => WavelengthRulesEngine.RetiredMessage(wavelength),
                "invalid" => $"PBM wavelength '{wavelength}' is not a wavelength: write one value such as \"810nm\".",
                _ => UnmappedMessage(wavelength)
            });
        var ch = channels[0];
        // Same text as iOS and Android: a channel this modality does not carry reads as "unmapped".
        if (!allowed.Contains(ch)) throw new HubCompileException(UnmappedMessage(wavelength));
        return ch;
    }

    private static string UnmappedMessage(string w) =>
        $"No emitter channel delivers {w} under the wavelength rules in force. Refused, not moved to the nearest channel.";

    private static byte ElectrodePair(string[]? pair)
    {
        if (pair is null || pair.Length < 2) return 0;
        var a = pair[0].ToUpperInvariant();
        var b = pair[1].ToUpperInvariant();
        return (a, b) switch
        {
            ("F3", "F4") or ("F4", "F3") => 0,
            ("P3", "P4") or ("P4", "P3") => 1,
            ("FZ", "PZ") or ("PZ", "FZ") => 2,
            _ => 0,
        };
    }

    private static bool IsT2Params(NPModalityParams p) =>
        p is NPModalityParams.QEeg21ch or NPModalityParams.Tms or NPModalityParams.PbmDeep1170nm
          or NPModalityParams.ClinicalTacs or NPModalityParams.HdTdcs or NPModalityParams.CervicalVns;

    // ── absolute dose → drive register (a request the hardware cannot reach is REFUSED, never clamped) ──

    private static byte IrradianceToRegister(double irradianceMwCm2, double fullScaleMwCm2, string channel)
    {
        if (!double.IsFinite(irradianceMwCm2) || irradianceMwCm2 <= 0)
            throw new HubCompileException($"PBM irradiance must be positive, got {irradianceMwCm2} mW/cm².");
        if (irradianceMwCm2 > fullScaleMwCm2 * (1 + Eps))
            throw new HubCompileException(
                $"PBM irradiance {irradianceMwCm2} mW/cm² exceeds what {channel} delivers at full " +
                $"drive ({fullScaleMwCm2} mW/cm²). The protocol is refused, not reduced to fit.");
        return (byte)Math.Max(1, Math.Min(255, JsRound(irradianceMwCm2 / fullScaleMwCm2 * 255)));
    }

    private static int DbToVolumePercent(double db)
    {
        var pct = (db - AudioDbAtZero) / AudioDbPerPercent;
        if (!double.IsFinite(db) || pct < -Eps || pct > 100 + Eps)
            throw new HubCompileException(
                $"Audio level {db} dB SPL is outside the range the drive can deliver " +
                $"({(int)AudioDbAtZero}–{(int)(AudioDbAtZero + 100 * AudioDbPerPercent)} dB SPL). " +
                "The protocol is refused, not reduced to fit.");
        return Math.Max(0, Math.Min(100, JsRound(pct)));
    }

    // ── helpers ──────────────────────────────────────────────────────────────

    private sealed record Encoded(byte ModType, CmdTarget Target, byte[] Params);

    private abstract class CmdTarget { }
    private sealed class SlotTarget(int slot) : CmdTarget { public int Slot { get; } = slot; }
    private sealed class SocketsTarget(int[] ids) : CmdTarget { public int[] Ids { get; } = ids; }

    private sealed class Cmd(byte modType, CmdTarget target, long startMs, long durationMs, byte[] prms)
    {
        public byte ModType { get; } = modType;
        public CmdTarget Target { get; } = target;
        public long StartMs { get; } = startMs;
        public long DurationMs { get; } = durationMs;
        public byte[] Params { get; set; } = prms;
        public int Block { get; set; } = -1;
    }

    /// Little-endian byte writer.
    private sealed class Writer
    {
        private readonly List<byte> _b = [];
        public int Length => _b.Count;
        public void U8(byte v) => _b.Add(v);
        public void U16(ushort v) { _b.Add((byte)(v & 0xFF)); _b.Add((byte)(v >> 8)); }
        public void U32(uint v) { for (var s = 0; s < 32; s += 8) _b.Add((byte)((v >> s) & 0xFF)); }
        public void Bytes(byte[] v) => _b.AddRange(v);
        public byte[] ToArray() => _b.ToArray();
    }

    /// JavaScript's Math.round — half rounds up — so this encodes what hubCompiler.ts encodes.
    /// (Math.Round defaults to banker's rounding, which would drift by one on every .5.)
    private static int JsRound(double x) =>
        double.IsFinite(x) ? (int)Math.Max(-1e9, Math.Min(1e9, Math.Floor(x + 0.5))) : 0;

    private static byte FreqCode(double hz) => hz <= 0 ? (byte)0 : U8(JsRound(hz) & 0xFF);
    private static byte DutyReg(int pct) => U8(Math.Min(pct * 2, 0x32));
    private static byte U8(int v) => unchecked((byte)v);
    private static ushort U16(int v) => unchecked((ushort)v);
}
