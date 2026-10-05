// HubDescriptorCompilerTests.cs — OI-AND-WIRE-01.
//
// The Windows compiler writes the descriptor of NP-FW-HUB-001 §4, byte for byte what
// app/web/src/lib/hubCompiler.ts writes. hub-descriptor-golden.json is the web compiler's output
// (bun scripts/gen-hub-descriptor-golden.ts), with the session UUID pinned to 0xAB and the compile
// time to 1700000000. Android's and iOS's HubDescriptorCompilerTests diff the same file.
//
// Run with: dotnet test app/windows/NeurOne.Tests  (needs the Windows TFM; written without a
// dotnet toolchain, so it has never been compiled or run).

using System.Runtime.CompilerServices;
using System.Text.Json;
using NeurOne.Protocol;
using Xunit;

namespace NeurOne.Tests;

public class HubDescriptorCompilerTests
{
    private static readonly string Root = RepoRoot();

    private static string RepoRoot([CallerFilePath] string file = "")
    {
        // .../app/windows/NeurOne.Tests/<this file> → repo root
        var dir = new DirectoryInfo(Path.GetDirectoryName(file)!);
        return dir.Parent!.Parent!.Parent!.FullName;
    }

    static HubDescriptorCompilerTests()
    {
        // Zones are read from the shipped file, not from a fixture, as on the other platforms.
        var zones = NPZoneRegistry.ParseZones(File.ReadAllText(
            Path.Combine(Root, "protocols", "predefined", "00-zones.npps")));
        NPZoneRegistry.Use(zones);
    }

    private static readonly Dictionary<string, string> Golden = JsonSerializer.Deserialize<Dictionary<string, string>>(
        File.ReadAllText(Path.Combine(Root, "app", "NeurOneShared", "TestData", "hub-descriptor-golden.json")))!;

    private static readonly DateTimeOffset Now = DateTimeOffset.FromUnixTimeSeconds(1_700_000_000);
    private static readonly byte[] Uuid = Enumerable.Repeat((byte)0xAB, 16).ToArray();

    private static HubDescriptor Build(NPProtocolDefinition d, byte[]? serial = null, int[]? clinician = null, bool autonomous = false)
        => HubDescriptorCompiler.Build(d, serial, clinician ?? [7, 3], autonomous, Now, Uuid);

    private static void Check(string name, NPProtocolDefinition d)
        => Assert.Equal(Golden[name], Convert.ToHexString(Build(d).Blob).ToLowerInvariant());

    private static NPProtocolDefinition Def(int secs, params NPProtocolModality[] mods)
        => new() { Name = "g", Timing = NPProtocolDefinition.TimingMode.Duration, TimingValue = secs, Modalities = mods };

    private static NPProtocolModality Mod(NPModalityParams p, NPIntervalConfig? i = null)
        => new() { Params = p, Interval = i ?? NPIntervalConfig.Continuous };

    private static NPProtocolModality Pbm(string wl, double irr, PbmTarget? target = null, double hz = 40, int duty = 25)
        => Mod(new NPModalityParams.PbmTranscranial(new PbmTranscranialParams
        {
            Target = target ?? new PbmTarget.Named("Frontal"), Wavelength = wl,
            IrradianceMwCm2 = irr, FrequencyHz = hz, DutyCyclePercent = duty,
        }));

    [Fact]
    public void PbmChannels()
    {
        Check("pbm660", Def(1200, Pbm("660nm", 200)));
        Check("pbm808", Def(1200, Pbm("808nm", 322)));
        Check("pbm1064", Def(1200, Pbm("1064nm", 28)));
    }

    [Fact]
    public void ParallelWavelengthsMergeIntoOneTileCommand()
        => Check("pbmMerged", Def(600, Pbm("660nm", 100), Pbm("808nm", 200)));

    [Fact]
    public void ClinicianSelectedSockets()
        => Check("pbmClin", Def(600, Pbm("808nm", 100, new PbmTarget.ClinicianSelected(), hz: 0, duty: 100)));

    [Fact]
    public void T1Modalities()
    {
        Check("nasal", Def(600, Mod(new NPModalityParams.PbmIntranasal(new PbmIntranasalParams
            { Wavelength = "808nm", IrradianceMwCm2 = 60, FrequencyHz = 40, DutyCyclePercent = 25 }))));
        Check("eegAll", Def(600, Mod(new NPModalityParams.EegNeurofeedback(new EegNeurofeedbackParams()))));
        Check("eegCustom", Def(600, Mod(new NPModalityParams.EegNeurofeedback(new EegNeurofeedbackParams
            { Channels = EegNeurofeedbackParams.ChannelSelection.Custom, CustomChannels = ["Fp1", "P4"], ClosedLoopEnabled = false }))));
        Check("bes", Def(600, Mod(new NPModalityParams.BesTacs(new BesTacsParams { FrequencyHz = 10, IntensityMilliamps = 0.8 }))));
        Check("tdcs", Def(600, Mod(new NPModalityParams.Tdcs(new TdcsParams
            { IntensityMilliamps = 1, ElectrodePairs = [["P3", "P4"]], ElectrodeAreaCm2 = 25.5 }))));
        Check("vns", Def(600, Mod(new NPModalityParams.VnsHrv(new VnsHrvParams
            { HrvProtocolMode = VnsHrvParams.HrvProtocol.TavnsSync }))));
        Check("audioBin", Def(600, Mod(new NPModalityParams.AudioEntrainment(new AudioEntrainmentParams
            { BinauralBeatsHz = 20, IsochronicTonesHz = null, NoiseTypeMode = null, VolumeDb = 75, BoneConductionPacer = false }))));
        Check("audioPink", Def(600, Mod(new NPModalityParams.AudioEntrainment(new AudioEntrainmentParams
            { BinauralBeatsHz = null, IsochronicTonesHz = null, NoiseTypeMode = AudioEntrainmentParams.NoiseType.Pink,
              VolumeDb = 60, EegAdaptive = false }))));
        Check("visual", Def(600, Mod(new NPModalityParams.VisualStimulation(new VisualStimParams { EmdrCadenceHz = 1.5 }))));
    }

    [Fact]
    public void T2Modalities()
    {
        Check("qeeg", Def(600, Mod(new NPModalityParams.QEeg21ch(new QEeg21chParams { ReferenceMode = QEeg21chParams.Reference.Average }))));
        Check("tms", Def(600, Mod(new NPModalityParams.Tms(new TmsParams
            { Protocol = TmsParams.TmsProtocol.Itbs, Target = TmsParams.TmsTarget.M1R, PulseCount = 600 }))));
        Check("deep", Def(600, Mod(new NPModalityParams.PbmDeep1170nm(new DeepPbm1170Params()))));
        Check("ctacs", Def(600, Mod(new NPModalityParams.ClinicalTacs(new ClinicalTacsParams
            { ChannelCount = 21, WaveformType = BesTacsParams.Waveform.Square }))));
        Check("hdtdcs", Def(600, Mod(new NPModalityParams.HdTdcs(new HdTdcsParams
            { Target = TmsParams.TmsTarget.DlpfcR, MontageMode = HdTdcsParams.Montage.Bilateral4x1 }))));
        Check("cvns", Def(600, Mod(new NPModalityParams.CervicalVns(new CervicalVnsParams()))));
        Check("vibro", Def(600, Mod(new NPModalityParams.Vibrotactile40hz(new VibrotactileParams { SyncToVisual = false }))));
        Assert.True(Build(Def(600, Mod(new NPModalityParams.CervicalVns(new CervicalVnsParams())))).IsT2);
    }

    [Fact]
    public void IntervalsExpandToOnAndStopAndBlockStartShiftsTheSchedule()
        => Check("interval", Def(100,
            Mod(new NPModalityParams.BesTacs(new BesTacsParams
                { FrequencyHz = 10, IntensityMilliamps = 0.8, WaveformType = BesTacsParams.Waveform.Square }),
                new NPIntervalConfig { IntervalOnSeconds = 20, IntervalOffSeconds = 10 }),
            Mod(new NPModalityParams.Tdcs(new TdcsParams
                { IntensityMilliamps = 1, ElectrodePairs = [["F3", "F4"]], RampSeconds = 45, ElectrodeAreaCm2 = 10 }),
                new NPIntervalConfig { StartOffsetSeconds = 30 })));

    [Fact]
    public void SignatureCoversTheRawRegionAndLandsInTheLast64Bytes()
    {
        byte[]? seen = null;
        var d = HubDescriptorCompiler.Compile(Def(600, Pbm("808nm", 100)), sign: region =>
        {
            seen = region;
            return (Enumerable.Repeat((byte)7, 64).ToArray(), "fp");
        });
        Assert.Equal(d.Blob.Length - 64, seen!.Length);
        Assert.Equal(seen, d.Blob[..^64]);
        Assert.All(d.Blob[^64..], b => Assert.Equal(7, b));
        Assert.Equal("fp", d.PublicKeyFingerprint);
    }

    [Fact]
    public void AWrongLengthSignatureIsRefused()
        => Assert.Throws<HubCompileException>(() =>
            HubDescriptorCompiler.Compile(Def(600, Pbm("808nm", 100)), sign: _ => (new byte[63], "fp")));

    [Fact]
    public void DeviceSerialIsTheReplayGuard()
    {
        var serial = Enumerable.Range(1, 32).Select(i => (byte)i).ToArray();
        var blob = Build(Def(600, Pbm("808nm", 100)), serial).Blob;
        Assert.Equal(serial, blob[28..60]);
    }

    [Fact]
    public void HeaderMagicAndAutonomousFlag()
    {
        var plain = Build(Def(600, Pbm("808nm", 100))).Blob;
        Assert.Equal(new byte[] { 0x50, 0x48, 0x50, 0x4E }, plain[..4]);   // "PHPN": NPHP little-endian
        Assert.Equal(0, plain[6]);
        Assert.Equal(0b10, Build(Def(600, Pbm("808nm", 100)), autonomous: true).Blob[6]);
    }

    [Fact]
    public void RefusalsNameTheProblemAndNeverReshape()
    {
        Assert.Throws<HubCompileException>(() => Build(Def(600, Pbm("808nm", 500))));    // above the 403 full scale
        Assert.Throws<HubCompileException>(() => Build(Def(600, Pbm("1064nm", 100))));   // above 28
        var retired = Assert.Throws<HubCompileException>(() => Build(Def(600, Pbm("660_808nm", 100))));
        Assert.Contains("retired", retired.Message);
        Assert.Throws<HubCompileException>(() => Build(Def(600)));                       // nothing enabled
        Assert.Throws<HubCompileException>(() => Build(Def(60, Mod(
            new NPModalityParams.BesTacs(new BesTacsParams()), new NPIntervalConfig { StartOffsetSeconds = 60 }))));
        Assert.Throws<HubCompileException>(() => Build(Def(600, Pbm("808nm", 100, new PbmTarget.Named("No Such Zone")))));
        Assert.Throws<HubCompileException>(() =>
            HubDescriptorCompiler.Build(Def(600, Pbm("808nm", 100, new PbmTarget.ClinicianSelected())), null, null));

        // Two PBM blocks on one tile that differ in duty are not one stimulus.
        Assert.Throws<HubCompileException>(() => Build(Def(600, Pbm("808nm", 100), Pbm("660nm", 100, duty: 10))));

        // More than 64 commands is refused, not truncated: the tail of an interval protocol is its STOPs.
        Assert.Throws<HubCompileException>(() => Build(Def(3600, Mod(
            new NPModalityParams.BesTacs(new BesTacsParams()),
            new NPIntervalConfig { IntervalOnSeconds = 10, IntervalOffSeconds = 10 }))));
    }

    [Fact]
    public void TheProbeRefusalReadsLikeUnmapped()
    {
        var e = Assert.Throws<HubCompileException>(() => Build(Def(600, Mod(new NPModalityParams.PbmIntranasal(
            new PbmIntranasalParams { Wavelength = "1064nm" })))));
        Assert.Equal(
            "No emitter channel delivers 1064nm under the wavelength rules in force. " +
            "Refused, not moved to the nearest channel.", e.Message);
    }

    [Fact]
    public void TheTargetBlockCarriesTheResolvedMask()
    {
        var blob = Build(Def(600, Pbm("808nm", 100, new PbmTarget.Named("Occipital Left")))).Blob;
        Assert.Equal(0x01, blob[64 + 12]);   // target_kind = socket mask
        Assert.Equal(16, blob[64 + 13]);     // target_len
        var ids = new List<int>();
        for (var i = 0; i < 16; i++)
            for (var bit = 0; bit < 8; bit++)
                if ((blob[78 + i] & (1 << bit)) != 0) ids.Add(i * 8 + bit + 1);
        Assert.Equal(new[] { 72, 73, 74, 77, 78 }, ids);
    }

    [Fact]
    public void ZoneBlocksParseFromNppsText()
    {
        var zones = NPZoneRegistry.ParseZones("""
            # a comment
            zone "A" {
                id: "x"
                sockets: [1, 2, 3]
            }
            zone "B" {
                sockets: [4]
            }
            """);
        Assert.Equal(new[] { 1, 2, 3 }, zones["A"]);
        Assert.Equal(new[] { 4 }, zones["B"]);
    }
}
