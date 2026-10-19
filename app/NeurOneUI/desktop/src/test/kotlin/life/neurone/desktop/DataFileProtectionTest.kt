package life.neurone.desktop

import java.io.File
import java.nio.file.Files
import java.nio.file.attribute.PosixFilePermissions
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertTrue

class DataFileProtectionTest {
    private val posix = java.nio.file.FileSystems.getDefault().supportedFileAttributeViews().contains("posix")

    private fun perms(f: File) = PosixFilePermissions.toString(Files.getPosixFilePermissions(f.toPath()))

    // @verifies SWR-MG-18#a
    @Test
    fun theDataFileAndItsFolderAreOwnerOnlyAndTheFileIsNeverInTheOpenWhileItIsWritten() {
        if (!posix) return
        val dir = Files.createTempDirectory("np-prot").toFile().also { Files.setPosixFilePermissions(it.toPath(), PosixFilePermissions.fromString("rwxr-xr-x")) }
        val file = dir.resolve("np-app.properties")
        val store = FileKeyValueStore(file, OwnerOnlyProtection())
        store.putString("a", "1")
        assertEquals("rw-------", perms(file))
        assertEquals("rwx------", perms(dir))
        assertEquals("1", FileKeyValueStore(file, OwnerOnlyProtection()).getString("a"), "and it still reads back")

        // A file an earlier release wrote with the default permissions is brought under the protection when the store opens.
        Files.setPosixFilePermissions(file.toPath(), PosixFilePermissions.fromString("rw-r--r--"))
        FileKeyValueStore(file, OwnerOnlyProtection())
        assertEquals("rw-------", perms(file))
    }

    // @verifies SWR-MG-18#a
    @Test
    fun aNewFileIsCreatedWithOwnerOnlyPermissionsBeforeAnythingIsWrittenToIt() {
        if (!posix) return
        val dir = Files.createTempDirectory("np-prot").toFile()
        val temporary = dir.resolve("np-app.properties.tmp")
        OwnerOnlyProtection().prepare(temporary)
        assertTrue(temporary.exists() && temporary.length() == 0L, "created empty")
        assertEquals("rw-------", perms(temporary))
    }

    // @verifies SWR-MG-18#b
    @Test
    fun onMacOsTheFileIsExcludedFromBackupsEachTimeItIsReplacedAndTheFolderIsNot() {
        if (!posix) return
        val dir = Files.createTempDirectory("np-prot").toFile()
        val excluded = mutableListOf<String>()
        val file = dir.resolve("np-app.properties")
        val store = FileKeyValueStore(file, OwnerOnlyProtection(excludeFromBackup = { excluded += it.name }))
        store.putString("a", "1")
        store.putString("b", "2")
        assertEquals(listOf("np-app.properties.tmp", "np-app.properties.tmp"), excluded, "the temporary file, which the rename carries over, each save")
        assertFalse(excluded.any { it == dir.name }, "the folder holding the person's backups is not excluded")
    }

    // @verifies SWR-MG-18#a
    @Test
    fun aFailureToProtectNeverStopsTheStoreFromSaving() {
        val dir = Files.createTempDirectory("np-prot").toFile()
        val file = dir.resolve("np-app.properties")
        val guarded = OwnerOnlyProtection(excludeFromBackup = { throw java.io.IOException("no tool") }, posix = false)
        FileKeyValueStore(file, guarded).putString("a", "1")
        assertEquals("1", FileKeyValueStore(file, guarded).getString("a"))
    }
}
