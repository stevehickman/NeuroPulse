package life.neurone.core.data

import kotlin.test.Test
import kotlin.test.assertContentEquals
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith

private class FakeArgon2 : Argon2Provider {
    var lastMemoryKiB = 0
    var lastIterations = 0
    var lastParallelism = 0
    var lastOutputLength = 0

    override fun argon2idHash(
        password: ByteArray,
        salt: ByteArray,
        memoryKiB: Int,
        iterations: Int,
        parallelism: Int,
        outputLength: Int,
    ): ByteArray {
        lastMemoryKiB = memoryKiB
        lastIterations = iterations
        lastParallelism = parallelism
        lastOutputLength = outputLength
        return ByteArray(outputLength) { (it % 251).toByte() }
    }
}

class UhdrKeyDerivationTests {

    // ISC-40: locked RFC 9106 / NP-FW-EMMC-001 §6 parameters reach the provider.
    @Test
    fun derivationUsesLockedArgon2idParameters() {
        val fake = FakeArgon2()
        UhdrKeyDerivation.deriveKey(fake, ByteArray(32) { 1 }, ByteArray(32) { 2 })
        assertEquals(65_536, fake.lastMemoryKiB)
        assertEquals(4, fake.lastIterations)
        assertEquals(1, fake.lastParallelism)
        assertEquals(64, fake.lastOutputLength)
    }

    @Test
    fun keySplitsIntoK1AndK2() {
        val key = UhdrKeyDerivation.deriveKey(FakeArgon2(), ByteArray(32) { 1 }, ByteArray(32) { 2 })
        assertEquals(32, key.k1.size)
        assertEquals(32, key.k2.size)
        assertContentEquals(ByteArray(32) { (it % 251).toByte() }, key.k1)
    }

    @Test
    fun destroyedKeyRefusesAccess() {
        val key = UhdrKeyDerivation.deriveKey(FakeArgon2(), ByteArray(32) { 1 }, ByteArray(32) { 2 })
        key.destroy()
        assertFailsWith<IllegalStateException> { key.k1 }
    }

    @Test
    fun invalidSaltOrSeedIsRejected() {
        assertFailsWith<IllegalArgumentException> {
            UhdrKeyDerivation.deriveKey(FakeArgon2(), ByteArray(32) { 1 }, ByteArray(16))
        }
        assertFailsWith<IllegalArgumentException> {
            UhdrKeyDerivation.deriveKey(FakeArgon2(), ByteArray(0), ByteArray(32))
        }
    }
}
