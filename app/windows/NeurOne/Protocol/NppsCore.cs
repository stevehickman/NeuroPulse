// NppsCore.cs — the shared NPPS core (common/npps-core, OI-NPPS-CORE-01) as Windows sees it.
//
// One lexer, parser and hub-descriptor compiler written once in Rust, built as the cdylib
// neurone_npps_ffi.dll from common/npps-ffi and called through P/Invoke. This file marshals and
// nothing else: what an input means is decided in the core, so it means the same here as on every
// other runtime, and a refusal carries the message the web parser or compiler gives. The C ABI is
// common/npps-ffi/include/neurone_npps.h; the iOS counterpart is app/ios/NeurOne/Session/NppsCore.swift.
//
// The DLL must sit beside NeurOne.dll. Until it does, a call throws DllNotFoundException, which is
// a build-configuration fault and deliberately not an NppsRefusal.

using System.Runtime.InteropServices;
using System.Text;
using System.Text.Json;
using System.Text.Json.Nodes;

namespace NeurOne.Protocol;

/// The core refused the input. `Message` is the web reference's, `Line N: …` for a parse.
sealed class NppsRefusal(string message) : Exception(message);

/// What a compile needs that is not in the protocol. Everything non-deterministic is an input.
sealed class NppsCompileOptions
{
    /// Zone name → its 1-based socket ids.
    public IReadOnlyDictionary<string, IReadOnlyList<int>>? Zones { get; init; }
    public IReadOnlyList<int>? ClinicianSockets { get; init; }
    public byte[]? DeviceSerial { get; init; }
    public required long NowUnix { get; init; }
    /// Exactly 16 bytes.
    public required byte[] SessionUuid { get; init; }
    /// Channel windows overriding the shipped defaults, as the core's JSON; null keeps the defaults.
    public JsonNode? WavelengthRules { get; init; }
    /// Mode 3: sets NP_PROTO_FLAG_AUTONOMOUS in the header.
    public bool Autonomous { get; init; }
}

static class NppsCore
{
    private const string Lib = "neurone_npps_ffi";

    // Return codes of neurone_npps.h.
    private const int Ok = 0, Refused = 1, Internal = 2;

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    private static extern int npps_parse_json(byte[] src, nuint srcLen, out IntPtr output, out nuint outLen);

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    private static extern int npps_compile_json(byte[] req, nuint reqLen, out IntPtr output, out nuint outLen);

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    private static extern void npps_free(IntPtr ptr, nuint len);

    private delegate int Entry(byte[] input, nuint inputLen, out IntPtr output, out nuint outLen);

    /// Parse NPPS text into the core's JSON: an array of `{"kind":"single","protocol":{…}}` entries, or
    /// `{"kind":"skipped","what":"zone"}` for a block the core does not interpret yet.
    public static byte[] Parse(string source) => Call(npps_parse_json, Encoding.UTF8.GetBytes(source));

    /// Compile a protocol (`{"timingMode":…,"modalities":[…]}`, the shape `Parse` returns under
    /// `protocol`) into the NP-FW-HUB-001 §4 descriptor. The 64-byte signature slot at the end is zeroed:
    /// sign the region before it and write the signature in.
    public static byte[] Compile(ReadOnlySpan<byte> protocolJson, NppsCompileOptions options)
    {
        if (options.SessionUuid.Length != 16) throw new NppsRefusal("sessionUUID must be 16 bytes");

        var request = new JsonObject
        {
            ["def"] = JsonNode.Parse(protocolJson),
            ["zones"] = options.Zones is null ? null : JsonSerializer.SerializeToNode(options.Zones),
            ["clinicianSockets"] = options.ClinicianSockets is null
                ? null : JsonSerializer.SerializeToNode(options.ClinicianSockets),
            ["deviceSerialHex"] = options.DeviceSerial is null ? null : Convert.ToHexString(options.DeviceSerial).ToLowerInvariant(),
            ["nowUnix"] = options.NowUnix,
            ["sessionUuidHex"] = Convert.ToHexString(options.SessionUuid).ToLowerInvariant(),
            ["wavelengthRules"] = options.WavelengthRules?.DeepClone(),
            ["autonomous"] = options.Autonomous,
        };
        return Call(npps_compile_json, Encoding.UTF8.GetBytes(request.ToJsonString()));
    }

    private static byte[] Call(Entry fn, byte[] input)
    {
        // Never empty, so the pointer handed over is never null (an empty source is a valid input).
        var storage = new byte[input.Length + 1];
        input.CopyTo(storage, 0);

        var code = fn(storage, (nuint)input.Length, out var output, out var outLen);
        try
        {
            var bytes = new byte[output == IntPtr.Zero ? 0 : checked((int)outLen)];
            if (bytes.Length > 0) Marshal.Copy(output, bytes, 0, bytes.Length);
            return code switch
            {
                Ok => bytes,
                Refused or Internal => throw new NppsRefusal(
                    bytes.Length > 0 ? Encoding.UTF8.GetString(bytes) : "the NPPS core refused the input"),
                _ => throw new NppsRefusal("the NPPS core was given an argument it cannot read"),
            };
        }
        finally
        {
            if (output != IntPtr.Zero) npps_free(output, outLen);
        }
    }
}
