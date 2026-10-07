package life.neurone.shared.ble

import life.neurone.core.common.UUID

// BLE central abstraction — port of iOS BLECentral.swift. Decouples
// NeurOneGattManager from every platform Bluetooth API (android.bluetooth, CoreBluetooth, Web Bluetooth) so connection logic is
// unit-testable without hardware (parity with the iOS BLECentralManager
// protocol + MockBLECentral pattern).

enum class AdapterState { UNKNOWN, OFF, UNAUTHORIZED, ON }

enum class ConnectionState { DISCONNECTED, SCANNING, CONNECTING, CONNECTED }

interface BleCentralListener {
    fun onAdapterStateChanged(state: AdapterState)
    fun onDeviceFound(deviceId: String)
    fun onConnected(deviceId: String)
    fun onDisconnected(deviceId: String)
    fun onCharacteristicsDiscovered(characteristics: Set<UUID>)
    fun onCharacteristicChanged(uuid: UUID, value: ByteArray)
    fun onCharacteristicRead(uuid: UUID, value: ByteArray)
}

interface BleCentral {
    val adapterState: AdapterState
    fun setListener(listener: BleCentralListener)
    fun startScan(serviceUuid: UUID)
    fun stopScan()
    fun connect(deviceId: String)
    fun disconnect()
    fun discoverCharacteristics(serviceUuid: UUID, uuids: List<UUID>)
    fun enableNotifications(uuid: UUID)
    fun read(uuid: UUID)
    fun write(uuid: UUID, value: ByteArray)

    /**
     * Re-read the adapter state after the user changed something outside the app (granted a
     * runtime permission, turned Bluetooth on). A central with nothing to re-read ignores it.
     */
    fun refresh() {}
}
