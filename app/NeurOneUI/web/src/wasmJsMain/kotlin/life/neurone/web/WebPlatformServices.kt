package life.neurone.web

import androidx.compose.runtime.Composable
import kotlinx.browser.localStorage
import life.neurone.core.analytics.AnalyticsBackend
import life.neurone.core.common.KeyValueStore
import life.neurone.core.common.MemoryOnlyKeysKeyValueStore
import life.neurone.core.protocol.PersistedKeys
import life.neurone.core.session.Ed25519ProtocolSigner
import life.neurone.core.session.ProtocolSigner
import life.neurone.shared.NoOpAnalyticsBackend
import life.neurone.shared.PlatformServices
import life.neurone.shared.ble.BleCentral
import life.neurone.shared.migration.FileTransfer

/**
 * The browser half of the cross-platform seam.
 *
 * The hub link is Web Bluetooth ([WebBleCentral]). Storage is
 * `localStorage`, which a user can clear; the app treats that as a fresh install, as it would any other
 * lost storage.
 *
 * `localStorage` is readable in the clear by every script on the page and every extension, the browser has no keystore to seal
 * a key in (SWR-MG-06), and no backup rule governs it. So the records of a person's sessions are not written to it at all:
 * [MEMORY_ONLY_KEYS] are held in memory for the life of the page (REQ-SW03-18 (b)). Everything else the app keeps here is
 * configuration or consent state, which is a separate decision (see the privacy review, Finding 2).
 */
class WebPlatformServices(
    override val keyValueStore: KeyValueStore = MemoryOnlyKeysKeyValueStore(LocalStorageKeyValueStore(), MEMORY_ONLY_KEYS),
    override val bleCentral: BleCentral = WebBleCentral(),
    override val protocolSigner: ProtocolSigner = Ed25519ProtocolSigner(),
    override val analyticsBackend: AnalyticsBackend = NoOpAnalyticsBackend(),
) : PlatformServices {

    /** A backup is a file the person downloads and later chooses: a browser has no folder to keep one in. */
    @Composable
    override fun rememberFileTransfer(): FileTransfer? = fileTransfer

    private val fileTransfer = WebFileTransfer()

    // The chooser must open inside the click, so the action is the central's own, not a generic refresh.
    @Composable
    override fun rememberBleConnectAction(): () -> Unit = { (bleCentral as? WebBleCentral)?.requestDevice() }
}

/** Keys the browser keeps in memory only: the session history is a record of the person's health (CLAUDE.md §5.1). */
val MEMORY_ONLY_KEYS: Set<String> = setOf(PersistedKeys.SESSION_HISTORY_KEY)

class LocalStorageKeyValueStore(private val prefix: String = "np-app:") : KeyValueStore {
    override fun getString(key: String): String? = localStorage.getItem(prefix + key)
    override fun putString(key: String, value: String) = localStorage.setItem(prefix + key, value)
    override fun getInt(key: String, default: Int): Int = getString(key)?.toIntOrNull() ?: default
    override fun putInt(key: String, value: Int) = putString(key, value.toString())
    override fun getBoolean(key: String, default: Boolean): Boolean = getString(key)?.toBooleanStrictOrNull() ?: default
    override fun putBoolean(key: String, value: Boolean) = putString(key, value.toString())
    override fun remove(key: String) = localStorage.removeItem(prefix + key)
}
