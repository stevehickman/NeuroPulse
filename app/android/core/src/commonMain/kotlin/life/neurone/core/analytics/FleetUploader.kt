package life.neurone.core.analytics

import kotlin.concurrent.Volatile
import life.neurone.core.common.EncryptedKeyValueStore
import life.neurone.core.common.KeyValueStore
import life.neurone.core.net.HttpRequest
import life.neurone.core.net.HttpTransport
import life.neurone.core.platform.secureRandomBytes
import life.neurone.core.platform.toHex

/**
 * Persistence seam for the opaque SHDR device token: the app's sealed key-value store ([SealedDeviceTokenStore]) in production, an
 * in-memory one in a test, so the token logic runs on the JVM without a keystore.
 */
interface DeviceTokenStore {
    /** The token kept for this helmet, or `null` if there is none. A token is per helmet, never per phone (REQ-SHDR-05). */
    fun load(hub: String): String?
    fun save(hub: String, token: String)
}

/**
 * The device tokens kept in a [KeyValueStore] that seals them (the app's `EncryptedKeyValueStore`), as one value keyed by a salted
 * hash of the helmet's identifier and never by the identifier (a Bluetooth hardware address on Android; `REQ-SHDR-05` (d)). A key
 * per helmet would be a key the store is not told to seal, so the table is kept inside the one that is.
 */
class SealedDeviceTokenStore(
    private val store: KeyValueStore,
    sha256: (ByteArray) -> ByteArray = life.neurone.core.platform.Digests::sha256,
) : DeviceTokenStore {
    private val table = HubTokenTable(
        read = { store.getString(FleetUploader.TOKEN_PREF) },
        write = { store.putString(FleetUploader.TOKEN_PREF, it) },
        sha256 = sha256,
    )

    override fun load(hub: String): String? = table.load(hub)

    override fun save(hub: String, token: String) = table.save(hub, token)
}

/** Where the hub leaves the SHDR record for the app to upload: a file in the app's own folder. */
interface ShdrStaging {
    /** The staged record, or null when nothing is staged. */
    fun read(): ByteArray?

    /** Remove it, after the fleet service has taken it. */
    fun clear()
}

/**
 * The SHDR fleet telemetry uploader, written once for every host (privacy review Finding 6; `REQ-SHDR-01`, `-02`, `-05`, `-07`).
 *
 * Consent subject: the WARRANTY OWNER (may be a clinic, `CLAUDE.md` §6.0). Gated exclusively by [WarrantyAnalyticsGate]; this class
 * never references `ConsentStore` or `ResearchAnalyticsGate` (the two consent subjects stay structurally independent).
 *
 * Device identity: an opaque locally generated 32-byte CSPRNG token, kept per helmet in the sealed store; never a hardware or
 * advertising identifier. Replaced by the hub-provisioned TRNG warranty token when the hub delivers it (`OI-BLE-01`).
 *
 * Transport: Oblivious HTTP ([FleetOhttp]). The record is sealed to the gateway's key and posted to the relay over [transport],
 * which the host builds with the relay's certificate pins. An empty gateway key uploads nothing (fail closed).
 */
class FleetUploader(
    private val tokenStore: DeviceTokenStore,
    private val warrantyGate: WarrantyAnalyticsGate,
    private val transport: HttpTransport,
    private val gatewayKeyConfigHex: String = FleetConfig.GATEWAY_KEY_CONFIG_HEX,
    private val relayUrl: String = FleetConfig.RELAY_URL,
    private val randomBytes: (Int) -> ByteArray = ::secureRandomBytes,
) {
    // Which helmet the link is to, `null` when not connected (privacy review Finding 6, REQ-SHDR-05).
    @Volatile
    private var hubIdentifier: String? = null

    /** The helmet the link is to, or `null` after a disconnect. Set by the wiring from the GATT manager. */
    fun setHubIdentifier(hub: String?) {
        hubIdentifier = hub
    }

    /**
     * Opaque device token for the connected helmet: generated once per helmet, replaced by [upgradeDeviceToken] when the hub
     * provisions its TRNG token. Never per phone. `null` when no helmet is connected, and then nothing is sent.
     */
    val deviceToken: String?
        get() {
            val hub = hubIdentifier ?: return null
            return tokenStore.load(hub) ?: randomBytes(32).toHex().also { tokenStore.save(hub, it) }
        }

    /** OI-BLE-01: swap in the hub-provisioned TRNG warranty token (exactly 32 bytes) for the connected helmet. */
    fun upgradeDeviceToken(hubToken: ByteArray) {
        if (hubToken.size != 32) return
        val hub = hubIdentifier ?: return
        tokenStore.save(hub, hubToken.toHex())
    }

    /**
     * Upload an SHDR payload; true only when the fleet service itself took it. No-ops (false) unless the warranty owner has
     * consented, a helmet is connected and a gateway key is set. The payload is device telemetry only: the SHDR schema contains no
     * user biology by construction (`CLAUDE.md` §5.1), and this class does not inspect it.
     */
    suspend fun upload(shdrPayload: ByteArray): Boolean {
        if (!warrantyGate.isOpen) return false
        val token = deviceToken ?: return false
        if (gatewayKeyConfigHex.isEmpty()) return false
        val sealed = try {
            FleetOhttp.seal(
                gatewayKeyConfigHex, FleetConfig.FLEET_AUTHORITY, FleetConfig.FLEET_PATH, FleetConfig.DEVICE_TOKEN_HEADER, token, shdrPayload,
                entropy = randomBytes(32),
            )
        } catch (e: IllegalArgumentException) {
            return false
        }
        val answer = transport.post(HttpRequest(relayUrl, mapOf("Content-Type" to FleetOhttp.REQUEST_MEDIA_TYPE), sealed.request))
        return FleetOhttp.accepted(sealed, relayOk = answer != null && answer.status in 200..299, answer?.header("Content-Type"), answer?.body)
    }

    companion object {
        const val TOKEN_PREF = EncryptedKeyValueStore.SHDR_DEVICE_TOKEN_KEY
    }
}

/**
 * Pure composition rules for the upload pipeline, so the wiring decisions are unit-testable (the same rules the hosts follow).
 */
object FleetUploadWiring {
    /**
     * OI-BLE-01 token upgrade. Adopt the hub-provisioned 32-byte TRNG token when it arrives. A null value (a disconnect clearing the
     * flow) is never forwarded, because that would downgrade the device identity back to the CSPRNG fallback;
     * [FleetUploader.upgradeDeviceToken] separately rejects any non-32-byte value.
     */
    fun applyWarrantyToken(token: ByteArray?, uploader: FleetUploader) {
        token?.let { uploader.upgradeDeviceToken(it) }
    }

    /** Which helmet the link is to (`null` after a disconnect); the uploader keeps its token per helmet. */
    fun applyHubIdentifier(hub: String?, uploader: FleetUploader) = uploader.setHubIdentifier(hub)

    /**
     * Upload only when the hub signalled a pending upload, the warranty owner has consented and the hub staged a payload;
     * [onResult] runs only if an upload was attempted. [FleetUploader.upload] re-checks the gate as a backstop.
     */
    suspend fun applyUploadPending(
        pending: Boolean,
        warrantyGateOpen: Boolean,
        stagingPayload: ByteArray?,
        uploader: FleetUploader,
        onResult: (Boolean) -> Unit = {},
    ) {
        if (!pending || !warrantyGateOpen || stagingPayload == null) return
        onResult(uploader.upload(stagingPayload))
    }
}
