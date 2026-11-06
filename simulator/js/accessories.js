/**
 * Accessories — which are in use, and where each is drawn (NP-SIM-FW-001 §3.4, §21).
 *
 * The rule, from the Principal (2026-11-05):
 *   1. An accessory is drawn ONLY while it is in use.
 *   2. An accessory that attaches directly to the helmet is drawn on the helmet, as attached.
 *   3. Every other accessory gets its own window, along the perimeter of the helmet window.
 *
 * Which is which comes from the hardware specifications, not from where the 3D scene happened to put things:
 *   - mounts to the headset (drawn on the helmet): visual goggles (snap-on lens mounts), audio cups ("mounts to the headset arms",
 *     NP-HW-AUDIO-001 §3), the TMS coil (the helmet's coil site, NP-HW-TMS-001), the mastoid haptic pad, and the tDCS / tACS
 *     electrodes at their 10-20 sites (the electrode pair numbers name sites, np_mod_*_params_t).
 *   - tethered to the hub's accessory port, so not attached to the helmet (own window): the auricular VNS clip (worn on the ear,
 *     NP-HW-VNSCLIP-001), the intranasal Y-probe (NP-HW-NASAL-001 A12.7) and the cervical VNS pads (neck-worn, NP-HW-CVNS-001 A14.4).
 *
 * "In use" has one source at a time, never a mix (the SimModule rule: a thing is drawn by what the firmware says it is doing and by
 * nothing else): when firmware modules are present, only they say; otherwise the standalone session does. An accessory the firmware
 * has no driver for (the TMS coil and the haptic pad are stubs there) is therefore never in use in firmware mode.
 *
 * Pure functions, no DOM and no three.js, so node can test them.
 */

export const MOUNT = Object.freeze({ HELMET: 'helmet', WINDOW: 'window' });

/** NP_CVNS_STAGE_* (firmware/cervical_vns/include/np_cvns_types.h). */
export const CVNS_STAGE_NAMES = Object.freeze(['Idle', 'Impedance check', 'Cardiac baseline', 'Ramp-up', 'Delivering', 'Ramp-down', 'Complete', 'Fault']);

/**
 * `kind` is the firmware module kind that drives it (null: no firmware driver). `fromSession` reads the standalone session
 * snapshot; `demo` is the user's manual switch for the one accessory no shipped protocol drives (the haptic pad; see the panel).
 */
export const ACCESSORIES = Object.freeze([
  { id: 'goggles',        label: 'Visual goggles',          mount: MOUNT.HELMET, kind: 'visual',
    fromSession: (s) => !!s.visual?.active },
  { id: 'earcups',        label: 'Audio cups',              mount: MOUNT.HELMET, kind: 'audio',
    fromSession: (s) => !!s.audio?.active },
  { id: 'stimElectrodes', label: 'tDCS / tACS electrodes',  mount: MOUNT.HELMET, kind: 'stim_pair',
    fromSession: (s) => !!(s.bes?.active || s.tdcs?.active) },
  { id: 'tmsCoil',        label: 'TMS coil',                mount: MOUNT.HELMET, kind: null,
    fromSession: (s) => !!s.tms?.active },
  { id: 'hapticPad',      label: 'Mastoid haptic pad',      mount: MOUNT.HELMET, kind: null,
    fromSession: (s, demo) => !!(s.haptic?.active || demo?.hapticPad) },
  { id: 'vnsClip',        label: 'Auricular VNS clip',      mount: MOUNT.WINDOW, kind: 'vns',
    fromSession: (s) => !!s.vns?.active },
  { id: 'intranasal',     label: 'Intranasal Y-probe',      mount: MOUNT.WINDOW, kind: 'intranasal',
    fromSession: (s) => !!s.intranasal?.active },
  { id: 'cvns',           label: 'Cervical VNS pads',       mount: MOUNT.WINDOW, kind: 'cvns',
    fromSession: (s) => !!s.cvns?.active },
]);

export const HELMET_ACCESSORIES = Object.freeze(ACCESSORIES.filter((a) => a.mount === MOUNT.HELMET));
export const WINDOW_ACCESSORIES = Object.freeze(ACCESSORIES.filter((a) => a.mount === MOUNT.WINDOW));

/** In use, from the firmware's modules of one kind. The cervical VNS counts from its session being active, not only its enable. */
function firmwareInUse(kind, modules) {
  if (kind === 'cvns') return modules.some((m) => m.driven || m.detail?.active === true);
  return modules.some((m) => m.driven);
}

/**
 * @param {{ registry?: { size:number, byKind:(k:string)=>object[] }, snap?: object, demo?: Record<string, boolean> }} src
 * @returns {Map<string, { id:string, inUse:boolean, source:'firmware'|'session'|'none', modules:object[] }>}
 */
export function accessoryStates({ registry, snap, demo } = {}) {
  const firmware = !!registry && registry.size > 0;
  const out = new Map();
  for (const a of ACCESSORIES) {
    if (firmware) {
      const modules = a.kind === null ? [] : registry.byKind(a.kind).filter((m) => m.present);
      out.set(a.id, { id: a.id, inUse: a.kind !== null && firmwareInUse(a.kind, modules), source: a.kind === null ? 'none' : 'firmware', modules });
    } else {
      out.set(a.id, { id: a.id, inUse: !!a.fromSession(snap ?? {}, demo ?? {}), source: 'session', modules: [] });
    }
  }
  return out;
}

// ── where the windows go ──────────────────────────────────────────────────────

export const WINDOW_BASE = Object.freeze({ w: 244, h: 272 });
const MARGIN = 12;
const TOP_INSET = 44;      // below the view buttons
const BOTTOM_INSET = 30;   // above the hint line

/**
 * One rect per window slot, around the PERIMETER of a viewport of `vw` x `vh` and never over its middle, where the helmet is.
 * The order is the order windows are given out in: top-right, bottom-right, bottom-left, top-left, then the middle of the right
 * and left edges. A slot is fixed by its accessory, so a window appears where it was, not wherever is free. The windows shrink
 * (never below 55 %) when the viewport is small.
 * @returns {{x:number, y:number, w:number, h:number, slot:string}[]}
 */
export function windowSlots(vw, vh) {
  const k = Math.max(0.55, Math.min(1, vw / (WINDOW_BASE.w * 3 + MARGIN * 4), vh / (WINDOW_BASE.h * 2 + MARGIN * 4 + TOP_INSET + BOTTOM_INSET)));
  const w = Math.round(WINDOW_BASE.w * k), h = Math.round(WINDOW_BASE.h * k);
  const right = vw - w - MARGIN, left = MARGIN;
  const top = MARGIN + TOP_INSET, bottom = vh - h - MARGIN - BOTTOM_INSET;
  const midY = Math.round((vh - h) / 2);
  const all = [
    { x: right, y: top,    w, h, slot: 'top-right' },
    { x: right, y: bottom, w, h, slot: 'bottom-right' },
    { x: left,  y: bottom, w, h, slot: 'bottom-left' },
    { x: left,  y: top,    w, h, slot: 'top-left' },
    { x: right, y: midY,   w, h, slot: 'right' },
    { x: left,  y: midY,   w, h, slot: 'left' },
  ];
  // A slot is given out only if it does not overlap one already given: on a short viewport the edge-middle slots would sit on the corners.
  const apart = (a, b) => a.x + a.w <= b.x || b.x + b.w <= a.x || a.y + a.h <= b.y || b.y + b.h <= a.y;
  const chosen = [];
  for (const slot of all) if (chosen.every((c) => apart(c, slot))) chosen.push(slot);
  return chosen;
}

/** The rect for each window accessory, by id. */
export function windowRects(vw, vh) {
  const slots = windowSlots(vw, vh);
  const out = new Map();
  WINDOW_ACCESSORIES.forEach((a, i) => out.set(a.id, slots[i % slots.length]));
  return out;
}
