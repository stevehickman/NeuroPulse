package life.neurone.core.platform

import life.neurone.core.platform.todayIso
import kotlinx.datetime.Clock
import kotlinx.datetime.TimeZone
import kotlinx.datetime.todayIn

/**
 * The few things `:core` needs from the operating system. Each has one actual per target, and
 * each is small enough that a reviewer can read all of them: anything larger belongs in common
 * code, where it runs the same everywhere.
 */

/** Cryptographically secure random bytes. */
expect fun secureRandomBytes(count: Int): ByteArray

/**
 * Verify an Ed25519 signature against a raw 32-byte public key, or return null when this target
 * has no Ed25519 verifier. Null is "cannot check", never "valid": callers refuse on null.
 */
expect fun ed25519Verify(publicKeyRaw: ByteArray, message: ByteArray, signature: ByteArray): Boolean?

/** Today in the device's time zone, `yyyy-MM-dd`. */
fun todayIso(): String = Clock.System.todayIn(TimeZone.currentSystemDefault()).toString()

/** Seconds since the Unix epoch. */
fun epochSeconds(): Long = Clock.System.now().epochSeconds
