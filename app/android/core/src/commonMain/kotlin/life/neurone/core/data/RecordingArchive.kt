package life.neurone.core.data

import kotlinx.serialization.json.JsonNull
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.boolean
import kotlinx.serialization.json.buildJsonObject
import kotlinx.serialization.json.int
import kotlinx.serialization.json.jsonPrimitive
import kotlinx.serialization.json.put
import life.neurone.core.npps.NppsCore

// A recording sealed for a backup, one file in chunks under the backup's key (SWR-MG-03; CLAUDE.md §22). The sealing is the Rust
// core's (`archive.rs`, `archiveSealChunk`, `archiveOpenChunk`), the same for every platform; this file cuts a recording into the
// chunks the core takes, one at a time, so a recording is never turned into one large JSON value, and puts the answers back
// together. The key is a [BundleKey] derived from the person's code or passphrase and is the caller's to destroy.

/** How an archive was refused when opened. */
enum class ArchiveProblem {
    /** The key is not the one it was sealed under, or the file was edited, cut short or rearranged. The core does not tell them apart. */
    AUTHENTICATION_FAILED,

    /** The file is not an archive, or is cut off inside its header. */
    MALFORMED,

    /** Made by a newer build than this one. */
    NEWER_VERSION,
}

sealed interface ArchiveOpened {
    class Ok(val data: ByteArray) : ArchiveOpened
    class Refused(val problem: ArchiveProblem) : ArchiveOpened
}

object RecordingArchive {
    private val info: JsonObject by lazy { NppsCore.migration(buildJsonObject { put("op", "archiveInfo") }) }

    /** The plain length of a chunk, from the core. */
    val chunkBytes: Int get() = info["chunkBytes"]!!.jsonPrimitive.int
    private val tagBytes: Int get() = info["tagBytes"]!!.jsonPrimitive.int
    private val prefixBytes: Int get() = info["prefixBytes"]!!.jsonPrimitive.int
    private val headerBytes: Int get() = info["headerBytes"]!!.jsonPrimitive.int
    private val maxKdfBytes: Int get() = info["maxKdfBytes"]!!.jsonPrimitive.int

    private fun ask(op: String, build: kotlinx.serialization.json.JsonObjectBuilder.() -> Unit): JsonObject =
        NppsCore.migration(buildJsonObject { put("op", op); build() })

    /**
     * [plain] sealed under [key]. [kdf] goes in the header when the file is the one a restore starts from (it says how the key was made);
     * the other files of a backup carry none.
     */
    fun seal(plain: ByteArray, key: BundleKey, random: RandomSource, kdf: JsonObject? = null): ByteArray {
        val prefix = random.nextBytes(prefixBytes)
        require(prefix.size == prefixBytes) { "The random source gave ${prefix.size} bytes, not $prefixBytes" }
        val header = ask("archiveStart") { put("prefix", prefix.toHex()); if (kdf != null) put("kdf", kdf) }["header"]!!.jsonPrimitive.content
        val chunk = chunkBytes
        val count = if (plain.isEmpty()) 1 else (plain.size + chunk - 1) / chunk
        val parts = ArrayList<ByteArray>(count + 1)
        parts += header.hexToBytes()
        for (i in 0 until count) {
            val from = i * chunk
            val data = plain.copyOfRange(from, minOf(plain.size, from + chunk))
            val sealed = ask("archiveSealChunk") {
                put("key", key.hex()); put("header", header); put("counter", i); put("last", i == count - 1); put("data", data.toHex())
            }["sealed"]!!.jsonPrimitive.content
            parts += sealed.hexToBytes()
        }
        return join(parts)
    }

    /** The `kdf` block in the header of [archive], or null when it has none or is not an archive. */
    fun readKdf(archive: ByteArray): JsonObject? {
        val head = readHeader(archive) ?: return null
        return head.kdf
    }

    /** [archive] opened under [key]. */
    fun open(archive: ByteArray, key: BundleKey): ArchiveOpened {
        val head = readHeader(archive)
            ?: return ArchiveOpened.Refused(headerProblem(archive))
        val header = archive.copyOfRange(0, head.length).toHex()
        val full = chunkBytes + tagBytes
        val body = archive.size - head.length
        val count = maxOf(1, (body + full - 1) / full)
        val out = ArrayList<ByteArray>(count)
        for (i in 0 until count) {
            val from = head.length + i * full
            val part = archive.copyOfRange(from, minOf(archive.size, from + full))
            val r = ask("archiveOpenChunk") {
                put("key", key.hex()); put("header", header); put("counter", i); put("last", i == count - 1); put("sealed", part.toHex())
            }
            if (!r["ok"]!!.jsonPrimitive.boolean) return ArchiveOpened.Refused(ArchiveProblem.AUTHENTICATION_FAILED)
            out += r["data"]!!.jsonPrimitive.content.hexToBytes()
        }
        return ArchiveOpened.Ok(join(out))
    }

    private class Head(val length: Int, val kdf: JsonObject?)

    private fun readHeader(archive: ByteArray): Head? {
        val r = ask("archiveReadHeader") { put("bytes", archive.copyOfRange(0, minOf(archive.size, headerBytes + maxKdfBytes)).toHex()) }
        if (!r["ok"]!!.jsonPrimitive.boolean) return null
        val kdf = r["kdf"]
        return Head(r["headerLength"]!!.jsonPrimitive.int, if (kdf == null || kdf is JsonNull || kdf !is JsonObject) null else kdf)
    }

    private fun headerProblem(archive: ByteArray): ArchiveProblem {
        val r = ask("archiveReadHeader") { put("bytes", archive.copyOfRange(0, minOf(archive.size, headerBytes + maxKdfBytes)).toHex()) }
        return if (r["reason"]?.jsonPrimitive?.content == "newer_version") ArchiveProblem.NEWER_VERSION else ArchiveProblem.MALFORMED
    }

    private fun join(parts: List<ByteArray>): ByteArray {
        val out = ByteArray(parts.sumOf { it.size })
        var at = 0
        for (p in parts) { p.copyInto(out, at); at += p.size }
        return out
    }
}
