package life.neurone.core.protocol

import life.neurone.core.common.InMemoryKeyValueStore
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertNotEquals
import kotlin.test.assertNotNull
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
}
