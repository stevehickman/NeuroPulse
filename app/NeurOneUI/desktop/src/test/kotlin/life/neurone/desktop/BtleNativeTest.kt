package life.neurone.desktop

import kotlin.test.Test
import kotlin.test.assertNotNull
import kotlin.test.assertTrue

/**
 * Loads the real library: proves the JNI symbol names match [BtleNative], that a central starts and
 * stops, and that polling times out rather than blocks. It needs no radio; on a machine with no
 * adapter the library reports Bluetooth off, which is an answer, not a failure.
 */
class BtleNativeTest {
    @Test fun the_library_loads_starts_reports_an_adapter_state_and_stops() {
        val link = assertNotNull(JniBtleLink.open(), "libneurone_btle_jni was not found on java.library.path")
        link.refresh()
        val deadline = System.nanoTime() + 5_000_000_000L
        var first: ByteArray? = null
        while (first == null && System.nanoTime() < deadline) first = link.poll(250)
        link.close()
        val event = assertNotNull(first, "no adapter state within 5 s")
        assertTrue(event[0].toInt() == 1 && event[1].toInt() in 0..3, "expected an adapter-state event")
    }
}
