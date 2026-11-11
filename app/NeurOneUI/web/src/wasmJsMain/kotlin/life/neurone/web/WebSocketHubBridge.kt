package life.neurone.web

import life.neurone.shared.ble.HubBridge
import life.neurone.shared.ble.HubBridgeCommand
import life.neurone.shared.ble.HubBridgeEvent

/**
 * The browser's link to the simulator server (`simulator/server/index.js`): a WebSocket to the bridge that fronts the twin's own GATT
 * server (NP-SIM-FW-001 §27, `REQ-SIMFW-19`). The server binds to 127.0.0.1 only (`OI-SIMFW-07`), so [url] is a loopback address.
 *
 * The JSON is built and read on the JavaScript side from typed fields, so no string a person typed reaches a command and nothing here
 * parses text: a command is a name, integers and hex, and an event comes back as fields.
 */
class WebSocketHubBridge(private val url: String) : HubBridge {

    private var started = false

    override fun setReceiver(receiver: (HubBridgeEvent) -> Unit) {
        if (started) return
        started = true
        hubOpen(url) { kind, a, b, c, d ->
            when (kind) {
                "opened" -> receiver(HubBridgeEvent.Opened)
                "closed" -> receiver(HubBridgeEvent.Closed)
                "char" -> receiver(HubBridgeEvent.Characteristic(index = a.toInt(), count = b.toInt(), id = c.toInt(), props = d.toInt()))
                "read" -> receiver(HubBridgeEvent.ReadResult(id = a.toInt(), att = b.toInt(), hex = c))
                "write" -> receiver(HubBridgeEvent.WriteResult(id = a.toInt(), att = b.toInt()))
                "notify" -> receiver(HubBridgeEvent.Notification(id = a.toInt(), hex = c))
            }
        }
    }

    override fun send(command: HubBridgeCommand): Boolean = when (command) {
        HubBridgeCommand.Init -> hubSend("gatt_init", 0, 0, "")
        HubBridgeCommand.Table -> hubSend("gatt_table", 0, 0, "")
        is HubBridgeCommand.Read -> hubSend("gatt_read", command.id, command.offset, "")
        is HubBridgeCommand.Write -> hubSend("gatt_write", command.id, 0, command.hex)
        HubBridgeCommand.Disconnect -> hubSend("gatt_disconnect", 0, 0, "")
        is HubBridgeCommand.Link -> hubSend("gatt_link", if (command.encrypted) 1 else 0, 0, "")
    }
}

// The whole of the WebSocket side. `globalThis.__neuroneHub` holds the socket. Events come back through one callback: (kind, a, b, c, d),
// every field a string. The socket identifies itself as a controller (the server's default) and ignores every message that is not a GATT_EVENT.
@JsFun(
    """(url, cb) => {
        const hub = globalThis.__neuroneHub = { ws: null };
        const ws = new WebSocket(url);
        hub.ws = ws;
        ws.onopen = () => { ws.send(JSON.stringify({ type: 'CLIENT_HELLO', role: 'controller', version: '1.0.0' })); cb('opened', '', '', '', ''); };
        ws.onclose = () => { cb('closed', '', '', '', ''); };
        ws.onmessage = (m) => {
            let msg; try { msg = JSON.parse(m.data); } catch (e) { return; }
            if (!msg || msg.type !== 'GATT_EVENT' || !msg.event) return;
            const e = msg.event;
            switch (e.ev) {
                case 'gatt_char': cb('char', String(e.index), String(e.count), String(e.id), String(e.props)); break;
                case 'gatt_read': cb('read', String(e.id), String(e.att), String(e.hex || ''), ''); break;
                case 'gatt_write': cb('write', String(e.id), String(e.att), '', ''); break;
                case 'gatt_notify': cb('notify', String(e.id), '', String(e.hex || ''), ''); break;
            }
        };
    }""",
)
private external fun hubOpen(url: String, cb: (String, String, String, String, String) -> Unit)

@JsFun(
    """(cmd, id, offset, hex) => {
        const hub = globalThis.__neuroneHub;
        if (!hub || !hub.ws || hub.ws.readyState !== 1) return false;
        const msg = { type: 'FW_COMMAND', cmd };
        if (cmd === 'gatt_read') { msg.id = id; msg.offset = offset; }
        if (cmd === 'gatt_write') { msg.id = id; msg.hex = hex; }
        if (cmd === 'gatt_link') { msg.encrypted = id === 1; }
        hub.ws.send(JSON.stringify(msg));
        return true;
    }""",
)
private external fun hubSend(cmd: String, id: Int, offset: Int, hex: String): Boolean
