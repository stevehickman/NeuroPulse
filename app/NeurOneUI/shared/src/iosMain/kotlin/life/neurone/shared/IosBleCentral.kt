package life.neurone.shared

import kotlinx.cinterop.ExperimentalForeignApi
import kotlinx.cinterop.addressOf
import kotlinx.cinterop.usePinned
import life.neurone.core.common.UUID
import life.neurone.shared.ble.AdapterState
import life.neurone.shared.ble.BleCentral
import life.neurone.shared.ble.BleCentralListener
import platform.CoreBluetooth.CBCentralManager
import platform.CoreBluetooth.CBCentralManagerDelegateProtocol
import platform.CoreBluetooth.CBCharacteristic
import platform.CoreBluetooth.CBCharacteristicWriteWithResponse
import platform.CoreBluetooth.CBManagerStatePoweredOff
import platform.CoreBluetooth.CBManagerStatePoweredOn
import platform.CoreBluetooth.CBManagerStateUnauthorized
import platform.CoreBluetooth.CBPeripheral
import platform.CoreBluetooth.CBPeripheralDelegateProtocol
import platform.CoreBluetooth.CBService
import platform.CoreBluetooth.CBUUID
import platform.Foundation.NSData
import platform.Foundation.NSError
import platform.Foundation.NSNumber
import platform.Foundation.create
import platform.darwin.NSObject
import platform.posix.memcpy

/**
 * The Apple [BleCentral], over CoreBluetooth: a `CBCentralManager` and the one hub peripheral, driven by the same
 * state machine (`NeurOneGattManager`) every other platform uses. This class only translates: scan, connect,
 * discover, notify, read and write go down, and the delegate callbacks come back as [BleCentralListener] calls.
 *
 * Callbacks arrive on the main queue (the manager is created with none), the same thread Compose runs on, so the
 * manager's state flows need no hop. Written without a macOS host to compile it on (`OI-UI-KMP-06`).
 */
@OptIn(ExperimentalForeignApi::class)
class IosBleCentral : NSObject(), BleCentral, CBCentralManagerDelegateProtocol, CBPeripheralDelegateProtocol {

    private var listener: BleCentralListener? = null
    private var peripheral: CBPeripheral? = null
    private var wantedCharacteristics: List<CBUUID> = emptyList()
    private val characteristics = mutableMapOf<String, CBCharacteristic>()
    private val manager: CBCentralManager = CBCentralManager(delegate = this, queue = null)

    override val adapterState: AdapterState
        get() = when (manager.state) {
            CBManagerStatePoweredOn -> AdapterState.ON
            CBManagerStatePoweredOff -> AdapterState.OFF
            CBManagerStateUnauthorized -> AdapterState.UNAUTHORIZED
            else -> AdapterState.UNKNOWN
        }

    override fun setListener(listener: BleCentralListener) {
        this.listener = listener
    }

    override fun refresh() {
        listener?.onAdapterStateChanged(adapterState)
    }

    // ── Down: BleCentral ─────────────────────────────────────────────────

    override fun startScan(serviceUuid: UUID) {
        manager.scanForPeripheralsWithServices(listOf(CBUUID.UUIDWithString(serviceUuid.toString())), options = null)
    }

    override fun stopScan() {
        manager.stopScan()
    }

    override fun connect(deviceId: String) {
        peripheral?.let { manager.connectPeripheral(it, options = null) }
    }

    override fun disconnect() {
        peripheral?.let { manager.cancelPeripheralConnection(it) }
    }

    override fun discoverCharacteristics(serviceUuid: UUID, uuids: List<UUID>) {
        wantedCharacteristics = uuids.map { CBUUID.UUIDWithString(it.toString()) }
        peripheral?.discoverServices(listOf(CBUUID.UUIDWithString(serviceUuid.toString())))
    }

    override fun enableNotifications(uuid: UUID) {
        val p = peripheral ?: return
        characteristics[uuid.toString()]?.let { p.setNotifyValue(true, forCharacteristic = it) }
    }

    override fun read(uuid: UUID) {
        val p = peripheral ?: return
        characteristics[uuid.toString()]?.let { p.readValueForCharacteristic(it) }
    }

    override fun write(uuid: UUID, value: ByteArray) {
        val p = peripheral ?: return
        characteristics[uuid.toString()]?.let {
            p.writeValue(value.toNSData(), forCharacteristic = it, type = CBCharacteristicWriteWithResponse)
        }
    }

    // ── Up: CBCentralManagerDelegate ─────────────────────────────────────

    override fun centralManagerDidUpdateState(central: CBCentralManager) {
        listener?.onAdapterStateChanged(adapterState)
    }

    override fun centralManager(
        central: CBCentralManager,
        didDiscoverPeripheral: CBPeripheral,
        advertisementData: Map<Any?, *>,
        RSSI: NSNumber,
    ) {
        peripheral = didDiscoverPeripheral.also { it.delegate = this }
        listener?.onDeviceFound(didDiscoverPeripheral.identifier.UUIDString)
    }

    override fun centralManager(central: CBCentralManager, didConnectPeripheral: CBPeripheral) {
        listener?.onConnected(didConnectPeripheral.identifier.UUIDString)
    }

    override fun centralManager(central: CBCentralManager, didDisconnectPeripheral: CBPeripheral, error: NSError?) {
        characteristics.clear()
        listener?.onDisconnected(didDisconnectPeripheral.identifier.UUIDString)
    }

    override fun centralManager(central: CBCentralManager, didFailToConnectPeripheral: CBPeripheral, error: NSError?) {
        // A failed connect is a disconnect to the manager: it rescans after its two-second delay.
        listener?.onDisconnected(didFailToConnectPeripheral.identifier.UUIDString)
    }

    // ── Up: CBPeripheralDelegate ─────────────────────────────────────────

    override fun peripheral(peripheral: CBPeripheral, didDiscoverServices: NSError?) {
        peripheral.services?.filterIsInstance<CBService>()?.forEach {
            peripheral.discoverCharacteristics(wantedCharacteristics, forService = it)
        }
    }

    override fun peripheral(peripheral: CBPeripheral, didDiscoverCharacteristicsForService: CBService, error: NSError?) {
        didDiscoverCharacteristicsForService.characteristics?.filterIsInstance<CBCharacteristic>()?.forEach {
            characteristics[it.UUID.UUIDString.lowercase()] = it
        }
        listener?.onCharacteristicsDiscovered(characteristics.keys.map(UUID::fromString).toSet())
    }

    override fun peripheral(peripheral: CBPeripheral, didUpdateValueForCharacteristic: CBCharacteristic, error: NSError?) {
        val data = didUpdateValueForCharacteristic.value ?: return
        // A read and a notification arrive here alike, and the manager treats them alike.
        listener?.onCharacteristicChanged(UUID.fromString(didUpdateValueForCharacteristic.UUID.UUIDString), data.toByteArray())
    }
}

@OptIn(ExperimentalForeignApi::class)
private fun NSData.toByteArray(): ByteArray {
    val size = length.toInt()
    val out = ByteArray(size)
    if (size > 0) out.usePinned { memcpy(it.addressOf(0), bytes, length) }
    return out
}

@OptIn(ExperimentalForeignApi::class)
private fun ByteArray.toNSData(): NSData =
    if (isEmpty()) NSData() else usePinned { NSData.create(bytes = it.addressOf(0), length = size.toULong()) }
