package life.neurone.shared.ble

import life.neurone.core.ble.GattParser
import life.neurone.core.ble.OtaOpcode
import life.neurone.core.common.InMemoryKeyValueStore
import life.neurone.core.common.KeyValueStore
import life.neurone.core.consumable.ConsumableResetQueue
import life.neurone.core.models.ActiveUserTag
import life.neurone.core.protocol.ProfileTagState
import life.neurone.core.models.CervicalFaultLedger
import life.neurone.core.models.CervicalFaultRecord
import life.neurone.core.models.CervicalFaultStatus
import life.neurone.core.models.CardiacSensitivity
import life.neurone.core.models.HoldCause
import life.neurone.core.models.PauseLimit
import life.neurone.core.models.PpxConfig
import life.neurone.core.models.PpxSensitivity
import life.neurone.core.models.SessionProgress
import life.neurone.core.models.UploadCheck
import life.neurone.core.models.CervicalPadStatus
import life.neurone.core.models.OtaStatusPacket
import life.neurone.core.models.SessionState
import life.neurone.core.models.SocketMap
import life.neurone.core.models.SocketMapFrameAssembler
import life.neurone.core.models.ZoneModuleConfiguration
import life.neurone.core.models.ZoneModuleFrameAssembler
import life.neurone.core.protocol.BleTransfer
import life.neurone.core.protocol.GattUuidStrings
import life.neurone.core.protocol.PersistedKeys
import life.neurone.core.common.UUID
import kotlin.concurrent.Volatile
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Job
import kotlinx.coroutines.delay
import kotlinx.coroutines.channels.BufferOverflow
import kotlinx.coroutines.flow.MutableSharedFlow
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.SharedFlow
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
    /** The presence lock when the host gates UHDR: the acknowledgement points and the queued consumable resets are sealed (`OI-PRIV-19`). Null on a host with no gate. */
    presenceLock: life.neurone.core.common.PresenceLock? = null,
) : BleCentralListener, life.neurone.shared.ota.OtaLink, life.neurone.shared.setup.SetupLink {

    private val _connectionState = MutableStateFlow(ConnectionState.DISCONNECTED)
    override val connectionState: StateFlow<ConnectionState> = _connectionState

    private val _session = MutableStateFlow(SessionState.EMPTY)
    val session: StateFlow<SessionState> = _session

    /** Where the helmet says the session is (SWR-SS-07): elapsed, remaining, active part, ramp. Null before the first frame. */
    private val _progress = MutableStateFlow<SessionProgress?>(null)
    val progress: StateFlow<SessionProgress?> = _progress

    /**
     * The helmet's hardware version (SWR-HV-06), read from the helmet and not assumed. Until it is read, or on a helmet
     * whose firmware does not publish it, it is [DEFAULT_HARDWARE_VERSION].
     */
    private val _hardwareVersion = MutableStateFlow(DEFAULT_HARDWARE_VERSION)
    val hardwareVersion: StateFlow<Int> = _hardwareVersion

    /**
     * The unit's tier as the hub reports it (DEVICE_TIER, OI-HWFIT-01): the safety MCU's verdict from its signed tier identity,
     * relayed and never decided here. Null until read, and for a hub whose firmware does not publish it. It only says what
     * the helmet can run; the hub and the safety MCU gate a T2 protocol whatever the app believes (REQ-UPG-01).
     */
    private val _deviceTier = MutableStateFlow<life.neurone.core.ble.HubTier?>(null)
    val deviceTier: StateFlow<life.neurone.core.ble.HubTier?> = _deviceTier

    /**
     * What the helmet says it received at the last protocol upload: its hash of the bytes and an accept or refuse verdict
     * (SWR-UP-01). Null before the first report and after a dropped link. The hash is of protocol bytes the person authored
     * and carries no user biology.
     */
    private val _uploadCheck = MutableStateFlow<UploadCheck?>(null)
    val uploadCheck: StateFlow<UploadCheck?> = _uploadCheck

    private var uploadCheckCharPresent = false

    /** Ask the helmet for its current upload report (a READ, so a missed notification does not leave the outcome unknown). */
    fun requestUploadCheck() {
        if (uploadCheckCharPresent) central.read(UUID.fromString(GattUuidStrings.UPLOAD_CHECK_ID))
    }

    /**
     * The hardware owner's pause time limit as the helmet holds it, in seconds; 0 is no limit, null is not known (not read
     * yet, the helmet does not publish it, or the link is not paired: it is an encrypted characteristic). SWR-SS-05.
     */
    private val _pauseLimitSeconds = MutableStateFlow<Long?>(null)
    val pauseLimitSeconds: StateFlow<Long?> = _pauseLimitSeconds

    private var pauseLimitCharPresent = false

    private val _pauseLimitPublished = MutableStateFlow(false)

    /** True when this helmet publishes the pause limit, so the owner screen can offer it. */
    val pauseLimitPublished: StateFlow<Boolean> = _pauseLimitPublished

    /** Ask the helmet for its pause limit (an encrypted READ, which is refused until the helmet is paired). */
    fun requestPauseLimit() {
        if (pauseLimitCharPresent) central.read(UUID.fromString(GattUuidStrings.PAUSE_LIMIT_ID))
    }

    /**
     * Set the pause limit, in seconds; 0 is no limit. Takes effect at the helmet's NEXT pause: the limit in force when a
     * pause begins is the limit for that pause. The write is followed by a read, so the screen shows what the helmet holds
     * and not what was asked for. Returns false when the helmet publishes none or the value does not fit.
     */
    fun writePauseLimit(seconds: Long): Boolean {
        val bytes = PauseLimit.toWire(seconds) ?: return false
        if (!pauseLimitCharPresent) return false
        if (!writeEncrypted(UUID.fromString(GattUuidStrings.PAUSE_LIMIT_ID), bytes)) return false
        central.read(UUID.fromString(GattUuidStrings.PAUSE_LIMIT_ID))
        return true
    }

    /**
     * The photoparoxysmal detector's two clinician switches and its evidence level as the helmet holds them (REQ-FWHUB-71),
     * null when not known: not read yet, the helmet does not publish it, or the link is not paired (it is encrypted).
     */
    private val _ppxConfig = MutableStateFlow<PpxConfig?>(null)
    val ppxConfig: StateFlow<PpxConfig?> = _ppxConfig

    private var ppxConfigCharPresent = false

    private val _ppxConfigPublished = MutableStateFlow(false)

    /** True when this helmet publishes the detector's settings, so the owner screen can offer them. */
    val ppxConfigPublished: StateFlow<Boolean> = _ppxConfigPublished

    /** Ask the helmet for the detector's settings (an encrypted READ, refused until the helmet is paired). */
    fun requestPpxConfig() {
        if (ppxConfigCharPresent) central.read(UUID.fromString(GattUuidStrings.PPX_CONFIG_ID))
    }

    /**
     * Set the detector's two switches. Turning "stop on occipital-only" on reaches a session already running; turning it off,
     * and the override either way, apply from the next. The helmet refuses the override while its detector has no evidence.
     * The write is followed by a read, so the screen shows what the helmet holds and not what was asked for. Returns false
     * when the helmet publishes none.
     */
    fun writePpxConfig(stopOnOccipitalOnly: Boolean, manualOverride: Boolean): Boolean {
        if (!ppxConfigCharPresent) return false
        if (!writeEncrypted(UUID.fromString(GattUuidStrings.PPX_CONFIG_ID), PpxConfig.toWire(stopOnOccipitalOnly, manualOverride))) return false
        central.read(UUID.fromString(GattUuidStrings.PPX_CONFIG_ID))
        return true
    }

    /**
     * Why the helmet is holding the session paused (SWR-SS-09), null when not known: not read yet, a helmet that does not publish
     * it, or a byte this app does not know. The app puts the Stop / Resume question to the person when the helmet halted on
     * its own ([HoldCause.isAutomated]); a pause or a panic the person made they already know about.
     */
    private val _holdCause = MutableStateFlow<HoldCause?>(null)
    val holdCause: StateFlow<HoldCause?> = _holdCause

    /** The detector's sensitivity level and the highest the helmet offers (REQ-FWHUB-68), null when not known. */
    private val _ppxSensitivity = MutableStateFlow<PpxSensitivity?>(null)
    val ppxSensitivity: StateFlow<PpxSensitivity?> = _ppxSensitivity

    private var ppxSensitivityCharPresent = false

    private val _ppxSensitivityPublished = MutableStateFlow(false)

    /** True when this helmet publishes the sensitivity, so the owner screen can offer it. */
    val ppxSensitivityPublished: StateFlow<Boolean> = _ppxSensitivityPublished

    /** Ask the helmet for the detector's sensitivity (an encrypted READ, refused until the helmet is paired). */
    fun requestPpxSensitivity() {
        if (ppxSensitivityCharPresent) central.read(UUID.fromString(GattUuidStrings.PPX_SENSITIVITY_ID))
    }

    /**
     * Set the detector's sensitivity level. Raising it reaches a session already running; lowering it applies from the next. Level
     * 0 is the validated setting and the helmet has none below it. The write is followed by a read, so the screen shows what the
     * helmet holds. Returns false when the helmet publishes none or the level is negative.
     */
    fun writePpxSensitivity(level: Int): Boolean {
        val bytes = PpxSensitivity.toWire(level) ?: return false
        if (!ppxSensitivityCharPresent) return false
        if (!writeEncrypted(UUID.fromString(GattUuidStrings.PPX_SENSITIVITY_ID), bytes)) return false
        central.read(UUID.fromString(GattUuidStrings.PPX_SENSITIVITY_ID))
        return true
    }

    /** The cervical VNS cardiac cutoff's sensitivity level and the highest the helmet offers (REQ-FWHUB-74), null when not known. */
    private val _cardiacSensitivity = MutableStateFlow<CardiacSensitivity?>(null)
    val cardiacSensitivity: StateFlow<CardiacSensitivity?> = _cardiacSensitivity

    private var cardiacSensitivityCharPresent = false

    private val _cardiacSensitivityPublished = MutableStateFlow(false)

    /** True when this helmet publishes the cutoff's sensitivity, so the owner screen can offer it. */
    val cardiacSensitivityPublished: StateFlow<Boolean> = _cardiacSensitivityPublished

    /** Ask the helmet for the cutoff's sensitivity (an encrypted READ, refused until the helmet is paired). */
    fun requestCardiacSensitivity() {
        if (cardiacSensitivityCharPresent) central.read(UUID.fromString(GattUuidStrings.CARDIAC_SENSITIVITY_ID))
    }

    /**
     * Set the cardiac cutoff's sensitivity level. Raising it reaches a cervical VNS session already running; lowering it applies
     * from the next one. Level 0 is the validated cutoff and the helmet has none below it. The write is followed by a read, so the
     * screen shows what the helmet holds. Returns false when the helmet publishes none or the level is negative.
     */
    fun writeCardiacSensitivity(level: Int): Boolean {
        val bytes = CardiacSensitivity.toWire(level) ?: return false
        if (!cardiacSensitivityCharPresent) return false
        if (!writeEncrypted(UUID.fromString(GattUuidStrings.CARDIAC_SENSITIVITY_ID), bytes)) return false
        central.read(UUID.fromString(GattUuidStrings.CARDIAC_SENSITIVITY_ID))
        return true
    }

    // ── Stealth (STEALTH 0x0022; NP-FW-HUB-001 §5.5a, REQ-FWHUB-86; SWR-IND-01, SWR-IND-02; OI-PRIV-37) ──
    //
    // The hub holds stealth in RAM and boots with it off, so the app keeps the person's choice with their profile and sends it at
    // every link. Whatever stealth says, a fault always shows red (CLAUDE.md §4.7). UHDR-class: written only on an encrypted link
    // ([writeEncrypted]), never logged or uploaded.

    /** What the helmet holds: true dark, false lit, null when not read on this link. */
    private val _stealthOnHub = MutableStateFlow<Boolean?>(null)
    val stealthOnHub: StateFlow<Boolean?> = _stealthOnHub

    private var stealthCharPresent = false

    private val _stealthPublished = MutableStateFlow(false)

    /** True when this helmet publishes STEALTH, so a screen can offer the switch. */
    val stealthPublished: StateFlow<Boolean> = _stealthPublished

    /**
     * What the app asks the helmet to show: the active profile's choice, or off when the warranty owner forbids stealth on this
     * helmet ([life.neurone.core.models.StealthFlag.desired]). Sent whenever it changes and at every link once the link is shown
     * to be encrypted. Null: nothing is sent, and the helmet keeps what it holds.
     */
    var desiredStealth: Boolean? = null
        set(value) {
            if (field == value) return
            field = value
            sendStealth()
        }

    /** Ask the helmet what it holds (an encrypted READ, refused until the helmet is paired). */
    fun requestStealth() {
        if (stealthCharPresent) central.read(UUID.fromString(GattUuidStrings.STEALTH_ID))
    }

    private fun sendStealth() {
        val on = desiredStealth ?: return
        if (!stealthCharPresent || _connectionState.value != ConnectionState.CONNECTED) return
        val id = UUID.fromString(GattUuidStrings.STEALTH_ID)
        // The read follows the write, so what is shown is what the helmet holds, not what was asked.
        if (writeEncrypted(id, life.neurone.core.models.StealthFlag.toWire(on))) central.read(id)
    }

    private val _bluetoothUnavailable = MutableStateFlow(false)
    val bluetoothUnavailable: StateFlow<Boolean> = _bluetoothUnavailable

    private val _allCharacteristicsResolved = MutableStateFlow(false)
    val allCharacteristicsResolved: StateFlow<Boolean> = _allCharacteristicsResolved

    /**
     * Live socket-keyed module inventory (ZONE_MODULE_STATUS). Sparse and variable-length —
     * replaces the retired fixed five-slot list (NP-HFE-002 OI-HFE2-02). SHDR-class.
     */
    private val _zoneModules = MutableStateFlow(ZoneModuleConfiguration.EMPTY)
    override val zoneModules: StateFlow<ZoneModuleConfiguration> = _zoneModules

    /** The latest IMPEDANCE_RESULT notification and how many have come, so a wait can tell a fresh result from the one before it. */
    private val _impedanceReading = MutableStateFlow<life.neurone.shared.setup.ImpedanceReading?>(null)
    override val impedanceReading: StateFlow<life.neurone.shared.setup.ImpedanceReading?> = _impedanceReading

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

    /**
     * True once the hub's `DEVICE_SERIAL` has been read on this link, and false again when the link drops. The hub
     * publishes that characteristic for an encrypted link only (`NP_GATT_PROP_ENC`, `REQ-FWHUB-81`), so a protocol
     * upload is not written until this is true (`REQ-SW03-31`, `OI-PRIV-16`): a peer that never answers the read is
     * never sent a protocol.
     *
     * The limit: this shows that a read of that characteristic succeeded on this link. A genuine hub answers it only
     * over an encrypted link; a peripheral that publishes the same UUID without demanding encryption answers it in the
     * clear and passes. It does not show the peer is a genuine hub, and it does not show how the link was paired
     * (`OI-SIMFW-72`). Payload encryption is what closes that (`OI-SIMFW-62`).
     */
    private val _encryptedReadShown = MutableStateFlow(false)
    val encryptedReadShown: StateFlow<Boolean> = _encryptedReadShown

    /**
     * True when the link refused a write of the protocol upload since the last upload began ([beginProtocolUpload]):
     * the helmet does not hold what was sent. Cleared when the next upload begins and when the link drops.
     */
    private val _uploadWriteRefused = MutableStateFlow(false)
    val uploadWriteRefused: StateFlow<Boolean> = _uploadWriteRefused

    /** Every write the link refused, by characteristic, for a screen or a test to act on. Nothing is replayed. */
    private val _writeRefusals = MutableSharedFlow<UUID>(extraBufferCapacity = 16, onBufferOverflow = BufferOverflow.DROP_OLDEST)
    val writeRefusals: SharedFlow<UUID> = _writeRefusals

    /** Whether there is a remembered hub to forget (desktop only). */
    val canForgetHub: Boolean get() = central.canForgetHub

    /** Drops the link and the remembered hub, then scans afresh, so the user can pick a different hub. */
    fun forgetHub() {
        central.disconnect()
        central.forgetHub()
        applyDisconnection()
        if (central.adapterState == AdapterState.ON) startScanning()
    }

    private val zoneFrameAssembler = ZoneModuleFrameAssembler()
    private val socketMapAssembler = SocketMapFrameAssembler()

    private val _otaStatus = MutableStateFlow<OtaStatusPacket?>(null)
    override val otaStatus: StateFlow<OtaStatusPacket?> = _otaStatus

    private val _hubFirmwareVersion = MutableStateFlow<String?>(null)
    override val hubFirmwareVersion: StateFlow<String?> = _hubFirmwareVersion

    /** SHDR-class opaque warranty token (NP-FW-EMMC-002 §A). Never mixed into session state. */
    private val _warrantyToken = MutableStateFlow<ByteArray?>(null)
    val warrantyToken: StateFlow<ByteArray?> = _warrantyToken

    /**
     * Which helmet the link is to (the central's id for the peripheral), `null` when not connected. The SHDR token is kept
     * per helmet by it (REQ-SHDR-05).
     */
    private val _hubIdentifier = MutableStateFlow<String?>(null)
    val hubIdentifier: StateFlow<String?> = _hubIdentifier

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
     * One event per SHDR_UPLOAD_STATUS notification of 0x01, including a repeat of the same value. The Swift app's `@Published`
     * property fires on every assignment, so a hub that notifies again (because the last attempt did not take the record) starts
     * another attempt; a state flow drops an equal value and would not. The upload pipeline listens to this.
     */
    private val _shdrUploadSignal = MutableSharedFlow<Unit>(extraBufferCapacity = 1, onBufferOverflow = BufferOverflow.DROP_OLDEST)
    val shdrUploadSignal: SharedFlow<Unit> = _shdrUploadSignal

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
    private val consumableResets = ConsumableResetQueue(ledgerStore, presenceLock, onUnlocked = { flushConsumableResets() })

    /** One sealed value for every person's acknowledgement point; the cardiac faults are re-read against it when the lock opens. */
    private val faultLedgers = life.neurone.core.models.CervicalFaultLedgerStore(ledgerStore, presenceLock) {
        _cervicalFaultStatus.value?.let { _unacknowledgedCervicalFaults.value = cervicalFaultLedger.unacknowledged(it.records) }
    }

    /** The person using the device: the active individual profile and how it is named to a helmet (`REQ-FAULTMSG-01`). */
    data class ActiveUser(val profileId: String, val tagState: ProfileTagState)

    /**
     * The person using the device, from the active individual profile; null = the app has not named anyone, so the device
     * keeps assuming whoever it last knew. Their tag is worked out per helmet once that helmet's serial has been read
     * ([recomputeActiveUserTag]).
     */
    var activeUser: ActiveUser? = null
        set(value) {
            if (field == value) return
            field = value
            recomputeActiveUserTag()
        }

    /**
     * Records a profile's move to its per-helmet tag before the tag is named (`REQ-FAULTMSG-01` (c)); true once it is
     * persisted. Until it answers true the old tag stays, so a restart can never bring the old tag back to a helmet that may
     * now hold a cutoff under the new one. The composition root points it at `NPLimitsStore.recordTagMigration`.
     */
    var recordTagMigration: ((profileId: String, tag: Long) -> Boolean)? = null

    /**
     * The tag named to the helmet on this link; null before its serial is read and after a disconnect. Set by
     * [recomputeActiveUserTag]; the setter is module-internal so the ledger tests can name a person without a helmet.
     */
    var activeUserTag: Long? = null
        internal set(value) {
            if (field == value) return
            field = value
            sendActiveUserTag()
            // Offline faults are the active user's; re-read them against this user's ledger.
            _cervicalFaultStatus.value?.let {
                _unacknowledgedCervicalFaults.value = cervicalFaultLedger.unacknowledged(it.records)
            }
        }

    /**
     * Work out the tag to name to this helmet ([ActiveUserTag.choose]) and name it if it changed. Run when the person, the
     * helmet's serial or its fault status changes. A move to the per-helmet tag is recorded first; if it cannot be, the old
     * tag is named instead.
     */
    private fun recomputeActiveUserTag() {
        val user = activeUser
        val serial = deviceSerial
        // Nothing to name yet: the helmet keeps whoever it last knew.
        if (user == null || serial == null || !_encryptedReadShown.value) return
        val choice = ActiveUserTag.choose(user.profileId, user.tagState, serial, _cervicalFaultStatus.value) ?: return
        var tag = choice.tag
        val moveTo = choice.moveTo
        if (moveTo != null) {
            if (recordTagMigration?.invoke(user.profileId, moveTo) == true) {
                activeUser = user.copy(tagState = user.tagState.copy(migrated = user.tagState.migrated + moveTo))
            } else {
                tag = ActiveUserTag.legacy(user.profileId) ?: return
            }
        }
        activeUserTag = tag
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
        _hubIdentifier.value = deviceId
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
        // SWR-HV-06: optional (hub firmware); read once at link. It never changes in the field.
        if (UUID.fromString(GattUuidStrings.HARDWARE_VERSION_ID) in characteristics) {
            central.read(UUID.fromString(GattUuidStrings.HARDWARE_VERSION_ID))
        }
        // OI-HWFIT-01: optional (hub firmware); read once at link. The tier never changes in the field.
        if (UUID.fromString(GattUuidStrings.DEVICE_TIER_ID) in characteristics) {
            central.read(UUID.fromString(GattUuidStrings.DEVICE_TIER_ID))
        }
        // SWR-UP-01: optional (hub firmware); read at link and notified, and read again after each upload.
        uploadCheckCharPresent = UUID.fromString(GattUuidStrings.UPLOAD_CHECK_ID) in characteristics
        if (uploadCheckCharPresent) {
            central.enableNotifications(UUID.fromString(GattUuidStrings.UPLOAD_CHECK_ID))
            central.read(UUID.fromString(GattUuidStrings.UPLOAD_CHECK_ID))
        }
        // SWR-SS-05: optional (hub firmware); encrypted, so the read is refused until the helmet is paired.
        pauseLimitCharPresent = UUID.fromString(GattUuidStrings.PAUSE_LIMIT_ID) in characteristics
        _pauseLimitPublished.value = pauseLimitCharPresent
        if (pauseLimitCharPresent) central.read(UUID.fromString(GattUuidStrings.PAUSE_LIMIT_ID))
        // REQ-FWHUB-74: optional (hub firmware); encrypted, so the read is refused until the helmet is paired.
        cardiacSensitivityCharPresent = UUID.fromString(GattUuidStrings.CARDIAC_SENSITIVITY_ID) in characteristics
        _cardiacSensitivityPublished.value = cardiacSensitivityCharPresent
        if (cardiacSensitivityCharPresent) central.read(UUID.fromString(GattUuidStrings.CARDIAC_SENSITIVITY_ID))
        // REQ-FWHUB-68: optional (hub firmware); encrypted, so the read is refused until the helmet is paired.
        ppxSensitivityCharPresent = UUID.fromString(GattUuidStrings.PPX_SENSITIVITY_ID) in characteristics
        _ppxSensitivityPublished.value = ppxSensitivityCharPresent
        if (ppxSensitivityCharPresent) central.read(UUID.fromString(GattUuidStrings.PPX_SENSITIVITY_ID))
        // REQ-FWHUB-71: optional (hub firmware); encrypted, so the read is refused until the helmet is paired.
        ppxConfigCharPresent = UUID.fromString(GattUuidStrings.PPX_CONFIG_ID) in characteristics
        _ppxConfigPublished.value = ppxConfigCharPresent
        if (ppxConfigCharPresent) central.read(UUID.fromString(GattUuidStrings.PPX_CONFIG_ID))
        // SWR-IND-01: optional (hub firmware); encrypted, so the read and the write wait for the helmet to be paired.
        stealthCharPresent = UUID.fromString(GattUuidStrings.STEALTH_ID) in characteristics
        _stealthPublished.value = stealthCharPresent
        if (stealthCharPresent) {
            central.read(UUID.fromString(GattUuidStrings.STEALTH_ID))
            sendStealth()
        }
        // SWR-SS-09: optional (hub firmware): why a session is held, so a halt the helmet made on its own is put to the person.
        if (UUID.fromString(GattUuidStrings.SESSION_HOLD_ID) in characteristics) {
            central.enableNotifications(UUID.fromString(GattUuidStrings.SESSION_HOLD_ID))
            central.read(UUID.fromString(GattUuidStrings.SESSION_HOLD_ID))
        }
        // SWR-SS-07: read once at link so a session already running shows its figures, then notified on the telemetry tick.
        if (UUID.fromString(GattUuidStrings.SESSION_PROGRESS_ID) in characteristics) {
            central.enableNotifications(UUID.fromString(GattUuidStrings.SESSION_PROGRESS_ID))
            central.read(UUID.fromString(GattUuidStrings.SESSION_PROGRESS_ID))
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
            // Set here and not only in onCharacteristicRead: CoreBluetooth delivers a read's value and a notification
            // through one callback, so the iOS central reports a read as a change. The hub never notifies this
            // characteristic and the app never subscribes to it, so a value here is the answer to a read.
            // Only the 32-byte serial counts: a short or long value is not the hub's answer and shows nothing.
            if (value.size == 32 && _connectionState.value == ConnectionState.CONNECTED) {
                val wasShown = _encryptedReadShown.value
                _encryptedReadShown.value = true
                // What was held at the gate goes now (OI-PRIV-17): the active user's tag, and replacements marked meanwhile.
                if (!wasShown) {
                    // A tag worked out now is sent as it is set; one set before the gate opened was held, and goes now.
                    val before = activeUserTag
                    recomputeActiveUserTag()
                    if (activeUserTag == before) sendActiveUserTag()
                    flushConsumableResets()
                    sendStealth()
                }
            }
            return
        }
        if (uuid == UUID.fromString(GattUuidStrings.SHDR_UPLOAD_STATUS_ID)) {
            // SHDR upload bookkeeping only; never touches session state. Publish the
            // pending trigger (0x01 = upload pending) for the SHDR upload pipeline.
            val pending = value.isNotEmpty() && value[0] == 0x01.toByte()
            _shdrUploadPending.value = pending
            if (pending) _shdrUploadSignal.tryEmit(Unit)
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
            UUID.fromString(GattUuidStrings.HARDWARE_VERSION_ID) ->
                GattParser.parseHardwareVersion(value)?.let { _hardwareVersion.value = it }
            UUID.fromString(GattUuidStrings.DEVICE_TIER_ID) -> _deviceTier.value = GattParser.parseDeviceTier(value)
            UUID.fromString(GattUuidStrings.UPLOAD_CHECK_ID) -> _uploadCheck.value = GattParser.parseUploadCheck(value)
            UUID.fromString(GattUuidStrings.PAUSE_LIMIT_ID) -> _pauseLimitSeconds.value = GattParser.parsePauseLimit(value)
            UUID.fromString(GattUuidStrings.PPX_CONFIG_ID) -> _ppxConfig.value = GattParser.parsePpxConfig(value)
            UUID.fromString(GattUuidStrings.PPX_SENSITIVITY_ID) -> _ppxSensitivity.value = GattParser.parsePpxSensitivity(value)
            UUID.fromString(GattUuidStrings.STEALTH_ID) -> _stealthOnHub.value = GattParser.parseStealth(value)
            UUID.fromString(GattUuidStrings.CARDIAC_SENSITIVITY_ID) -> _cardiacSensitivity.value = GattParser.parseCardiacSensitivity(value)
            UUID.fromString(GattUuidStrings.SESSION_PROGRESS_ID) -> _progress.value = GattParser.parseSessionProgress(value)
            UUID.fromString(GattUuidStrings.SESSION_HOLD_ID) -> _holdCause.value = GattParser.parseHoldCause(value)
            UUID.fromString(GattUuidStrings.HRV_COHERENCE_ID) -> GattParser.parseHrvCoherence(value)?.let { hrv ->
                _session.value = _session.value.copy(hrv = hrv)
            }
            UUID.fromString(GattUuidStrings.PACER_PHASE_ID) -> GattParser.parsePacerPhase(value)?.let { (phase, pct) ->
                _session.value = _session.value.copy(pacerPhase = phase, pacerElapsedPercent = pct)
            }
            UUID.fromString(GattUuidStrings.IMPEDANCE_RESULT_ID) -> GattParser.parseImpedanceResult(value)?.let { flags ->
                _session.value = _session.value.copy(impedancePassFlags = flags)
                _impedanceReading.value = life.neurone.shared.setup.ImpedanceReading((_impedanceReading.value?.sequence ?: 0L) + 1, flags)
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
        _encryptedReadShown.value = false // the latest read was refused: the upload waits for one that is not
        serialRetryJob?.cancel()
        serialRetryJob = scope.launch {
            delay(SERIAL_RETRY_MS)
            if (_connectionState.value == ConnectionState.CONNECTED) central.read(serial)
        }
    }

    /**
     * A refused write is recorded, and nothing is written again because of it. A refused protocol upload write marks the
     * upload refused, and asks for the serial again: on an unpaired link that read is refused too, which raises the
     * pairing notice ([onCharacteristicReadFailed]); on a paired one it succeeds and changes nothing.
     */
    override fun onCharacteristicWriteFailed(uuid: UUID) {
        if (_connectionState.value != ConnectionState.CONNECTED) return
        _writeRefusals.tryEmit(uuid)
        if (uuid != UUID.fromString(GattUuidStrings.PROTOCOL_UPLOAD_ID)) return
        _uploadWriteRefused.value = true
        if (serialRetryJob?.isActive != true) central.read(UUID.fromString(GattUuidStrings.DEVICE_SERIAL_ID))
    }

    // ── Commands ─────────────────────────────────────────────────────────

    /**
     * A write to a characteristic that demands an encrypted link, made only once the encrypted-only serial read has succeeded on
     * this link (OI-PRIV-17, CLAUDE.md §23): a peer that only advertises the service receives none of them. False, with nothing
     * written, before that. **Never used for stopping stimulation** (stop, pause, panic, resume): those are not held behind a
     * read, and a refused one is shown by its own refusal event. The hub itself refuses a pause or a resume (ATT 0x0F) on a link
     * that is not encrypted (`REQ-FWHUB-81` (e), `OI-PRIV-24` (NP-PRIV-ANALYSIS-004)); stop and panic it never refuses for encryption.
     */
    private fun writeEncrypted(uuid: UUID, value: ByteArray): Boolean {
        if (!_encryptedReadShown.value) return false
        central.write(uuid, value)
        return true
    }

    /** The write size the upload is cut at: what the link carries now (the negotiated MTU less 3), at most the hub characteristic's 512. */
    val writeLength: Int get() = central.writeLength.coerceIn(1, BleTransfer.MAX_WRITE_SIZE)

    /**
     * Read `DEVICE_SERIAL` again, for an upload held back because the encrypted-only read has not succeeded: the person may
     * have paired the hub since the last refusal, and not every central reports a refused read for the retry to hang on
     * (`OI-PRIV-17`). Nothing is asked while a retry is already waiting or the link is down.
     */
    fun requestEncryptedRead() {
        if (_connectionState.value == ConnectionState.CONNECTED && serialRetryJob?.isActive != true) central.read(UUID.fromString(GattUuidStrings.DEVICE_SERIAL_ID))
    }

    /** A new upload starts: a refusal recorded for the last one says nothing about this one. */
    fun beginProtocolUpload() {
        _uploadWriteRefused.value = false
    }

    /**
     * Mode 2 protocol upload: one write of the stream, at most [writeLength] bytes (the caller cuts it, `ProtocolChunker`).
     * False, with nothing written, until the encrypted-only read has succeeded on this link ([encryptedReadShown]).
     */
    fun writeProtocolChunk(chunk: ByteArray): Boolean {
        if (!_encryptedReadShown.value) return false
        central.write(UUID.fromString(GattUuidStrings.PROTOCOL_UPLOAD_ID), chunk)
        return true
    }

    fun requestSessionStop() = central.write(UUID.fromString(GattUuidStrings.SESSION_STOP_ID), byteArrayOf(0x01))

    /**
     * Pause the running session (SWR-SS-05). The hub refuses it during a ramp-up (ATT 0x82) and in any state but running;
     * the figures that follow in SESSION_PROGRESS say what happened, so the app does not guess. Like its resume, it needs an
     * encrypted link (ATT 0x0F otherwise, `REQ-FWHUB-81` (e)); the session screen offers it only while SESSION_PROGRESS,
     * itself encrypted, is being read.
     */
    fun requestSessionPause() = central.write(UUID.fromString(GattUuidStrings.SESSION_PAUSE_ID), byteArrayOf(0x01))

    /**
     * The panic control (SWR-SS-09, `REQ-FWHUB-72`): cut every modality at once and hold the session paused, in any
     * state of a running session, ramp-up included. It is the pause characteristic's third code, so a helmet that
     * predates it refuses the write. The second step is [requestSessionStop] or [requestSessionResume].
     */
    fun requestSessionPanic() = central.write(UUID.fromString(GattUuidStrings.SESSION_PAUSE_ID), byteArrayOf(0x03))

    /**
     * Resume a paused session (SWR-SS-05). Stimulation returns only where the safety MCU grants it. The hub takes it only over
     * an encrypted link and refuses it otherwise (ATT 0x0F, `REQ-FWHUB-81` (e)), which [onCharacteristicWriteFailed] records.
     */
    fun requestSessionResume() = central.write(UUID.fromString(GattUuidStrings.SESSION_PAUSE_ID), byteArrayOf(0x02))

    /**
     * Mode 4: trigger EDF+ download for the given hub session ID (LE uint32). False, with nothing written, until the encrypted-only
     * read has succeeded on this link: the session ID is the session's start time (`REQ-SW03-31` (d), NP-PRIV-ANALYSIS-006 F-11).
     */
    fun requestEdfDownload(sessionId: Long): Boolean {
        val bytes = ByteArray(4) { i -> ((sessionId shr (8 * i)) and 0xFF).toByte() }
        return writeEncrypted(UUID.fromString(GattUuidStrings.EDF_REQUEST_ID), bytes)
    }

    /** A firmware command or image chunk; false, with nothing written, until the encrypted-only read has succeeded (`REQ-SW03-31` (d), CLAUDE.md §23). */
    override fun sendOtaCommand(opcode: OtaOpcode, payload: ByteArray): Boolean =
        writeEncrypted(UUID.fromString(GattUuidStrings.OTA_COMMAND_ID), byteArrayOf(opcode.rawValue.toByte()) + payload)

    /** Trigger a calibration/setup command (impedance check, ADS1299 self-cal, etc.); false, with nothing written, until the encrypted-only read has succeeded (`REQ-SW03-31` (d)). */
    override fun sendCalibration(opcode: life.neurone.core.ble.CalibrationOpcode): Boolean =
        writeEncrypted(UUID.fromString(GattUuidStrings.CALIBRATION_CMD_ID), byteArrayOf(opcode.rawValue.toByte()))

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
        // A valid "nothing outstanding" is what lets a profile leave its old tag (REQ-FAULTMSG-01 (c)).
        recomputeActiveUserTag()
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
        return writeEncrypted(UUID.fromString(GattUuidStrings.CVNS_REENABLE_CONFIRM_ID), byteArrayOf(0x01))
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
        // Held, not dropped, until the encrypted-only read has succeeded: the queue drains when it does.
        if (!consumableCharPresent || _connectionState.value != ConnectionState.CONNECTED || !_encryptedReadShown.value) return
        consumableResets.drain { central.write(UUID.fromString(GattUuidStrings.CONSUMABLE_STATUS_ID), it) }
    }

    private var cervicalFaultLedger: CervicalFaultLedger
        get() = faultLedgers.ledgerFor(activeUserTag)
        set(value) = faultLedgers.set(activeUserTag, value)

    private fun sendActiveUserTag() {
        val tag = activeUserTag ?: return
        if (!activeUserCharPresent || _connectionState.value != ConnectionState.CONNECTED) return
        writeEncrypted(UUID.fromString(GattUuidStrings.ACTIVE_USER_ID), ActiveUserTag.toWire(tag))
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
        _encryptedReadShown.value = false // the next link shows it again, or is not written to
        _uploadWriteRefused.value = false
        _connectionState.value = ConnectionState.DISCONNECTED
        _allCharacteristicsResolved.value = false
        _session.value = SessionState.EMPTY   // clear stale UHDR display state
        _progress.value = null
        _hardwareVersion.value = DEFAULT_HARDWARE_VERSION // the next link may be another helmet
        _deviceTier.value = null                          // and another tier
        _uploadCheck.value = null
        uploadCheckCharPresent = false
        _pauseLimitSeconds.value = null
        pauseLimitCharPresent = false
        _pauseLimitPublished.value = false
        _holdCause.value = null
        _cardiacSensitivity.value = null
        cardiacSensitivityCharPresent = false
        _cardiacSensitivityPublished.value = false
        _ppxSensitivity.value = null
        ppxSensitivityCharPresent = false
        _ppxSensitivityPublished.value = false
        _ppxConfig.value = null
        ppxConfigCharPresent = false
        _ppxConfigPublished.value = false
        _stealthOnHub.value = null // UHDR-class: not shown once the link is gone, and the next link may be another helmet
        stealthCharPresent = false
        _stealthPublished.value = false
        // Clear the inventory rather than zeroing a fixed shape: the hub sends a fresh snapshot
        // on reconnect, and a stale "present" socket would let a placement gate pass on a module
        // pulled while disconnected. The map goes too — the next link may be another helmet.
        _zoneModules.value = ZoneModuleConfiguration.EMPTY
        zoneFrameAssembler.reset()
        _socketMap.value = SocketMap.EMPTY
        socketMapAssembler.reset()
        _hubFirmwareVersion.value = null
        _warrantyToken.value = null
        _hubIdentifier.value = null
        deviceSerial = null
        activeUserTag = null   // the next link may be another helmet, with another tag
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

        /**
         * The hardware version of a helmet that states none (SWR-HV-07): version 1 is the only layout that exists. This
         * default is removed in the same change that adds hardware version 2 (a checked item on the increment record,
         * `np_spi_wire_types.h`).
         */
        const val DEFAULT_HARDWARE_VERSION = 1
    }
}
