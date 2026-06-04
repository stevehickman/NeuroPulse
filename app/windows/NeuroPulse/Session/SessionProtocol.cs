// SessionProtocol.cs — hub wire format types and Ed25519 signer.
// Mirrors SessionProtocol.swift exactly. JSON key names match Swift Codable
// auto-synthesis so the hub firmware parses Windows-uploaded protocols
// identically to iOS-uploaded ones.

using System.Security.Cryptography;
using System.Text.Json;
using System.Text.Json.Serialization;
using NSec.Cryptography;

namespace NeuroPulse.Session;

// MARK: - Session protocol

sealed class NPSessionProtocol
{
    public Guid Id { get; init; } = Guid.NewGuid();
    public byte SchemaVersion { get; init; } = 1;
    public required string Name { get; init; }
    public required ModalityConfig[] Modalities { get; init; }
    public required int TotalDurationSeconds { get; init; }
    // Swift JSONEncoder encodes Date as seconds since 2001-01-01 UTC (Apple reference date).
    public double CreatedAt { get; init; } = ToAppleReferenceSeconds(DateTime.UtcNow);
    public byte Mode { get; init; } = 2; // mode2Programming

    private static readonly DateTime AppleReferenceDate = new(2001, 1, 1, 0, 0, 0, DateTimeKind.Utc);
    internal static double ToAppleReferenceSeconds(DateTime utc) => (utc - AppleReferenceDate).TotalSeconds;
}

// MARK: - Modality configs

// Mirrors ModalityConfig enum in SessionProtocol.swift.
// ModalityConfigConverter serializes each subclass as {"caseName": {...}} to
// match Swift's synthesized Codable encoding of enums with associated values.
[JsonConverter(typeof(ModalityConfigConverter))]
abstract class ModalityConfig { }

// case pbmTranscranial(PBMTranscranialConfig)
sealed class PbmTranscranialConfig : ModalityConfig
{
    public required int[] Zones { get; init; }            // active zone indices 0–4
    public required double FrequencyHz { get; init; }     // 0 = CW
    public required int DutyCyclePercent { get; init; }   // ≤25 (firmware-enforced)
    public required int DurationSeconds { get; init; }
    public required double TargetDoseJoules { get; init; } // J/cm²
}

// case pbmIntranasal(PBMIntranasalConfig)
sealed class PbmIntranasalConfig : ModalityConfig
{
    public required double FrequencyHz { get; init; }
    public required int DutyCyclePercent { get; init; }
    public required int DurationSeconds { get; init; }
}

// case eegNeurofeedback(EEGConfig)
sealed class EegConfig : ModalityConfig
{
    public required string[] EnabledChannels { get; init; }
    public int SampleRateHz { get; init; } = 500;
    public required string NeurofeedbackBand { get; init; }
    public bool ClosedLoopEnabled { get; init; } = true;
}

// case bes(BESConfig)
sealed class BesConfig : ModalityConfig
{
    public required double FrequencyHz { get; init; }        // 0.5–40 Hz
    public required double AmplitudeMilliamps { get; init; } // ≤1 mA
    public required int DurationSeconds { get; init; }
    public string Waveform { get; init; } = "sinusoidal";
}

// case tdcs(TDCSConfig)
sealed class TdcsConfig : ModalityConfig
{
    public required double AmplitudeMilliamps { get; init; } // 0.1–2 mA; 40 µC/cm² limit on safety MCU
    public required int DurationSeconds { get; init; }
    public int RampSeconds { get; init; } = 30;              // hardware-enforced
    public required string[][] ElectrodePairs { get; init; }
}

// case vnsHRV(VNSHRVConfig)
sealed class VnsHrvConfig : ModalityConfig
{
    public required double FrequencyHz { get; init; }           // 1–25 Hz
    public required double AmplitudeMilliamps { get; init; }    // ≤2 mA
    [JsonPropertyName("enableHRVBiofeedback")]
    public bool EnableHrvBiofeedback { get; init; } = true;
    public double ResonanceBreathingRateDefault { get; init; } = 6.0;
    // "protocol" is a C# keyword; serialized as "protocol" to match Swift.
    [JsonPropertyName("protocol")]
    public string HrvProtocol { get; init; } = "standalone";
}

// case neuralAudio(NeuralAudioConfig)
sealed class NeuralAudioConfig : ModalityConfig
{
    public double? BinauralBeatHz { get; init; }
    public double? IsochronicToneHz { get; init; }
    public string? NoiseType { get; init; }
    public bool EegAdaptive { get; init; } = true;
    public bool UseBoneConductionForPacer { get; init; } = true;
}

// case visualStimulation(VisualStimConfig)
sealed class VisualStimConfig : ModalityConfig
{
    public required double FrequencyHz { get; init; }       // 0.5–100 Hz
    public string Mode { get; init; } = "binocular";
    [JsonPropertyName("enableModeFInvisibleNIR")]
    public bool EnableModeFInvisibleNir { get; init; } = false;
    public double EmdrCadenceHz { get; init; } = 1.0;
}

// MARK: - JSON converter for ModalityConfig

// Produces {"pbmTranscranial": {...}} format matching Swift Codable enum synthesis.
sealed class ModalityConfigConverter : JsonConverter<ModalityConfig>
{
    public override ModalityConfig Read(ref Utf8JsonReader reader, Type typeToConvert, JsonSerializerOptions options)
    {
        using var doc = JsonDocument.ParseValue(ref reader);
        var prop = doc.RootElement.EnumerateObject().First();
        var raw = prop.Value.GetRawText();
        return prop.Name switch
        {
            "pbmTranscranial"   => JsonSerializer.Deserialize<PbmTranscranialConfig>(raw, options)!,
            "pbmIntranasal"     => JsonSerializer.Deserialize<PbmIntranasalConfig>(raw, options)!,
            "eegNeurofeedback"  => JsonSerializer.Deserialize<EegConfig>(raw, options)!,
            "bes"               => JsonSerializer.Deserialize<BesConfig>(raw, options)!,
            "tdcs"              => JsonSerializer.Deserialize<TdcsConfig>(raw, options)!,
            "vnsHRV"            => JsonSerializer.Deserialize<VnsHrvConfig>(raw, options)!,
            "neuralAudio"       => JsonSerializer.Deserialize<NeuralAudioConfig>(raw, options)!,
            "visualStimulation" => JsonSerializer.Deserialize<VisualStimConfig>(raw, options)!,
            _ => throw new JsonException($"Unknown ModalityConfig case: {prop.Name}")
        };
    }

    public override void Write(Utf8JsonWriter writer, ModalityConfig value, JsonSerializerOptions options)
    {
        (string key, object inner) = value switch
        {
            PbmTranscranialConfig c => ("pbmTranscranial",   c),
            PbmIntranasalConfig c   => ("pbmIntranasal",     c),
            EegConfig c             => ("eegNeurofeedback",  c),
            BesConfig c             => ("bes",               c),
            TdcsConfig c            => ("tdcs",              c),
            VnsHrvConfig c          => ("vnsHRV",            c),
            NeuralAudioConfig c     => ("neuralAudio",       c),
            VisualStimConfig c      => ("visualStimulation", c),
            _ => throw new JsonException($"Unknown ModalityConfig subtype: {value.GetType().Name}")
        };
        writer.WriteStartObject();
        writer.WritePropertyName(key);
        JsonSerializer.Serialize(writer, inner, inner.GetType(), options);
        writer.WriteEndObject();
    }
}

// MARK: - Shared JSON options

static class NpJsonOptions
{
    // camelCase keys + include nulls — matches Swift JSONEncoder default output.
    public static readonly JsonSerializerOptions Hub = new(JsonSerializerDefaults.General)
    {
        WriteIndented = false,
        PropertyNamingPolicy = JsonNamingPolicy.CamelCase,
        DefaultIgnoreCondition = JsonIgnoreCondition.Never,
    };
}

// MARK: - Signed protocol blob

sealed class SignedProtocolBlob
{
    // Wire format: 4-byte magic "NPPR" + 4-byte LE payload length + payload + 64-byte Ed25519 sig
    private static readonly byte[] Magic = [0x4E, 0x50, 0x50, 0x52];

    public required byte[] Payload { get; init; }   // canonical JSON of NPSessionProtocol
    public required byte[] Signature { get; init; } // 64-byte Ed25519 sig
    public required string PublicKeyFingerprint { get; init; } // 8-byte hex prefix

    public byte[] ToWireFormat()
    {
        var buf = new byte[4 + 4 + Payload.Length + 64];
        Magic.CopyTo(buf.AsSpan());
        var lenBytes = BitConverter.GetBytes((uint)Payload.Length);
        if (!BitConverter.IsLittleEndian) Array.Reverse(lenBytes);
        lenBytes.CopyTo(buf.AsSpan(4));
        Payload.CopyTo(buf.AsSpan(8));
        Signature.CopyTo(buf.AsSpan(8 + Payload.Length));
        return buf;
    }
}

// MARK: - Protocol signer

// Mirrors SessionProtocolSigner.swift.
// The Ed25519 private key is generated once at first launch, protected with
// DPAPI (Windows user-scope), and persisted to %LOCALAPPDATA%\NeuroPulse\.
// The paired hub stores the corresponding public key registered at BLE pairing.
static class SessionProtocolSigner
{
    private static readonly SignatureAlgorithm Ed25519Alg = SignatureAlgorithm.Ed25519;
    private static readonly string KeyPath = Path.Combine(
        Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
        "NeuroPulse", "signing.key");

    public static SignedProtocolBlob Sign(NPSessionProtocol proto)
    {
        var payload = JsonSerializer.SerializeToUtf8Bytes(proto, NpJsonOptions.Hub);
        using var key = LoadOrCreateSigningKey();
        var sig = Ed25519Alg.Sign(key, payload);
        var fp = Convert.ToHexString(key.PublicKey.Export(KeyBlobFormat.RawPublicKey).AsSpan()[..8]).ToLowerInvariant();
        return new SignedProtocolBlob { Payload = payload, Signature = sig, PublicKeyFingerprint = fp };
    }

    public static byte[] PublicKeyData()
    {
        using var key = LoadOrCreateSigningKey();
        return key.PublicKey.Export(KeyBlobFormat.RawPublicKey);
    }

    private static Key LoadOrCreateSigningKey()
    {
        var creationParams = new KeyCreationParameters { ExportPolicy = KeyExportPolicies.AllowPlaintextExport };

        if (File.Exists(KeyPath))
        {
            var dpapi = File.ReadAllBytes(KeyPath);
            var raw = ProtectedData.Unprotect(dpapi, null, DataProtectionScope.CurrentUser);
            return Key.Import(Ed25519Alg, raw, KeyBlobFormat.RawPrivateKey, creationParams);
        }

        Directory.CreateDirectory(Path.GetDirectoryName(KeyPath)!);
        var key = Key.Create(Ed25519Alg, creationParams);
        var exported = key.Export(KeyBlobFormat.RawPrivateKey);
        var protected_ = ProtectedData.Protect(exported, null, DataProtectionScope.CurrentUser);
        File.WriteAllBytes(KeyPath, protected_);
        return key;
    }
}
