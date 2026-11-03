/**
 * NeurOne Helmet Simulator — WebSocket device API server
 *
 * Exposes the same wire protocol the NeurOne hub uses over USB-C/BLE so
 * macOS/iOS/Windows apps can target this server instead of real hardware.
 *
 * Two client roles connect to ws://localhost:9000:
 *   controller — external apps (send SESSION_START / ZONE_CONFIG, receive TELEMETRY)
 *   display    — browser simulator tab (receives commands, sends TELEMETRY back)
 *
 * Usage:
 *   npm install   (once)
 *   node index.js
 */

'use strict';

const { WebSocketServer } = require('ws');
const { verify }          = require('crypto');
const fs                  = require('fs');
const path                = require('path');

const PORT           = 9000;

// NP-SIM-FW-001: the firmware-in-the-loop bridge is opt-in.  Set NP_SIM_FW_BIN to the
// np_sim_pbm_spike binary; the server then also binds to 127.0.0.1 only, because a
// client that can reach it can inject faults into a process running the hub's logic.
const { FwBridge, BridgeError } = require('./fw-bridge');
const FW_BIN = process.env.NP_SIM_FW_BIN || null;
const SERVER_VERSION = require('../version.json').version;

const TEST_PUBLIC_KEY = fs.readFileSync(
  path.join(__dirname, 'keys', 'test-public.pem'), 'utf8'
);

// ─── server setup ─────────────────────────────────────────────────────────────

const wss = new WebSocketServer(FW_BIN ? { port: PORT, host: '127.0.0.1' } : { port: PORT });
const clients = new Set();

console.log(`[NP-SIM] WebSocket server listening on ws://localhost:${PORT}`);
let fw = null;
if (FW_BIN) {
  fw = new FwBridge({ bin: FW_BIN }).start();
  fw.on('message', (m) => broadcast(m));
  fw.on('exit', (code) => console.log(`[NP-SIM] firmware process exited (${code})`));
  console.log(`[NP-SIM] Firmware bridge started: ${FW_BIN}  (bound to 127.0.0.1)`);
}
console.log(`[NP-SIM] Serve the repo root and open /simulator/ — badge should turn green.`);

wss.on('connection', (ws, req) => {
  ws.role    = 'controller';  // default; overridden by CLIENT_HELLO
  ws.isAlive = true;
  clients.add(ws);

  const addr = req.socket.remoteAddress ?? 'unknown';
  console.log(`[NP-SIM] Client connected from ${addr}  (${clients.size} total)`);

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
    console.log(`[NP-SIM] Client disconnected  (${clients.size} remaining)`);
  });

  ws.on('error', (err) => {
    console.error(`[NP-SIM] Client error: ${err.message}`);
    clients.delete(ws);
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
      console.log(`[NP-SIM] CLIENT_HELLO  role=${ws.role}  version=${ws.clientVersion}`);
      break;
    }

    case 'SESSION_START': {
      if (msg.descriptor?.signature) {
        if (!verifyDescriptor(msg.descriptor)) {
          send(ws, { type: 'ERROR', code: 'INVALID_SIGNATURE',
            message: 'Ed25519 signature failed — check test key pair (simulator/server/keys/)' });
          return;
        }
        console.log('[NP-SIM] SESSION_START signature verified ✓');
      }
      console.log(`[NP-SIM] SESSION_START  protocol=${msg.descriptor?.protocolId ?? 'raw'}`);
      broadcast(msg, { exclude: ws, toRole: 'display' });
      break;
    }

    case 'SESSION_PAUSE':
    case 'SESSION_STOP': {
      console.log(`[NP-SIM] ${msg.type}`);
      broadcast(msg, { exclude: ws, toRole: 'display' });
      break;
    }

    case 'ZONE_CONFIG': {
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
      broadcast(msg, { exclude: ws, toRole: 'controller' });
      break;
    }

    default:
      send(ws, { type: 'ERROR', code: 'UNKNOWN_TYPE', message: `Unknown message type: ${msg.type}` });
  }
}

// ─── helpers ──────────────────────────────────────────────────────────────────

function verifyDescriptor(descriptor) {
  try {
    const { signature, ...payload } = descriptor;
    return verify(
      null,
      Buffer.from(JSON.stringify(payload)),
      { key: TEST_PUBLIC_KEY, format: 'pem', type: 'spki' },
      Buffer.from(signature, 'base64')
    );
  } catch {
    return false;
  }
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
