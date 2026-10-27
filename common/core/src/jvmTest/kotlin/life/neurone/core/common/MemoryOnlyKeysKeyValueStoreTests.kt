package life.neurone.core.common

import kotlinx.serialization.json.buildJsonObject
import kotlinx.serialization.json.jsonArray
import kotlinx.serialization.json.jsonPrimitive
import kotlinx.serialization.json.put
import life.neurone.core.npps.NppsCore
import kotlin.test.Test
import kotlin.test.assertTrue
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertNull

/** The browser keeps the app's state in memory and nowhere else (REQ-SW03-18 (b)). */
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

    // @verifies REQ-SW03-18#b
    @Test
    fun theBrowserKeepsEveryKeyTheCoreListsInMemoryAndWritesNoneOfThemToLocalStorage() {
        val keys = MemoryOnlyKeysKeyValueStore.browserKeys()
        val map = NppsCore.migration(buildJsonObject { put("op", "itemKeys") })
        fun list(name: String) = map[name]!!.jsonArray.map { it.jsonPrimitive.content }
        assertTrue(history in keys, "the session history")
        assertTrue(list("consent").all { it in keys }, "every consent key, including the contact the person gave")
        assertTrue(list("perInstall").all { it in keys }, "the per-install keys: a launch count, a path, a helmet id")
        assertTrue(list("unsorted").all { it in keys }, "the onboarding records (age, biometric-privacy consent) and the rest")
        assertTrue(map["carried"]!!.let { c -> listOf("protocols", "limitsProfiles", "characterisationEnrolment").flatMap { c.jsonObjectList(it) }.all { it in keys } },
            "saved protocols, limits and the research enrolment")
        assertEquals(map["sealedAtRest"]!!.jsonArray.map { it.jsonPrimitive.content }.toSet(), keys, "the core's one list, so a new key is covered")

        val inner = InMemoryKeyValueStore()
        val s = MemoryOnlyKeysKeyValueStore(inner, keys)
        for (k in keys) s.putString(k, "value of $k")
        assertTrue(keys.all { inner.getString(it) == null }, "nothing reaches the host store")
        assertTrue(keys.all { s.getString(it) == "value of $it" }, "and all of it is read back within the page")
    }

    private fun kotlinx.serialization.json.JsonElement.jsonObjectList(name: String): List<String> =
        (this as kotlinx.serialization.json.JsonObject)[name]!!.jsonArray.map { it.jsonPrimitive.content }
}
