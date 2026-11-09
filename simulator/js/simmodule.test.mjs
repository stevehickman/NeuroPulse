/**
 * Tests for SimModule and SimModuleRegistry (NP-SIM-FW-001).  Run: node --test js/simmodule.test.mjs
 * The end-to-end test runs the real np_sim_pbm_spike when NP_SIM_FW_BIN names it.
 */
import test from 'node:test';
import assert from 'node:assert';
import { createRequire } from 'node:module';
import { SimModule, SimModuleRegistry, FAULT_FLASH_S, latticeIdToFirmwareIndex } from './simmodule.js';

const require = createRequire(import.meta.url);

function mesh() {
  const hex = (o) => ({ h: 0, setHex(v) { this.h = v; } , ...o });
  return { material: { color: hex(), emissive: hex(), emissiveIntensity: -1 } };
}

// @verifies-none Rendering of one module from firmware events; a browser feature, no requirement row.
test('a module is drawn from the firmware events it was fed, and from nothing else', () => {
  const m = new SimModule('pbm_base:3', 'pbm_base', 3);
  const me = mesh();
  m.apply({ ev: 'module', present: true, ntc_c: 34 }, 0);
  m.render(me, 0);
  assert.strictEqual(me.material.emissiveIntensity, 0.05, 'present and idle');
  m.apply({ ev: 'hal', op: 'socket_pwm_set', args: [64, 64, 40, 32], code: 0, source: 'default' }, 1);
  m.render(me, 1);
  assert.strictEqual(me.material.emissive.h, 0xff3b1f, 'driven');
  m.apply({ ev: 'module_state', present: true, driven: false, cur_a: 0, cur_b: 0, ntc_c: 34 }, 2);
  m.render(me, 2);
  assert.strictEqual(me.material.emissiveIntensity, 0.05, 'dark again when the firmware says so');
  m.apply({ ev: 'module_state', present: false }, 3);
  m.render(me, 3);
  assert.strictEqual(me.material.emissiveIntensity, 0, 'absent');
});

// @verifies REQ-SIMFW-04
test('a module records which ops it has only ever answered from the default model', () => {
  const m = new SimModule('pbm_base:3', 'pbm_base', 3);
  m.apply({ ev: 'hal', op: 'socket_pwm_set', args: [1, 1, 0, 0], code: 0, source: 'default' }, 0);
  assert.ok(m.unscripted.has('socket_pwm_set'));
  const reg = new SimModuleRegistry();
  assert.strictEqual(reg.banner(), '', 'no banner while no firmware is attached');
  reg.onMessage({ type: 'MODULE_STATE', event: { ev: 'module', id: 'pbm_base:1', kind: 'pbm_base', index: 1, present: true } }, 0);
  assert.match(reg.banner(), /Safety MCU not in loop/, 'the scope notice appears with the first module');
});

// @verifies REQ-SIMFW-11
test('an event for one tile changes no other tile', () => {
  const reg = new SimModuleRegistry();
  for (const i of [0, 1, 2]) {
    reg.onMessage({ type: 'MODULE_STATE', event: { ev: 'module', id: `pbm_base:${i}`, kind: 'pbm_base', index: i, present: true } }, 0);
  }
  reg.onMessage({ type: 'FW_LOG', event: { ev: 'hal', id: 'pbm_base:1', op: 'socket_pwm_set', args: [64, 64, 40, 32], code: -17, source: 'injected' } }, 5);
  assert.strictEqual(reg.bySocket(1).lastFault.code, -17);
  assert.strictEqual(reg.bySocket(0).lastFault, null);
  assert.strictEqual(reg.bySocket(2).lastFault, null);
  assert.strictEqual(reg.bySocket(1).driven, false, 'a failed write does not mark the tile driven');
  const me = mesh();
  reg.bySocket(1).render(me, 5.1);
  assert.strictEqual(me.material.emissive.h, 0xff2d9a, 'the faulted tile flashes');
  reg.bySocket(1).render(me, 5 + FAULT_FLASH_S + 0.1);
  assert.notStrictEqual(me.material.emissive.h, 0xff2d9a, 'and stops flashing');
});

// @verifies-none The lattice id is 1-based and the firmware index 0-based (OI-SIMFW-06); a browser mapping, no requirement row.
test('the lattice id (1..80) maps to the firmware index (0..79), and a registry is keyed by index', () => {
  assert.strictEqual(latticeIdToFirmwareIndex(1), 0);
  assert.strictEqual(latticeIdToFirmwareIndex(80), 79);
  const reg = new SimModuleRegistry();
  reg.onMessage({ type: 'MODULE_STATE', event: { ev: 'module', id: 'pbm_base:0', kind: 'pbm_base', index: 0, present: true } }, 0);
  assert.ok(reg.byLatticeId(1), 'firmware index 0 is lattice socket 1');
  assert.strictEqual(reg.byLatticeId(0), undefined, 'there is no lattice socket 0');
  assert.strictEqual(reg.byLatticeId(2), undefined);
});

const BIN = process.env.NP_SIM_FW_BIN;

// @verifies-none End to end: the real firmware process through the bridge into the registry. Simulator evidence only (REQ-SIMFW-06).
// OI-SIMFW-11 (closed): the simulator links the production power governor, which refuses every load (OI-FWHUB-09), as the
// device does. A drive therefore returns NP_HUB_ERR_POWER_BUDGET (-18) before any tile write, so an injected write fault
// is armed and never consumed. The write-fault and rollback path behind the governor is covered in C by np_sim_pbm_tests,
// which opens the governor through a C-only seam that no bridge message reaches.
test('end to end: the real firmware refuses a PBM drive at the power governor (-18), drives nothing, and an armed write fault is never reached', { skip: !BIN }, async () => {
  const { FwBridge } = require('../server/fw-bridge');
  const reg = new SimModuleRegistry();
  const bridge = new FwBridge({ bin: BIN }).start();
  let t = 0;
  bridge.on('message', (m) => reg.onMessage(m, (t += 0.001)));
  for (const s of [0, 1, 2]) bridge.handle({ type: 'MODULE_SET', socket: s, patch: { add: 34 } });
  bridge.handle({ type: 'MODULE_FAIL', socket: 1, op: 'socket_pwm_set', code: -17, times: 1 });
  bridge.handle({ type: 'FW_COMMAND', cmd: 'drive', sockets: [0, 1, 2] });
  bridge.handle({ type: 'FW_COMMAND', cmd: 'status' });
  await new Promise((r) => setTimeout(r, 400));
  bridge.stop();
  assert.strictEqual(reg.size, 3);
  const drive = reg.log.find((e) => e.ev === 'result' && e.cmd === 'drive');
  assert.ok(drive, 'the firmware answered the drive command');
  assert.strictEqual(drive.code, -18, 'refused at the governor: NP_HUB_ERR_POWER_BUDGET');
  assert.strictEqual(drive.active, 0, 'no tile was activated');
  assert.ok([0, 1, 2].every((i) => reg.bySocket(i).driven === false), 'all dark: nothing is drawn as driven');
  assert.ok([0, 1, 2].every((i) => reg.bySocket(i).lastFault === null), 'no write was attempted, so no tile faulted');
});
