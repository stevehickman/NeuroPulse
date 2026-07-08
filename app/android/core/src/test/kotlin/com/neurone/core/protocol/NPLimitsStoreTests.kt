package com.neurone.core.protocol

import com.neurone.core.common.InMemoryKeyValueStore
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNull
import kotlin.test.assertTrue

class NPLimitsStoreTests {

    @Test
    fun resolvePrefersIndividualThenHelmetThenGlobal() {
        val global = NPLimitsSet(
            name = "g", pbmTranscranial = NPPBMTranscranialLimits(maxIntensityPercent = 90.0, maxSessionDoseJCm2 = 30.0),
        )
        val helmet = NPLimitsSet(name = "h", pbmTranscranial = NPPBMTranscranialLimits(maxIntensityPercent = 80.0))
        val individual = NPLimitsSet(name = "i", pbmTranscranial = NPPBMTranscranialLimits(maxIntensityPercent = 70.0))

        val r = resolveLimits(global, helmet, individual).pbmTranscranial!!
        assertEquals(70.0, r.maxIntensityPercent) // individual wins
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
    fun globalLimitsPersistAcrossReloadViaNpps() {
        val kv = InMemoryKeyValueStore()
        val s1 = NPLimitsStore(kv)
        s1.saveGlobalLimits(NPLimitsSet(name = "g", pbmTranscranial = NPPBMTranscranialLimits(maxIntensityPercent = 60.0)))
        val s2 = NPLimitsStore(kv)
        assertEquals(60.0, s2.globalLimits?.pbmTranscranial?.maxIntensityPercent)
    }

    @Test
    fun makeValidatorEnforcesResolvedLimits() {
        val store = NPLimitsStore(InMemoryKeyValueStore())
        store.saveGlobalLimits(NPLimitsSet(name = "g", pbmTranscranial = NPPBMTranscranialLimits(maxIntensityPercent = 50.0)))
        val def = NPProtocolDefinition(
            name = "hot",
            modalities = listOf(
                NPProtocolModality(params = NPModalityParams.PbmTranscranial(NPPBMTranscranialParams(intensityPercent = 80.0))),
            ),
        )
        val result = store.makeValidator().validate(def)
        assertTrue(result.errors.any { it.parameterKey == "intensityPercent" })
    }
}
