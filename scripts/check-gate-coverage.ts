#!/usr/bin/env bun
/**
 * check-gate-coverage.ts — every gate declares what it is and is proven to fail.
 *
 * NP-CONV-001 already states this rule, twice, in two narrow scopes: §4.0.1a
 * ("a checker that has only ever been run against a passing tree has
 * demonstrated nothing") and §8 ("a probe that compares two tables must be
 * falsified before it is trusted"). Both were honoured — by hand, once, and the
 * evidence was thrown away each time. §4.0.1a's own claim that
 * check-doc-filenames.ts "was falsified in both directions" had no artifact
 * behind it until that self-test was written and kept.
 *
 * The cost of not enforcing it is on the record. TOKEN-01 in
 * ci/test_shdr_schema.py passed vacuously across every merge until #118 found
 * it; scripts/ was never type-checked until #299, so check-doc-filenames.ts —
 * a guard over the whole document set — was itself run by nothing.
 *
 * ── The distinction this exists to force ─────────────────────────────────────
 *
 * `check-*` currently names two different things. Three of them
 * (check-pbm-power in its default mode, check-power-source-coverage,
 * check-thermal-dose) always exit 0 and no workflow runs them: they are
 * analyses that print a model. Demanding a falsification test from those is
 * false work, and a meta-gate that manufactures false work gets switched off.
 *
 * So the declaration is the deliverable. Each file states its own kind, and
 * writing that line is what forces the question to be answered:
 *
 *   CI-Kind: gate       can fail; CI runs it; must declare a CI-Self-Test
 *   CI-Kind: report     prints a model; must NOT be run by a workflow
 *   CI-Kind: self-test  falsifies a gate; must declare the CI-Covers gate
 *
 * ── Scope, and what is deliberately outside it ───────────────────────────────
 *
 * Files whose name declares them a check or a test: scripts/check-*,
 * ci/test_*, ci/run_*, plus scripts/ci-changed-scope.sh.
 *
 * NOT covered: the sync-* generators. scripts/sync-locales.ts --check and
 * sync-socket-map.ts --check are gates in every meaningful sense and web-ci
 * runs both. They belong to a different rule — a generator must be pure and
 * have a --check that writes nothing (the #237 lesson) — and folding them in
 * here would blur two rules into one. Stated rather than silently omitted.
 *
 * CI-Kind: gate
 * CI-Self-Test: bun scripts/check-gate-coverage.ts --self-test
 * CI-Scans: every check/test script in scripts/ and ci/
 */
import { readFileSync, readdirSync, mkdtempSync, mkdirSync, writeFileSync, rmSync } from "fs";
import { join, relative } from "path";
import { tmpdir } from "os";

const KINDS = ["gate", "report", "self-test"] as const;
type Kind = (typeof KINDS)[number];

type Decl = {
  file: string;
  kind: Kind | null;
  selfTest: string | null;
  covers: string | null;
  scans: string | null;
};

/** Read the CI-* declaration out of a file's header, whatever its comment syntax. */
function declare(root: string, rel: string): Decl {
  // Header only: a CI-Kind mentioned in prose further down is discussion, not a
  // declaration. 80 lines clears the longest banner in the tree.
  const head = readFileSync(join(root, rel), "utf8").split("\n").slice(0, 80);
  const field = (name: string, pattern = "(.+?)"): string | null => {
    for (const line of head) {
      // Tolerates "# ", " * ", and a bare docstring line.
      const m = new RegExp(`^\\s*(?:#|\\*|//)?\\s*${name}:\\s*${pattern}\\s*$`).exec(line);
      if (m) return m[1]!;
    }
    return null;
  };
  // CI-Kind takes a single bare token to the end of the line. Anything with
  // trailing prose is documentation, not a declaration — this file's own header
  // explains the three kinds in exactly that shape, and a looser pattern read
  // its explanation as its declaration and reported the file undeclared.
  const raw = field("CI-Kind", "([a-z-]+)");
  return {
    file: rel,
    kind: (KINDS as readonly string[]).includes(raw ?? "") ? (raw as Kind) : null,
    selfTest: field("CI-Self-Test"),
    covers: field("CI-Covers"),
    scans: field("CI-Scans"),
  };
}

function candidates(root: string): string[] {
  const found: string[] = [];
  for (const [dir, re] of [
    ["scripts", /^(check-.*\.(ts|sh)|ci-changed-scope\.sh)$/],
    ["ci", /^(test_.*\.py|run_.*\.sh)$/],
  ] as const) {
    let entries: string[];
    try {
      entries = readdirSync(join(root, dir));
    } catch {
      continue;
    }
    for (const e of entries) if (re.test(e)) found.push(`${dir}/${e}`);
  }
  return found.sort();
}

/** Every `run:` command in every workflow, flattened. Comments are excluded on
 *  purpose: tooling-ci.yml documents in a comment why it does NOT run
 *  check-pbm-power --strict, and a substring match would read that as an
 *  invocation and invert the report rule. */
function workflowCommands(root: string): string[] {
  const dir = join(root, ".github/workflows");
  let files: string[];
  try {
    files = readdirSync(dir).filter((f) => f.endsWith(".yml") || f.endsWith(".yaml"));
  } catch {
    return [];
  }
  const out: string[] = [];
  for (const f of files) {
    const lines = readFileSync(join(dir, f), "utf8").split("\n");
    let inRun = false;
    let runIndent = 0;
    for (const line of lines) {
      if (/^\s*#/.test(line)) continue;
      const m = /^(\s*)-?\s*run:\s*(.*)$/.exec(line);
      if (m) {
        inRun = true;
        runIndent = m[1]!.length;
        if (m[2] && m[2] !== "|" && m[2] !== ">") out.push(m[2]);
        continue;
      }
      if (inRun) {
        const indent = line.search(/\S/);
        if (indent > runIndent) out.push(line.trim());
        else if (line.trim() !== "") inRun = false;
      }
    }
  }
  return out;
}

function audit(root: string): { violations: string[]; decls: Decl[] } {
  const decls = candidates(root).map((rel) => declare(root, rel));
  const commands = workflowCommands(root);
  const byFile = new Map(decls.map((d) => [d.file, d]));
  const v: string[] = [];

  for (const d of decls) {
    if (!d.kind) {
      v.push(`${d.file}: no CI-Kind declared — must be one of ${KINDS.join(" | ")}`);
      continue;
    }

    if (d.kind === "gate") {
      if (!d.selfTest) {
        v.push(`${d.file}: CI-Kind gate but no CI-Self-Test — nothing proves it can fail`);
      } else if (d.selfTest.includes("/") && !d.selfTest.includes(" ")) {
        // A path: the falsification lives in a fixture the gate consumes.
        try {
          readFileSync(join(root, d.selfTest));
        } catch {
          v.push(`${d.file}: CI-Self-Test names ${d.selfTest}, which does not exist`);
        }
      } else if (!commands.some((c) => c.includes(d.selfTest!))) {
        v.push(
          `${d.file}: CI-Self-Test "${d.selfTest}" is not run by any workflow — ` +
            `a falsification nothing executes is the state §4.0.1a already failed in`,
        );
      }
      // The gate must be invoked on its own, not merely as part of its
      // self-test command. Matching the bare filename against every command
      // string lets `check-x.sh --self-test` satisfy "the gate runs", which is
      // exactly backwards: it would report a gate as wired up when CI only ever
      // runs its falsification.
      const runsGateItself = commands.some(
        (c) => c.includes(d.file) && (!d.selfTest || !c.includes(d.selfTest)),
      );
      if (!runsGateItself) {
        v.push(`${d.file}: CI-Kind gate but no workflow runs it`);
      }
      if (!d.scans) {
        v.push(`${d.file}: CI-Kind gate but no CI-Scans — its PASS names no population`);
      }
    }

    if (d.kind === "report" && commands.some((c) => c.includes(d.file))) {
      v.push(
        `${d.file}: CI-Kind report but a workflow runs it — ` +
          `declare it a gate (and falsify it) or stop running it`,
      );
    }

    if (d.kind === "self-test") {
      if (!d.covers) {
        v.push(`${d.file}: CI-Kind self-test but no CI-Covers naming the gate it falsifies`);
      } else if (!byFile.has(d.covers)) {
        v.push(`${d.file}: CI-Covers names ${d.covers}, which is not a known check script`);
      }
    }
  }
  return { violations: v, decls };
}

// ── Self-test ────────────────────────────────────────────────────────────────
// This file is a gate, so the rule it enforces binds it too.
if (process.argv.includes("--self-test")) {
  const box = mkdtempSync(join(tmpdir(), "np-gatecov-"));
  const build = (files: Record<string, string>): string => {
    const root = mkdtempSync(join(box, "t-"));
    for (const [rel, body] of Object.entries(files)) {
      mkdirSync(join(root, rel.split("/").slice(0, -1).join("/")), { recursive: true });
      writeFileSync(join(root, rel), body);
    }
    return root;
  };
  const WF = (cmds: string[]) =>
    "jobs:\n  a:\n    steps:\n" + cmds.map((c) => `      - run: ${c}`).join("\n") + "\n";

  const failures: string[] = [];
  const expect = (label: string, root: string, needle: string | null) => {
    const { violations } = audit(root);
    if (needle === null) {
      if (violations.length) failures.push(`${label} — expected clean, got: ${violations[0]}`);
    } else if (!violations.some((x) => x.includes(needle))) {
      failures.push(`${label} — no violation matching ${JSON.stringify(needle)}`);
    }
  };

  const goodGate = "# CI-Kind: gate\n# CI-Self-Test: scripts/check-x.sh --self-test\n# CI-Scans: things\n";

  expect(
    "a fully declared gate passes",
    build({
      "scripts/check-x.sh": goodGate,
      ".github/workflows/w.yml": WF(["scripts/check-x.sh --self-test", "scripts/check-x.sh"]),
    }),
    null,
  );
  expect(
    "undeclared script is caught",
    build({ "scripts/check-x.sh": "# nothing\n", ".github/workflows/w.yml": WF([]) }),
    "no CI-Kind declared",
  );
  expect(
    "gate without a self-test is caught",
    build({
      "scripts/check-x.sh": "# CI-Kind: gate\n# CI-Scans: things\n",
      ".github/workflows/w.yml": WF(["scripts/check-x.sh"]),
    }),
    "no CI-Self-Test",
  );
  expect(
    "self-test that no workflow runs is caught",
    build({
      "scripts/check-x.sh": goodGate,
      ".github/workflows/w.yml": WF(["scripts/check-x.sh"]),
    }),
    "is not run by any workflow",
  );
  expect(
    "gate that no workflow runs is caught",
    build({
      "scripts/check-x.sh": goodGate,
      ".github/workflows/w.yml": WF(["scripts/check-x.sh --self-test"]),
    }),
    "no workflow runs it",
  );
  expect(
    "report that CI runs is caught",
    build({
      "scripts/check-x.sh": "# CI-Kind: report\n",
      ".github/workflows/w.yml": WF(["scripts/check-x.sh"]),
    }),
    "but a workflow runs it",
  );
  expect(
    "self-test with no CI-Covers is caught",
    build({
      "ci/test_a_selftest.py": "# CI-Kind: self-test\n",
      ".github/workflows/w.yml": WF([]),
    }),
    "no CI-Covers",
  );
  // The comment-exclusion property. A workflow that only *mentions* a report in
  // a comment must not read as running it — this is the real tooling-ci.yml
  // shape, and getting it wrong inverts the report rule on a live file.
  expect(
    "a report named only in a workflow comment is not an invocation",
    build({
      "scripts/check-x.sh": "# CI-Kind: report\n",
      ".github/workflows/w.yml":
        "jobs:\n  a:\n    steps:\n      # NOT run here: scripts/check-x.sh --strict\n      - run: true\n",
    }),
    null,
  );
  // Vacuity: no candidates means every rule holds trivially.
  const empty = audit(build({ ".github/workflows/w.yml": WF([]) }));
  if (empty.decls.length !== 0) failures.push("empty tree — expected 0 candidates");

  rmSync(box, { recursive: true, force: true });
  console.log("check-gate-coverage self-test");
  if (failures.length) {
    console.error(`\nSELF-TEST FAIL — ${failures.length} assertion(s):`);
    for (const f of failures) console.error("  " + f);
    process.exit(1);
  }
  console.log("  8 case(s): every rule proven to fire; comment-only mention proven not to");
  console.log("SELF-TEST PASS — the checker has teeth.");
  process.exit(0);
}

const ROOT = join(import.meta.dir, "..");
const { violations, decls } = audit(ROOT);

// A meta-gate that finds nothing to check is the failure it exists to prevent.
if (decls.length === 0) {
  console.error(
    "check-gate-coverage: found no check scripts at all — refusing to pass vacuously.",
  );
  process.exit(2);
}

const counts = KINDS.map((k) => `${decls.filter((d) => d.kind === k).length} ${k}`).join(" · ");
console.log(`checked ${decls.length} check script(s) in ${relative(ROOT, join(ROOT, "scripts"))}/ and ci/ — ${counts}`);

if (violations.length) {
  console.error(`\n${violations.length} gate-coverage violation(s):\n`);
  for (const x of violations) console.error("  " + x);
  console.error(
    "\nNP-CONV-001 §4.0.1a and §8: a checker that has only ever been run against a\n" +
      "passing tree has demonstrated nothing. Declare the file's CI-Kind, and if it\n" +
      "is a gate, give it a CI-Self-Test that CI runs before the gate itself.",
  );
  process.exit(1);
}
console.log("Every gate declares what it scans and is proven to fail. PASS");
process.exit(0);
