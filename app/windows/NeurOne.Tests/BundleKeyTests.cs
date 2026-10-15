using System.Text.Json.Nodes;
using NeurOne.Protocol;
using Xunit;

namespace NeurOne.Tests;

// The user-held bundle key through neurone_npps_ffi.dll, Argon2id included (SWR-MG-02). The Rust tests hold the rules (the
// code's checksum, the passphrase's normalisation, the pinned cost); this holds the marshalling. The same cases run on
// Kotlin and iOS.
public class BundleKeyTests
{
    private static byte[] Steps(int n) => Enumerable.Range(0, n).Select(i => (byte)(i % 251)).ToArray();

    // @verifies SWR-MG-02
    [Fact]
    public void ARecoveryCodeShownToTheUserDerivesTheSameKeyWhenTypedBack()
    {
        var made = BundleKeyDerivation.NewRecoveryCode(Steps);
        var kdf = BundleKeyDerivation.NewKdf(BundleKeyMethod.RecoveryCode, Steps);
        using var first = BundleKeyDerivation.DeriveKey(made.Secret, kdf);
        var typed = BundleKeyDerivation.RecoveryCodeSecret(made.Text.ToLowerInvariant().Replace('-', ' '));
        Assert.True(typed.Ok);
        using var second = BundleKeyDerivation.DeriveKey(typed.Secret!, kdf);
        Assert.Equal(first.Hex, second.Hex);
    }

    // @verifies SWR-MG-05
    [Fact]
    public void AHeaderCannotAskThisMachineForACheaperDerivation()
    {
        var made = BundleKeyDerivation.NewRecoveryCode(Steps);
        var kdf = BundleKeyDerivation.NewKdf(BundleKeyMethod.Passphrase, Steps);
        using (var ok = BundleKeyDerivation.DeriveKey(made.Secret, kdf)) Assert.Equal(64, ok.Hex.Length);
        var weak = (JsonObject)kdf.DeepClone();
        weak["memoryKiB"] = 8;
        Assert.Throws<NppsRefusal>(() => BundleKeyDerivation.DeriveKey(made.Secret, weak));
        var other = (JsonObject)kdf.DeepClone();
        other["algorithm"] = "argon2i";
        Assert.Throws<NppsRefusal>(() => BundleKeyDerivation.DeriveKey(made.Secret, other));
    }

    // @verifies SWR-MG-02
    [Fact]
    public void APassphraseIsRefusedWhenShortAndNormalisedWhenNot()
    {
        var shortOne = BundleKeyDerivation.PassphraseSecret("too short");
        Assert.False(shortOne.Ok);
        Assert.Equal("too_short", shortOne.Reason);
        Assert.True(shortOne.MinChars > 0);
        var kdf = BundleKeyDerivation.NewKdf(BundleKeyMethod.Passphrase, Steps);
        using var a = BundleKeyDerivation.PassphraseSecret("café au lait noir").Secret!;
        using var b = BundleKeyDerivation.PassphraseSecret("café au lait noir").Secret!;
        using var ka = BundleKeyDerivation.DeriveKey(a, kdf);
        using var kb = BundleKeyDerivation.DeriveKey(b, kdf);
        Assert.Equal(ka.Hex, kb.Hex);
    }

    // @verifies SWR-MG-02
    [Fact]
    public void AMistypedCodeIsATypingErrorAndADisposedSecretRefusesUse()
    {
        var made = BundleKeyDerivation.NewRecoveryCode(Steps);
        var flipped = (made.Text[0] == '2' ? '3' : '2') + made.Text[1..];
        var r = BundleKeyDerivation.RecoveryCodeSecret(flipped);
        Assert.False(r.Ok);
        Assert.Equal("bad_checksum", r.Reason);
        var kdf = BundleKeyDerivation.NewKdf(BundleKeyMethod.RecoveryCode, Steps);
        made.Secret.Dispose();
        Assert.Throws<ObjectDisposedException>(() => BundleKeyDerivation.DeriveKey(made.Secret, kdf));
    }
}
