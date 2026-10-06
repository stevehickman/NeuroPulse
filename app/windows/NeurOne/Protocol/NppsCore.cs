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

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    private static extern int npps_namespace_json(byte[] req, nuint reqLen, out IntPtr output, out nuint outLen);

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    private static extern int npps_serialize_json(byte[] req, nuint reqLen, out IntPtr output, out nuint outLen);

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    private static extern int npps_validate_json(byte[] req, nuint reqLen, out IntPtr output, out nuint outLen);

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    private static extern int npps_resolve_limits_json(byte[] req, nuint reqLen, out IntPtr output, out nuint outLen);

    private delegate int Entry(byte[] input, nuint inputLen, out IntPtr output, out nuint outLen);

    /// Parse NPPS text into everything the file declares, as the core's JSON: `{"entries":[{"kind":"single",
    /// "protocol":{…}} | {"kind":"composite","composite":{…}}], "zones":[…], "conditions":[…],
    /// "wavelengthRules":[…], "limits":[…]}`. Ids and timestamps are left out; the caller supplies them.
    public static byte[] Parse(string source) => Call(npps_parse_json, Encoding.UTF8.GetBytes(source));

    /// Fold parse results, in load order, into one namespace and check its references:
    /// `{"entries", "zones", "conditions", "errors", "referenceErrors"}`. A name two files define is left
    /// undefined and reported in `errors`.
    public static byte[] Namespace(IEnumerable<JsonNode> files)
    {
        var request = new JsonObject { ["files"] = new JsonArray(files.Select(f => (JsonNode?)f.DeepClone()).ToArray()) };
        return Call(npps_namespace_json, Encoding.UTF8.GetBytes(request.ToJsonString()));
    }

    /// Write models as `.npps` text. Each item is one of the shapes `Parse` returns (`{"kind":"single","protocol":…}`,
    /// `{"kind":"composite","composite":…}`, `{"kind":"zone","zone":…}`, `{"kind":"condition","condition":…}`,
    /// `{"kind":"wavelengthRules","wavelengthRules":…}`, `{"kind":"limits","limits":…}`); items are separated by a blank
    /// line. A model that cannot be written (a zone holding an id that is not a socket) is refused with the core's
    /// message rather than written into a file the parser would reject.
    public static string Serialize(IEnumerable<JsonNode> items)
    {
        var request = new JsonObject { ["items"] = new JsonArray(items.Select(i => (JsonNode?)i.DeepClone()).ToArray()) };
        return Encoding.UTF8.GetString(Call(npps_serialize_json, Encoding.UTF8.GetBytes(request.ToJsonString())));
    }

    /// Validate an entry against resolved limits: `{"issues":[…], "isValid", "hasWarnings"}`. The request is
    /// `{"entry":…, "limits":…, "allProtocols":[…]|null, "zones":{…}|null, "limitSources":{…}|null}`. The core
    /// returns locale keys and arguments, never text: a message is a plain string or `{"key", "args"}`, and the caller
    /// resolves a key with its own strings. (Windows has no localized UI layer yet, so nothing resolves them today.)
    public static byte[] Validate(JsonNode request)
        => Call(npps_validate_json, Encoding.UTF8.GetBytes(request.ToJsonString()));

    /// Resolve three limit sets (null for a tier that does not exist), most specific first, field by field:
    /// `{"limits":{"level":"global", <modality blocks>}, "sources":{<modalityProperty>:{<limitField>:tier}}}`. `sources` is what
    /// the validator takes as `limitSources`. (Windows has no limits store yet, so nothing calls this today.)
    public static byte[] ResolveLimits(JsonNode? global, JsonNode? helmet, JsonNode? individual)
    {
        var request = new JsonObject
        {
            ["global"] = global?.DeepClone(),
            ["helmet"] = helmet?.DeepClone(),
            ["individual"] = individual?.DeepClone(),
        };
        return Call(npps_resolve_limits_json, Encoding.UTF8.GetBytes(request.ToJsonString()));
    }

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
