package life.neurone.desktop

import life.neurone.core.common.SecretKeyStore
import life.neurone.core.common.SecretKeyUnavailableException
import java.util.concurrent.TimeUnit

/**
 * The data key that protects the app's UHDR-class state (SWR-MG-06), kept in the operating system's own credential store: the
 * macOS Keychain (`security`), the Windows Credential Locker (Windows PowerShell) or the freedesktop Secret Service
 * (`secret-tool`). Destroying the key removes the credential there, so the sealed data is unreadable and not merely a file
 * gone. The key is passed to the tool on its standard input where the tool allows it; macOS's `security` takes it as an
 * argument, which another process of the same user could read for the moment the command runs.
 *
 * Whether the tool exists and works is not assumed: [create] stores, reads and removes a probe secret and returns null if any
 * step fails, and the app then keeps its data unencrypted and reports that it cannot clear itself.
 */
class DesktopSecretKeyStore private constructor(private val os: Os, private val service: String) : SecretKeyStore {
    enum class Os { MAC, WINDOWS, LINUX }

    private class Result(val exit: Int, val out: String, val err: String = "")

    private fun run(command: List<String>, stdin: String? = null): Result {
        val process = ProcessBuilder(command).redirectErrorStream(false).start()
        if (stdin != null) process.outputStream.use { it.write(stdin.toByteArray()) } else process.outputStream.close()
        val out = process.inputStream.bufferedReader().readText()
        val err = process.errorStream.bufferedReader().readText()
        if (!process.waitFor(TIMEOUT_SECONDS, TimeUnit.SECONDS)) {
            process.destroyForcibly()
            return Result(TIMED_OUT, "", err)
        }
        return Result(process.exitValue(), out, err)
    }

    private fun powershell(script: String, stdin: String? = null): Result =
        run(listOf("powershell.exe", "-NoProfile", "-NonInteractive", "-Command", VAULT + script), stdin)

    override fun load(): ByteArray? {
        val r = try {
            when (os) {
                Os.MAC -> run(listOf("security", "find-generic-password", "-a", ACCOUNT, "-s", service, "-w"))
                Os.LINUX -> run(listOf("secret-tool", "lookup", "service", service, "account", ACCOUNT))
                Os.WINDOWS -> powershell(
                    "try { \$c = \$v.Retrieve('$service','$ACCOUNT'); \$c.RetrievePassword(); [Console]::Out.Write(\$c.Password) } " +
                        "catch { if (\$_.Exception.HResult -eq $HRESULT_NOT_FOUND) { exit $EXIT_NOT_FOUND } else { exit 3 } }",
                )
            }
        } catch (ex: java.io.IOException) {
            throw SecretKeyUnavailableException("the system credential tool could not be run", ex)
        }
        return interpretLoad(os, r.exit, r.out, r.err)
    }

    override fun store(key: ByteArray) {
        val hex = key.joinToString("") { "%02x".format(it) }
        val r = when (os) {
            Os.MAC -> run(listOf("security", "add-generic-password", "-U", "-a", ACCOUNT, "-s", service, "-w", hex))
            Os.LINUX -> run(listOf("secret-tool", "store", "--label=NeurOne data key", "service", service, "account", ACCOUNT), hex)
            Os.WINDOWS -> powershell(
                "\$s = [Console]::In.ReadToEnd().Trim(); " +
                    "try { \$v.Remove(\$v.Retrieve('$service','$ACCOUNT')) } catch {}; " +
                    "\$v.Add((New-Object Windows.Security.Credentials.PasswordCredential '$service','$ACCOUNT',\$s))",
                hex,
            )
        }
        check(r.exit == 0) { "could not keep the data key in the system credential store (exit ${r.exit})" }
    }

    override fun destroy() {
        when (os) {
            Os.MAC -> run(listOf("security", "delete-generic-password", "-a", ACCOUNT, "-s", service))
            Os.LINUX -> run(listOf("secret-tool", "clear", "service", service, "account", ACCOUNT))
            Os.WINDOWS -> powershell("try { \$v.Remove(\$v.Retrieve('$service','$ACCOUNT')) } catch {}")
        }
        // Removing a credential that is already gone is not an error, so success is judged by the key being gone.
        check(load() == null) { "the data key is still in the system credential store" }
    }

    companion object {
        private const val TIMED_OUT = -1
        private const val EXIT_NOT_FOUND = 4
        private const val HRESULT_NOT_FOUND = -2147023728 // 0x80070490, ERROR_NOT_FOUND
        private const val MAC_ITEM_NOT_FOUND = 44 // errSecItemNotFound, as `security` exits
        private const val ACCOUNT = "np-data-key"
        private const val SERVICE = "life.neurone.data-key"
        private const val TIMEOUT_SECONDS = 20L
        private const val KEY_HEX_CHARS = 64

        // The Credential Locker is a Windows Runtime type, reached from Windows PowerShell and not from PowerShell 7.
        private const val VAULT =
            "[void][Windows.Security.Credentials.PasswordVault,Windows.Security.Credentials,ContentType=WindowsRuntime]; " +
                "\$v = New-Object Windows.Security.Credentials.PasswordVault; "

        /**
         * What a credential tool's answer means for the data key: the key, null if the tool says there is none, and a
         * [SecretKeyUnavailableException] for everything else (a timeout, a locked keychain, an unreadable answer). Only the tool's
         * own "not found" is "no key": a failure taken for it would let a new key replace the real one (OI-DESKTOP-01, item 3).
         */
        internal fun interpretLoad(os: Os, exit: Int, out: String, err: String): ByteArray? {
            val notFound = when (os) {
                Os.MAC -> exit == MAC_ITEM_NOT_FOUND
                Os.LINUX -> exit == 1 && err.isBlank() && out.isBlank() // `secret-tool lookup` says nothing when there is no item
                Os.WINDOWS -> exit == EXIT_NOT_FOUND
            }
            if (exit != 0) {
                if (notFound) return null
                throw SecretKeyUnavailableException("the system credential store did not answer (exit $exit)")
            }
            val hex = out.trim()
            if (hex.length != KEY_HEX_CHARS || !hex.all { it in "0123456789abcdef" }) {
                throw SecretKeyUnavailableException("the system credential store gave an answer that is not a data key")
            }
            return ByteArray(KEY_HEX_CHARS / 2) { hex.substring(it * 2, it * 2 + 2).toInt(16).toByte() }
        }

        fun osOf(name: String = System.getProperty("os.name").lowercase()): Os = when {
            "mac" in name -> Os.MAC
            "win" in name -> Os.WINDOWS
            else -> Os.LINUX
        }

        /** The store for this machine, or null when its credential store cannot keep, return and remove a secret. */
        fun create(os: Os = osOf()): SecretKeyStore? {
            val probe = DesktopSecretKeyStore(os, "$SERVICE.probe")
            val sample = ByteArray(KEY_HEX_CHARS / 2) { (it * 7 + 3).toByte() }
            return try {
                probe.store(sample)
                val ok = probe.load()?.contentEquals(sample) == true
                probe.destroy()
                if (ok) DesktopSecretKeyStore(os, SERVICE) else null
            } catch (ignored: Exception) {
                // No tool, no unlocked keychain, no desktop session: the app then holds its data as before and says so.
                try { probe.destroy() } catch (ignoredToo: Exception) { /* nothing left to remove */ }
                null
            }
        }
    }
}
