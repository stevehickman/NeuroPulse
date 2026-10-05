// SessionProtocolCompiler.cs — compiles NPProtocolDefinition → NPSessionProtocol.
// Mirrors buildSessionProtocol(from:) in ProtocolMenuView.swift exactly.
// Same field mappings, same dose formula, same default duration fallback.

using NeurOne.Session;

namespace NeurOne.Protocol;

static class SessionProtocolCompiler
{
    private const int DefaultDurationSeconds = 20 * 60;

    /// Compiles a protocol definition into a signed blob ready for BLE/USB upload to the hub.
    public static SignedProtocolBlob CompileAndSign(NPProtocolDefinition proto)
        => SessionProtocolSigner.Sign(Compile(proto));

    /// Compiles a protocol definition into an NPSessionProtocol (unsigned).
    public static NPSessionProtocol Compile(NPProtocolDefinition proto)
    {
        var durationSeconds = proto.TotalDurationSeconds ?? DefaultDurationSeconds;
        var modalities = proto.Modalities
            .Where(m => m.Enabled)
            .Select(m => CompileModality(m, durationSeconds))
            .OfType<ModalityConfig>()
            .ToArray();

        return new NPSessionProtocol
        {
            Name = proto.Name,
            Modalities = modalities,
            TotalDurationSeconds = durationSeconds,
            Mode = 2 // mode2Programming
        };
    }

    // Mirrors the switch in buildSessionProtocol(from:). T2 and accessory
    // modalities return null — not yet mapped to hub wire format.
    private static ModalityConfig? CompileModality(NPProtocolModality mod, int sessionDurationSeconds)
    {
        // This session wire has no per-block timing: every config runs from 0. A
        // block with a `start` would be delivered at the wrong time, so it is
        // refused rather than flattened (NP-NPPS-REF-001 §5; OI-AND-WIRE-01's
        // shared-schema work is where per-block timing lands).
        if (mod.Interval.StartOffsetSeconds > 0)
            throw new NotSupportedException(
                "This protocol times a block with `start`, which the session wire cannot express yet. Refused, not reshaped.");

        return mod.Params switch
        {
            NPModalityParams.PbmTranscranial m => new PbmTranscranialConfig
            {
                Zones = RequireZones(m.P),
                Wavelength = WavelengthRawValue(m.P.Wavelength),
                FrequencyHz = m.P.FrequencyHz,
                DutyCyclePercent = m.P.DutyCyclePercent,
                DurationSeconds = sessionDurationSeconds,
                IrradianceMwCm2 = m.P.IrradianceMwCm2,
                // J/cm² = irradiance (mW/cm²) × duty (CW is full duty) × time (s) / 1000.
                TargetDoseJoules = m.P.IrradianceMwCm2
                    * (m.P.FrequencyHz == 0 ? 1.0 : m.P.DutyCyclePercent / 100.0)
                    * sessionDurationSeconds / 1000.0
            },

            NPModalityParams.PbmIntranasal m => new PbmIntranasalConfig
            {
                Wavelength = IntranasalWavelength(m.P.Wavelength),
                IrradianceMwCm2 = m.P.IrradianceMwCm2,
                FrequencyHz = m.P.FrequencyHz,
                DutyCyclePercent = m.P.DutyCyclePercent,
                DurationSeconds = sessionDurationSeconds
            },

            NPModalityParams.EegNeurofeedback m => new EegConfig
            {
                EnabledChannels = m.P.ResolvedChannels,
                SampleRateHz = 500,
                NeurofeedbackBand = BandRawValue(m.P.Band),
                ClosedLoopEnabled = m.P.ClosedLoopEnabled
            },

            NPModalityParams.BesTacs m => new BesConfig
            {
                FrequencyHz = m.P.FrequencyHz,
                AmplitudeMilliamps = m.P.IntensityMilliamps,
                DurationSeconds = mod.Interval.IsContinuous ? sessionDurationSeconds : mod.Interval.IntervalOnSeconds,
                Waveform = WaveformRawValue(m.P.WaveformType)
            },

            NPModalityParams.Tdcs m => new TdcsConfig
            {
                AmplitudeMilliamps = m.P.IntensityMilliamps,
                DurationSeconds = mod.Interval.IsContinuous ? sessionDurationSeconds : mod.Interval.IntervalOnSeconds,
                RampSeconds = m.P.RampSeconds,
                ElectrodePairs = m.P.ElectrodePairs,
                ElectrodeAreaCm2 = m.P.ElectrodeAreaCm2
            },

            NPModalityParams.VnsHrv m => new VnsHrvConfig
            {
                FrequencyHz = m.P.FrequencyHz,
                AmplitudeMilliamps = m.P.IntensityMilliamps,
                EnableHrvBiofeedback = true,
                ResonanceBreathingRateDefault = m.P.ResonanceBreathingRate,
                HrvProtocol = HrvProtocolRawValue(m.P.HrvProtocolMode)
            },

            NPModalityParams.AudioEntrainment m => new NeuralAudioConfig
            {
                BinauralBeatHz = m.P.BinauralBeatsHz,
                IsochronicToneHz = m.P.IsochronicTonesHz,
                NoiseType = m.P.NoiseTypeMode switch
                {
                    AudioEntrainmentParams.NoiseType.Pink  => "pink",
                    AudioEntrainmentParams.NoiseType.Brown => "brown",
                    _ => null
                },
                VolumeDb = m.P.VolumeDb,
                EegAdaptive = m.P.EegAdaptive,
                UseBoneConductionForPacer = m.P.BoneConductionPacer
            },

            NPModalityParams.VisualStimulation m => new VisualStimConfig
            {
                FrequencyHz = m.P.FrequencyHz,
                Mode = VisualModeRawValue(m.P.Mode),
                EnableModeFInvisibleNir = m.P.EnableModeF,
                EmdrCadenceHz = m.P.EmdrCadenceHz
            },

            _ => null // T2 and accessory modalities not yet mapped to hub wire format
        };
    }

    // MARK: - Raw value helpers (mirror Swift enum rawValue)

    // Mirrors VNSHRVConfig.HRVProtocol raw values in SessionProtocol.swift.
    private static string HrvProtocolRawValue(VnsHrvParams.HrvProtocol hrv) => hrv switch
    {
        VnsHrvParams.HrvProtocol.Standalone     => "standalone",
        VnsHrvParams.HrvProtocol.TavnsSync      => "hrv_tavns_sync",
        VnsHrvParams.HrvProtocol.EegBiofeedback => "hrv_eeg_biofeedback",
        VnsHrvParams.HrvProtocol.CombinedPbm    => "hrv_pbm",
        _ => "standalone"
    };

    private static string BandRawValue(EegNeurofeedbackParams.EegBand band) => band switch
    {
        EegNeurofeedbackParams.EegBand.Delta      => "delta",
        EegNeurofeedbackParams.EegBand.Theta      => "theta",
        EegNeurofeedbackParams.EegBand.Alpha      => "alpha",
        EegNeurofeedbackParams.EegBand.Beta       => "beta",
        EegNeurofeedbackParams.EegBand.Gamma      => "gamma",
        EegNeurofeedbackParams.EegBand.AlphaTheta => "alphaTheta",
        EegNeurofeedbackParams.EegBand.GammaTheta => "gammaTheta",
        _ => "alpha"
    };

    // The NPPS `wavelength` token, carried verbatim. No fallback: substituting a
    // default wavelength is the defect OI-PBMCH-04 records (NP-FEAS-PBMCH-001 §7.3).
    // A value that is not a wavelength, or that no wavelength rule maps onto a
    // channel, refuses to compile (NP-NPPS-REF-001 §4.1a, §7a).
    private static string WavelengthRawValue(string w)
    {
        if (WavelengthRulesEngine.ResolveChannels(w, WavelengthRulesEngine.Default, out var refusal) is null)
            throw new NotSupportedException(refusal switch
            {
                "retired" => WavelengthRulesEngine.RetiredMessage(w),
                "invalid" => $"PBM wavelength '{w}' is not a wavelength: write one value such as \"810nm\".",
                _ => UnmappedMessage(w)
            });
        return w;
    }

    private static string UnmappedMessage(string w) =>
        $"No emitter channel delivers {w} under the wavelength rules in force. Refused, not moved to the nearest channel.";

    // A target that names no zone is refused, never substituted (iOS throws NPSocketTargetError.emptyTarget).
    // Windows still carries the five-slot selector; the named-zone / clinician-selected model is not ported.
    private static int[] RequireZones(PbmTranscranialParams p)
    {
        var zones = p.ResolvedZones;
        if (zones.Length == 0)
            throw new NotSupportedException("PBM target resolves to no zones. Refused, not substituted.");
        return zones;
    }

    // The intranasal probe carries the 660 and 808 nm channels only: a wavelength that maps to
    // 1064 nm is refused for it, as is anything WavelengthRawValue refuses.
    private static string IntranasalWavelength(string w)
    {
        WavelengthRawValue(w);
        var channels = WavelengthRulesEngine.ResolveChannels(w, WavelengthRulesEngine.Default, out _)!;
        if (!channels.Any(c => c is PbmChannelElement.Led660 or PbmChannelElement.Led808))
            throw new NotSupportedException(UnmappedMessage(w));
        return w;
    }

    private static string WaveformRawValue(BesTacsParams.Waveform w) => w switch
    {
        BesTacsParams.Waveform.Sinusoidal  => "sinusoidal",
        BesTacsParams.Waveform.Square      => "square",
        BesTacsParams.Waveform.Triangular  => "triangular",
        _ => "sinusoidal"
    };

    private static string VisualModeRawValue(VisualStimParams.VisualMode m) => m switch
    {
        VisualStimParams.VisualMode.Binocular  => "binocular",
        VisualStimParams.VisualMode.Emdr       => "emdr",
        VisualStimParams.VisualMode.RetinalPbm => "retinalPBM",
        VisualStimParams.VisualMode.ModeF      => "retinalPBM", // Mode F uses retinalPBM path + NIR flag
        _ => "binocular"
    };
}
