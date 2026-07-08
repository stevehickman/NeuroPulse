package com.neurone.app.ble

import android.Manifest
import android.annotation.SuppressLint
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothGatt
import android.bluetooth.BluetoothGattCharacteristic
import android.bluetooth.BluetoothGattDescriptor
import android.bluetooth.BluetoothManager
import android.bluetooth.BluetoothProfile
import android.bluetooth.le.ScanCallback
import android.bluetooth.le.ScanFilter
import android.bluetooth.le.ScanResult
import android.bluetooth.le.ScanSettings
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.content.pm.PackageManager
import android.os.Build
import android.os.ParcelUuid
import androidx.core.content.ContextCompat
import java.util.UUID

// Real Android implementation of BleCentral (parity with iOS BLECentralManager over
// CoreBluetooth). Drives BluetoothLeScanner + BluetoothGatt; NeurOneGattManager (the
// listener) owns the scan→connect→discover→notify state machine. Permission-aware:
// `adapterState` reports UNAUTHORIZED until BLUETOOTH_SCAN/CONNECT are granted, so the
// manager never scans before the user grants runtime permission. Cannot be unit-tested
// here (needs a device/adapter); MockBleCentral covers the manager's logic in :core-style
// app tests. OI-AND-BLE-01.
@SuppressLint("MissingPermission") // guarded by hasBlePermissions() before every BLE call
class AndroidBleCentral(private val context: Context) : BleCentral {

    private companion object {
        // Client Characteristic Configuration Descriptor (enables notifications).
        val CCCD_UUID: UUID = UUID.fromString("00002902-0000-1000-8000-00805F9B34FB")
    }

    private val bluetoothManager: BluetoothManager? =
        context.getSystemService(Context.BLUETOOTH_SERVICE) as? BluetoothManager
    private val adapter: BluetoothAdapter? = bluetoothManager?.adapter

    private var listener: BleCentralListener? = null
    private var gatt: BluetoothGatt? = null
    private var scanning = false
    private var serviceUuid: UUID? = null

    // Characteristics resolved after service discovery, keyed by UUID.
    private val characteristics: MutableMap<UUID, BluetoothGattCharacteristic> = mutableMapOf()

    // Re-notify the manager when Bluetooth is turned on/off at the OS level.
    private val adapterStateReceiver = object : BroadcastReceiver() {
        override fun onReceive(ctx: Context?, intent: Intent?) {
            if (intent?.action == BluetoothAdapter.ACTION_STATE_CHANGED) {
                listener?.onAdapterStateChanged(adapterState)
            }
        }
    }

    // MARK: BleCentral

    override val adapterState: AdapterState
        get() = when {
            adapter == null -> AdapterState.UNKNOWN
            !hasBlePermissions() -> AdapterState.UNAUTHORIZED
            !adapter.isEnabled -> AdapterState.OFF
            else -> AdapterState.ON
        }

    override fun setListener(listener: BleCentralListener) {
        this.listener = listener
        context.registerReceiver(
            adapterStateReceiver,
            IntentFilter(BluetoothAdapter.ACTION_STATE_CHANGED),
        )
    }

    /** Re-emit the current adapter state to the listener (call after a permission grant). */
    fun refresh() {
        listener?.onAdapterStateChanged(adapterState)
    }

    override fun startScan(serviceUuid: UUID) {
        if (!hasBlePermissions() || scanning) return
        val scanner = adapter?.bluetoothLeScanner ?: return
        this.serviceUuid = serviceUuid
        val filter = ScanFilter.Builder().setServiceUuid(ParcelUuid(serviceUuid)).build()
        val settings = ScanSettings.Builder()
            .setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY)
            .build()
        scanning = true
        scanner.startScan(listOf(filter), settings, scanCallback)
    }

    override fun stopScan() {
        if (!scanning) return
        scanning = false
        if (hasBlePermissions()) adapter?.bluetoothLeScanner?.stopScan(scanCallback)
    }

    override fun connect(deviceId: String) {
        if (!hasBlePermissions()) return
        val device: BluetoothDevice = adapter?.getRemoteDevice(deviceId) ?: return
        gatt = device.connectGatt(context, false, gattCallback, BluetoothDevice.TRANSPORT_LE)
    }

    override fun disconnect() {
        gatt?.let {
            if (hasBlePermissions()) {
                it.disconnect()
                it.close()
            }
        }
        gatt = null
        characteristics.clear()
    }

    override fun discoverCharacteristics(serviceUuid: UUID, uuids: List<UUID>) {
        this.serviceUuid = serviceUuid
        if (hasBlePermissions()) gatt?.discoverServices()
    }

    override fun enableNotifications(uuid: UUID) {
        val char = characteristics[uuid] ?: return
        val g = gatt ?: return
        if (!hasBlePermissions()) return
        g.setCharacteristicNotification(char, true)
        val cccd = char.getDescriptor(CCCD_UUID) ?: return
        writeDescriptor(g, cccd, BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE)
    }

    override fun read(uuid: UUID) {
        val char = characteristics[uuid] ?: return
        if (hasBlePermissions()) gatt?.readCharacteristic(char)
    }

    override fun write(uuid: UUID, value: ByteArray) {
        val char = characteristics[uuid] ?: return
        val g = gatt ?: return
        if (!hasBlePermissions()) return
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            g.writeCharacteristic(char, value, BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT)
        } else {
            @Suppress("DEPRECATION")
            run {
                char.writeType = BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT
                char.value = value
                g.writeCharacteristic(char)
            }
        }
    }

    // MARK: Helpers

    private fun hasBlePermissions(): Boolean {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.S) return true // pre-Android 12: normal perms
        return granted(Manifest.permission.BLUETOOTH_SCAN) && granted(Manifest.permission.BLUETOOTH_CONNECT)
    }

    private fun granted(permission: String): Boolean =
        ContextCompat.checkSelfPermission(context, permission) == PackageManager.PERMISSION_GRANTED

    private fun writeDescriptor(g: BluetoothGatt, descriptor: BluetoothGattDescriptor, value: ByteArray) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            g.writeDescriptor(descriptor, value)
        } else {
            @Suppress("DEPRECATION")
            run {
                descriptor.value = value
                g.writeDescriptor(descriptor)
            }
        }
    }

    // MARK: Callbacks

    private val scanCallback = object : ScanCallback() {
        override fun onScanResult(callbackType: Int, result: ScanResult) {
            listener?.onDeviceFound(result.device.address)
        }
    }

    private val gattCallback = object : android.bluetooth.BluetoothGattCallback() {
        override fun onConnectionStateChange(g: BluetoothGatt, status: Int, newState: Int) {
            when (newState) {
                BluetoothProfile.STATE_CONNECTED -> listener?.onConnected(g.device.address)
                BluetoothProfile.STATE_DISCONNECTED -> {
                    val address = g.device.address
                    characteristics.clear()
                    listener?.onDisconnected(address)
                }
            }
        }

        override fun onServicesDiscovered(g: BluetoothGatt, status: Int) {
            characteristics.clear()
            val service = serviceUuid?.let { g.getService(it) }
            service?.characteristics?.forEach { characteristics[it.uuid] = it }
            listener?.onCharacteristicsDiscovered(characteristics.keys.toSet())
        }

        // API 33+ delivers the value directly.
        override fun onCharacteristicChanged(
            g: BluetoothGatt,
            characteristic: BluetoothGattCharacteristic,
            value: ByteArray,
        ) {
            listener?.onCharacteristicChanged(characteristic.uuid, value)
        }

        // Pre-33 delivers via characteristic.value.
        @Deprecated("Deprecated in API 33")
        @Suppress("DEPRECATION")
        override fun onCharacteristicChanged(g: BluetoothGatt, characteristic: BluetoothGattCharacteristic) {
            if (Build.VERSION.SDK_INT < Build.VERSION_CODES.TIRAMISU) {
                characteristic.value?.let { listener?.onCharacteristicChanged(characteristic.uuid, it) }
            }
        }

        override fun onCharacteristicRead(
            g: BluetoothGatt,
            characteristic: BluetoothGattCharacteristic,
            value: ByteArray,
            status: Int,
        ) {
            if (status == BluetoothGatt.GATT_SUCCESS) listener?.onCharacteristicRead(characteristic.uuid, value)
        }

        @Deprecated("Deprecated in API 33")
        @Suppress("DEPRECATION")
        override fun onCharacteristicRead(g: BluetoothGatt, characteristic: BluetoothGattCharacteristic, status: Int) {
            if (Build.VERSION.SDK_INT < Build.VERSION_CODES.TIRAMISU && status == BluetoothGatt.GATT_SUCCESS) {
                characteristic.value?.let { listener?.onCharacteristicRead(characteristic.uuid, it) }
            }
        }
    }
}
