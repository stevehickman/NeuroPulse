package life.neurone.desktop

import life.neurone.core.cloudauth.AuthorizationLauncher
import life.neurone.core.cloudauth.LoopbackAuthorizationLauncher

/**
 * Shows a cloud provider's sign-in page in the person's own browser (SWR-MG-19): the system's default browser through the command each
 * system has for it, never a window inside the app, so the app cannot read what is typed there and the person's existing sign-in to the
 * provider is used (RFC 8252 section 4). Only an `https` address is opened.
 *
 * The command is built as an argument list and never passed through a shell, so an address with `&` or quotes in it is one argument.
 * [start] runs it; it is a parameter so the commands can be checked without opening a browser.
 */
class DesktopBrowser(
    private val os: String,
    private val start: (List<String>) -> Unit = { ProcessBuilder(it).redirectErrorStream(true).start() },
) {
    fun open(address: String) {
        require(address.startsWith("https://")) { "a sign-in page is opened over https only" }
        start(command(address))
    }

    internal fun command(address: String): List<String> = when {
        "mac" in os -> listOf("open", address)
        "win" in os -> listOf("rundll32", "url.dll,FileProtocolHandler", address)
        else -> listOf("xdg-open", address)
    }

    companion object {
        /**
         * The launcher a desktop sign-in is given: the browser above, and the loopback listener that takes the answer. [donePage] is what
         * the browser shows afterwards; it is text the person reads, so the host supplies it from the locale files (`CLAUDE.md` §17).
         * Nothing constructs a `CloudAuth` yet.
         */
        fun launcher(donePage: String = ""): AuthorizationLauncher {
            val browser = DesktopBrowser(System.getProperty("os.name").lowercase())
            return LoopbackAuthorizationLauncher(donePage) { browser.open(it) }
        }
    }
}
