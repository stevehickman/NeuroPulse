package life.neurone.core.migration

import java.io.File
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

/** Every stored key is sorted: a new key cannot move by accident or be forgotten (OI-UI-KMP-11 (d), SWR-MG-01). */
class MigrationKeyMapTests {
    private val keyLiteral = Regex("\"((?:np|ble)\\.[A-Za-z0-9_.\\-]+)\"")

    private fun sourceDirs(): List<File> {
        // Gradle runs :core's tests from app/android/core.
        val roots = listOf(
            "src/commonMain",
            "../../NeurOneUI/shared/src/commonMain",
            "../../NeurOneUI/desktop/src/main",
            "../app/src/main",
        ).map { File(it) }
        assertTrue(roots.first().isDirectory, "run from app/android/core: ${File(".").absolutePath}")
        return roots.filter { it.isDirectory }
    }

    // @verifies SWR-MG-01
    @Test
    fun everyStoredKeyIsCarriedPerInstallConsentOrUnsorted() {
        val found = sourceDirs().flatMap { d -> d.walkTopDown().filter { it.isFile && it.extension == "kt" }.toList() }
            .flatMap { f -> keyLiteral.findAll(f.readText()).map { it.groupValues[1] }.toList() }
            .toSet()
        assertTrue(found.size > 20, "the scan found ${found.size} keys; the source directories moved")
        val unsorted = found - MigrationKeyMap.all
        assertEquals(emptySet(), unsorted, "a key the app writes is in no list of MigrationKeyMap: decide whether it moves")
    }

    // @verifies SWR-MG-01
    @Test
    fun noKeyIsInTwoLists() {
        val lists = MigrationKeyMap.carried.values.flatten() + MigrationKeyMap.consent + MigrationKeyMap.perInstall + MigrationKeyMap.unsorted
        assertEquals(lists.size, lists.toSet().size, "a key in two lists would move and not move at once")
    }
}
