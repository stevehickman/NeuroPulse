package life.neurone.core.models

import kotlin.test.Test
import kotlin.test.assertContentEquals
import kotlin.test.assertEquals
import kotlin.test.assertNull

// STEALTH (0x0022) as the app reads and writes it, and what the app asks for (SWR-IND-01, SWR-IND-02; OI-PRIV-37).
class StealthFlagTests {

    // @verifies SWR-IND-01#c
    @Test
    fun theWriteIsBitZeroAndNothingElse() {
        assertContentEquals(byteArrayOf(0x01), StealthFlag.toWire(true))
        assertContentEquals(byteArrayOf(0x00), StealthFlag.toWire(false))
    }

    // @verifies SWR-IND-01#c
    @Test
    fun aReadIsBelievedOnlyWhenItIsOneByteWithBitZeroAlone() {
        assertEquals(true, StealthFlag.fromWire(byteArrayOf(0x01)))
        assertEquals(false, StealthFlag.fromWire(byteArrayOf(0x00)))
        assertNull(StealthFlag.fromWire(byteArrayOf(0x02)))
        assertNull(StealthFlag.fromWire(byteArrayOf(0x81.toByte())))
        assertNull(StealthFlag.fromWire(byteArrayOf()))
        assertNull(StealthFlag.fromWire(byteArrayOf(0x01, 0x00)))
    }

    // @verifies SWR-IND-02#a
    @Test
    fun theOwnersLockOutranksTheProfile() {
        assertEquals(true, StealthFlag.desired(profileChoice = true, ownerForbids = false))
        assertEquals(false, StealthFlag.desired(profileChoice = true, ownerForbids = true))
        assertEquals(false, StealthFlag.desired(profileChoice = null, ownerForbids = true), "a lock with no one named still lights the helmet")
        assertNull(StealthFlag.desired(profileChoice = null, ownerForbids = false), "no one named and no lock: nothing is sent")
        assertEquals(false, StealthFlag.desired(profileChoice = false, ownerForbids = false))
    }
}
