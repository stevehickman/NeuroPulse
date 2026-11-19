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

// The secret entry point (SWR-MG-32). Kotlin/Wasm cannot hand a ByteArray to JavaScript, so the secrets are staged one byte
// at a time into an array JavaScript owns, copied from there into the module's memory for the call, and both are
// overwritten before the call returns. The answer's secrets come back the same way and are overwritten once read.
@JsFun("(n) => { globalThis.__neuroneNpps.secretsIn = new Uint8Array(n); }")
private external fun nppsSecretsBegin(n: Int)

@JsFun("(i, b) => { globalThis.__neuroneNpps.secretsIn[i] = b; }")
private external fun nppsSecretsPut(i: Int, b: Int)

@JsFun(
    """(input) => {
        const state = globalThis.__neuroneNpps;
        if (!state || !state.module) throw new Error('the NPPS core is not loaded: await initNppsCore() first');
        const given = state.secretsIn || new Uint8Array(0);
        try {
            if (!state.exports) state.exports = new WebAssembly.Instance(state.module, {}).exports;
            const w = state.exports;
            const bytes = new TextEncoder().encode(input);
            // Never empty, so no pointer handed over is null.
            const inPtr = w.npps_alloc(bytes.length + 1);
            const secPtr = w.npps_alloc(given.length + 1);
            const cells = w.npps_alloc(16);   // out pointer, out length, secret-out pointer, secret-out length
            try {
                new Uint8Array(w.memory.buffer, inPtr, bytes.length).set(bytes);
                new Uint8Array(w.memory.buffer, secPtr, given.length).set(given);
                const code = w.npps_migration_secret_json(inPtr, bytes.length, secPtr, given.length, cells, cells + 4, cells + 8, cells + 12);
                const view = new DataView(w.memory.buffer);
                const outPtr = view.getUint32(cells, true);
                const outLen = view.getUint32(cells + 4, true);
                const besidePtr = view.getUint32(cells + 8, true);
                const besideLen = view.getUint32(cells + 12, true);
                state.out = outPtr === 0 ? new Uint8Array(0) : new Uint8Array(w.memory.buffer, outPtr, outLen).slice();
                state.secretsOut = besidePtr === 0 ? new Uint8Array(0) : new Uint8Array(w.memory.buffer, besidePtr, besideLen).slice();
                if (outPtr !== 0) w.npps_free(outPtr, outLen);
                if (besidePtr !== 0) w.npps_free(besidePtr, besideLen);
                return code;
            } finally {
                new Uint8Array(w.memory.buffer, inPtr, bytes.length + 1).fill(0);
                new Uint8Array(w.memory.buffer, secPtr, given.length + 1).fill(0);
                bytes.fill(0);
                w.npps_free(inPtr, bytes.length + 1);
                w.npps_free(secPtr, given.length + 1);
                w.npps_free(cells, 16);
            }
        } catch (e) {
            state.exports = null;
            throw e;
        } finally {
            given.fill(0);
            state.secretsIn = null;
        }
    }""",
)
private external fun nppsCallSecret(input: String): Int

@JsFun("() => globalThis.__neuroneNpps.secretsOut.length")
private external fun nppsSecretsOutLength(): Int

@JsFun("(i) => globalThis.__neuroneNpps.secretsOut[i]")
private external fun nppsSecretsOutByte(i: Int): Int

@JsFun("() => { const s = globalThis.__neuroneNpps; if (s.secretsOut) s.secretsOut.fill(0); s.secretsOut = null; if (s.out) s.out.fill(0); }")
private external fun nppsSecretsOutClear()

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
    override fun contentHash(request: String): String = text("npps_content_hash_json", request)
    override fun library(request: String): String = text("npps_library_json", request)
    override fun migration(request: String): String = text("npps_migration_json", request)

    override fun migrationSecret(request: String, secrets: ByteArray): ByteArray {
        nppsSecretsBegin(secrets.size)
        for (i in secrets.indices) nppsSecretsPut(i, secrets[i].toInt() and 0xff)
        val status = nppsCallSecret(request)
        try {
            if (status != NppsFfi.OK) NppsFfi.refuse(status, nppsOutText())
            val answer = ByteArray(nppsOutLength()) { nppsOutByte(it).toByte() }
            val answered = ByteArray(nppsSecretsOutLength()) { nppsSecretsOutByte(it).toByte() }
            return try { NppsFfi.joined(answer, answered) } finally { answered.fill(0); answer.fill(0) }
        } finally {
            nppsSecretsOutClear()
        }
    }

    override fun compile(request: String): ByteArray {
        val status = nppsCall("npps_compile_json", request)
        if (status != NppsFfi.OK) NppsFfi.refuse(status, nppsOutText())
        return ByteArray(nppsOutLength()) { nppsOutByte(it).toByte() }
    }
}

internal actual fun defaultNppsBackend(): NppsBackend = WasmNppsBackend
