/**
 * Tests for the safety MCU as the page knows it (NP-SIM-FW-001 §23, REQ-SIMFW-07, REQ-SIMFW-15).
 * Run: node --test js/safety.test.mjs js/simmodule.test.mjs
 */
import test from 'node:test';
import assert from 'node:assert';
import { createRequire } from 'node:module';
import { HUB_MODULE_LIST, SafetyMcu, SCRIPTS, LINES, VERDICT, decodeStatus, describeSlot, lineNameFor, TEST_AID, FAULTS, FAULTS_UNAVAILABLE, outageScript, OUTAGE_MIN_MS, OUTAGE_MAX_MS } from './safety.js';
import { SimModuleRegistry } from './simmodule.js';
import { accessoryStates } from './accessories.js';

const require = createRequire(import.meta.url);
const { messageToLines, SAFETY_LINES } = require('../server/fw-bridge');

const state = (over = {}) => ({
  ev: 'safety_state', cmd: 'status', code: 0, t: 1000, attached: true, held_in_reset: false, iwdg_resets: 0, iterations: 1, requested: 0,
  granted: 0, status: 0, fault_slot: 255, lines: 0, spi_overruns: 0, spi_discarded: 0, tier: 1, tier_reason: 0, tier_refused: false,
  safety_mcu: 'in_loop', ...over,
});
const BES_LINE = 1 << 1;      // index 1 of `lines`
const BES_BIT = 1 << 5;       // bit 5 of requested / granted

// @verifies REQ-SIMFW-15#c
test('the page\'s line table is the bridge\'s: same names, same order, same enable bits', () => {
  assert.deepStrictEqual(LINES.map((l) => l.name), Object.keys(SAFETY_LINES));
  assert.deepStrictEqual(LINES.map((l) => 1 << l.bit), Object.values(SAFETY_LINES));
});

// @verifies REQ-SIMFW-15#d
test('with no safety MCU the verdict is not_in_loop, and the module is drawn on the hub\'s command', () => {
  const reg = new SimModuleRegistry();
  reg.onMessage({ type: 'MODULE_STATE', event: { ev: 'module', id: 'stim_pair:0', kind: 'stim_pair', index: 0, present: true } }, 0);
  reg.onMessage({ type: 'MODULE_STATE', event: { ev: 'module_state', id: 'stim_pair:0', present: true, driven: true, dc: false } }, 0);
  const m = reg.byKind('stim_pair')[0];
  assert.strictEqual(reg.safety.label, 'not_in_loop');
  assert.strictEqual(m.gate.verdict, VERDICT.NOT_IN_LOOP);
  assert.strictEqual(m.drawnDriven, true, 'the existing hub-commanded behaviour');
  assert.match(reg.banner(), /Safety MCU not in loop/, 'and the page says so');
});

// @verifies REQ-SIMFW-07 REQ-SIMFW-15#c
test('with the safety MCU in the loop a module is drawn driven only while its line is enabled', () => {
  const reg = new SimModuleRegistry();
  reg.onMessage({ type: 'MODULE_STATE', event: { ev: 'module', id: 'stim_pair:0', kind: 'stim_pair', index: 0, present: true } }, 0);
  reg.onMessage({ type: 'MODULE_STATE', event: { ev: 'module_state', id: 'stim_pair:0', present: true, driven: true, dc: false } }, 0);
  const m = reg.byKind('stim_pair')[0];
  reg.onMessage({ type: 'SAFETY_STATE', event: state({ requested: BES_BIT, granted: 0, lines: 0 }) }, 0);
  assert.strictEqual(reg.safety.label, 'in_loop');
  assert.strictEqual(m.gate.verdict, VERDICT.WITHHELD);
  assert.strictEqual(m.driven, true, 'the hub commanded it');
  assert.strictEqual(m.drawnDriven, false, 'the MCU withheld it, so it is not drawn as driven');
  assert.strictEqual(m.withheld, true);
  assert.doesNotMatch(reg.banner(), /not in loop/);
  reg.onMessage({ type: 'SAFETY_STATE', event: state({ requested: BES_BIT, granted: BES_BIT, lines: BES_LINE }) }, 0);
  assert.strictEqual(m.gate.verdict, VERDICT.GRANTED);
  assert.strictEqual(m.drawnDriven, true, 'granted: drawn driven');
  reg.onMessage({ type: 'SAFETY_STATE', event: state({ requested: BES_BIT, granted: BES_BIT, lines: 0, status: 0x04 }) }, 0);
  assert.strictEqual(m.drawnDriven, false, 'cut again: the line is the authority, not the last reply\'s granted mask');
});

// @verifies REQ-SIMFW-15#c
test('the line that gates a module follows its kind, and a stimulation pair follows its channel; audio and EEG are not gated', () => {
  assert.strictEqual(lineNameFor('pbm_base'), 'pbm_cranial');
  assert.strictEqual(lineNameFor('stim_pair', { dc: true }), 'tdcs');
  assert.strictEqual(lineNameFor('stim_pair', { dc: false }), 'bes');
  assert.strictEqual(lineNameFor('cvns'), 'cvns');
  assert.strictEqual(lineNameFor('audio'), null);
  assert.strictEqual(lineNameFor('eeg'), null);
  const s = new SafetyMcu();
  s.apply(state());
  assert.strictEqual(s.gateFor('audio', {}).verdict, VERDICT.UNGATED, 'in the loop, but nothing for the MCU to grant');
});

// @verifies REQ-SIMFW-15#c
test('the tile path: a PBM tile is dim amber when commanded and withheld, red only when granted', () => {
  const reg = new SimModuleRegistry();
  reg.onMessage({ type: 'MODULE_STATE', event: { ev: 'module', id: 'pbm_base:0', kind: 'pbm_base', index: 0, present: true, ntc_c: 34 } }, 0);
  reg.onMessage({ type: 'MODULE_STATE', event: { ev: 'module_state', id: 'pbm_base:0', present: true, driven: true, cur_a: 64, cur_b: 64, ntc_c: 34 } }, 0);
  const hex = () => ({ h: 0, setHex(v) { this.h = v; } });
  const mesh = { material: { color: hex(), emissive: hex(), emissiveIntensity: -1 } };
  reg.onMessage({ type: 'SAFETY_STATE', event: state({ requested: 1, lines: 0 }) }, 0);
  reg.bySocket(0).render(mesh, 100);
  assert.strictEqual(mesh.material.emissive.h, 0x8a6d00, 'withheld');
  assert.ok(mesh.material.emissiveIntensity < 0.5);
  reg.onMessage({ type: 'SAFETY_STATE', event: state({ requested: 1, granted: 1, lines: 1 }) }, 0);
  reg.bySocket(0).render(mesh, 100);
  assert.strictEqual(mesh.material.emissive.h, 0xff3b1f, 'granted');
});

// @verifies REQ-SIMFW-07 REQ-SIMFW-15#c
test('an accessory is in use only while its module is drawn driven', () => {
  const reg = new SimModuleRegistry();
  reg.onMessage({ type: 'MODULE_STATE', event: { ev: 'module', id: 'stim_pair:0', kind: 'stim_pair', index: 0, present: true } }, 0);
  reg.onMessage({ type: 'MODULE_STATE', event: { ev: 'module_state', id: 'stim_pair:0', present: true, driven: true, dc: true } }, 0);
  assert.strictEqual(accessoryStates({ registry: reg, snap: {} }).get('stimElectrodes').inUse, true, 'not in the loop: the hub\'s command');
  reg.onMessage({ type: 'SAFETY_STATE', event: state({ requested: 1 << 6, lines: 0 }) }, 0);
  assert.strictEqual(accessoryStates({ registry: reg, snap: {} }).get('stimElectrodes').inUse, false, 'tDCS line withheld');
  reg.onMessage({ type: 'SAFETY_STATE', event: state({ requested: 1 << 6, granted: 1 << 6, lines: 1 << 2 }) }, 0);
  assert.strictEqual(accessoryStates({ registry: reg, snap: {} }).get('stimElectrodes').inUse, true, 'tDCS line enabled');
});

// @verifies REQ-SIMFW-15#b
test('a page that connects late takes the last state from MODULE_LIST, and a SAFETY_EVENT is kept', () => {
  const reg = new SimModuleRegistry();
  reg.onMessage({ type: 'MODULE_LIST', modules: [], safety: state({ lines: BES_LINE }) }, 0);
  assert.strictEqual(reg.safety.inLoop, true);
  assert.strictEqual(reg.safety.lineEnabled('bes'), true);
  reg.onMessage({ type: 'SAFETY_EVENT', event: { ev: 'safety_iwdg_reset', t: 5, resets: 1 } }, 0);
  assert.strictEqual(reg.safety.events.length, 1);
  const none = new SimModuleRegistry();
  none.onMessage({ type: 'MODULE_LIST', modules: [], safety: null }, 0);
  assert.strictEqual(none.safety.inLoop, false);
});

// @verifies REQ-SIMFW-15#c
test('status flags and fault slots are decoded', () => {
  assert.deepStrictEqual(decodeStatus(0x04 | 0x20), ['CUTOFF', 'CHARGE']);
  assert.deepStrictEqual(decodeStatus(0), []);
  assert.strictEqual(describeSlot(0xFD), 'bad session signature');
  assert.strictEqual(describeSlot(5), 'channel 5 (BES / tACS)');
  assert.strictEqual(describeSlot(255), 'none');
});

// @verifies REQ-SIMFW-15#c
test('a flag that rises is the last cutoff, and a heartbeat loss is pending until a reply says why', () => {
  const s = new SafetyMcu();
  s.apply(state({ t: 1000, cmd: 'safety_run', requested: BES_BIT, granted: BES_BIT, lines: BES_LINE }));
  assert.strictEqual(s.lastCutoff, null);
  assert.strictEqual(s.heartbeatAgeMs(), 0);
  s.apply(state({ t: 3500, cmd: 'status', requested: BES_BIT, granted: BES_BIT, lines: 0 }));
  assert.deepStrictEqual(s.lastCutoff.lines, ['bes']);
  assert.strictEqual(s.lastCutoff.pending, true, 'the line dropped before any reply could say why');
  assert.strictEqual(s.heartbeatAgeMs(), 2500);
  s.apply(state({ t: 3700, cmd: 'safety_run', requested: BES_BIT, granted: 0, lines: 0, status: 0x02 | 0x04 }));
  assert.deepStrictEqual(s.lastCutoff.flags, ['WATCHDOG', 'CUTOFF']);
  assert.strictEqual(s.lastCutoff.pending, false);
  const t = new SafetyMcu();
  t.apply(state({ requested: 1, granted: 1, lines: 1 }));
  t.apply(state({ requested: 1, granted: 0, lines: 0, status: 0x10 }));
  assert.deepStrictEqual(t.lastCutoff.flags, ['THERMAL']);
});

// @verifies REQ-SIMFW-15#c
test('the independent watchdog holding the MCU in reset is a cutoff of its own', () => {
  const s = new SafetyMcu();
  s.apply(state({ requested: 1, granted: 1, lines: 1 }));
  s.apply(state({ requested: 1, lines: 0, held_in_reset: true, iwdg_resets: 1 }));
  assert.deepStrictEqual(s.lastCutoff.flags, ['INDEPENDENT_WATCHDOG']);
  assert.strictEqual(s.summary().watchdog.heldInReset, true);
});

// @verifies REQ-SIMFW-15#e
test('every message the test aid sends is one the bridge accepts, and none carries a client mask', () => {
  const scripts = [
    SCRIPTS.attach(), SCRIPTS.attach(2), SCRIPTS.heartbeat(1000), SCRIPTS.heartbeatLoss(2000), SCRIPTS.signSession('good'), SCRIPTS.signSession('bad'),
    SCRIPTS.request('bes'), SCRIPTS.release('bes'), SCRIPTS.contact(2, 5000), SCRIPTS.declare(5, 2, 20000, 1000), SCRIPTS.thermal(TEST_AID.hotAdcCounts),
    SCRIPTS.cardiacBaseline(), SCRIPTS.cardiacStep(), SCRIPTS.cvnsPrepare(), SCRIPTS.hubStim('bes'), SCRIPTS.hubStim('tdcs'), SCRIPTS.hubStimStop('bes'),
    ...LINES.map((l) => SCRIPTS.prepareLine(l.name)).filter((x) => x !== null),
    ...HUB_MODULE_LIST.flatMap((m) => ['place', 'on', 'off'].map((ph) => SCRIPTS.hubModule(m.id, ph))),
  ];
  assert.ok(scripts.length > 17);
  for (const sc of scripts) {
    for (const msg of sc) assert.doesNotThrow(() => messageToLines(msg), JSON.stringify(msg));
  }
  assert.strictEqual(SCRIPTS.prepareLine('tms'), null, 'a line the page does not know how to prepare is not guessed at');
  assert.strictEqual(TEST_AID.seed, 7, 'the TEST root key seed the harness scenarios use');
});

// @verifies REQ-SIMFW-16#a
test('every fault on the panel is a list of messages the bridge accepts, and a clear undoes only what it set', () => {
  assert.ok(FAULTS.length >= 14);
  const ids = new Set();
  for (const f of FAULTS) {
    assert.ok(!ids.has(f.id), `fault id ${f.id} is unique`); ids.add(f.id);
    for (const arg of f.id === 'contact_lost' ? [0, 1, 2, 3] : [undefined]) {
      for (const msg of f.script(arg)) assert.doesNotThrow(() => messageToLines(msg), `${f.id}: ${JSON.stringify(msg)}`);
      if (f.clear) for (const msg of f.clear(arg)) assert.doesNotThrow(() => messageToLines(msg), `${f.id} clear: ${JSON.stringify(msg)}`);
    }
  }
  assert.ok(FAULTS.every((f) => ['any', 'attached', 'unattached'].includes(f.needs)));
});

// @verifies REQ-SIMFW-16#b
test('a fault is an input: no fault script carries a verdict, a line mask or a status value', () => {
  for (const f of FAULTS) for (const msg of f.script(0)) {
    for (const k of ['mask', 'lines', 'granted', 'status', 'verdict', 'fault_slot']) assert.ok(!(k in msg) || (msg.cmd === 'safety_enable'), `${f.id} sets ${k}`);
  }
  assert.ok(FAULTS_UNAVAILABLE.some((u) => /Photoparoxysmal/.test(u.label)), 'a fault the page cannot cause is listed, not hidden');
});

// @verifies REQ-SIMFW-16#c
test('a real outage of the link becomes a heartbeat loss of the same length, and a blip does not', () => {
  assert.strictEqual(outageScript(1000, 1000 + OUTAGE_MIN_MS - 1), null);
  const sc = outageScript(1000, 1000 + 4000);
  assert.deepStrictEqual(sc.map((m) => m.cmd), ['advance', 'status']);
  assert.strictEqual(sc[0].ms, 4000);
  assert.strictEqual(outageScript(0, 10 * 60 * 1000)[0].ms, OUTAGE_MAX_MS, 'capped at one harness call');
  assert.strictEqual(outageScript(5000, 1000), null, 'a clock that went backwards is not an outage');
  for (const m of sc) assert.doesNotThrow(() => messageToLines(m));
});

// @verifies REQ-SIMFW-16#a
test('attach can start a part whose root key is corrupt, and no other message can', () => {
  const lines = messageToLines(SCRIPTS.attach(1, true)[0]);
  assert.ok(lines.indexOf('safety_root_corrupt') > lines.findIndex((l) => l.startsWith('safety_root ')));
  assert.strictEqual(lines.at(-1), 'safety_attach');
  assert.ok(!messageToLines(SCRIPTS.attach(1)[0]).includes('safety_root_corrupt'));
});

// ── the withhold report: which gate held a line (OI-SIMFW-61, REQ-FWHUB-82) ─────────────────────────────────────────────────────────

// @verifies REQ-FWHUB-82#d
test('the withhold report is decoded into line names, and an event without it reads as no report', () => {
  const reg = new SimModuleRegistry();
  reg.onMessage({ type: 'SAFETY_STATE', event: state({ withhold_valid: true, withhold_decl: (1 << 5) | (1 << 6), withhold_geom: 1 << 13 }) }, 0);
  const w = reg.safety.withheld;
  assert.strictEqual(w.valid, true);
  assert.deepStrictEqual(w.declaration, ['bes', 'tdcs'], 'the declaration gate\'s lines, by the enable bit the page\'s line table gives them');
  assert.deepStrictEqual(w.geometry, ['clin_stim']);
  assert.deepStrictEqual(reg.safety.summary().withheld, w, 'and the panel\'s summary carries the same');

  reg.onMessage({ type: 'SAFETY_STATE', event: state({ withhold_valid: false, withhold_decl: 0, withhold_geom: 0 }) }, 0);
  assert.deepStrictEqual(reg.safety.withheld, { valid: false, declaration: [], geometry: [] }, 'a reply with no valid report');

  reg.onMessage({ type: 'SAFETY_STATE', event: state() }, 0);
  assert.deepStrictEqual(reg.safety.withheld, { valid: false, declaration: [], geometry: [] }, 'an event with no such fields');
});

// @verifies REQ-FWHUB-82#d
test('with no safety MCU in the loop there is no withhold report', () => {
  const reg = new SimModuleRegistry();
  assert.deepStrictEqual(reg.safety.withheld, { valid: false, declaration: [], geometry: [] });
});
