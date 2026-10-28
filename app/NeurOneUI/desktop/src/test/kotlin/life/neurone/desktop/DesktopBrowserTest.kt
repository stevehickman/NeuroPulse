package life.neurone.desktop

import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertTrue

/** The command each system gets for showing a sign-in page, against a recording stand-in for starting a process. */
class DesktopBrowserTest {
    private val ran = mutableListOf<List<String>>()
    private val url = "https://accounts.example/auth?client_id=a&scope=x%20y&redirect_uri=http%3A%2F%2F127.0.0.1%3A9004"

    // @verifies SWR-MG-19#i
    @Test fun each_system_gets_its_own_command_with_the_address_as_one_argument() {
        DesktopBrowser("mac os x") { ran += it }.open(url)
        DesktopBrowser("windows 11") { ran += it }.open(url)
        DesktopBrowser("linux") { ran += it }.open(url)
        assertEquals(
            listOf(listOf("open", url), listOf("rundll32", "url.dll,FileProtocolHandler", url), listOf("xdg-open", url)),
            ran,
        )
    }

    // @verifies SWR-MG-19#i
    @Test fun only_an_https_address_is_opened_and_nothing_runs_for_another() {
        val browser = DesktopBrowser("linux") { ran += it }
        for (bad in listOf("http://accounts.example/auth", "file:///etc/passwd", "javascript:alert(1)", "-c evil", "")) {
            assertFailsWith<IllegalArgumentException>(bad) { browser.open(bad) }
        }
        assertTrue(ran.isEmpty())
    }
}
