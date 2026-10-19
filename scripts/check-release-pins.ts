#!/usr/bin/env bun
/**
 * check-release-pins.ts — a release build cannot ship with the placeholder fleet certificate pins.
 *
 * Privacy review Finding 9, REQ-SHDR-04. The SHDR fleet endpoint is SPKI-pinned (iOS `SHDRFleetPinningDelegate`, Android
 * `ShdrUploader`), and both apps ship placeholder hashes ("AAAA…=", "BBBB…=") until the real fleet certificate exists. A
 * placeholder fails closed: it matches no certificate, so every fleet upload is refused. That is the right default and also a
 * silent one, because a release built with them looks fine and uploads nothing, forever. This makes the failure loud, where a
 * release is built.
 *
 *   P1  Each pinned file yields at least two well-formed pins (a primary and a backup, which REQ-SHDR-04 requires).
 *   P2  Every pin-shaped literal is a well-formed SHA-256 SPKI pin: 43 base64 characters and one "=".
 *   P4  Each pinned file declares GATEWAY_KEY_CONFIG_HEX, and (--release) it is not empty. The fleet upload is Oblivious HTTP (REQ-SHDR-07):
 *       the record is sealed to the gateway's key, and an empty key sends nothing, which is the same silent failure as a placeholder pin.
 *   P3  (--release) No pin is a placeholder. A placeholder is a pin that is one character repeated: a real SHA-256 of a public
 *       key is not.
 *
 * Without --release it reports how many placeholders remain and exits 0 (CI must stay green until the fleet certificate exists),
 * but P1 and P2 still fail, so a change that breaks the pin lists or the detector's view of them cannot pass quietly. With
 * --release it is the gate the release build calls: the iOS Release pre-build phase (app/ios/project.yml) and the Android
 * assembleRelease/bundleRelease tasks (app/android/app/build.gradle.kts).
 *
 * Reach, stated narrowly: it reads the two source files named below. A pin added in a third place, or fetched at run time, is not
 * seen. It does not check that a real-looking pin matches the real certificate.
 *
 * Usage: bun scripts/check-release-pins.ts [--release] [--self-test]
 *
 * CI-Kind: gate
 * CI-Self-Test: bun scripts/check-release-pins.ts --self-test
 * CI-Scans: the SPKI pin literals in the iOS and Android SHDR uploaders (P1 two pins each, P2 well-formed, P3 no placeholder under --release)
 * CI-Self-Test-Reads-Tree: one case reads the real iOS and Android uploaders to prove the detector can see their pins, which a fixture cannot show
 * CI-Scan-Paths: app/ios/NeurOne/Data/SHDRUploader.swift app/android/app/src/main/kotlin/life/neurone/app/data/ShdrUploader.kt scripts/check-release-pins.ts
 */
import { readFileSync } from "fs";
import { join, resolve } from "path";

export const PIN_FILES = [
  "app/ios/NeurOne/Data/SHDRUploader.swift",
  "app/android/app/src/main/kotlin/life/neurone/app/data/ShdrUploader.kt",
];

// A quoted string that looks like a pin: an optional "sha256/" and a run of base64 characters (and its padding).
const PIN_SHAPED = /"(?:sha256\/)?([A-Za-z0-9+/]{30,}={0,2})"/g;
const WELL_FORMED = /^[A-Za-z0-9+/]{43}=$/;
const PLACEHOLDER = /^(.)\1{42}=$/;
const KEY_CONFIG = /GATEWAY_KEY_CONFIG_HEX\s*(?::\s*\w+\s*)?=\s*"([^"]*)"/;

export interface Finding {
  file: string;
  message: string;
}

export interface Pins {
  wellFormed: string[];
  malformed: string[];
}

export function pinsIn(source: string): Pins {
  const wellFormed: string[] = [];
  const malformed: string[] = [];
  // The gateway's key configuration is hex of the right alphabet and length to look like a pin; it is not one.
  const withoutKey = source.replace(new RegExp(KEY_CONFIG.source, "g"), "");
  for (const m of withoutKey.matchAll(PIN_SHAPED)) {
    const pin = m[1]!;
    (WELL_FORMED.test(pin) ? wellFormed : malformed).push(pin);
  }
  return { wellFormed, malformed };
}

export const isPlaceholder = (pin: string): boolean => PLACEHOLDER.test(pin);

export function check(files: Record<string, string>, release: boolean): { findings: Finding[]; scanned: number; placeholders: number } {
  const findings: Finding[] = [];
  let scanned = 0;
  let placeholders = 0;
  for (const [file, source] of Object.entries(files)) {
    const { wellFormed, malformed } = pinsIn(source);
    scanned += wellFormed.length;
    if (wellFormed.length < 2) {
      findings.push({ file, message: `P1 ${wellFormed.length} well-formed pin(s) found; a primary and a backup are required` });
    }
    for (const bad of malformed) {
      findings.push({ file, message: `P2 "${bad.slice(0, 12)}…" is pin-shaped but not 43 base64 characters and one "="` });
    }
    const kc = KEY_CONFIG.exec(source);
    if (!kc) {
      findings.push({ file, message: "P4 GATEWAY_KEY_CONFIG_HEX is not declared" });
    } else if (release && kc[1] === "") {
      findings.push({ file, message: "P4 GATEWAY_KEY_CONFIG_HEX is empty: a release build would send nothing; set the gateway's key configuration (docs/np_infra_001.md §3.3b)" });
    }
    const placeholder = wellFormed.filter(isPlaceholder);
    placeholders += placeholder.length;
    if (release && placeholder.length > 0) {
      findings.push({
        file,
        message: `P3 ${placeholder.length} placeholder pin(s) (${placeholder.map((p) => p.slice(0, 4) + "…").join(", ")}): derive the real hashes from the fleet certificate (docs/np_infra_001.md §3.5) before a release build`,
      });
    }
  }
  return { findings, scanned, placeholders };
}

function selfTest(): number {
  const real = "k4Vq2yJ0mXr8nQe1uZpC7sT5wLbH3aGdF6oYiNx9RMc=";
  const real2 = "Zr1B8dEw0tUy5KfQh3nPaLs7VcXgJ2mO4iNeAoT6bYk=";
  const A = "A".repeat(43) + "=";
  const B = "B".repeat(43) + "=";
  const KEY = "01002031e1f05a740102115220e9af918f738674aec95f54db6e04eb705aae8e79815500080001000100010003";
  const swift = (...p: string[]) => `private let GATEWAY_KEY_CONFIG_HEX = "${KEY}"\nstatic let PINNED_HASHES: Set<Data> = { let h = [${p.map((x) => `"${x}"`).join(",")}] }()`;
  const kt = (...p: string[]) => `const val GATEWAY_KEY_CONFIG_HEX = "${KEY}"\nprivate val PINNED_SPKI_HASHES = listOf(${p.map((x) => `"sha256/${x}"`).join(",")})`;
  const files = (s: string, k: string) => ({ ios: s, android: k });
  const failures: string[] = [];
  const t = (name: string, ok: boolean) => ok || failures.push(name);
  const codes = (f: Finding[]) => f.map((x) => x.message.slice(0, 2));

  const good = check(files(swift(real, real2), kt(real, real2)), true);
  t("real pins pass in release mode", good.findings.length === 0 && good.scanned === 4 && good.placeholders === 0);

  const ph = files(swift(A, B), kt(A, B));
  t("placeholders pass without --release and are counted", check(ph, false).findings.length === 0 && check(ph, false).placeholders === 4);
  t("placeholders fail with --release (P3, both files)", codes(check(ph, true).findings).filter((c) => c === "P3").length === 2);
  t("one placeholder among real pins fails with --release", codes(check(files(swift(real, B), kt(real, real2)), true).findings).join() === "P3");
  t("a single pin fails (P1) even without --release", codes(check(files(swift(real), kt(real, real2)), false).findings).join() === "P1");
  t("no pins at all fails (P1): the detector must not go vacuous", codes(check(files(`private let GATEWAY_KEY_CONFIG_HEX = "${KEY}"\nnothing here`, kt(real, real2)), false).findings).join() === "P1");
  t("a short pin is P2, not silently skipped", codes(check(files(swift(real, real2, real.slice(0, 40) + "="), kt(real, real2)), false).findings).join() === "P2");
  const noKey = (src: string) => src.replace(/GATEWAY_KEY_CONFIG_HEX = "[0-9a-f]+"/, 'GATEWAY_KEY_CONFIG_HEX = ""');
  t("an empty gateway key fails with --release (P4) and passes without", codes(check(files(noKey(swift(real, real2)), kt(real, real2)), true).findings).join() === "P4" && check(files(noKey(swift(real, real2)), kt(real, real2)), false).findings.length === 0);
  t("a missing gateway key declaration fails (P4) even without --release", codes(check(files(swift(real, real2).replace(/private let GATEWAY_KEY_CONFIG_HEX = "[0-9a-f]+"\n/, ""), kt(real, real2)), false).findings).join() === "P4");
  t("a real hash made of one repeated pair is not a placeholder", !isPlaceholder("ABABABABABABABABABABABABABABABABABABABABABA="));
  t("a placeholder is", isPlaceholder(A) && isPlaceholder(B));
  t("the real source files carry pins the detector can see", (() => {
    const root = resolve(import.meta.dir, "..");
    const r = check(Object.fromEntries(PIN_FILES.map((f) => [f, readFileSync(join(root, f), "utf8")])), false);
    return r.findings.length === 0 && r.scanned >= 4;
  })());

  failures.forEach((f) => console.error(`FAIL ${f}`));
  console.log(failures.length ? `SELF-TEST FAIL (${failures.length})` : "SELF-TEST PASS — each rule goes red on an input broken in its way");
  return failures.length ? 1 : 0;
}

function main(argv: string[]): number {
  if (argv.includes("--self-test")) return selfTest();
  const release = argv.includes("--release");
  const root = resolve(import.meta.dir, "..");
  const files = Object.fromEntries(PIN_FILES.map((f) => [f, readFileSync(join(root, f), "utf8")]));
  const { findings, scanned, placeholders } = check(files, release);
  console.log(`scanned: ${scanned} pins in ${PIN_FILES.length} files (${placeholders} placeholder)`);
  for (const f of findings) console.error(`FAIL ${f.file}: ${f.message}`);
  if (findings.length) return 1;
  console.log(
    release
      ? "check-release-pins: ok — no placeholder pin in a release build"
      : `check-release-pins: ok — ${placeholders} placeholder pin(s) remain; a --release build will refuse them`,
  );
  return 0;
}

if (import.meta.main) process.exit(main(process.argv.slice(2)));
