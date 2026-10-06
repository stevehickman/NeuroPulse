// NPPSParser.cs — NPPS parsing on Windows: the shared NPPS core (common/npps-core, OI-NPPS-CORE-01),
// through P/Invoke (NppsCore.cs).
//
// There is one parser, and it is in Rust: what an `.npps` file means, and what refuses it, is decided there
// and is the same on every runtime. This file builds the Windows models from the core's JSON, which means
// supplying what the core leaves out because it is not the same twice (a random id for a protocol that
// states none) and the C# spelling of each field. Windows had no parser before this; it read zone blocks
// with a regular expression (NPZoneRegistry.cs).
//
// What the Windows models do not hold yet is kept as the core's JSON, untouched: composites, limits sets and
// wavelength rules have no C# model, and a protocol's `conditions` and `references` have no field.

using System.Text.Json.Nodes;

namespace NeurOne.Protocol;

/// A refusal: the core's message, `Line N: …` when it names a line.
sealed class NPPSParseException(string message, int line) : Exception(message)
{
    /// 1-based, or 0 when the refusal names no line.
    public int Line { get; } = line;
}

/// Everything one `.npps` file declares.
sealed class NPPSFile
{
    public required NPProtocolDefinition[] Protocols { get; init; }
    /// Zone name → its 1-based socket ids, in file order.
    public required IReadOnlyDictionary<string, int[]> Zones { get; init; }
    /// Condition name → its external definition link.
    public required IReadOnlyDictionary<string, string> Conditions { get; init; }
    /// The core's JSON for the blocks Windows has no model for yet.
    public required JsonArray Composites { get; init; }
    public required JsonArray Limits { get; init; }
    public required JsonArray WavelengthRules { get; init; }
}

static class NPPSParser
{
    /// Parse a whole `.npps` text. Throws NPPSParseException carrying the core's message.
    public static NPPSFile ParseFile(string text)
    {
        JsonObject parsed;
        try
        {
            parsed = JsonNode.Parse(NppsCore.Parse(text))!.AsObject();
        }
        catch (NppsRefusal refusal)
        {
            throw Refusal(refusal);
        }

        var protocols = new List<NPProtocolDefinition>();
        var composites = new JsonArray();
        foreach (var entry in parsed["entries"]!.AsArray())
        {
            if (entry!["protocol"] is JsonObject p) protocols.Add(ProtocolOf(p));
            else if (entry["composite"] is JsonObject c) composites.Add(c.DeepClone());
        }

        var zones = new Dictionary<string, int[]>(StringComparer.Ordinal);
        foreach (var z in parsed["zones"]!.AsArray())
            zones[Str(z!, "name")] = z["sockets"]!.AsArray().Select(s => s!.GetValue<int>()).ToArray();

        var conditions = new Dictionary<string, string>(StringComparer.Ordinal);
        foreach (var c in parsed["conditions"]!.AsArray()) conditions[Str(c!, "name")] = Str(c, "link");

        return new NPPSFile
        {
            Protocols = protocols.ToArray(), Zones = zones, Conditions = conditions, Composites = composites,
            Limits = (JsonArray)parsed["limits"]!.DeepClone(), WavelengthRules = (JsonArray)parsed["wavelengthRules"]!.DeepClone(),
        };
    }

    /// The protocols of a text, in file order.
    public static NPProtocolDefinition[] Parse(string text) => ParseFile(text).Protocols;

    private static NPPSParseException Refusal(NppsRefusal refusal)
    {
        var m = System.Text.RegularExpressions.Regex.Match(refusal.Message, @"^Line (\d+): ([\s\S]*)$");
        return m.Success ? new NPPSParseException(refusal.Message, int.Parse(m.Groups[1].Value)) : new NPPSParseException(refusal.Message, 0);
    }

    // ── field readers ────────────────────────────────────────────────────────

    private static string Str(JsonNode n, string key, string fallback = "") => n[key]?.GetValue<string>() ?? fallback;
    private static double Dbl(JsonNode n, string key, double fallback) => n[key]?.GetValue<double>() ?? fallback;
    private static double? OptDbl(JsonNode n, string key) => n[key]?.GetValue<double>();
    private static int Int(JsonNode n, string key, int fallback) => (int?)n[key]?.GetValue<double>() ?? fallback;
    private static int? OptInt(JsonNode n, string key) => (int?)n[key]?.GetValue<double>();
    private static bool Bool(JsonNode n, string key, bool fallback) => n[key]?.GetValue<bool>() ?? fallback;
    private static string[]? Strings(JsonNode n, string key) => n[key]?.AsArray().Select(s => s!.GetValue<string>()).ToArray();

    // ── protocols ────────────────────────────────────────────────────────────

    private static NPProtocolDefinition ProtocolOf(JsonObject p)
    {
        var timing = p["timingMode"]!;
        var intervalCount = Str(timing, "type") == "interval_count";
        return new NPProtocolDefinition
        {
            Id = Guid.TryParse(Str(p, "id"), out var id) ? id : Guid.NewGuid(),
            Name = Str(p, "name"),
            Description = Str(p, "description"),
            Author = Str(p, "author", "NeurOne"),
            Version = Str(p, "version", "1.0"),
            Tags = Strings(p, "tags") ?? [],
            IsPredefined = Bool(p, "isPredefined", false),
            IsReadOnly = Bool(p, "isReadOnly", false),
            Timing = intervalCount ? NPProtocolDefinition.TimingMode.IntervalCount : NPProtocolDefinition.TimingMode.Duration,
            TimingValue = intervalCount ? Int(timing, "count", 1) : Int(timing, "seconds", 20 * 60),
            Modalities = p["modalities"]!.AsArray().Select(m => ModalityOf(m!)).ToArray(),
        };
    }

    private static NPProtocolModality ModalityOf(JsonNode m)
    {
        var i = m["interval"]!;
        return new NPProtocolModality
        {
            Params = ParamsOf(Str(m, "type"), m["params"]!),
            Interval = new NPIntervalConfig
            {
                IntervalOnSeconds = Int(i, "intervalOnSeconds", 0), IntervalOffSeconds = Int(i, "intervalOffSeconds", 0),
                RepeatCount = OptInt(i, "repeatCount"), StartOffsetSeconds = Int(i, "startOffsetSeconds", 0),
            },
            Enabled = Bool(m, "enabled", true),
        };
    }

    private static NPModalityParams ParamsOf(string type, JsonNode p) => type switch
    {
        "pbm_transcranial" => new NPModalityParams.PbmTranscranial(new PbmTranscranialParams
        {
            Target = Str(p, "zones") == "clinician_selected"
                ? new PbmTarget.ClinicianSelected()
                : new PbmTarget.Named(Strings(p, "zoneRefs") ?? ["All"]),
            Wavelength = Str(p, "wavelength", "808nm"), IrradianceMwCm2 = Dbl(p, "irradianceMWcm2", 300),
            FrequencyHz = Dbl(p, "frequencyHz", 40), DutyCyclePercent = Int(p, "dutyCyclePercent", 25),
        }),
        "pbm_intranasal" => new NPModalityParams.PbmIntranasal(new PbmIntranasalParams
        {
            Wavelength = Str(p, "wavelength", "660nm"), IrradianceMwCm2 = Dbl(p, "irradianceMWcm2", 60),
            FrequencyHz = Dbl(p, "frequencyHz", 10), DutyCyclePercent = Int(p, "dutyCyclePercent", 50),
        }),
        "eeg_neurofeedback" => new NPModalityParams.EegNeurofeedback(new EegNeurofeedbackParams
        {
            Channels = Str(p, "channels") switch
            {
                "front" => EegNeurofeedbackParams.ChannelSelection.Front,
                "central" => EegNeurofeedbackParams.ChannelSelection.Central,
                "custom" => EegNeurofeedbackParams.ChannelSelection.Custom,
                _ => EegNeurofeedbackParams.ChannelSelection.All,
            },
            CustomChannels = Strings(p, "customChannels"),
            Band = Str(p, "band") switch
            {
                "delta" => EegNeurofeedbackParams.EegBand.Delta, "theta" => EegNeurofeedbackParams.EegBand.Theta,
                "beta" => EegNeurofeedbackParams.EegBand.Beta, "gamma" => EegNeurofeedbackParams.EegBand.Gamma,
                "alpha_theta" => EegNeurofeedbackParams.EegBand.AlphaTheta,
                "gamma_theta" => EegNeurofeedbackParams.EegBand.GammaTheta,
                _ => EegNeurofeedbackParams.EegBand.Alpha,
            },
            ClosedLoopEnabled = Bool(p, "closedLoopEnabled", true),
        }),
        "bes_tacs" => new NPModalityParams.BesTacs(new BesTacsParams
        {
            FrequencyHz = Dbl(p, "frequencyHz", 40), IntensityMilliamps = Dbl(p, "intensityMilliamps", 0.5),
            WaveformType = WaveformOf(Str(p, "waveform")),
        }),
        "tdcs" => new NPModalityParams.Tdcs(new TdcsParams
        {
            IntensityMilliamps = Dbl(p, "intensityMilliamps", 1.0),
            ElectrodePairs = p["electrodePairs"]?.AsArray().Select(pair => pair!.AsArray().Select(s => s!.GetValue<string>()).ToArray()).ToArray()
                ?? [["Fp1", "Fp2"]],
            RampSeconds = Int(p, "rampSeconds", 30), ElectrodeAreaCm2 = Dbl(p, "electrodeAreaCm2", 35),
        }),
        "vns_hrv" => new NPModalityParams.VnsHrv(new VnsHrvParams
        {
            FrequencyHz = Dbl(p, "frequencyHz", 25), IntensityMilliamps = Dbl(p, "intensityMilliamps", 0.5),
            HrvProtocolMode = Str(p, "hrvProtocol") switch
            {
                "tavns_sync" => VnsHrvParams.HrvProtocol.TavnsSync,
                "eeg_biofeedback" => VnsHrvParams.HrvProtocol.EegBiofeedback,
                "combined_pbm" => VnsHrvParams.HrvProtocol.CombinedPbm,
                _ => VnsHrvParams.HrvProtocol.Standalone,
            },
            ResonanceBreathingRate = Dbl(p, "resonanceBreathingRate", 6),
        }),
        "audio_entrainment" => new NPModalityParams.AudioEntrainment(new AudioEntrainmentParams
        {
            BinauralBeatsHz = OptDbl(p, "binauralBeatsHz"), IsochronicTonesHz = OptDbl(p, "isochronicTonesHz"),
            NoiseTypeMode = Str(p, "noiseType") switch
            {
                "pink" => AudioEntrainmentParams.NoiseType.Pink, "brown" => AudioEntrainmentParams.NoiseType.Brown, _ => null,
            },
            CarrierHz = Dbl(p, "carrierHz", 200), VolumeDb = Dbl(p, "volumeDb", 75),
            EegAdaptive = Bool(p, "eegAdaptive", true), BoneConductionPacer = Bool(p, "boneConductionPacer", false),
        }),
        "visual_stimulation" => new NPModalityParams.VisualStimulation(new VisualStimParams
        {
            FrequencyHz = Dbl(p, "frequencyHz", 40),
            Mode = Str(p, "mode") switch
            {
                "emdr" => VisualStimParams.VisualMode.Emdr, "retinal_pbm" => VisualStimParams.VisualMode.RetinalPbm,
                "mode_f" => VisualStimParams.VisualMode.ModeF, _ => VisualStimParams.VisualMode.Binocular,
            },
            EmdrCadenceHz = Dbl(p, "emdrCadenceHz", 1), EnableModeF = Bool(p, "enableModeF", false),
        }),
        "qeeg_21ch" => new NPModalityParams.QEeg21ch(new QEeg21chParams
        {
            MontageMode = Str(p, "montage") == "custom" ? QEeg21chParams.Montage.Custom : QEeg21chParams.Montage.Standard1020,
            SloretaEnabled = Bool(p, "sloretaEnabled", true),
            ReferenceMode = Str(p, "reference") switch
            {
                "cz" => QEeg21chParams.Reference.Cz, "average" => QEeg21chParams.Reference.Average,
                _ => QEeg21chParams.Reference.LinkedEar,
            },
        }),
        "tms" => new NPModalityParams.Tms(new TmsParams
        {
            Protocol = Str(p, "tmsProtocol") switch
            {
                "TBS" => TmsParams.TmsProtocol.Tbs, "iTBS" => TmsParams.TmsProtocol.Itbs, _ => TmsParams.TmsProtocol.RTms,
            },
            FrequencyHz = Dbl(p, "frequencyHz", 10), IntensityPercentMt = Int(p, "intensityPercentMT", 110),
            Target = TmsTargetOf(Str(p, "target")), PulseCount = Int(p, "pulseCount", 3000),
        }),
        "pbm_deep_1170nm" => new NPModalityParams.PbmDeep1170nm(new DeepPbm1170Params
        {
            IntensityMwCm2 = Dbl(p, "intensityMWcm2", 500), FrequencyHz = Dbl(p, "frequencyHz", 10),
            DutyCyclePercent = Int(p, "dutyCyclePercent", 50),
        }),
        "clinical_tacs" => new NPModalityParams.ClinicalTacs(new ClinicalTacsParams
        {
            FrequencyHz = Dbl(p, "frequencyHz", 40), IntensityMilliamps = Dbl(p, "intensityMilliamps", 2),
            ChannelCount = Int(p, "channelCount", 8), WaveformType = WaveformOf(Str(p, "waveform")),
        }),
        "hd_tdcs" => new NPModalityParams.HdTdcs(new HdTdcsParams
        {
            Target = TmsTargetOf(Str(p, "target")),
            MontageMode = Str(p, "montage") switch
            {
                "bilateral_4x1" => HdTdcsParams.Montage.Bilateral4x1,
                "standard_2_electrode" => HdTdcsParams.Montage.Standard2Electrode,
                _ => HdTdcsParams.Montage.Ring4x1,
            },
            IntensityMilliamps = Dbl(p, "intensityMilliamps", 1.5),
        }),
        "cervical_vns" => new NPModalityParams.CervicalVns(new CervicalVnsParams
        {
            FrequencyHz = Dbl(p, "frequencyHz", 25), IntensityMilliamps = Dbl(p, "intensityMilliamps", 1.0),
        }),
        "vibrotactile_40hz" => new NPModalityParams.Vibrotactile40hz(new VibrotactileParams
        {
            IntensityG = Dbl(p, "intensityG", 0.9), SyncToAudio = Bool(p, "syncToAudio", true),
            SyncToVisual = Bool(p, "syncToVisual", true),
        }),
        _ => throw new NPPSParseException($"Unknown modality block type: '{type}'", 0),
    };

    private static BesTacsParams.Waveform WaveformOf(string raw) => raw switch
    {
        "square" => BesTacsParams.Waveform.Square, "triangular" => BesTacsParams.Waveform.Triangular,
        _ => BesTacsParams.Waveform.Sinusoidal,
    };

    private static TmsParams.TmsTarget TmsTargetOf(string raw) => raw switch
    {
        "DLPFC_R" => TmsParams.TmsTarget.DlpfcR, "VLPFC_L" => TmsParams.TmsTarget.VlpfcL, "ACC" => TmsParams.TmsTarget.Acc,
        "MPFC" => TmsParams.TmsTarget.Mpfc, "M1_L" => TmsParams.TmsTarget.M1L, "M1_R" => TmsParams.TmsTarget.M1R,
        _ => TmsParams.TmsTarget.DlpfcL,
    };
}
