package com.neuropulse.core.data

// UHDR key derivation — platform-independent core of the iOS UHDRKeyManager.
// Parameters are locked per NP-FW-EMMC-001 Rev A §6 / RFC 9106 and must match
// the iOS app (PHC argon2 vendored there) and hub firmware exactly:
// Argon2id, m=65536 KiB, t=4, p=1, 64-byte output → K1+K2 for AES-256-XTS.
//
// The Argon2 primitive itself is provided by the :app module (vendored
// libsodium/signal-argon2 as SOUP — NP-SW-001 SOUP table entry required
// before beta). The derived key is memory-only: never persisted to any
// KeyValueStore, Keystore, or file, and never transmitted. NeuroPulse never
// holds the key (CLAUDE.md §5.1).

interface Argon2Provider {
    /**
     * Argon2id raw hash. Implementations MUST honor the exact parameters given
     * (no silent parameter substitution).
     */
    fun argon2idHash(
        password: ByteArray,
        salt: ByteArray,
        memoryKiB: Int,
        iterations: Int,
        parallelism: Int,
        outputLength: Int,
    ): ByteArray
}

/** Memory-only key material. Zero after use via [destroy]. */
class UhdrKey(bytes: ByteArray) {
    init {
        require(bytes.size == UhdrKeyDerivation.OUTPUT_LENGTH) {
            "UHDR key must be ${UhdrKeyDerivation.OUTPUT_LENGTH} bytes"
        }
    }

    private val material = bytes.copyOf()
    private var destroyed = false

    /** First 32 bytes — AES-256-XTS K1. */
    val k1: ByteArray get() = checkAlive().copyOfRange(0, 32)

    /** Last 32 bytes — AES-256-XTS K2. */
    val k2: ByteArray get() = checkAlive().copyOfRange(32, 64)

    private fun checkAlive(): ByteArray {
        check(!destroyed) { "UhdrKey has been destroyed" }
        return material
    }

    /** Best-effort zeroization (parallel of the iOS bzero-in-deinit). */
    fun destroy() {
        material.fill(0)
        destroyed = true
    }
}

object UhdrKeyDerivation {
    // Locked parameters — RFC 9106 / NP-FW-EMMC-001 §6. Do not change without
    // a firmware-coordinated migration; existing UHDR partitions would become
    // undecryptable.
    const val MEMORY_KIB = 65_536
    const val ITERATIONS = 4
    const val PARALLELISM = 1
    const val OUTPUT_LENGTH = 64
    const val SALT_LENGTH = 32

    /**
     * Derive the UHDR key from the credential-protected seed and the
     * device-bound salt. The result is memory-only.
     */
    fun deriveKey(provider: Argon2Provider, credentialSeed: ByteArray, salt: ByteArray): UhdrKey {
        require(salt.size == SALT_LENGTH) { "Salt must be $SALT_LENGTH bytes" }
        require(credentialSeed.isNotEmpty()) { "Credential seed must not be empty" }
        val raw = provider.argon2idHash(
            password = credentialSeed,
            salt = salt,
            memoryKiB = MEMORY_KIB,
            iterations = ITERATIONS,
            parallelism = PARALLELISM,
            outputLength = OUTPUT_LENGTH,
        )
        val key = UhdrKey(raw)
        raw.fill(0)
        return key
    }
}
