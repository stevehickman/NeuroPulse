package life.neurone.core.protocol

import life.neurone.core.common.PresenceInstall
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertNull
import kotlin.test.assertTrue

/**
 * The wavelength rules store under the presence gate (`REQ-PRES-01` (d), `OI-PRIV-19`): built locked, the shipped windows apply
 * and the person's override must neither be replaced nor stay unread after the unlock.
 */
class NPWavelengthRulesStorePresenceTests {

    private val sealed = setOf(NPWavelengthRulesStore.USER_KEY)

    private val rule808 = NPWavelengthChannelRule(NPPBMChannelElement.LED_808, 808.0, 798.0, 860.0)
    private val rule660 = NPWavelengthChannelRule(NPPBMChannelElement.LED_660, 660.0, 655.0, 675.0)

    private fun installWithOverride(): PresenceInstall = PresenceInstall { sealed }.also { install ->
        val run = install.Run()
        run.unlock()
        val store = NPWavelengthRulesStore(run.kv, lock = run.lock)
        check(store.setChannel(rule808).isEmpty())
        run.lock.lock()
    }

    // @verifies REQ-PRES-01#d
    @Test fun the_override_applies_once_the_person_unlocks_a_store_built_locked() {
        val run = installWithOverride().Run()
        val store = NPWavelengthRulesStore(run.kv, lock = run.lock)

        assertFalse(store.isReadable)
        assertNull(store.userRules)
        assertTrue(store.editedChannels.isEmpty(), "locked, the shipped windows apply")

        run.unlock()

        assertEquals(setOf(NPPBMChannelElement.LED_808), store.editedChannels)
        assertEquals(NPPBMChannelElement.LED_808, NPWavelengthRulesEngine.map(850.0, store.resolved))
    }

    // @verifies REQ-PRES-01#d
    @Test fun a_change_made_while_locked_is_added_to_the_sealed_override_not_written_in_its_place() {
        val install = installWithOverride()
        val run = install.Run()
        val store = NPWavelengthRulesStore(run.kv, lock = run.lock)

        assertTrue(store.setChannel(rule660).isEmpty())

        run.unlock()

        assertEquals(setOf(NPPBMChannelElement.LED_808, NPPBMChannelElement.LED_660), store.editedChannels, "the sealed 808 window survived")

        run.lock.lock()
        val next = install.Run()
        val again = NPWavelengthRulesStore(next.kv, lock = next.lock)
        next.unlock()
        assertEquals(setOf(NPPBMChannelElement.LED_808, NPPBMChannelElement.LED_660), again.editedChannels)
    }

    // @verifies REQ-PRES-01#d
    @Test fun a_reset_made_while_locked_clears_the_sealed_override_at_the_unlock() {
        val install = installWithOverride()
        val run = install.Run()
        val store = NPWavelengthRulesStore(run.kv, lock = run.lock)

        store.resetChannel()

        run.unlock()

        assertTrue(store.editedChannels.isEmpty())
        assertNull(store.userRules)
    }
}
