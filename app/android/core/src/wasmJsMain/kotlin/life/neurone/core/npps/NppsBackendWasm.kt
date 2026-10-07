package life.neurone.core.npps

import kotlin.js.Promise
import kotlinx.coroutines.await

/**
 * The browser (and Node) binding of the shared NPPS core: `neurone_npps.wasm`, the C ABI of
 * `common/npps-ffi` built by `scripts/build-npps-wasm.sh`, instantiated by a few lines of JavaScript.
 * The module imports nothing (no clock, no randomness, no I/O), so there is no glue beyond memory.
 *
 * **Loading is asynchronous and calling is not.** A browser cannot compile a module this size
 * synchronously, so a host awaits [initNppsCore] once before it composes anything that reads a protocol;
 * every call after that is synchronous, which is what [NppsBackend] is. A call before that throws.
 */
suspend fun initNppsCore(wasmLocation: String) {
    if (!nppsIsLoaded()) nppsLoad(wasmLocation).await<JsAny?>()
}

// `globalThis.__neuroneNpps` keeps the compiled module and the live instance. A trap (a Rust panic, built
// with panic=abort) leaves the instance's memory untrustworthy, so the instance is dropped and the next
// call makes a fresh one from the module; the module itself is never reloaded.

@JsFun(
    """(location) => {
        const g = globalThis;
        const proc = g.process;
        const isNode = !!(proc && proc.versions && proc.versions.node);
        const bytes = isNode && !/^https?:/.test(location)
            ? import(/* webpackIgnore: true */ 'node:fs').then((fs) => fs.readFileSync(location))
            : fetch(location).then((r) => {
                if (!r.ok) throw new Error('could not load ' + location + ': HTTP ' + r.status);
                return r.arrayBuffer();
            });
        return bytes.then((b) => WebAssembly.compile(b)).then((m) => { g.__neuroneNpps = { module: m, exports: null }; });
    }""",
)
private external fun nppsLoad(location: String): Promise<JsAny?>

@JsFun("() => !!(globalThis.__neuroneNpps && globalThis.__neuroneNpps.module)")
private external fun nppsIsLoaded(): Boolean

@JsFun(
    """(entry, input) => {
        const state = globalThis.__neuroneNpps;
        if (!state || !state.module) throw new Error('the NPPS core is not loaded: await initNppsCore() first');
        try {
            if (!state.exports) state.exports = new WebAssembly.Instance(state.module, {}).exports;
            const w = state.exports;
            const bytes = new TextEncoder().encode(input);
            // Never empty, so the pointer handed over is never null (an empty source is a valid input).
            const inPtr = w.npps_alloc(bytes.length + 1);
            const cells = w.npps_alloc(8);   // out pointer, out length
            try {
                new Uint8Array(w.memory.buffer, inPtr, bytes.length).set(bytes);
                const code = w[entry](inPtr, bytes.length, cells, cells + 4);
                const view = new DataView(w.memory.buffer);
                const outPtr = view.getUint32(cells, true);
                const outLen = view.getUint32(cells + 4, true);
                state.out = outPtr === 0 ? new Uint8Array(0) : new Uint8Array(w.memory.buffer, outPtr, outLen).slice();
                if (outPtr !== 0) w.npps_free(outPtr, outLen);
                return code;
            } finally {
                w.npps_free(inPtr, bytes.length + 1);
                w.npps_free(cells, 8);
            }
        } catch (e) {
            state.exports = null;
            throw e;
        }
    }""",
)
private external fun nppsCall(entry: String, input: String): Int

@JsFun("() => new TextDecoder().decode(globalThis.__neuroneNpps.out)")
private external fun nppsOutText(): String

@JsFun("() => globalThis.__neuroneNpps.out.length")
private external fun nppsOutLength(): Int

@JsFun("(i) => globalThis.__neuroneNpps.out[i]")
private external fun nppsOutByte(i: Int): Int

private object WasmNppsBackend : NppsBackend {

    private fun text(entry: String, input: String): String {
        val status = nppsCall(entry, input)
        val out = nppsOutText()
        if (status != NppsFfi.OK) NppsFfi.refuse(status, out)
        return out
    }

    override fun parse(source: String): String = text("npps_parse_json", source)
    override fun namespace(request: String): String = text("npps_namespace_json", request)
    override fun serialize(request: String): String = text("npps_serialize_json", request)
    override fun validate(request: String): String = text("npps_validate_json", request)
    override fun resolveLimits(request: String): String = text("npps_resolve_limits_json", request)

    override fun compile(request: String): ByteArray {
        val status = nppsCall("npps_compile_json", request)
        if (status != NppsFfi.OK) NppsFfi.refuse(status, nppsOutText())
        return ByteArray(nppsOutLength()) { nppsOutByte(it).toByte() }
    }
}

internal actual fun defaultNppsBackend(): NppsBackend = WasmNppsBackend
