package life.neurone.core.protocol

import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertFalse
import kotlin.test.assertNotNull
import kotlin.test.assertTrue

/**
 * Zone resolution and socket numbering: ports of the iOS `NPSocketMaskTests` cases that name a zone, the lattice and the numbering
 * base. The byte-level mask is the Rust core's (`hub-descriptor-golden.json` pins it against the web encoder in
 * `HubDescriptorCompilerTests`), so what is checked here is which sockets a target resolves to.
 */
class NPZoneResolutionTests {

    private fun named(vararg zones: String) = NPPBMTarget.Named(zones.toList()).resolveSockets()

    // @verifies REQ-NPPS-17
    @Test fun aNamedZoneResolvesToExactlyTheSocketsItsFileAuthors() {
        assertEquals(listOf(72, 73, 74, 77, 78), named("Occipital Left"))
    }

    // @verifies REQ-NPPS-17
    @Test fun theAllZoneCoversEveryFittedSocket() {
        assertEquals(SocketLattice.NP_SOCKET_COUNT, named("All").size)
        assertEquals((SocketLattice.NP_SOCKET_ID_MIN..SocketLattice.NP_SOCKET_ID_MAX).toList(), named("All"))
    }

    // @verifies REQ-NPPS-17
    @Test fun twoZonesSharingAMidlineSocketResolveItOnceNotTwice() {
        // Socket 74 (Oz) is in both occipital zones, so the union is not 5 + 5 and a module is never driven twice.
        val both = named("Occipital Left", "Occipital Right")
        assertEquals(listOf(72, 73, 74, 75, 76, 77, 78, 79, 80), both)
        assertEquals(both, (named("Occipital Left") + named("Occipital Right")).toSortedSet().toList())

        assertTrue("Frontal Left" in NPZoneRegistry.zones(2) && "Frontal Right" in NPZoneRegistry.zones(2))
        val frontal = named("Frontal Left", "Frontal Right")
        assertEquals(1, frontal.count { it == 2 })
        assertTrue(frontal.size < named("Frontal Left").size + named("Frontal Right").size, "shared midline sockets are not counted twice")
        assertEquals(37, frontal.size)
    }

    // @verifies REQ-NPPS-17
    @Test fun namingTheSameZoneTwiceChangesNothing() {
        assertEquals(named("Frontal"), named("Frontal", "Frontal"))
    }

    // @verifies REQ-NPPS-17
    @Test fun socketNumbersAreOneBasedAndTheLowestAndHighestBothAddress() {
        assertEquals(1, SocketLattice.NP_SOCKET_NUMBERING_BASE)
        assertEquals(1, SocketLattice.NP_SOCKET_ID_MIN)
        assertEquals(SocketLattice.NP_SOCKET_COUNT, SocketLattice.NP_SOCKET_ID_MAX)
        assertFalse(SocketLattice.isValid(0), "socket 0 does not exist: refused, never clamped to 1")
        assertTrue(SocketLattice.isValid(SocketLattice.NP_SOCKET_ID_MIN) && SocketLattice.isValid(SocketLattice.NP_SOCKET_ID_MAX))
        assertEquals("1–${SocketLattice.NP_SOCKET_COUNT}", SocketLattice.rangeLabel)
        for (bad in listOf(0, -1, SocketLattice.NP_SOCKET_COUNT + 1, 128)) assertFalse(SocketLattice.isValid(bad), "socket $bad")
    }

    // @verifies REQ-NPPS-17
    @Test fun everyAuthoredZoneUsesOneBasedSocketsListsNoSocketTwiceAndIsEmptyNowhere() {
        for (name in NPZoneRegistry.zoneNames) {
            val sockets = assertNotNull(NPZoneRegistry.sockets(name), "$name is listed but resolves to nothing")
            assertTrue(sockets.isNotEmpty(), "$name is empty")
            assertEquals(sockets.size, sockets.toSet().size, "$name lists a socket twice")
            sockets.forEach { assertTrue(SocketLattice.isValid(it), "zone $name names socket $it") }
            sockets.forEach { assertTrue(name in NPZoneRegistry.zones(it), "socket $it is in $name but the inverse lookup disagrees") }
        }
    }

    // @verifies REQ-NPPS-08#c
    @Test fun anUnknownZoneNameFailsAndNamesTheZone() {
        val e = assertFailsWith<NPPSError> { named("Left Frontal") }
        assertTrue("Left Frontal" in (e.message ?: ""), e.message)
    }

    // @verifies REQ-NPPS-17
    @Test fun anEmptyZoneListFails() {
        assertFailsWith<NPPSError> { NPPBMTarget.Named(emptyList()).resolveSockets() }
    }

    // @verifies REQ-FWHUB-31
    @Test fun clinicianSelectedNeedsAnOperatorSelectionAndSortsItWithoutRepeats() {
        assertFailsWith<NPPSError> { NPPBMTarget.ClinicianSelected.resolveSockets() }
        assertFailsWith<NPPSError> { NPPBMTarget.ClinicianSelected.resolveSockets(emptyList()) }
        assertEquals(listOf(4, 6, 21), NPPBMTarget.ClinicianSelected.resolveSockets(listOf(21, 4, 6, 4)))
    }

    // @verifies REQ-NPPS-13
    @Test fun zonesComeFromTheLoadedNppsFilesAndFromNowhereElse() {
        val fromFiles = NPBundledProtocols.namespace.zones.keys
        assertTrue(fromFiles.isNotEmpty(), "the bundled .npps library defines no zones")
        assertTrue(fromFiles.all { it in NPZoneRegistry.zoneNames }, "every bundled zone is registered")
    }
}
