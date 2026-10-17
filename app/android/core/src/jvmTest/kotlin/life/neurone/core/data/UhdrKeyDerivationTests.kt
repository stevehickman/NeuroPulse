package life.neurone.core.data

import kotlin.test.Test
import kotlin.test.assertContentEquals
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertNotEquals

// The derivation itself is the shared core's (`bundle_key.rs`, which pins the locked Argon2id parameters against the crate
// called directly); what is checked here is that Kotlin reaches it, splits the result, and refuses what it must.
class UhdrKeyDerivationTests {

    // @verifies REQ-SW03-08
    @Test
    fun theKeyIsDerivedByTheCoreAndFollowsTheSeedAndTheSalt() {
        val seed = ByteArray(32) { 1 }
        val salt = ByteArray(32) { 2 }
        val a = UhdrKeyDerivation.deriveKey(seed, salt)
        val again = UhdrKeyDerivation.deriveKey(seed, salt)
        assertContentEquals(a.k1, again.k1, "deterministic")
        assertContentEquals(a.k2, again.k2)
        assertNotEquals(a.k1.toList(), UhdrKeyDerivation.deriveKey(ByteArray(32) { 3 }, salt).k1.toList(), "another seed")
        assertNotEquals(a.k1.toList(), UhdrKeyDerivation.deriveKey(seed, ByteArray(32) { 4 }).k1.toList(), "another salt")
    }

    // @verifies REQ-SW03-08
    @Test
    fun keySplitsIntoK1AndK2() {
        val key = UhdrKeyDerivation.deriveKey(ByteArray(32) { 1 }, ByteArray(32) { 2 })
        assertEquals(32, key.k1.size)
        assertEquals(32, key.k2.size)
        assertNotEquals(key.k1.toList(), key.k2.toList(), "the two halves are not one repeated")
    }

    // @verifies REQ-SW03-08
    @Test
    fun destroyedKeyRefusesAccess() {
        val key = UhdrKeyDerivation.deriveKey(ByteArray(32) { 1 }, ByteArray(32) { 2 })
        key.destroy()
        assertFailsWith<IllegalStateException> { key.k1 }
    }

    // @verifies REQ-SW03-08
    @Test
    fun invalidSaltOrSeedIsRejected() {
        assertFailsWith<IllegalArgumentException> { UhdrKeyDerivation.deriveKey(ByteArray(32) { 1 }, ByteArray(16)) }
        assertFailsWith<IllegalArgumentException> { UhdrKeyDerivation.deriveKey(ByteArray(0), ByteArray(32)) }
    }
}
