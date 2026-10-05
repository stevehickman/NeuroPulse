// SessionProtocol.cs — session descriptor signing.
//
// The session descriptor is the binary blob of NP-FW-HUB-001 §4, written by
// HubDescriptorCompiler (OI-AND-WIRE-01). The hub verifies Ed25519 over its raw signed region and
// rejects unsigned or corrupted descriptors (CLAUDE.md §4.2). The JSON NPSessionProtocol, its
// Swift-Codable-shaped converter and the `NPPR` frame that used to live here were a format no
// hub could parse, and are gone.

using System.Security.Cryptography;
using NSec.Cryptography;

namespace NeurOne.Session;

// MARK: - Protocol signer

// Mirrors SessionProtocolSigner.swift.
// The Ed25519 private key is generated once at first launch, protected with
// DPAPI (Windows user-scope), and persisted to %LOCALAPPDATA%\NeurOne\.
// The paired hub stores the corresponding public key registered at BLE pairing.
static class SessionProtocolSigner
{
    private static readonly SignatureAlgorithm Ed25519Alg = SignatureAlgorithm.Ed25519;
    private static readonly string KeyPath = Path.Combine(
        Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
        "NeurOne", "signing.key");

    /// Ed25519 over the descriptor's raw signed region (NP-FW-HUB-001 §4.1): the bytes, not a digest.
    public static (byte[] Signature, string Fingerprint) Sign(byte[] region)
    {
        using var key = LoadOrCreateSigningKey();
        var sig = Ed25519Alg.Sign(key, region);
        var fp = Convert.ToHexString(key.PublicKey.Export(KeyBlobFormat.RawPublicKey).AsSpan()[..8]).ToLowerInvariant();
        return (sig, fp);
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
            // Read-then-unprotect in a single try so a concurrent delete between
            // Exists and ReadAllBytes surfaces as IOException rather than silent failure.
            try
            {
                var dpapi = File.ReadAllBytes(KeyPath);
                var raw = ProtectedData.Unprotect(dpapi, null, DataProtectionScope.CurrentUser);
                return Key.Import(Ed25519Alg, raw, KeyBlobFormat.RawPrivateKey, creationParams);
            }
            catch (FileNotFoundException)
            {
                // File was deleted between Exists check and ReadAllBytes — fall through to create.
            }
        }

        Directory.CreateDirectory(Path.GetDirectoryName(KeyPath)!);
        var key = Key.Create(Ed25519Alg, creationParams);
        var exported = key.Export(KeyBlobFormat.RawPrivateKey);
        try
        {
            var protected_ = ProtectedData.Protect(exported, null, DataProtectionScope.CurrentUser);
            File.WriteAllBytes(KeyPath, protected_);
        }
        finally
        {
            // Zero private key bytes before GC can observe them.
            CryptographicOperations.ZeroMemory(exported);
        }
        return key;
    }
}
