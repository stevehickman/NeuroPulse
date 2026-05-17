// NPProtocolDefinition.cs — NPPS protocol model types.
// C# port of NPProtocolDefinition.swift. Field names and defaults are identical.

namespace NeuroPulse.Protocol;

// MARK: - Interval config

sealed class NPIntervalConfig
{
    /// 0 = continuous (no pulsing at the protocol scheduling level)
    public int IntervalOnSeconds { get; init; }
    public int IntervalOffSeconds { get; init; }
    /// null = run until session end
    public int? RepeatCount { get; init; }

    public static readonly NPIntervalConfig Continuous = new() { IntervalOnSeconds = 0, IntervalOffSeconds = 0 };
    public bool IsContinuous => IntervalOnSeconds == 0;
}

// MARK: - Per-modality parameter types (mirror Swift NPXxxParams structs)

sealed class PbmTranscranialParams
{
    public enum ZoneSelection { All, Front, Rear, Custom }
    public enum Wavelength { Base660_808nm, Smart1064nm, Tri660_808_1064nm }

    public ZoneSelection Zones { get; init; } = ZoneSelection.All;
    public int[]? CustomZones { get; init; }
    public Wavelength WavelengthMode { get; init; } = Wavelength.Base660_808nm;
    public double IntensityPercent { get; init; } = 75;
    public double FrequencyHz { get; init; } = 20;     // 0 = CW
    public int DutyCyclePercent { get; init; } = 25;   // ≤25

    public int[] ResolvedZones => Zones switch
    {
        ZoneSelection.All    => [0, 1, 2, 3, 4],
        ZoneSelection.Front  => [0, 1, 2],
        ZoneSelection.Rear   => [2, 3, 4],
        ZoneSelection.Custom => CustomZones ?? [],
        _ => [0, 1, 2, 3, 4]
    };

    public bool RequiresSmartModule =>
        WavelengthMode == Wavelength.Smart1064nm || WavelengthMode == Wavelength.Tri660_808_1064nm;
}

sealed class PbmIntranasalParams
{
    public double IntensityPercent { get; init; } = 60;
    public double FrequencyHz { get; init; } = 40;
    public int DutyCyclePercent { get; init; } = 25;
}

sealed class EegNeurofeedbackParams
{
    public enum ChannelSelection { All, Front, Central, Custom }
    public enum EegBand { Delta, Theta, Alpha, Beta, Gamma, AlphaTheta, GammaTheta }

    public ChannelSelection Channels { get; init; } = ChannelSelection.All;
    public string[]? CustomChannels { get; init; }
    public EegBand Band { get; init; } = EegBand.Alpha;
    public bool ClosedLoopEnabled { get; init; } = true;

    public string[] ResolvedChannels => Channels switch
    {
        ChannelSelection.All     => ["Fp1", "Fp2", "F3", "F4", "C3", "C4", "P3", "P4"],
        ChannelSelection.Front   => ["Fp1", "Fp2", "F3", "F4"],
        ChannelSelection.Central => ["C3", "C4", "P3", "P4"],
        ChannelSelection.Custom  => CustomChannels ?? [],
        _ => ["Fp1", "Fp2", "F3", "F4", "C3", "C4", "P3", "P4"]
    };
}

sealed class BesTacsParams
{
    public enum Waveform { Sinusoidal, Square, Triangular }

    public double FrequencyHz { get; init; } = 20;        // 0.5–40 Hz
    public double IntensityMilliamps { get; init; } = 0.8; // ≤1 mA
    public Waveform WaveformType { get; init; } = Waveform.Sinusoidal;
}

sealed class TdcsParams
{
    public double IntensityMilliamps { get; init; } = 1.0;
    public string[][] ElectrodePairs { get; init; } = [["Fp1", "P3"]];
    public int RampSeconds { get; init; } = 30;
}

sealed class VnsHrvParams
{
    public enum HrvProtocol { Standalone, TavnsSync, EegBiofeedback, CombinedPbm }

    public double FrequencyHz { get; init; } = 25;
    public double IntensityMilliamps { get; init; } = 1.5;
    public HrvProtocol HrvProtocolMode { get; init; } = HrvProtocol.Standalone;
    public double ResonanceBreathingRate { get; init; } = 6.0;
}

sealed class AudioEntrainmentParams
{
    public enum NoiseType { Pink, Brown }

    public double? BinauralBeatsHz { get; init; } = 20;
    public double? IsochronicTonesHz { get; init; }
    public NoiseType? NoiseTypeMode { get; init; } = NoiseType.Pink;
    public double CarrierHz { get; init; } = 440;
    public double VolumePercent { get; init; } = 60;
    public bool EegAdaptive { get; init; } = true;
    public bool BoneConductionPacer { get; init; } = true;
}

sealed class VisualStimParams
{
    public enum VisualMode { Binocular, Emdr, RetinalPbm, ModeF }

    public double FrequencyHz { get; init; } = 40;
    public VisualMode Mode { get; init; } = VisualMode.Binocular;
    public double EmdrCadenceHz { get; init; } = 1.0;
    public bool EnableModeF { get; init; } = false;
}

// T2 — not yet mapped to hub wire format

sealed class QEeg21chParams
{
    public enum Montage { Standard1020, Custom }
    public enum Reference { LinkedEar, Cz, Average }

    public Montage MontageMode { get; init; } = Montage.Standard1020;
    public bool SloretaEnabled { get; init; } = true;
    public Reference ReferenceMode { get; init; } = Reference.LinkedEar;
}

sealed class TmsParams
{
    public enum TmsProtocol { RTms, Tbs, Itbs }
    public enum TmsTarget { DlpfcL, DlpfcR, VlpfcL, Acc, Mpfc, M1L, M1R }

    public TmsProtocol Protocol { get; init; } = TmsProtocol.RTms;
    public double FrequencyHz { get; init; } = 10;
    public int IntensityPercentMt { get; init; } = 80;
    public TmsTarget Target { get; init; } = TmsTarget.DlpfcL;
    public int PulseCount { get; init; } = 1200;
}

sealed class DeepPbm1170Params
{
    public double IntensityMwCm2 { get; init; } = 500;
    public double FrequencyHz { get; init; } = 40;
    public int DutyCyclePercent { get; init; } = 25;
}

sealed class ClinicalTacsParams
{
    public double FrequencyHz { get; init; } = 40;
    public double IntensityMilliamps { get; init; } = 2.0;
    public int ChannelCount { get; init; } = 8;
    public BesTacsParams.Waveform WaveformType { get; init; } = BesTacsParams.Waveform.Sinusoidal;
}

sealed class HdTdcsParams
{
    public enum Montage { Ring4x1, Bilateral4x1, Standard2Electrode }

    public TmsParams.TmsTarget Target { get; init; } = TmsParams.TmsTarget.DlpfcL;
    public Montage MontageMode { get; init; } = Montage.Ring4x1;
    public double IntensityMilliamps { get; init; } = 1.5;
}

sealed class CervicalVnsParams
{
    public double FrequencyHz { get; init; } = 25;
    public double IntensityMilliamps { get; init; } = 1.5;
}

sealed class VibrotactileParams
{
    public double FrequencyHz { get; init; } = 40;   // display only — locked at 40Hz by firmware
    public double IntensityG { get; init; } = 0.9;   // 0.6–1.2G
    public bool SyncToAudio { get; init; } = true;
    public bool SyncToVisual { get; init; } = true;
}

// MARK: - Discriminated union (mirrors NPModalityParams in Swift)

abstract class NPModalityParams
{
    public sealed class PbmTranscranial(PbmTranscranialParams p) : NPModalityParams { public PbmTranscranialParams P => p; }
    public sealed class PbmIntranasal(PbmIntranasalParams p) : NPModalityParams { public PbmIntranasalParams P => p; }
    public sealed class EegNeurofeedback(EegNeurofeedbackParams p) : NPModalityParams { public EegNeurofeedbackParams P => p; }
    public sealed class BesTacs(BesTacsParams p) : NPModalityParams { public BesTacsParams P => p; }
    public sealed class Tdcs(TdcsParams p) : NPModalityParams { public TdcsParams P => p; }
    public sealed class VnsHrv(VnsHrvParams p) : NPModalityParams { public VnsHrvParams P => p; }
    public sealed class AudioEntrainment(AudioEntrainmentParams p) : NPModalityParams { public AudioEntrainmentParams P => p; }
    public sealed class VisualStimulation(VisualStimParams p) : NPModalityParams { public VisualStimParams P => p; }
    public sealed class QEeg21ch(QEeg21chParams p) : NPModalityParams { public QEeg21chParams P => p; }
    public sealed class Tms(TmsParams p) : NPModalityParams { public TmsParams P => p; }
    public sealed class PbmDeep1170nm(DeepPbm1170Params p) : NPModalityParams { public DeepPbm1170Params P => p; }
    public sealed class ClinicalTacs(ClinicalTacsParams p) : NPModalityParams { public ClinicalTacsParams P => p; }
    public sealed class HdTdcs(HdTdcsParams p) : NPModalityParams { public HdTdcsParams P => p; }
    public sealed class CervicalVns(CervicalVnsParams p) : NPModalityParams { public CervicalVnsParams P => p; }
    public sealed class Vibrotactile40hz(VibrotactileParams p) : NPModalityParams { public VibrotactileParams P => p; }
}

// MARK: - Protocol modality

sealed class NPProtocolModality
{
    public Guid Id { get; init; } = Guid.NewGuid();
    public required NPModalityParams Params { get; init; }
    public NPIntervalConfig Interval { get; init; } = NPIntervalConfig.Continuous;
    public bool Enabled { get; init; } = true;
}

// MARK: - Protocol definition

sealed class NPProtocolDefinition
{
    public enum TimingMode
    {
        Duration,      // TotalDurationSeconds applies
        IntervalCount  // IntervalCount applies
    }

    public Guid Id { get; init; } = Guid.NewGuid();
    public required string Name { get; init; }
    public string Description { get; init; } = "";
    public string Author { get; init; } = "NeuroPulse";
    public string Version { get; init; } = "1.0";
    public string[] Tags { get; init; } = [];
    public DateTime CreatedAt { get; init; } = DateTime.UtcNow;
    public DateTime ModifiedAt { get; init; } = DateTime.UtcNow;
    public bool IsPredefined { get; init; } = false;
    public bool IsReadOnly { get; init; } = false;
    public TimingMode Timing { get; init; } = TimingMode.Duration;
    public int TimingValue { get; init; } = 20 * 60; // seconds when Duration; cycle count when IntervalCount
    public NPProtocolModality[] Modalities { get; init; } = [];

    // null when timing mode is IntervalCount (mirrors Swift totalDurationSeconds: Int?)
    public int? TotalDurationSeconds =>
        Timing == TimingMode.Duration ? TimingValue : null;
}
