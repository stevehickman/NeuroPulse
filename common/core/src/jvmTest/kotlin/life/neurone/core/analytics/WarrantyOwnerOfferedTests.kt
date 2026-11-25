package life.neurone.core.analytics

import life.neurone.core.common.InMemoryKeyValueStore
import life.neurone.core.protocol.NPProtocolEntry
import life.neurone.core.protocol.NPProtocolLibrary
import life.neurone.core.protocol.familyKey
import life.neurone.core.protocol.id
import life.neurone.core.protocol.name
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertNull
import kotlin.test.assertTrue

/**
 * The warranty owner's offered set (`SWR-WO-01`): kept in the owner's store, per helmet, and apart from the person's own archive,
 * which travels with the person's protocols (`REQ-SHDR-10` (d); `OI-PRIV-60`, NP-PRIV-ANALYSIS-006 F-04). Every refusal is
 * asserted beside a control that succeeds.
 */
class WarrantyOwnerOfferedTests {

    private val helmetA = "AA:BB:CC:00:00:01"
    private val helmetB = "AA:BB:CC:00:00:02"

    /** The keys the protocol library writes for the person, all of them carried in a user's migration and backup. */
    private val personKeys = listOf(
        NPProtocolLibrary.USER_PROTOCOLS_KEY, NPProtocolLibrary.USED_FAMILIES_KEY,
        NPProtocolLibrary.ARCHIVED_FAMILIES_KEY, NPProtocolLibrary.CLONE_SOURCES_KEY,
    )

    private class Rig {
        val kv = InMemoryKeyValueStore()
        val owner = WarrantyOwnerStore(kv)
        var connected: String? = null
        val library: NPProtocolLibrary

        init {
            kv.putBoolean(NPProtocolLibrary.BIPA_ACCEPTED_KEY, true)
            library = NPProtocolLibrary(kv, ownerWithdrawn = { owner.withdrawnFamilies(connected) })
        }

        fun gamma(): NPProtocolEntry = library.bundledProtocols.first { it.name == "Gamma Focus" }
        fun offered(entry: NPProtocolEntry): Boolean = library.offeredProtocols.any { it.id == entry.id }
        fun snapshot(keys: List<String>): Map<String, String?> = keys.associateWith { kv.getString(it) }
    }

    // @verifies SWR-WO-01
    // @verifies REQ-SHDR-10#d
    @Test
    fun theOwnersWithdrawalIsKeptPerHelmetInTheOwnersRecordAndInNoKeyOfThePersons() {
        val rig = Rig()
        val gamma = rig.gamma()
        rig.connected = helmetA
        assertTrue(rig.library.isOffered(gamma) && rig.offered(gamma), "control: offered before the owner withdraws it")
        val before = rig.snapshot(personKeys)

        rig.owner.setOffered(helmetA, gamma.familyKey(), offered = false)

        assertFalse(rig.library.isOffered(gamma))
        assertFalse(rig.offered(gamma), "withdrawn: out of the person's offered list on that helmet")
        assertTrue(rig.library.allProtocols.any { it.id == gamma.id }, "and still in the library")
        assertEquals(before, rig.snapshot(personKeys), "no key of the person's was written")
        assertFalse(rig.library.isArchived(gamma), "the person's archive says nothing about the owner's choice")
        assertEquals(setOf(gamma.familyKey()), WarrantyOwnerStore(rig.kv).withdrawnFamilies(helmetA), "kept across a restart")
        assertTrue(WarrantyOwnerStore.RECORDS_KEY !in personKeys)

        rig.connected = helmetB
        assertTrue(rig.offered(gamma), "a withdrawal on one helmet withdraws nothing on another")
        rig.connected = null
        assertTrue(rig.offered(gamma), "with no helmet there is no owner's set to apply")

        rig.connected = helmetA
        rig.owner.setOffered(helmetA, gamma.familyKey(), offered = true)
        assertTrue(rig.offered(gamma), "offered again when the owner switches it back on")
    }

    // @verifies SWR-WO-01
    // @verifies REQ-SHDR-10#d
    @Test
    fun neitherThePersonsArchiveNorTheOwnersChoiceChangesTheOther() {
        val rig = Rig()
        val gamma = rig.gamma()
        rig.connected = helmetA

        // The person archives and restores: the owner's record is not touched.
        val ownerBefore = rig.kv.getString(WarrantyOwnerStore.RECORDS_KEY)
        rig.library.archiveFamily(gamma)
        assertTrue(rig.library.isArchived(gamma))
        assertFalse(rig.offered(gamma), "control: the person's archive takes it out of their list")
        assertEquals(ownerBefore, rig.kv.getString(WarrantyOwnerStore.RECORDS_KEY))
        assertTrue(rig.library.isOffered(gamma), "archived by the person, still offered by the owner")
        rig.library.unarchiveFamily(gamma)

        // The owner withdraws; the person's Restore does not offer it again.
        rig.owner.setOffered(helmetA, gamma.familyKey(), offered = false)
        val archiveBefore = rig.kv.getString(NPProtocolLibrary.ARCHIVED_FAMILIES_KEY)
        rig.library.archiveFamily(gamma)
        rig.library.unarchiveFamily(gamma)
        assertFalse(rig.offered(gamma), "the person's Restore does not undo the owner's withdrawal")
        assertEquals(setOf(gamma.familyKey()), rig.owner.withdrawnFamilies(helmetA))
        assertEquals(archiveBefore, rig.kv.getString(NPProtocolLibrary.ARCHIVED_FAMILIES_KEY))
    }

    // A family id is the protocol's own text; whatever it holds, it does not break the record or the table around it.
    // @verifies SWR-WO-01
    @Test
    fun anyFamilyIdRoundTripsAndLeavesTheOwnersOtherDecisionsAlone() {
        val kv = InMemoryKeyValueStore()
        val owner = WarrantyOwnerStore(kv)
        WarrantyAnalyticsGate(owner).grant(helmetA)
        owner.update(helmetA) { it.copy(stealthForbidden = true) }
        val odd = setOf("a,b", "tab\there", "line\nbreak", "dot.ted", "ünï", "plain")
        odd.forEach { owner.setOffered(helmetA, it, offered = false) }

        val reread = WarrantyOwnerStore(kv).record(helmetA)!!
        assertEquals(odd, reread.withdrawnFamilies)
        assertTrue(reread.fleetConsent && reread.stealthForbidden, "the owner's other decisions are unchanged")
        assertEquals(2, kv.getString(WarrantyOwnerStore.RECORDS_KEY)!!.lines().size, "still a salt and one line")
        odd.forEach { assertFalse(it in kv.getString(WarrantyOwnerStore.RECORDS_KEY)!!, "an id is not stored as its text") }
    }

    // A change of custody ends the old owner's decisions, the offered set with them.
    // @verifies SWR-WO-01
    @Test
    fun forgettingAHelmetForgetsItsOfferedSet() {
        val owner = WarrantyOwnerStore(InMemoryKeyValueStore())
        owner.setOffered(helmetA, "family-1", offered = false)
        owner.setOffered(helmetB, "family-1", offered = false)
        owner.forget(helmetA)
        assertNull(owner.record(helmetA))
        assertTrue(owner.withdrawnFamilies(helmetA).isEmpty())
        assertEquals(setOf("family-1"), owner.withdrawnFamilies(helmetB), "control: the other helmet's set stays")
        assertTrue(owner.withdrawnFamilies(null).isEmpty(), "no helmet named, nothing withdrawn")
    }
}
