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

/**
 * [value] with exactly [digits] decimals, rounded half away from zero, as `String.format("%.Nf")`
 * does on the JVM, which Kotlin common does not have. Written once so a score reads the same on
 * every platform.
 */
fun formatFixed(value: Double, digits: Int): String {
    require(digits in 0..9)
    if (value.isNaN()) return "NaN"
    if (value.isInfinite()) return if (value > 0) "Infinity" else "-Infinity"
    var scale = 1L
    repeat(digits) { scale *= 10 }
    val scaled = kotlin.math.floor(kotlin.math.abs(value) * scale + 0.5).toLong()
    val whole = scaled / scale
    val fraction = (scaled % scale).toString().padStart(digits, '0')
    val sign = if (value < 0 && scaled != 0L) "-" else ""
    return if (digits == 0) "$sign$whole" else "$sign$whole.$fraction"
}
