package com.neurone.app.ble

import com.neurone.core.ble.GattParser
import com.neurone.core.ble.GattUuids
import com.neurone.core.ble.OtaOpcode
import com.neurone.core.models.OtaStatusPacket
import com.neurone.core.models.SessionState
import java.util.UUID
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Job
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.launch

/**
 * Hub GATT connection manager — port of iOS NeurOneGATTManager.swift.
 *
 * Behavior parity:
 *  - Auto-scan when the adapter turns on, no manual user action (iOS ISC-11).
 *  - 2-second reconnect delay after disconnect (iOS ISC-12).
 *  - allCharacteristicsResolved requires all 14 of GattUuids.all;
 *    warrantyToken and firmwareVersion are optional (iOS ISC-13).
 *  - On any adapter state other than ON, session state is cleared —
 *    stale UHDR display data must not survive a BLE reset (iOS privacy fix).
 *
 * UHDR/SHDR boundary (structural, not conventional): SHDR-class
 * characteristics (warrantyToken, shdrUploadStatus) are handled by
 * early-return guards BEFORE any session-state mutation, so SHDR payloads
 * can never flow into UHDR display state — mirrors the iOS guard ordering.
 */
class NeurOneGattManager(
    private val central: BleCentral,
    private val scope: CoroutineScope,
) : BleCentralListener {

    private val _connectionState = MutableStateFlow(ConnectionState.DISCONNECTED)
    val connectionState: StateFlow<ConnectionState> = _connectionState

    private val _session = MutableStateFlow(SessionState.EMPTY)
    val session: StateFlow<SessionState> = _session

    private val _bluetoothUnavailable = MutableStateFlow(false)
    val bluetoothUnavailable: StateFlow<Boolean> = _bluetoothUnavailable

    private val _allCharacteristicsResolved = MutableStateFlow(false)
    val allCharacteristicsResolved: StateFlow<Boolean> = _allCharacteristicsResolved

    private val _zoneModules = MutableStateFlow<List<Int>>(listOf(0, 0, 0, 0, 0))
    val zoneModules: StateFlow<List<Int>> = _zoneModules

    private val _otaStatus = MutableStateFlow<OtaStatusPacket?>(null)
    val otaStatus: StateFlow<OtaStatusPacket?> = _otaStatus

    private val _hubFirmwareVersion = MutableStateFlow<String?>(null)
    val hubFirmwareVersion: StateFlow<String?> = _hubFirmwareVersion

    /** SHDR-class opaque warranty token (NP-FW-EMMC-002 §A). Never mixed into session state. */
    private val _warrantyToken = MutableStateFlow<ByteArray?>(null)
    val warrantyToken: StateFlow<ByteArray?> = _warrantyToken

    private var reconnectJob: Job? = null

    init {
        central.setListener(this)
        onAdapterStateChanged(central.adapterState)
    }

    // ── BleCentralListener ───────────────────────────────────────────────

    override fun onAdapterStateChanged(state: AdapterState) {
        when (state) {
            AdapterState.ON -> {
                _bluetoothUnavailable.value = false
                startScanning()
            }
            else -> {
                // Clear stale UHDR display state on any non-ON adapter state
                // (parallel of iOS applyStateUpdate default branch fix).
                applyDisconnection()
                _bluetoothUnavailable.value = true
            }
        }
    }

    override fun onDeviceFound(deviceId: String) {
        central.stopScan()
        _connectionState.value = ConnectionState.CONNECTING
        central.connect(deviceId)
    }

    override fun onConnected(deviceId: String) {
        _connectionState.value = ConnectionState.CONNECTED
        central.discoverCharacteristics(
            GattUuids.service,
            GattUuids.all + listOf(GattUuids.warrantyToken, GattUuids.firmwareVersion),
        )
    }

    override fun onDisconnected(deviceId: String) {
        applyDisconnection()
        // 2-second reconnect delay, then re-scan — only when the adapter is on.
        reconnectJob?.cancel()
        reconnectJob = scope.launch {
            delay(2_000)
            if (central.adapterState == AdapterState.ON) startScanning()
        }
    }

    override fun onCharacteristicsDiscovered(characteristics: Set<UUID>) {
        // warrantyToken / firmwareVersion absence is tolerated silently
        // (hub firmware may not ship them yet — OI-WA-03).
        _allCharacteristicsResolved.value = characteristics.containsAll(GattUuids.all)
        GattUuids.all.forEach { central.enableNotifications(it) }
        if (GattUuids.warrantyToken in characteristics) central.read(GattUuids.warrantyToken)
        if (GattUuids.firmwareVersion in characteristics) central.read(GattUuids.firmwareVersion)
        // Restore session status immediately on (re)connect.
        if (GattUuids.sessionStatus in characteristics) central.read(GattUuids.sessionStatus)
    }

    override fun onCharacteristicChanged(uuid: UUID, value: ByteArray) {
        // SHDR guards FIRST — structural UHDR/SHDR boundary (see class doc).
        if (uuid == GattUuids.warrantyToken) {
            applyWarrantyToken(value)
            return
        }
        if (uuid == GattUuids.shdrUploadStatus) {
            return // SHDR upload bookkeeping only; never touches session state
        }
        when (uuid) {
            GattUuids.sessionState -> GattParser.parseSessionState(value)?.let { epoch ->
                _session.value = _session.value.copy(epoch = epoch)
            }
            GattUuids.sessionStatus -> GattParser.parseSessionStatus(value)?.let { (pid, status) ->
                _session.value = _session.value.copy(protocolId = pid, status = status)
            }
            GattUuids.hrvCoherence -> GattParser.parseHrvCoherence(value)?.let { hrv ->
                _session.value = _session.value.copy(hrv = hrv)
            }
            GattUuids.pacerPhase -> GattParser.parsePacerPhase(value)?.let { (phase, pct) ->
                _session.value = _session.value.copy(pacerPhase = phase, pacerElapsedPercent = pct)
            }
            GattUuids.impedanceResult -> GattParser.parseImpedanceResult(value)?.let { flags ->
                _session.value = _session.value.copy(impedancePassFlags = flags)
            }
            GattUuids.consumableStatus -> GattParser.parseConsumableStatus(value)?.let { counts ->
                _session.value = _session.value.copy(consumableSessionCounts = counts)
            }
            GattUuids.zoneModuleStatus -> GattParser.parseZoneModuleStatus(value)?.let {
                _zoneModules.value = it
            }
            GattUuids.otaStatus -> GattParser.parseOtaStatus(value)?.let { _otaStatus.value = it }
            GattUuids.firmwareVersion -> GattParser.parseFirmwareVersion(value)?.let {
                _hubFirmwareVersion.value = it.toString()
            }
        }
    }

    override fun onCharacteristicRead(uuid: UUID, value: ByteArray) =
        onCharacteristicChanged(uuid, value)

    // ── Commands ─────────────────────────────────────────────────────────

    /** Mode 2 protocol upload. Caller must pre-chunk to ≤512-byte writes. */
    fun writeProtocolChunk(chunk: ByteArray) = central.write(GattUuids.protocolUpload, chunk)

    fun requestSessionStop() = central.write(GattUuids.sessionStop, byteArrayOf(0x01))

    /** Mode 4: trigger EDF+ download for the given hub session ID (LE uint32). */
    fun requestEdfDownload(sessionId: Long) {
        val bytes = ByteArray(4) { i -> ((sessionId shr (8 * i)) and 0xFF).toByte() }
        central.write(GattUuids.edfRequest, bytes)
    }

    fun sendOtaCommand(opcode: OtaOpcode, payload: ByteArray = ByteArray(0)) =
        central.write(GattUuids.otaCommand, byteArrayOf(opcode.rawValue.toByte()) + payload)

    /** Trigger a calibration/setup command (impedance check, ADS1299 self-cal, etc.). */
    fun sendCalibration(opcode: com.neurone.core.ble.CalibrationOpcode) =
        central.write(GattUuids.calibrationCmd, byteArrayOf(opcode.rawValue.toByte()))

    // ── Internals ────────────────────────────────────────────────────────

    private fun startScanning() {
        _connectionState.value = ConnectionState.SCANNING
        central.startScan(GattUuids.service)
    }

    private fun applyDisconnection() {
        _connectionState.value = ConnectionState.DISCONNECTED
        _allCharacteristicsResolved.value = false
        _session.value = SessionState.EMPTY   // clear stale UHDR display state
        _hubFirmwareVersion.value = null
        _warrantyToken.value = null
    }

    private fun applyWarrantyToken(value: ByteArray) {
        // Reject short payloads; truncate long ones to 32 bytes (iOS parity).
        if (value.size < 32) return
        _warrantyToken.value = value.copyOf(32)
    }
}
