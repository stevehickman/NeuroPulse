package life.neurone.core.data

import kotlin.test.Test
import kotlin.test.assertContentEquals
import kotlin.test.assertEquals
import kotlin.test.assertIs
import kotlin.test.assertNotNull
import kotlin.test.assertNull

private class Counting : RandomSource {
    private var n = 7
    override fun nextBytes(count: Int): ByteArray = ByteArray(count) { (n++ % 251).toByte() }
}

/** A recording sealed and opened through the Rust core's archive, with the cutting into chunks done here. The format is tested in Rust. */
class RecordingArchiveTests {
    private val kdf = BundleKeyDerivation.newKdf(BundleKeyMethod.RECOVERY_CODE, Counting())
    private val secret = BundleKeyDerivation.newRecoveryCode(Counting()).secret
    private val key = BundleKeyDerivation.deriveKey(secret, kdf)

    private fun sample(len: Int) = ByteArray(len) { (it * 31 % 251).toByte() }

    // @verifies SWR-MG-03
    @Test fun aRecordingOfSeveralChunksRoundTripsAndANeighbouringSizeDoes() {
        val chunk = RecordingArchive.chunkBytes
        for (len in listOf(0, 1, chunk - 1, chunk, chunk + 1, 2 * chunk + 17)) {
            val plain = sample(len)
            val sealed = RecordingArchive.seal(plain, key, Counting())
            val opened = RecordingArchive.open(sealed, key)
            assertIs<ArchiveOpened.Ok>(opened, "length $len")
            assertContentEquals(plain, opened.data, "length $len")
        }
    }

    // @verifies SWR-MG-03
    @Test fun theKdfBlockIsReadBackFromTheHeaderAndAbsentWhenNoneWasGiven() {
        val with = RecordingArchive.seal(sample(10), key, Counting(), kdf)
        assertEquals(kdf, RecordingArchive.readKdf(with))
        assertIs<ArchiveOpened.Ok>(RecordingArchive.open(with, key))
        assertNull(RecordingArchive.readKdf(RecordingArchive.seal(sample(10), key, Counting())))
        assertNull(RecordingArchive.readKdf(ByteArray(40)), "not an archive")
    }

    // @verifies SWR-MG-03
    @Test fun anotherKeyAnEditAndACutAreRefusedAndTheReasonIsNotWhichOne() {
        val chunk = RecordingArchive.chunkBytes
        val sealed = RecordingArchive.seal(sample(3 * chunk), key, Counting())
        val other = BundleKeyDerivation.deriveKey(BundleKeyDerivation.newRecoveryCode(Counting().also { it.nextBytes(3) }).secret, kdf)
        assertEquals(ArchiveProblem.AUTHENTICATION_FAILED, (RecordingArchive.open(sealed, other) as ArchiveOpened.Refused).problem)
        val edited = sealed.copyOf().also { it[sealed.size / 2] = (it[sealed.size / 2] + 1).toByte() }
        assertEquals(ArchiveProblem.AUTHENTICATION_FAILED, (RecordingArchive.open(edited, key) as ArchiveOpened.Refused).problem)
        val cut = sealed.copyOf(sealed.size - (chunk + 16))
        assertEquals(ArchiveProblem.AUTHENTICATION_FAILED, (RecordingArchive.open(cut, key) as ArchiveOpened.Refused).problem)
        assertEquals(ArchiveProblem.MALFORMED, (RecordingArchive.open(ByteArray(40), key) as ArchiveOpened.Refused).problem)
        assertNotNull(sealed)
    }
}
