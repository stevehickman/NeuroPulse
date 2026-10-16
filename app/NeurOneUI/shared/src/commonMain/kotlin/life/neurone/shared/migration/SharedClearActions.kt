package life.neurone.shared.migration

import life.neurone.core.common.EncryptedKeyValueStore
import life.neurone.core.migration.ClearActions
import life.neurone.shared.ble.NeurOneGattManager

/**
 * The two clear steps only the platform can do (SWR-MG-06, SWR-MG-09), written once for every platform: what differs is only
 * where the key is kept, and that is the host's [life.neurone.core.common.SecretKeyStore].
 *
 * `destroyEncryptionKey` destroys the data key in the keystore, so what the app sealed is unreadable from then on. It comes
 * before the files are overwritten, so what the overwrite misses is ciphertext with no key. `forgetPairingRecord` drops the
 * link to the helmet and the central's own record of it; the operating system's Bluetooth bond is the person's to remove and
 * the clear screen says so.
 */
class SharedClearActions(
    private val data: EncryptedKeyValueStore,
    private val gatt: NeurOneGattManager,
) : ClearActions {
    override suspend fun run(step: String): Boolean = try {
        when (step) {
            "destroyEncryptionKey" -> { data.destroyKey(); true }
            "forgetPairingRecord" -> { gatt.forgetHub(); true }
            else -> false
        }
    } catch (ignored: Exception) {
        // A step that could not be done is reported as not done; the clear stops there and carries on at the next try.
        false
    }
}
