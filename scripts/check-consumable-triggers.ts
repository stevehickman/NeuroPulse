#!/usr/bin/env bun
/**
 * check-consumable-triggers.ts — every consumable replacement prompt is measurement-triggered,
 * and says which measurement (CLAUDE.md §2.3, OI-ACC-06).
 *
 * CLAUDE.md §2.3 (Rev 48) admits exactly two trigger kinds for a consumable replacement prompt:
 * a CONDITION MEASUREMENT of the part, or an EXPOSURE COUNT of the quantity that degrades it with
 * that mechanism named. A threshold back-derived from a calendar interval is neither. Until this
 * gate the rule was enforced by reading, and reading is what let the audio cup foam row sit on a
 * calendar interval from the table's first revision until a ranking exercise happened to read it
 * (OI-ACC-02), and let the mesh frame row name a measurement that was an unimplemented HAL stub
 * (OI-ACC-05) — a row that NAMES a measurement reads as compliant, which is why this gate asks
 * where the measurement is produced rather than whether one is named.
 *
 * ── What this checks ─────────────────────────────────────────────────────────
 *
 * The §2.3 table in docs/reference/commercial-model.md carries a Trigger column. Each cell opens
 * with one of three bold kinds and then `·`-separated `key: value` fields:
 *
 *   **Condition measurement** · producer: `X` · …
 *   **Exposure count** · producer: `X` · threshold: N — provenance · mechanism: … · cannot see: …
 *   **No prompt** · <why no prompt ships for this row>
 *
 *   T1  Every row declares one of the three kinds.
 *   T2  A prompting row (condition / exposure) names a producer, and the producer EXISTS in the
 *       app on both platforms. `ConsumableKind.<case>` resolves to the enum case on iOS and
 *       Android plus the CONSUMABLE_STATUS characteristic that feeds it; anything else must be
 *       registered in PRODUCERS below with where it lives.
 *   T3  A prompting row's Interval cell is not calendar-denominated.
 *   T4  An exposure count names its mechanism and what its count cannot see (both required by
 *       §2.3's own text), and its threshold states a provenance: either "unvalidated placeholder"
 *       with the open item carrying it, or a backticked reference it is derived from. When the
 *       producer is a ConsumableKind, the threshold equals `sessionLimit` on BOTH platforms.
 *   T5  A condition measurement is not produced by a ConsumableKind. Every ConsumableKind is a
 *       session count, so that pairing is a count wearing a measurement's name — the shape the
 *       hydrogel tip row had before this gate (OI-ACC-09).
 *   T6  A No-prompt row names no producer, and gives a reason.
 *   T7  Every ConsumableKind case is claimed by exactly one row, and the two platforms define the
 *       same cases. A fifth kind added in code with no §2.3 row fails here.
 *   T8  The reminder engine carries no calendar-shaped trigger: no calendar/repeating notification,
 *       no AlarmManager or periodic work. The device has no VBAT rail, so a calendar prompt is
 *       unimplementable on the headset; this stops one being built in the phone instead.
 *
 * ── Declared absences, which must break when they stop being true ────────────
 *
 * Two kinds of entry below record something that is NOT the case yet, the pattern
 * check-consent-reachability.ts uses for `pending` and check-locale-strings.ts for PENDING_PATHS.
 * Both are printed on every run, never silently tolerated, and both FAIL once the absence ends:
 *
 *   EXEMPTIONS        a row excused from one rule, with the open item. Fails when the row would
 *                     pass that rule without it — an exemption must not outlive its gap.
 *   FIRMWARE_PENDING  a producer the app consumes and the hub does not publish. Fails when live
 *                     firmware code references it, because then the declaration is a lie about
 *                     the code and the row should say where the hub produces it.
 *
 * And its mirror, a declared presence:
 *
 *   FIRMWARE_PUBLISHED  a producer that left FIRMWARE_PENDING because the hub now publishes it.
 *                     Fails when no live firmware code references it any more — the §2.3 row
 *                     says where the hub produces it, and that must stay true.
 *
 * ── The reach, stated narrowly ───────────────────────────────────────────────
 *
 * This checks that each trigger is DECLARED in an admissible shape and that its producer EXISTS.
 * It does not check that a stated provenance is true (a cited derivation could be wrong), that a
 * mechanism is the right one, or that a producer works — the unit tests and the hardware do that.
 * FIRMWARE_PENDING detection is a text match on the characteristic's name and UUID; a hub producer
 * spelled neither way would not flip it. Price, GM% and pack size are outside it entirely.
 *
 * CI-Kind: gate
 * CI-Self-Test: bun scripts/check-consumable-triggers.ts --self-test
 * CI-Scans: every row of the commercial-model.md §2.3 consumables table, against the ConsumableKind enums, reminder engines and GATT producers on iOS and Android, and firmware
 * CI-Scan-Paths: docs/reference/commercial-model.md app/ios/** app/android/** firmware/**
 */
import { readFileSync, readdirSync, statSync, mkdtempSync, mkdirSync, writeFileSync, rmSync } from "fs";
import { join } from "path";
import { tmpdir } from "os";

type Platform = "ios" | "android";
const PLATFORMS: Platform[] = ["ios", "android"];

const DOC = "docs/reference/commercial-model.md";
const SECTION = "### 2.3 Consumables";

const KIND_FILE: Record<Platform, string> = {
  ios: "app/ios/NeurOne/Models/ConsumableInventory.swift",
  android: "app/android/core/src/main/kotlin/life/neurone/core/consumable/ConsumableModels.kt",
};

/** Where the characteristic feeding every ConsumableKind is declared. */
const CONSUMABLE_STATUS: Record<Platform, { file: string; re: RegExp }> = {
  ios: { file: "app/ios/NeurOne/BLE/GATTCharacteristics.swift", re: /static\s+let\s+consumableStatus\b/ },
  android: {
    file: "app/android/core/src/main/kotlin/life/neurone/core/ble/GattUuids.kt",
    re: /val\s+consumableStatus\b/,
  },
};

/** Producers that are not a ConsumableKind: where each lives on each platform. */
const PRODUCERS: Record<string, Record<Platform, { file: string; re: RegExp }>> = {
  CVNS_PAD_STATUS: {
    ios: { file: "app/ios/NeurOne/BLE/GATTCharacteristics.swift", re: /static\s+let\s+cvnsPadStatus\b/ },
    android: {
      file: "app/android/core/src/main/kotlin/life/neurone/core/ble/GattUuids.kt",
      re: /val\s+cvnsPadStatus\b/,
    },
  },
};

/** The reminder engines T8 reads. */
const ENGINE_FILES: Record<Platform, string[]> = {
  ios: ["app/ios/NeurOne/Consumable/ConsumableTracker.swift", "app/ios/NeurOne/Models/ConsumableInventory.swift"],
  android: [
    "app/android/core/src/main/kotlin/life/neurone/core/consumable/ConsumableTracker.kt",
    "app/android/core/src/main/kotlin/life/neurone/core/consumable/ConsumableModels.kt",
  ],
};

/**
 * Hub-side producers the app consumes and the firmware does not publish yet. `match` is tested
 * against every firmware .c/.h with comments stripped.
 */
const FIRMWARE_PENDING: Array<{ producer: string; match: RegExp; oi: string; note: string }> = [
  {
    producer: "CVNS_PAD_STATUS",
    match: /cvns_pad_status|4e455550-0013/i,
    oi: "OI-ACC-07",
    note: "the hub does not publish the failed-pad mask or its side mapping yet",
  },
];

 /**
 * Hub-side producers that WERE pending and are now published. Same text match as
 * FIRMWARE_PENDING, in the other direction.
 */
const FIRMWARE_PUBLISHED: Array<{ producer: string; match: RegExp; oi: string; where: string }> = [
  {
    producer: "CONSUMABLE_STATUS",
    match: /consumable_status|4e455550-0007/i,
    oi: "OI-ACC-08",
    where: "firmware/hub_control/src/np_consumables.c, row 0x0007 of np_gatt_server.c (#381)",
  },
];

type Rule = "T2" | "T3" | "T4-mechanism" | "T4-cannot-see" | "T4-threshold" | "T5" | "T6";

/** A row excused from one rule. The row is matched by the start of its Item cell. */
const EXEMPTIONS: Array<{ row: string; rule: Rule; oi: string; why: string }> = [
  {
    row: "Electrode hydrogel tips",
    rule: "T4-mechanism",
    oi: "OI-ACC-09",
    why:
      "the shipped trigger is a 45-session count and no document names what it is a count OF; the row's " +
      "intended trigger, an impedance trend, has no producer",
  },
];

// ── Parsing ────────────────────────────────────────────────────────────────

type Kind = "condition" | "exposure" | "none";
type Row = {
  item: string;
  interval: string;
  trigger: string;
  kind: Kind | null;
  fields: Record<string, string>;
  line: number;
};

const KIND_LABEL: Record<string, Kind> = {
  "condition measurement": "condition",
  "exposure count": "exposure",
  "no prompt": "none",
};

function splitCells(line: string): string[] {
  // A `|` inside backticks is not a cell boundary; none occurs today, but guard it anyway.
  const cells: string[] = [];
  let cur = "";
  let tick = false;
  for (const ch of line.trim().replace(/^\|/, "").replace(/\|$/, "")) {
    if (ch === "`") tick = !tick;
    if (ch === "|" && !tick) {
      cells.push(cur.trim());
      cur = "";
    } else cur += ch;
  }
  cells.push(cur.trim());
  return cells;
}

export function parseTable(doc: string): { rows: Row[]; error: string | null } {
  const lines = doc.split("\n");
  const start = lines.findIndex((l) => l.startsWith(SECTION));
  if (start === -1) return { rows: [], error: `${DOC}: no "${SECTION}" heading` };
  let h = -1;
  for (let i = start + 1; i < lines.length && !/^#{1,3} /.test(lines[i]!); i++) {
    if (/^\|\s*Item\s*\|/.test(lines[i]!)) {
      h = i;
      break;
    }
  }
  if (h === -1) return { rows: [], error: `${DOC} §2.3: no table with an "Item" header` };
  const header = splitCells(lines[h]!).map((c) => c.toLowerCase());
  const col = (name: string) => header.indexOf(name);
  for (const need of ["item", "interval", "trigger"]) {
    if (col(need) === -1) {
      return { rows: [], error: `${DOC} §2.3: the table has no "${need}" column — every row must declare its trigger` };
    }
  }
  const rows: Row[] = [];
  for (let i = h + 2; i < lines.length && lines[i]!.trim().startsWith("|"); i++) {
    const cells = splitCells(lines[i]!);
    const trigger = cells[col("trigger")] ?? "";
    const m = /^\*\*([^*]+)\*\*/.exec(trigger);
    const kind = m ? KIND_LABEL[m[1]!.trim().toLowerCase()] ?? null : null;
    const fields: Record<string, string> = {};
    for (const part of trigger.split(/\s·\s/).slice(1)) {
      const f = /^([a-z][a-z ]*?):\s*(.*)$/i.exec(part.trim());
      if (f) fields[f[1]!.toLowerCase()] = f[2]!.trim();
    }
    rows.push({
      item: cells[col("item")] ?? "",
      interval: cells[col("interval")] ?? "",
      trigger,
      kind,
      fields,
      line: i + 1,
    });
  }
  if (!rows.length) return { rows, error: `${DOC} §2.3: the consumables table has no rows` };
  return { rows, error: null };
}

/** Remove comments. Strings are kept — a UUID lives in one. */
export function stripComments(src: string): string {
  return src.replace(/\/\*[\s\S]*?\*\//g, " ").replace(/\/\/[^\n]*/g, " ");
}

const camelToUpperSnake = (s: string) => s.replace(/([a-z0-9])([A-Z])/g, "$1_$2").toUpperCase();

export function kindCases(p: Platform, src: string): string[] {
  const code = stripComments(src);
  if (p === "ios") {
    const body = /enum\s+ConsumableKind\b[^{]*\{([\s\S]*?)\n\}/.exec(code)?.[1] ?? "";
    return [...body.matchAll(/^\s*case\s+(\w+)\s*=/gm)].map((m) => m[1]!);
  }
  const body = /enum\s+class\s+ConsumableKind\b[^{]*\{([\s\S]*?);/.exec(code)?.[1] ?? "";
  return [...body.matchAll(/^\s*([A-Z][A-Z0-9_]*)\s*\(/gm)].map((m) => m[1]!);
}

/** sessionLimit per case, keyed by the platform's own case name. */
export function sessionLimits(p: Platform, src: string): Record<string, number> {
  const code = stripComments(src);
  const out: Record<string, number> = {};
  const at = code.search(p === "ios" ? /var\s+sessionLimit\b/ : /val\s+sessionLimit\b/);
  if (at === -1) return out;
  const rest = code.slice(at + 1);
  const next = rest.search(p === "ios" ? /\n\s{4}var\s/ : /\n\s{4}val\s/);
  const block = next === -1 ? rest : rest.slice(0, next);
  const re = p === "ios" ? /case\s+\.(\w+)\s*:\s*return\s+(\d+)/g : /([A-Z][A-Z0-9_]*)\s*->\s*(\d+)/g;
  for (const m of block.matchAll(re)) out[m[1]!] = Number(m[2]);
  return out;
}

const CALENDAR = /\b(annual(ly)?|yearly|years?|months?|weeks?|weekly|days?|daily|hours?)\b|\/\s*(yr|mo)\b/i;
const CALENDAR_CODE: Array<[RegExp, string]> = [
  [/UNCalendarNotificationTrigger/, "a calendar notification trigger"],
  [/repeats\s*:\s*true/, "a repeating notification"],
  [/\bAlarmManager\b/, "AlarmManager"],
  [/PeriodicWorkRequest/, "periodic WorkManager work"],
  [/\bsetRepeating\b|\bsetInexactRepeating\b/, "a repeating alarm"],
];

function walk(root: string, dir: string, exts: string[], out: string[] = []): string[] {
  let entries: string[];
  try {
    entries = readdirSync(join(root, dir));
  } catch {
    return out;
  }
  for (const e of entries) {
    if (e === "build" || e === "vendor" || e.startsWith(".")) continue;
    const rel = `${dir}/${e}`;
    const st = statSync(join(root, rel));
    if (st.isDirectory()) walk(root, rel, exts, out);
    else if (exts.some((x) => e.endsWith(x))) out.push(rel);
  }
  return out;
}

// ── Audit ──────────────────────────────────────────────────────────────────

type Audit = { violations: string[]; rows: Row[]; notes: string[] };

function audit(root: string): Audit {
  const v: string[] = [];
  const notes: string[] = [];
  const read = (rel: string): string | null => {
    try {
      return readFileSync(join(root, rel), "utf8");
    } catch {
      return null;
    }
  };

  const doc = read(DOC);
  if (doc === null) return { violations: [`${DOC}: cannot be read`], rows: [], notes };
  const { rows, error } = parseTable(doc);
  if (error) return { violations: [error], rows, notes };

  // Code side.
  const cases: Record<Platform, string[]> = { ios: [], android: [] };
  const limits: Record<Platform, Record<string, number>> = { ios: {}, android: {} };
  for (const p of PLATFORMS) {
    const src = read(KIND_FILE[p]);
    if (src === null) {
      v.push(`${KIND_FILE[p]}: cannot be read — the ConsumableKind enum cannot be checked`);
      continue;
    }
    cases[p] = kindCases(p, src);
    limits[p] = sessionLimits(p, src);
    if (!cases[p].length) v.push(`${KIND_FILE[p]}: no ConsumableKind case found — refusing to pass vacuously`);
  }
  const iosAsAndroid = cases.ios.map(camelToUpperSnake).sort();
  const android = [...cases.android].sort();
  if (cases.ios.length && cases.android.length && iosAsAndroid.join() !== android.join()) {
    v.push(
      `T7: the two ConsumableKind enums differ — iOS [${iosAsAndroid.join(", ")}] vs Android ` +
        `[${android.join(", ")}]. The reminder engine is one design on two platforms`,
    );
  }

  const exemptFor = (r: Row, rule: Rule) => EXEMPTIONS.find((e) => r.item.startsWith(e.row) && e.rule === rule);
  const exemptionUsed = new Set<(typeof EXEMPTIONS)[number]>();
  /** Record a rule failure, unless exempted; an exemption records that it was needed. */
  const fail = (r: Row, rule: Rule, msg: string) => {
    const ex = exemptFor(r, rule);
    if (ex) exemptionUsed.add(ex);
    else v.push(`${rule.split("-")[0]}: ${DOC}:${r.line} "${r.item}" — ${msg}`);
  };

  const claimed = new Map<string, string>();

  for (const r of rows) {
    if (!r.kind) {
      v.push(
        `T1: ${DOC}:${r.line} "${r.item}" — the Trigger cell must open with **Condition measurement**, ` +
          `**Exposure count** or **No prompt** (CLAUDE.md §2.3)`,
      );
      continue;
    }
    const producer = /^`([^`]+)`/.exec(r.fields["producer"] ?? "")?.[1] ?? null;

    if (r.kind === "none") {
      if (r.fields["producer"] !== undefined) fail(r, "T6", "a No-prompt row names a producer; either it prompts or it does not");
      const reason = r.trigger.replace(/^\*\*[^*]+\*\*\s*·?\s*/, "");
      if (reason.length < 20) fail(r, "T6", "a No-prompt row must say why no prompt ships");
      continue;
    }

    // T2 — the producer exists.
    let kindCase: string | null = null;
    if (!producer) {
      fail(r, "T2", "a prompting row must name its producer as producer: `…`");
    } else if (producer.startsWith("ConsumableKind.")) {
      kindCase = producer.slice("ConsumableKind.".length);
      const androidCase = camelToUpperSnake(kindCase);
      const prev = claimed.get(kindCase);
      if (prev) v.push(`T7: ConsumableKind.${kindCase} is claimed by two rows, "${prev}" and "${r.item}"`);
      claimed.set(kindCase, r.item);
      if (!cases.ios.includes(kindCase)) fail(r, "T2", `producer ${producer} is not a case of the iOS enum (${KIND_FILE.ios})`);
      if (!cases.android.includes(androidCase)) {
        fail(r, "T2", `producer ${producer} has no Android case ${androidCase} (${KIND_FILE.android})`);
      }
      for (const p of PLATFORMS) {
        const s = read(CONSUMABLE_STATUS[p].file);
        if (s === null || !CONSUMABLE_STATUS[p].re.test(stripComments(s))) {
          fail(r, "T2", `the CONSUMABLE_STATUS characteristic every ConsumableKind is read from is not declared in ${CONSUMABLE_STATUS[p].file}`);
        }
      }
    } else if (PRODUCERS[producer]) {
      for (const p of PLATFORMS) {
        const loc = PRODUCERS[producer]![p];
        const s = read(loc.file);
        if (s === null || !loc.re.test(stripComments(s))) {
          fail(r, "T2", `producer ${producer} is not found on ${p} (${loc.file})`);
        }
      }
    } else {
      fail(r, "T2", `producer ${producer} is neither a ConsumableKind nor registered in PRODUCERS — say where it lives`);
    }

    // T3 — no calendar interval on a prompting row.
    const cal = CALENDAR.exec(r.interval);
    if (cal) fail(r, "T3", `a prompting row's Interval is calendar-denominated ("${cal[0]}") — CLAUDE.md §2.3`);

    if (r.kind === "condition") {
      if (kindCase) {
        fail(r, "T5", `a condition measurement cannot be produced by ${producer}: every ConsumableKind is a session count`);
      }
      continue;
    }

    // T4 — exposure count.
    const mech = r.fields["mechanism"] ?? "";
    if (mech.length < 10 || /^not named\b/i.test(mech)) {
      fail(r, "T4-mechanism", "an exposure count must name the mechanism its count drives (CLAUDE.md §2.3)");
    }
    if ((r.fields["cannot see"] ?? "").length < 10) {
      fail(r, "T4-cannot-see", "an exposure count must say what its count cannot see (CLAUDE.md §2.3)");
    }
    const thr = r.fields["threshold"] ?? "";
    const n = /^(\d+)\b/.exec(thr);
    if (!n) {
      fail(r, "T4-threshold", "an exposure count must state threshold: <N> — <provenance>");
    } else {
      const placeholder = /unvalidated placeholder/i.test(thr) && /OI-[A-Z0-9]+-\d+/.test(thr);
      const cited = /`[^`]+`/.test(thr.replace(/`OI-[^`]+`/g, ""));
      if (!placeholder && !cited) {
        fail(
          r,
          "T4-threshold",
          `threshold ${n[1]} states no provenance — cite what it is derived from, or label it an ` +
            `unvalidated placeholder with its open item (CLAUDE.md §2.3, NP-FW-EMMC-002 §G.2)`,
        );
      }
      if (kindCase) {
        const want = Number(n[1]);
        const got = { ios: limits.ios[kindCase], android: limits.android[camelToUpperSnake(kindCase)] };
        for (const p of PLATFORMS) {
          if (got[p] !== want) {
            fail(r, "T4-threshold", `threshold ${want} but ${p} sessionLimit is ${got[p] ?? "absent"} (${KIND_FILE[p]})`);
          }
        }
      }
    }
  }

  // T7 — every code case is claimed.
  for (const c of cases.ios) {
    if (!claimed.has(c)) {
      v.push(
        `T7: ConsumableKind.${c} prompts in the app but no §2.3 row names it as producer — ` +
          `a prompt with no declared trigger is the state this gate exists to end`,
      );
    }
  }

  // T8 — no calendar-shaped trigger in the engine.
  for (const p of PLATFORMS) {
    for (const f of ENGINE_FILES[p]) {
      const s = read(f);
      if (s === null) {
        v.push(`T8: ${f}: reminder-engine file cannot be read — if it moved, move it here too`);
        continue;
      }
      const code = stripComments(s);
      for (const [re, what] of CALENDAR_CODE) {
        if (re.test(code)) v.push(`T8: ${f} uses ${what} — a consumable prompt must not be calendar-triggered (CLAUDE.md §2.3)`);
      }
    }
  }

  // Exemptions must still be needed, and must name a row that exists.
  for (const e of EXEMPTIONS) {
    if (!rows.some((r) => r.item.startsWith(e.row))) {
      v.push(`exemption: "${e.row}" (${e.rule}, ${e.oi}) names no §2.3 row — stale entry`);
    } else if (!exemptionUsed.has(e)) {
      v.push(
        `exemption: "${e.row}" is excused from ${e.rule} under ${e.oi}, but the row now passes it — ` +
          `remove the exemption, it must not outlive its gap`,
      );
    } else notes.push(`exempt  ${e.row} from ${e.rule} — ${e.oi}: ${e.why}`);
  }

  // Firmware producers declared absent must still be absent.
  const fw = walk(root, "firmware", [".c", ".h"]);
  for (const f of FIRMWARE_PENDING) {
    const hit = fw.find((rel) => f.match.test(stripComments(read(rel) ?? "")));
    if (hit) {
      v.push(
        `pending: ${f.producer} is declared unpublished by the hub (${f.oi}) but ${hit} now references it — ` +
          `update FIRMWARE_PENDING and the §2.3 row to say where the hub produces it`,
      );
    } else {
      notes.push(`pending ${f.producer} has no hub producer — ${f.oi}: ${f.note}`);
    }
  }
  for (const f of FIRMWARE_PUBLISHED) {
    const hit = fw.find((rel) => f.match.test(stripComments(read(rel) ?? "")));
    if (!hit) {
      v.push(
        `published: ${f.producer} is declared produced by the hub (${f.oi}: ${f.where}) but no firmware ` +
          `code references it — restore the producer, or move it back to FIRMWARE_PENDING and say so in the §2.3 row`,
      );
    } else {
      notes.push(`published ${f.producer} — hub producer in ${hit} (${f.oi})`);
    }
  }

  return { violations: v, rows, notes };
}

// ── Self-test ────────────────────────────────────────────────────────────────
if (process.argv.includes("--self-test")) {
  const failures: string[] = [];
  const box = mkdtempSync(join(tmpdir(), "np-consumable-"));
  const build = (files: Record<string, string>): string => {
    const root = mkdtempSync(join(box, "t-"));
    for (const [rel, body] of Object.entries(files)) {
      mkdirSync(join(root, rel.split("/").slice(0, -1).join("/")), { recursive: true });
      writeFileSync(join(root, rel), body);
    }
    return root;
  };

  const HYDRO =
    "| Electrode hydrogel tips | $12 | 30–60 sessions | 60% | " +
    "**Exposure count** · producer: `ConsumableKind.electrodeHydrogel` · threshold: 45 — unvalidated placeholder (`OI-ACC-09`) · " +
    "mechanism: not named · cannot see: a tip damaged or dried off the device | x |";
  const row = {
    pads:
      "| VNS clip pads | $8 | 20–40 sessions | 65% | " +
      "**Exposure count** · producer: `ConsumableKind.vnsPads` · threshold: 30 — unvalidated placeholder (`OI-VNSCLIP-07`) · " +
      "mechanism: electrochemical degradation from VNS current · cannot see: a pad damaged or degraded off the device | x |",
    cervical:
      "| Cervical VNS gel pads | — | On failure | — | " +
      "**Condition measurement** · producer: `CVNS_PAD_STATUS` · per-electrode impedance before every session | x |",
    covers: "| Interface covers | $22 | Annual | 70% | **No prompt** · loss is not degradation; replacement is the user's call | x |",
  };
  const table = (rows: string[], header = "| Item | Price | Interval | GM% | Trigger | Notes |") =>
    `# x\n\n${SECTION} + recurring revenue\n\nprose\n\n${header}\n|---|---|---|---|---|---|\n${rows.join("\n")}\n\n---\n`;
  const IOS_KINDS = (cases: Array<[string, number]>) =>
    "enum ConsumableKind: Int, CaseIterable {\n" +
    cases.map(([c], i) => `    case ${c} = ${i}\n`).join("") +
    "\n    var sessionLimit: Int {\n        switch self {\n" +
    cases.map(([c, n]) => `        case .${c}: return ${n}\n`).join("") +
    "        }\n    }\n\n    var lowThreshold: Int { 0 }\n}\n";
  const AND_KINDS = (cases: Array<[string, number]>) =>
    "enum class ConsumableKind(val rawValue: Int) {\n" +
    cases.map(([c], i) => `    ${c}(${i})`).join(",\n") +
    ";\n\n    val sessionLimit: Int\n        get() = when (this) {\n" +
    cases.map(([c, n]) => `            ${c} -> ${n}\n`).join("") +
    "        }\n\n    val lowThreshold: Int get() = 0\n}\n";
  const GOOD_CASES: Array<[string, number]> = [["electrodeHydrogel", 45], ["vnsPads", 30]];
  const base = (over: Record<string, string> = {}): Record<string, string> => ({
    [DOC]: table([HYDRO, row.pads, row.cervical, row.covers]),
    [KIND_FILE.ios]: IOS_KINDS(GOOD_CASES),
    [KIND_FILE.android]: AND_KINDS(GOOD_CASES.map(([c, n]) => [camelToUpperSnake(c), n])),
    [CONSUMABLE_STATUS.ios.file]: "enum NPUUID {\n    static let consumableStatus = X\n    static let cvnsPadStatus = Y\n}\n",
    [CONSUMABLE_STATUS.android.file]: "object U {\n    val consumableStatus: UUID = X\n    val cvnsPadStatus: UUID = Y\n}\n",
    [ENGINE_FILES.ios[0]!]: "let trigger = UNTimeIntervalNotificationTrigger(timeInterval: 1, repeats: false)\n",
    [ENGINE_FILES.android[0]!]: "class ConsumableTracker {}\n",
    "firmware/hub/np_x.c": "/* CVNS_PAD_STATUS in a comment must not count */\nvoid f(void) {}\n",
    "firmware/hub/np_cons.c": "#define NP_GATT_ID_CONSUMABLE_STATUS 0x0007u\n",
    ...over,
  });
  const expect = (label: string, root: string, needle: string | null) => {
    const { violations } = audit(root);
    if (needle === null) {
      if (violations.length) failures.push(`${label} — expected clean, got: ${violations[0]}`);
    } else if (!violations.some((x) => x.includes(needle))) {
      failures.push(`${label} — no violation matching ${JSON.stringify(needle)}; got ${JSON.stringify(violations)}`);
    }
  };

  // Parser and extractors.
  const lim = sessionLimits("ios", IOS_KINDS(GOOD_CASES));
  if (lim["vnsPads"] !== 30 || lim["electrodeHydrogel"] !== 45) failures.push("extractor — iOS sessionLimit misread");
  const alim = sessionLimits("android", AND_KINDS([["VNS_PADS", 30]]));
  if (alim["VNS_PADS"] !== 30) failures.push("extractor — Android sessionLimit misread");
  if (kindCases("android", AND_KINDS([["A_B", 1], ["C", 2]])).join() !== "A_B,C") failures.push("extractor — Android cases misread");
  if (camelToUpperSnake("audioCupFoam") !== "AUDIO_CUP_FOAM") failures.push("extractor — case mapping");

  // Pass direction.
  expect("a well-formed table over matching code passes", build(base()), null);

  // Fail direction, one per rule.
  expect("T1: an undeclared trigger kind is caught", build(base({ [DOC]: table([HYDRO, row.pads, row.cervical, row.covers.replace("**No prompt**", "Annual reminder")]) })), "T1:");
  expect("T1: a table without a Trigger column is caught", build(base({ [DOC]: table([HYDRO], "| Item | Price | Interval | GM% | Kind | Notes |") })), "no \"trigger\" column");
  expect("T2: a producer that does not exist is caught", build(base({ [DOC]: table([HYDRO, row.pads.replace("vnsPads", "vnsClips"), row.cervical, row.covers]) })), "not a case of the iOS enum");
  expect("T2: an unregistered producer is caught", build(base({ [DOC]: table([HYDRO, row.pads, row.cervical.replace("CVNS_PAD_STATUS", "MESH_IMPEDANCE"), row.covers]) })), "neither a ConsumableKind nor registered");
  expect("T2: a registered producer missing on one platform is caught", build(base({ [CONSUMABLE_STATUS.android.file]: "val consumableStatus: UUID = X\n" })), "not found on android");
  expect("T2: a prompting row with no producer is caught", build(base({ [DOC]: table([HYDRO, row.pads, row.cervical.replace(" · producer: `CVNS_PAD_STATUS`", ""), row.covers]) })), "must name its producer");
  expect("T3: a calendar interval on a prompting row is caught (the OI-ACC-02 shape)", build(base({ [DOC]: table([HYDRO, row.pads.replace("20–40 sessions", "6–12 months"), row.cervical, row.covers]) })), "calendar-denominated");
  expect("T4: an exposure count with no mechanism is caught", build(base({ [DOC]: table([HYDRO, row.pads.replace("mechanism: electrochemical degradation from VNS current · ", ""), row.cervical, row.covers]) })), "name the mechanism");
  expect("T4: an exposure count that does not say what it cannot see is caught", build(base({ [DOC]: table([HYDRO, row.pads.replace(/ · cannot see: [^|]+/, " "), row.cervical, row.covers]) })), "cannot see");
  expect("T4: a threshold with no provenance is caught", build(base({ [DOC]: table([HYDRO, row.pads.replace(" — unvalidated placeholder (`OI-VNSCLIP-07`)", " — midpoint"), row.cervical, row.covers]) })), "states no provenance");
  expect("T4: a doc threshold that disagrees with the code is caught", build(base({ [KIND_FILE.ios]: IOS_KINDS([["electrodeHydrogel", 45], ["vnsPads", 25]]) })), "ios sessionLimit is 25");
  expect("T5: a session count labelled a condition measurement is caught (the hydrogel shape)", build(base({ [DOC]: table([HYDRO, row.pads.replace("**Exposure count**", "**Condition measurement**"), row.cervical, row.covers]) })), "T5:");
  expect("T6: a No-prompt row naming a producer is caught", build(base({ [DOC]: table([HYDRO, row.pads, row.cervical, row.covers.replace("**No prompt** ·", "**No prompt** · producer: `X` ·")]) })), "T6:");
  expect("T7: a ConsumableKind with no row is caught", build(base({ [KIND_FILE.ios]: IOS_KINDS([...GOOD_CASES, ["meshFrame", 365]]), [KIND_FILE.android]: AND_KINDS([["ELECTRODE_HYDROGEL", 45], ["VNS_PADS", 30], ["MESH_FRAME", 365]]) })), "no §2.3 row names it");
  expect("T7: diverging enums are caught", build(base({ [KIND_FILE.android]: AND_KINDS([["ELECTRODE_HYDROGEL", 45], ["VNS_PADS", 30], ["EXTRA", 1]]) })), "two ConsumableKind enums differ");
  expect("T7: two rows claiming one kind are caught", build(base({ [DOC]: table([HYDRO, row.pads, row.pads.replace("VNS clip pads", "VNS clip pads again"), row.cervical, row.covers]) })), "claimed by two rows");
  expect("T8: a calendar notification trigger is caught", build(base({ [ENGINE_FILES.ios[0]!]: "let t = UNCalendarNotificationTrigger(dateMatching: d, repeats: false)\n" })), "calendar notification trigger");
  expect("T8: periodic Android work is caught", build(base({ [ENGINE_FILES.android[0]!]: "val r = PeriodicWorkRequestBuilder<W>(30, DAYS)\n" })), "periodic WorkManager");
  expect("T8: a calendar trigger in a comment is not counted", build(base({ [ENGINE_FILES.ios[0]!]: "// never UNCalendarNotificationTrigger\n" })), null);

  // Declared absences must break when the absence ends.
  expect("an exemption the row no longer needs is caught", build(base({ [DOC]: table([HYDRO.replace("mechanism: not named", "mechanism: dehydration of the gel under wear"), row.pads, row.cervical, row.covers]) })), "now passes it");
  expect("an exemption naming no row is caught", build(base({ [DOC]: table([row.pads, row.cervical, row.covers]), [KIND_FILE.ios]: IOS_KINDS([["vnsPads", 30]]), [KIND_FILE.android]: AND_KINDS([["VNS_PADS", 30]]) })), "names no §2.3 row");
  expect("a hub producer appearing flips its pending declaration", build(base({ "firmware/hub/np_gatt.c": "#define NP_GATT_CVNS_PAD_STATUS_UUID 0x13\n" })), "now references it");
  expect("a published hub producer disappearing is caught", build(base({ "firmware/hub/np_cons.c": "/* NP_GATT_ID_CONSUMABLE_STATUS */\nvoid g(void) {}\n" })), "no firmware code references it");
  expect("a hub producer appearing by UUID flips its pending declaration", build(base({ "firmware/hub/np_gatt.c": "static const char *u = \"4E455550-0013-1000\";\n" })), "CVNS_PAD_STATUS is declared unpublished");

  rmSync(box, { recursive: true, force: true });
  console.log("check-consumable-triggers self-test");
  if (failures.length) {
    console.error(`\nSELF-TEST FAIL — ${failures.length} assertion(s):`);
    for (const f of failures) console.error("  " + f);
    process.exit(1);
  }
  console.log("  extractors proven on both enums and both sessionLimit tables; a well-formed tree passes;");
  console.log("  T1–T8 each proven to fire; a stale exemption, a stale firmware-pending and a lost firmware-published proven to fire");
  console.log("SELF-TEST PASS — the checker has teeth.");
  process.exit(0);
}

const ROOT = join(import.meta.dir, "..");
const result = audit(ROOT);

if (result.rows.length === 0) {
  for (const x of result.violations) console.error("  " + x);
  console.error("check-consumable-triggers: no §2.3 row was read — refusing to pass vacuously.");
  process.exit(2);
}

const counts = { condition: 0, exposure: 0, none: 0 };
for (const r of result.rows) if (r.kind) counts[r.kind]++;
console.log(
  `scanned: ${result.rows.length} §2.3 row(s) — ${counts.condition} condition measurement, ` +
    `${counts.exposure} exposure count, ${counts.none} no prompt`,
);
for (const r of result.rows.filter((x) => x.kind === "none")) {
  console.log(`  no prompt  ${r.item}`);
}
if (result.notes.length) {
  console.log("\ndeclared absences (each fails once it is no longer true):");
  for (const n of result.notes) console.log("  " + n);
}

if (result.violations.length) {
  console.error(`\n${result.violations.length} consumable-trigger violation(s):\n`);
  for (const x of result.violations) console.error("  " + x);
  console.error(
    "\nCLAUDE.md §2.3: every consumable replacement prompt is triggered by a condition measurement,\n" +
      "or by an exposure count with its mechanism named. A threshold back-derived from a calendar is\n" +
      "neither, and a consumable with no measurement gets no prompt.",
  );
  process.exit(1);
}
console.log("\nEvery §2.3 prompt declares an admissible trigger with a producer that exists. PASS");
process.exit(0);
