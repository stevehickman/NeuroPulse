package life.neurone.core.analytics

import java.io.File

/**
 * The hub's SHDR records as files in the app's own [folder], one per helmet (`shdr_staging-<key>.bin`, `REQ-SHDR-08` (e)), for the
 * hosts that run on the JVM. The key is the warranty owner's store's salted hash for the helmet, never its identifier, so a file
 * name says nothing about which helmet it is to anyone without the sealed salt. Nothing staged reads as null.
 *
 * A record left by an earlier version as the one `shdr_staging.bin` names no helmet, so it is deleted unread when this is built:
 * sent, it could have left under whichever helmet's consent happened to be connected.
 */
class FileShdrStaging(private val folder: File) : ShdrStaging {
    init {
        File(folder, LEGACY_FILE_NAME).delete()
    }

    private fun fileFor(helmetKey: String): File {
        require(helmetKey.matches(ShdrStaging.HELMET_KEY)) { "not a helmet key" }
        return File(folder, fileName(helmetKey))
    }

    override fun read(helmetKey: String): ByteArray? = fileFor(helmetKey).let { if (it.isFile) it.readBytes() else null }

    override fun stage(helmetKey: String, record: ByteArray) {
        val target = fileFor(helmetKey)
        folder.mkdirs()
        // Written beside and moved into place, so a reader never sees half a record.
        val partial = File(folder, target.name + ".part")
        partial.writeBytes(record)
        if (!partial.renameTo(target)) {
            target.delete()
            check(partial.renameTo(target)) { "the staged record could not be moved into place" }
        }
    }

    override fun clear(helmetKey: String) {
        fileFor(helmetKey).delete()
    }

    companion object {
        /** The one unbound file an earlier version read. Never read now; deleted. */
        const val LEGACY_FILE_NAME = "shdr_staging.bin"

        /** The file a helmet's record is staged in. */
        fun fileName(helmetKey: String): String = "shdr_staging-$helmetKey.bin"
    }
}
