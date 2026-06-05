// SessionProtocolCompiler.cs — compiles NPProtocolDefinition → NPSessionProtocol.
// Mirrors buildSessionProtocol(from:) in ProtocolMenuView.swift exactly.
// Same field mappings, same dose formula, same default duration fallback.

using NeuroPulse.Session;

namespace NeuroPulse.Protocol;

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
        return mod.Params switch
        {
            NPModalityParams.PbmTranscranial m => new PbmTranscranialConfig
            {
                Zones = m.P.ResolvedZones,
                FrequencyHz = m.P.FrequencyHz,
                DutyCyclePercent = m.P.DutyCyclePercent,
                DurationSeconds = sessionDurationSeconds,
                // Dose formula (mirrors Swift buildSessionProtocol):
                // peak irradiance ≈ 400 mW/cm² × dutyCycle(25%) = 100 mW/cm² average
                // dose = durationSeconds × intensityFraction × 0.4 W/cm² = J/cm²
                // 0.4 is the CW-equivalent irradiance at 100% intensity (W/cm²).
                TargetDoseJoules = sessionDurationSeconds * (m.P.IntensityPercent / 100.0) * 0.4
            },

            NPModalityParams.PbmIntranasal m => new PbmIntranasalConfig
            {
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
                ElectrodePairs = m.P.ElectrodePairs
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
