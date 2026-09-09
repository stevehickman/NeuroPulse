#!/usr/bin/env bun
/**
 * check-tcap-map.ts — the T2 cap electrode map is diffed, not read.
 *
 * NP-HW-TCAP-001 §3 (`REQ-TCAP-02`) specifies which T2 cap electrode reaches
 * which tACS driver channel. Until that document existed the authority was
 * `k_driver_channel[]` in Class B firmware — a C array standing in for a
 * hardware contract — and the map it replaced had aliased five pairs of
 * geometric NEIGHBOURS (Cz/Pz, C4/P4, T8/P8, P7/O1, P3/O2), which is what made
 * the M1_R ring undeliverable. Every one of those five reads as reasonable.
 *
 * That is the case NP-CONV-001 §8 legislates for: cross-document interface
 * agreement is established by mechanical diff, never by review, because a name
 * mismatch — or an off-by-one in a 21-row map — reads as agreement.
 *
 *   bun scripts/check-tcap-map.ts
 *   bun scripts/check-tcap-map.ts --self-test
 *
 * ── The four correspondences it checks ───────────────────────────────────────
 *
 *  A. ROW FOR ROW. Each §3 row's electrode name against `k_electrode_names[]`,
 *     its MNI triple against `k_electrode_mni[]`, and its driver channel against
 *     `k_driver_channel[]` — all in np_hd_montage.c. The MNI column is checked
 *     because `REQ-TCAP-03` is a statement about distances between those
 *     coordinates: a constraint whose inputs are unverified is not a constraint.
 *
 *  B. THE COUNT, AT THREE CORNERS. The §3 row count against BOTH
 *     `NP_HD_DRIVER_CHANNELS` (np_hd_config.h) and `NP_CLIN_TACS_CHANNELS`
 *     (np_hub_types.h). np_protocol_tests.c already asserts those two equal each
 *     other; this adds the third corner — that they equal what the specification
 *     says. Two of three agreeing is the failure worth catching, and it is the
 *     exact shape OI-TACS-01 was: the driver was 21 and the wire format was 16,
 *     each self-consistent.
 *
 *  C. `REQ-TCAP-03`, RE-DERIVED. No two electrodes sharing a driver channel may
 *     lie within one ring radius (60 mm) of each other. This is computed from
 *     the §3 coordinates rather than asserted from the map being an identity,
 *     so it still holds if sharing is ever reintroduced under cost pressure —
 *     which NP-HW-TCAP-001 §7 leaves open, since no per-channel price exists.
 *
 *  D. NO CHANNEL IS CLAIMED TWICE by two electrodes, and none is out of range.
 *
 * ── Direction of authority ───────────────────────────────────────────────────
 *
 * The document specifies and the firmware implements. On a mismatch the
 * FIRMWARE is wrong — that is the whole point of writing NP-HW-TCAP-001 — so
 * the failure message names the document as the expected value.
 *
 * CI-Kind: gate
 * CI-Self-Test: bun scripts/check-tcap-map.ts --self-test
 * CI-Scans: NP-HW-TCAP-001 §3's electrode map against the sLORETA and hub firmware tables
 * CI-Scan-Paths: docs/** firmware/** scripts/check-tcap-map.ts
 */

import { readFileSync, mkdtempSync, mkdirSync, writeFileSync } from "fs";
import { join, resolve } from "path";
import { tmpdir } from "os";

const rootFlag = process.argv.indexOf("--root");
const ROOT =
  rootFlag >= 0 && process.argv[rootFlag + 1]
    ? resolve(process.argv[rootFlag + 1]!)
    : join(import.meta.dir, "..");

const DOC = "docs/np_hw_tcap_001.md";
const MONTAGE = "firmware/sloreta_hdtdcs/src/np_hd_montage.c";
const HD_CONFIG = "firmware/sloreta_hdtdcs/include/np_hd_config.h";
const HUB_TYPES = "firmware/hub_control/include/np_hub_types.h";

/** One ring radius, NP-HW-TCAP-001 `REQ-TCAP-03`. */
const RING_RADIUS_MM = 60;

type Row = { ordinal: number; name: string; mni: [number, number, number]; channel: number };

/**
 * The document set writes a typographic minus (U+2212); C writes hyphen-minus.
 * Normalising here rather than constraining the prose keeps the rule about the
 * map instead of about punctuation.
 */
const num = (s: string): number => Number(s.trim().replace(/−/g, "-"));

function read(root: string, rel: string): string {
  return readFileSync(join(root, rel), "utf8");
}

// ── The specification side ───────────────────────────────────────────────────

/**
 * §3's table. Anchored on the header row rather than on a section heading, so
 * renumbering the document does not silently empty the population — an empty
 * scan that exits 0 is the #118 shape and is refused below.
 */
function parseDocRows(text: string): Row[] {
  const lines = text.split("\n");
  const head = lines.findIndex((l) => /^\|\s*Ordinal\s*\|\s*Electrode\s*\|/i.test(l));
  if (head < 0) return [];
  const rows: Row[] = [];
  for (let i = head + 2; i < lines.length; i++) {
    const line = lines[i]!;
    if (!line.startsWith("|")) break;
    const cells = line.split("|").slice(1, -1).map((c) => c.trim());
    if (cells.length < 4) break;
    const m = /^\(\s*(-?−?[\d−-]+)\s*,\s*([^,]+)\s*,\s*([^)]+)\)/.exec(cells[2]!);
    if (!m) break;
    rows.push({
      ordinal: num(cells[0]!),
      name: cells[1]!,
      mni: [num(m[1]!), num(m[2]!), num(m[3]!)],
      channel: num(cells[3]!),
    });
  }
  return rows;
}

// ── The firmware side ────────────────────────────────────────────────────────

/** Body of a `static const <type> <name>[...] = { ... };` initialiser. */
function arrayBody(src: string, name: string): string | null {
  const at = src.indexOf(`${name}[`);
  if (at < 0) return null;
  const open = src.indexOf("{", at);
  const close = src.indexOf("};", open);
  if (open < 0 || close < 0) return null;
  return src.slice(open + 1, close);
}

/** Comments carry the electrode labels, so they must go before values are read. */
const stripComments = (s: string): string => s.replace(/\/\*[\s\S]*?\*\//g, " ").replace(/\/\/[^\n]*/g, " ");

function parseChannels(src: string): number[] | null {
  const body = arrayBody(src, "k_driver_channel");
  if (body === null) return null;
  return stripComments(body)
    .split(",")
    .map((t) => t.trim())
    .filter((t) => t.length > 0)
    .map(Number);
}

function parseNames(src: string): string[] | null {
  const body = arrayBody(src, "k_electrode_names");
  if (body === null) return null;
  return [...stripComments(body).matchAll(/"([^"]*)"/g)].map((m) => m[1]!);
}

function parseMni(src: string): [number, number, number][] | null {
  const body = arrayBody(src, "k_electrode_mni");
  if (body === null) return null;
  return [...stripComments(body).matchAll(/\{\s*(-?\d+)\s*,\s*(-?\d+)\s*,\s*(-?\d+)\s*\}/g)].map(
    (m) => [Number(m[1]), Number(m[2]), Number(m[3])] as [number, number, number],
  );
}

function parseDefine(src: string, name: string): number | null {
  const m = new RegExp(`^#define\\s+${name}\\s+(\\d+)`, "m").exec(src);
  return m ? Number(m[1]) : null;
}

// ── The audit ────────────────────────────────────────────────────────────────

export function audit(root: string): { violations: string[]; scanned: number } {
  const v: string[] = [];

  let doc: string, montage: string, hdConfig: string, hubTypes: string;
  try {
    doc = read(root, DOC);
    montage = read(root, MONTAGE);
    hdConfig = read(root, HD_CONFIG);
    hubTypes = read(root, HUB_TYPES);
  } catch (e) {
    return { violations: [`cannot read a required file — ${(e as Error).message}`], scanned: 0 };
  }

  const rows = parseDocRows(doc);
  if (rows.length === 0) {
    return {
      violations: [`${DOC}: §3's map table did not parse — no rows found under its header`],
      scanned: 0,
    };
  }

  const channels = parseChannels(montage);
  const names = parseNames(montage);
  const mni = parseMni(montage);
  for (const [label, arr] of [
    ["k_driver_channel[]", channels],
    ["k_electrode_names[]", names],
    ["k_electrode_mni[]", mni],
  ] as const) {
    if (arr === null) v.push(`${MONTAGE}: ${label} did not parse`);
  }
  if (v.length) return { violations: v, scanned: rows.length };

  // B — the count, at three corners.
  for (const [file, macro] of [
    [HD_CONFIG, "NP_HD_DRIVER_CHANNELS"],
    [HUB_TYPES, "NP_CLIN_TACS_CHANNELS"],
  ] as const) {
    const got = parseDefine(file === HD_CONFIG ? hdConfig : hubTypes, macro);
    if (got === null) v.push(`${file}: ${macro} did not parse`);
    else if (got !== rows.length) {
      v.push(
        `${macro} is ${got}, but ${DOC} §3 specifies ${rows.length} channels — ` +
          `the specification is the authority (REQ-TCAP-02)`,
      );
    }
  }
  for (const [label, len] of [
    ["k_driver_channel[]", channels!.length],
    ["k_electrode_names[]", names!.length],
    ["k_electrode_mni[]", mni!.length],
  ] as const) {
    if (len !== rows.length) {
      v.push(`${MONTAGE}: ${label} has ${len} entries; ${DOC} §3 specifies ${rows.length}`);
    }
  }

  // A — row for row. Only over indices both sides have.
  const n = Math.min(rows.length, channels!.length, names!.length, mni!.length);
  for (let i = 0; i < n; i++) {
    const r = rows[i]!;
    if (r.ordinal !== i) {
      v.push(`${DOC} §3 row ${i}: ordinal is ${r.ordinal}, expected ${i} (REQ-TCAP-01)`);
    }
    if (names![i] !== r.name) {
      v.push(
        `electrode ${i}: ${DOC} §3 says "${r.name}", ${MONTAGE} k_electrode_names[] says ` +
          `"${names![i]}" — firmware must follow the specification`,
      );
    }
    if (channels![i] !== r.channel) {
      v.push(
        `electrode ${i} (${r.name}): ${DOC} §3 assigns driver channel ${r.channel}, ` +
          `${MONTAGE} k_driver_channel[] says ${channels![i]} — firmware must follow the specification`,
      );
    }
    const got = mni![i]!;
    if (got[0] !== r.mni[0] || got[1] !== r.mni[1] || got[2] !== r.mni[2]) {
      v.push(
        `electrode ${i} (${r.name}): ${DOC} §3 gives MNI (${r.mni.join(", ")}), ` +
          `${MONTAGE} k_electrode_mni[] gives (${got.join(", ")}) — REQ-TCAP-03 is derived from these`,
      );
    }
  }

  // D — no channel claimed twice, none out of range.
  const claim = new Map<number, number>();
  for (const r of rows) {
    if (r.channel < 0 || r.channel >= rows.length) {
      v.push(`electrode ${r.ordinal} (${r.name}): driver channel ${r.channel} is out of range`);
      continue;
    }
    const prior = claim.get(r.channel);
    if (prior !== undefined) {
      // C — sharing is not forbidden outright; aliasing NEIGHBOURS is.
      const a = rows[prior]!.mni;
      const b = r.mni;
      const d = Math.hypot(a[0] - b[0], a[1] - b[1], a[2] - b[2]);
      if (d < RING_RADIUS_MM) {
        v.push(
          `REQ-TCAP-03: ${rows[prior]!.name} and ${r.name} share driver channel ${r.channel} ` +
            `and are ${d.toFixed(1)} mm apart — inside one ring radius (${RING_RADIUS_MM} mm). ` +
            `A 4x1 ring draws cathodes from its anode's nearest electrodes, so this is a ` +
            `guaranteed collision, not a risked one`,
        );
      }
    } else {
      claim.set(r.channel, r.ordinal);
    }
  }

  return { violations: v, scanned: rows.length };
}

// ── Self-test ────────────────────────────────────────────────────────────────
// NP-CONV-001 §8: a probe that compares two tables must be falsified before it
// is trusted. Every fixture below perturbs exactly one thing, and the check must
// fail on each — otherwise it is a check that agrees with everything.
if (process.argv.includes("--self-test")) {
  const box = mkdtempSync(join(tmpdir(), "np-tcapmap-"));

  const NAMES = ["Fp1", "Fp2", "Cz", "Pz"];
  // Cz/Pz are 56.6 mm apart — inside one ring radius, which is what made the
  // retired 16-channel map's Cz/Pz alias a collision. That is the pair the
  // sharing fixture below uses, so REQ-TCAP-03 is exercised on a real case.
  const MNI: [number, number, number][] = [
    [-21, 66, 5],
    [21, 66, 5],
    [0, -10, 83],
    [0, -65, 75],
  ];

  const docFor = (rows: Row[]) =>
    "# fixture\n\n## 3. Map\n\n| Ordinal | Electrode | MNI (x, y, z) mm | Driver channel |\n" +
    "|---|---|---|---|\n" +
    rows
      .map(
        (r) =>
          `| ${r.ordinal} | ${r.name} | (${r.mni
            .map((x) => (x < 0 ? `−${-x}` : `${x}`))
            .join(", ")}) | ${r.channel} |`,
      )
      .join("\n") +
    "\n\ntail\n";

  const montageFor = (names: string[], mni: [number, number, number][], ch: number[]) =>
    `static const np_hd_mni_t k_electrode_mni[NP_HD_CH_COUNT] = {\n` +
    mni.map((m, i) => `    /* ${names[i] ?? "?"} */ { ${m[0]}, ${m[1]}, ${m[2]} },`).join("\n") +
    `\n};\n\nstatic const char *const k_electrode_names[NP_HD_CH_COUNT] = {\n    ` +
    names.map((s) => `"${s}"`).join(", ") +
    `,\n};\n\nstatic const uint8_t k_driver_channel[NP_HD_CH_COUNT] = {\n    ` +
    ch.map((c, i) => `/*${names[i] ?? "?"}*/ ${c}`).join(", ") +
    `,\n};\n`;

  const build = (opts: {
    rows?: Row[];
    names?: string[];
    mni?: [number, number, number][];
    ch?: number[];
    driverChannels?: number;
    clinChannels?: number;
  }): string => {
    const rows =
      opts.rows ??
      NAMES.map((name, i) => ({ ordinal: i, name, mni: MNI[i]!, channel: i }) as Row);
    const root = mkdtempSync(join(box, "t-"));
    mkdirSync(join(root, "docs"), { recursive: true });
    mkdirSync(join(root, "firmware/sloreta_hdtdcs/src"), { recursive: true });
    mkdirSync(join(root, "firmware/sloreta_hdtdcs/include"), { recursive: true });
    mkdirSync(join(root, "firmware/hub_control/include"), { recursive: true });
    writeFileSync(join(root, DOC), docFor(rows));
    writeFileSync(
      join(root, MONTAGE),
      montageFor(opts.names ?? NAMES, opts.mni ?? MNI, opts.ch ?? NAMES.map((_, i) => i)),
    );
    writeFileSync(
      join(root, HD_CONFIG),
      `#define NP_HD_DRIVER_CHANNELS       ${opts.driverChannels ?? NAMES.length}U\n`,
    );
    writeFileSync(
      join(root, HUB_TYPES),
      `#define NP_CLIN_TACS_CHANNELS       ${opts.clinChannels ?? NAMES.length}U\n`,
    );
    return root;
  };

  const failures: string[] = [];
  const expect = (label: string, root: string, needle: string | null) => {
    const { violations } = audit(root);
    if (needle === null) {
      if (violations.length) failures.push(`${label} — expected clean, got: ${violations[0]}`);
    } else if (!violations.some((x) => x.includes(needle))) {
      failures.push(
        `${label} — no violation matching ${JSON.stringify(needle)}; got ${
          violations.length ? JSON.stringify(violations[0]) : "nothing"
        }`,
      );
    }
  };

  // The vacuity case first: a fixture that agrees must pass, or every failure
  // below proves nothing.
  expect("an agreeing tree passes", build({}), null);

  // A — one perturbed channel, one perturbed name, one perturbed coordinate.
  expect("a changed driver channel is caught", build({ ch: [0, 1, 3, 2] }), "assigns driver channel 2");
  expect("a changed electrode name is caught", build({ names: ["Fp1", "Fp2", "CZ", "Pz"] }), "k_electrode_names[] says");
  expect(
    "a changed MNI coordinate is caught",
    build({ mni: [[-21, 66, 5], [21, 66, 5], [0, -10, 84], [0, -65, 75]] }),
    "k_electrode_mni[] gives",
  );

  // B — each count corner, independently. Two of three agreeing is the shape
  // OI-TACS-01 actually was.
  expect("a short firmware table is caught", build({ ch: [0, 1, 2], names: ["Fp1", "Fp2", "Cz"] }), "entries;");
  expect("a stale NP_HD_DRIVER_CHANNELS is caught", build({ driverChannels: 16 }), "NP_HD_DRIVER_CHANNELS is 16");
  expect("a stale NP_CLIN_TACS_CHANNELS is caught", build({ clinChannels: 16 }), "NP_CLIN_TACS_CHANNELS is 16");

  // C — sharing a channel between two electrodes 56.6 mm apart. Note the map is
  // internally consistent here: firmware agrees with the document exactly, and
  // the violation is the document's own map failing REQ-TCAP-03. A checker that
  // only diffed the two sides would call this tree clean.
  const shared: Row[] = NAMES.map((name, i) => ({
    ordinal: i,
    name,
    mni: MNI[i]!,
    channel: i === 3 ? 2 : i,
  }));
  expect(
    "aliasing two electrodes inside one ring radius is caught",
    build({ rows: shared, ch: [0, 1, 2, 2] }),
    "REQ-TCAP-03",
  );

  // D — an out-of-range channel.
  const oor: Row[] = NAMES.map((name, i) => ({ ordinal: i, name, mni: MNI[i]!, channel: i === 2 ? 9 : i }));
  expect("an out-of-range channel is caught", build({ rows: oor, ch: [0, 1, 9, 3] }), "is out of range");

  // The population guard itself: a document whose table cannot be found must
  // fail loudly rather than scan nothing and pass.
  const empty = build({});
  writeFileSync(join(empty, DOC), "# fixture\n\nno table here\n");
  expect("an unparseable map table is caught", empty, "did not parse");

  if (failures.length) {
    console.error("check-tcap-map self-test FAILED:");
    for (const f of failures) console.error(`  - ${f}`);
    process.exit(1);
  }
  console.log(`check-tcap-map self-test PASS (${10} fixtures)`);
  process.exit(0);
}

const { violations, scanned } = audit(ROOT);
console.log(
  `scanned: ${scanned} electrode row(s) of NP-HW-TCAP-001 §3 against ` +
    `k_driver_channel[], k_electrode_names[], k_electrode_mni[], ` +
    `NP_HD_DRIVER_CHANNELS and NP_CLIN_TACS_CHANNELS`,
);
if (violations.length) {
  console.error("\nNP-HW-TCAP-001 §3 and firmware disagree:");
  for (const x of violations) console.error(`  - ${x}`);
  console.error(
    "\nThe document is the authority (REQ-TCAP-02). If firmware is right, revise the document.",
  );
  process.exit(1);
}
console.log("PASS — the specified map, the firmware tables and both channel counts agree.");
