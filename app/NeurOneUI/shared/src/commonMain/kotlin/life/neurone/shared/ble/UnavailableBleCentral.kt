package life.neurone.shared.ble

import life.neurone.core.common.UUID

/**
 * A [BleCentral] for a platform whose Bluetooth LE binding is not written yet. It reports
 * [AdapterState.UNKNOWN] and does nothing else, so the hub stays disconnected and the rest of the
 * app (consent, history, consumables) works. It never pretends to be a radio that is switched
 * off, because "Bluetooth is off" is advice the wearer could act on and here it would be wrong.
 */
class UnavailableBleCentral : BleCentral {
    private var listener: BleCentralListener? = null

    override val adapterState: AdapterState = AdapterState.UNKNOWN

    override fun setListener(listener: BleCentralListener) {
        this.listener = listener
        listener.onAdapterStateChanged(adapterState)
    }

    override fun startScan(serviceUuid: UUID) {}
    override fun stopScan() {}
    override fun connect(deviceId: String) {}
    override fun disconnect() {}
    override fun discoverCharacteristics(serviceUuid: UUID, uuids: List<UUID>) {}
    override fun enableNotifications(uuid: UUID) {}
    override fun read(uuid: UUID) {}
    override fun write(uuid: UUID, value: ByteArray) {}
}
