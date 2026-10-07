package life.neurone.core.platform

import life.neurone.core.common.UUID
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertNotEquals

/**
 * The digests and the UUID are written once in common code, so they are pinned to published
 * vectors: if a target computed them differently, a descriptor hash or a derived id would differ
 * between the apps that must agree on it.
 */
class DigestsAndUuidTests {

    @Test fun sha256_matchesFips180Vectors() {
        assertEquals(
            "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
            Digests.sha256(ByteArray(0)).toHex(),
        )
        assertEquals(
            "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
            Digests.sha256("abc".encodeToByteArray()).toHex(),
        )
        // Two blocks: the 56-byte message forces the length into a second block.
        assertEquals(
            "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1",
            Digests.sha256("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq".encodeToByteArray()).toHex(),
        )
    }

    @Test fun md5_matchesRfc1321Vectors() {
        assertEquals("d41d8cd98f00b204e9800998ecf8427e", Digests.md5(ByteArray(0)).toHex())
        assertEquals("900150983cd24fb0d6963f7d28e17f72", Digests.md5("abc".encodeToByteArray()).toHex())
        assertEquals(
            "57edf4a22be3c955ac49da2e2107b67a",
            Digests.md5("12345678901234567890123456789012345678901234567890123456789012345678901234567890".encodeToByteArray()).toHex(),
        )
    }

    @Test fun uuid_roundTripsAndIsVersion4() {
        val id = UUID.randomUUID()
        assertEquals(id, UUID.fromString(id.toString()))
        assertEquals('4', id.toString()[14])
        assertNotEquals(id, UUID.randomUUID())
    }

    @Test fun uuid_nameBased_matchesRfc4122AndJavaDerivation() {
        // java.util.UUID.nameUUIDFromBytes("zone:frontal".getBytes()) — version 3, MD5.
        val a = UUID.nameUUIDFromBytes("zone:frontal".encodeToByteArray())
        assertEquals(a, UUID.nameUUIDFromBytes("zone:frontal".encodeToByteArray()))
        assertEquals('3', a.toString()[14])
        // RFC 4122 appendix C: the DNS-namespace UUID's v3 form of "python.org" is 6fa459ea-ee8a-3ca4-894e-db77e160355e.
        val ns = UUID.fromString("6ba7b810-9dad-11d1-80b4-00c04fd430c8").toBytes()
        assertEquals(
            "6fa459ea-ee8a-3ca4-894e-db77e160355e",
            UUID.nameUUIDFromBytes(ns + "python.org".encodeToByteArray()).toString(),
        )
    }

    @Test fun uuid_fromString_refusesMalformedText() {
        assertFailsWith<IllegalArgumentException> { UUID.fromString("not-a-uuid") }
        assertFailsWith<IllegalArgumentException> { UUID.fromString("6ba7b810-9dad-11d1-80b4-00c04fd430cZ") }
    }
}

class FormatFixedTests {
    @Test fun roundsHalfUpAndPadsDecimals() {
        assertEquals("0.3", formatFixed(0.25, 1))
        assertEquals("72.0", formatFixed(72.0, 1))
        assertEquals("1.50", formatFixed(1.5, 2))
        assertEquals("-0.3", formatFixed(-0.25, 1))
        assertEquals("0.0", formatFixed(-0.04, 1))
        assertEquals("3", formatFixed(2.5, 0))
    }
}
