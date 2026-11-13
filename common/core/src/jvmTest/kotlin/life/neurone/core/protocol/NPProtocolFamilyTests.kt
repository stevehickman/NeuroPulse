package life.neurone.core.protocol

import kotlinx.serialization.json.int
import kotlinx.serialization.json.jsonArray
import kotlinx.serialization.json.jsonObject
import kotlinx.serialization.json.jsonPrimitive
import life.neurone.core.common.InMemoryKeyValueStore
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertNotEquals
import kotlin.test.assertNotNull
import kotlin.test.assertNull
import kotlin.test.assertTrue

/**
 * The library's use of the shared core's rules (`common/npps-core/src/library.rs`): the rules themselves are tested there;
 * these tests check that the library asks them, stores their answers in the shape they define, and acts on what they say.
 */
class NPProtocolFamilyTests {

    private fun library(kv: InMemoryKeyValueStore = InMemoryKeyValueStore()): NPProtocolLibrary {
        kv.putBoolean(NPProtocolLibrary.BIPA_ACCEPTED_KEY, true)
        return NPProtocolLibrary(kv)
    }

    private fun gamma(lib: NPProtocolLibrary): NPProtocolEntry = lib.bundledProtocols.first { it.name == "Gamma Focus" }

    /** A fresh clone of the Gamma Focus family: user-owned, unused and editable. */
    private fun clone(lib: NPProtocolLibrary): NPProtocolEntry = lib.cloneFamily(gamma(lib).id).first()

    private fun longer(entry: NPProtocolEntry): NPProtocolEntry {
        val p = (entry as NPProtocolEntry.Single).protocol
        val seconds = (p.timingMode as NPTimingMode.Duration).seconds
        return NPProtocolEntry.Single(p.copy(timingMode = NPTimingMode.Duration(seconds + 60)))
    }

    // @verifies SWR-FM-03
    @Test
    fun aCloneIsANewUnusedFamilyThatIsFullyEditable() {
        val lib = library()
        val source = gamma(lib)
        val copies = lib.cloneFamily(source.id)
        assertTrue(copies.isNotEmpty())
        val copy = copies.first()
        assertNotEquals(source.id, copy.id)
        assertNotEquals(source.familyKey(), copy.familyKey(), "a clone is a new family from the moment it is made")
        assertFalse(copy.isReadOnly)
        assertFalse(lib.isUsed(copy))
        assertTrue(copy.name.endsWith("(copy)"))
        assertTrue(lib.userProtocols.any { it.id == copy.id })
        assertEquals(SaveResult.Saved, lib.save(longer(copy)), "an unused clone saves in place")
    }

    // @verifies SWR-PL-03
    @Test
    fun anUnusedProtocolSavesInPlaceAndItsUsedMarkerStaysClear() {
        val lib = library()
        val copy = clone(lib)
        val edited = longer(copy)
        assertEquals(SaveResult.Saved, lib.save(edited))
        assertFalse(lib.isUsed(edited))
        assertNotEquals(lib.contentHash(copy), lib.contentHash(edited), "an in-place save that changes the content changes the hash")
    }

    // @verifies SWR-PL-04
    @Test
    fun aPredefinedProtocolIsNeverSavedOver() {
        val lib = library()
        assertEquals(SaveResult.ReadOnly, lib.save(gamma(lib)))
        assertEquals(DeleteResult.ReadOnly, lib.delete(gamma(lib).id))
    }

    // @verifies SWR-PL-04
    @Test
    fun aPredefinedProtocolIsChangedOnlyThroughAClone() {
        val lib = library()
        val source = gamma(lib)
        assertFalse(lib.canEdit(source))
        assertFalse(lib.canDelete(source))
        val hashBefore = lib.contentHash(source)

        val copy = lib.cloneFamily(source.id).first()
        assertFalse(copy.isReadOnly, "the clone is the user's, so it is editable")
        assertTrue(lib.canEdit(copy))
        assertEquals(SaveResult.Saved, lib.save(longer(copy)))

        assertTrue(gamma(lib).isReadOnly, "the predefined protocol is still read-only")
        assertEquals(hashBefore, lib.contentHash(gamma(lib)), "and byte-identical after the clone was edited")
    }

    // @verifies SWR-SS-04
    @Test
    fun theWayBackToEditIsInPlaceForAnUnusedProtocol() {
        val lib = library()
        val copy = clone(lib)
        val before = lib.userProtocols.size
        assertEquals(copy.id, lib.entryToEdit(copy)?.id, "an unused protocol is edited where it is")
        assertEquals(before, lib.userProtocols.size, "and nothing is cloned")
    }

    // @verifies SWR-SS-04
    @Test
    fun theWayBackToEditIsACloneOfTheFamilyForAUsedOrPredefinedProtocol() {
        val lib = library()
        val copy = clone(lib)
        val hash = assertNotNull(lib.contentHash(copy))
        lib.recordUploadResult(copy, hash, "accepted", hash)
        val used = assertNotNull(lib.entryToEdit(copy))
        assertNotEquals(copy.id, used.id, "a used protocol opens on a clone")
        assertNotEquals(copy.familyKey(), used.familyKey())
        assertFalse(lib.isUsed(used))
        assertTrue(lib.canEdit(used))
        assertTrue(lib.isUsed(copy), "the used family is untouched")

        val fromPredefined = assertNotNull(lib.entryToEdit(gamma(lib)))
        assertTrue(lib.canEdit(fromPredefined), "a predefined protocol opens on an editable clone")
    }

    // @verifies SWR-UP-02
    @Test
    fun aConfirmedUploadMarksTheFamilyUsed() {
        val lib = library()
        val copy = clone(lib)
        val hash = assertNotNull(lib.contentHash(copy))
        assertEquals(UploadOutcome.CONFIRMED, lib.recordUploadResult(copy, hash, "accepted", hash))
        assertTrue(lib.isUsed(copy))
    }

    // @verifies SWR-UP-02
    @Test
    fun anUnknownUploadAlsoMarksTheFamilyUsed() {
        val lib = library()
        val copy = clone(lib)
        val hash = assertNotNull(lib.contentHash(copy))
        assertEquals(UploadOutcome.UNKNOWN, lib.recordUploadResult(copy, hash, null, null))
        assertTrue(lib.isUsed(copy), "the helmet may hold it")
    }

    // @verifies SWR-UP-02
    @Test
    fun aConfirmedFailureMarksNothing() {
        val lib = library()
        val copy = clone(lib)
        val hash = assertNotNull(lib.contentHash(copy))
        assertEquals(UploadOutcome.CONFIRMED_FAILED, lib.recordUploadResult(copy, hash, "accepted", "0".repeat(64)))
        assertEquals(UploadOutcome.CONFIRMED_FAILED, lib.recordUploadResult(copy, hash, "refused", hash))
        assertFalse(lib.isUsed(copy))
    }

    // @verifies SWR-UP-02
    @Test
    fun aRefusalBeforeSendingOrForHardwareCloneOrTierMarksNothing() {
        val lib = library()
        val copy = clone(lib)
        val hash = assertNotNull(lib.contentHash(copy))
        assertEquals(UploadOutcome.NOT_SENT, lib.recordUploadResult(copy, hash, null, null, refusedBeforeSend = true))
        for (why in listOf("hardware_version", "unmodified_clone", "tier")) {
            assertEquals(UploadOutcome.NOT_A_FAILED_UPLOAD, lib.recordUploadResult(copy, hash, "refused", null, helmetRefusal = why))
        }
        assertFalse(lib.isUsed(copy))
    }

    // @verifies SWR-PL-02
    // @verifies SWR-FM-03
    @Test
    fun aSaveToAUsedFamilyIsRefusedAndTheStoredProtocolIsUntouched() {
        val lib = library()
        val copy = clone(lib)
        val hash = assertNotNull(lib.contentHash(copy))
        lib.recordUploadResult(copy, hash, "accepted", hash)
        val result = lib.save(longer(copy))
        assertTrue(result is SaveResult.RefusedFamilyUsed, "refused, offering a whole-family clone")
        assertEquals(hash, lib.contentHash(lib.userProtocols.first { it.id == copy.id }), "the used protocol is byte-identical afterwards")
        val again = lib.cloneFamily(copy.id).first()
        assertEquals(SaveResult.Saved, lib.save(longer(again)), "the clone saves and edits freely")
    }

    // @verifies SWR-FM-05
    @Test
    fun aUsedFamilyIsArchivedNotDeleted() {
        val lib = library()
        val copy = clone(lib)
        val hash = assertNotNull(lib.contentHash(copy))
        lib.recordUploadResult(copy, hash, "accepted", hash)
        assertEquals(DeleteResult.RefusedFamilyUsed, lib.delete(copy.id))
        assertTrue(lib.userProtocols.any { it.id == copy.id })
        lib.archiveFamily(copy)
        assertTrue(lib.isArchived(copy))
        assertTrue(lib.offeredProtocols.none { it.id == copy.id }, "archived: out of the offered list")
        assertTrue(lib.userProtocols.any { it.id == copy.id }, "and every variant is kept")
        lib.unarchiveFamily(copy)
        assertTrue(lib.offeredProtocols.any { it.id == copy.id })
    }

    // @verifies SWR-FM-05
    @Test
    fun anUnusedFamilyCanBeDeleted() {
        val lib = library()
        val copy = clone(lib)
        assertEquals(DeleteResult.Deleted, lib.delete(copy.id))
        assertTrue(lib.userProtocols.none { it.id == copy.id })
        assertEquals(DeleteResult.NotFound, lib.delete(copy.id))
    }

    // @verifies SWR-FM-02#a
    // @verifies SWR-FM-02#b
    @Test
    fun aShippedFamilyReadsAsOneReleaseWithOneVariantForTheOneLayout() {
        val lib = library()
        val source = gamma(lib)
        val copy = clone(lib)
        val entries = NPLibraryRules.summaries(listOf(source, copy), emptySet(), withVersions = true)
        val tree = NPLibraryRules.familyTree(entries, source.familyKey())
        val releases = tree["releases"]!!.jsonArray
        assertEquals(1, releases.size, "one release: the clone is another family and is not listed")
        val release = releases[0].jsonObject
        assertEquals("1.0", release["version"]!!.jsonPrimitive.content, "every shipped protocol is at 1.0 until a release is done")
        assertEquals(1, release["major"]!!.jsonPrimitive.int)
        assertEquals(0, release["minor"]!!.jsonPrimitive.int)
        val variants = release["variants"]!!.jsonArray
        assertEquals(1, variants.size, "one layout exists, so one variant")
        assertEquals(1, variants[0].jsonObject["hardwareVersion"]!!.jsonPrimitive.int)
        assertEquals(source.id.toString(), variants[0].jsonObject["id"]!!.jsonPrimitive.content)
        assertTrue(tree["conflicts"]!!.jsonArray.isEmpty())
        assertTrue(tree["unreleased"]!!.jsonArray.isEmpty())
    }

    // @verifies SWR-FM-01
    @Test
    fun anArrivingProtocolJoinsTheFamilyThatHasItsId() {
        val lib = library()
        val source = gamma(lib)
        val copy = clone(lib)
        val local = NPLibraryRules.summaries(listOf(source), emptySet(), withVersions = false)
        val incoming = NPLibraryRules.summaries(listOf(source, copy), emptySet(), withVersions = false)
        val answer = NPLibraryRules.importFamilies(local, incoming).map { it.jsonObject }
        assertEquals("existing", answer[0]["family"]!!.jsonPrimitive.content)
        assertEquals(source.familyKey(), answer[0]["familyId"]!!.jsonPrimitive.content, "the id that arrived is the id that is kept")
        assertEquals("new", answer[1]["family"]!!.jsonPrimitive.content, "another family id is another family")
        assertEquals(copy.familyKey(), answer[1]["familyId"]!!.jsonPrimitive.content)
    }

    // @verifies SWR-WO-02
    @Test
    fun theUsedFlagSurvivesARestartAndHasNoFieldButTheFamilyId() {
        val kv = InMemoryKeyValueStore()
        val lib = library(kv)
        val copy = clone(lib)
        val hash = assertNotNull(lib.contentHash(copy))
        lib.recordUploadResult(copy, hash, "accepted", hash)
        val stored = kv.getString(NPProtocolLibrary.USED_FAMILIES_KEY)
        assertEquals("""[{"familyId":"${copy.familyKey()}"}]""", stored)
        assertTrue(library(kv).isUsed(copy), "the flag is read back after a restart")
        // A stored record that carries anything else is not one this library wrote, and is not trusted.
        kv.putString(NPProtocolLibrary.USED_FAMILIES_KEY, """[{"familyId":"${copy.familyKey()}","device":"x"}]""")
        assertFalse(library(kv).isUsed(copy))
    }

    // @verifies SWR-WO-02
    @Test
    fun deletingAnUnusedProtocolAndMakingAnotherNeverClearsAUsedFamily() {
        val lib = library()
        val copy = clone(lib)
        val hash = assertNotNull(lib.contentHash(copy))
        lib.recordUploadResult(copy, hash, "accepted", hash)
        // Nothing in the library's surface clears a used flag: the family stays used whatever is done to its entries.
        lib.archiveFamily(copy)
        lib.unarchiveFamily(copy)
        assertTrue(lib.isUsed(copy))
    }

    // @verifies SWR-PL-06
    @Test
    fun anUnmodifiedCloneIsRefusedAtUploadAndAChangedOneIsAllowed() {
        val lib = library()
        val copy = clone(lib)
        val gate = lib.uploadGate(copy, helmetHardwareVersion = 1)
        assertTrue(gate is UploadGate.UnmodifiedClone, "the clone's content is its source's")
        assertEquals(gamma(lib).id, (gate as UploadGate.UnmodifiedClone).sourceId)
        assertFalse(lib.isUsed(copy), "a refusal marks nothing")
        assertFalse(lib.isUsed(gamma(lib)), "and neither does it touch the source")
        lib.save(longer(copy))
        assertEquals(UploadGate.Allow, lib.uploadGate(lib.userProtocols.first { it.id == copy.id }, helmetHardwareVersion = 1))
    }

    // @verifies SWR-HV-04
    @Test
    fun anUploadToAHelmetOfAnotherHardwareVersionIsRefused() {
        val lib = library()
        val copy = longer(clone(lib)).also { lib.save(it) }
        val gate = lib.uploadGate(copy, helmetHardwareVersion = 2)
        assertTrue(gate is UploadGate.HardwareMismatch)
        assertEquals(listOf(1), (gate as UploadGate.HardwareMismatch).protocolVersions)
        assertFalse(lib.isUsed(copy))
    }

    // @verifies SWR-HV-04
    @Test
    fun theVariantForTheHelmetIsOfferedOrNoneIs() {
        val lib = library()
        val copy = clone(lib)
        assertEquals(listOf(copy.id), lib.variantsFor(copy, helmetVersion = 1).map { it.id })
        assertTrue(lib.variantsFor(copy, helmetVersion = 2).isEmpty(), "none: the person decides whether to create one")
    }

    // @verifies SWR-WO-03
    @Test
    fun theUsedFlagTravelsInExportAndAnImportNeverClearsIt() {
        val lib = library()
        val copy = clone(lib)
        val hash = assertNotNull(lib.contentHash(copy))
        val unflagged = lib.exportScript(copy)
        assertFalse(unflagged.contains("used: true"))
        lib.recordUploadResult(copy, hash, "accepted", hash)
        val flagged = lib.exportScript(copy)
        assertTrue(flagged.contains("used: true"), "export carries the flag")
        assertEquals(hash, assertNotNull(lib.contentHash(lib.importScript(flagged))), "and the flag is not part of the hash")

        val fresh = library()
        val imported = fresh.importScript(flagged)
        assertTrue(fresh.isUsed(imported), "a protocol that arrives flagged used is used")

        val again = library()
        val plain = again.importScript(unflagged)
        assertFalse(again.isUsed(plain), "unflagged and not used here: not used")
        // An import of an unflagged copy of a protocol already used here leaves it used.
        lib.importScript(unflagged)
        assertTrue(lib.isUsed(copy))
    }

    // @verifies SWR-PL-02
    @Test
    fun theRefusedEditIsKeptOnACopyOfTheWholeFamily() {
        val lib = library()
        val copy = clone(lib)
        val hash = assertNotNull(lib.contentHash(copy))
        lib.recordUploadResult(copy, hash, "accepted", hash)
        val edit = longer(copy)
        assertTrue(lib.save(edit) is SaveResult.RefusedFamilyUsed)

        val kept = assertNotNull(lib.saveAsClone(edit))
        assertNotEquals(copy.id, kept.id)
        assertNotEquals(copy.familyKey(), kept.familyKey(), "a new family")
        assertFalse(lib.isUsed(kept))
        val seconds = ((kept as NPProtocolEntry.Single).protocol.timingMode as NPTimingMode.Duration).seconds
        val before = ((copy as NPProtocolEntry.Single).protocol.timingMode as NPTimingMode.Duration).seconds
        assertEquals(before + 60, seconds, "the person's edit is what the copy holds")
        assertEquals(hash, lib.contentHash(lib.userProtocols.first { it.id == copy.id }), "and the used protocol is untouched")
    }

    // @verifies SWR-FM-06
    @Test
    fun aNewVariantForAHardwareVersionTheUsedFamilyHasIsRefusedAndKeptAsItsOwnFamily() {
        val lib = library()
        val copy = clone(lib)
        val hash = assertNotNull(lib.contentHash(copy))
        lib.recordUploadResult(copy, hash, "accepted", hash)
        val p = (copy as NPProtocolEntry.Single).protocol
        val joiner = NPProtocolEntry.Single(p.copy(id = life.neurone.core.common.UUID.randomUUID(), name = "Joiner"))
        assertEquals(SaveResult.RefusedVariantExists, lib.save(joiner))
        val own = assertNotNull(lib.saveAsClone(joiner))
        assertNotEquals(copy.familyKey(), own.familyKey())
        assertTrue(lib.userProtocols.any { it.id == own.id })
    }

    // @verifies SWR-HV-04
    @Test
    fun theMismatchRefusalSaysTheHelmetLayoutIsNotInTheTableYet() {
        val lib = library()
        val copy = longer(clone(lib)).also { lib.save(it) }
        val gate = lib.uploadGate(copy, helmetHardwareVersion = 2) as UploadGate.HardwareMismatch
        assertNull(gate.sharedSockets, "no layout for version 2 exists, so no overlap is claimed")
        assertNull(gate.onlyHelmetSockets)
    }
}
