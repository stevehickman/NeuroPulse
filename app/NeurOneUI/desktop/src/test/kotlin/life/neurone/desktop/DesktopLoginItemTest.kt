package life.neurone.desktop

import java.io.File
import java.nio.file.Files
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertTrue

/** The desktop start-at-sign-in entry for each system, against a temporary home folder and a recording stand-in for the registry. */
class DesktopLoginItemTest {

    private fun home(): File = Files.createTempDirectory("neurone-login").toFile()

    // @verifies REQ-SW03-28#e
    @Test fun macos_writes_and_removes_a_launch_agent_that_starts_the_app_in_the_background() {
        val home = home()
        val item = DesktopLoginItem("mac os x", "/Applications/NeurOne.app/Contents/MacOS/NeurOne", home)
        assertFalse(item.isOn)
        assertTrue(item.set(true))
        assertTrue(item.isOn)
        val plist = File(home, "Library/LaunchAgents/life.neurone.NeurOne.plist").readText()
        assertTrue("/Applications/NeurOne.app/Contents/MacOS/NeurOne</string><string>--background" in plist)
        assertTrue(item.set(false))
        assertFalse(item.isOn)
        assertFalse(File(home, "Library/LaunchAgents/life.neurone.NeurOne.plist").exists())
    }

    // @verifies REQ-SW03-28#e
    @Test fun linux_writes_and_removes_an_autostart_entry() {
        val home = home()
        val item = DesktopLoginItem("linux", "/opt/neurone/bin/NeurOne", home)
        assertTrue(item.set(true))
        assertTrue("Exec=\"/opt/neurone/bin/NeurOne\" --background" in File(home, ".config/autostart/life.neurone.NeurOne.desktop").readText())
        assertTrue(item.set(false))
        assertFalse(item.isOn)
    }

    // @verifies REQ-SW03-28#e
    @Test fun windows_adds_and_deletes_a_run_key_value_with_the_quoted_path() {
        val ran = mutableListOf<List<String>>()
        var present = false
        val item = DesktopLoginItem("windows 11", "C:\\Program Files\\NeurOne\\NeurOne.exe", home()) { command ->
            ran += command
            when (command[1]) {
                "add" -> { present = true; true }
                "delete" -> { present = false; true }
                else -> present
            }
        }
        assertTrue(item.set(true))
        val add = ran.first { it[1] == "add" }
        assertEquals("\"C:\\Program Files\\NeurOne\\NeurOne.exe\" --background", add[add.indexOf("/d") + 1])
        assertTrue(item.set(false))
        assertFalse(item.isOn)
    }
}
