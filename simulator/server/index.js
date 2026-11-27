/**
 * NeurOne Helmet Simulator — WebSocket device API server
 *
 * Exposes the same wire protocol the NeurOne hub uses over USB-C/BLE so
 * macOS/iOS/Windows apps can target this server instead of real hardware.
 *
 * Two client roles connect to ws://127.0.0.1:9000 (loopback only, Origin-checked: origin.js, OI-SIMFW-07):
 *   controller — external apps (send SESSION_START / ZONE_CONFIG, receive TELEMETRY)
 *   display    — browser simulator tab (receives commands, sends TELEMETRY back)
 *
 * Usage:
 *   npm install   (once)
 *   node index.js
 */

'use strict';

const { WebSocketServer } = require('ws');
const { createVerifier, SessionGate } = require('./session-auth');
const { narrowDisplayMessage } = require('./device-messages');

// NP_SIM_PORT exists so a test can run the server beside a developer's own on 9000.
const PORT = process.env.NP_SIM_PORT === undefined ? 9000 : Number(process.env.NP_SIM_PORT);
if (!Number.isInteger(PORT) || PORT < 1 || PORT > 65535) {
  console.error(`[NP-SIM] NP_SIM_PORT must be an integer 1-65535, not ${JSON.stringify(process.env.NP_SIM_PORT)}`);
  process.exit(2);
}

// NP-SIM-FW-001 REQ-SIMFW-28 (OI-SIMFW-07): loopback only, in every mode, and a browser page may join only
// from a loopback origin. A client that can reach the server receives every TELEMETRY and, with the
// firmware bridge, can inject faults into a process running the hub's logic.
const { BIND_HOST, verifyClient } = require('./origin');

// NP-SIM-FW-001: the firmware-in-the-loop bridge is opt-in.  Set NP_SIM_FW_BIN to the
// np_sim_pbm_spike binary.
const { FwBridge, BridgeError } = require('./fw-bridge');
const FW_BIN = process.env.NP_SIM_FW_BIN || null;
const SERVER_VERSION = require('../version.json').version;

// REQ-SIMFW-27 (b): no client address in the log; version and protocol ID only with NP_SIM_DEBUG=1.
const { logDetailEnabled, connectedLine, helloLine, sessionStartLine } = require('./log-lines');
const LOG_DETAIL = logDetailEnabled(process.env);

// OI-PRIV-43, REQ-SIMFW-29: refuse what the hub refuses. A SESSION_START needs a valid signature (the shared
// core's Ed25519 over framed canonical bytes, against the TEST key), and neither a second SESSION_START nor a
// ZONE_CONFIG is taken while a session runs. The link itself is plain ws://: the CLAUDE.md §23 gap OI-SIMFW-71.
const gate = new SessionGate(createVerifier());

// ─── server setup ─────────────────────────────────────────────────────────────

const wss = new WebSocketServer({ port: PORT, host: BIND_HOST, verifyClient });
const clients = new Set();

wss.on('listening', () => {
  const a = wss.address();
  console.log(`[NP-SIM] WebSocket server listening on ws://${a.address}:${a.port}  (loopback only)`);
});
if (!gate.verifier.available) {
  console.warn(`[NP-SIM] The NPPS core is not built (${gate.verifier.loadError}); every SESSION_START will be refused. Run scripts/build-npps-wasm.sh`);
}
let fw = null;
if (FW_BIN) {
  fw = new FwBridge({ bin: FW_BIN, autoStatus: 40 }).start();
  fw.on('message', (m) => broadcast(m));
  fw.on('exit', (code) => console.log(`[NP-SIM] firmware process exited (${code})`));
  console.log(`[NP-SIM] Firmware bridge started: ${FW_BIN}`);
}
console.log(`[NP-SIM] Serve the repo root and open /simulator/ — badge should turn green.`);

wss.on('connection', (ws) => {
  ws.role    = 'controller';  // default; overridden by CLIENT_HELLO
  ws.isAlive = true;
  clients.add(ws);

  console.log(connectedLine(clients.size));

  send(ws, {
    type:         'CONNECTED',
    version:      SERVER_VERSION,
    capabilities: ['SESSION_START', 'SESSION_PAUSE', 'SESSION_STOP',
                   'ZONE_CONFIG', 'ACCESSORY_CONFIG',
                   'TELEMETRY', 'FAULT', 'SESSION_COMPLETE'],
  });

  ws.on('pong', () => { ws.isAlive = true; });
  if (fw) send(ws, fw.list());

  ws.on('message', (raw) => {
    let msg;
    try {
      msg = JSON.parse(raw);
    } catch {
      send(ws, { type: 'ERROR', code: 'INVALID_JSON', message: 'Message must be valid JSON' });
      return;
    }
    handleMessage(ws, msg);
  });

  ws.on('close', () => {
    clients.delete(ws);
    gate.noteDisplays(displayCount());
    console.log(`[NP-SIM] Client disconnected  (${clients.size} remaining)`);
  });

  ws.on('error', (err) => {
    console.error(`[NP-SIM] Client error: ${err.message}`);
    clients.delete(ws);
    gate.noteDisplays(displayCount());
  });
});

// ─── heartbeat (detect stale connections) ─────────────────────────────────────

const heartbeat = setInterval(() => {
  for (const ws of clients) {
    if (!ws.isAlive) { ws.terminate(); continue; }
    ws.isAlive = false;
    ws.ping();
  }
}, 30_000);

wss.on('close', () => clearInterval(heartbeat));

// ─── message dispatch ─────────────────────────────────────────────────────────

function handleMessage(ws, msg) {
  switch (msg.type) {

    case 'CLIENT_HELLO': {
      ws.role = msg.role === 'display' ? 'display' : 'controller';
      ws.clientVersion = msg.version ?? 'unknown';
      console.log(helloLine(ws.role, ws.clientVersion, LOG_DETAIL));
      break;
    }

    case 'SESSION_START': {
      const verdict = gate.admitStart(msg.descriptor);
      if (!verdict.ok) {
        console.log(`[NP-SIM] SESSION_START refused: ${verdict.code}`);
        send(ws, { type: 'ERROR', code: verdict.code, message: verdict.message });
        return;
      }
      console.log('[NP-SIM] SESSION_START signature verified against the TEST key ✓');
      console.log(sessionStartLine(msg.descriptor?.protocolId, LOG_DETAIL));
      broadcast(msg, { exclude: ws, toRole: 'display' });
      break;
    }

    case 'SESSION_PAUSE':
    case 'SESSION_STOP': {
      console.log(`[NP-SIM] ${msg.type}`);
      if (msg.type === 'SESSION_STOP') gate.noteStop();
      broadcast(msg, { exclude: ws, toRole: 'display' });
      break;
    }

    case 'ZONE_CONFIG': {
      const verdict = gate.admitZoneConfig();
      if (!verdict.ok) {
        send(ws, { type: 'ERROR', code: verdict.code, message: verdict.message });
        return;
      }
      console.log(`[NP-SIM] ZONE_CONFIG  zone=${msg.zoneId}`);
      broadcast(msg, { exclude: ws, toRole: 'display' });
      break;
    }

    case 'ACCESSORY_CONFIG': {
      console.log(`[NP-SIM] ACCESSORY_CONFIG  name=${msg.name}  visible=${msg.visible}`);
      broadcast(msg, { exclude: ws, toRole: 'display' });
      break;
    }

    // Firmware bridge (opt-in): fault injection and spike commands
    case 'MODULE_SET':
    case 'MODULE_FAIL':
    case 'MODULE_CLEAR':
    case 'FW_COMMAND': {
      if (!fw) {
        send(ws, { type: 'ERROR', code: 'NO_FIRMWARE', message: 'Start the server with NP_SIM_FW_BIN to use the firmware bridge' });
        return;
      }
      try {
        fw.handle(msg);
      } catch (e) {
        if (!(e instanceof BridgeError)) throw e;
        send(ws, { type: 'ERROR', code: 'BAD_FW_MESSAGE', message: e.message });
      }
      break;
    }

    // Display → controllers
    case 'TELEMETRY':
    case 'FAULT':
    case 'SESSION_COMPLETE': {
      // No UHDR value reaches a controller: of TELEMETRY and SESSION_COMPLETE only the fields of NP-SIM-FW-001
      // REQ-SIMFW-31 are relayed, whatever the display sent (OI-PRIV-42).
      const out = narrowDisplayMessage(msg);
      // Only a display's report moves the session state: a controller cannot clear it by claiming the session ended.
      if (ws.role === 'display') {
        if (out.type === 'TELEMETRY') gate.noteTelemetry(out);
        if (out.type === 'SESSION_COMPLETE') gate.noteComplete();
      }
      broadcast(out, { exclude: ws, toRole: 'controller' });
      break;
    }

    default:
      send(ws, { type: 'ERROR', code: 'UNKNOWN_TYPE', message: `Unknown message type: ${msg.type}` });
  }
}

// ─── helpers ──────────────────────────────────────────────────────────────────

function displayCount() {
  let n = 0;
  for (const c of clients) if (c.role === 'display') n++;
  return n;
}

function broadcast(msg, { exclude, toRole } = {}) {
  const raw = JSON.stringify(msg);
  for (const ws of clients) {
    if (ws === exclude) continue;
    if (toRole && ws.role !== toRole) continue;
    send(ws, raw);
  }
}

function send(ws, msgOrRaw) {
  if (ws.readyState !== 1 /* OPEN */) return;
  ws.send(typeof msgOrRaw === 'string' ? msgOrRaw : JSON.stringify(msgOrRaw));
}
