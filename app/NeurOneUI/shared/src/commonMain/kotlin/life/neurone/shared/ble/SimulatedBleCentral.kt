package life.neurone.shared.ble

import life.neurone.core.common.UUID
import life.neurone.core.protocol.GattUuidStrings

// The simulated hub's end of the BLE seam (NP-SIM-FW-001 §27, REQ-SIMFW-19).
//
// A real hub is a GATT peripheral: the app scans, connects, discovers characteristics, reads and writes them and takes notifications.
// The simulator's twin runs the hub's own GATT server (np_gatt_server.c) and the bridge relays GATT reads and writes to it as validated
// ids and hex. This class is the BleCentral the app uses in place of a real one: every discovered characteristic, every read result and
// every notification comes from the twin, and nothing is made up here. A characteristic the hub does not publish is not discovered, and a
// read or write the hub refuses is refused here with the ATT status the hub's code gave.
//
// Test code only: no real hub is behind it, and it says so ([isSimulated], and the host's notice). It is not verification evidence
// (REQ-SIMFW-06).

/** What the app asks the twin, as typed commands: the bridge's wire form is the host's, so this class holds no JSON. */
sealed interface HubBridgeCommand {
    /** Bring the twin's storage and GATT service up (its first boot); answered by nothing when it is up already. */
    data object Init : HubBridgeCommand

    /** List the characteristics the hub publishes. */
    data object Table : HubBridgeCommand

    data class Read(val id: Int, val offset: Int = 0) : HubBridgeCommand

    data class Write(val id: Int, val hex: String) : HubBridgeCommand

    /** The link is down: the hub drops a half-written upload and keeps a complete one (np_gatt_on_disconnect, REQ-FWHUB-80). */
    data object Disconnect : HubBridgeCommand
}

/** What the twin reports, as typed events. */
sealed interface HubBridgeEvent {
    /** The link to the simulator server is open (or was lost). */
    data object Opened : HubBridgeEvent
    data object Closed : HubBridgeEvent

    /** One row of the hub's published table; [count] rows arrive, [index] from 0. */
    data class Characteristic(val index: Int, val count: Int, val id: Int, val props: Int) : HubBridgeEvent

    /** The hub's answer to a read: the ATT status (0 is success) and the bytes. */
    data class ReadResult(val id: Int, val att: Int, val hex: String) : HubBridgeEvent

    data class WriteResult(val id: Int, val att: Int) : HubBridgeEvent

    data class Notification(val id: Int, val hex: String) : HubBridgeEvent
}

/** The link to the simulator server. A host supplies it (the browser's WebSocket); a test supplies a fake. */
interface HubBridge {
    fun setReceiver(receiver: (HubBridgeEvent) -> Unit)

    /** False when the link is not open; nothing was sent. */
    fun send(command: HubBridgeCommand): Boolean
}

class SimulatedBleCentral(private val bridge: HubBridge) : BleCentral {

    /** The id the simulated hub is found under; the same on every run, because there is one twin. */
    companion object {
        const val SIMULATED_HUB_ID = "simulated-hub"

        // 128-bit UUID of a NeurOne characteristic: the id is the four hex digits after the first group (np_app_wire_constants.h).
        fun uuidOf(id: Int): UUID {
            val hex = id.toString(16).padStart(4, '0').uppercase()
            return UUID.fromString(GattUuidStrings.SERVICE_ID.replaceRange(9, 13, hex))
        }

        fun idOf(uuid: UUID): Int = uuid.toString().substring(9, 13).toInt(16)
    }

    val isSimulated: Boolean get() = true

    /** An ATT refusal the hub gave (a read or write the app asked for), for the screen or a test to show. Not an app-visible callback. */
    var onAttRefusal: ((uuid: UUID, att: Int, wasRead: Boolean) -> Unit)? = null

    private var listener: BleCentralListener? = null
    private var open = false
    private var scanning = false
    private var connecting = false
    private var connected = false
    private val table = mutableListOf<HubBridgeEvent.Characteristic>()
    private val subscribed = mutableSetOf<UUID>()

    // The twin is a hub that is always there: the adapter is on. (The link to the server is what can be missing.)
    override val adapterState: AdapterState get() = AdapterState.ON

    override fun setListener(listener: BleCentralListener) {
        this.listener = listener
        bridge.setReceiver(::onEvent)
    }

    override fun startScan(serviceUuid: UUID) {
        scanning = true
        if (open) found()
    }

    override fun stopScan() {
        scanning = false
    }

    override fun connect(deviceId: String) {
        if (connecting || connected) return
        connecting = true
        table.clear()
        // The twin boots at the first command; an Init to a twin that is up already is refused and changes nothing.
        if (!bridge.send(HubBridgeCommand.Init) || !bridge.send(HubBridgeCommand.Table)) connecting = false
    }

    override fun disconnect() {
        if (!connected && !connecting) return
        val wasConnected = connected
        connected = false
        connecting = false
        subscribed.clear()
        // A link that was up tells the hub it is down, as a stack's link-down callback does; a lost link to the server cannot.
        if (wasConnected) bridge.send(HubBridgeCommand.Disconnect)
        listener?.onDisconnected(SIMULATED_HUB_ID)
    }

    override fun discoverCharacteristics(serviceUuid: UUID, uuids: List<UUID>) {
        // What the hub publishes, intersected with what was asked for: the same answer a stack gives.
        val published = table.map { uuidOf(it.id) }.toSet()
        listener?.onCharacteristicsDiscovered(uuids.filter { it in published }.toSet())
    }

    override fun enableNotifications(uuid: UUID) {
        if (table.any { uuidOf(it.id) == uuid }) subscribed += uuid
    }

    override fun read(uuid: UUID) {
        bridge.send(HubBridgeCommand.Read(idOf(uuid)))
    }

    override fun write(uuid: UUID, value: ByteArray) {
        bridge.send(HubBridgeCommand.Write(idOf(uuid), value.joinToString("") { (it.toInt() and 0xFF).toString(16).padStart(2, '0') }))
    }

    private fun found() {
        if (!connected && !connecting) listener?.onDeviceFound(SIMULATED_HUB_ID)
    }

    private fun onEvent(event: HubBridgeEvent) {
        val l = listener ?: return
        when (event) {
            HubBridgeEvent.Opened -> {
                open = true
                if (scanning) found()
            }
            HubBridgeEvent.Closed -> {
                open = false
                if (connected || connecting) {
                    connected = false
                    connecting = false
                    subscribed.clear()
                    l.onDisconnected(SIMULATED_HUB_ID)
                }
            }
            is HubBridgeEvent.Characteristic -> {
                if (!connecting) return
                if (event.index == 0) table.clear()
                table += event
                // The table is whole once every row has arrived: the link is up, as a stack reports it after its service discovery.
                if (table.size == event.count) {
                    connecting = false
                    connected = true
                    l.onConnected(SIMULATED_HUB_ID)
                }
            }
            is HubBridgeEvent.ReadResult -> {
                val uuid = uuidOf(event.id)
                if (event.att == 0) l.onCharacteristicRead(uuid, hexToBytes(event.hex)) else onAttRefusal?.invoke(uuid, event.att, true)
            }
            is HubBridgeEvent.WriteResult -> if (event.att != 0) onAttRefusal?.invoke(uuidOf(event.id), event.att, false)
            is HubBridgeEvent.Notification -> {
                val uuid = uuidOf(event.id)
                // A notification reaches only a central that subscribed to that characteristic.
                if (uuid in subscribed) l.onCharacteristicChanged(uuid, hexToBytes(event.hex))
            }
        }
    }

    private fun hexToBytes(hex: String): ByteArray = ByteArray(hex.length / 2) { hex.substring(it * 2, it * 2 + 2).toInt(16).toByte() }
}
