package life.neurone.core.common

import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertNull

/** The browser keeps the session history in memory and nowhere else (REQ-SW03-18 (b)). */
class MemoryOnlyKeysKeyValueStoreTests {
    private val history = "np.session.history"
    private val other = "np.limits.global"

    // @verifies REQ-SW03-18#b
    @Test
    fun aMemoryOnlyValueIsReadBackButNeverReachesTheHostStore() {
        val inner = InMemoryKeyValueStore()
        val s = MemoryOnlyKeysKeyValueStore(inner, setOf(history))
        s.putString(history, "[{\"sessionDay\":\"2026-10-18\"}]")
        assertEquals("[{\"sessionDay\":\"2026-10-18\"}]", s.getString(history))
        assertNull(inner.getString(history), "nothing is written under a memory-only key")
    }

    // @verifies REQ-SW03-18#b
    @Test
    fun aMemoryOnlyValueDoesNotSurviveARestart() {
        val inner = InMemoryKeyValueStore()
        MemoryOnlyKeysKeyValueStore(inner, setOf(history)).putString(history, "records")
        assertNull(MemoryOnlyKeysKeyValueStore(inner, setOf(history)).getString(history), "a new process starts without it")
    }

    // @verifies REQ-SW03-18#b
    @Test
    fun aCopyAnEarlierBuildLeftInTheHostStoreIsRemovedWhenTheStoreIsMade() {
        val inner = InMemoryKeyValueStore()
        inner.putString(history, "plaintext history from before")
        val s = MemoryOnlyKeysKeyValueStore(inner, setOf(history))
        assertNull(inner.getString(history), "the old plaintext copy is gone from the host store")
        assertNull(s.getString(history), "and is not served from it")
    }

    // @verifies REQ-SW03-18#b
    @Test
    fun everyOtherKeyGoesStraightToTheHostStore() {
        val inner = InMemoryKeyValueStore()
        val s = MemoryOnlyKeysKeyValueStore(inner, setOf(history))
        s.putString(other, "limits")
        s.putInt("n", 3)
        s.putBoolean("flag", true)
        assertEquals("limits", inner.getString(other))
        assertEquals(3, inner.getInt("n"))
        assertEquals(true, inner.getBoolean("flag"))
        assertEquals("limits", s.getString(other))
        s.remove(other)
        assertNull(inner.getString(other))
    }

    // @verifies REQ-SW03-18#b
    @Test
    fun removeClearsAMemoryOnlyValue() {
        val s = MemoryOnlyKeysKeyValueStore(InMemoryKeyValueStore(), setOf(history))
        s.putString(history, "records")
        s.remove(history)
        assertNull(s.getString(history))
    }

    // @verifies REQ-SW03-18#b
    @Test
    fun aMemoryOnlyNumberOrFlagIsHeldAsWrittenAndFallsBackToTheDefaultWhenAbsent() {
        val inner = InMemoryKeyValueStore()
        val s = MemoryOnlyKeysKeyValueStore(inner, setOf("a", "b"))
        assertEquals(7, s.getInt("a", 7))
        assertFalse(s.getBoolean("b", false))
        s.putInt("a", 9)
        s.putBoolean("b", true)
        assertEquals(9, s.getInt("a", 7))
        assertEquals(true, s.getBoolean("b", false))
        assertEquals(0, inner.getInt("a"), "neither reached the host store")
        assertFalse(inner.getBoolean("b"))
    }
}
