package life.neurone.core.analytics

import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertNotEquals
import kotlin.test.assertNull
import kotlin.test.assertTrue

/** The per-helmet device tokens are keyed by a salted hash, never by the helmet's hardware address (`REQ-SHDR-05` (d)). */
class HubTokenTableTests {
    private val mac = "AA:BB:CC:00:11:22"
    private val otherMac = "AA:BB:CC:00:11:23"

    private class Holder(var text: String? = null)

    private fun table(h: Holder, fill: Byte = 7) = HubTokenTable({ h.text }, { h.text = it }, { ByteArray(it) { fill } })

    // @verifies REQ-SHDR-05#d
    @Test fun aTokenComesBackForItsHelmetAndNotForAnother() {
        val h = Holder()
        val t = table(h)
        t.save(mac, "tok-a")
        t.save(otherMac, "tok-b")
        assertEquals("tok-a", t.load(mac))
        assertEquals("tok-b", t.load(otherMac))
        assertNull(t.load("AA:BB:CC:99:99:99"))
    }

    // @verifies REQ-SHDR-05#d
    @Test fun theHardwareAddressIsNotInTheStoredValue() {
        val h = Holder()
        table(h).save(mac, "tok-a")
        val stored = h.text!!
        assertFalse(stored.contains(mac), "the address is not stored")
        assertFalse(stored.lowercase().contains(mac.lowercase().replace(":", "")), "nor its digits")
        assertTrue(stored.contains("tok-a"))
    }

    // @verifies REQ-SHDR-05#d
    @Test fun theKeyIsSaltedSoTwoInstallsKeyOneHelmetDifferently() {
        val a = Holder()
        val b = Holder()
        table(a, fill = 1).save(mac, "t")
        table(b, fill = 2).save(mac, "t")
        val keyOf = { h: Holder -> h.text!!.lines().first { !it.startsWith("#salt") }.substringBefore('\t') }
        assertNotEquals(keyOf(a), keyOf(b))
        assertEquals(64, keyOf(a).length)
    }

    // @verifies REQ-SHDR-05#d
    @Test fun theSaltIsMadeOnceAndKept() {
        val h = Holder()
        val t = table(h)
        t.save(mac, "a")
        val salt = h.text!!.lines().first { it.startsWith("#salt") }
        t.save(otherMac, "b")
        assertEquals(salt, h.text!!.lines().first { it.startsWith("#salt") })
        assertEquals("a", table(h, fill = 99).load(mac), "a later table over the same value keeps its salt")
    }

    // OI-PRIV-08 (4): no raw address waits for its helmet to reconnect.
    // @verifies REQ-SHDR-05#d
    @Test fun everyLineWrittenUnderARawAddressIsRewrittenAtLaunchWithoutAnyHelmetConnecting() {
        val h = Holder("$mac\tlegacy-token\n$otherMac\tother-legacy")
        val t = table(h)
        t.migrateLegacy()
        val after = h.text!!
        assertFalse(after.contains(mac) || after.contains(otherMac), "no raw address is left, for either helmet")
        assertTrue(after.lines().first().matches(Regex("#salt\t[0-9a-f]{64}")), "a salt is made if there was none")
        assertTrue(after.lines().drop(1).all { it.substringBefore('\t').matches(Regex("[0-9a-f]{64}")) })
        assertEquals("legacy-token", t.load(mac), "and each token still reads for its helmet")
        assertEquals("other-legacy", t.load(otherMac))
    }

    // @verifies REQ-SHDR-05#d
    @Test fun aLoadOrASaveForOneHelmetAlsoRewritesTheOthersRawLines() {
        val viaLoad = Holder("$mac\tlegacy-token\n$otherMac\tother-legacy")
        assertEquals("legacy-token", table(viaLoad).load(mac))
        assertFalse(viaLoad.text!!.contains(otherMac), "a helmet that has not reconnected does not keep its line")

        val viaSave = Holder("$otherMac\tother-legacy")
        table(viaSave).save(mac, "new")
        assertFalse(viaSave.text!!.contains(otherMac))
        assertEquals("other-legacy", table(viaSave).load(otherMac))
    }

    // @verifies REQ-SHDR-05#d
    @Test fun aMigrationKeepsTheSaltAndTheLaterHashedTokenAndWritesNothingWhenThereIsNothingToMove() {
        val h = Holder()
        val t = table(h)
        t.save(mac, "hashed-and-later")
        val salt = h.text!!.lines().first()
        h.text = h.text + "\n$mac\tstale-raw\n$otherMac\tother-legacy"
        t.migrateLegacy()
        assertEquals(salt, h.text!!.lines().first(), "the salt is kept")
        assertEquals("hashed-and-later", t.load(mac), "a helmet that already has a hashed line keeps it")
        assertEquals("other-legacy", t.load(otherMac))

        var writes = 0
        val quiet = HubTokenTable({ h.text }, { writes++; h.text = it }, { ByteArray(it) { 7 } })
        quiet.migrateLegacy()
        quiet.load(mac)
        assertEquals(0, writes, "nothing to move, nothing written")
        HubTokenTable({ null }, { writes++ }, { ByteArray(it) { 7 } }).migrateLegacy()
        assertEquals(0, writes, "a value that cannot be read yet is left alone")
    }

    // @verifies REQ-SHDR-05#a
    @Test fun anUpgradedTokenReplacesTheOldOneForThatHelmetOnly() {
        val h = Holder()
        val t = table(h)
        t.save(mac, "interim")
        t.save(otherMac, "keep")
        t.save(mac, "hub-token")
        assertEquals("hub-token", t.load(mac))
        assertEquals("keep", t.load(otherMac))
    }

    // @verifies REQ-SHDR-05#d
    @Test fun malformedLinesAndANonHexSaltAreIgnoredNotTrusted() {
        val h = Holder("#salt\tnot-hex\njunk\n\t\nx\ty\tz")
        val t = table(h)
        assertNull(t.load(mac))
        t.save(mac, "t")
        assertEquals("t", t.load(mac))
        assertTrue(h.text!!.lines().first().matches(Regex("#salt\t[0-9a-f]{64}")))
    }
}
