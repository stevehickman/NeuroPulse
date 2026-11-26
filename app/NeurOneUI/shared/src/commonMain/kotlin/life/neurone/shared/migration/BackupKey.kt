package life.neurone.shared.migration

import kotlinx.serialization.json.JsonObject
import life.neurone.core.data.BundleKey
import life.neurone.core.data.BundleKeyDerivation
import life.neurone.core.data.BundleKeyMethod
import life.neurone.core.data.BundleSecret
import life.neurone.core.data.RandomSource

/**
 * What a health-data backup's key comes from: the person's secret, typed for this backup, or a key the automatic backup derived from
 * it once and keeps sealed in place of the secret (`OI-PRIV-65` (2), the principal, 2026-11-24; `SWR-MG-32` (i)). Either way the bundle
 * and the recordings restore with the code or passphrase alone.
 */
sealed interface BackupKey {
    val method: BundleKeyMethod

    /** The `kdf` every bundle and every new set of recordings must be sealed under, or null for a fresh salt each time. */
    val fixedKdf: JsonObject?

    /** The key for [kdf], kept in the core, which the caller destroys; null when there is none for it here. */
    fun keyFor(kdf: JsonObject): BundleKey?

    /** The `kdf` for a new set of recordings. */
    fun newKdf(random: RandomSource): JsonObject = fixedKdf ?: BundleKeyDerivation.newKdf(method, random)
}

/** The person's own secret: each bundle and each set of recordings gets a fresh salt and a fresh Argon2id derivation. The caller destroys [secret]. */
class SecretBackupKey(override val method: BundleKeyMethod, private val secret: BundleSecret) : BackupKey {
    override val fixedKdf: JsonObject? get() = null
    override fun keyFor(kdf: JsonObject): BundleKey? = runCatching { BundleKeyDerivation.deriveKey(secret, kdf) }.getOrNull()
}

/**
 * A key derived once under [kdf] and kept sealed ([open] unseals it into a new handle each time). It is the key for [kdf] and for no
 * other, so a set of recordings made under another salt is not opened with it.
 */
class KeptBackupKey(
    override val method: BundleKeyMethod,
    private val kdf: JsonObject,
    private val open: () -> BundleKey?,
) : BackupKey {
    override val fixedKdf: JsonObject get() = kdf
    override fun keyFor(kdf: JsonObject): BundleKey? = if (kdf == this.kdf) open() else null
}
