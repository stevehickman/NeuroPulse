package life.neurone.core.protocol

import life.neurone.core.common.InMemoryKeyValueStore
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertTrue

/**
 * Port of iOS NPProtocolLibraryTests (ISC-39) plus availability + EEG-consent integration.
 * Verifies every bundled NPPS template parses, is read-only, and that availability honors
 * device tier and the BIPA consent gate.
 */
class NPProtocolLibraryTests {

    private fun library(consent: Boolean = true): NPProtocolLibrary {
        val kv = InMemoryKeyValueStore()
        kv.putBoolean(NPProtocolLibrary.BIPA_ACCEPTED_KEY, consent)
        return NPProtocolLibrary(kv)
    }

    @Test
    fun bundledProtocolCountMatchesManifest() {
        // Counted from protocols/predefined/manifest.json, not hardcoded. This
        // object used to be a hand-maintained Kotlin transcription carrying 19
        // of the library's protocols; it is now the whole library, copied onto
        // the resource path by the bundlePredefinedProtocols Gradle task.
        val expected = NPBundledProtocols.manifestFiles.size
        assertEquals(expected, library().bundledProtocols.size,
            "Every predefined NPPS template in manifest.json must parse without error.")
    }

    @Test
    fun allBundledProtocolsAreReadOnly() {
        val lib = library()
        for (entry in lib.bundledProtocols) {
            assertTrue(entry.isReadOnly, "Bundled protocol '${entry.name}' must be read-only.")
            assertFalse(lib.canEdit(entry))
            assertFalse(lib.canDelete(entry))
        }
    }

    @Test
    fun bundledIncludesFourComposites() {
        assertEquals(4, library().bundledProtocols.count { it.isComposite })
    }

    @Test
    fun availabilityUnavailableWhenNoDevice() {
        val lib = library()
        val gamma = lib.bundledProtocols.first { it.name == "Gamma Focus" }
        // No device connected → unavailable.
        assertFalse(lib.availability(gamma).isAvailable)
    }

    @Test
    fun availabilityAvailableForT1ProtocolWithT1Device() {
        val lib = library()
        lib.updateAvailability(DeviceTier.T1, smartModules = false)
        val gamma = lib.bundledProtocols.first { it.name == "Gamma Focus" }
        assertTrue(lib.availability(gamma).isAvailable, "Gamma Focus is all-T1 and must be available on a T1 device.")
    }

    @Test
    fun eegProtocolBlockedWhenConsentDeclined() {
        val lib = library(consent = false)
        lib.updateAvailability(DeviceTier.T1, smartModules = false)
        // Gamma Focus includes eeg_neurofeedback → blocked without BIPA consent.
        val gamma = lib.bundledProtocols.first { it.name == "Gamma Focus" }
        assertEquals(ProtocolAvailability.EegConsentRequired, lib.availability(gamma))
    }

    @Test
    fun eegConsentTogglePropagates() {
        val lib = library(consent = false)
        lib.updateAvailability(DeviceTier.T1, smartModules = false)
        val gamma = lib.bundledProtocols.first { it.name == "Gamma Focus" }
        assertEquals(ProtocolAvailability.EegConsentRequired, lib.availability(gamma))
        lib.updateEEGConsent(true)
        assertTrue(lib.availability(gamma).isAvailable)
    }

    @Test
    fun retinalHealthNonEEGAvailableWithoutConsent() {
        val lib = library(consent = false)
        lib.updateAvailability(DeviceTier.T1, smartModules = false)
        // Retinal Health is visual-only (no EEG) → unaffected by consent.
        val retinal = lib.bundledProtocols.first { it.name == "Retinal Health" }
        assertTrue(lib.availability(retinal).isAvailable)
    }

    @Test
    fun userProtocolPersistsAcrossReload() {
        val kv = InMemoryKeyValueStore()
        kv.putBoolean(NPProtocolLibrary.BIPA_ACCEPTED_KEY, true)
        val lib1 = NPProtocolLibrary(kv)
        val custom = NPProtocolEntry.Single(
            NPProtocolDefinition(
                name = "My Custom",
                modalities = listOf(
                    NPProtocolModality(params = NPModalityParams.PbmTranscranial(NPPBMTranscranialParams())),
                ),
            ),
        )
        lib1.save(custom)
        assertEquals(1, lib1.userProtocols.size)

        // New library over the same store must reload the user protocol.
        val lib2 = NPProtocolLibrary(kv)
        assertEquals(1, lib2.userProtocols.size)
        assertEquals("My Custom", lib2.userProtocols.first().name)
    }

    @Test
    fun cannotDeleteBundledProtocol() {
        val lib = library()
        val gamma = lib.bundledProtocols.first { it.name == "Gamma Focus" }
        lib.delete(gamma.id)
        assertEquals(NPBundledProtocols.manifestFiles.size, lib.bundledProtocols.size,
            "Bundled protocols must be immutable.")
    }

    @Test
    fun validationResultCachedAndCounted() {
        val lib = library()
        val gamma = lib.bundledProtocols.first { it.name == "Gamma Focus" }
        val result = lib.validationResult(gamma)
        assertTrue(result.isValid, "The bundled Gamma Focus template must validate clean under unlimited limits.")
        val (errors, _) = lib.issueCount(gamma)
        assertEquals(0, errors)
    }
}
