#!/usr/bin/env bun
/**
 * check-shared-placement.ts — a file two apps use lives in app/NeurOneShared/.
 *
 * CLAUDE.md §20. A file whose content is common to more than one app (iOS, watchOS, Android,
 * Windows, web) is placed in app/NeurOneShared/ — directly or in a subdirectory of it — and never
 * in one app's directory, where the others reach across into a sibling's tree. The case that
 * motivated the rule: hub-descriptor-golden.json lived under app/android/core/src/test/resources/
 * while the iOS and Windows tests read it from there.
 *
 * Two mechanical checks over every tracked file under app/:
 *
 *   1. REACH-ACROSS. A file under app/<A>/ names a non-code file under app/<B>/ (B != A), as
 *      `app/<B>/…` or `../<B>/…`. "Non-code" is the data and resource extensions in DATA_EXT:
 *      source files (.swift .kt .cs .ts) are the app's own code, and a comment mentioning one is
 *      prose. Mentions inside app/<B>/ itself are fine.
 *   2. DUPLICATE. Two tracked files under different apps have identical, non-trivial content.
 *      Identical content in two places is a shared file stored twice.
 *
 * A third and fourth check cover the other half of the rule (CLAUDE.md §20, common/): a file
 * shared by an app and a NON-app build artifact (simulator/, firmware/) lives in common/ at the
 * repository root, never in an app's tree and never in app/NeurOneShared/.
 *
 *   3. NON-APP REACH-IN. A file under simulator/ or firmware/ imports, requires or #includes
 *      something under app/. Only module specifiers and #include paths count; a comment
 *      mentioning an app path is prose.
 *   4. COMMON REACH-OUT. A file under common/ imports something outside common/. common/ is the
 *      lowest layer: if it needed an app, the shared piece was cut in the wrong place.
 *
 * Out of scope, stated rather than omitted: references from outside app/ in CI workflows, scripts/
 * and docs/ — a workflow is not an app and scripts/ makes build artifacts without being one — and
 * files that are the same only by build-tool convention (EXEMPT_NAMES). A name there is a decision,
 * so it needs a reason beside it.
 *
 * Usage: bun scripts/check-shared-placement.ts [--self-test]
 *
 * CI-Kind: gate
 * CI-Self-Test: bun scripts/check-shared-placement.ts --self-test
 * CI-Scans: every tracked file under app/ for data files shared across apps outside app/NeurOneShared; simulator/, firmware/ and common/ for imports that cross the app/common boundary
 * CI-Scan-Paths: app/** common/** simulator/** firmware/** scripts/check-shared-placement.ts
 */
import { execFileSync } from "child_process";
import { createHash } from "crypto";
import { readFileSync } from "fs";
import { basename, dirname, join, resolve } from "path";

export const APP_DIRS = ["ios", "watchos", "macos", "android", "windows", "web"];
export const SHARED = "NeurOneShared";
const DATA_EXT = ["json", "xcstrings", "plist", "xcprivacy", "entitlements", "xml", "npps", "csv", "txt", "bin", "properties", "strings"];
// Same-name, same-content files that each toolchain demands in its own tree.
const EXEMPT_NAMES = new Set([
  "Info.plist",            // per-target Xcode requirement
  ".gitignore", ".gitattributes", ".DS_Store",
  "gradle-wrapper.jar", "gradle-wrapper.properties", "gradlew", "gradlew.bat", // Gradle wrapper
  "package-lock.json", "bun.lock", // per-package lockfiles
]);
const MIN_DUP_BYTES = 64;
// A re-export shim ("@_exported import NeurOneShared") is the sanctioned way an app keeps a
// file at its old path once the source has moved into NeurOneShared.
const SHIM_MARK = "@_exported import NeurOneShared";
// Project specs (project.yml, build.gradle.kts, *.csproj) and docs are per-app by nature, so
// .yml/.yaml/.md are not in DATA_EXT: a mention of one in a comment is prose.

export type Finding = { file: string; message: string };

export const COMMON = "common";
const NON_APP_DIRS = ["simulator", "firmware"];
const CODE_EXT = /\.(ts|tsx|js|mjs|cjs|c|h|cmake)$|(^|\/)CMakeLists\.txt$/;
// A module specifier or #include path — never a comment's prose.
const specRe = /(?:\bfrom|\bimport\s*\(|\bimport|\brequire\s*\(|#\s*include)\s*['"<]([^'">\n]+)['">]/g;
const APP_SPEC = new RegExp(`(?:^|/)app/(?:${[...APP_DIRS, SHARED].join("|")})(?:/|$)`);
const topOf = (p: string): string => p.split("/")[0];

const appOf = (p: string): string | null => {
  const m = /^app\/([^/]+)\//.exec(p);
  return m && APP_DIRS.includes(m[1]) ? m[1] : null;
};

export function check(files: Map<string, string>): Finding[] {
  const out: Finding[] = [];
  const refRe = new RegExp(`(?:app/|\\.\\./)(${APP_DIRS.join("|")})/([A-Za-z0-9_./@+-]+?\\.(?:${DATA_EXT.join("|")}))(?![A-Za-z0-9_])`, "g");
  const byHash = new Map<string, string[]>();
  for (const [path, text] of files) {
    const a = appOf(path);
    if (!a) continue;
    for (const m of text.matchAll(refRe)) {
      if (m[1] !== a) {
        out.push({ file: path, message: `reaches into app/${m[1]}/ for ${m[2]} — a file used by more than one app belongs in app/${SHARED}/` });
      }
    }
    if (text.length >= MIN_DUP_BYTES && !EXEMPT_NAMES.has(basename(path)) && !text.includes(SHIM_MARK)) {
      const h = createHash("sha256").update(text).digest("hex");
      byHash.set(h, [...(byHash.get(h) ?? []), path]);
    }
  }
  for (const [path, text] of files) {
    const top = topOf(path);
    if (!CODE_EXT.test(path)) continue;
    if (NON_APP_DIRS.includes(top)) {
      for (const m of text.matchAll(specRe)) {
        if (APP_SPEC.test(m[1])) {
          out.push({ file: path, message: `imports ${m[1]} from an app — a file shared with a non-app artifact belongs in ${COMMON}/` });
        }
      }
    } else if (top === COMMON) {
      for (const m of text.matchAll(specRe)) {
        const spec = m[1];
        if (!spec.startsWith(".")) continue; // a package name is not a path
        const resolved = join(dirname(path), spec).replace(/\\/g, "/");
        if (resolved === ".." || resolved.startsWith("../") || !resolved.startsWith(`${COMMON}/`)) {
          out.push({ file: path, message: `imports ${spec}, which is outside ${COMMON}/ — ${COMMON}/ may not depend on an artifact` });
        }
      }
    }
  }
  for (const paths of byHash.values()) {
    if (new Set(paths.map(appOf)).size > 1) {
      out.push({ file: paths[0], message: `identical content in ${paths.join(", ")} — move one copy to app/${SHARED}/ and delete the rest` });
    }
  }
  return out;
}

function selfTest(): void {
  const base = new Map<string, string>([
    ["app/android/core/Foo.kt", "// reads app/NeurOneShared/TestData/golden.json\n"],
    ["app/ios/Foo.swift", "// see ../NeurOneShared/Resources/x.xcstrings and app/ios/own.json\n"],
  ]);
  const expect = (name: string, files: Map<string, string>, want: number) => {
    const got = check(files).length;
    if (got !== want) { console.error(`self-test FAILED: ${name}: expected ${want} finding(s), got ${got}`); process.exit(1); }
  };
  expect("clean tree passes", base, 0);
  expect("reach-across fails", new Map([...base, ["app/windows/T.cs", "// app/android/core/src/test/resources/golden.json"]]), 1);
  expect("relative reach-across fails", new Map([...base, ["app/watchos/project.yml", "path: ../ios/NeurOne/Localizable.xcstrings"]]), 1);
  expect("own-app reference passes", new Map([...base, ["app/android/a.kt", "// app/android/x/y.json"]]), 0);
  expect("reference to code passes", new Map([...base, ["app/ios/b.swift", "// app/web/src/lib/hubCompiler.ts"]]), 0);
  const body = "x".repeat(200);
  const shim = "// moved\n@_exported import NeurOneShared\n" + body;
  expect("shim duplicate passes", new Map([...base, ["app/ios/S.swift", shim], ["app/watchos/S.swift", shim]]), 0);
  expect("duplicate fails", new Map([...base, ["app/ios/d.json", body], ["app/android/d.json", body]]), 1);
  expect("exempt duplicate passes", new Map([...base, ["app/ios/Info.plist", body], ["app/watchos/Info.plist", body]]), 0);
  expect("same app duplicate passes", new Map([...base, ["app/ios/d1.json", body], ["app/ios/d2.json", body]]), 0);
  expect("simulator importing an app fails", new Map([...base, ["simulator/src/r.ts", "import { x } from '../../app/web/src/lib/nppsParser';"]]), 1);
  expect("simulator importing common passes", new Map([...base, ["simulator/src/r.ts", "import { x } from '../../common/lib/nppsParser';"]]), 0);
  expect("simulator comment naming an app passes", new Map([...base, ["simulator/src/r.ts", "// bundles app/web/src/lib/nppsParser.ts\n"]]), 0);
  expect("firmware #include of an app fails", new Map([...base, ["firmware/x/a.c", '#include "../../app/NeurOneShared/Foo.h"']]), 1);
  expect("common importing within common passes", new Map([...base, ["common/lib/a.ts", "import { y } from '../types/protocol';"]]), 0);
  expect("common importing a package passes", new Map([...base, ["common/lib/a.ts", "import { z } from 'react';"]]), 0);
  expect("common reaching out fails", new Map([...base, ["common/lib/a.ts", "import { y } from '../../app/web/src/lib/b';"]]), 1);
  expect("common climbing past the repo root fails", new Map([...base, ["common/a.ts", "import { y } from '../../../b';"]]), 1);
  console.log("check-shared-placement self-test: ok");
}

if (import.meta.main) {
  if (process.argv.includes("--self-test")) { selfTest(); process.exit(0); }
  const root = resolve(import.meta.dir, "..");
  const tracked = execFileSync("git", ["ls-files", "-z", "app", COMMON, ...NON_APP_DIRS], { cwd: root, encoding: "utf8" }).split("\0").filter(Boolean);
  const files = new Map<string, string>();
  for (const p of tracked) {
    if (/\.(png|jpg|jar|ttf|otf|ico|a|so|dylib)$/i.test(p)) continue;
    try { files.set(p, readFileSync(join(root, p), "utf8")); } catch { /* deleted or unreadable */ }
  }
  const findings = check(files);
  console.log(`scanned: ${files.size} file(s) under app/, ${COMMON}/ and ${NON_APP_DIRS.map((d) => `${d}/`).join(", ")}`);
  for (const f of findings) console.error(`${f.file}: ${f.message}`);
  if (findings.length) {
    console.error(`\n${findings.length} finding(s). CLAUDE.md §20: files shared by apps live in app/${SHARED}/; files shared with a non-app artifact live in ${COMMON}/.`);
    process.exit(1);
  }
  console.log("check-shared-placement: ok");
}
