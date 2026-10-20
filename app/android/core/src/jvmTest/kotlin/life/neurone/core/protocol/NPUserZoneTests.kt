package life.neurone.core.protocol

import life.neurone.core.common.InMemoryKeyValueStore
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertIs
import kotlin.test.assertNull
import kotlin.test.assertTrue

/** A zone is defined as a name and a socket set, and it becomes real only by being saved with the user's scripts (NP-NPPS-REF-001 §8). */
class NPUserZoneTests {

    private fun library(kv: InMemoryKeyValueStore = InMemoryKeyValueStore()) = NPProtocolLibrary(kv)

    // @verifies REQ-NPPS-12
    @Test
    fun socketText_isReadAsASet_withTheBadTokensReportedAsWritten() {
        val r = NPSocketSet.parse("6, 4 4 21, 99, 0")
        assertEquals(listOf(4, 6, 21), r.sockets)
        assertEquals(listOf("99", "0"), r.invalid)
        assertEquals(listOf(1, 2, 3), NPSocketSet.union(listOf(3, 1), listOf(2, 1)))
        assertEquals(emptyList(), NPSocketSet.parse("").sockets)
    }

    // @verifies REQ-NPPS-12
    @Test
    fun aSavedZoneResolvesLikeAShippedOne_andSurvivesARestart() {
        val kv = InMemoryKeyValueStore()
        val lib = library(kv)
        assertNull(NPZoneRegistry.sockets("My Spot"))
        assertEquals(NPProtocolLibrary.ZoneSaveResult.Saved, lib.addUserZone("  My Spot ", listOf(21, 4, 4, 6)))
        assertEquals(listOf(4, 6, 21), NPZoneRegistry.sockets("My Spot"))
        assertEquals(listOf("My Spot"), lib.userZones.map { it.name })

        val again = library(kv)
        assertEquals(listOf(4, 6, 21), NPZoneRegistry.sockets("My Spot"))
        assertEquals(listOf("My Spot"), again.userZones.map { it.name })
    }

    // @verifies REQ-NPPS-12
    @Test
    fun aZoneThatIsRefused_isNeverSaved() {
        val lib = library()
        assertEquals(NPProtocolLibrary.ZoneSaveResult.NameEmpty, lib.addUserZone("  ", listOf(1)))
        assertEquals(NPProtocolLibrary.ZoneSaveResult.NoSockets, lib.addUserZone("Empty", emptyList()))
        assertEquals(NPProtocolLibrary.ZoneSaveResult.InvalidSockets(listOf(99)), lib.addUserZone("Far", listOf(1, 99)))
        // A shipped name is taken, and so is a name the user already defined.
        assertEquals(NPProtocolLibrary.ZoneSaveResult.NameTaken, lib.addUserZone("Frontal Left", listOf(1)))
        assertEquals(NPProtocolLibrary.ZoneSaveResult.Saved, lib.addUserZone("Twice", listOf(1)))
        assertEquals(NPProtocolLibrary.ZoneSaveResult.NameTaken, lib.addUserZone("Twice", listOf(2)))
        assertEquals(listOf("Twice"), lib.userZones.map { it.name })
    }

    // @verifies REQ-NPPS-12
    @Test
    fun aZoneAProtocolNames_cannotBeDeleted_andOneNobodyNamesCan() {
        val lib = library()
        lib.addUserZone("Used", listOf(1, 2))
        lib.addUserZone("Free", listOf(3))
        val protocol = NPProtocolDefinition(
            name = "Uses it",
            timingMode = NPTimingMode.Duration(600),
            modalities = listOf(
                NPProtocolModality(
                    params = NPModalityParams.PbmTranscranial(NPPBMTranscranialParams(target = NPPBMTarget.Named(listOf("Used")))),
                    interval = NPIntervalConfig.CONTINUOUS,
                ),
            ),
        )
        lib.save(NPProtocolEntry.Single(protocol))
        assertEquals(listOf("Uses it"), lib.protocolsUsingZone("Used"))
        assertIs<NPProtocolLibrary.ZoneDeleteResult.InUse>(lib.deleteUserZone("Used"))
        assertEquals(NPProtocolLibrary.ZoneDeleteResult.Deleted, lib.deleteUserZone("Free"))
        assertNull(NPZoneRegistry.sockets("Free"))
        assertEquals(NPProtocolLibrary.ZoneDeleteResult.NotFound, lib.deleteUserZone("Frontal Left"))
    }

    // @verifies REQ-NPPS-12
    @Test
    fun theMapColoursASocketByItsSmallestZone() {
        val g = NPZoneGrouping(
            listOf(
                NPZoneDefinition("All", (1..10).toList()),
                NPZoneDefinition("Front", listOf(1, 2, 3)),
                NPZoneDefinition("Tip", listOf(1)),
            ),
        )
        assertEquals("Tip", g.primaryOf[1])
        assertEquals("Front", g.primaryOf[2])
        assertEquals("All", g.primaryOf[10])
        assertEquals(listOf("Tip", "Front", "All"), g.membershipOf[1])
        assertEquals(listOf("All", "Front", "Tip"), g.legend)
        assertTrue(SocketLattice.cells.size == SocketLattice.NP_SOCKET_COUNT)
    }
}
