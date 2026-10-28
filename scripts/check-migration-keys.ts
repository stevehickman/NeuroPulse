#!/usr/bin/env bun
/**
 * check-migration-keys.ts — every key an app stores is sorted for migration, on every platform.
 *
 * SWR-MG-01 (docs/np_sw_001.md §6.3) says which state a device-replacement migration moves, which it never moves
 * because it belongs to the install, and which is not yet sorted. OI-UI-KMP-11 (d) asks for that to survive new keys:
 * "a list of keys goes stale the first time a key is added". So there is ONE list, common/npps/migration-keys.json,
 * read by the shared core (neurone_npps_core::migration, migration_flow) and by this gate, and a key an app writes that
 * is in none of its lists fails here, for iOS, Android, Windows, macOS and the web alike: a new key cannot move by
 * accident or be forgotten.
 *
 * What it reads: every tracked app source file (.kt .swift .cs .ts .tsx) and common/lib, tests excluded, for a string
 * literal that looks like a stored key: "np.…" or "ble.…".
 *
 *   K1  A key literal in the sources is in no list of the JSON.                 (a new key nobody sorted)
 *   K2  A key is in two lists, or twice in one.                                 (it would move and not move)
 *   K3  A key in the JSON appears in no source file.                            (a stale entry; delete it)
 *   K4  A list is missing, or an entry is not a plain key.
 *
 * Reach, stated narrowly: this finds a key written as a literal. A key assembled at run time ("np." + name) is not seen.
 * It does not check that a carried key's value round-trips, which the core's flow tests do.
 *
 * Usage: bun scripts/check-migration-keys.ts [--self-test]
 *
 * CI-Kind: gate
 * CI-Self-Test: bun scripts/check-migration-keys.ts --self-test
 * CI-Scans: every stored-key literal in the app sources (iOS, Android, Windows, macOS, web) common/lib, common/core and common/cloudauth against the one key list common/npps/migration-keys.json
 * CI-Scan-Paths: app/** common/lib/** common/core/** common/cloudauth/** common/npps/migration-keys.json scripts/check-migration-keys.ts
 */
import { execFileSync } from "child_process";
import { readFileSync } from "fs";
import { join, resolve } from "path";

export const KEYS_JSON = "common/npps/migration-keys.json";
const LISTS = ["carried", "consent", "perInstall", "unsorted"] as const;
const KEY_LITERAL = /"((?:np|ble)\.[A-Za-z0-9_.\-]+)"/g;
const PLAIN_KEY = /^(?:np|ble)\.[A-Za-z0-9_.\-]+$/;
const SOURCE_EXT = /\.(kt|swift|cs|ts|tsx)$/;
// Tests and generated output name keys that are not stored (fixtures, golden data).
const EXCLUDED = /(^|\/)(node_modules|build|dist|generated|test|tests|Tests|NeurOneTests|NeurOne\.Tests|jvmTest|commonTest|androidTest|desktopTest)\/|\.test\.(ts|tsx)$|Tests?\.(kt|swift|cs)$/;

export interface Finding {
  file: string;
  message: string;
}

export function listedKeys(map: any, out: Finding[]): Map<string, string> {
  const seen = new Map<string, string>();
  const add = (key: unknown, list: string) => {
    if (typeof key !== "string" || !PLAIN_KEY.test(key)) {
      out.push({ file: KEYS_JSON, message: `K4: '${String(key)}' in '${list}' is not a plain np.* or ble.* key` });
      return;
    }
    if (seen.has(key)) out.push({ file: KEYS_JSON, message: `K2: '${key}' is in '${seen.get(key)}' and in '${list}'` });
    else seen.set(key, list);
  };
  for (const l of LISTS) {
    if (map?.[l] === undefined) { out.push({ file: KEYS_JSON, message: `K4: the list '${l}' is missing` }); continue; }
    if (l === "carried") {
      for (const [item, keys] of Object.entries<any>(map.carried)) {
        if (!Array.isArray(keys)) { out.push({ file: KEYS_JSON, message: `K4: carried item '${item}' is not a list` }); continue; }
        for (const k of keys) add(k, `carried/${item}`);
      }
    } else if (Array.isArray(map[l])) {
      for (const k of map[l]) add(k, l);
    } else {
      out.push({ file: KEYS_JSON, message: `K4: '${l}' is not a list` });
    }
  }
  return seen;
}

export function check(files: Map<string, string>, map: any): Finding[] {
  const out: Finding[] = [];
  const listed = listedKeys(map, out);
  // The device tier (NP-APP-PRESENCE-001 §4) is a subset of the sorted keys: a key readable while locked is still a stored key.
  for (const k of Array.isArray(map?.deviceTier) ? map.deviceTier : []) {
    if (!listed.has(k)) out.push({ file: KEYS_JSON, message: `K5: device-tier key '${String(k)}' is in no list` });
  }
  const found = new Map<string, string>();
  for (const [path, text] of files) {
    if (!SOURCE_EXT.test(path) || EXCLUDED.test(path)) continue;
    for (const m of text.matchAll(KEY_LITERAL)) {
      const key = m[1];
      if (!found.has(key)) found.set(key, path);
      if (!listed.has(key)) out.push({ file: path, message: `K1: stored key '${key}' is in no list of ${KEYS_JSON}: decide whether it moves (carried), is the install's (perInstall), is consent state (consent) or is not yet sorted (unsorted)` });
    }
  }
  for (const [key, list] of listed) {
    if (!found.has(key)) out.push({ file: KEYS_JSON, message: `K3: '${key}' (${list}) appears in no app source: delete the entry or restore the key` });
  }
  return out;
}

function selfTest(): void {
  const map = {
    carried: { sessionHistory: ["np.session.history"] },
    consent: ["np.consent.research"],
    perInstall: ["ble.last-hub-id"],
    unsorted: ["np.setup.first-complete"],
  };
  const src = new Map<string, string>([
    ["common/core/A.kt", 'const val K = "np.session.history"; val c = "np.consent.research"'],
    ["app/ios/NeurOne/B.swift", 'let k = "ble.last-hub-id"; let s = "np.setup.first-complete"'],
  ]);
  const expect = (name: string, files: Map<string, string>, m: any, want: string[]) => {
    const got = check(files, m).map((f) => f.message.slice(0, 2));
    if (JSON.stringify(got.sort()) !== JSON.stringify(want.sort())) {
      console.error(`self-test FAILED: ${name}: expected [${want}], got [${got}]`);
      process.exit(1);
    }
  };
  expect("a sorted tree passes", src, map, []);
  expect("a new key in Kotlin fails", new Map([...src, ["common/core/C.kt", 'val x = "np.new.key"']]), map, ["K1"]);
  expect("a new key in Swift fails", new Map([...src, ["app/ios/NeurOne/C.swift", 'let x = "np.new.key"']]), map, ["K1"]);
  expect("a new key in C# fails", new Map([...src, ["app/windows/NeurOne/C.cs", 'var x = "np.new.key";']]), map, ["K1"]);
  expect("a new key in the web app fails", new Map([...src, ["app/web/src/c.ts", 'const x = "np.new.key";']]), map, ["K1"]);
  expect("a key in a test is not read", new Map([...src, ["common/core/src/jvmTest/T.kt", 'val x = "np.fixture.only"']]), map, []);
  expect("a key in a .test.ts is not read", new Map([...src, ["common/lib/x.test.ts", 'const x = "np.fixture.only";']]), map, []);
  expect("a stale entry fails", new Map([["common/core/A.kt", 'val c = "np.consent.research"; val s = "np.session.history"']]), map, ["K3", "K3"]);
  expect("a key in two lists fails", src, { ...map, unsorted: [...map.unsorted, "np.session.history"] }, ["K2"]);
  expect("an entry that is not a key fails", src, { ...map, unsorted: [...map.unsorted, "session.history"] }, ["K4"]);
  expect("a device-tier key in no list fails", src, { ...map, deviceTier: ["np.not.stored"] }, ["K5"]);
  expect("a device-tier key that is listed passes", src, { ...map, deviceTier: ["ble.last-hub-id"] }, []);
  expect("a missing list fails", src, { carried: map.carried, consent: map.consent, perInstall: map.perInstall }, ["K4", "K1"]);
  console.log("check-migration-keys self-test: ok");
}

if (import.meta.main) {
  if (process.argv.includes("--self-test")) { selfTest(); process.exit(0); }
  const root = resolve(import.meta.dir, "..");
  const tracked = execFileSync("git", ["ls-files", "-z", "app", "common/lib", "common/core", "common/cloudauth"], { cwd: root, encoding: "utf8" }).split("\0").filter(Boolean);
  const files = new Map<string, string>();
  for (const p of tracked) {
    if (!SOURCE_EXT.test(p)) continue;
    try { files.set(p, readFileSync(join(root, p), "utf8")); } catch { /* deleted or unreadable */ }
  }
  const map = JSON.parse(readFileSync(join(root, KEYS_JSON), "utf8"));
  const findings = check(files, map);
  console.log(`scanned: ${files.size} source file(s) under app/, common/lib/, common/core/ and common/cloudauth/ against ${KEYS_JSON}`);
  for (const f of findings) console.error(`${f.file}: ${f.message}`);
  if (findings.length) {
    console.error(`\n${findings.length} finding(s). SWR-MG-01: every stored key is sorted in ${KEYS_JSON}, for every platform.`);
    process.exit(1);
  }
  console.log("check-migration-keys: ok");
}
