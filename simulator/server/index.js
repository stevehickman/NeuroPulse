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
const { verify }          = require('crypto');
const fs                  = require('fs');
const path                = require('path');

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

const TEST_PUBLIC_KEY = fs.readFileSync(
  path.join(__dirname, 'keys', 'test-public.pem'), 'utf8'
);

// ─── server setup ─────────────────────────────────────────────────────────────

const wss = new WebSocketServer({ port: PORT, host: BIND_HOST, verifyClient });
const clients = new Set();

wss.on('listening', () => {
  const a = wss.address();
  console.log(`[NP-SIM] WebSocket server listening on ws://${a.address}:${a.port}  (loopback only)`);
});
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
      console.log(helloLine(ws.role, ws.clientVersion, LOG_DETAIL));
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
      console.log(sessionStartLine(msg.descriptor?.protocolId, LOG_DETAIL));
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
