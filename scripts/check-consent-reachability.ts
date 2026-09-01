#!/usr/bin/env bun
/**
 * check-consent-reachability.ts — a consent method with no caller is not a feature.
 *
 * CLAUDE.md Rev 37 records the defect this exists to prevent:
 * `withdrawBlanketResearchConsent()` was "correct and tested on both platforms,
 * but nothing on iOS called it from the UI" — so turning blanket consent off in
 * Research Preferences committed through `updateResearchConsent()` and skipped
 * the analytics teardown, while Android routed it correctly.
 *
 * Tests proved the method worked. Nothing proved it ran. That gap is invisible
 * in the test suite (green), invisible in the diff (nothing is wrong with the
 * method), and invisible to a type checker (it compiles). It is visible here.
 *
 * ── What this checks ─────────────────────────────────────────────────────────
 *
 *   1. The two ConsentStore surfaces are IDENTICAL. §6.0 makes the consent model
 *      a cross-platform invariant, so a method on one platform and not the other
 *      is a divergence regardless of who calls it.
 *   2. Every method carries a declared reachability, and the declaration holds.
 *      An undeclared method is a violation: adding one must be a decision, not
 *      an omission.
 *
 * ── Why declarations rather than "everything needs a UI caller" ──────────────
 *
 * Because that rule is false here, and a gate that manufactures false work gets
 * switched off (the lesson check-gate-coverage.ts is built around). Of the 11
 * methods, three legitimately have no UI caller: one is called only from inside
 * the store, one is superseded by a wholesale-state path, and one belongs to an
 * unimplemented workflow. Only a per-method declaration can tell those apart
 * from the #277 defect, which looks identical from outside.
 *
 * ── Waivers are per-platform, and deliberately narrow ────────────────────────
 *
 * A `ui` method waived on one platform is still ENFORCED on the other. That
 * matters: the Android consent dashboard does not exist yet (OI-CONSENT-01), and
 * classing those methods "pending" wholesale would stop the gate noticing if the
 * iOS path — which does exist — were removed. Waivers are printed on every run,
 * never silently tolerated, and each must name an open item.
 *
 * CI-Kind: gate
 * CI-Self-Test: bun scripts/check-consent-reachability.ts --self-test
 * CI-Scans: every public ConsentStore method on iOS and Android, and its callers
 * CI-Scan-Paths: app/ios/** app/android/**
 */
import { readFileSync, readdirSync, statSync, mkdtempSync, mkdirSync, writeFileSync, rmSync } from "fs";
import { join } from "path";
import { tmpdir } from "os";

type Platform = "ios" | "android";

const STORE: Record<Platform, string> = {
  ios: "app/ios/NeurOne/Consent/ConsentStore.swift",
  android: "app/android/core/src/main/kotlin/life/neurone/core/consent/ConsentStore.kt",
};
const SOURCE_ROOT: Record<Platform, string> = { ios: "app/ios", android: "app/android" };
const EXT: Record<Platform, string> = { ios: ".swift", android: ".kt" };

/** Declaration syntax for `func name(` / `fun name(` at one indent level. */
const DEFN: Record<Platform, RegExp> = {
  ios: /^\s{4}(?:public\s+)?func\s+(\w+)\s*\(/gm,
  android: /^\s{4}(?:public\s+)?fun\s+(\w+)\s*\(/gm,
};

type Reach =
  /** Reached from the UI. Enforced per platform unless waived there. */
  | { kind: "ui"; waived?: Partial<Record<Platform, string>>; note?: string }
  /** Reached only from inside ConsentStore itself. */
  | { kind: "internal"; note: string }
  /** A different path supersedes it; that path must exist and be reachable. */
  | { kind: "superseded-by"; by: string; note: string }
  /** No caller expected yet. Must name the open item that will change that. */
  | { kind: "pending"; oi: string; note: string };

/**
 * The declared consent surface. Every entry is a decision about how a
 * privileged method is meant to be reached — the point of the file.
 */
const SURFACE: Record<string, Reach> = {
  // ── Clinician access (§6.1). iOS has ConsentDashboardView; Android has no
  //    consent dashboard at all, which is OI-CONSENT-01.
  grantClinicianAccess: { kind: "ui", waived: { android: "OI-CONSENT-01" } },
  revokeClinicianAccess: { kind: "ui", waived: { android: "OI-CONSENT-01" } },
  expandClinicianAccess: {
    kind: "pending",
    oi: "OI-CONSENT-02",
    note: "§6.1's expansion workflow (differential consent document, approve/deny/ask) is unimplemented on both platforms",
  },

  // ── Research consent (§6.2). The Rev 37 path, and the one that broke.
  updateResearchConsent: {
    kind: "ui",
    note: "the S2 commit path; also carries the §6.2.5 blanket true→false teardown guard",
  },
  withdrawBlanketResearchConsent: {
    kind: "ui",
    note:
      "the CLAUDE.md Rev 37 defect: correct and tested on both platforms, unreachable from the iOS UI. " +
      "Reachable on BOTH now — iOS ConsentDashboardView, Android Screens.kt — so it is enforced on both " +
      "with no waiver. Android routed it correctly even when iOS did not, which is why the Rev 37 " +
      "regression was one-sided",
  },
  revokeResearchAnalytics: {
    kind: "internal",
    note: "§6.0 teardown. Reached from withdrawBlanketResearchConsent() and from updateResearchConsent()'s true→false transition (§6.2.5); a direct UI opt-out toggle is optional, not required",
  },
  setCategoryConsent: {
    kind: "superseded-by",
    by: "updateResearchConsent",
    note: "both UIs edit ResearchConsentState.categoryConsents wholesale and commit through updateResearchConsent, so the per-category setter has no caller by design, not by omission",
  },

  // ── Study invitations (§6.3 per-project workflow).
  addInvitation: {
    kind: "pending",
    oi: "OI-CONSENT-03",
    note: "invitations are ingested from the study-descriptor sync layer, which does not exist yet; no UI caller is expected",
  },
  acceptInvitation: { kind: "ui", waived: { android: "OI-CONSENT-01" } },
  declineInvitation: { kind: "ui", waived: { android: "OI-CONSENT-01" } },
  withdrawFromStudy: { kind: "ui", waived: { android: "OI-CONSENT-01" } },
};

function definedMethods(root: string, p: Platform): string[] {
  const src = readFileSync(join(root, STORE[p]), "utf8");
  return [...src.matchAll(DEFN[p])].map((m) => m[1]!).sort();
}

function* walk(dir: string): Generator<string> {
  let entries: string[];
  try {
    entries = readdirSync(dir);
  } catch {
    return;
  }
  for (const e of entries) {
    if (e === "build" || e === ".build" || e === "node_modules") continue;
    const full = join(dir, e);
    let st;
    try {
      st = statSync(full);
    } catch {
      continue;
    }
    if (st.isDirectory()) yield* walk(full);
    else yield full;
  }
}

/** Files that could call the store: same platform, not a test, not the store. */
function callerFiles(root: string, p: Platform): string[] {
  const out: string[] = [];
  for (const f of walk(join(root, SOURCE_ROOT[p]))) {
    if (!f.endsWith(EXT[p])) continue;
    const rel = f.slice(root.length + 1);
    if (rel === STORE[p]) continue;
    // Test sources prove a method works; they do not prove anything reaches it.
    if (/(^|\/)(test|tests)\//i.test(rel) || /Tests?\.(swift|kt)$/.test(rel)) continue;
    out.push(rel);
  }
  return out;
}

type Audit = {
  violations: string[];
  waivers: string[];
  methods: string[];
  callerFileCount: number;
};

function audit(root: string): Audit {
  const violations: string[] = [];
  const waivers: string[] = [];

  const defined: Record<Platform, string[]> = {
    ios: definedMethods(root, "ios"),
    android: definedMethods(root, "android"),
  };

  // 1. Surface parity. §6.0 makes the consent model cross-platform.
  for (const [a, b] of [["ios", "android"], ["android", "ios"]] as const) {
    for (const m of defined[a]) {
      if (!defined[b].includes(m)) {
        violations.push(`${m}: defined on ${a} but not on ${b} — the consent surface has diverged`);
      }
    }
  }

  const all = [...new Set([...defined.ios, ...defined.android])].sort();

  // 2. Declaration completeness, both directions.
  for (const m of all) {
    if (!SURFACE[m]) {
      violations.push(
        `${m}: defined but not declared in SURFACE — say how it is meant to be reached ` +
          `(ui | internal | superseded-by | pending)`,
      );
    }
  }
  for (const m of Object.keys(SURFACE)) {
    if (!all.includes(m)) {
      violations.push(`${m}: declared in SURFACE but no longer defined on either platform — stale entry`);
    }
  }

  // 3. The declarations must hold.
  const callers: Record<Platform, { rel: string; text: string }[]> = {
    ios: callerFiles(root, "ios").map((rel) => ({ rel, text: readFileSync(join(root, rel), "utf8") })),
    android: callerFiles(root, "android").map((rel) => ({ rel, text: readFileSync(join(root, rel), "utf8") })),
  };
  const storeText: Record<Platform, string> = {
    ios: readFileSync(join(root, STORE.ios), "utf8"),
    android: readFileSync(join(root, STORE.android), "utf8"),
  };

  const externalCallers = (m: string, p: Platform): string[] =>
    callers[p].filter((f) => f.text.includes(`.${m}(`)).map((f) => f.rel);

  for (const m of all) {
    const d = SURFACE[m];
    if (!d) continue;

    if (d.kind === "ui") {
      for (const p of ["ios", "android"] as const) {
        if (!defined[p].includes(m)) continue;
        const waivedOI = d.waived?.[p];
        const found = externalCallers(m, p);
        if (waivedOI) {
          if (!/^OI-[A-Z0-9]+-\d+$/.test(waivedOI)) {
            violations.push(`${m} [${p}]: waiver must name an open item, got "${waivedOI}"`);
          } else if (found.length > 0) {
            violations.push(
              `${m} [${p}]: waived under ${waivedOI} but a caller exists (${found[0]}) — ` +
                `remove the waiver, the gap it records is closed`,
            );
          } else {
            waivers.push(`${m} [${p}] — no UI caller, waived under ${waivedOI}`);
          }
          continue;
        }
        if (found.length === 0) {
          violations.push(
            `${m} [${p}]: declared ui but no non-test caller outside ConsentStore — ` +
              `this is the CLAUDE.md Rev 37 shape: tested, correct, unreachable`,
          );
        }
      }
    }

    if (d.kind === "internal") {
      for (const p of ["ios", "android"] as const) {
        if (!defined[p].includes(m)) continue;
        // Its own definition line is not a call; require a call elsewhere in the store.
        const calls = (storeText[p].match(new RegExp(`\\b${m}\\s*\\(`, "g")) ?? []).length;
        const defs = (storeText[p].match(new RegExp(`(?:func|fun)\\s+${m}\\s*\\(`, "g")) ?? []).length;
        if (calls - defs < 1) {
          violations.push(
            `${m} [${p}]: declared internal but nothing inside ConsentStore calls it`,
          );
        }
      }
    }

    if (d.kind === "superseded-by") {
      if (!SURFACE[d.by]) {
        violations.push(`${m}: superseded-by names ${d.by}, which is not part of the declared surface`);
      } else if (SURFACE[d.by]!.kind !== "ui") {
        violations.push(
          `${m}: superseded-by names ${d.by}, which is not itself a ui path — ` +
            `the supersession claim would leave neither reachable`,
        );
      }
    }

    if (d.kind === "pending" && !/^OI-[A-Z0-9]+-\d+$/.test(d.oi)) {
      violations.push(`${m}: pending must name an open item, got "${d.oi}"`);
    }
  }

  return {
    violations,
    waivers,
    methods: all,
    callerFileCount: callers.ios.length + callers.android.length,
  };
}

// ── Self-test ────────────────────────────────────────────────────────────────
if (process.argv.includes("--self-test")) {
  const box = mkdtempSync(join(tmpdir(), "np-consentreach-"));

  const build = (
    iosStore: string,
    androidStore: string,
    extra: Record<string, string> = {},
  ): string => {
    const root = mkdtempSync(join(box, "t-"));
    const put = (rel: string, body: string) => {
      mkdirSync(join(root, rel.split("/").slice(0, -1).join("/")), { recursive: true });
      writeFileSync(join(root, rel), body);
    };
    put(STORE.ios, iosStore);
    put(STORE.android, androidStore);
    for (const [rel, body] of Object.entries(extra)) put(rel, body);
    return root;
  };

  // Fixture stores carry only the methods under test, so the real SURFACE table
  // reports every other entry as stale. Those are expected here and filtered out.
  const relevant = (v: string[]) => v.filter((x) => !x.includes("stale entry"));

  const iosFn = (n: string, body = "") => `    func ${n}() {\n${body}    }\n`;
  const ktFn = (n: string, body = "") => `    fun ${n}() {\n${body}    }\n`;

  const failures: string[] = [];
  const expect = (label: string, root: string, needle: string | null) => {
    const v = relevant(audit(root).violations);
    if (needle === null) {
      if (v.length) failures.push(`${label} — expected clean, got: ${v[0]}`);
    } else if (!v.some((x) => x.includes(needle))) {
      failures.push(`${label} — no violation matching ${JSON.stringify(needle)}`);
    }
  };

  // A ui method with a caller on the unwaived platform, waived on the other.
  expect(
    "ui method with an iOS caller and an Android waiver is clean",
    build(iosFn("withdrawFromStudy"), ktFn("withdrawFromStudy"), {
      "app/ios/NeurOne/Views/V.swift": "store.withdrawFromStudy()\n",
    }),
    null,
  );

  // The Rev 37 defect itself: defined, tested, and unreachable.
  expect(
    "ui method with no caller on an unwaived platform is caught",
    build(iosFn("updateResearchConsent"), ktFn("updateResearchConsent"), {
      "app/ios/NeurOne/Views/V.swift": "// nothing calls it\n",
      "app/android/app/src/main/kotlin/S.kt": "store.updateResearchConsent()\n",
    }),
    "this is the CLAUDE.md Rev 37 shape",
  );

  // A test-only caller must not satisfy reachability — the whole point.
  expect(
    "a test-only caller does not count as reachable",
    build(iosFn("updateResearchConsent"), ktFn("updateResearchConsent"), {
      "app/ios/NeurOneTests/ConsentStoreTests.swift": "store.updateResearchConsent()\n",
      "app/android/app/src/main/kotlin/S.kt": "store.updateResearchConsent()\n",
    }),
    "this is the CLAUDE.md Rev 37 shape",
  );

  // Surface parity.
  expect(
    "a method on one platform only is caught",
    build(iosFn("withdrawFromStudy"), ktFn("somethingElse")),
    "the consent surface has diverged",
  );

  // An undeclared method must force a decision.
  expect(
    "an undeclared method is caught",
    build(iosFn("brandNewConsentMethod"), ktFn("brandNewConsentMethod")),
    "defined but not declared in SURFACE",
  );

  // A waiver whose gap has closed must be removed, or it rots into a lie.
  expect(
    "a waiver with a live caller is caught",
    build(iosFn("withdrawFromStudy"), ktFn("withdrawFromStudy"), {
      "app/ios/NeurOne/Views/V.swift": "store.withdrawFromStudy()\n",
      "app/android/app/src/main/kotlin/S.kt": "store.withdrawFromStudy()\n",
    }),
    "the gap it records is closed",
  );

  // internal: declared reachable from inside the store, and it must be.
  expect(
    "an internal method nothing in the store calls is caught",
    build(
      iosFn("revokeResearchAnalytics"),
      ktFn("revokeResearchAnalytics"),
    ),
    "nothing inside ConsentStore calls it",
  );
  expect(
    "an internal method the store does call is clean",
    build(
      iosFn("revokeResearchAnalytics") + iosFn("withdrawBlanketResearchConsent", "        revokeResearchAnalytics()\n"),
      ktFn("revokeResearchAnalytics") + ktFn("withdrawBlanketResearchConsent", "        revokeResearchAnalytics()\n"),
      {
        // withdrawBlanketResearchConsent is declared `ui` with no waiver, so
        // this fixture must give it a caller on BOTH platforms or it trips that
        // rule instead of exercising the internal one.
        "app/ios/NeurOne/Views/V.swift": "store.withdrawBlanketResearchConsent()\n",
        "app/android/app/src/main/kotlin/S.kt": "store.withdrawBlanketResearchConsent()\n",
      },
    ),
    null,
  );

  rmSync(box, { recursive: true, force: true });
  console.log("check-consent-reachability self-test");
  if (failures.length) {
    console.error(`\nSELF-TEST FAIL — ${failures.length} assertion(s):`);
    for (const f of failures) console.error("  " + f);
    process.exit(1);
  }
  console.log("  8 case(s): the Rev 37 shape, test-only callers, parity, undeclared,");
  console.log("  stale waivers and internal reachability all proven to fire");
  console.log("SELF-TEST PASS — the checker has teeth.");
  process.exit(0);
}

const ROOT = join(import.meta.dir, "..");
const result = audit(ROOT);

if (result.methods.length === 0) {
  console.error("check-consent-reachability: parsed no ConsentStore methods — refusing to pass vacuously.");
  process.exit(2);
}

console.log(
  `scanned: ${result.methods.length} consent method(s) across 2 platform(s), ` +
    `${result.callerFileCount} candidate caller file(s)`,
);

if (result.waivers.length) {
  console.log(`\n${result.waivers.length} waived reachability gap(s) — open, not resolved:`);
  for (const w of result.waivers) console.log("  " + w);
}

if (result.violations.length) {
  console.error(`\n${result.violations.length} consent-reachability violation(s):\n`);
  for (const v of result.violations) console.error("  " + v);
  console.error(
    "\nCLAUDE.md Rev 37: withdrawBlanketResearchConsent() was correct and tested on\n" +
      "both platforms, and nothing on iOS called it. Tests prove a method works;\n" +
      "only a caller proves it runs.",
  );
  process.exit(1);
}
console.log("\nEvery consent method's declared reachability holds. PASS");
process.exit(0);
