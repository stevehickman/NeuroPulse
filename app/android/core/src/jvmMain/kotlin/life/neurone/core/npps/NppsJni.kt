package life.neurone.core.npps

/**
 * JNI binding of the shared NPPS core (`common/npps-jni`). The Rust entry points are named for
 * this class (`Java_life_neurone_core_npps_NppsJni_native*`), so renaming it means renaming them.
 */
internal object NppsJni : NppsBackend {
    init {
        life.neurone.core.platform.NativeLibrary.load("neurone_npps_jni")
    }

    @JvmStatic private external fun nativeParse(source: String): String

    @JvmStatic private external fun nativeCompile(request: String): ByteArray

    @JvmStatic private external fun nativeNamespace(request: String): String

    @JvmStatic private external fun nativeSerialize(request: String): String

    @JvmStatic private external fun nativeValidate(request: String): String

    @JvmStatic private external fun nativeResolveLimits(request: String): String

    override fun parse(source: String): String = nativeParse(source)
    override fun compile(request: String): ByteArray = nativeCompile(request)
    override fun namespace(request: String): String = nativeNamespace(request)
    override fun serialize(request: String): String = nativeSerialize(request)
    override fun validate(request: String): String = nativeValidate(request)
    override fun resolveLimits(request: String): String = nativeResolveLimits(request)
}

internal actual fun defaultNppsBackend(): NppsBackend = NppsJni
