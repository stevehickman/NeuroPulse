'use strict';
/**
 * The server relays no UHDR value to a controller (NP-SIM-FW-001 REQ-SIMFW-30 (c), OI-PRIV-42).  Run: node --test server/
 * Starts index.js in its default mode on a free port, joins a display and a controller, and has the display send the
 * messages the simulator used to send. Needs `ws` installed in simulator/server (bun install).
 */
const test = require('node:test');
const assert = require('node:assert');
const net = require('node:net');
const path = require('node:path');
const { spawn } = require('node:child_process');
const WebSocket = require('ws');

function freePort() {
  return new Promise((resolve, reject) => {
    const s = net.createServer();
    s.once('error', reject);
    s.listen(0, '127.0.0.1', () => { const { port } = s.address(); s.close(() => resolve(port)); });
  });
}

async function startServer() {
  const port = await freePort();
  const env = { ...process.env, NP_SIM_PORT: String(port) };
  delete env.NP_SIM_FW_BIN;
  const child = spawn(process.execPath, [path.join(__dirname, 'index.js')], { env, stdio: ['ignore', 'pipe', 'pipe'] });
  let out = '';
  await new Promise((resolve, reject) => {
    const timer = setTimeout(() => reject(new Error(`server did not start:\n${out}`)), 10_000);
    const onData = (d) => { out += d; if (/listening on ws:/.test(out)) { clearTimeout(timer); resolve(); } };
    child.stdout.on('data', onData);
    child.stderr.on('data', onData);
    child.once('exit', (code) => { clearTimeout(timer); reject(new Error(`server exited ${code}:\n${out}`)); });
  });
  return { child, port };
}

function join(port, role) {
  return new Promise((resolve, reject) => {
    const ws = new WebSocket(`ws://127.0.0.1:${port}`);
    const inbox = [];
    ws.on('message', (raw) => inbox.push(JSON.parse(raw)));
    ws.once('error', reject);
    ws.once('open', () => {
      ws.send(JSON.stringify({ type: 'CLIENT_HELLO', role, version: 'test' }));
      resolve({ ws, inbox });
    });
  });
}

async function next(inbox, type, ms = 5_000) {
  const end = Date.now() + ms;
  while (Date.now() < end) {
    const i = inbox.findIndex((m) => m.type === type);
    if (i >= 0) return inbox.splice(i, 1)[0];
    await new Promise((r) => setTimeout(r, 20));
  }
  throw new Error(`no ${type} within ${ms} ms`);
}

// @verifies REQ-SIMFW-30#c
test('a controller receives only the REQ-SIMFW-30 fields of a display\'s TELEMETRY and SESSION_COMPLETE', async (t) => {
  const { child, port } = await startServer();
  t.after(() => child.kill());
  const display = await join(port, 'display');
  const controller = await join(port, 'controller');
  t.after(() => { display.ws.close(); controller.ws.close(); });
  await new Promise((r) => setTimeout(r, 100));   // both hellos are handled before the display speaks

  display.ws.send(JSON.stringify({ type: 'TELEMETRY', timestamp: 1716000000000, session_id: 'sess_0123456789abcdef',
    phase: 'running', snap: { running: true, vns: { rmssd: 44.1, hr: 63 }, eeg: { bands: { alpha: 38.2 } } } }));
  assert.deepStrictEqual(await next(controller.inbox, 'TELEMETRY'),
    { type: 'TELEMETRY', session_id: 'sess_0123456789abcdef', phase: 'running', hub_temp_c: null });

  display.ws.send(JSON.stringify({ type: 'SESSION_COMPLETE', session_id: 'sess_0123456789abcdef', uhdr_written: false,
    uhdr_summary: { protocol: '01-gamma-focus', duration_s: 1200 },
    shdr_summary: { protocol_id: '01-gamma-focus', modalities_active: ['pbm'] } }));
  assert.deepStrictEqual(await next(controller.inbox, 'SESSION_COMPLETE'),
    { type: 'SESSION_COMPLETE', session_id: 'sess_0123456789abcdef', uhdr_written: false });
});
