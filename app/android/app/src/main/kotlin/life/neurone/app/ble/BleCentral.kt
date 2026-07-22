package life.neurone.app.ble

import java.util.UUID

// BLE central abstraction — port of iOS BLECentral.swift. Decouples
// NeurOneGattManager from android.bluetooth so connection logic is
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
}
