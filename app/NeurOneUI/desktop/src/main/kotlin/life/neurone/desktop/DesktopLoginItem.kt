package life.neurone.desktop

import life.neurone.shared.LoginItem
import life.neurone.shared.LoginItemFiles
import java.io.File

/**
 * Starts the desktop app when the person signs in, the way each system does it for an ordinary program: a LaunchAgent on macOS, a value in
 * the user's `Run` key on Windows, an autostart entry on Linux. Everything is the person's own (nothing is written outside their home or
 * their registry hive, and nothing needs an administrator).
 *
 * Only an installed copy can do it: the path to start comes from the packaged launcher (`jpackage.app-path`), which a development run
 * (`gradle run`) does not have, so [forThisSystem] gives null there and the choice is not offered.
 *
 * [run] runs a command and says whether it succeeded; it is a parameter so the Windows calls can be checked without a registry.
 */
class DesktopLoginItem(
    private val os: String,
    private val executable: String,
    private val home: File,
    private val run: (List<String>) -> Boolean = ::runCommand,
) : LoginItem {

    private val macFile get() = File(home, "Library/LaunchAgents/" + LoginItemFiles.ENTRY_NAME + ".plist")
    private val linuxFile get() = File(home, ".config/autostart/" + LoginItemFiles.ENTRY_NAME + ".desktop")
    private val runKey = "HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Run"

    override val isOn: Boolean
        get() = when {
            "mac" in os -> macFile.isFile
            "win" in os -> run(listOf("reg", "query", runKey, "/v", LoginItemFiles.ENTRY_NAME))
            else -> linuxFile.isFile
        }

    override fun set(on: Boolean): Boolean = try {
        when {
            "mac" in os -> writeOrDelete(macFile, on) { LoginItemFiles.macPlist(executable) }
            "win" in os ->
                if (on) run(listOf("reg", "add", runKey, "/v", LoginItemFiles.ENTRY_NAME, "/t", "REG_SZ", "/d", LoginItemFiles.windowsCommand(executable), "/f"))
                else !isOn || run(listOf("reg", "delete", runKey, "/v", LoginItemFiles.ENTRY_NAME, "/f"))
            else -> writeOrDelete(linuxFile, on) { LoginItemFiles.linuxEntry(executable) }
        } && isOn == on
    } catch (ex: java.io.IOException) {
        false
    }

    private fun writeOrDelete(file: File, on: Boolean, text: () -> String): Boolean {
        if (on) {
            file.parentFile?.mkdirs()
            file.writeText(text())
        } else {
            file.delete()
        }
        return true
    }

    companion object {
        /** The login item for the system the app is on, or null when this is not an installed copy and cannot say what to start. */
        fun forThisSystem(): DesktopLoginItem? {
            val launcher = System.getProperty("jpackage.app-path")?.takeIf { it.isNotBlank() } ?: return null
            return DesktopLoginItem(System.getProperty("os.name").lowercase(), launcher, File(System.getProperty("user.home")))
        }

        private fun runCommand(command: List<String>): Boolean = try {
            ProcessBuilder(command).redirectErrorStream(true).start().also { it.inputStream.readBytes() }.waitFor() == 0
        } catch (ex: java.io.IOException) {
            false
        }
    }
}
