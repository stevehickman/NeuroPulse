package life.neurone.web

import androidx.compose.runtime.Composable
import kotlinx.browser.localStorage
import kotlinx.browser.window
import life.neurone.core.analytics.AnalyticsBackend
import life.neurone.core.common.KeyValueStore
import life.neurone.core.common.MemoryOnlyKeysKeyValueStore
import life.neurone.core.session.Ed25519ProtocolSigner
import life.neurone.core.session.ProtocolSigner
import life.neurone.shared.NoOpAnalyticsBackend
import life.neurone.shared.PlatformServices
import life.neurone.shared.ble.BleCentral
import life.neurone.shared.migration.FileTransfer

/**
 * The browser half of the cross-platform seam.
 *
 * The hub link is Web Bluetooth ([WebBleCentral]). The app keeps its state in memory only (below), so a reload is a fresh start,
 * as it would be after any other lost storage.
 *
 * `localStorage` is readable in the clear by every script on the page and every extension, the browser has no keystore to seal
 * a key in (SWR-MG-06), and no backup rule governs it. So nothing the app stores is written to it: the keys of
 * [MemoryOnlyKeysKeyValueStore.browserKeys], which are every key the core lists, are held in memory for the life of the page
 * (REQ-SW03-18 (b)), and the person can erase whatever an earlier build left there ([eraseLocalData], REQ-SW03-18 (c)).
 */
class WebPlatformServices(
    private val storage: LocalStorageKeyValueStore = LocalStorageKeyValueStore(),
    override val keyValueStore: KeyValueStore = MemoryOnlyKeysKeyValueStore(storage, MemoryOnlyKeysKeyValueStore.browserKeys()),
    override val bleCentral: BleCentral = WebBleCentral(),
    override val protocolSigner: ProtocolSigner = Ed25519ProtocolSigner(),
    override val analyticsBackend: AnalyticsBackend = NoOpAnalyticsBackend(),
) : PlatformServices {

    /** Nothing the app keeps is written to `localStorage`, so the person is told it is gone when the page is (REQ-SW03-18 (b)). */
    override val holdsStateInMemoryOnly: Boolean = true

    /** A backup is a file the person downloads and later chooses: a browser has no folder to keep one in. */
    @Composable
    override fun rememberFileTransfer(): FileTransfer? = fileTransfer

    private val fileTransfer = WebFileTransfer()

    // Empties what this page keeps and reloads, which also drops what is held in memory (the session history, the consent state).
    override val eraseLocalData: (() -> Unit) = {
        storage.eraseAll()
        window.location.reload()
    }

    // The chooser must open inside the click, so the action is the central's own, not a generic refresh.
    @Composable
    override fun rememberBleConnectAction(): () -> Unit = { (bleCentral as? WebBleCentral)?.requestDevice() }
}

class LocalStorageKeyValueStore(private val prefix: String = "np-app:") : KeyValueStore {
    override fun getString(key: String): String? = localStorage.getItem(prefix + key)
    override fun putString(key: String, value: String) = localStorage.setItem(prefix + key, value)
    override fun getInt(key: String, default: Int): Int = getString(key)?.toIntOrNull() ?: default
    override fun putInt(key: String, value: Int) = putString(key, value.toString())
    override fun getBoolean(key: String, default: Boolean): Boolean = getString(key)?.toBooleanStrictOrNull() ?: default
    override fun putBoolean(key: String, value: Boolean) = putString(key, value.toString())
    override fun remove(key: String) = localStorage.removeItem(prefix + key)

    /** Removes every item this app wrote (those under [prefix]), and leaves any other page's items alone. */
    fun eraseAll() {
        val mine = (0 until localStorage.length).mapNotNull { localStorage.key(it) }.filter { it.startsWith(prefix) }
        mine.forEach { localStorage.removeItem(it) }
    }
}
