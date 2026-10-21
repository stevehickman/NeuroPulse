package life.neurone.shared

/**
 * Whether the app starts by itself when the person signs in to their computer (the desktop apps). The automatic backup is only made while
 * the app is running (`SWR-MG-03`), so a computer that is restarted would otherwise go without it until the app is next opened. The
 * person chooses it; the app never turns it on for them, and turns it off with the automatic backup it exists for (`REQ-SW03-28`).
 */
interface LoginItem {
    /** Whether the system will start the app at sign-in. */
    val isOn: Boolean

    /** Ask the system to start the app at sign-in ([on]) or to stop. False when the system refused or the change did not take. */
    fun set(on: Boolean): Boolean
}

/**
 * The text the system reads to start the app at sign-in, one function per system, pure so they can be checked without the system. The app
 * is started with [BACKGROUND_ARGUMENT], which keeps its window out of sight (in the tray) so signing in does not open a window the person
 * did not ask for.
 */
object LoginItemFiles {
    /** What the host is started with at sign-in; it shows no window at start when the tray can bring one back. */
    const val BACKGROUND_ARGUMENT = "--background"

    /** The name the entry goes under: the plist's label, the registry value, the autostart file. */
    const val ENTRY_NAME = "life.neurone.NeurOne"

    /** Whether the app starts with no window: only when it was started at sign-in and a tray exists to bring the window back from. */
    fun startsInTray(args: List<String>, trayAvailable: Boolean): Boolean = BACKGROUND_ARGUMENT in args && trayAvailable

    /** macOS: a LaunchAgent that runs the app at login. */
    fun macPlist(executable: String): String = listOf(
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>",
        "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">",
        "<plist version=\"1.0\">",
        "<dict>",
        "  <key>Label</key><string>" + ENTRY_NAME + "</string>",
        "  <key>ProgramArguments</key>",
        "  <array><string>" + xml(executable) + "</string><string>" + BACKGROUND_ARGUMENT + "</string></array>",
        "  <key>RunAtLoad</key><true/>",
        "</dict>",
        "</plist>",
        "",
    ).joinToString("\n")

    /** Linux: a freedesktop autostart entry. */
    fun linuxEntry(executable: String): String = listOf(
        "[Desktop Entry]",
        "Type=Application",
        "Name=NeurOne",
        "Exec=" + desktopQuote(executable) + " " + BACKGROUND_ARGUMENT,
        "X-GNOME-Autostart-enabled=true",
        "",
    ).joinToString("\n")

    /** Windows: the command the Run key holds. The path is quoted, so a space in it is not read as the start of an argument. */
    fun windowsCommand(executable: String): String = "\"" + executable + "\" " + BACKGROUND_ARGUMENT

    private fun xml(text: String): String =
        text.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;").replace("\"", "&quot;")

    /** An Exec value is quoted, and `"`, `` ` ``, `$` and `\` inside it are escaped (the Desktop Entry specification). */
    private fun desktopQuote(text: String): String =
        "\"" + text.replace("\\", "\\\\").replace("\"", "\\\"").replace("`", "\\`").replace("$", "\\$") + "\""
}
