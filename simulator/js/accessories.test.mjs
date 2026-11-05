/**
 * Tests for the accessory rules (NP-SIM-FW-001 §21).  Run: node --test js/accessories.test.mjs
 */
import test from 'node:test';
import assert from 'node:assert';
import { ACCESSORIES, HELMET_ACCESSORIES, WINDOW_ACCESSORIES, MOUNT, accessoryStates, windowSlots, windowRects, WINDOW_BASE } from './accessories.js';
import { SimModuleRegistry } from './simmodule.js';

const registryWith = (events) => {
  const r = new SimModuleRegistry();
  for (const ev of events) r.onMessage({ type: ev.ev === 'module' || ev.ev === 'module_state' ? 'MODULE_STATE' : 'FW_LOG', event: ev }, 0);
  return r;
};
const snap = (over = {}) => ({ visual: {}, audio: {}, bes: {}, tdcs: {}, tms: {}, vns: {}, ...over });

test('each accessory is attached to the helmet or has its own window, as the hardware documents say', () => {
  const mount = Object.fromEntries(ACCESSORIES.map((a) => [a.id, a.mount]));
  for (const id of ['goggles', 'earcups', 'tmsCoil', 'hapticPad', 'stimElectrodes']) assert.strictEqual(mount[id], MOUNT.HELMET, id);
  for (const id of ['vnsClip', 'intranasal', 'cvns']) assert.strictEqual(mount[id], MOUNT.WINDOW, id);
  assert.strictEqual(HELMET_ACCESSORIES.length + WINDOW_ACCESSORIES.length, ACCESSORIES.length);
});

test('nothing is in use when nothing is running', () => {
  for (const st of accessoryStates({ snap: snap() }).values()) assert.strictEqual(st.inUse, false, st.id);
  for (const st of accessoryStates({ registry: new SimModuleRegistry(), snap: snap() }).values()) assert.strictEqual(st.inUse, false, st.id);
});

test('standalone: an accessory is in use exactly when its modality is live in the session', () => {
  const st = accessoryStates({ snap: snap({ visual: { active: true }, vns: { active: true }, bes: { active: true } }) });
  assert.strictEqual(st.get('goggles').inUse, true);
  assert.strictEqual(st.get('vnsClip').inUse, true);
  assert.strictEqual(st.get('stimElectrodes').inUse, true);
  assert.strictEqual(st.get('earcups').inUse, false);
  assert.strictEqual(st.get('tmsCoil').inUse, false);
  assert.strictEqual(st.get('cvns').inUse, false, 'no session modality drives the cervical pads');
});

test('standalone: an accessory no protocol drives is in use only when the user shows it', () => {
  assert.strictEqual(accessoryStates({ snap: snap() }).get('intranasal').inUse, false);
  const st = accessoryStates({ snap: snap(), demo: { intranasal: true } });
  assert.strictEqual(st.get('intranasal').inUse, true);
  assert.strictEqual(st.get('hapticPad').inUse, false);
});

test('firmware: an accessory is in use exactly when its module is driven, and the session is not consulted', () => {
  const r = registryWith([
    { ev: 'module', id: 'vns:0', kind: 'vns', index: 0, present: true },
    { ev: 'module_state', id: 'vns:0', present: true, driven: true, left: true, right: false, amp_ua: 2000 },
    { ev: 'module', id: 'intranasal:0', kind: 'intranasal', index: 0, present: true },
    { ev: 'module_state', id: 'intranasal:0', present: true, driven: false },
  ]);
  const st = accessoryStates({ registry: r, snap: snap({ visual: { active: true }, tms: { active: true } }) });
  assert.strictEqual(st.get('vnsClip').inUse, true);
  assert.strictEqual(st.get('vnsClip').modules[0].detail.amp_ua, 2000);
  assert.strictEqual(st.get('intranasal').inUse, false, 'present but not driven');
  assert.strictEqual(st.get('goggles').inUse, false, 'the session says live, but in firmware mode only the firmware module says');
  assert.strictEqual(st.get('tmsCoil').inUse, false, 'no firmware driver: never in use in firmware mode');
  assert.strictEqual(st.get('tmsCoil').source, 'none');
});

test('firmware: a module of another kind with the same index is a different module', () => {
  const r = registryWith([
    { ev: 'module', id: 'pbm_base:0', kind: 'pbm_base', index: 0, present: true },
    { ev: 'module_state', id: 'pbm_base:0', present: true, driven: true },
    { ev: 'module', id: 'stim_pair:0', kind: 'stim_pair', index: 0, present: true },
    { ev: 'module_state', id: 'stim_pair:0', present: true, driven: false },
  ]);
  assert.strictEqual(r.size, 2, 'two modules at index 0');
  assert.strictEqual(r.bySocket(0).kind, 'pbm_base');
  assert.strictEqual(accessoryStates({ registry: r }).get('stimElectrodes').inUse, false, 'a driven tile does not make the stimulation pair driven');
});

test('firmware: the cervical VNS is in use while its session is active, before its enable is requested', () => {
  const r = registryWith([
    { ev: 'module', id: 'cvns:0', kind: 'cvns', index: 0, present: true },
    { ev: 'module_state', id: 'cvns:0', present: true, driven: false },
  ]);
  assert.strictEqual(accessoryStates({ registry: r }).get('cvns').inUse, false);
  r.onMessage({ type: 'FW_LOG', event: { ev: 'cvns_state', cmd: 'cvns_run', code: 0, stage: 2, active: true } }, 0);
  const st = accessoryStates({ registry: r }).get('cvns');
  assert.strictEqual(st.inUse, true, 'in the cardiac baseline stage');
  assert.strictEqual(st.modules[0].detail.stage, 2);
});

test('window slots lie around the perimeter and never over the middle', () => {
  for (const [vw, vh] of [[1000, 700], [1400, 900], [700, 600]]) {
    const slots = windowSlots(vw, vh);
    assert.ok(slots.length >= 4 && slots.length <= 6, `${slots.length} slots`);
    for (const s of slots) {
      assert.ok(s.x >= 0 && s.y >= 0 && s.x + s.w <= vw && s.y + s.h <= vh, `${s.slot} is inside ${vw}x${vh}`);
    }
    // the centre third of the viewport is clear of every corner slot (the helmet is there)
    const cx0 = vw / 3, cx1 = (2 * vw) / 3, cy0 = vh / 3, cy1 = (2 * vh) / 3;
    for (const s of slots) {
      const overlapsCentre = s.x < cx1 && s.x + s.w > cx0 && s.y < cy1 && s.y + s.h > cy0;
      assert.strictEqual(overlapsCentre, false, `${s.slot} stays out of the centre of ${vw}x${vh}`);
    }
    // no two windows overlap
    for (let i = 0; i < slots.length; i++) for (let j = i + 1; j < slots.length; j++) {
      const a = slots[i], b = slots[j];
      assert.ok(a.x + a.w <= b.x || b.x + b.w <= a.x || a.y + a.h <= b.y || b.y + b.h <= a.y, `${a.slot} and ${b.slot} do not overlap`);
    }
  }
});

test('a window keeps its slot: the rect for an accessory does not depend on which others are showing', () => {
  const a = windowRects(1200, 800);
  const b = windowRects(1200, 800);
  assert.deepStrictEqual([...a.entries()], [...b.entries()]);
  assert.deepStrictEqual([...a.keys()].sort(), WINDOW_ACCESSORIES.map((x) => x.id).sort());
  assert.strictEqual(new Set([...a.values()].map((r) => r.slot)).size, a.size, 'each window has its own slot');
  assert.ok(windowSlots(1200, 800)[0].w <= WINDOW_BASE.w);
});

test('firmware: the cervical VNS window goes when its session ends, reported by the result of the stop', () => {
  const r = registryWith([
    { ev: 'module', id: 'cvns:0', kind: 'cvns', index: 0, present: true },
    { ev: 'module_state', id: 'cvns:0', present: true, driven: false },
  ]);
  r.onMessage({ type: 'FW_LOG', event: { ev: 'cvns_state', cmd: 'cvns_run', stage: 4, active: true } }, 0);
  assert.strictEqual(accessoryStates({ registry: r }).get('cvns').inUse, true);
  r.onMessage({ type: 'FW_LOG', event: { ev: 'result', cmd: 'cvns_stop', code: 0, stage: 0, active: false } }, 1);
  assert.strictEqual(accessoryStates({ registry: r }).get('cvns').inUse, false, 'after the stop');
  assert.strictEqual(r.byKind('cvns')[0].detail.stage, 0);
});

test('a result that is not a cervical VNS stage is not folded into a module', () => {
  const r = registryWith([{ ev: 'module', id: 'vns:0', kind: 'vns', index: 0, present: true }]);
  r.onMessage({ type: 'FW_LOG', event: { ev: 'result', cmd: 'vns', code: 0, stimulating: true } }, 0);
  assert.strictEqual(r.size, 1);
  assert.deepStrictEqual(r.byKind('vns')[0].detail, { present: true });
});
