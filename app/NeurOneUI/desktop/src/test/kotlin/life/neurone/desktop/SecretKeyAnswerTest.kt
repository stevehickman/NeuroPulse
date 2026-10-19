package life.neurone.desktop

import life.neurone.core.common.SecretKeyUnavailableException
import life.neurone.desktop.DesktopSecretKeyStore.Os
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertNull

class SecretKeyAnswerTest {
    private val hex = "0123456789abcdef".repeat(4)

    // @verifies SWR-MG-15#b
    @Test
    fun onlyTheToolsOwnNotFoundIsNoKeyAndEveryOtherFailureIsCouldNotAsk() {
        assertNull(DesktopSecretKeyStore.interpretLoad(Os.MAC, 44, "", "not found"))
        assertNull(DesktopSecretKeyStore.interpretLoad(Os.LINUX, 1, "", ""))
        assertNull(DesktopSecretKeyStore.interpretLoad(Os.WINDOWS, 4, "", ""))
        assertFailsWith<SecretKeyUnavailableException> { DesktopSecretKeyStore.interpretLoad(Os.MAC, 36, "", "user interaction is not allowed") }
        assertFailsWith<SecretKeyUnavailableException> { DesktopSecretKeyStore.interpretLoad(Os.MAC, -1, "", "") }
        assertFailsWith<SecretKeyUnavailableException> { DesktopSecretKeyStore.interpretLoad(Os.LINUX, 1, "", "Cannot autolaunch D-Bus") }
        assertFailsWith<SecretKeyUnavailableException> { DesktopSecretKeyStore.interpretLoad(Os.LINUX, -1, "", "") }
        assertFailsWith<SecretKeyUnavailableException> { DesktopSecretKeyStore.interpretLoad(Os.WINDOWS, 3, "", "") }
        assertFailsWith<SecretKeyUnavailableException> { DesktopSecretKeyStore.interpretLoad(Os.WINDOWS, -1, "", "") }
    }

    // @verifies SWR-MG-15#b
    @Test
    fun aKeyIsReadAndAnAnswerThatIsNotAKeyIsCouldNotRead() {
        val key = DesktopSecretKeyStore.interpretLoad(Os.MAC, 0, "$hex\n", "")!!
        assertEquals(32, key.size)
        assertEquals(0x01.toByte(), key[0])
        assertEquals(0xef.toByte(), key[3])
        assertFailsWith<SecretKeyUnavailableException> { DesktopSecretKeyStore.interpretLoad(Os.MAC, 0, "garbage", "") }
        assertFailsWith<SecretKeyUnavailableException> { DesktopSecretKeyStore.interpretLoad(Os.MAC, 0, "", "") }
    }
}
