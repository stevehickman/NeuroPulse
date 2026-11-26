'use strict';
/**
 * Tests for the device API's session gate (OI-PRIV-43, NP-SIM-FW-001 REQ-SIMFW-27). Run: node --test server/
 * They need the shared core as WebAssembly (scripts/build-npps-wasm.sh): the signature is the core's, and a
 * missing module is a failure here, not a skip.
 */
const test = require('node:test');
const assert = require('node:assert');
const fs = require('node:fs');
const path = require('node:path');
const { canonicalJson, signedBytes, createVerifier, signDescriptorForTest, SessionGate, loadCore, DOMAIN } = require('./session-auth');

const PRIVATE = fs.readFileSync(path.join(__dirname, 'keys', 'test-private.pem'), 'utf8');
const core = loadCore(path.join(__dirname, '..', 'js', 'generated', 'neurone_npps.wasm'));
const verifier = createVerifier({ core });
const sign = (d) => signDescriptorForTest(d, PRIVATE, { core });

const DESCRIPTOR = { protocolId: '13-focus-prime',
  zones: [{ zoneId: 'Frontal Left', config: { installed: true, wavelengths: ['660nm', '1064nm'], frequency: 40 } }] };

// @verifies REQ-SIMFW-27#a
test('a SESSION_START with no signature, an empty one or a malformed one is refused', () => {
  const gate = new SessionGate(verifier);
  for (const d of [{ protocolId: '01-gamma-focus' }, { protocolId: '01-gamma-focus', signature: '' },
    { protocolId: '01-gamma-focus', signature: 42 }, { protocolId: '01-gamma-focus', signature: 'not base64' }]) {
    const v = gate.admitStart(d);
    assert.strictEqual(v.ok, false, JSON.stringify(d));
    assert.match(v.code, /^(UNSIGNED|INVALID_SIGNATURE)$/);
  }
  for (const d of [undefined, null, [], 'x']) {
    assert.strictEqual(gate.admitStart(d).code, 'INVALID_DESCRIPTOR');
  }
  assert.strictEqual(gate.running, false, 'nothing refused starts a session');
});

// @verifies REQ-SIMFW-27#a
test('a signature from another key, or over a changed descriptor, is refused; a TEST-key signature is accepted', () => {
  const signed = sign(DESCRIPTOR);
  assert.deepStrictEqual(verifier.verify(signed), { ok: true });

  const tampered = { ...signed, protocolId: '01-gamma-focus' };
  assert.strictEqual(verifier.verify(tampered).code, 'INVALID_SIGNATURE');

  const zonesTampered = JSON.parse(JSON.stringify(signed));
  zonesTampered.zones[0].config.frequency = 41;
  assert.strictEqual(verifier.verify(zonesTampered).code, 'INVALID_SIGNATURE');

  // Another Ed25519 key (seed of all 0x01), in PKCS#8 form.
  const other = '-----BEGIN PRIVATE KEY-----\n' +
    Buffer.concat([Buffer.from('302e020100300506032b657004220420', 'hex'), Buffer.alloc(32, 1)]).toString('base64') +
    '\n-----END PRIVATE KEY-----\n';
  const foreign = signDescriptorForTest(DESCRIPTOR, other, { core });
  const v = verifier.verify(foreign);
  assert.strictEqual(v.code, 'INVALID_SIGNATURE');
  assert.match(v.message, /TEST key/, 'the refusal says the key is a test key');
});

// @verifies REQ-SIMFW-27#a
test('without the shared core every SESSION_START is refused (fail closed)', () => {
  const none = createVerifier({ wasmPath: path.join(__dirname, 'no-such.wasm') });
  assert.strictEqual(none.available, false);
  const signed = sign(DESCRIPTOR);
  assert.strictEqual(new SessionGate(none).admitStart(signed).code, 'VERIFIER_UNAVAILABLE');
});

// @verifies REQ-SIMFW-27#b
test('the signed bytes are framed and canonical: key order does not change them', () => {
  const reordered = { zones: [{ config: { frequency: 40, wavelengths: ['660nm', '1064nm'], installed: true }, zoneId: 'Frontal Left' }],
    protocolId: '13-focus-prime' };
  assert.deepStrictEqual(signedBytes(reordered), signedBytes(DESCRIPTOR));
  assert.deepStrictEqual(verifier.verify({ ...reordered, signature: sign(DESCRIPTOR).signature }), { ok: true });

  const bytes = signedBytes(DESCRIPTOR);
  assert.strictEqual(bytes.readUInt32LE(0), bytes.length - 4, 'a 4-byte little-endian length, then the blob');
  const blob = bytes.subarray(4);
  assert.strictEqual(blob.subarray(0, DOMAIN.length).toString('utf8'), DOMAIN);
  assert.strictEqual(blob[DOMAIN.length], 0);
  assert.strictEqual(blob.subarray(DOMAIN.length + 1).toString('utf8'), canonicalJson(DESCRIPTOR));
  assert.deepStrictEqual(signedBytes({ ...DESCRIPTOR, signature: 'ignored' }), bytes, 'the signature is not signed');

  assert.strictEqual(canonicalJson({ b: 1, a: [true, null, 'é'], c: { z: 1.5, y: -0 } }), '{"a":[true,null,"é"],"b":1,"c":{"y":0,"z":1.5}}');
  assert.throws(() => canonicalJson({ a: NaN }));
  assert.throws(() => canonicalJson({ a: Infinity }));
});

// @verifies REQ-SIMFW-27#c
test('a second SESSION_START and a ZONE_CONFIG are refused while a session runs, paused or not', () => {
  const gate = new SessionGate(verifier);
  assert.deepStrictEqual(gate.admitZoneConfig(), { ok: true }, 'zones change while nothing runs');
  assert.deepStrictEqual(gate.admitStart(sign(DESCRIPTOR)), { ok: true });
  assert.strictEqual(gate.admitStart(sign({ protocolId: '01-gamma-focus' })).code, 'SESSION_ACTIVE');
  assert.strictEqual(gate.admitZoneConfig().code, 'SESSION_ACTIVE');

  gate.noteTelemetry({ running: true, paused: true });
  assert.strictEqual(gate.admitZoneConfig().code, 'SESSION_ACTIVE', 'a paused session still runs');

  gate.noteStop();
  assert.deepStrictEqual(gate.admitZoneConfig(), { ok: true });

  // A session the page started from its own controls, reported in TELEMETRY.
  gate.noteTelemetry({ running: true });
  assert.strictEqual(gate.admitZoneConfig().code, 'SESSION_ACTIVE');
  assert.strictEqual(gate.admitStart(sign(DESCRIPTOR)).code, 'SESSION_ACTIVE');
  gate.noteComplete();
  assert.deepStrictEqual(gate.admitZoneConfig(), { ok: true });

  // A relayed session stays running though a TELEMETRY says otherwise (the display may not have taken it yet).
  assert.deepStrictEqual(gate.admitStart(sign(DESCRIPTOR)), { ok: true });
  gate.noteTelemetry({ running: false });
  assert.strictEqual(gate.admitZoneConfig().code, 'SESSION_ACTIVE');

  // With no display left, nothing runs.
  gate.noteDisplays(1);
  assert.strictEqual(gate.running, true);
  gate.noteDisplays(0);
  assert.strictEqual(gate.running, false);
});
