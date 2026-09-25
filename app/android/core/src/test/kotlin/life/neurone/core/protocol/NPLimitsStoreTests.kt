package life.neurone.core.protocol

import life.neurone.core.common.InMemoryKeyValueStore
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNull
import kotlin.test.assertTrue

class NPLimitsStoreTests {

    @Test
    fun resolvePrefersIndividualThenHelmetThenGlobal() {
        val global = NPLimitsSet(
            name = "g", pbmTranscranial = NPPBMTranscranialLimits(maxIrradianceMWcm2 = 90.0, maxSessionDoseJCm2 = 30.0),
        )
        val helmet = NPLimitsSet(name = "h", pbmTranscranial = NPPBMTranscranialLimits(maxIrradianceMWcm2 = 80.0))
        val individual = NPLimitsSet(name = "i", pbmTranscranial = NPPBMTranscranialLimits(maxIrradianceMWcm2 = 70.0))

        val r = resolveLimits(global, helmet, individual).pbmTranscranial!!
        assertEquals(70.0, r.maxIrradianceMWcm2) // individual wins
        assertEquals(30.0, r.maxSessionDoseJCm2)  // falls through to global (only it set this field)
    }

    @Test
    fun resolveReturnsNullBlockWhenAllTiersOmitIt() {
        val r = resolveLimits(NPLimitsSet(name = "g"), null, null)
        assertNull(r.pbmTranscranial)
        assertNull(r.tdcs)
    }

    @Test
    fun resolveWithNoTiersIsUnlimited() {
        val r = resolveLimits(null, null, null)
        assertNull(r.pbmTranscranial)
        assertNull(r.besTacs)
    }

    @Test
    fun storeResolvedLimitsHonorsActiveProfile() {
        val store = NPLimitsStore(InMemoryKeyValueStore())
        store.saveGlobalLimits(NPLimitsSet(name = "g", tdcs = NPTDCSLimits(maxIntensityMilliamps = 2.0)))
        val profile = NPIndividualProfile(name = "Alex")
        store.saveProfile(profile)
        store.saveIndividualLimits(NPLimitsSet(name = "i", tdcs = NPTDCSLimits(maxIntensityMilliamps = 1.0)), profile.id)

        // No active profile → global 2.0 mA.
        assertEquals(2.0, store.resolvedLimits.tdcs!!.maxIntensityMilliamps)
        // Activate the profile → individual 1.0 mA wins.
        store.activeProfileId = profile.id
        assertEquals(1.0, store.resolvedLimits.tdcs!!.maxIntensityMilliamps)
    }

    @Test
    fun profilesAndActiveProfilePersistAcrossReload() {
        val kv = InMemoryKeyValueStore()
        val s1 = NPLimitsStore(kv)
        val alex = NPIndividualProfile(name = "Alex", notes = "left-handed")
        val sam = NPIndividualProfile(name = "Sam")
        s1.saveProfile(alex)
        s1.saveProfile(sam)
        s1.setActiveProfile(sam.id)

        val s2 = NPLimitsStore(kv)
        assertEquals(listOf(alex, sam), s2.profiles)
        assertEquals(sam.id, s2.activeProfileId)
        assertEquals(sam, s2.activeProfile)
    }

    @Test
    fun setActiveProfileIgnoresUnknownIdAndNotifies() {
        val store = NPLimitsStore(InMemoryKeyValueStore())
        val alex = NPIndividualProfile(name = "Alex")
        store.saveProfile(alex)
        val seen = mutableListOf<String?>()
        store.onActiveProfileChanged = { seen += it }

        store.setActiveProfile("not-a-profile")
        assertNull(store.activeProfileId, "an id that is not a saved profile is ignored")
        store.setActiveProfile(alex.id)
        store.setActiveProfile(alex.id)   // no change, no second notification
        store.setActiveProfile(null)
        assertEquals(listOf(alex.id, null), seen)
    }

    @Test
    fun deletingTheActiveProfileClearsItDurably() {
        val kv = InMemoryKeyValueStore()
        val s1 = NPLimitsStore(kv)
        val alex = NPIndividualProfile(name = "Alex")
        s1.saveProfile(alex)
        s1.setActiveProfile(alex.id)
        s1.deleteProfile(alex.id)
        assertNull(s1.activeProfileId)

        val s2 = NPLimitsStore(kv)
        assertTrue(s2.profiles.isEmpty())
        assertNull(s2.activeProfileId)
    }

    @Test
    fun corruptProfileRecordLoadsAsNoProfiles() {
        val kv = InMemoryKeyValueStore()
        kv.putString(NPLimitsStore.PROFILES_KEY, "{not json")
        kv.putString(NPLimitsStore.ACTIVE_PROFILE_KEY, "orphan")
        val store = NPLimitsStore(kv)
        assertTrue(store.profiles.isEmpty())
        assertNull(store.activeProfileId, "an active id with no saved profile is not restored")
    }

    @Test
    fun globalLimitsPersistAcrossReloadViaNpps() {
        val kv = InMemoryKeyValueStore()
        val s1 = NPLimitsStore(kv)
        s1.saveGlobalLimits(NPLimitsSet(name = "g", pbmTranscranial = NPPBMTranscranialLimits(maxIrradianceMWcm2 = 60.0)))
        val s2 = NPLimitsStore(kv)
        assertEquals(60.0, s2.globalLimits?.pbmTranscranial?.maxIrradianceMWcm2)
    }

    @Test
    fun makeValidatorEnforcesResolvedLimits() {
        val store = NPLimitsStore(InMemoryKeyValueStore())
        store.saveGlobalLimits(NPLimitsSet(name = "g", pbmTranscranial = NPPBMTranscranialLimits(maxIrradianceMWcm2 = 50.0)))
        val def = NPProtocolDefinition(
            name = "hot",
            modalities = listOf(
                NPProtocolModality(params = NPModalityParams.PbmTranscranial(NPPBMTranscranialParams(irradianceMWcm2 = 80.0))),
            ),
        )
        val result = store.makeValidator().validate(def)
        assertTrue(result.errors.any { it.parameterKey == "irradianceMWcm2" })
    }
}
