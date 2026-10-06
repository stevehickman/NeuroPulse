// HubDescriptorCompiler.cs — the hub session descriptor (OI-AND-WIRE-01, OI-NPPS-CORE-01).
//
// There is ONE wire format: the binary descriptor of NP-FW-HUB-001 §4, which
// firmware/hub_control/src/np_protocol.c parses. It has ONE writer: the shared NPPS core
// (common/npps-core), which this file calls through NppsCore. This used to be a C# port of the web
// compiler, one of four hand-written ports that drifted; it now maps the Windows models to the
// core's protocol shape (NppsCoreMapping.cs), hands over the clock, the session UUID and the zone
// namespace, and signs the result. What a protocol compiles to, and what refuses it, is decided in
// the core and is the same on every runtime. app/NeurOneShared/TestData/hub-descriptor-golden.json
// holds the web compiler's output for the same definitions; NeurOne.Tests/HubDescriptorCompilerTests.cs
// diffs this file against it, so the core is held to the web reference from here as well as from Rust.
//
// A refusal is a HubCompileException carrying the core's message.
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
    public const int UUID_LEN = 16;
    public const int SIG_LEN = 64;
    public const byte FLAG_T2_TIER = 1 << 0;      // NP_PROTO_FLAG_T2_TIER (app-computed, carries no authority)

    /// Compile and sign. Ed25519 covers the raw signed region and fills the last 64 bytes.
    /// `deviceSerial` is the 32-byte replay guard the hub checks (§4.2); omitted only on the bench
    /// (`HubSerial.Parse` is the seam a transport fills), where a hub with a provisioned serial refuses
    /// the descriptor (OI-AND-WIRE-02).
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
        var region = unsigned.Blob[..^SIG_LEN];
        var (signature, fingerprint) = (sign ?? SessionProtocolSigner.Sign)(region);
        if (signature.Length != SIG_LEN)
            throw new HubCompileException($"Ed25519 signature must be {SIG_LEN} bytes, got {signature.Length}.");
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
        var uuid = sessionUuid ?? RandomNumberGenerator.GetBytes(UUID_LEN);
        if (uuid.Length != UUID_LEN) throw new ArgumentException($"sessionUuid must be {UUID_LEN} bytes.", nameof(sessionUuid));

        // The zone namespace is the loaded .npps one (NP-NPPS-REF-001 §8). A name it does not hold is
        // left out, and the core refuses the protocol naming it.
        var zones = new Dictionary<string, IReadOnlyList<int>>();
        foreach (var name in definition.NppsNamedZoneRefs())
        {
            var sockets = NPZoneRegistry.Sockets(name);
            if (sockets is not null) zones[name] = sockets;
        }
        var options = new NppsCompileOptions
        {
            Zones = zones, ClinicianSockets = clinicianSockets, DeviceSerial = deviceSerial,
            NowUnix = (now ?? DateTimeOffset.UtcNow).ToUnixTimeSeconds(), SessionUuid = uuid,
            Autonomous = autonomous,
        };

        byte[] blob;
        try
        {
            blob = NppsCore.Compile(definition.NppsCoreJson(), options);
        }
        catch (NppsRefusal refusal)
        {
            throw new HubCompileException(refusal.Message);
        }
        return new HubDescriptor
        {
            Blob = blob, SessionUuid = uuid, IsT2 = (blob[6] & FLAG_T2_TIER) != 0, CmdCount = blob[7],
        };
    }
}
