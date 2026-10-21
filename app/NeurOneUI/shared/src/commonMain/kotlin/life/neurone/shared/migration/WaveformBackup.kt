package life.neurone.shared.migration

import kotlinx.serialization.json.Json
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.buildJsonObject
import kotlinx.serialization.json.jsonObject
import kotlinx.serialization.json.jsonPrimitive
import kotlinx.serialization.json.put
import life.neurone.core.data.ArchiveOpened
import life.neurone.core.data.ArchiveProblem
import life.neurone.core.data.BundleKey
import life.neurone.core.data.BundleKeyDerivation
import life.neurone.core.data.BundleKeyMethod
import life.neurone.core.data.BundleSecret
import life.neurone.core.data.RandomSource
import life.neurone.core.data.RecordingArchive
import life.neurone.core.data.SystemRandomSource
import life.neurone.shared.MigrationPlace
import life.neurone.shared.session.UhdrFolder
import kotlin.coroutines.cancellation.CancellationException

// The recordings in a health-data backup (SWR-MG-03; OI-BACKUP-01). A recording is raw EEG and HRV waveform (UHDR, CLAUDE.md §5.1) kept
// in the app's UHDR folder, and the backup bundle holds small records as one object, which does not suit it. So each recording is
// sealed into its own file in the same backup folder (the Rust core's chunked archive, `RecordingArchive`), under the key of the same
// code or passphrase, and one sealed index file says which file holds which recording and how the key was made, which is all a restore
// needs. A recording never changes, so each is sealed once and the next backup writes only the new ones.
//
// What the backup folder shows: files named by random ids, no recording's name, no time (the Swift app's rule, UHDRBackupFormat); and
// the number of files and their sizes, which say roughly how many sessions and how long (OI-BACKUP-01 item 1, a decision still open).

/** How a backup of the recordings ended. */
sealed interface WaveformBackupResult {
    /** There are no recordings and never were a backup of any. Nothing is written. */
    data object NothingToBackUp : WaveformBackupResult

    /** [written] recordings were sealed this time; [total] are in the backup now. */
    class Done(val written: Int, val total: Int) : WaveformBackupResult

    /** The backup folder or the recordings could not be written or read; what was sealed before the stop is kept. */
    data object Failed : WaveformBackupResult
}

/** How a restore of the recordings ended. */
sealed interface WaveformRestoreResult {
    /** The backup has no recordings. */
    data object NoneInBackup : WaveformRestoreResult

    /** [restored] were put back, [alreadyHere] were left alone because the app already holds that recording, [unreadable] could not be restored. */
    class Done(val restored: Int, val alreadyHere: Int, val unreadable: Int) : WaveformRestoreResult

    /** The code or passphrase does not open the recordings' index. */
    data object WrongSecret : WaveformRestoreResult

    data object Failed : WaveformRestoreResult
}

class WaveformBackup(
    private val folder: UhdrFolder,
    private val random: RandomSource = SystemRandomSource,
) {
    private class Opened(val kdf: JsonObject, val key: BundleKey, val entries: MutableMap<String, String>)

    /**
     * Seal every recording the backup does not have yet into [place], under [secret]. A backup made under a different code or passphrase
     * from the last starts a new set, because the old files cannot be opened with the new secret; the old ones are removed. The secret is
     * the caller's to destroy.
     */
    suspend fun backUp(place: MigrationPlace, method: BundleKeyMethod, secret: BundleSecret): WaveformBackupResult = try {
        val sources = folder.store.list(SOURCE_PREFIX).filter { it.endsWith(SOURCE_SUFFIX) }
        val earlier = openIndex(place, secret)
        if (earlier == null && sources.isEmpty()) {
            WaveformBackupResult.NothingToBackUp
        } else {
            val set = earlier ?: newSet(method, secret)
            try {
                val held = place.store.list(ARCHIVE_PREFIX).toSet()
                var written = 0
                for (name in sources) {
                    if (set.entries[name]?.let { it in held } == true) continue
                    val data = folder.store.get(name) ?: continue
                    val archiveName = ARCHIVE_PREFIX + hex(random.nextBytes(ID_BYTES)) + ARCHIVE_SUFFIX
                    place.store.put(archiveName, RecordingArchive.seal(data, set.key, random))
                    set.entries[name] = archiveName
                    writeIndex(place, set)
                    written++
                }
                if (earlier == null && written == 0) writeIndex(place, set)
                removeOrphans(place, set)
                WaveformBackupResult.Done(written, set.entries.size)
            } finally {
                set.key.destroy()
            }
        }
    } catch (ex: CancellationException) {
        throw ex
    } catch (ex: Exception) {
        WaveformBackupResult.Failed
    }

    /** Put the recordings of the backup in [place] back in the app's UHDR folder, under [secret]. A recording the app already holds is left alone. */
    suspend fun restore(place: MigrationPlace, secret: BundleSecret): WaveformRestoreResult = try {
        val bytes = place.store.get(INDEX_NAME)
        if (bytes == null) {
            WaveformRestoreResult.NoneInBackup
        } else {
            val kdf = RecordingArchive.readKdf(bytes)
            if (kdf == null) {
                WaveformRestoreResult.Failed
            } else {
                val key = BundleKeyDerivation.deriveKey(secret, kdf)
                try {
                    when (val opened = RecordingArchive.open(bytes, key)) {
                        is ArchiveOpened.Refused ->
                            if (opened.problem == ArchiveProblem.AUTHENTICATION_FAILED) WaveformRestoreResult.WrongSecret else WaveformRestoreResult.Failed
                        is ArchiveOpened.Ok -> putBack(place, key, entriesOf(opened.data))
                    }
                } finally {
                    key.destroy()
                }
            }
        }
    } catch (ex: CancellationException) {
        throw ex
    } catch (ex: Exception) {
        WaveformRestoreResult.Failed
    }

    private suspend fun putBack(place: MigrationPlace, key: BundleKey, entries: Map<String, String>): WaveformRestoreResult {
        var restored = 0
        var here = 0
        var unreadable = 0
        for ((name, archiveName) in entries) {
            if (!isRecordingName(name) || !archiveName.startsWith(ARCHIVE_PREFIX)) { unreadable++; continue }
            if (folder.store.get(name) != null) { here++; continue }
            val archive = place.store.get(archiveName)
            val opened = archive?.let { RecordingArchive.open(it, key) }
            if (opened is ArchiveOpened.Ok) {
                folder.store.put(name, opened.data)
                folder.protect(name)
                restored++
            } else {
                unreadable++
            }
        }
        return WaveformRestoreResult.Done(restored, here, unreadable)
    }

    private suspend fun openIndex(place: MigrationPlace, secret: BundleSecret): Opened? {
        val bytes = place.store.get(INDEX_NAME) ?: return null
        val kdf = RecordingArchive.readKdf(bytes) ?: return null
        val key = runCatching { BundleKeyDerivation.deriveKey(secret, kdf) }.getOrNull() ?: return null
        val opened = RecordingArchive.open(bytes, key)
        if (opened !is ArchiveOpened.Ok) { key.destroy(); return null }
        return Opened(kdf, key, entriesOf(opened.data).toMutableMap())
    }

    private fun newSet(method: BundleKeyMethod, secret: BundleSecret): Opened {
        val kdf = BundleKeyDerivation.newKdf(method, random)
        return Opened(kdf, BundleKeyDerivation.deriveKey(secret, kdf), mutableMapOf())
    }

    private suspend fun writeIndex(place: MigrationPlace, set: Opened) {
        val index = buildJsonObject { put(ENTRIES, buildJsonObject { set.entries.forEach { (k, v) -> put(k, v) } }) }
        place.store.put(INDEX_NAME, RecordingArchive.seal(index.toString().encodeToByteArray(), set.key, random, set.kdf))
    }

    private suspend fun removeOrphans(place: MigrationPlace, set: Opened) {
        val keep = set.entries.values.toSet() + INDEX_NAME
        for (name in place.store.list(ARCHIVE_PREFIX)) if (name !in keep) place.store.delete(name)
    }

    private fun entriesOf(plain: ByteArray): Map<String, String> =
        Json.parseToJsonElement(plain.decodeToString()).jsonObject[ENTRIES]!!.jsonObject.mapValues { it.value.jsonPrimitive.content }

    private fun hex(bytes: ByteArray): String = bytes.joinToString("") { (it.toInt() and 0xff).toString(16).padStart(2, '0') }

    companion object {
        private const val SOURCE_PREFIX = "session_"
        private const val SOURCE_SUFFIX = ".edf"
        const val ARCHIVE_PREFIX = "wf-"
        private const val ARCHIVE_SUFFIX = ".npa"
        const val INDEX_NAME = "wf-index.npa"
        private const val ENTRIES = "entries"
        private const val ID_BYTES = 16

        /** A name the app would have given a recording: a hub session id between the prefix and the suffix. */
        fun isRecordingName(name: String): Boolean =
            name.startsWith(SOURCE_PREFIX) && name.endsWith(SOURCE_SUFFIX) &&
                name.removePrefix(SOURCE_PREFIX).removeSuffix(SOURCE_SUFFIX).let { id -> id.isNotEmpty() && id.all { it in '0'..'9' } }
    }
}
