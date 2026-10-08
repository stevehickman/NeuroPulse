package life.neurone.shared.ble

import life.neurone.core.ble.GattParser
import life.neurone.core.ble.OtaOpcode
import life.neurone.core.common.InMemoryKeyValueStore
import life.neurone.core.common.KeyValueStore
import life.neurone.core.consumable.ConsumableResetQueue
import life.neurone.core.models.ActiveUserTag
import life.neurone.core.models.CervicalFaultLedger
import life.neurone.core.models.CervicalFaultRecord
import life.neurone.core.models.CervicalFaultStatus
import life.neurone.core.models.CervicalPadStatus
import life.neurone.core.models.OtaStatusPacket
import life.neurone.core.models.SessionState
import life.neurone.core.models.SocketMap
import life.neurone.core.models.SocketMapFrameAssembler
import life.neurone.core.models.ZoneModuleConfiguration
import life.neurone.core.models.ZoneModuleFrameAssembler
import life.neurone.core.protocol.GattUuidStrings
import life.neurone.core.protocol.PersistedKeys
import life.neurone.core.common.UUID
import kotlin.concurrent.Volatile
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
 *  - allCharacteristicsResolved requires all 14 of GattUuidStrings.REQUIRED_IDS;
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
    /** Holds the per-user offline-fault acknowledgement point (CervicalFaultLedger). */
    private val ledgerStore: KeyValueStore = InMemoryKeyValueStore(),
) : BleCentralListener {

    private val _connectionState = MutableStateFlow(ConnectionState.DISCONNECTED)
    val connectionState: StateFlow<ConnectionState> = _connectionState

    private val _session = MutableStateFlow(SessionState.EMPTY)
    val session: StateFlow<SessionState> = _session

    private val _bluetoothUnavailable = MutableStateFlow(false)
    val bluetoothUnavailable: StateFlow<Boolean> = _bluetoothUnavailable

    private val _allCharacteristicsResolved = MutableStateFlow(false)
    val allCharacteristicsResolved: StateFlow<Boolean> = _allCharacteristicsResolved

    /**
     * Live socket-keyed module inventory (ZONE_MODULE_STATUS). Sparse and variable-length —
     * replaces the retired fixed five-slot list (NP-HFE-002 OI-HFE2-02). SHDR-class.
     */
    private val _zoneModules = MutableStateFlow(ZoneModuleConfiguration.EMPTY)
    val zoneModules: StateFlow<ZoneModuleConfiguration> = _zoneModules

    /** This helmet's socket geometry (SOCKET_MAP), read once at link. SHDR-class. */
    private val _socketMap = MutableStateFlow(SocketMap.EMPTY)
    val socketMap: StateFlow<SocketMap> = _socketMap

    /**
     * True while the hub's encrypted `DEVICE_SERIAL` read is being refused, which on an unpaired link means the
     * operating system has not paired it (`OI-UI-KMP-03`). Cleared by a successful read or a disconnect. Display only.
     */
    private val _pairingRequired = MutableStateFlow(false)
    val pairingRequired: StateFlow<Boolean> = _pairingRequired

    private var serialRetryJob: Job? = null

    private val zoneFrameAssembler = ZoneModuleFrameAssembler()
    private val socketMapAssembler = SocketMapFrameAssembler()

    private val _otaStatus = MutableStateFlow<OtaStatusPacket?>(null)
    val otaStatus: StateFlow<OtaStatusPacket?> = _otaStatus

    private val _hubFirmwareVersion = MutableStateFlow<String?>(null)
    val hubFirmwareVersion: StateFlow<String?> = _hubFirmwareVersion

    /** SHDR-class opaque warranty token (NP-FW-EMMC-002 §A). Never mixed into session state. */
    private val _warrantyToken = MutableStateFlow<ByteArray?>(null)
    val warrantyToken: StateFlow<ByteArray?> = _warrantyToken

    /**
     * The hub's 32-byte replay-guard serial (NP-FW-HUB-001 §4.2), read over the encrypted link.
     * Memory only: neither persisted nor uploaded, and cleared on disconnect. null until read, and
     * before the read completes (OI-AND-WIRE-02).
     */
    @Volatile var deviceSerial: ByteArray? = null
        private set

    /**
     * Fires true when the hub signals a pending SHDR upload (SHDR_UPLOAD_STATUS
     * notifies 0x01). SHDR-class trigger only — carries no user biology and never
     * touches session state. Mirrors iOS NeurOneGATTManager.$shdrUploadPending
     * (SHDRUploadTriggering). The SHDR upload pipeline observes this in the
     * composition root (NeurOneApplication).
     */
    private val _shdrUploadPending = MutableStateFlow(false)
    val shdrUploadPending: StateFlow<Boolean> = _shdrUploadPending

    /**
     * A cervical VNS gel pad failure the wearer has not yet acknowledged (OI-ACC-07).
     * Set from CVNS_PAD_STATUS; cleared when the hub reports both pads passing, when the
     * wearer acknowledges it, or on disconnect. UHDR-class — display only, never persisted.
     * Mirrors iOS NeurOneGATTManager.cervicalPadAlert.
     */
    private val _cervicalPadAlert = MutableStateFlow<CervicalPadStatus?>(null)
    val cervicalPadAlert: StateFlow<CervicalPadStatus?> = _cervicalPadAlert

    /**
     * The hub's offline cervical-fault summary and re-enable state (NP-SW-FAULTMSG-001 P3/P4).
     * UHDR-class, held in memory, cleared on disconnect. Mirrors iOS cervicalFaultStatus.
     */
    private val _cervicalFaultStatus = MutableStateFlow<CervicalFaultStatus?>(null)
    val cervicalFaultStatus: StateFlow<CervicalFaultStatus?> = _cervicalFaultStatus

    /** Offline faults this user has not yet read — drives the summary sheet. */
    private val _unacknowledgedCervicalFaults = MutableStateFlow<List<CervicalFaultRecord>>(emptyList())
    val unacknowledgedCervicalFaults: StateFlow<List<CervicalFaultRecord>> = _unacknowledgedCervicalFaults

    /** The blanket warning has been read on this connection. Reset at every disconnect. */
    private val _cardiacWarningAcknowledged = MutableStateFlow(false)
    val cardiacWarningAcknowledged: StateFlow<Boolean> = _cardiacWarningAcknowledged

    private var activeUserCharPresent = false
    private var consumableCharPresent = false

    /** Replacements not yet written to the hub (OI-ACC-08); persisted, flushed at connect. */
    private val consumableResets = ConsumableResetQueue(ledgerStore)

    /**
     * The person using the device, from the active individual profile. Sent whenever it
     * changes and at every connect; null = the app has not named anyone, so the device keeps
     * assuming whoever it last knew.
     */
    var activeUserTag: Long? = null
        set(value) {
            if (field == value) return
            field = value
            sendActiveUserTag()
            // Offline faults are the active user's; re-read them against this user's ledger.
            _cervicalFaultStatus.value?.let {
                _unacknowledgedCervicalFaults.value = cervicalFaultLedger.unacknowledged(it.records)
            }
        }

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
            UUID.fromString(GattUuidStrings.SERVICE_ID),
            GattUuidStrings.REQUIRED_IDS.map(UUID::fromString) + listOf(
                UUID.fromString(GattUuidStrings.WARRANTY_TOKEN_ID), UUID.fromString(GattUuidStrings.FIRMWARE_VERSION_ID), UUID.fromString(GattUuidStrings.SOCKET_MAP_ID), UUID.fromString(GattUuidStrings.CVNS_PAD_STATUS_ID),
                UUID.fromString(GattUuidStrings.CVNS_FAULT_STATUS_ID), UUID.fromString(GattUuidStrings.CVNS_REENABLE_CONFIRM_ID), UUID.fromString(GattUuidStrings.ACTIVE_USER_ID),
                UUID.fromString(GattUuidStrings.DEVICE_SERIAL_ID),
            ),
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
        _allCharacteristicsResolved.value = characteristics.containsAll(GattUuidStrings.REQUIRED_IDS.map(UUID::fromString))
        GattUuidStrings.REQUIRED_IDS.map(UUID::fromString).forEach { central.enableNotifications(it) }
        if (UUID.fromString(GattUuidStrings.WARRANTY_TOKEN_ID) in characteristics) central.read(UUID.fromString(GattUuidStrings.WARRANTY_TOKEN_ID))
        if (UUID.fromString(GattUuidStrings.FIRMWARE_VERSION_ID) in characteristics) central.read(UUID.fromString(GattUuidStrings.FIRMWARE_VERSION_ID))
        if (UUID.fromString(GattUuidStrings.DEVICE_SERIAL_ID) in characteristics) central.read(UUID.fromString(GattUuidStrings.DEVICE_SERIAL_ID))
        // Inventory is read, not only awaited: the hub notifies on change, and nothing may have
        // changed since the last link (iOS parity).
        if (UUID.fromString(GattUuidStrings.ZONE_MODULE_STATUS_ID) in characteristics) central.read(UUID.fromString(GattUuidStrings.ZONE_MODULE_STATUS_ID))
        // Optional — hub firmware not yet shipped (OI-WA-03). Read once at link; subscribed so a
        // lattice-changing OTA can re-publish.
        if (UUID.fromString(GattUuidStrings.SOCKET_MAP_ID) in characteristics) {
            central.enableNotifications(UUID.fromString(GattUuidStrings.SOCKET_MAP_ID))
            central.read(UUID.fromString(GattUuidStrings.SOCKET_MAP_ID))
        }
        // Optional — T2 cervical accessory only, and hub firmware not yet shipped.
        if (UUID.fromString(GattUuidStrings.CVNS_PAD_STATUS_ID) in characteristics) central.enableNotifications(UUID.fromString(GattUuidStrings.CVNS_PAD_STATUS_ID))
        // Name the person before the fault summary is read: the summary is theirs.
        activeUserCharPresent = UUID.fromString(GattUuidStrings.ACTIVE_USER_ID) in characteristics
        sendActiveUserTag()
        // Replacements marked while disconnected, before the counts are read (OI-ACC-08).
        consumableCharPresent = UUID.fromString(GattUuidStrings.CONSUMABLE_STATUS_ID) in characteristics
        flushConsumableResets()
        if (UUID.fromString(GattUuidStrings.CVNS_FAULT_STATUS_ID) in characteristics) {
            central.enableNotifications(UUID.fromString(GattUuidStrings.CVNS_FAULT_STATUS_ID))
            central.read(UUID.fromString(GattUuidStrings.CVNS_FAULT_STATUS_ID))
        }
        // Restore session status immediately on (re)connect.
        if (UUID.fromString(GattUuidStrings.SESSION_STATUS_ID) in characteristics) central.read(UUID.fromString(GattUuidStrings.SESSION_STATUS_ID))
    }

    override fun onCharacteristicChanged(uuid: UUID, value: ByteArray) {
        // SHDR guards FIRST — structural UHDR/SHDR boundary (see class doc).
        if (uuid == UUID.fromString(GattUuidStrings.WARRANTY_TOKEN_ID)) {
            applyWarrantyToken(value)
            return
        }
        if (uuid == UUID.fromString(GattUuidStrings.DEVICE_SERIAL_ID)) {
            // Exactly 32 bytes or nothing: a short read must not be zero-padded into a serial no hub holds.
            deviceSerial = if (value.size == 32) value.copyOf() else null
            return
        }
        if (uuid == UUID.fromString(GattUuidStrings.SHDR_UPLOAD_STATUS_ID)) {
            // SHDR upload bookkeeping only; never touches session state. Publish the
            // pending trigger (0x01 = upload pending) for the SHDR upload pipeline.
            _shdrUploadPending.value = value.isNotEmpty() && value[0] == 0x01.toByte()
            return
        }
        if (uuid == UUID.fromString(GattUuidStrings.SOCKET_MAP_ID)) {
            // Device geometry — SHDR-class, never session state.
            applySocketMap(value)
            return
        }
        if (uuid == UUID.fromString(GattUuidStrings.ZONE_MODULE_STATUS_ID)) {
            // Socket occupancy, module type and module health are COMPONENT facts — SHDR-class,
            // handled here so they can never be folded into the UHDR session record.
            applyZoneModuleStatus(value)
            return
        }
        if (uuid == UUID.fromString(GattUuidStrings.CVNS_PAD_STATUS_ID)) {
            // UHDR-class, but not part of the session record — published on its own.
            applyCervicalPadStatus(value)
            return
        }
        if (uuid == UUID.fromString(GattUuidStrings.CVNS_FAULT_STATUS_ID)) {
            applyCervicalFaultStatus(value)
            return
        }
        when (uuid) {
            UUID.fromString(GattUuidStrings.SESSION_STATE_ID) -> GattParser.parseSessionState(value)?.let { epoch ->
                _session.value = _session.value.copy(epoch = epoch)
            }
            UUID.fromString(GattUuidStrings.SESSION_STATUS_ID) -> GattParser.parseSessionStatus(value)?.let { (pid, status) ->
                _session.value = _session.value.copy(protocolId = pid, status = status)
            }
            UUID.fromString(GattUuidStrings.HRV_COHERENCE_ID) -> GattParser.parseHrvCoherence(value)?.let { hrv ->
                _session.value = _session.value.copy(hrv = hrv)
            }
            UUID.fromString(GattUuidStrings.PACER_PHASE_ID) -> GattParser.parsePacerPhase(value)?.let { (phase, pct) ->
                _session.value = _session.value.copy(pacerPhase = phase, pacerElapsedPercent = pct)
            }
            UUID.fromString(GattUuidStrings.IMPEDANCE_RESULT_ID) -> GattParser.parseImpedanceResult(value)?.let { flags ->
                _session.value = _session.value.copy(impedancePassFlags = flags)
            }
            UUID.fromString(GattUuidStrings.CONSUMABLE_STATUS_ID) -> GattParser.parseConsumableStatus(value)?.let { counts ->
                _session.value = _session.value.copy(consumableSessionCounts = counts)
            }
            UUID.fromString(GattUuidStrings.OTA_STATUS_ID) -> GattParser.parseOtaStatus(value)?.let { _otaStatus.value = it }
            UUID.fromString(GattUuidStrings.FIRMWARE_VERSION_ID) -> GattParser.parseFirmwareVersion(value)?.let {
                _hubFirmwareVersion.value = it.toString()
            }
        }
    }

    override fun onCharacteristicRead(uuid: UUID, value: ByteArray) {
        if (uuid == UUID.fromString(GattUuidStrings.DEVICE_SERIAL_ID)) {
            _pairingRequired.value = false
            serialRetryJob?.cancel()
        }
        onCharacteristicChanged(uuid, value)
    }

    /**
     * A refused serial read is retried every [SERIAL_RETRY_MS] while the link stays up, so the user can pair the hub in
     * the operating system's settings without reconnecting. Other refused reads are not retried.
     */
    override fun onCharacteristicReadFailed(uuid: UUID) {
        val serial = UUID.fromString(GattUuidStrings.DEVICE_SERIAL_ID)
        if (uuid != serial || _connectionState.value != ConnectionState.CONNECTED) return
        _pairingRequired.value = true
        serialRetryJob?.cancel()
        serialRetryJob = scope.launch {
            delay(SERIAL_RETRY_MS)
            if (_connectionState.value == ConnectionState.CONNECTED) central.read(serial)
        }
    }

    // ── Commands ─────────────────────────────────────────────────────────

    /** Mode 2 protocol upload. Caller must pre-chunk to ≤512-byte writes. */
    fun writeProtocolChunk(chunk: ByteArray) = central.write(UUID.fromString(GattUuidStrings.PROTOCOL_UPLOAD_ID), chunk)

    fun requestSessionStop() = central.write(UUID.fromString(GattUuidStrings.SESSION_STOP_ID), byteArrayOf(0x01))

    /** Mode 4: trigger EDF+ download for the given hub session ID (LE uint32). */
    fun requestEdfDownload(sessionId: Long) {
        val bytes = ByteArray(4) { i -> ((sessionId shr (8 * i)) and 0xFF).toByte() }
        central.write(UUID.fromString(GattUuidStrings.EDF_REQUEST_ID), bytes)
    }

    fun sendOtaCommand(opcode: OtaOpcode, payload: ByteArray = ByteArray(0)) =
        central.write(UUID.fromString(GattUuidStrings.OTA_COMMAND_ID), byteArrayOf(opcode.rawValue.toByte()) + payload)

    /** Trigger a calibration/setup command (impedance check, ADS1299 self-cal, etc.). */
    fun sendCalibration(opcode: life.neurone.core.ble.CalibrationOpcode) =
        central.write(UUID.fromString(GattUuidStrings.CALIBRATION_CMD_ID), byteArrayOf(opcode.rawValue.toByte()))

    /**
     * The wearer has read the alert. The hub has already refused or stopped stimulation —
     * acknowledging changes nothing on the device; the next failure raises a new alert.
     */
    fun acknowledgeCervicalPadAlert() {
        _cervicalPadAlert.value = null
    }

    /**
     * Decode one ZONE_MODULE_STATUS fragment and fold it into the inventory. Malformed frames
     * are dropped whole — presence gates placement checks, so a frame that is not exactly
     * well-formed must not be partially believed. A multi-fragment snapshot is committed only
     * when its final fragment arrives.
     */
    fun applyZoneModuleStatus(value: ByteArray) {
        val frame = GattParser.parseZoneModuleStatus(value) ?: return
        val complete = zoneFrameAssembler.accept(frame) ?: return
        _zoneModules.value = _zoneModules.value.applying(complete)
    }

    /** Decode one SOCKET_MAP fragment; a completed run replaces the map wholesale. */
    fun applySocketMap(value: ByteArray) {
        val frame = GattParser.parseSocketMap(value) ?: return
        val complete = socketMapAssembler.accept(frame) ?: return
        _socketMap.value = _socketMap.value.applying(complete)
    }

    /** Decodes CVNS_FAULT_STATUS; a malformed frame changes nothing. */
    fun applyCervicalFaultStatus(value: ByteArray) {
        val status = GattParser.parseCervicalFaultStatus(value) ?: return
        _cervicalFaultStatus.value = status
        _unacknowledgedCervicalFaults.value = cervicalFaultLedger.unacknowledged(status.records)
    }

    /** The wearer has read the offline-fault summary. Moves this user's ledger past these records. */
    fun acknowledgeCervicalFaults() {
        val records = _cervicalFaultStatus.value?.records ?: return
        cervicalFaultLedger = cervicalFaultLedger.acknowledge(records)
        _unacknowledgedCervicalFaults.value = emptyList()
    }

    /**
     * True while a cardiac cutoff from an offline session is unread: the protocol uploader then
     * refuses a protocol containing cervical VNS.
     */
    val cervicalRestartBlocked: Boolean
        get() = _cervicalFaultStatus.value?.let { cervicalFaultLedger.blocksCervicalRestart(it.records) } ?: false

    /** Someone other than the active user has an outstanding cardiac cutoff. */
    val cervicalOutstandingForAnotherUser: Boolean
        get() = _cervicalFaultStatus.value?.outstandingForAnotherUser ?: false

    /** The wearer has read this connection's blanket warning. */
    fun acknowledgeCardiacWarning() {
        _cardiacWarningAcknowledged.value = true
    }

    /**
     * The wearer confirms resuming cervical VNS after a cardiac cutoff. The hub accepts it only
     * while awaiting a confirmation, then re-checks the pads; its next CVNS_FAULT_STATUS shows
     * the outcome. Returns false when the hub has no such characteristic or is not connected.
     */
    fun sendCervicalReenableConfirm(): Boolean {
        if (_connectionState.value != ConnectionState.CONNECTED ||
            _cervicalFaultStatus.value?.reenableState != CervicalFaultStatus.ReenableState.AWAIT_CONFIRM
        ) return false
        central.write(UUID.fromString(GattUuidStrings.CVNS_REENABLE_CONFIRM_ID), byteArrayOf(0x01))
        return true
    }

    /**
     * The wearer replaced a consumable (ConsumableTracker.markReplaced). The hub owns the count,
     * so it is told to zero it: written now if connected, else queued for the next connect.
     */
    fun requestConsumableReset(kind: Int) {
        consumableResets.add(kind)
        flushConsumableResets()
    }

    // ── Internals ────────────────────────────────────────────────────────

    private fun flushConsumableResets() {
        if (!consumableCharPresent || _connectionState.value != ConnectionState.CONNECTED) return
        consumableResets.drain { central.write(UUID.fromString(GattUuidStrings.CONSUMABLE_STATUS_ID), it) }
    }

    private val cervicalFaultLedgerKey: String
        get() = PersistedKeys.CERVICAL_FAULT_LEDGER_KEY + "." + (activeUserTag?.toString() ?: "unnamed")

    private var cervicalFaultLedger: CervicalFaultLedger
        get() = CervicalFaultLedger(ledgerStore.getString(cervicalFaultLedgerKey)?.toLongOrNull())
        set(value) {
            val n = value.lastAcknowledgedSession
            if (n == null) ledgerStore.remove(cervicalFaultLedgerKey)
            else ledgerStore.putString(cervicalFaultLedgerKey, n.toString())
        }

    private fun sendActiveUserTag() {
        val tag = activeUserTag ?: return
        if (!activeUserCharPresent || _connectionState.value != ConnectionState.CONNECTED) return
        central.write(UUID.fromString(GattUuidStrings.ACTIVE_USER_ID), ActiveUserTag.toWire(tag))
    }

    /** A failure raises the alert; a both-pads-pass frame clears it; a malformed frame changes nothing. */
    private fun applyCervicalPadStatus(value: ByteArray) {
        val status = GattParser.parseCervicalPadStatus(value) ?: return
        _cervicalPadAlert.value = status.takeIf { it.hasFailure }
    }

    private fun startScanning() {
        _connectionState.value = ConnectionState.SCANNING
        central.startScan(UUID.fromString(GattUuidStrings.SERVICE_ID))
    }

    private fun applyDisconnection() {
        serialRetryJob?.cancel()
        _pairingRequired.value = false
        _connectionState.value = ConnectionState.DISCONNECTED
        _allCharacteristicsResolved.value = false
        _session.value = SessionState.EMPTY   // clear stale UHDR display state
        // Clear the inventory rather than zeroing a fixed shape: the hub sends a fresh snapshot
        // on reconnect, and a stale "present" socket would let a placement gate pass on a module
        // pulled while disconnected. The map goes too — the next link may be another helmet.
        _zoneModules.value = ZoneModuleConfiguration.EMPTY
        zoneFrameAssembler.reset()
        _socketMap.value = SocketMap.EMPTY
        socketMapAssembler.reset()
        _hubFirmwareVersion.value = null
        _warrantyToken.value = null
        deviceSerial = null
        _cervicalPadAlert.value = null
        _cervicalFaultStatus.value = null
        _unacknowledgedCervicalFaults.value = emptyList()
        _cardiacWarningAcknowledged.value = false
        activeUserCharPresent = false
        consumableCharPresent = false
        // Reset the SHDR upload trigger — it is tied to a live connection; the hub
        // re-signals 0x01 on reconnect (retry on next USB-C session, iOS parity).
        _shdrUploadPending.value = false
    }

    private fun applyWarrantyToken(value: ByteArray) {
        // Reject short payloads; truncate long ones to 32 bytes (iOS parity).
        if (value.size < 32) return
        _warrantyToken.value = value.copyOf(32)
    }

    private companion object {
        /** UC-078 (timings): unmeasured. */
        const val SERIAL_RETRY_MS = 5_000L
    }
}
