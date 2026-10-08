package life.neurone.desktop

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
 * Windows does is unverified (`OI-UI-KMP-03`).
 */
internal class DesktopBleCentral(private val link: BtleLink) : BleCentral {
    @Volatile private var listener: BleCentralListener? = null
    @Volatile private var state = AdapterState.UNKNOWN
    @Volatile private var running = true

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

    override fun startScan(serviceUuid: UUID) = link.startScan(serviceUuid.toString())
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
            val bytes = link.poll(POLL_MS) ?: continue
            try {
                dispatch(bytes)
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
            2 -> l?.onDeviceFound(body.decodeToString())
            3 -> l?.onConnected(body.decodeToString())
            4 -> l?.onDisconnected(body.decodeToString())
            5 -> l?.onCharacteristicsDiscovered((0 until body.size / 16).map { uuid(body.copyOfRange(it * 16, it * 16 + 16)) }.toSet())
            6 -> l?.onCharacteristicChanged(uuid(body), body.copyOfRange(16, body.size))
            7 -> l?.onCharacteristicRead(uuid(body), body.copyOfRange(16, body.size))
        }
    }

    private fun uuid(b: ByteArray): UUID {
        fun word(from: Int) = (0 until 8).fold(0L) { acc, i -> (acc shl 8) or (b[from + i].toLong() and 0xFF) }
        return UUID(word(0), word(8))
    }

    private companion object {
        const val POLL_MS = 250
    }
}

/**
 * The desktop central, or [UnavailableBleCentral] when the native library is not there (an
 * unpackaged build, or an OS the library was not built for): the app then runs without a hub link
 * rather than failing to start.
 */
internal fun createDesktopBleCentral(): BleCentral {
    val link = JniBtleLink.open() ?: return UnavailableBleCentral()
    return DesktopBleCentral(link)
}
