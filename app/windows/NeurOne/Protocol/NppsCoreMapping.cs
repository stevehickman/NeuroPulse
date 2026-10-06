// NppsCoreMapping.cs — the Windows models as the shared NPPS core reads them (common/npps/fields.json).
//
// The core takes the normalised protocol shape the web parser produces, so this is the one place the
// Windows models are spelled in it. Two things are deliberate:
//   - Enum values are written the way the field table spells them, not the way these models' enum
//     names do: `HrvProtocol.TavnsSync` is `tavns_sync` there. The core compares strings.
//   - Nothing is validated, clamped or defaulted here. A value that is wrong reaches the core as it
//     is, and the core refuses it with the message every runtime gives.
// The iOS counterpart is app/ios/NeurOne/Session/NppsCoreMapping.swift.

using System.Text.Json.Nodes;

namespace NeurOne.Protocol;

static class NppsCoreMapping
{
    /// Every zone name a PBM block of this protocol targets by name.
    public static IReadOnlySet<string> NppsNamedZoneRefs(this NPProtocolDefinition def)
    {
        var names = new HashSet<string>();
        foreach (var m in def.Modalities)
            if (m.Params is NPModalityParams.PbmTranscranial { P.Target: PbmTarget.Named n })
                names.UnionWith(n.ZoneNames);
        return names;
    }

    /// The protocol in the shape `NppsCore.Compile` takes: `timingMode` and `modalities`.
    public static byte[] NppsCoreJson(this NPProtocolDefinition def)
    {
        var timing = def.Timing == NPProtocolDefinition.TimingMode.Duration
            ? new JsonObject { ["type"] = "duration", ["seconds"] = def.TimingValue }
            : new JsonObject { ["type"] = "interval_count", ["count"] = def.TimingValue };
        var blocks = new JsonArray();
        foreach (var m in def.Modalities)
        {
            var (type, prms) = Core(m.Params);
            blocks.Add(new JsonObject
            {
                ["type"] = type, ["enabled"] = m.Enabled, ["params"] = prms, ["interval"] = Core(m.Interval),
            });
        }
        return System.Text.Encoding.UTF8.GetBytes(
            new JsonObject { ["timingMode"] = timing, ["modalities"] = blocks }.ToJsonString());
    }

    private static JsonObject Core(NPIntervalConfig i)
    {
        var json = new JsonObject
        {
            ["intervalOnSeconds"] = i.IntervalOnSeconds, ["intervalOffSeconds"] = i.IntervalOffSeconds,
        };
        if (i.RepeatCount is { } repeat) json["repeatCount"] = repeat;
        if (i.StartOffsetSeconds > 0) json["startOffsetSeconds"] = i.StartOffsetSeconds;
        return json;
    }

    private static JsonArray Strings(IEnumerable<string> v) => new(v.Select(s => (JsonNode?)JsonValue.Create(s)).ToArray());

    private static string Waveform(BesTacsParams.Waveform w) => w switch
    {
        BesTacsParams.Waveform.Sinusoidal => "sinusoidal",
        BesTacsParams.Waveform.Square => "square",
        _ => "triangular",
    };

    private static string Band(EegNeurofeedbackParams.EegBand b) => b switch
    {
        EegNeurofeedbackParams.EegBand.Delta => "delta",
        EegNeurofeedbackParams.EegBand.Theta => "theta",
        EegNeurofeedbackParams.EegBand.Alpha => "alpha",
        EegNeurofeedbackParams.EegBand.Beta => "beta",
        EegNeurofeedbackParams.EegBand.Gamma => "gamma",
        EegNeurofeedbackParams.EegBand.AlphaTheta => "alpha_theta",
        _ => "gamma_theta",
    };

    private static string Channels(EegNeurofeedbackParams.ChannelSelection c) => c switch
    {
        EegNeurofeedbackParams.ChannelSelection.All => "all",
        EegNeurofeedbackParams.ChannelSelection.Front => "front",
        EegNeurofeedbackParams.ChannelSelection.Central => "central",
        _ => "custom",
    };

    private static string Hrv(VnsHrvParams.HrvProtocol p) => p switch
    {
        VnsHrvParams.HrvProtocol.Standalone => "standalone",
        VnsHrvParams.HrvProtocol.TavnsSync => "tavns_sync",
        VnsHrvParams.HrvProtocol.EegBiofeedback => "eeg_biofeedback",
        _ => "combined_pbm",
    };

    private static string Visual(VisualStimParams.VisualMode m) => m switch
    {
        VisualStimParams.VisualMode.Binocular => "binocular",
        VisualStimParams.VisualMode.Emdr => "emdr",
        VisualStimParams.VisualMode.RetinalPbm => "retinal_pbm",
        _ => "mode_f",
    };

    private static string TmsTargetName(TmsParams.TmsTarget t) => t switch
    {
        TmsParams.TmsTarget.DlpfcL => "DLPFC_L",
        TmsParams.TmsTarget.DlpfcR => "DLPFC_R",
        TmsParams.TmsTarget.VlpfcL => "VLPFC_L",
        TmsParams.TmsTarget.Acc => "ACC",
        TmsParams.TmsTarget.Mpfc => "MPFC",
        TmsParams.TmsTarget.M1L => "M1_L",
        _ => "M1_R",
    };

    private static (string, JsonObject) Core(NPModalityParams mp)
    {
        switch (mp)
        {
            case NPModalityParams.PbmTranscranial { P: var p }:
            {
                var json = new JsonObject
                {
                    ["wavelength"] = p.Wavelength, ["irradianceMWcm2"] = p.IrradianceMwCm2,
                    ["frequencyHz"] = p.FrequencyHz, ["dutyCyclePercent"] = p.DutyCyclePercent,
                };
                if (p.Target is PbmTarget.Named n)
                {
                    json["zones"] = "named";
                    json["zoneRefs"] = Strings(n.ZoneNames);
                }
                else
                {
                    json["zones"] = "clinician_selected";
                }
                return ("pbm_transcranial", json);
            }
            case NPModalityParams.PbmIntranasal { P: var p }:
                return ("pbm_intranasal", new JsonObject
                {
                    ["wavelength"] = p.Wavelength, ["irradianceMWcm2"] = p.IrradianceMwCm2,
                    ["frequencyHz"] = p.FrequencyHz, ["dutyCyclePercent"] = p.DutyCyclePercent,
                });
            case NPModalityParams.EegNeurofeedback { P: var p }:
            {
                var json = new JsonObject
                {
                    ["channels"] = Channels(p.Channels), ["band"] = Band(p.Band),
                    ["closedLoopEnabled"] = p.ClosedLoopEnabled,
                };
                if (p.CustomChannels is { } custom) json["customChannels"] = Strings(custom);
                return ("eeg_neurofeedback", json);
            }
            case NPModalityParams.BesTacs { P: var p }:
                return ("bes_tacs", new JsonObject
                {
                    ["frequencyHz"] = p.FrequencyHz, ["intensityMilliamps"] = p.IntensityMilliamps,
                    ["waveform"] = Waveform(p.WaveformType),
                });
            case NPModalityParams.Tdcs { P: var p }:
                return ("tdcs", new JsonObject
                {
                    ["intensityMilliamps"] = p.IntensityMilliamps,
                    ["electrodePairs"] = new JsonArray(p.ElectrodePairs.Select(pair => (JsonNode?)Strings(pair)).ToArray()),
                    ["rampSeconds"] = p.RampSeconds, ["electrodeAreaCm2"] = p.ElectrodeAreaCm2,
                });
            case NPModalityParams.VnsHrv { P: var p }:
                return ("vns_hrv", new JsonObject
                {
                    ["frequencyHz"] = p.FrequencyHz, ["intensityMilliamps"] = p.IntensityMilliamps,
                    ["hrvProtocol"] = Hrv(p.HrvProtocolMode), ["resonanceBreathingRate"] = p.ResonanceBreathingRate,
                });
            case NPModalityParams.AudioEntrainment { P: var p }:
            {
                var json = new JsonObject
                {
                    ["carrierHz"] = p.CarrierHz, ["volumeDb"] = p.VolumeDb, ["eegAdaptive"] = p.EegAdaptive,
                    ["boneConductionPacer"] = p.BoneConductionPacer,
                };
                if (p.BinauralBeatsHz is { } beats) json["binauralBeatsHz"] = beats;
                if (p.IsochronicTonesHz is { } tones) json["isochronicTonesHz"] = tones;
                if (p.NoiseTypeMode is { } noise)
                    json["noiseType"] = noise == AudioEntrainmentParams.NoiseType.Pink ? "pink" : "brown";
                return ("audio_entrainment", json);
            }
            case NPModalityParams.VisualStimulation { P: var p }:
                return ("visual_stimulation", new JsonObject
                {
                    ["frequencyHz"] = p.FrequencyHz, ["mode"] = Visual(p.Mode), ["emdrCadenceHz"] = p.EmdrCadenceHz,
                    ["enableModeF"] = p.EnableModeF,
                });
            case NPModalityParams.QEeg21ch { P: var p }:
                return ("qeeg_21ch", new JsonObject
                {
                    ["montage"] = p.MontageMode == QEeg21chParams.Montage.Standard1020 ? "standard_1020" : "custom",
                    ["sloretaEnabled"] = p.SloretaEnabled,
                    ["reference"] = p.ReferenceMode switch
                    {
                        QEeg21chParams.Reference.LinkedEar => "linked_ear",
                        QEeg21chParams.Reference.Cz => "cz",
                        _ => "average",
                    },
                });
            case NPModalityParams.Tms { P: var p }:
                return ("tms", new JsonObject
                {
                    ["tmsProtocol"] = p.Protocol switch
                    {
                        TmsParams.TmsProtocol.RTms => "rTMS",
                        TmsParams.TmsProtocol.Tbs => "TBS",
                        _ => "iTBS",
                    },
                    ["frequencyHz"] = p.FrequencyHz, ["intensityPercentMT"] = p.IntensityPercentMt,
                    ["target"] = TmsTargetName(p.Target), ["pulseCount"] = p.PulseCount,
                });
            case NPModalityParams.PbmDeep1170nm { P: var p }:
                return ("pbm_deep_1170nm", new JsonObject
                {
                    ["intensityMWcm2"] = p.IntensityMwCm2, ["frequencyHz"] = p.FrequencyHz,
                    ["dutyCyclePercent"] = p.DutyCyclePercent,
                });
            case NPModalityParams.ClinicalTacs { P: var p }:
                return ("clinical_tacs", new JsonObject
                {
                    ["frequencyHz"] = p.FrequencyHz, ["intensityMilliamps"] = p.IntensityMilliamps,
                    ["channelCount"] = p.ChannelCount, ["waveform"] = Waveform(p.WaveformType),
                });
            case NPModalityParams.HdTdcs { P: var p }:
                return ("hd_tdcs", new JsonObject
                {
                    ["target"] = TmsTargetName(p.Target),
                    ["montage"] = p.MontageMode switch
                    {
                        HdTdcsParams.Montage.Ring4x1 => "ring_4x1",
                        HdTdcsParams.Montage.Bilateral4x1 => "bilateral_4x1",
                        _ => "standard_2_electrode",
                    },
                    ["intensityMilliamps"] = p.IntensityMilliamps,
                });
            case NPModalityParams.CervicalVns { P: var p }:
                return ("cervical_vns", new JsonObject
                {
                    ["frequencyHz"] = p.FrequencyHz, ["intensityMilliamps"] = p.IntensityMilliamps,
                });
            case NPModalityParams.Vibrotactile40hz { P: var p }:
                return ("vibrotactile_40hz", new JsonObject
                {
                    ["intensityG"] = p.IntensityG, ["syncToAudio"] = p.SyncToAudio, ["syncToVisual"] = p.SyncToVisual,
                });
            default:
                throw new HubCompileException("modality is unhandled");
        }
    }
}
