package life.neurone.desktop

import java.nio.file.Files
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNull
import kotlin.test.assertTrue

class DataDirectoryTest {

    private fun temp() = Files.createTempDirectory("np-datadir").toFile()

    // @verifies SWR-MG-14#a
    @Test
    fun windowsKeepsTheDataInTheMachineLocalFolderAndTheOthersAreUnchanged() {
        val env = mapOf("APPDATA" to "C:/Users/a/AppData/Roaming", "LOCALAPPDATA" to "C:/Users/a/AppData/Local")::get
        assertEquals("C:/Users/a/AppData/Local/NeurOne", appDataDirectory("windows 11", "C:/Users/a", env).path)
        assertEquals("C:/Users/a/AppData/Roaming/NeurOne", legacyAppDataDirectory("windows 11", "C:/Users/a", env)?.path)
        assertEquals("/h/Library/Application Support/NeurOne", appDataDirectory("mac os x", "/h") { null }.path)
        assertNull(legacyAppDataDirectory("mac os x", "/h") { null })
        assertNull(legacyAppDataDirectory("linux", "/h") { null })
    }

    // @verifies SWR-MG-14#b
    @Test
    fun theRoamingFolderMovesToTheLocalOneOnceAndLeavesNothingBehind() {
        val root = temp()
        val legacy = root.resolve("Roaming/NeurOne").also { it.mkdirs() }
        legacy.resolve(KEY_VALUE_FILE).writeText("a=1")
        legacy.resolve("backups").mkdirs()
        legacy.resolve("backups/bundle").writeText("b")
        val current = root.resolve("Local/NeurOne")
        moveLegacyDataDirectory(legacy, current)
        assertEquals("a=1", current.resolve(KEY_VALUE_FILE).readText())
        assertEquals("b", current.resolve("backups/bundle").readText())
        assertTrue(!legacy.exists(), "no copy is left to roam")
        moveLegacyDataDirectory(legacy, current)
        assertEquals("a=1", current.resolve(KEY_VALUE_FILE).readText(), "a second start changes nothing")
    }

    // @verifies SWR-MG-14#b
    @Test
    fun stateAlreadyMadeInTheLocalFolderIsNeverOverwrittenByTheRoamingOne() {
        val root = temp()
        val legacy = root.resolve("Roaming/NeurOne").also { it.mkdirs() }
        legacy.resolve(KEY_VALUE_FILE).writeText("old")
        val current = root.resolve("Local/NeurOne").also { it.mkdirs() }
        current.resolve(KEY_VALUE_FILE).writeText("new")
        moveLegacyDataDirectory(legacy, current)
        assertEquals("new", current.resolve(KEY_VALUE_FILE).readText())
        assertEquals("old", legacy.resolve(KEY_VALUE_FILE).readText(), "the unread copy is left for the person, not deleted")
    }
}
