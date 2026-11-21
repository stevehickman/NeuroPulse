package life.neurone.core.protocol

import life.neurone.core.common.PresenceInstall
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertIs
import kotlin.test.assertTrue

/**
 * The protocol library under the presence gate (`REQ-PRES-01` (d), `OI-PRIV-19`). Built locked it read "absent", and the first save
 * wrote the library and the family state from empty over the sealed ones; the BIPA consent it read once stayed false for the run.
 */
class NPProtocolLibraryPresenceTests {

    private val sealed = setOf(
        NPProtocolLibrary.USER_PROTOCOLS_KEY, NPProtocolLibrary.USED_FAMILIES_KEY, NPProtocolLibrary.ARCHIVED_FAMILIES_KEY,
        NPProtocolLibrary.CLONE_SOURCES_KEY, NPProtocolLibrary.BIPA_ACCEPTED_KEY,
    )

    private fun custom(name: String) = NPProtocolEntry.Single(
        NPProtocolDefinition(
            name = name,
            modalities = listOf(NPProtocolModality(params = NPModalityParams.PbmTranscranial(NPPBMTranscranialParams()))),
        ),
    )

    private fun installWithProtocol(): PresenceInstall = PresenceInstall { sealed }.also { install ->
        val run = install.Run()
        run.unlock()
        run.kv.putBoolean(NPProtocolLibrary.BIPA_ACCEPTED_KEY, true)
        val lib = NPProtocolLibrary(run.kv, lock = run.lock)
        check(lib.save(custom("My Custom")) == SaveResult.Saved)
        run.lock.lock()
    }

    // @verifies REQ-PRES-01#d
    @Test fun a_library_built_locked_shows_no_user_protocol_and_reads_them_and_the_consent_at_the_unlock() {
        val run = installWithProtocol().Run()
        val lib = NPProtocolLibrary(run.kv, lock = run.lock)

        assertFalse(lib.isReadable)
        assertTrue(lib.userProtocols.isEmpty())
        assertFalse(lib.eegConsentGranted)
        assertTrue(lib.bundledProtocols.isNotEmpty(), "the shipped protocols are not the person's and still show")

        run.unlock()

        assertEquals(listOf("My Custom"), lib.userProtocols.map { it.name })
        assertTrue(lib.eegConsentGranted, "the sealed BIPA consent is read at the unlock, not taken for false for the run")
    }

    // The defect: a save from the empty library replaced the sealed one.
    // @verifies REQ-PRES-01#d
    @Test fun a_save_a_delete_or_an_import_while_locked_changes_nothing_and_the_sealed_library_survives() {
        val install = installWithProtocol()
        val run = install.Run()
        val lib = NPProtocolLibrary(run.kv, lock = run.lock)

        assertIs<SaveResult.Locked>(lib.save(custom("Made while locked")))
        assertIs<NPProtocolLibrary.ZoneSaveResult.Locked>(lib.addUserZone("Z", listOf(1, 2)))
        assertTrue(lib.openScript("anything").locked)

        run.unlock()

        assertEquals(listOf("My Custom"), lib.userProtocols.map { it.name }, "the sealed library is whole, with nothing added")

        run.lock.lock()
        val next = install.Run()
        val again = NPProtocolLibrary(next.kv, lock = next.lock)
        next.unlock()
        assertEquals(listOf("My Custom"), again.userProtocols.map { it.name })
    }

    // An archive is an operation on one family: it joins the sealed family state at the unlock.
    // @verifies REQ-PRES-01#d
    @Test fun an_archive_made_while_locked_is_applied_to_the_sealed_family_state_at_the_unlock() {
        val install = installWithProtocol()
        val run = install.Run()
        val lib = NPProtocolLibrary(run.kv, lock = run.lock)
        run.unlock()
        val entry = lib.userProtocols.single()
        run.lock.lock()

        lib.archiveFamily(entry)
        run.unlock()

        assertTrue(lib.isArchived(lib.userProtocols.single()))
    }

    // @verifies REQ-PRES-01#d
    @Test fun the_library_leaves_memory_when_the_lock_closes() {
        val run = installWithProtocol().Run()
        val lib = NPProtocolLibrary(run.kv, lock = run.lock)
        run.unlock()
        assertEquals(1, lib.userProtocols.size)

        run.lock.lock()

        assertFalse(lib.isReadable)
        assertTrue(lib.userProtocols.isEmpty())
        assertFalse(lib.eegConsentGranted)
    }
}
