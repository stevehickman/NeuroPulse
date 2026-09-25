#!/usr/bin/env bun
/**
 * check-hub-wire-format.ts — the hub session descriptor is diffed, not read.
 *
 * NP-FW-HUB-001 §4 specifies the wire format that `app/web/src/lib/hubCompiler.ts`
 * writes and `firmware/hub_control/src/np_protocol.c` parses. Until this gate the
 * two implementations agreed only by inspection (OI-FWHUB-03) — the state that
 * made OI-DOC-01 expensive, and one NP-CONV-001 §8 forbids for a cross-artifact
 * interface: agreement is established by mechanical diff, never by review.
 *
 *   bun scripts/check-hub-wire-format.ts
 *   bun scripts/check-hub-wire-format.ts --self-test
 *
 * ── The three corners ────────────────────────────────────────────────────────
 *
 *  DOC       §4.1's constants, §4.3's target table, §4.5's current version and
 *            §4.6's per-modality parameter table.
 *  FIRMWARE  np_hub_config.h's defines, np_hub_types.h's enums, and the size
 *            and field offsets of every packed struct, computed from its
 *            declaration.
 *  COMPILER  what compileProtocol() WRITES. The compiler is run once per
 *            modality and its blob is decoded with the FIRMWARE's field
 *            offsets, so a header field written at the wrong offset, a
 *            parameter block of the wrong length or a mis-numbered slot is
 *            seen as the parser would see it. The compiler's mirrored
 *            constants (`const SLOT_TMS = 12;`) are also read statically,
 *            because a constant no fixture reaches is still a wire value.
 *
 * ── What it checks ───────────────────────────────────────────────────────────
 *
 *  A. CONSTANTS at all three corners: magic, version, UUID/serial/signature
 *     lengths, command and params maxima, the socket mask width, the slot
 *     range and NP_HUB_SLOT_NONE.
 *  B. LAYOUT: header and command-header sizes; each compiled blob's header
 *     fields and command headers decoded at the firmware's offsets; the body
 *     consumed exactly (§4.4's last row).
 *  C. PER MODALITY: §4.6's code, struct and byte count against the enum and
 *     the packed struct; the compiler's params_len and target against both.
 *  D. COVERAGE: every firmware mod type but NONE is a §4.6 row and is emitted
 *     by the compiler; no row names a code the firmware lacks.
 *
 * ── Direction of authority ───────────────────────────────────────────────────
 *
 * §4 says "this section and np_protocol.c are the same artifact seen twice, and
 * the compiler is wrong" (REQ-FWHUB-08). A doc/firmware disagreement names both;
 * a compiler disagreement names the compiler.
 *
 * CI-Kind: gate
 * CI-Self-Test: bun scripts/check-hub-wire-format.ts --self-test
 * CI-Scans: NP-FW-HUB-001 §4's wire format against np_hub_config.h, np_hub_types.h and hubCompiler.ts's output
 * CI-Scan-Paths: docs/np_fw_hub_001.md firmware/hub_control/include/** app/web/src/lib/** scripts/check-hub-wire-format.ts
 * CI-Self-Test-Reads-Tree: its fixtures are the real §4, headers and compiler with one perturbation each — the compiler is RUN, and a hand-written fixture compiler would only test itself
 */

import { readFileSync, mkdtempSync, mkdirSync, writeFileSync, copyFileSync } from "fs";
import { join, resolve, dirname } from "path";
import { tmpdir } from "os";

const rootFlag = process.argv.indexOf("--root");
const ROOT =
  rootFlag >= 0 && process.argv[rootFlag + 1]
    ? resolve(process.argv[rootFlag + 1]!)
    : join(import.meta.dir, "..");

const DOC = "docs/np_fw_hub_001.md";
const CONFIG = "firmware/hub_control/include/np_hub_config.h";
const TYPES = "firmware/hub_control/include/np_hub_types.h";
const COMPILER = "app/web/src/lib/hubCompiler.ts";
/** The compiler's runtime imports — the self-test copies these beside it. */
const COMPILER_DEPS = ["app/web/src/lib/socketMap.generated.ts", "app/web/src/lib/hardwareLimits.ts"];

const read = (root: string, rel: string): string => readFileSync(join(root, rel), "utf8");
const stripComments = (s: string): string =>
  s.replace(/\/\*[\s\S]*?\*\//g, " ").replace(/\/\/[^\n]*/g, " ");

// ── Firmware side ────────────────────────────────────────────────────────────

/** `#define NAME <int>` or `#define NAME OTHER_NAME`, resolved. */
function parseDefines(src: string): Map<string, number> {
  const raw = new Map<string, string>();
  for (const m of stripComments(src).matchAll(/^[ \t]*#define[ \t]+(\w+)[ \t]+([^\n]+)$/gm)) {
    raw.set(m[1]!, m[2]!.trim());
  }
  const out = new Map<string, number>();
  const resolveOne = (name: string, depth = 0): number | undefined => {
    if (out.has(name)) return out.get(name);
    const v = raw.get(name);
    if (v === undefined || depth > 8) return undefined;
    const lit = /^\(?\s*(0[xX][0-9a-fA-F]+|\d+)[uUlL]*\s*\)?$/.exec(v);
    let n: number | undefined;
    if (lit) n = Number(lit[1]);
    else if (/^\w+$/.test(v)) n = resolveOne(v, depth + 1);
    if (n !== undefined) out.set(name, n);
    return n;
  };
  for (const k of raw.keys()) resolveOne(k);
  return out;
}

/** Body of `typedef enum { ... } <name>;`, as name → value. */
function parseEnum(src: string, name: string): Map<string, number> | null {
  const re = new RegExp(`typedef\\s+enum\\s*\\{([^}]*)\\}\\s*${name}\\s*;`);
  const m = re.exec(stripComments(src));
  if (!m) return null;
  const out = new Map<string, number>();
  let next = 0;
  for (const part of m[1]!.split(",")) {
    const e = /^\s*(\w+)\s*(?:=\s*(0[xX][0-9a-fA-F]+|-?\d+)[uU]?)?\s*$/.exec(part);
    if (!e) continue;
    const v = e[2] !== undefined ? Number(e[2]) : next;
    out.set(e[1]!, v);
    next = v + 1;
  }
  return out;
}

type Field = { name: string; offset: number; size: number };
type Struct = { packed: boolean; fields: Field[]; size: number };

const TYPE_SIZE: Record<string, number> = {
  uint8_t: 1, int8_t: 1, char: 1, bool: 1,
  uint16_t: 2, int16_t: 2,
  uint32_t: 4, int32_t: 4, float: 4,
};

/**
 * `typedef struct [__attribute__((packed))] { ... } <name>;`. Only packed
 * structs are laid out — an unpacked one's offsets depend on the ABI, and a
 * wire struct that is not packed is itself the finding.
 */
function parseStruct(src: string, name: string, defs: Map<string, number>): Struct | string {
  const clean = stripComments(src);
  const re = new RegExp(`typedef\\s+struct\\s*(__attribute__\\s*\\(\\(\\s*packed\\s*\\)\\))?\\s*\\{([^}]*)\\}\\s*${name}\\s*;`);
  const m = re.exec(clean);
  if (!m) return `${name} not found`;
  const fields: Field[] = [];
  let off = 0;
  for (const decl of m[2]!.split(";")) {
    const d = decl.trim();
    if (!d) continue;
    const f = /^(\w+)\s+(\w+)\s*(?:\[\s*(\w+)\s*\])?$/.exec(d);
    if (!f) return `${name}: cannot read field "${d}"`;
    const base = TYPE_SIZE[f[1]!];
    if (base === undefined) return `${name}.${f[2]}: unknown type ${f[1]}`;
    let n = 1;
    if (f[3] !== undefined) {
      const k = /^\d+$/.test(f[3]) ? Number(f[3]) : defs.get(f[3]);
      if (k === undefined) return `${name}.${f[2]}: array bound ${f[3]} does not resolve`;
      n = k;
    }
    fields.push({ name: f[2]!, offset: off, size: base * n });
    off += base * n;
  }
  return { packed: m[1] !== undefined, fields, size: off };
}

// ── Specification side ───────────────────────────────────────────────────────

type DocRow = { code: number; modType: string; struct: string; bytes: number; slot: string | null };
type DocSpec = {
  magic?: number; version?: number; uuidLen?: number; serialLen?: number; sigLen?: number;
  cmdMax?: number; paramsMax?: number; targetMax?: number; maskBytes?: number;
  slotNone?: number; slotLo?: number; slotHi?: number; latestVersion?: number;
  rows: DocRow[];
};

function parseDoc(text: string): DocSpec {
  const start = text.search(/^## 4\. /m);
  const end = text.search(/^## 5\. /m);
  const s = start >= 0 ? text.slice(start, end > start ? end : undefined) : "";
  const num = (re: RegExp): number | undefined => {
    const m = re.exec(s);
    return m ? Number(m[1]) : undefined;
  };
  const spec: DocSpec = {
    magic: num(/`NP_HUB_PROTO_MAGIC`\s*=\s*`(0x[0-9A-Fa-f]+)`/),
    version: num(/`NP_HUB_PROTO_VERSION`\s*=\s*(\d+)/),
    uuidLen: num(/UUID\s+(\d+)\s*B/),
    serialLen: num(/serial\s+(\d+)\s*B/),
    sigLen: num(/signature\s+(\d+)\s*B/),
    cmdMax: num(/`NP_HUB_PROTO_CMD_MAX`\s*=\s*(\d+)/),
    paramsMax: num(/`NP_HUB_PROTO_PARAMS_MAX`\s*=\s*(\d+)/),
    targetMax: num(/`NP_HUB_PROTO_TARGET_MAX`\s*=\s*(\d+)/),
    maskBytes: num(/`NP_HUB_SOCKET_MASK_BYTES`\s*=\s*(\d+)/),
    slotNone: num(/`NP_HUB_SLOT_NONE`\s*\(`(0x[0-9A-Fa-f]+)`\)/),
    slotLo: num(/valid slot in `\[(\d+),\s*\d+\]`/),
    slotHi: num(/valid slot in `\[\d+,\s*(\d+)\]`/),
    rows: [],
  };
  // §4.5: the current version is the bold row.
  const v = /^\|\s*\*\*(\d+)\*\*\s*\|/m.exec(s);
  if (v) spec.latestVersion = Number(v[1]);
  // §4.6: anchored on the header row, not the heading.
  const lines = s.split("\n");
  const head = lines.findIndex((l) => /^\|\s*Code\s*\|\s*`mod_type`\s*\|/.test(l));
  if (head >= 0) {
    for (let i = head + 2; i < lines.length; i++) {
      const line = lines[i]!;
      if (!line.startsWith("|")) break;
      const c = line.split("|").slice(1, -1).map((x) => x.trim().replace(/`/g, ""));
      if (c.length < 5) break;
      const slot = /^(NP_HUB_SLOT_\w+)/.exec(c[4]!);
      spec.rows.push({
        code: Number(c[0]),
        modType: c[1]!,
        struct: c[2]!,
        bytes: Number(c[3]),
        slot: /socket mask/i.test(c[4]!) ? null : slot ? slot[1]! : "?",
      });
    }
  }
  return spec;
}

// ── Compiler side ────────────────────────────────────────────────────────────

/** Top-level `const NAME = <int>;` — the constants the compiler mirrors. */
function parseCompilerConsts(src: string): Map<string, number> {
  const out = new Map<string, number>();
  for (const m of stripComments(src).matchAll(/^const\s+(\w+)\s*=\s*(0[xX][0-9a-fA-F]+|\d+)\s*;/gm)) {
    out.set(m[1]!, Number(m[2]));
  }
  return out;
}

/**
 * One protocol per modality. Parameter values do not affect any length but
 * PBM's, whose wavelength selects base vs smart, so PBM appears twice.
 */
const FIXTURES: { label: string; type: string; params: object }[] = [
  { label: "pbm_transcranial (660/808)", type: "pbm_transcranial", params: { zones: "named", zoneRefs: ["All"], wavelength: "660_808nm", intensityPercent: 50, frequencyHz: 40, dutyCyclePercent: 25 } },
  { label: "pbm_transcranial (1064)", type: "pbm_transcranial", params: { zones: "named", zoneRefs: ["All"], wavelength: "1064nm", intensityPercent: 50, frequencyHz: 40, dutyCyclePercent: 25 } },
  { label: "pbm_intranasal", type: "pbm_intranasal", params: { intensityPercent: 60, frequencyHz: 10, dutyCyclePercent: 25 } },
  { label: "eeg_neurofeedback", type: "eeg_neurofeedback", params: { channels: "all", band: "alpha", closedLoopEnabled: false } },
  { label: "bes_tacs", type: "bes_tacs", params: { frequencyHz: 10, intensityMilliamps: 0.8, waveform: "sinusoidal" } },
  { label: "tdcs", type: "tdcs", params: { intensityMilliamps: 1.5, electrodePairs: [["F3", "F4"]], rampSeconds: 30, electrodeAreaCm2: 35 } },
  { label: "vns_hrv", type: "vns_hrv", params: { frequencyHz: 20, intensityMilliamps: 1, hrvProtocol: "standalone" } },
  { label: "audio_entrainment", type: "audio_entrainment", params: { carrierHz: 200, binauralBeatsHz: 10, volumePercent: 50, boneConductionPacer: false, eegAdaptive: false } },
  { label: "visual_stimulation", type: "visual_stimulation", params: { mode: "binocular", frequencyHz: 10, emdrCadenceHz: 1, enableModeF: false } },
  { label: "qeeg_21ch", type: "qeeg_21ch", params: { montage: "standard_1020", reference: "linked_ear", sloretaEnabled: true } },
  { label: "tms", type: "tms", params: { tmsProtocol: "rTMS", target: "DLPFC_L", frequencyHz: 10, intensityPercentMT: 110, pulseCount: 3000 } },
  { label: "pbm_deep_1170nm", type: "pbm_deep_1170nm", params: { intensityMWcm2: 500, frequencyHz: 40, dutyCyclePercent: 25 } },
  { label: "clinical_tacs", type: "clinical_tacs", params: { frequencyHz: 10, intensityMilliamps: 2, waveform: "sinusoidal", channelCount: 21 } },
  { label: "hd_tdcs", type: "hd_tdcs", params: { target: "DLPFC_L", montage: "ring_4x1", intensityMilliamps: 2 } },
  { label: "cervical_vns", type: "cervical_vns", params: { frequencyHz: 25, intensityMilliamps: 1.5 } },
  { label: "vibrotactile_40hz", type: "vibrotactile_40hz", params: { intensityG: 0.9, syncToAudio: true, syncToVisual: false } },
];

const DURATION_S = 60;

function protocolFor(type: string, params: object): object {
  return {
    id: "wire-check", name: "wire-check", description: "", author: "check", version: "1",
    tags: [], createdAt: "2026-01-01", modifiedAt: "2026-01-01", isPredefined: false,
    timingMode: { type: "duration", seconds: DURATION_S },
    modalities: [{
      id: "m1", enabled: true,
      interval: { intervalOnSeconds: 0, intervalOffSeconds: 0 },
      modalityParams: { type, params },
    }],
  };
}

type Compiled = { label: string; blob: Uint8Array } | { label: string; error: string };

async function runCompiler(root: string): Promise<Compiled[] | string> {
  let mod: { compileProtocol: (p: object, o: object) => { blob: Uint8Array } };
  try {
    mod = await import(join(root, COMPILER));
  } catch (e) {
    return `${COMPILER} did not load — ${(e as Error).message}`;
  }
  // One zone naming sockets 1..3 is enough: the bitmap is fixed-width.
  const zones = new Map([["All", { name: "All", sockets: [1, 2, 3] }]]);
  return FIXTURES.map((f) => {
    try {
      return { label: f.label, blob: mod.compileProtocol(protocolFor(f.type, f.params), { zones }).blob };
    } catch (e) {
      return { label: f.label, error: (e as Error).message };
    }
  });
}

// ── The audit ────────────────────────────────────────────────────────────────

function readField(dv: DataView, base: number, f: Field): number {
  if (f.size === 1) return dv.getUint8(base + f.offset);
  if (f.size === 2) return dv.getUint16(base + f.offset, true);
  return dv.getUint32(base + f.offset, true);
}

export async function audit(root: string): Promise<{ violations: string[]; scanned: number }> {
  const v: string[] = [];
  let doc: string, config: string, types: string, compilerSrc: string;
  try {
    doc = read(root, DOC);
    config = read(root, CONFIG);
    types = read(root, TYPES);
    compilerSrc = read(root, COMPILER);
  } catch (e) {
    return { violations: [`cannot read a required file — ${(e as Error).message}`], scanned: 0 };
  }

  const spec = parseDoc(doc);
  if (spec.rows.length === 0) {
    return { violations: [`${DOC}: §4.6's parameter table did not parse — no rows found under its header`], scanned: 0 };
  }
  const defs = parseDefines(config);
  for (const [k, val] of parseDefines(types)) defs.set(k, val);
  const modEnum = parseEnum(types, "np_hub_mod_type_t");
  const kindEnum = parseEnum(types, "np_proto_target_kind_t");
  if (!modEnum) v.push(`${TYPES}: np_hub_mod_type_t did not parse`);
  if (!kindEnum) v.push(`${TYPES}: np_proto_target_kind_t did not parse`);
  const hdr = parseStruct(types, "np_proto_header_t", defs);
  const cmd = parseStruct(types, "np_proto_cmd_hdr_t", defs);
  for (const [n, s] of [["np_proto_header_t", hdr], ["np_proto_cmd_hdr_t", cmd]] as const) {
    if (typeof s === "string") v.push(`${TYPES}: ${s}`);
    else if (!s.packed) v.push(`${TYPES}: ${n} is not packed — its wire offsets would depend on the ABI`);
  }
  if (v.length) return { violations: v, scanned: spec.rows.length };
  const H = hdr as Struct;
  const C = cmd as Struct;
  const hf = (n: string) => H.fields.find((f) => f.name === n);
  const cf = (n: string) => C.fields.find((f) => f.name === n);
  for (const n of ["magic", "version", "cmd_count", "session_duration_ms"]) {
    if (!hf(n)) v.push(`${TYPES}: np_proto_header_t has no field ${n}`);
  }
  for (const n of ["mod_type", "slot_id", "start_ms", "duration_ms", "params_len", "target_kind", "target_len"]) {
    if (!cf(n)) v.push(`${TYPES}: np_proto_cmd_hdr_t has no field ${n}`);
  }
  if (v.length) return { violations: v, scanned: spec.rows.length };

  const cc = parseCompilerConsts(compilerSrc);

  // A — constants, three corners. [doc, firmware define, compiler const].
  const fw = (n: string) => defs.get(n);
  const triples: [string, number | undefined, number | undefined, string | null][] = [
    ["magic", spec.magic, fw("NP_HUB_PROTO_MAGIC"), "PROTO_MAGIC"],
    ["version (§4.1)", spec.version, fw("NP_HUB_PROTO_VERSION"), "PROTO_VERSION"],
    ["version (§4.5 current row)", spec.latestVersion, fw("NP_HUB_PROTO_VERSION"), null],
    ["UUID length", spec.uuidLen, fw("NP_HUB_PROTO_UUID_LEN"), "PROTO_UUID_LEN"],
    ["serial length", spec.serialLen, fw("NP_HUB_PROTO_SERIAL_LEN"), "PROTO_SERIAL_LEN"],
    ["signature length", spec.sigLen, fw("NP_HUB_PROTO_SIG_LEN"), "PROTO_SIG_LEN"],
    ["command maximum", spec.cmdMax, fw("NP_HUB_PROTO_CMD_MAX"), "PROTO_CMD_MAX"],
    ["params maximum", spec.paramsMax, fw("NP_HUB_PROTO_PARAMS_MAX"), null],
    ["target block maximum", spec.targetMax, fw("NP_HUB_PROTO_TARGET_MAX"), null],
    ["socket mask bytes", spec.maskBytes, fw("NP_HUB_SOCKET_MASK_BYTES"), "SOCKET_MASK_BYTES"],
    ["NP_HUB_SLOT_NONE", spec.slotNone, fw("NP_HUB_SLOT_NONE"), "SLOT_NONE"],
    ["first valid slot", spec.slotLo, fw("NP_HUB_SLOT_FIRST_VALID"), "SLOT_FIRST_VALID"],
    ["last valid slot", spec.slotHi, (fw("NP_HUB_SLOT_MAX") ?? NaN) - 1, null],
    ["header length", undefined, H.size, "HEADER_LEN"],
    ["command header length", undefined, C.size, "CMD_HDR_LEN"],
  ];
  for (const [label, d, f, cname] of triples) {
    if (f === undefined || Number.isNaN(f)) { v.push(`firmware: ${label} did not parse`); continue; }
    if (d !== undefined && d !== f) {
      v.push(`${label}: ${DOC} §4 says ${d}, firmware says ${f} — §4 and np_protocol.c must be the same artifact`);
    }
    if (cname !== null) {
      const c = cc.get(cname);
      if (c === undefined) v.push(`${COMPILER}: const ${cname} (${label}) not found`);
      else if (c !== f) v.push(`${label}: ${COMPILER} ${cname} = ${c}, firmware says ${f} — the compiler is wrong (REQ-FWHUB-08)`);
    }
  }
  if (spec.magic === undefined) v.push(`${DOC} §4.1: magic did not parse`);
  if (spec.version === undefined) v.push(`${DOC} §4.1: version did not parse`);
  if (spec.latestVersion === undefined) v.push(`${DOC} §4.5: no bold current-version row`);
  if (spec.slotLo === undefined || spec.slotHi === undefined) v.push(`${DOC} §4.3: slot range did not parse`);

  // Compiler's mirrored slot and mod-type constants, statically.
  for (const [name, val] of cc) {
    if (/^SLOT_/.test(name) && !["SLOT_NONE", "SLOT_FIRST_VALID", "SLOT_MAX"].includes(name)) {
      const f = fw(`NP_HUB_${name}`);
      if (f === undefined) v.push(`${COMPILER}: ${name} has no firmware NP_HUB_${name}`);
      else if (f !== val) v.push(`${COMPILER}: ${name} = ${val}, firmware NP_HUB_${name} = ${f}`);
    }
    if (name === "SLOT_MAX" && val !== fw("NP_HUB_SLOT_MAX")) {
      v.push(`${COMPILER}: SLOT_MAX = ${val}, firmware NP_HUB_SLOT_MAX = ${fw("NP_HUB_SLOT_MAX")}`);
    }
    if (/^NP_MOD_/.test(name)) {
      const f = modEnum!.get(name);
      if (f === undefined) v.push(`${COMPILER}: ${name} is not a firmware np_hub_mod_type_t value`);
      else if (f !== val) v.push(`${COMPILER}: ${name} = ${val}, firmware says ${f}`);
    }
  }
  for (const [name, fv] of [["TARGET_SLOT", "NP_PROTO_TARGET_SLOT"], ["TARGET_SOCKET_MASK", "NP_PROTO_TARGET_SOCKET_MASK"]] as const) {
    const c = cc.get(name);
    const f = kindEnum!.get(fv);
    if (c === undefined || f === undefined) v.push(`target kind ${fv}: missing on ${c === undefined ? "compiler" : "firmware"} side`);
    else if (c !== f) v.push(`${COMPILER}: ${name} = ${c}, firmware ${fv} = ${f}`);
  }

  // C — §4.6 against firmware.
  const byCode = new Map<number, DocRow>();
  for (const r of spec.rows) {
    const e = modEnum!.get(r.modType);
    if (e === undefined) v.push(`${DOC} §4.6: ${r.modType} is not a firmware np_hub_mod_type_t value`);
    else if (e !== r.code) v.push(`${DOC} §4.6: ${r.modType} is code ${r.code}, firmware says ${e}`);
    if (byCode.has(r.code)) v.push(`${DOC} §4.6: code ${r.code} appears twice`);
    byCode.set(r.code, r);
    const st = parseStruct(types, r.struct, defs);
    if (typeof st === "string") v.push(`${TYPES}: ${st} (named by §4.6 for ${r.modType})`);
    else {
      if (!st.packed) v.push(`${TYPES}: ${r.struct} is not packed — its length depends on the ABI`);
      if (st.size !== r.bytes) {
        v.push(`${r.modType}: ${DOC} §4.6 says ${r.bytes} bytes, ${r.struct} is ${st.size} — REQ-FWHUB-10: a length change bumps the version`);
      }
      const pmax = fw("NP_HUB_PROTO_PARAMS_MAX");
      if (pmax !== undefined && st.size > pmax) v.push(`${r.struct} (${st.size} B) exceeds NP_HUB_PROTO_PARAMS_MAX (${pmax})`);
    }
    if (r.slot === "?") v.push(`${DOC} §4.6: ${r.modType}'s target is neither "socket mask" nor an NP_HUB_SLOT_* name`);
    else if (r.slot !== null) {
      const s = fw(r.slot);
      if (s === undefined) v.push(`${DOC} §4.6: ${r.modType} targets ${r.slot}, which firmware does not define`);
      else if (s < (fw("NP_HUB_SLOT_FIRST_VALID") ?? 0) || s >= (fw("NP_HUB_SLOT_MAX") ?? 0)) {
        v.push(`${DOC} §4.6: ${r.modType} targets ${r.slot} = ${s}, outside the valid slot range`);
      }
    }
  }
  // D — coverage, firmware → doc.
  for (const [name, code] of modEnum!) {
    if (name === "NP_MOD_NONE" || name === "NP_MOD_TYPE_COUNT") continue;
    if (!byCode.has(code)) v.push(`${name} (${code}) has no ${DOC} §4.6 row`);
  }

  // B + C — the compiler's actual output, decoded at firmware offsets.
  const compiled = await runCompiler(root);
  if (typeof compiled === "string") {
    v.push(compiled);
    return { violations: v, scanned: spec.rows.length };
  }
  const emitted = new Set<number>();
  const sigLen = fw("NP_HUB_PROTO_SIG_LEN") ?? 64;
  for (const c of compiled) {
    if ("error" in c) { v.push(`${COMPILER}: ${c.label} did not compile — ${c.error}`); continue; }
    const blob = c.blob;
    const dv = new DataView(blob.buffer, blob.byteOffset, blob.byteLength);
    const at = `${c.label}:`;
    if (blob.length < H.size + sigLen) { v.push(`${at} blob of ${blob.length} B is shorter than header + signature`); continue; }
    const magic = readField(dv, 0, hf("magic")!);
    const version = readField(dv, 0, hf("version")!);
    const count = readField(dv, 0, hf("cmd_count")!);
    const dur = readField(dv, 0, hf("session_duration_ms")!);
    if (magic !== fw("NP_HUB_PROTO_MAGIC")) v.push(`${at} header magic reads 0x${magic.toString(16)} at the firmware's offset`);
    if (version !== fw("NP_HUB_PROTO_VERSION")) v.push(`${at} header version reads ${version}, firmware expects ${fw("NP_HUB_PROTO_VERSION")}`);
    if (dur !== DURATION_S * 1000) v.push(`${at} session_duration_ms reads ${dur} at the firmware's offset, expected ${DURATION_S * 1000}`);
    if (count < 1) { v.push(`${at} cmd_count reads ${count}`); continue; }
    let p = H.size;
    const end = blob.length - sigLen;
    for (let i = 0; i < count; i++) {
      if (p + C.size > end) { v.push(`${at} command ${i} header overruns the body`); break; }
      const modType = readField(dv, p, cf("mod_type")!);
      const slotId = readField(dv, p, cf("slot_id")!);
      const plen = readField(dv, p, cf("params_len")!);
      const kind = readField(dv, p, cf("target_kind")!);
      const tlen = readField(dv, p, cf("target_len")!);
      p += C.size + tlen + plen;
      if (plen === 0) continue;   // a stop command carries no params
      emitted.add(modType);
      const row = byCode.get(modType);
      if (!row) { v.push(`${at} emits mod_type ${modType}, which §4.6 has no row for`); continue; }
      if (plen !== row.bytes) {
        v.push(`${at} ${row.modType} params_len is ${plen}; ${DOC} §4.6 and ${row.struct} say ${row.bytes} — the compiler is wrong (REQ-FWHUB-08)`);
      }
      if (row.slot === null) {
        if (kind !== kindEnum!.get("NP_PROTO_TARGET_SOCKET_MASK")) v.push(`${at} ${row.modType} target_kind is ${kind}; §4.6 says socket mask`);
        if (tlen !== fw("NP_HUB_SOCKET_MASK_BYTES")) v.push(`${at} ${row.modType} target_len is ${tlen}; a socket mask is ${fw("NP_HUB_SOCKET_MASK_BYTES")}`);
        if (slotId !== fw("NP_HUB_SLOT_NONE")) v.push(`${at} ${row.modType} slot_id is ${slotId} on a socket target; §4.4 requires NP_HUB_SLOT_NONE`);
      } else {
        if (kind !== kindEnum!.get("NP_PROTO_TARGET_SLOT")) v.push(`${at} ${row.modType} target_kind is ${kind}; §4.6 says slot`);
        if (tlen !== 0) v.push(`${at} ${row.modType} target_len is ${tlen} on a slot target; §4.3 says 0`);
        if (slotId !== fw(row.slot)) v.push(`${at} ${row.modType} slot_id is ${slotId}; §4.6 says ${row.slot} = ${fw(row.slot)}`);
      }
    }
    if (p !== end) v.push(`${at} the command body is ${end - H.size} B but its ${count} command(s) consume ${p - H.size} — §4.4 refuses a body not exactly consumed`);
  }
  for (const r of spec.rows) {
    if (!emitted.has(r.code)) v.push(`${r.modType}: no compiled fixture emitted it — the compiler has no encoder for a §4.6 modality, or this check's fixtures are short`);
  }

  return { violations: v, scanned: spec.rows.length };
}

// ── Self-test ────────────────────────────────────────────────────────────────
// NP-CONV-001 §8: falsified before it is trusted. Each fixture copies the real
// tree and perturbs exactly one thing at one corner; the check must fail on
// each, and must pass on the unperturbed copy.
if (process.argv.includes("--self-test")) {
  const box = mkdtempSync(join(tmpdir(), "np-wirefmt-"));
  const FILES = [DOC, CONFIG, TYPES, COMPILER, ...COMPILER_DEPS];
  let n = 0;
  const build = (edit?: { file: string; from: string | RegExp; to: string }): string => {
    const root = join(box, `t${n++}`);
    for (const f of FILES) {
      mkdirSync(dirname(join(root, f)), { recursive: true });
      copyFileSync(join(ROOT, f), join(root, f));
    }
    if (edit) {
      const p = join(root, edit.file);
      const before = readFileSync(p, "utf8");
      const after = before.replace(edit.from, edit.to);
      if (after === before) throw new Error(`self-test fixture did not apply: ${edit.from} in ${edit.file}`);
      writeFileSync(p, after);
    }
    return root;
  };

  const failures: string[] = [];
  const expect = async (label: string, root: string, needle: string | null) => {
    const { violations } = await audit(root);
    if (needle === null) {
      if (violations.length) failures.push(`${label} — expected clean, got: ${violations[0]}`);
    } else if (!violations.some((x) => x.includes(needle))) {
      failures.push(`${label} — no violation matching ${JSON.stringify(needle)}; got ${violations.length ? JSON.stringify(violations[0]) : "nothing"}`);
    }
  };

  const cases: [string, Parameters<typeof build>[0], string | null][] = [
    // Vacuity first: an agreeing tree must pass, or every failure proves nothing.
    ["the real tree, copied, passes", undefined, null],
    // DOC corner.
    ["doc: §4.1 version changed", { file: DOC, from: "`NP_HUB_PROTO_VERSION` = 3", to: "`NP_HUB_PROTO_VERSION` = 4" }, "version (§4.1)"],
    ["doc: §4.6 byte count changed", { file: DOC, from: "| `np_mod_tdcs_params_t` | 8 |", to: "| `np_mod_tdcs_params_t` | 6 |" }, "§4.6 says 6 bytes"],
    ["doc: §4.6 row dropped", { file: DOC, from: /\n\| `0x10` \| `NP_MOD_VIBROTACTILE`[^\n]*/, to: "" }, "NP_MOD_VIBROTACTILE (16) has no"],
    ["doc: §4.6 slot changed", { file: DOC, from: "`NP_HUB_SLOT_TMS` (12)", to: "`NP_HUB_SLOT_PBM_1170NM` (13)" }, "slot_id is 12"],
    ["doc: §4.6 table unparseable", { file: DOC, from: "| Code | `mod_type` |", to: "| Kode | `mod_type` |" }, "did not parse"],
    // FIRMWARE corner.
    ["firmware: magic changed", { file: CONFIG, from: "0x4E504850UL", to: "0x4E504851UL" }, "magic"],
    ["firmware: a params struct grew", { file: TYPES, from: "    uint16_t electrode_area_mcm2;\n} np_mod_tdcs_params_t;", to: "    uint16_t electrode_area_mcm2;\n    uint8_t  spare;\n} np_mod_tdcs_params_t;" }, "np_mod_tdcs_params_t is 9"],
    ["firmware: a mod type renumbered", { file: TYPES, from: "NP_MOD_TMS          = 0x0C", to: "NP_MOD_TMS          = 0x1C" }, "NP_MOD_TMS"],
    ["firmware: header fields reordered", { file: TYPES, from: /(uint8_t\s+flags;[^\n]*\n)(\s+uint8_t\s+cmd_count;[^\n]*\n)/, to: "$2$1" }, "cmd_count reads"],
    ["firmware: a params struct unpacked", { file: TYPES, from: /typedef struct __attribute__\(\(packed\)\) \{(\s+uint8_t  gain_reg)/, to: "typedef struct {$1" }, "is not packed"],
    // COMPILER corner.
    ["compiler: tDCS block one byte long", { file: COMPILER, from: "const buf = new Uint8Array(8);\n  const dv  = new DataView(buf.buffer);\n  dv.setUint8(0, resolveElectrodePair", to: "const buf = new Uint8Array(9);\n  const dv  = new DataView(buf.buffer);\n  dv.setUint8(0, resolveElectrodePair" }, "tdcs: NP_MOD_TDCS params_len is 9"],
    ["compiler: version constant stale", { file: COMPILER, from: "const PROTO_VERSION = 0x0003;", to: "const PROTO_VERSION = 0x0002;" }, "PROTO_VERSION = 2"],
    ["compiler: a slot constant wrong", { file: COMPILER, from: "const SLOT_TMS          = 12;", to: "const SLOT_TMS          = 13;" }, "SLOT_TMS = 13"],
    ["compiler: target kinds swapped", { file: COMPILER, from: "const TARGET_SOCKET_MASK = 0x01;", to: "const TARGET_SOCKET_MASK = 0x02;" }, "TARGET_SOCKET_MASK = 2"],
    ["compiler: header field at the wrong offset", { file: COMPILER, from: "dv.setUint16(4, PROTO_VERSION, true);", to: "dv.setUint16(5, PROTO_VERSION, true);" }, "header version reads"],
  ];
  for (const [label, edit, needle] of cases) await expect(label, build(edit), needle);

  if (failures.length) {
    console.error("check-hub-wire-format self-test FAILED:");
    for (const f of failures) console.error(`  - ${f}`);
    process.exit(1);
  }
  console.log(`check-hub-wire-format self-test PASS (${cases.length} fixtures)`);
  process.exit(0);
}

const { violations, scanned } = await audit(ROOT);
console.log(
  `scanned: ${scanned} §4.6 modality row(s), §4.1/§4.3/§4.5 constants, np_hub_config.h, np_hub_types.h, ` +
    `and ${FIXTURES.length} compiled protocols decoded at firmware offsets`,
);
if (violations.length) {
  console.error("\nNP-FW-HUB-001 §4, the firmware and hubCompiler.ts disagree:");
  for (const x of violations) console.error(`  - ${x}`);
  console.error("\n§4 and np_protocol.c are one artifact; where the compiler differs, it is wrong (REQ-FWHUB-08).");
  process.exit(1);
}
console.log("PASS — §4, the firmware wire structs and the compiler's output agree.");
