package life.neurone.core.protocol

import life.neurone.core.common.PresenceInstall
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertNull
import kotlin.test.assertTrue

/**
 * The limits store under the presence gate (`REQ-PRES-01` (d), `OI-PRIV-19`). Built locked it reads "absent", which is not "no limits":
 * it must not replace or remove a sealed tier, and the configured limits must apply once the person unlocks.
 */
class NPLimitsStorePresenceTests {

    private val sealed = setOf(
        NPLimitsStore.GLOBAL_KEY, NPLimitsStore.PROFILES_KEY, NPLimitsStore.ACTIVE_PROFILE_KEY,
        NPLimitsStore.HELMETS_KEY, NPLimitsStore.INDIVIDUALS_KEY,
    )

    private fun limits(name: String, mA: Double) = NPLimitsSet(name = name, tdcs = NPTDCSLimits(maxIntensityMilliamps = mA))

    private val alex = NPIndividualProfile(id = "alex", name = "Alex")

    /** An install whose sealed limits hold a global 2.0 mA, a profile with its own 1.0 mA and a helmet tier, as an earlier run left it. */
    private fun installWithLimits(): PresenceInstall = PresenceInstall { sealed }.also { install ->
        val run = install.Run()
        run.unlock()
        val store = NPLimitsStore(run.kv, lock = run.lock)
        store.saveGlobalLimits(limits("g", 2.0))
        store.saveProfile(alex)
        store.saveIndividualLimits(limits("i", 1.0), alex.id)
        store.saveHelmetLimits(limits("h", 1.5), "HELMET-1")
        store.setActiveProfile(alex.id)
        run.lock.lock()
    }

    // The defect: the store loaded once at construction, so one built locked ran with no limits for the whole run, even after the unlock.
    // @verifies REQ-PRES-01#d
    @Test fun the_configured_limits_apply_once_the_person_unlocks_a_store_built_locked() {
        val run = installWithLimits().Run()
        var announced: String? = "unset"
        val store = NPLimitsStore(run.kv, lock = run.lock).also { it.onActiveProfileChanged = { id -> announced = id } }

        assertFalse(store.isReadable)
        assertNull(store.resolvedLimits.tdcs, "locked, nothing is configured as far as this store can say")

        run.unlock()

        assertTrue(store.isReadable)
        assertEquals(1.0, store.resolvedLimits.tdcs!!.maxIntensityMilliamps)
        assertEquals(alex.id, announced, "the hub is told who is using the device when the profile can be read")
    }

    // A change made locked is an operation on one tier, replayed on what was read: it neither replaces nor removes what was sealed.
    // @verifies REQ-PRES-01#d
    @Test fun a_change_made_while_locked_is_applied_to_its_tier_and_the_other_tiers_survive() {
        val install = installWithLimits()
        val run = install.Run()
        val store = NPLimitsStore(run.kv, lock = run.lock)

        store.saveGlobalLimits(limits("g2", 1.2))
        store.clearHelmetLimits("HELMET-1")
        store.saveProfile(NPIndividualProfile(id = "sam", name = "Sam"))
        assertTrue(store.profiles.isEmpty(), "nothing is shown while locked")

        run.unlock()

        assertEquals(1.2, store.globalLimits!!.tdcs!!.maxIntensityMilliamps)
        assertTrue(store.helmetLimits.isEmpty(), "the helmet tier was cleared, as asked")
        assertEquals(listOf("alex", "sam"), store.profiles.map { it.id }, "the sealed profile is still there beside the new one")
        assertEquals(1.0, store.individualLimits.getValue("alex").tdcs!!.maxIntensityMilliamps, "the individual tier was not replaced")

        // It is what was written: the next run reads the same.
        run.lock.lock()
        val next = install.Run()
        val again = NPLimitsStore(next.kv, lock = next.lock)
        next.unlock()
        assertEquals(1.2, again.globalLimits!!.tdcs!!.maxIntensityMilliamps)
        assertEquals(listOf("alex", "sam"), again.profiles.map { it.id })
        assertEquals(1.0, again.individualLimits.getValue("alex").tdcs!!.maxIntensityMilliamps)
    }

    // The tier a save would have written from an empty cache is the one that used to be removed at once.
    // @verifies REQ-PRES-01#d
    @Test fun a_save_while_locked_does_not_remove_a_sealed_tier() {
        val install = installWithLimits()
        val run = install.Run()
        val store = NPLimitsStore(run.kv, lock = run.lock)

        store.saveIndividualLimits(limits("i2", 0.8), "sam")
        store.deleteProfile("nobody")

        run.unlock()

        assertEquals(1.5, store.helmetLimits.getValue("HELMET-1").tdcs!!.maxIntensityMilliamps, "the helmet tier survives")
        assertEquals(1.0, store.individualLimits.getValue("alex").tdcs!!.maxIntensityMilliamps)
    }

    // The limits leave memory when the lock closes, without telling the hub nobody is using the device.
    // @verifies REQ-PRES-01#d
    @Test fun the_limits_leave_memory_when_the_lock_closes_without_announcing_nobody() {
        val run = installWithLimits().Run()
        var announced: String? = "unset"
        val store = NPLimitsStore(run.kv, lock = run.lock).also { it.onActiveProfileChanged = { id -> announced = id } }
        run.unlock()
        assertEquals(alex.id, announced)

        run.lock.lock()

        assertFalse(store.isReadable)
        assertTrue(store.profiles.isEmpty())
        assertNull(store.globalLimits)
        assertEquals(alex.id, announced, "the hub keeps its last word: closing the lock is not the person leaving")
    }
}
