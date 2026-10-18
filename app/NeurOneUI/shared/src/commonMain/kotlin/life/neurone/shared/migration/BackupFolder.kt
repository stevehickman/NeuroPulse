package life.neurone.shared.migration

import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import life.neurone.core.common.KeyValueStore
import life.neurone.shared.MigrationPlace

// The folder a backup is kept in, chosen by the person (SWR-MG-10, a local computer; issue #569 package 6zz). The desktop apps kept
// the backup in one folder beside the app's data, which a person who wants it on a USB drive or a shared folder could not change.
// What is common to every host that has folders is here: remembering the choice, refusing a folder that cannot be used, checking
// the folder takes a file before a backup depends on it, and saying why not. Only two things are the host's: the dialog that asks
// (Swing on the desktop) and the object store a folder becomes.

/** The key that holds the chosen folder, per install (`np.backup.folder` in the core's key map). Absent means the default. */
const val BACKUP_FOLDER_KEY = "np.backup.folder"

/** Why a folder is not used. The screen words each with a locale key; the model decides nothing about paths. */
enum class FolderProblem {
    /** The folder is the app's own data folder, or holds it: a backup there would be cleared with the data it protects. */
    HoldsAppData,

    /** The folder could not be written to, or what was written could not be read back. */
    NotWritable,
}

/**
 * What a host that keeps backups in folders supplies (the seam in [life.neurone.shared.PlatformServices]). A host with a fixed
 * place and no choice supplies none, and neither do the phones, whose folders belong to the system's own dialogs.
 */
interface FolderPlaces {
    /** The folder used until one is chosen. */
    val defaultFolder: String

    /** Ask the person for a folder, starting at [current]. Null when they cancel. */
    suspend fun choose(current: String): String?

    /** The place a backup in [folder] goes to: its object store and what that store can promise. */
    fun placeAt(folder: String): MigrationPlace

    /** Whether [folder] must not be used, for a reason that is about the host's own layout. Null if it may be. */
    fun refusal(folder: String): FolderProblem?
}

/**
 * The chosen folder and its place, with no Compose in it. [place] is the place the backup, restore and clear screens use; it
 * changes only when a folder passes both checks, so a screen never holds a place that was refused. [problem] is why the last
 * try failed, and is cleared by the next.
 */
class BackupFolderModel(private val places: FolderPlaces, private val keys: KeyValueStore) {
    // A folder chosen in an earlier run may be gone (a drive that is not plugged in). The app then starts on the default folder and
    // says so; the choice is kept, so the drive being back is enough.
    private val saved: String? = keys.getString(BACKUP_FOLDER_KEY)?.takeIf { it.isNotBlank() }
    private val start: Pair<String, MigrationPlace?> = saved?.let { f -> runCatching { places.placeAt(f) }.getOrNull()?.let { f to it } }
        ?: (places.defaultFolder to null)
    private val chosen = MutableStateFlow(start.first)
    private val current = MutableStateFlow(start.second ?: places.placeAt(places.defaultFolder))
    private val trouble = MutableStateFlow<FolderProblem?>(if (saved != null && start.second == null) FolderProblem.NotWritable else null)

    val folder: StateFlow<String> = chosen.asStateFlow()
    val place: StateFlow<MigrationPlace> = current.asStateFlow()
    val problem: StateFlow<FolderProblem?> = trouble.asStateFlow()

    /** The folder used when none is chosen; the screen offers a way back to it only while another is in use. */
    val defaultFolder: String get() = places.defaultFolder

    private fun open(folder: String): MigrationPlace = places.placeAt(folder)

    /** Ask the person for a folder and use it if it passes. A cancel changes nothing. */
    suspend fun choose() {
        val picked = places.choose(chosen.value) ?: return
        use(picked)
    }

    /** Go back to the default folder. */
    suspend fun useDefault() = use(places.defaultFolder)

    private suspend fun use(folder: String) {
        trouble.value = null
        places.refusal(folder)?.let { trouble.value = it; return }
        val candidate = try { open(folder) } catch (ex: Exception) {
            if (ex is kotlin.coroutines.cancellation.CancellationException) throw ex
            trouble.value = FolderProblem.NotWritable
            return
        }
        if (!takesAFile(candidate)) { trouble.value = FolderProblem.NotWritable; return }
        if (folder == places.defaultFolder) keys.remove(BACKUP_FOLDER_KEY) else keys.putString(BACKUP_FOLDER_KEY, folder)
        chosen.value = folder
        current.value = candidate
    }

    /** Write, read back and remove a throwaway object, so a folder that is read-only or gone is found now and not mid-backup. */
    private suspend fun takesAFile(place: MigrationPlace): Boolean = try {
        val bytes = byteArrayOf(1, 2, 3)
        place.store.put(CHECK_OBJECT, bytes)
        val back = place.store.get(CHECK_OBJECT)
        place.store.delete(CHECK_OBJECT)
        back != null && back.contentEquals(bytes)
    } catch (ex: Exception) {
        if (ex is kotlin.coroutines.cancellation.CancellationException) throw ex
        false
    }

    private companion object {
        /** One safe object name; deleted straight away, and the core's own objects carry other names. */
        const val CHECK_OBJECT = "np-folder-check"
    }
}
