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

    /** Keep an interim (locally generated) token for this helmet. */
    fun save(hub: String, token: String)

    /**
     * The hub-provisioned token kept for this helmet, or `null` when the token kept is an interim one or there is none. A hub token
     * replaced by a different one is a change of custody (`REQ-SHDR-09` (d)); an interim one replaced by a hub token is not.
     */
    fun loadHubToken(hub: String): String?

    /** Keep the hub-provisioned token for this helmet, in place of whatever was kept. */
    fun saveHubToken(hub: String, token: String)
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

    // A helmet's raw identifier written by an earlier version leaves the value at launch, not when that helmet next connects
    // (`OI-PRIV-08` (4)).
    init {
        table.migrateLegacy()
    }

    override fun load(hub: String): String? = table.load(hub)?.removePrefix(HUB_PREFIX)

    override fun save(hub: String, token: String) = table.save(hub, token)

    override fun loadHubToken(hub: String): String? = table.load(hub)?.takeIf { it.startsWith(HUB_PREFIX) }?.removePrefix(HUB_PREFIX)

    override fun saveHubToken(hub: String, token: String) = table.save(hub, HUB_PREFIX + token)

    private companion object {
        /**
         * Marks a line whose token the hub provisioned. A token is hex, so it never starts with this. A hub token kept by an earlier
         * version has no mark and reads as interim: the first hub token after it is adopted without a change of custody.
         */
        const val HUB_PREFIX = "hub:"
    }
}

/**
 * Where the hub's SHDR record waits for the app to upload it: one record per helmet, in the app's own folder (`REQ-SHDR-08` (e)).
 *
 * Every call names the helmet by [helmetKey], the warranty owner's store's salted hash for it ([WarrantyOwnerStore]): 64 lowercase
 * hex digits, never the helmet's identifier (`REQ-SHDR-05` (d)). A record is read only under the key it was staged under, so one
 * helmet's record cannot leave under another helmet's consent and token. The implementation refuses a key of any other form.
 */
interface ShdrStaging {
    /** The record staged for this helmet, or null when nothing is staged for it. */
    fun read(helmetKey: String): ByteArray?

    /** Stage [record] for this helmet, replacing any earlier one. Called by the host's hub path. */
    fun stage(helmetKey: String, record: ByteArray)

    /** Remove this helmet's record: after the fleet service has taken it, on a revoke, or at a change of custody. */
    fun clear(helmetKey: String)

    companion object {
        /** The form of a helmet key: a SHA-256 in lowercase hex. */
        val HELMET_KEY = Regex("^[0-9a-f]{64}$")
    }
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
        get() = hubIdentifier?.let(::tokenFor)

    private fun tokenFor(hub: String): String = tokenStore.load(hub) ?: randomBytes(32).toHex().also { tokenStore.save(hub, it) }

    /**
     * OI-BLE-01: swap in the hub-provisioned TRNG warranty token (exactly 32 bytes) for [hub], by default the connected helmet, read
     * once. The wiring passes the helmet the GATT link named when the token arrived, so the token is kept for the helmet that sent it.
     *
     * A hub token different from the hub token already kept for this helmet is what a factory reset or a change of owner produces
     * (`NP-FW-EMMC-002` §A.6), so it is a change of custody (`REQ-SHDR-09` (d)): the previous owner's consent, enrolment, epoch and
     * the helmet's staged record are forgotten ([WarrantyAnalyticsGate.endCustody]) before the new token is kept. The same token
     * again changes nothing, and a hub token over an interim one is the ordinary upgrade.
     */
    fun upgradeDeviceToken(hubToken: ByteArray, hub: String? = hubIdentifier) {
        if (hubToken.size != 32) return
        if (hub == null) return
        val incoming = hubToken.toHex()
        val kept = tokenStore.loadHubToken(hub)
        if (kept == incoming) return
        if (kept != null) warrantyGate.endCustody(hub)
        tokenStore.saveHubToken(hub, incoming)
    }

    /**
     * Upload the SHDR record staged for [hub], by default the connected helmet; true only when the fleet service itself took it, and
     * then that helmet's staged record is deleted. The helmet is read once and that one value names the consent asked for, the
     * token sent and the record read (`REQ-SHDR-08` (e)), so a reconnect to another helmet midway cannot mix them. Nothing is read
     * unless that helmet's warranty owner has consented.
     */
    suspend fun uploadStaged(hub: String? = hubIdentifier): Boolean {
        if (hub == null) return false
        val record = warrantyGate.stagedRecord(hub) ?: return false
        return uploadFor(hub, record).also { if (it) warrantyGate.clearStaged(hub) }
    }

    /**
     * Upload [shdrPayload] for the connected helmet, read once. For the tests of the sealed transport only: a payload handed in is
     * bound to no helmet, so production sends only through [uploadStaged].
     */
    internal suspend fun upload(shdrPayload: ByteArray): Boolean = uploadFor(hubIdentifier, shdrPayload)

    /**
     * Upload an SHDR payload for [hub]; true only when the fleet service itself took it. No-ops (false) unless that helmet's warranty
     * owner has consented, a helmet is named and a gateway key is set. The payload is device telemetry only: the SHDR schema contains
     * no user biology by construction (`CLAUDE.md` §5.1), and this class does not inspect it.
     */
    private suspend fun uploadFor(hub: String?, shdrPayload: ByteArray): Boolean {
        // The consent asked for is this helmet's and no other's (`REQ-SHDR-08`); with none connected it is closed.
        if (hub == null || !warrantyGate.isOpen(hub)) return false
        val token = tokenFor(hub)
        if (gatewayKeyConfigHex.isEmpty()) return false
        val sealed = try {
            FleetOhttp.seal(
                gatewayKeyConfigHex, FleetConfig.FLEET_AUTHORITY, FleetConfig.FLEET_PATH, FleetConfig.DEVICE_TOKEN_HEADER, token, shdrPayload,
                entropy = randomBytes(32),
            )
        } catch (e: IllegalArgumentException) {
            return false
        }
        // The response secret is in the core until the answer is opened. If the post throws or is cancelled there is no
        // answer, so the secret is released here; after `accepted` it is already gone and this does nothing.
        try {
            val answer = transport.post(HttpRequest(relayUrl, mapOf("Content-Type" to FleetOhttp.REQUEST_MEDIA_TYPE), sealed.request))
            return FleetOhttp.accepted(sealed, relayOk = answer != null && answer.status in 200..299, answer?.header("Content-Type"), answer?.body)
        } finally {
            sealed.close()
        }
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
     * [FleetUploader.upgradeDeviceToken] separately rejects any non-32-byte value. [hub] is the helmet the GATT link names as the
     * token arrives, taken from the same source as the token rather than from the uploader's copy, so a token is never kept for
     * the helmet before it; with none named nothing is kept.
     */
    fun applyWarrantyToken(token: ByteArray?, hub: String?, uploader: FleetUploader) {
        if (token != null && hub != null) uploader.upgradeDeviceToken(token, hub)
    }

    /** Which helmet the link is to (`null` after a disconnect); the uploader keeps its token per helmet. */
    fun applyHubIdentifier(hub: String?, uploader: FleetUploader) = uploader.setHubIdentifier(hub)
}
