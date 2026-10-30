package life.neurone.core.cloud

import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.async
import kotlinx.coroutines.awaitAll
import kotlinx.coroutines.runBlocking
import life.neurone.core.migration.ObjectStore
import kotlin.test.Test
import kotlin.test.assertContentEquals
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertNull
import kotlin.test.assertTrue

/**
 * The contract every [ObjectStore] meets, whatever it is backed by (SWR-MG-11, `docs/np_sw_cloudstore_001.md` §5): a provider
 * adapter is added by subclassing this with its store, and the same tests run against it. Run against a fake transport here; the
 * run against a real account of each provider is still owed (the suite's own claim test is the "two simultaneous claim writes"
 * the requirement names).
 */
abstract class ObjectStoreContract {
    protected abstract fun newStore(): ObjectStore

    /**
     * False for a store that cannot give an atomic create-if-absent and says so by throwing [CloudStoreException] from it
     * (Google Drive, `SWR-MG-07`): the claim tests then check the throw and the rest of the suite runs unchanged.
     */
    protected open val claimsAreAtomic: Boolean = true

    /**
     * False for a real server that does not arbitrate racing conditional creates (the live WebDAV run against wsgidav: two of 24 won). The race test then
     * checks only that some create won and that what is stored is one writer's bytes whole; it no longer asserts that exactly one won.
     */
    protected open val arbitratesRacingCreates: Boolean = true

    private suspend fun refusesToClaim(s: ObjectStore): Boolean {
        if (claimsAreAtomic) return false
        kotlin.test.assertFailsWith<CloudStoreException> { s.createIfAbsent("np-migration-claim.json", byteArrayOf(1)) }
        assertNull(s.get("np-migration-claim.json"), "a refused claim writes nothing")
        return true
    }

    // @verifies SWR-MG-11#calls
    @Test
    fun aMissingObjectReadsAsNullAndDeletingItIsNotAnError() = runBlocking {
        val s = newStore()
        assertNull(s.get("np-migration-bundle.npbk"))
        s.delete("np-migration-bundle.npbk")
    }

    // @verifies SWR-MG-11#calls
    @Test
    fun putThenGetReturnsTheSameBytesAndAPutReplaces() = runBlocking {
        val s = newStore()
        val first = ByteArray(5000) { (it * 31).toByte() } // every byte value, not text
        s.put("np-migration-bundle.npbk", first)
        assertContentEquals(first, s.get("np-migration-bundle.npbk"))
        s.put("np-migration-bundle.npbk", byteArrayOf(1, 2, 3))
        assertContentEquals(byteArrayOf(1, 2, 3), s.get("np-migration-bundle.npbk"))
    }

    // @verifies SWR-MG-11#calls
    @Test
    fun aSecondCreateIsRefusedAndLeavesTheFirstWritersBytes() = runBlocking {
        val s = newStore()
        if (refusesToClaim(s)) return@runBlocking
        assertTrue(s.createIfAbsent("np-migration-claim.json", "A".encodeToByteArray()))
        assertFalse(s.createIfAbsent("np-migration-claim.json", "B".encodeToByteArray()))
        assertEquals("A", s.get("np-migration-claim.json")!!.decodeToString())
    }

    // @verifies SWR-MG-11#calls
    @Test
    fun aReleasedClaimCanBeClaimedAgain() = runBlocking {
        val s = newStore()
        if (refusesToClaim(s)) return@runBlocking
        assertTrue(s.createIfAbsent("np-migration-claim.json", "A".encodeToByteArray()))
        s.delete("np-migration-claim.json")
        assertNull(s.get("np-migration-claim.json"))
        assertTrue(s.createIfAbsent("np-migration-claim.json", "B".encodeToByteArray()))
    }

    // @verifies SWR-MG-07
    @Test
    fun ofManySimultaneousCreatesExactlyOneWins() = runBlocking {
        val s = newStore()
        if (refusesToClaim(s)) return@runBlocking
        val won = (1..24).map { i -> async(Dispatchers.Default) { s.createIfAbsent("np-migration-claim.json", byteArrayOf(i.toByte())) } }.awaitAll()
        if (arbitratesRacingCreates) {
            assertEquals(1, won.count { it })
            val winner = won.indexOf(true) + 1
            assertContentEquals(byteArrayOf(winner.toByte()), s.get("np-migration-claim.json"))
        } else {
            assertTrue(won.any { it }, "some create must win")
            val stored = s.get("np-migration-claim.json")!!
            assertEquals(1, stored.size)
            assertTrue(stored[0] in 1..24, "what is stored is one writer's bytes")
        }
    }

    // @verifies SWR-MG-11#calls
    @Test
    fun listReturnsOnlyTheNamesUnderThePrefixWhateverThePageSize() = runBlocking {
        val s = newStore()
        for (i in 1..5) s.put("np-probe-$i", byteArrayOf(i.toByte()))
        s.put("np-migration-claim.json", byteArrayOf(9))
        assertEquals((1..5).map { "np-probe-$it" }.sorted(), s.list("np-probe-").sorted())
        assertEquals(emptyList(), s.list("zz-"))
    }

    // @verifies SWR-MG-11#calls
    @Test
    fun aNameWithSpacesSlashesAndPlusSignsRoundTrips() = runBlocking {
        val s = newStore()
        s.put("dir/a b+c", byteArrayOf(7))
        assertContentEquals(byteArrayOf(7), s.get("dir/a b+c"))
        assertEquals(listOf("dir/a b+c"), s.list("dir/"))
    }
}
