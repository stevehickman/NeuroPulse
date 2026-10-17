package life.neurone.desktop

import kotlinx.coroutines.runBlocking
import java.nio.file.Files
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith

class DesktopOwnFilesTest {

    // @verifies SWR-MG-06
    @Test
    fun theClearTakesTheKeyValueFileAndLeavesTheBackupPlaceInTheSameFolder() = runBlocking<Unit> {
        val data = Files.createTempDirectory("np-desktop-data").toFile()
        data.resolve("np-app.properties").writeBytes(ByteArray(64) { 1 })
        data.resolve("np-app.properties.tmp").writeBytes(ByteArray(8) { 2 })
        data.resolve(BACKUP_FOLDER).mkdirs()
        data.resolve(BACKUP_FOLDER).resolve("bundle").writeBytes(ByteArray(100) { 3 })

        val files = desktopOwnFiles(data)
        assertEquals(listOf("data/np-app.properties", "data/np-app.properties.tmp"), files.list().map { it.path })
        files.delete("data/np-app.properties")
        assertEquals(false, data.resolve("np-app.properties").exists())
        assertFailsWith<IllegalArgumentException> { files.delete("data/$BACKUP_FOLDER/bundle") }
        assertEquals(100, data.resolve(BACKUP_FOLDER).resolve("bundle").length())
    }
}
