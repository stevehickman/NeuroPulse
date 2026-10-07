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
}

/** The six entry points of the C ABI (`common/npps-ffi/include/neurone_npps.h`). */
internal enum class NppsEntry { PARSE, COMPILE, NAMESPACE, SERIALIZE, VALIDATE, RESOLVE_LIMITS }

/** What a C ABI call returns: its status code and the bytes it wrote to `out`. */
internal class NppsFfiResult(val status: Int, val output: ByteArray)

/** The status codes of `neurone_npps_status.h`, and the one place they become an exception. */
internal object NppsFfi {
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
}
