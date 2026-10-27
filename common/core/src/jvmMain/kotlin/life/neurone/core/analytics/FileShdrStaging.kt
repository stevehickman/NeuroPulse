package life.neurone.core.analytics

import java.io.File

/**
 * The hub's SHDR record as a file in the app's own folder (`shdr_staging.bin`), for the hosts that run on the JVM: the file the hub
 * drops over its USB-C CDC interface, the same convention as the iOS app's Documents file. Nothing staged reads as null.
 */
class FileShdrStaging(private val file: File) : ShdrStaging {
    override fun read(): ByteArray? = if (file.isFile) file.readBytes() else null

    override fun clear() {
        file.delete()
    }

    companion object {
        const val FILE_NAME = "shdr_staging.bin"
    }
}
