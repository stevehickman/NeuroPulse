package life.neurone.web

import life.neurone.core.common.UUID
import life.neurone.core.platform.toHex
import life.neurone.core.protocol.GattUuidStrings
import life.neurone.shared.ble.AdapterState
import life.neurone.shared.ble.BleCentral
import life.neurone.shared.ble.BleCentralListener

/**
 * The browser's [BleCentral], over Web Bluetooth (`navigator.bluetooth`).
 *
 * Two things differ from a native central, and both are the browser's rules, not choices:
 *  - **A scan needs a click.** `requestDevice` shows the browser's own chooser and is refused without a user gesture.
 *    Until the user has picked a device the adapter reports [AdapterState.UNAUTHORIZED] ("needs the user"), so the
 *    manager stays disconnected and the Connect button stays enabled; the button's action ([requestDevice]) runs
 *    the chooser inside the click. Once a device is picked the adapter reports [AdapterState.ON], the manager's scan
 *    reconnects that device without any click, and so does its two-second reconnect after a drop.
 *  - **GATT operations cannot overlap.** The manager issues a burst of notification and read requests the moment
 *    it connects; the JavaScript side queues them and runs them one at a time.
 *
 * Needs a secure origin (https or localhost) and a Chromium-based browser; elsewhere [adapterState] is
 * [AdapterState.UNKNOWN] and the hub stays disconnected.
 */
class WebBleCentral : BleCentral {

    private var listener: BleCentralListener? = null
    private var knownDeviceId: String? = null
    private val available = bleAvailable()

    override val adapterState: AdapterState
        get() = when {
            !available -> AdapterState.UNKNOWN
            knownDeviceId == null -> AdapterState.UNAUTHORIZED
            else -> AdapterState.ON
        }

    override fun setListener(listener: BleCentralListener) {
        this.listener = listener
        bleInit { kind, a, b -> dispatch(kind, a, b) }
    }

    override fun startScan(serviceUuid: UUID) {
        // Only reached once a device has been picked (the adapter is ON): it is reconnected, not discovered.
        knownDeviceId?.let { listener?.onDeviceFound(it) }
    }

    /** Show the browser's device chooser for the hub's service. Call from a click handler. */
    fun requestDevice() {
        if (!available) return
        bleRequest(GattUuidStrings.SERVICE_ID.lowercase())
    }

    override fun stopScan() {}

    override fun connect(deviceId: String) = bleConnect()

    override fun disconnect() = bleDisconnect()

    override fun discoverCharacteristics(serviceUuid: UUID, uuids: List<UUID>) =
        bleDiscover(serviceUuid.toString(), uuids.joinToString(",") { it.toString() })

    override fun enableNotifications(uuid: UUID) = bleNotify(uuid.toString())

    override fun read(uuid: UUID) = bleRead(uuid.toString())

    override fun write(uuid: UUID, value: ByteArray) = bleWrite(uuid.toString(), value.toHex())

    private fun dispatch(kind: String, a: String, b: String) {
        val l = listener ?: return
        when (kind) {
            "found" -> {
                knownDeviceId = a
                // The adapter is now ON: the manager scans, which reconnects this device.
                l.onAdapterStateChanged(AdapterState.ON)
            }
            "connected" -> l.onConnected(a)
            "disconnected" -> l.onDisconnected(a)
            "discovered" -> l.onCharacteristicsDiscovered(if (a.isEmpty()) emptySet() else a.split(",").map(UUID::fromString).toSet())
            "changed" -> l.onCharacteristicChanged(UUID.fromString(a), hexToBytes(b))
            "read" -> l.onCharacteristicRead(UUID.fromString(a), hexToBytes(b))
            // "cancelled" (the user closed the chooser) and "error" leave the manager scanning, so the next click retries.
        }
    }

    override fun refresh() {}
}

private fun hexToBytes(hex: String): ByteArray = ByteArray(hex.length / 2) { hex.substring(it * 2, it * 2 + 2).toInt(16).toByte() }

@JsFun("() => !!(globalThis.navigator && globalThis.navigator.bluetooth)")
private external fun bleAvailable(): Boolean

// The whole of the Web Bluetooth side. `globalThis.__neuroneBle` holds the chosen device, its characteristics and a
// promise chain that serialises every GATT operation. Results come back through one callback: (kind, a, b).
@JsFun(
    """(cb) => {
        const g = globalThis;
        const ble = g.__neuroneBle = { cb, device: null, server: null, chars: new Map(), queue: Promise.resolve() };
        ble.enqueue = (job) => {
            ble.queue = ble.queue.then(job).catch((e) => ble.cb('error', String(e && e.message || e), ''));
        };
        ble.hex = (view) => Array.from(new Uint8Array(view.buffer, view.byteOffset, view.byteLength))
            .map((x) => x.toString(16).padStart(2, '0')).join('');
    }""",
)
private external fun bleInit(cb: (String, String, String) -> Unit)

@JsFun(
    """(service) => {
        const ble = globalThis.__neuroneBle;
        navigator.bluetooth.requestDevice({ filters: [{ services: [service] }], optionalServices: [service] })
            .then((device) => {
                ble.device = device;
                device.addEventListener('gattserverdisconnected', () => {
                    ble.server = null; ble.chars.clear(); ble.queue = Promise.resolve();
                    ble.cb('disconnected', device.id, '');
                });
                ble.cb('found', device.id, '');
            })
            .catch((e) => ble.cb(e && e.name === 'NotFoundError' ? 'cancelled' : 'error', String(e && e.message || e), ''));
    }""",
)
private external fun bleRequest(service: String)

@JsFun(
    """() => {
        const ble = globalThis.__neuroneBle;
        if (!ble.device) return;
        ble.enqueue(async () => {
            ble.server = await ble.device.gatt.connect();
            ble.cb('connected', ble.device.id, '');
        });
    }""",
)
private external fun bleConnect()

@JsFun("() => { const ble = globalThis.__neuroneBle; if (ble && ble.device && ble.device.gatt.connected) ble.device.gatt.disconnect(); }")
private external fun bleDisconnect()

@JsFun(
    """(service, uuids) => {
        const ble = globalThis.__neuroneBle;
        ble.enqueue(async () => {
            const svc = await ble.server.getPrimaryService(service);
            const found = [];
            for (const uuid of uuids.split(',').filter((u) => u)) {
                try { ble.chars.set(uuid, await svc.getCharacteristic(uuid)); found.push(uuid); }
                catch (e) { /* an optional characteristic the hub firmware does not have yet */ }
            }
            ble.cb('discovered', found.join(','), '');
        });
    }""",
)
private external fun bleDiscover(service: String, uuids: String)

@JsFun(
    """(uuid) => {
        const ble = globalThis.__neuroneBle;
        ble.enqueue(async () => {
            const c = ble.chars.get(uuid);
            if (!c) return;
            c.addEventListener('characteristicvaluechanged', (e) => ble.cb('changed', uuid, ble.hex(e.target.value)));
            await c.startNotifications();
        });
    }""",
)
private external fun bleNotify(uuid: String)

@JsFun(
    """(uuid) => {
        const ble = globalThis.__neuroneBle;
        ble.enqueue(async () => {
            const c = ble.chars.get(uuid);
            if (!c) return;
            ble.cb('read', uuid, ble.hex(await c.readValue()));
        });
    }""",
)
private external fun bleRead(uuid: String)

@JsFun(
    """(uuid, hex) => {
        const ble = globalThis.__neuroneBle;
        ble.enqueue(async () => {
            const c = ble.chars.get(uuid);
            if (!c) return;
            const bytes = new Uint8Array((hex.match(/../g) || []).map((h) => parseInt(h, 16)));
            if (c.writeValueWithResponse) await c.writeValueWithResponse(bytes); else await c.writeValue(bytes);
        });
    }""",
)
private external fun bleWrite(uuid: String, hex: String)
