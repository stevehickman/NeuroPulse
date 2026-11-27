'use strict';
/**
 * The simulator device API's session gate (OI-PRIV-43, REQ-SIMFW-28).
 *
 * The server says it speaks "the same wire protocol the NeurOne hub uses", so it refuses what the hub
 * refuses (CLAUDE.md §4.2):
 *   - a SESSION_START whose descriptor carries no valid signature, or that arrives while a session runs
 *     (the hub's session lease, REQ-SIMFW-17 (e));
 *   - a ZONE_CONFIG while a session runs.
 *
 * What is signed. The bytes are framed the way the hub's transport frames an upload (REQ-SIMFW-17 (a)):
 * a 4-byte little-endian length, then the blob. The blob is a domain tag, a 0x00 byte, then the descriptor
 * without its `signature` member as canonical JSON (RFC 8785, JCS: object members sorted by UTF-16 code
 * unit, no whitespace, numbers and strings written as ECMAScript JSON.stringify writes them). Two clients
 * that build the same descriptor in a different key order sign the same bytes. The descriptor here names a
 * protocol by slug; it is not the compiled protocol blob the hub verifies, which only the firmware-twin
 * path carries (REQ-SIMFW-17, NP-SIM-FW-001 §26).
 *
 * Who checks the signature. Ed25519 is the shared NPPS core's (`ed25519Verify`, common/npps-core
 * primitives.rs), reached as WebAssembly (CLAUDE.md §22): nothing here signs or verifies in JavaScript.
 * Without the module (scripts/build-npps-wasm.sh) every SESSION_START is refused: the gate fails closed.
 *
 * The key is a TEST key (simulator/server/keys/README.md, REQ-SIMFW-08); a signature it checks says
 * nothing about a production protocol.
 *
 * The link is plain ws:// and carries this in the clear: the CLAUDE.md §23 gap `OI-SIMFW-71`, not solved here.
 */

const fs = require('fs');
const path = require('path');

const DOMAIN = 'NP-SIM-SESSION-START/1';
const DEFAULT_WASM = path.join(__dirname, '..', 'js', 'generated', 'neurone_npps.wasm');
const DEFAULT_PUBLIC_KEY = path.join(__dirname, 'keys', 'test-public.pem');
const KEY_NOTE = 'checked against the simulator TEST key (simulator/server/keys/), never a production key';

// DER prefixes of an Ed25519 key in SPKI (public) and PKCS#8 (private) form, RFC 8410 §4, §7: the key's 32
// bytes follow them. Reading them is parsing, not cryptography.
const SPKI_ED25519 = Buffer.from('302a300506032b6570032100', 'hex');
const PKCS8_ED25519 = Buffer.from('302e020100300506032b657004220420', 'hex');

class GateError extends Error {}

/** RFC 8785 canonical JSON of a JSON value. Refuses what JSON cannot carry exactly. */
function canonicalJson(v) {
  if (v === null || typeof v === 'boolean') return JSON.stringify(v);
  if (typeof v === 'number') {
    if (!Number.isFinite(v)) throw new GateError('a descriptor number must be finite');
    return JSON.stringify(v);
  }
  if (typeof v === 'string') return JSON.stringify(v);
  if (Array.isArray(v)) return `[${v.map(canonicalJson).join(',')}]`;
  if (typeof v === 'object' && Object.getPrototypeOf(v) === Object.prototype) {
    const keys = Object.keys(v).sort();
    return `{${keys.map((k) => `${JSON.stringify(k)}:${canonicalJson(v[k])}`).join(',')}}`;
  }
  throw new GateError(`a descriptor cannot hold a ${typeof v}`);
}

/** The bytes a SESSION_START descriptor's signature covers: u32le(length) || DOMAIN || 0x00 || JCS(descriptor − signature). */
function signedBytes(descriptor) {
  if (descriptor === null || typeof descriptor !== 'object' || Array.isArray(descriptor)) {
    throw new GateError('a SESSION_START needs a descriptor object');
  }
  const { signature: _omit, ...payload } = descriptor;
  const blob = Buffer.concat([Buffer.from(DOMAIN, 'utf8'), Buffer.from([0]), Buffer.from(canonicalJson(payload), 'utf8')]);
  const frame = Buffer.alloc(4);
  frame.writeUInt32LE(blob.length, 0);
  return Buffer.concat([frame, blob]);
}

function rawKey(pem, prefix, what) {
  const der = Buffer.from(pem.replace(/-----[^-]+-----/g, '').replace(/\s+/g, ''), 'base64');
  if (der.length !== prefix.length + 32 || !der.subarray(0, prefix.length).equals(prefix)) {
    throw new GateError(`not an Ed25519 ${what} key`);
  }
  return der.subarray(prefix.length);
}

/** The shared core's npps_migration_json, from the WebAssembly module (the C ABI of common/npps-ffi). */
function loadCore(wasmPath) {
  const wasm = new WebAssembly.Instance(new WebAssembly.Module(fs.readFileSync(wasmPath)), {}).exports;
  return function migration(request) {
    const bytes = Buffer.from(JSON.stringify(request), 'utf8');
    const inPtr = wasm.npps_alloc(bytes.length + 1);
    const cells = wasm.npps_alloc(8);
    try {
      new Uint8Array(wasm.memory.buffer, inPtr, bytes.length).set(bytes);
      const code = wasm.npps_migration_json(inPtr, bytes.length, cells, cells + 4);
      const view = new DataView(wasm.memory.buffer);
      const outPtr = view.getUint32(cells, true);
      const outLen = view.getUint32(cells + 4, true);
      const out = outPtr === 0 ? '' : Buffer.from(new Uint8Array(wasm.memory.buffer, outPtr, outLen)).toString('utf8');
      if (outPtr !== 0) wasm.npps_free(outPtr, outLen);
      if (code !== 0) throw new GateError(`the NPPS core refused: ${out}`);
      return JSON.parse(out);
    } finally {
      wasm.npps_free(inPtr, bytes.length + 1);
      wasm.npps_free(cells, 8);
    }
  };
}

const STRICT_BASE64 = /^[A-Za-z0-9+/]{86}==$/;   // 64 bytes, padded

/**
 * The signature check. `core` may be injected (tests); otherwise the WebAssembly module is loaded, and if it
 * cannot be, `available` is false and every descriptor is refused.
 */
function createVerifier({ wasmPath = DEFAULT_WASM, publicKeyPem, core } = {}) {
  const publicKey = rawKey(publicKeyPem ?? fs.readFileSync(DEFAULT_PUBLIC_KEY, 'utf8'), SPKI_ED25519, 'public').toString('hex');
  let migration = core ?? null;
  let loadError = null;
  if (!migration) {
    try { migration = loadCore(wasmPath); } catch (e) { loadError = e.message; }
  }
  return {
    available: migration !== null,
    loadError,
    verify(descriptor) {
      if (!migration) {
        return { ok: false, code: 'VERIFIER_UNAVAILABLE',
          message: 'The NPPS core is not built, so no signature can be checked: run scripts/build-npps-wasm.sh' };
      }
      let message;
      try { message = signedBytes(descriptor); } catch (e) {
        if (!(e instanceof GateError)) throw e;
        return { ok: false, code: 'INVALID_DESCRIPTOR', message: e.message };
      }
      if (typeof descriptor.signature !== 'string' || descriptor.signature === '') {
        return { ok: false, code: 'UNSIGNED', message: `A SESSION_START must carry a signature; the hub refuses an unsigned protocol (${KEY_NOTE})` };
      }
      if (!STRICT_BASE64.test(descriptor.signature)) {
        return { ok: false, code: 'INVALID_SIGNATURE', message: 'The signature must be 64 bytes, base64 with padding' };
      }
      const answer = migration({ op: 'ed25519Verify', publicKey, message: message.toString('hex'),
        signature: Buffer.from(descriptor.signature, 'base64').toString('hex') });
      return answer.ok === true
        ? { ok: true }
        : { ok: false, code: 'INVALID_SIGNATURE', message: `Ed25519 signature failed (${KEY_NOTE})` };
    },
  };
}

/**
 * SIMULATOR TEST AID: signs a descriptor with the TEST private key, through the shared core. It exists for
 * the tests and the examples in simulator/HOWTO.md §17.4, and a signature it makes is a TEST signature.
 */
function signDescriptorForTest(descriptor, privateKeyPem, { wasmPath = DEFAULT_WASM, core } = {}) {
  const migration = core ?? loadCore(wasmPath);
  const seed = rawKey(privateKeyPem, PKCS8_ED25519, 'private').toString('hex');
  const { signature } = migration({ op: 'ed25519Sign', seed, message: signedBytes(descriptor).toString('hex') });
  const { signature: _old, ...payload } = descriptor;
  return { ...payload, signature: Buffer.from(signature, 'hex').toString('base64') };
}

/**
 * Whether a session runs, as the server can know it: one it relayed and has not seen stopped or completed,
 * or one a display reports in its TELEMETRY (the page can start a session from its own controls). A paused
 * session still runs. With no display connected, nothing runs.
 */
class SessionGate {
  constructor(verifier) {
    this.verifier = verifier;
    this.started = false;           // relayed by this server, not yet stopped or completed
    this.displayRunning = false;    // the last TELEMETRY from a display said running
  }

  get running() { return this.started || this.displayRunning; }

  admitStart(descriptor) {
    if (this.running) {
      return { ok: false, code: 'SESSION_ACTIVE', message: 'A session is running; the hub refuses a second protocol while one holds the session (stop it first)' };
    }
    const verdict = this.verifier.verify(descriptor);
    if (verdict.ok) this.started = true;
    return verdict;
  }

  admitZoneConfig() {
    return this.running
      ? { ok: false, code: 'SESSION_ACTIVE', message: 'Zones cannot change while a session runs (stop it first)' }
      : { ok: true };
  }

  noteStop() { this.started = false; this.displayRunning = false; }
  noteComplete() { this.noteStop(); }
  noteTelemetry(snap) { if (typeof snap?.running === 'boolean') this.displayRunning = snap.running; }
  noteDisplays(count) { if (count === 0) this.noteStop(); }
}

module.exports = { DOMAIN, GateError, canonicalJson, signedBytes, createVerifier, signDescriptorForTest, SessionGate, loadCore };
