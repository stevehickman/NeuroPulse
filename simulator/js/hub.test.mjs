/**
 * Tests for the hub's own tasks as the page knows them (NP-SIM-FW-001 §31, REQ-SIMFW-23).
 * Run: node --test js/hub.test.mjs
 */
import test from 'node:test';
import assert from 'node:assert';
import { createRequire } from 'node:module';
import { HubModel, HUB_SCRIPTS, MODALITY_BITS, GATT, WRITE, TICK_MS } from './hub.js';
import { SimModuleRegistry } from './simmodule.js';

const require = createRequire(import.meta.url);
const { messageToLines } = require('../server/fw-bridge');

const st = (over = {}) => ({ ev: 'hub_state', t: 10, why: 'run', runner: 'running', part: 0, active_mask: 512, elapsed_ms: 1500, remaining_ms: 18500, total_ms: 20000,
  ramp_phase: 0, ramp_pct: 0, hold: 0, led: 'session', safety_mcu: 'in_loop', ...over });

// @verifies REQ-SIMFW-23#g
test('every message the hub panel sends passes the bridge\'s own validation, and none starts or paces a session by a command of its own', () => {
  for (const [name, build] of Object.entries(HUB_SCRIPTS)) {
    for (const m of build()) {
      assert.doesNotThrow(() => messageToLines(m), `${name}: ${JSON.stringify(m)}`);
      assert.ok(!/^hub_(start|stop|pause|resume|session)/.test(m.cmd), `${name}: ${m.cmd}`);
    }
  }
  assert.deepStrictEqual(HUB_SCRIPTS.tick().flatMap(messageToLines), [`hub_run ${TICK_MS}`]);
  assert.deepStrictEqual(HUB_SCRIPTS.stop().flatMap(messageToLines), [`gatt_write ${GATT.stop} ${WRITE.stop}`], 'a stop is the app\'s own write to the hub\'s characteristic');
  assert.deepStrictEqual(HUB_SCRIPTS.pause().flatMap(messageToLines), [`gatt_write ${GATT.pause} 01`]);
  assert.deepStrictEqual(HUB_SCRIPTS.resume().flatMap(messageToLines), [`gatt_write ${GATT.pause} 02`]);
  assert.ok(HUB_SCRIPTS.boot('refuse').flatMap(messageToLines).includes('hub_boot refuse'));
});

// @verifies REQ-SIMFW-23#g
test('the model shows only what the runner\'s progress frame says, and a late-joining page is given the last state', () => {
  const m = new HubModel();
  assert.strictEqual(m.running, false);
  assert.strictEqual(m.runner, 'not running');
  m.onMessage({ type: 'HUB_STATE', event: st() });
  assert.strictEqual(m.runner, 'running');
  assert.deepStrictEqual(m.activeModalities(), ['Visual']);
  assert.strictEqual(m.progress(), 0.075);
  assert.ok(m.canPause() && m.canStop() && !m.canResume());
  m.onMessage({ type: 'HUB_STATE', event: st({ runner: 'paused', active_mask: 0 }) });
  assert.ok(!m.canPause() && m.canResume() && m.canStop());
  assert.deepStrictEqual(m.activeModalities(), []);
  m.onMessage({ type: 'HUB_STATE', event: { ev: 'hub_led', led: 'fault' } });
  assert.strictEqual(m.state.led, 'fault');
  const late = new HubModel();
  late.onMessage({ type: 'MODULE_LIST', modules: [], safety: null, hub: st({ runner: 'complete' }) });
  assert.strictEqual(late.runner, 'complete');
});

// @verifies REQ-SIMFW-23#g
test('the page\'s registry feeds the hub model, and saying anything makes the page one that talks to a firmware process', () => {
  const reg = new SimModuleRegistry();
  assert.strictEqual(reg.sawFirmware, false);
  reg.onMessage({ type: 'HUB_STATE', event: st() }, 0);
  assert.strictEqual(reg.sawFirmware, true);
  assert.strictEqual(reg.hub.runner, 'running');
});

// @verifies REQ-SIMFW-23#g
test('the modality names are the hub\'s np_hub_mod_type_t order', () => {
  assert.strictEqual(MODALITY_BITS[5], 'BES / tACS');
  assert.strictEqual(MODALITY_BITS[9], 'Visual');
});
