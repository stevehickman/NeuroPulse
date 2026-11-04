package life.neurone.core.protocol

import life.neurone.core.ble.HubTier
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

    // @verifies REQ-UPG-01
    @Test
    fun theMenuTierIsT2OnlyWhenTheHubSaysT2() {
        assertEquals(DeviceTier.NONE, deviceTierFor(false, HubTier.T2), "no helmet connected")
        assertEquals(DeviceTier.T2, deviceTierFor(true, HubTier.T2))
        assertEquals(DeviceTier.T1, deviceTierFor(true, HubTier.T1))
        assertEquals(DeviceTier.T1, deviceTierFor(true, HubTier.UNKNOWN), "no valid report yet is not T2")
        assertEquals(DeviceTier.T1, deviceTierFor(true, null), "firmware with no DEVICE_TIER characteristic is not T2")
    }

    // @verifies REQ-NPPS-18
    @Test
    fun bundledProtocolCountMatchesManifest() {
        // Counted from protocols/predefined/manifest.json, not hardcoded. This
        // object used to be a hand-maintained Kotlin transcription carrying 19
        // of the library's protocols; it is now the whole library, copied onto
        // the resource path by the bundlePredefinedProtocols Gradle task.
        //
        // protocolFiles, not manifestFiles: the manifest also lists the zone and
        // condition definition files, which load into the namespace but are not
        // library items.
        val expected = NPBundledProtocols.protocolFiles.size
        assertEquals(expected, library().bundledProtocols.size,
            "Every predefined NPPS template in manifest.json must parse without error.")
    }

    // @verifies SWR-PL-04
    @Test
    fun allBundledProtocolsAreReadOnly() {
        val lib = library()
        for (entry in lib.bundledProtocols) {
            assertTrue(entry.isReadOnly, "Bundled protocol '${entry.name}' must be read-only.")
            assertFalse(lib.canEdit(entry))
            assertFalse(lib.canDelete(entry))
        }
    }

    // @verifies REQ-NPPS-18
    @Test
    fun bundledIncludesFourComposites() {
        assertEquals(4, library().bundledProtocols.count { it.isComposite })
    }

    // @verifies REQ-NPPS-19
    @Test
    fun availabilityUnavailableWhenNoDevice() {
        val lib = library()
        val gamma = lib.bundledProtocols.first { it.name == "Gamma Focus" }
        // No device connected → unavailable.
        assertFalse(lib.availability(gamma).isAvailable)
    }

    // @verifies REQ-NPPS-19
    @Test
    fun availabilityAvailableForT1ProtocolWithT1Device() {
        val lib = library()
        lib.updateAvailability(DeviceTier.T1, smartModules = false)
        val gamma = lib.bundledProtocols.first { it.name == "Gamma Focus" }
        assertTrue(lib.availability(gamma).isAvailable, "Gamma Focus is all-T1 and must be available on a T1 device.")
    }

    // @verifies REQ-ONBOARD-05#a
    @Test
    fun eegProtocolBlockedWhenConsentDeclined() {
        val lib = library(consent = false)
        lib.updateAvailability(DeviceTier.T1, smartModules = false)
        // Gamma Focus includes eeg_neurofeedback → blocked without BIPA consent.
        val gamma = lib.bundledProtocols.first { it.name == "Gamma Focus" }
        assertEquals(ProtocolAvailability.EegConsentRequired, lib.availability(gamma))
    }

    // @verifies REQ-NPPS-18
    @Test
    fun eegConsentTogglePropagates() {
        val lib = library(consent = false)
        lib.updateAvailability(DeviceTier.T1, smartModules = false)
        val gamma = lib.bundledProtocols.first { it.name == "Gamma Focus" }
        assertEquals(ProtocolAvailability.EegConsentRequired, lib.availability(gamma))
        lib.updateEEGConsent(true)
        assertTrue(lib.availability(gamma).isAvailable)
    }

    // @verifies REQ-ONBOARD-05#b
    @Test
    fun retinalHealthNonEEGAvailableWithoutConsent() {
        val lib = library(consent = false)
        lib.updateAvailability(DeviceTier.T1, smartModules = false)
        // Retinal Health is visual-only (no EEG) → unaffected by consent.
        val retinal = lib.bundledProtocols.first { it.name == "Retinal Health" }
        assertTrue(lib.availability(retinal).isAvailable)
    }

    // @verifies REQ-NPPS-18
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

    // @verifies SWR-PL-04
    @Test
    fun cannotDeleteBundledProtocol() {
        val lib = library()
        val gamma = lib.bundledProtocols.first { it.name == "Gamma Focus" }
        lib.delete(gamma.id)
        assertEquals(NPBundledProtocols.protocolFiles.size, lib.bundledProtocols.size,
            "Bundled protocols must be immutable.")
    }

    // @verifies REQ-NPPS-18
    @Test
    fun validationResultCachedAndCounted() {
        val lib = library()
        val gamma = lib.bundledProtocols.first { it.name == "Gamma Focus" }
        val result = lib.validationResult(gamma)
        assertTrue(result.isValid, "The bundled Gamma Focus template must validate clean under unlimited limits.")
        val (errors, _) = lib.issueCount(gamma)
        assertEquals(0, errors)
    }

    // @verifies REQ-NPPS-18
    @Test
    fun everyBundledProtocolHasAUniqueNameAndAUniqueId() {
        val bundled = library().bundledProtocols
        assertTrue(bundled.isNotEmpty())
        val names = bundled.map { it.name }
        assertEquals(names.size, names.toSet().size, "a duplicate name is a copy-paste error in the bundled files")
        val ids = bundled.map { it.id }
        assertEquals(ids.size, ids.toSet().size, "a duplicate id would break lookup and persistence")
    }

    // @verifies REQ-NPPS-18
    @Test
    fun savingABundledProtocolIsRefusedAndChangesNothing() {
        val lib = library()
        val first = lib.bundledProtocols.first()
        val before = lib.bundledProtocols.size
        assertEquals(SaveResult.ReadOnly, lib.save(first))
        assertEquals(before, lib.bundledProtocols.size)
        assertEquals(first.name, lib.bundledProtocols.first().name)
    }

    // @verifies REQ-NPPS-10#a
    @Test
    fun theRetiredCombinedWavelengthsAreRefusedWhetherQuotedOrNot() {
        fun script(wavelength: String) = """
            protocol "Wavelength Test" {
                id: "AABB0200-0000-0000-0000-000000000000"
                description: "x"
                author: "Test"
                version: "1.0"
                readonly: false
                duration: 10m
                pbm_transcranial {
                    irradiance: 100mW_cm2
                    frequency: 40Hz
                    duty_cycle: 25%
                    zones: ["Frontal"]
                    wavelength: $wavelength
                }
            }
        """.trimIndent()
        for (wl in listOf("660_808nm", "660_808_1064nm")) {
            val quoted = kotlin.test.assertFails { NPPSParser.parse(script("\"$wl\"")) }
            val text = quoted.message ?: ""
            assertTrue("retired" in text && "one block per wavelength" in text, text)
            kotlin.test.assertFails { NPPSParser.parse(script(wl)) }
        }
    }
}
