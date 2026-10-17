// SessionProtocol.cs — session descriptor signing.
//
// The session descriptor is the binary blob of NP-FW-HUB-001 §4, written by
// HubDescriptorCompiler (OI-AND-WIRE-01). The hub verifies Ed25519 over its raw signed region and
// rejects unsigned or corrupted descriptors (CLAUDE.md §4.2). The JSON NPSessionProtocol, its
// Swift-Codable-shaped converter and the `NPPR` frame that used to live here were a format no
// hub could parse, and are gone.

using System.Security.Cryptography;
using System.Text.Json.Nodes;
using NeurOne.Protocol;

namespace NeurOne.Session;

// MARK: - Protocol signer

// Mirrors SessionProtocolSigner.swift.
// The Ed25519 key is a 32-byte seed, generated once at first launch, protected with DPAPI (Windows user-scope),
// and persisted to %LOCALAPPDATA%\NeurOne\. The Ed25519 arithmetic is the shared Rust core's (CLAUDE.md §22);
// this class only holds the seed and moves bytes. The file holds the same 32 raw bytes the earlier NSec build wrote,
// so an existing key keeps working. The paired hub stores the corresponding public key registered at BLE pairing.
static class SessionProtocolSigner
{
    private const int SeedBytes = 32;
    private static readonly string KeyPath = Path.Combine(
        Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
        "NeurOne", "signing.key");

    /// Ed25519 over the descriptor's raw signed region (NP-FW-HUB-001 §4.1): the bytes, not a digest.
    public static (byte[] Signature, string Fingerprint) Sign(byte[] region)
    {
        var seed = LoadOrCreateSeed();
        try
        {
            var sig = Convert.FromHexString(Ask("ed25519Sign", seed, ("message", region))["signature"]!.GetValue<string>());
            var fp = Convert.ToHexString(PublicKey(seed).AsSpan()[..8]).ToLowerInvariant();
            return (sig, fp);
        }
        finally
        {
            CryptographicOperations.ZeroMemory(seed);
        }
    }

    public static byte[] PublicKeyData()
    {
        var seed = LoadOrCreateSeed();
        try { return PublicKey(seed); }
        finally { CryptographicOperations.ZeroMemory(seed); }
    }

    private static byte[] PublicKey(byte[] seed)
        => Convert.FromHexString(Ask("ed25519PublicKey", seed)["publicKey"]!.GetValue<string>());

    private static JsonNode Ask(string op, byte[] seed, (string Name, byte[] Value)? extra = null)
    {
        var request = new JsonObject { ["op"] = op, ["seed"] = Convert.ToHexString(seed).ToLowerInvariant() };
        if (extra is { } e) request[e.Name] = Convert.ToHexString(e.Value).ToLowerInvariant();
        return NppsCore.Migration(request);
    }

    private static byte[] LoadOrCreateSeed()
    {
        if (File.Exists(KeyPath))
        {
            // Read-then-unprotect in a single try so a concurrent delete between
            // Exists and ReadAllBytes surfaces as IOException rather than silent failure.
            try
            {
                var dpapi = File.ReadAllBytes(KeyPath);
                var raw = ProtectedData.Unprotect(dpapi, null, DataProtectionScope.CurrentUser);
                if (raw.Length == SeedBytes) return raw;
            }
            catch (FileNotFoundException)
            {
                // File was deleted between Exists check and ReadAllBytes — fall through to create.
            }
        }

        Directory.CreateDirectory(Path.GetDirectoryName(KeyPath)!);
        var seed = RandomNumberGenerator.GetBytes(SeedBytes);
        var protected_ = ProtectedData.Protect(seed, null, DataProtectionScope.CurrentUser);
        File.WriteAllBytes(KeyPath, protected_);
        return seed;
    }
}
