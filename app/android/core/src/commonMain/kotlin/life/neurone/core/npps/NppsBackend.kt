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
