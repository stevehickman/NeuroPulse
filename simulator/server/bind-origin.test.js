'use strict';
/**
 * Who may reach the simulator's device API (NP-SIM-FW-001 REQ-SIMFW-27, OI-SIMFW-07).  Run: node --test server/
 * The end-to-end tests start index.js in its default mode (no NP_SIM_FW_BIN), the mode that used to listen on
 * every interface, and speak raw HTTP upgrades to it so the Origin header is under the test's control.
 * They need `ws` installed in simulator/server (bun install).
 */
const test = require('node:test');
const assert = require('node:assert');
const crypto = require('node:crypto');
const http = require('node:http');
const net = require('node:net');
const os = require('node:os');
const path = require('node:path');
const { spawn } = require('node:child_process');
const { BIND_HOST, isAllowedOrigin } = require('./origin');

// @verifies REQ-SIMFW-27#b
test('loopback origins and a missing Origin are allowed', () => {
  for (const o of [undefined, 'http://localhost:8080', 'http://127.0.0.1:8080', 'http://[::1]:8080',
                   'http://localhost', 'https://localhost:3000', 'http://127.0.0.1:5173']) {
    assert.strictEqual(isAllowedOrigin(o), true, String(o));
  }
});

// @verifies REQ-SIMFW-27#b
test('every other origin is refused', () => {
  for (const o of ['null', '', 'https://example.com', 'http://localhost.example.com', 'http://127.0.0.1.nip.io',
                   'http://evil.example:9000', 'file://', 'chrome-extension://abcdef', 'http://192.168.1.10:8080',
                   'http://0.0.0.0:8080', 'http://localhost:8080/path', 'http://user@localhost:8080',
                   'ws://localhost:8080', 'HTTP://LOCALHOST:8080', null, 42]) {
    assert.strictEqual(isAllowedOrigin(o), false, String(o));
  }
});

// @verifies REQ-SIMFW-27#a
test('the bind address is the IPv4 loopback', () => {
  assert.strictEqual(BIND_HOST, '127.0.0.1');
});

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
  delete env.NP_SIM_FW_BIN;                                     // the default mode
  const child = spawn(process.execPath, [path.join(__dirname, 'index.js')], { env, stdio: ['ignore', 'pipe', 'pipe'] });
  let out = '';
  const listening = await new Promise((resolve, reject) => {
    const timer = setTimeout(() => reject(new Error(`server did not start:\n${out}`)), 10_000);
    const onData = (d) => {
      out += d;
      const m = /listening on ws:\/\/(\S+):(\d+)/.exec(out);
      if (m) { clearTimeout(timer); resolve({ address: m[1], port: Number(m[2]) }); }
    };
    child.stdout.on('data', onData);
    child.stderr.on('data', onData);
    child.once('exit', (code) => { clearTimeout(timer); reject(new Error(`server exited ${code}:\n${out}`)); });
  });
  return { child, port, listening };
}

/** One WebSocket upgrade; resolves with the HTTP status (101 when accepted). */
function upgrade(host, port, origin) {
  return new Promise((resolve, reject) => {
    const headers = {
      Connection: 'Upgrade', Upgrade: 'websocket', 'Sec-WebSocket-Version': '13',
      'Sec-WebSocket-Key': crypto.randomBytes(16).toString('base64'),
    };
    if (origin !== undefined) headers.Origin = origin;
    const req = http.request({ host, port, path: '/', headers });
    req.once('upgrade', (res, socket) => { socket.destroy(); resolve(res.statusCode); });
    req.once('response', (res) => { res.resume(); resolve(res.statusCode); });
    req.once('error', reject);
    req.end();
  });
}

let server;
test.before(async () => { server = await startServer(); });
test.after(() => { if (server) server.child.kill(); });

// @verifies REQ-SIMFW-27#a
test('the running server listens on the loopback address only, in the default mode', async () => {
  const { port, listening } = server;
  assert.deepStrictEqual(listening, { address: '127.0.0.1', port });
  // Every other address this host has is refused. A host with none has nothing else to reach.
  const others = Object.values(os.networkInterfaces()).flat()
    .filter((i) => i && i.family === 'IPv4' && !i.internal).map((i) => i.address);
  for (const addr of others) {
    await assert.rejects(upgrade(addr, port, undefined), { code: 'ECONNREFUSED' }, addr);
  }
});

// @verifies REQ-SIMFW-27#b
test('the running server accepts a native client and the loopback origins', async () => {
  const { port } = server;
  assert.strictEqual(await upgrade('127.0.0.1', port, undefined), 101);
  assert.strictEqual(await upgrade('127.0.0.1', port, 'http://localhost:8080'), 101);
  assert.strictEqual(await upgrade('127.0.0.1', port, 'http://127.0.0.1:8080'), 101);
});

// @verifies REQ-SIMFW-27#b
test('the running server refuses any other page with a 403 before the upgrade', async () => {
  const { port } = server;
  for (const o of ['https://example.com', 'null', 'http://localhost.example.com:8080']) {
    assert.strictEqual(await upgrade('127.0.0.1', port, o), 403, o);
  }
});
