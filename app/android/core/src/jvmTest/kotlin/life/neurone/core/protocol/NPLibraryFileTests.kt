package life.neurone.core.protocol

import life.neurone.core.common.InMemoryKeyValueStore
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertNotNull
import kotlin.test.assertNull
import kotlin.test.assertTrue

/** The user's whole library as one `.npps` file the person keeps, and opens again (the web's save and open, for every platform). */
class NPLibraryFileTests {

    private fun library() = NPProtocolLibrary(InMemoryKeyValueStore())

    private fun protocol(name: String) = NPProtocolEntry.Single(
        NPProtocolDefinition(
            name = name,
            timingMode = NPTimingMode.Duration(600),
            modalities = listOf(
                NPProtocolModality(
                    params = NPModalityParams.Tdcs(NPTDCSParams(intensityMilliamps = 1.0)),
                    interval = NPIntervalConfig.CONTINUOUS,
                ),
            ),
        ),
    )

    // @verifies REQ-NPPS-18
    @Test
    fun noProtocolsOfYourOwn_hasNothingToSave() {
        assertNull(library().exportAllScript())
    }

    // @verifies REQ-NPPS-18
    @Test
    fun theLibraryOpensIntoAnotherOne_withItsZones() {
        val a = library()
        a.save(protocol("Mine"))
        a.addUserZone("Spot", listOf(4, 6))
        val text = assertNotNull(a.exportAllScript())

        val b = library()
        val result = b.openScript(text)
        assertEquals(NPProtocolLibrary.OpenResult(opened = 2, refused = 0, unreadable = false), result)
        assertTrue(b.userProtocols.any { it is NPProtocolEntry.Single && it.protocol.name == "Mine" })
        assertEquals(listOf(4, 6), b.userZones.single().sockets)
    }

    // @verifies REQ-NPPS-18
    @Test
    fun aUsedFamilyStaysUsed_whenTheFileIsOpenedElsewhere() {
        val a = library()
        val entry = protocol("Used one")
        a.save(entry)
        a.recordUploadResult(entry, sentHash = "h", helmetStatus = "accepted", helmetHash = "h")
        assertTrue(a.isUsed(entry))
        val text = assertNotNull(a.exportAllScript())
        assertTrue(text.contains("is_used") || text.contains("used"), text)

        val b = library()
        b.openScript(text)
        val opened = b.userProtocols.single { it is NPProtocolEntry.Single } 
        assertTrue(b.isUsed(opened), "the used flag travels with the protocol")
    }

    // @verifies REQ-NPPS-18
    @Test
    fun whatTheRulesRefuse_isCounted_andNeverOverwritten() {
        val a = library()
        a.save(protocol("Mine"))
        a.addUserZone("Frontal Left2", listOf(1))
        val text = assertNotNull(a.exportAllScript())

        // Opening the same file into the library it came from: the protocol is saved over itself (same id), the zone name is taken.
        val result = a.openScript(text)
        assertEquals(1, result.refused)
        assertEquals(1, result.opened)
        assertEquals(1, a.userZones.size)
    }

    // @verifies REQ-NPPS-18
    @Test
    fun aFileThatDoesNotParse_changesNothing() {
        val lib = library()
        lib.save(protocol("Mine"))
        val before = lib.userProtocols.size
        val result = lib.openScript("this is not a protocol file {")
        assertTrue(result.unreadable)
        assertFalse(result.opened > 0)
        assertEquals(before, lib.userProtocols.size)
    }
}
