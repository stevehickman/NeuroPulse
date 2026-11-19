package life.neurone.core.npps

/**
 * The six calls the Rust NPPS core exposes, as strings in and strings out (bytes out for
 * [compile]). [NppsCore] marshals JSON around them and nothing else, so a backend is one
 * binding of `common/npps-core`: JNI on the JVM, the C ABI (`common/npps-ffi`) on Apple
 * targets, the WebAssembly build in a browser. Every binding must answer for the same input
 * exactly as the others do, because the core, not the binding, decides what an input means.
 */
internal interface NppsBackend {
    fun parse(source: String): String
    fun compile(request: String): ByteArray
    fun namespace(request: String): String
    fun serialize(request: String): String
    fun validate(request: String): String
    fun resolveLimits(request: String): String

    /** A protocol's identity: 64 lowercase hex digits (`SWR-HV-01`). */
    fun contentHash(request: String): String

    /** The protocol library's rules (`SWR-PL`, `SWR-UP`, `SWR-WO`, `SWR-FM`, `SWR-HV-04`): `{"op": …}` in, JSON out. */
    fun library(request: String): String

    /** The device-replacement migration and UHDR backup rules (`SWR-MG`): `{"op": …}` in, JSON out. */
    fun migration(request: String): String

    /**
     * [migration] for an operation that takes or returns a key (`SWR-MG-30`): [secrets] travels beside the request as bytes
     * (each a 4-byte big-endian length and then the bytes; the n-th is `{"$secret": n}` in the JSON) and is never text. The
     * answer is one array: a 4-byte big-endian length and the answer's JSON, then the answer's secrets framed the same way.
     * The caller overwrites both arrays.
     *
     * A binding that has not been given this entry point refuses here, by name, and nothing falls back to sending the key
     * as text. Every binding has it: JNI (the JVM and Android), the C ABI through cinterop (iOS) and WebAssembly (the browser).
     */
    fun migrationSecret(request: String, secrets: ByteArray): ByteArray = throw UnsupportedOperationException(
        "This binding of the shared NPPS core cannot yet take a key as bytes (OI-PRIV-12 stage 2).",
    )
}

/** The backend this target has, or one that refuses with a message naming the missing binding. */
internal expect fun defaultNppsBackend(): NppsBackend

/** For a target whose binding is not wired yet: refuses on use, so the gap is loud, not silent. */
internal class UnwiredNppsBackend(private val target: String) : NppsBackend {
    private fun unwired(): Nothing = throw UnsupportedOperationException(
        "The shared NPPS core has no binding on $target yet (OI-UI-KMP-01).",
    )

    override fun parse(source: String): String = unwired()
    override fun compile(request: String): ByteArray = unwired()
    override fun namespace(request: String): String = unwired()
    override fun serialize(request: String): String = unwired()
    override fun validate(request: String): String = unwired()
    override fun resolveLimits(request: String): String = unwired()
    override fun contentHash(request: String): String = unwired()
    override fun library(request: String): String = unwired()
    override fun migration(request: String): String = unwired()
}

/** The entry points of the C ABI (`common/npps-ffi/include/neurone_npps.h`). */
internal enum class NppsEntry { PARSE, COMPILE, NAMESPACE, SERIALIZE, VALIDATE, RESOLVE_LIMITS, CONTENT_HASH, LIBRARY, MIGRATION }

/** What a C ABI call returns: its status code and the bytes it wrote to `out`. */
internal class NppsFfiResult(val status: Int, val output: ByteArray)

/** What `npps_migration_secret_json` returns: its status, the bytes it wrote to `out`, and those it wrote to `secret_out`. */
internal class NppsSecretFfiResult(val status: Int, val output: ByteArray, val secrets: ByteArray)

/** The status codes of `neurone_npps_status.h`, and the one place they become an exception. */
internal object NppsFfi {
    /**
     * What [NppsBackend.migrationSecret] answers, from the two buffers of the C ABI: a 4-byte big-endian length and the
     * answer's JSON, then the answer's secrets as the core framed them. The JNI library builds the same array in Rust.
     */
    fun joined(answer: ByteArray, secrets: ByteArray): ByteArray {
        val out = ByteArray(4 + answer.size + secrets.size)
        for (k in 0..3) out[k] = (answer.size ushr (24 - 8 * k)).toByte()
        answer.copyInto(out, 4)
        secrets.copyInto(out, 4 + answer.size)
        return out
    }

    const val OK = 0
    const val REFUSED = 1
    const val INTERNAL_ERROR = 2
    const val BAD_ARGUMENT = 3

    /**
     * Every non-OK status is an [IllegalArgumentException], as the JNI binding throws it: a refusal carries the
     * core's own message (`Line N: …`), and a panic or an unreadable argument carries a fixed one.
     */
    fun refuse(status: Int, message: String?): Nothing = throw IllegalArgumentException(
        when {
            (status == REFUSED || status == INTERNAL_ERROR) && !message.isNullOrEmpty() -> message
            status == REFUSED || status == INTERNAL_ERROR -> "the NPPS core refused the input"
            else -> "the NPPS core was given an argument it cannot read"
        },
    )
}

/**
 * A backend over the C ABI, for targets that reach the core through `npps_*_json(src, len, &out, &out_len)`
 * (the Apple targets, by cinterop on `common/npps-ffi`). A subclass does the call and nothing else;
 * the status handling and the UTF-8 are here, once, so they cannot differ between such targets.
 */
internal abstract class CAbiNppsBackend : NppsBackend {
    protected abstract fun call(entry: NppsEntry, input: ByteArray): NppsFfiResult

    /** `npps_migration_secret_json`: the status, the answer, and the answer's secrets as the core framed them. */
    protected abstract fun callSecret(input: ByteArray, secrets: ByteArray): NppsSecretFfiResult

    override fun migrationSecret(request: String, secrets: ByteArray): ByteArray {
        val result = callSecret(request.encodeToByteArray(), secrets)
        try {
            if (result.status != NppsFfi.OK) NppsFfi.refuse(result.status, result.output.decodeToString())
            return NppsFfi.joined(result.output, result.secrets)
        } finally {
            result.secrets.fill(0)
            result.output.fill(0)
        }
    }

    private fun bytes(entry: NppsEntry, input: String): ByteArray {
        val result = call(entry, input.encodeToByteArray())
        if (result.status != NppsFfi.OK) NppsFfi.refuse(result.status, result.output.decodeToString())
        return result.output
    }

    override fun parse(source: String): String = bytes(NppsEntry.PARSE, source).decodeToString()
    override fun compile(request: String): ByteArray = bytes(NppsEntry.COMPILE, request)
    override fun namespace(request: String): String = bytes(NppsEntry.NAMESPACE, request).decodeToString()
    override fun serialize(request: String): String = bytes(NppsEntry.SERIALIZE, request).decodeToString()
    override fun validate(request: String): String = bytes(NppsEntry.VALIDATE, request).decodeToString()
    override fun resolveLimits(request: String): String = bytes(NppsEntry.RESOLVE_LIMITS, request).decodeToString()
    override fun contentHash(request: String): String = bytes(NppsEntry.CONTENT_HASH, request).decodeToString()
    override fun library(request: String): String = bytes(NppsEntry.LIBRARY, request).decodeToString()
    override fun migration(request: String): String = bytes(NppsEntry.MIGRATION, request).decodeToString()
}
