// BundleKey.cs — the user-held key of a migration or backup bundle as Windows sees it (SWR-MG-02, issue #569 packages 6d, 6f).
//
// The UHDR key is bound to the device, so a bundle moving to another PC or phone is sealed under a key made from something
// the user holds: a recovery code the app shows once, or a passphrase the user chooses. Every rule (the cost, the code's
// text form and checksum, the passphrase's normalisation and minimum length, what a header's kdf block may say) and the
// Argon2id derivation itself are the shared core's (common/npps-core, bundle_key.rs), so every app derives the same key and
// none carries a second implementation. This file only marshals them. The iOS counterpart is
// app/ios/NeurOne/Session/BundleKey.swift, Kotlin's life.neurone.core.data.BundleKey, the web's common/lib/bundleKey.ts.
//
// The secret and the key are memory-only: never written to a file, a log or the registry, never transmitted, zeroed on
// Dispose. NeurOne never holds either (CLAUDE.md §5.1).

using System.Security.Cryptography;
using System.Text.Json.Nodes;

namespace NeurOne.Protocol;

enum BundleKeyMethod
{
    RecoveryCode,
    Passphrase,
}

static class BundleKeyMethodExtensions
{
    public static string Wire(this BundleKeyMethod m) => m == BundleKeyMethod.RecoveryCode ? "recoveryCode" : "passphrase";

    public static BundleKeyMethod? FromWire(string w) => w switch
    {
        "recoveryCode" => BundleKeyMethod.RecoveryCode,
        "passphrase" => BundleKeyMethod.Passphrase,
        _ => null,
    };
}

/// Memory-only secret bytes, zeroed on Dispose.
sealed class BundleSecret : IDisposable
{
    private byte[]? _bytes;

    internal BundleSecret(byte[] bytes) => _bytes = (byte[])bytes.Clone();

    internal byte[] Bytes => _bytes ?? throw new ObjectDisposedException(nameof(BundleSecret));

    public void Dispose()
    {
        if (_bytes is not null) CryptographicOperations.ZeroMemory(_bytes);
        _bytes = null;
    }
}

/// What a secret from a person became. `Reason` is a code for the screen to word from the locale files: `malformed` or
/// `bad_checksum` (a recovery code), `too_short` (a passphrase, with `MinChars`).
sealed record BundleSecretResult(BundleSecret? Secret, string? Reason, int? MinChars)
{
    public bool Ok => Secret is not null;
}

/// A recovery code just made: the `Text` to show the user once, and the `Secret` it stands for.
sealed record GeneratedRecoveryCode(string Text, BundleSecret Secret);

/// The derived 32-byte AES-256-GCM key, as the lowercase hex the core's sealBundle and openBundle take.
sealed class BundleKey : IDisposable
{
    private string? _hex;

    internal BundleKey(string hex) => _hex = hex;

    public string Hex => _hex ?? throw new ObjectDisposedException(nameof(BundleKey));

    public void Dispose() => _hex = null;
}

static class BundleKeyDerivation
{
    public const int SaltLength = 32;
    public const int RecoveryBytes = 32;

    private static JsonObject Ask(string op, Action<JsonObject> build)
    {
        var req = new JsonObject { ["op"] = op };
        build(req);
        return NppsCore.Migration(req).AsObject();
    }

    private static string Hex(byte[] b) => Convert.ToHexString(b).ToLowerInvariant();

    /// A fresh recovery code. Show `Text` once; the bundle can be opened only with it. `random` is the platform's CSPRNG.
    public static GeneratedRecoveryCode NewRecoveryCode(Func<int, byte[]>? random = null)
    {
        var bytes = (random ?? RandomNumberGenerator.GetBytes)(RecoveryBytes);
        if (bytes.Length != RecoveryBytes) throw new NppsRefusal($"The random source gave {bytes.Length} bytes, not {RecoveryBytes}");
        var code = (string)Ask("encodeRecoveryCode", r => r["bytes"] = Hex(bytes))["code"]!;
        var secret = new BundleSecret(bytes);
        CryptographicOperations.ZeroMemory(bytes);
        return new GeneratedRecoveryCode(code, secret);
    }

    /// A recovery code typed or pasted by the user.
    public static BundleSecretResult RecoveryCodeSecret(string text) => Result(Ask("parseRecoveryCode", r => r["code"] = text));

    /// A passphrase the user chose (or typed to restore). Normalised and length-checked by the core.
    public static BundleSecretResult PassphraseSecret(string text) => Result(Ask("passphraseSecret", r => r["passphrase"] = text));

    private static BundleSecretResult Result(JsonObject r)
    {
        if (r["ok"]?.GetValue<bool>() == true)
        {
            var bytes = Convert.FromHexString((string)r["secret"]!);
            var secret = new BundleSecret(bytes);
            CryptographicOperations.ZeroMemory(bytes);
            return new BundleSecretResult(secret, null, null);
        }
        return new BundleSecretResult(null, (string?)r["reason"] ?? "malformed", r["minChars"]?.GetValue<int>());
    }

    /// The `kdf` block for the header of a bundle about to be sealed: a fresh random salt and the core's fixed cost.
    public static JsonObject NewKdf(BundleKeyMethod method, Func<int, byte[]>? random = null)
    {
        var salt = (random ?? RandomNumberGenerator.GetBytes)(SaltLength);
        if (salt.Length != SaltLength) throw new NppsRefusal($"The random source gave {salt.Length} bytes, not {SaltLength}");
        return Ask("bundleKdf", r => { r["method"] = method.Wire(); r["salt"] = Hex(salt); })["kdf"]!.AsObject();
    }

    /// The bundle key for a secret and a header's `kdf` block: Argon2id computed by the shared core. The core checks the
    /// block against the fixed cost first, so a bundle cannot ask this machine for a cheaper derivation or another algorithm.
    /// Throws NppsRefusal for a block the core refuses.
    public static BundleKey DeriveKey(BundleSecret secret, JsonObject kdf)
    {
        var r = Ask("deriveBundleKey", q => { q["secret"] = Hex(secret.Bytes); q["kdf"] = kdf.DeepClone(); });
        if (r["ok"]?.GetValue<bool>() != true) throw new NppsRefusal($"unsupported kdf: {(string?)r["reason"]}");
        return new BundleKey((string)r["key"]!);
    }
}
