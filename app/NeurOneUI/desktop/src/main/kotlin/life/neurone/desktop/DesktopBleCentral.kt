package life.neurone.desktop

import life.neurone.core.common.KeyValueStore
import life.neurone.core.common.UUID
import life.neurone.shared.ble.AdapterState
import life.neurone.shared.ble.BleCentral
import life.neurone.shared.ble.BleCentralListener
import life.neurone.shared.ble.UnavailableBleCentral

/**
 * The macOS / Windows (and Linux) [BleCentral], over btleplug (`common/btle-jni`): the OS Bluetooth
 * stack is reached by the Rust library, and this class only translates. Commands go down at once;
 * one daemon thread polls the library's event queue and calls the listener, which is the shared
 * `NeurOneGattManager`, so the hub link behaves as it does on every other platform.
 *
 * Pairing: btleplug has no pairing call. The hub's encrypted characteristics (`DEVICE_SERIAL`) rely
 * on the operating system pairing the link when one is touched, which CoreBluetooth does; whether
 * Windows does is unverified (`OI-UI-KMP-03`). A refused read arrives as `onCharacteristicReadFailed`,
 * so the manager can say the hub needs pairing in the OS and retry once the user has done it.
 *
 * Identity: the last connected hub's id is kept in [identity]. When one is remembered, a different
 * device is held for [preferenceWindowMs] so the remembered hub wins when both are in range; with none
 * in range after the window the other is taken, so a new hub is never refused.
 */
internal class DesktopBleCentral(
    private val link: BtleLink,
    /** Holds the last hub's device id; null keeps none. The id is the OS's own and names a device, not a person. */
    private val identity: KeyValueStore? = null,
    private val preferenceWindowMs: Long = PREFERENCE_WINDOW_MS,
    private val now: () -> Long = System::currentTimeMillis,
) : BleCentral {
    @Volatile private var listener: BleCentralListener? = null
    @Volatile private var state = AdapterState.UNKNOWN
    @Volatile private var running = true

    /** A device found while another is remembered: reported after [preferenceWindowMs] unless the remembered one appears. */
    @Volatile private var held: String? = null
    @Volatile private var heldDeadline = 0L

    /** Forgets the remembered hub, so the next one found is taken at once. */
    fun forgetHub() {
        identity?.remove(HUB_KEY)
        held = null
    }

    private val poller = Thread(::pollLoop, "neurone-ble-events").apply { isDaemon = true }

    init {
        poller.start()
        link.refresh()
    }

    override val adapterState: AdapterState get() = state

    override fun setListener(listener: BleCentralListener) {
        this.listener = listener
        listener.onAdapterStateChanged(state)
    }

    override fun refresh() = link.refresh()

    override fun startScan(serviceUuid: UUID) {
        held = null
        link.startScan(serviceUuid.toString())
    }
    override fun stopScan() = link.stopScan()
    override fun connect(deviceId: String) = link.connect(deviceId)
    override fun disconnect() = link.disconnect()

    override fun discoverCharacteristics(serviceUuid: UUID, uuids: List<UUID>) =
        link.discover(serviceUuid.toString(), uuids.map(UUID::toString))

    override fun enableNotifications(uuid: UUID) = link.enableNotifications(uuid.toString())
    override fun read(uuid: UUID) = link.read(uuid.toString())
    override fun write(uuid: UUID, value: ByteArray) = link.write(uuid.toString(), value)

    /** Stops the event thread, then the radio. */
    fun close() {
        running = false
        poller.join(2_000)
        link.close()
    }

    private fun pollLoop() {
        while (running) {
            val bytes = link.poll(POLL_MS)
            try {
                releaseHeld()
                if (bytes != null) dispatch(bytes)
            } catch (e: Exception) {
                // A malformed event or a listener fault must not end the thread that carries every later one.
                System.err.println("neurone-ble: dropped an event: $e")
            }
        }
    }

    /** The wire form is documented on `Event::encode` in `common/btle-jni/src/central.rs`. */
    internal fun dispatch(bytes: ByteArray) {
        if (bytes.isEmpty()) return
        val body = bytes.copyOfRange(1, bytes.size)
        val l = listener
        when (bytes[0].toInt()) {
            1 -> {
                state = AdapterState.entries.getOrElse(body[0].toInt()) { AdapterState.UNKNOWN }
                l?.onAdapterStateChanged(state)
            }
            2 -> deviceFound(body.decodeToString())
            3 -> {
                held = null
                val id = body.decodeToString()
                identity?.putString(HUB_KEY, id)
                l?.onConnected(id)
            }
            4 -> l?.onDisconnected(body.decodeToString())
            5 -> l?.onCharacteristicsDiscovered((0 until body.size / 16).map { uuid(body.copyOfRange(it * 16, it * 16 + 16)) }.toSet())
            6 -> l?.onCharacteristicChanged(uuid(body), body.copyOfRange(16, body.size))
            7 -> l?.onCharacteristicRead(uuid(body), body.copyOfRange(16, body.size))
            8 -> l?.onCharacteristicReadFailed(uuid(body))
        }
    }

    /** The remembered hub, or any when none is remembered, goes straight through; another waits for the remembered one. */
    private fun deviceFound(id: String) {
        val remembered = identity?.getString(HUB_KEY)
        if (remembered == null || id == remembered) {
            held = null
            listener?.onDeviceFound(id)
        } else if (held == null) {
            held = id
            heldDeadline = now() + preferenceWindowMs
        }
    }

    private fun releaseHeld() {
        val id = held ?: return
        if (now() < heldDeadline) return
        held = null
        listener?.onDeviceFound(id)
    }

    private fun uuid(b: ByteArray): UUID {
        fun word(from: Int) = (0 until 8).fold(0L) { acc, i -> (acc shl 8) or (b[from + i].toLong() and 0xFF) }
        return UUID(word(0), word(8))
    }

    private companion object {
        const val POLL_MS = 250
        const val HUB_KEY = "ble.lastHubId"

        /** UC-078 (timings): unmeasured. */
        const val PREFERENCE_WINDOW_MS = 3_000L
    }
}

/**
 * The desktop central, or [UnavailableBleCentral] when the native library is not there (an
 * unpackaged build, or an OS the library was not built for): the app then runs without a hub link
 * rather than failing to start.
 */
internal fun createDesktopBleCentral(identity: KeyValueStore? = null): BleCentral {
    val link = JniBtleLink.open() ?: return UnavailableBleCentral()
    return DesktopBleCentral(link, identity)
}
