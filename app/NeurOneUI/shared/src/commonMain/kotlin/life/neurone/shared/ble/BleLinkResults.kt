package life.neurone.shared.ble

import life.neurone.core.common.UUID

// What each platform central does with the answer to a GATT read or write, written once so the rule is tested once (OI-PRIV-17).
// A central only translates its platform's callback into these calls: the Android status code, the Web Bluetooth promise outcome.

/**
 * The answer to a read. A failed read is reported as a failure and never as a value: the pairing notice and the upload gate
 * depend on a refused read never passing for an answer. Only the characteristic is passed on, never the platform's message.
 */
fun BleCentralListener.onReadResult(uuid: UUID, value: ByteArray, succeeded: Boolean) {
    if (succeeded) onCharacteristicRead(uuid, value) else onCharacteristicReadFailed(uuid)
}

/** The answer to a write with response. Success needs no event; a refusal or a write that did not land is reported. */
fun BleCentralListener.onWriteResult(uuid: UUID, succeeded: Boolean) {
    if (!succeeded) onCharacteristicWriteFailed(uuid)
}

/**
 * The Web Bluetooth side's one callback `(kind, a, b)`, as the browser central receives it. `found` is the central's own
 * (it records the device); `cancelled` and `error` leave the manager scanning so the next click retries.
 */
fun BleCentralListener.onWebBleEvent(kind: String, a: String, b: String) {
    when (kind) {
        "connected" -> onConnected(a)
        "disconnected" -> onDisconnected(a)
        "discovered" -> onCharacteristicsDiscovered(if (a.isEmpty()) emptySet() else a.split(",").map(UUID::fromString).toSet())
        "changed" -> onCharacteristicChanged(UUID.fromString(a), hexToBytes(b))
        "read" -> onReadResult(UUID.fromString(a), hexToBytes(b), succeeded = true)
        "readFailed" -> onReadResult(UUID.fromString(a), ByteArray(0), succeeded = false)
        "writeFailed" -> onWriteResult(UUID.fromString(a), succeeded = false)
    }
}

private fun hexToBytes(hex: String): ByteArray = ByteArray(hex.length / 2) { hex.substring(it * 2, it * 2 + 2).toInt(16).toByte() }
