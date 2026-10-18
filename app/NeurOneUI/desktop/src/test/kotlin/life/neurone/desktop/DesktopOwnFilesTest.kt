package life.neurone.desktop

import kotlinx.coroutines.runBlocking
import life.neurone.shared.migration.FolderProblem
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

        val files = desktopOwnFiles(data, data.resolve(BACKUP_FOLDER).path)
        assertEquals(listOf("data/np-app.properties", "data/np-app.properties.tmp"), files.list().map { it.path })
        files.delete("data/np-app.properties")
        assertEquals(false, data.resolve("np-app.properties").exists())
        assertFailsWith<IllegalArgumentException> { files.delete("data/$BACKUP_FOLDER/bundle") }
        assertEquals(100, data.resolve(BACKUP_FOLDER).resolve("bundle").length())
    }
}

class DesktopFolderPlacesTest {

    private fun temp() = Files.createTempDirectory("np-folder").toFile()

    // @verifies SWR-MG-10
    @Test
    fun theDataFolderAndAnyFolderAboveItAreRefusedAndEveryOtherFolderIsNot() {
        val root = temp()
        val data = root.resolve("app-data").also { it.mkdirs() }
        val places = DesktopFolderPlaces(data) { null }
        assertEquals(FolderProblem.HoldsAppData, places.refusal(data.path), "the data folder itself")
        assertEquals(FolderProblem.HoldsAppData, places.refusal(root.path), "a folder above it")
        assertEquals(FolderProblem.HoldsAppData, places.refusal(data.resolve("..").path), "spelled with ..")
        assertEquals(null, places.refusal(data.resolve("backups").path), "a folder inside it is allowed (it is kept by the clear)")
        assertEquals(null, places.refusal(temp().path), "a folder elsewhere")
    }

    // @verifies SWR-MG-10
    @Test
    fun aFolderThatDoesNotExistIsNotMadeExceptTheDefault() {
        val data = temp()
        val places = DesktopFolderPlaces(data) { null }
        assertFailsWith<IllegalArgumentException> { places.placeAt(data.parentFile.resolve("unplugged-drive").path) }
        assertEquals(false, data.parentFile.resolve("unplugged-drive").exists(), "nothing was made")
        places.placeAt(places.defaultFolder)
        assertEquals(true, data.resolve(BACKUP_FOLDER).isDirectory, "the default is made on first use")
    }

    // @verifies SWR-MG-10
    @Test
    fun theDialogsAnswerIsWhatChooseReturns() = runBlocking<Unit> {
        val data = temp()
        val elsewhere = temp()
        assertEquals(elsewhere.path, DesktopFolderPlaces(data) { elsewhere }.choose(data.path))
        assertEquals(null, DesktopFolderPlaces(data) { null }.choose(data.path), "a cancel")
    }

    // @verifies SWR-MG-06
    @Test
    fun theClearKeepsTheChosenFolderOnlyWhenItIsInsideTheDataFolder() = runBlocking<Unit> {
        val data = temp()
        val inside = data.resolve("my backups/week1").also { it.mkdirs() }.also { it.resolve("bundle").writeBytes(ByteArray(5)) }
        data.resolve("np-app.properties").writeBytes(ByteArray(5))
        assertEquals(listOf("data/np-app.properties"), desktopOwnFiles(data, inside.path).list().map { it.path })
        assertEquals(setOf("data/my backups/week1/bundle", "data/np-app.properties"), desktopOwnFiles(data, temp().path).list().map { it.path }.toSet(),
            "a folder outside the data folder keeps nothing here, so everything in the data folder is listed")
    }
}
