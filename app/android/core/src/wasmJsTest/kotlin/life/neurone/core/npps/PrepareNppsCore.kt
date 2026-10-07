package life.neurone.core.npps

@JsFun("() => (globalThis.process && globalThis.process.env && globalThis.process.env.NEURONE_NPPS_WASM) || ''")
private external fun wasmPathFromEnvironment(): String

/** Under Node the build passes the module's path in NEURONE_NPPS_WASM (core/build.gradle.kts). */
internal actual suspend fun prepareNppsCore() {
    val path = wasmPathFromEnvironment()
    check(path.isNotEmpty()) { "NEURONE_NPPS_WASM is not set; run the test through Gradle" }
    initNppsCore(path)
}
