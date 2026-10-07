package life.neurone.core.npps

import kotlinx.cinterop.ExperimentalForeignApi
import kotlinx.cinterop.ULongVar
import kotlinx.cinterop.UByteVar
import kotlinx.cinterop.ByteVar
import kotlinx.cinterop.CPointerVar
import kotlinx.cinterop.addressOf
import kotlinx.cinterop.alloc
import kotlinx.cinterop.convert
import kotlinx.cinterop.memScoped
import kotlinx.cinterop.ptr
import kotlinx.cinterop.readBytes
import kotlinx.cinterop.reinterpret
import kotlinx.cinterop.usePinned
import kotlinx.cinterop.value
import life.neurone.core.npps.ffi.npps_compile_json
import life.neurone.core.npps.ffi.npps_free
import life.neurone.core.npps.ffi.npps_namespace_json
import life.neurone.core.npps.ffi.npps_parse_json
import life.neurone.core.npps.ffi.npps_resolve_limits_json
import life.neurone.core.npps.ffi.npps_serialize_json
import life.neurone.core.npps.ffi.npps_validate_json

/**
 * The Apple binding of the shared NPPS core: the C ABI of `common/npps-ffi` (`libneurone_npps_ffi.a`, built
 * by the `buildNppsFfi*` Gradle tasks) called through cinterop. Marshalling only: a source goes in as bytes, the
 * core's output is copied out and handed back to `npps_free`. Written without a macOS host to compile it on.
 */
@OptIn(ExperimentalForeignApi::class)
internal class CinteropNppsBackend : CAbiNppsBackend() {

    override fun call(entry: NppsEntry, input: ByteArray): NppsFfiResult = memScoped {
        val outPointer = alloc<CPointerVar<UByteVar>>()
        val outLength = alloc<ULongVar>()
        // The core refuses a null source, and an empty `.npps` file is a valid input: pin a byte that is never read.
        val source = if (input.isEmpty()) ByteArray(1) else input

        val status = source.usePinned { pinned ->
            val src = pinned.addressOf(0).reinterpret<UByteVar>()
            val length = input.size.convert<ULong>()
            when (entry) {
                NppsEntry.PARSE -> npps_parse_json(src, length, outPointer.ptr, outLength.ptr)
                NppsEntry.COMPILE -> npps_compile_json(src, length, outPointer.ptr, outLength.ptr)
                NppsEntry.NAMESPACE -> npps_namespace_json(src, length, outPointer.ptr, outLength.ptr)
                NppsEntry.SERIALIZE -> npps_serialize_json(src, length, outPointer.ptr, outLength.ptr)
                NppsEntry.VALIDATE -> npps_validate_json(src, length, outPointer.ptr, outLength.ptr)
                NppsEntry.RESOLVE_LIMITS -> npps_resolve_limits_json(src, length, outPointer.ptr, outLength.ptr)
            }
        }

        val pointer = outPointer.value
        val written = outLength.value
        val output = if (pointer == null || written == 0uL) {
            ByteArray(0)
        } else {
            pointer.reinterpret<ByteVar>().readBytes(written.toInt())
        }
        if (pointer != null) npps_free(pointer, written)
        NppsFfiResult(status, output)
    }
}

internal actual fun defaultNppsBackend(): NppsBackend = CinteropNppsBackend()
