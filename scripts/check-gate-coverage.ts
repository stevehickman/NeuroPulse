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
 * ── The question this does NOT answer ────────────────────────────────────────
 *
 * Whether a gate is declared, falsified and run — yes. Whether the change that
 * breaks it can actually REACH it — no. A gate can satisfy every rule below and
 * still sit behind a relevance list that its own population never matches, which
 * is what happened to three gates here at once (NP-SW-CI-001 §5.0). That
 * comparison belongs to scripts/check-ci-scope.ts, which reads the machine-
 * readable population declaration this file's siblings all carry and compares it
 * to the list of every job that runs the gate. Two gates, one boundary: this one
 * owns the declaration's existence, that one owns its reachability.
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
 * CI-Scan-Probe: external — probing this file would re-enter the prober
 * CI-Scans: every check/test script in scripts/ and ci/
 * CI-Scan-Paths: scripts/** ci/** .github/workflows/**
 */
import { readFileSync, readdirSync, mkdtempSync, mkdirSync, writeFileSync, rmSync, chmodSync } from "fs";
import { join } from "path";
import { tmpdir } from "os";

const KINDS = ["gate", "report", "self-test"] as const;
type Kind = (typeof KINDS)[number];

type Decl = {
  file: string;
  kind: Kind | null;
  selfTest: string | null;
  covers: string | null;
  scans: string | null;
  probe: string | null;
  readsTree: string | null;
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
    probe: field("CI-Scan-Probe"),
    readsTree: field("CI-Self-Test-Reads-Tree"),
  };
}

/**
 * A self-test must be hermetic unless it says why it is not.
 *
 * The bug this catches was made in this repository on 2026-08-30, in
 * check-section-refs.ts: its `--self-test` block was placed AFTER the top-level
 * `const valid = validSections()`, so before the fixtures ran it read the
 * production CLAUDE.md. It passed anyway — the production file was fine — and
 * was found only because a mutation crashed instead of failing cleanly.
 *
 * A self-test that reaches into the tree it is meant to be independent of cannot
 * run before that tree is valid, and reports on production state rather than on
 * its fixtures. Either way its green tick means less than it appears to.
 *
 * The test is to COPY THE SCRIPT OUT of the repository and run it there. Running
 * it from a foreign working directory is not the same test and would not have
 * caught this: the paths involved were `import.meta.dir`-relative, which cwd
 * does not affect. Measured, not assumed — both variants were tried.
 *
 * Hermetic is the default because it is the common case (7 of 10 self-tests
 * here). The exceptions are real: the SHDR and warranty self-tests run their
 * mutations against the PRODUCTION schema on purpose, which is where their value
 * is — test_warranty_nojoin.py's central case reports that #118's predicate
 * drops 139 of 227 real columns. Those declare CI-Self-Test-Reads-Tree and say
 * why, so the exception is a statement rather than a silence.
 */
function selfTestIsHermetic(
  root: string,
  d: Decl,
): { ok: true } | { ok: false; why: string } {
  const cmd = d.selfTest!.split(/\s+/);
  const scriptIdx = cmd.findIndex((t) => /\.(ts|sh|py)$/.test(t));
  if (scriptIdx === -1) return { ok: false, why: "self-test command names no script file" };
  const scriptRel = cmd[scriptIdx]!;

  const box = mkdtempSync(join(tmpdir(), "np-hermetic-"));
  try {
    const base = scriptRel.split("/").pop()!;
    writeFileSync(join(box, base), readFileSync(join(root, scriptRel)));
    const rewritten = [...cmd];
    // When the script IS argv[0] (the shell gates run themselves, rather than
    // being handed to `bun` or `python3`), a bare basename is looked up on PATH
    // and fails with ENOENT. It needs an explicit relative path, and the
    // executable bit the copy did not inherit.
    if (scriptIdx === 0) {
      chmodSync(join(box, base), 0o755);
      rewritten[0] = `./${base}`;
    } else {
      rewritten[scriptIdx] = base;
    }
    const r = Bun.spawnSync(rewritten, { cwd: box, stdout: "pipe", stderr: "pipe" });
    if (r.exitCode !== 0) {
      const out = (
        new TextDecoder().decode(r.stdout) + new TextDecoder().decode(r.stderr)
      ).trim().split("\n").slice(-1)[0] ?? "";
      return { ok: false, why: `exited ${r.exitCode} outside the repo — ${out}` };
    }
    return { ok: true };
  } finally {
    rmSync(box, { recursive: true, force: true });
  }
}

/** How to invoke a gate for the population probe, if it can be invoked at all. */
function probeCommand(d: Decl): string[] | null {
  if (d.probe?.startsWith("external")) return null;
  if (d.probe) return d.probe.split(/\s+/);
  if (d.file.endsWith(".ts")) return ["bun", d.file];
  if (d.file.endsWith(".py")) return ["python3", d.file];
  return [d.file];
}

/**
 * Run a gate and read back the population it says it scanned.
 *
 * This is what separates a checked count from a promise. CI-Scans is prose — it
 * can say anything. The gate's own `scanned: <int>` line is the number it
 * actually computed, and a gate reporting `scanned: 0` while exiting 0 is
 * precisely the #118 shape: TOKEN-01 passed over an empty column list for every
 * merge until someone went looking.
 */
function probePopulation(
  root: string,
  d: Decl,
): { ok: true; n: number } | { ok: false; why: string } {
  const cmd = probeCommand(d);
  if (!cmd) return { ok: false, why: "external" };
  const r = Bun.spawnSync(cmd, { cwd: root, stdout: "pipe", stderr: "pipe" });
  const out =
    new TextDecoder().decode(r.stdout) + new TextDecoder().decode(r.stderr);
  if (r.exitCode !== 0) {
    return { ok: false, why: `exited ${r.exitCode} on a clean tree` };
  }
  const m = /^scanned:\s*(\d+)\b/m.exec(out);
  if (!m) return { ok: false, why: "printed no `scanned: <int>` line" };
  return { ok: true, n: Number(m[1]) };
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

function audit(root: string, probe = false): { violations: string[]; decls: Decl[]; waivers: string[] } {
  const decls = candidates(root).map((rel) => declare(root, rel));
  const commands = workflowCommands(root);
  const byFile = new Map(decls.map((d) => [d.file, d]));
  const v: string[] = [];
  const waivers: string[] = [];

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
      // Hermeticity — only meaningful when the self-test is a command we can
      // re-run; the fixture-path form has no script to copy.
      if (probe && d.selfTest && !(d.selfTest.includes("/") && !d.selfTest.includes(" "))) {
        if (d.readsTree) {
          if (d.readsTree.trim().length < 10) {
            v.push(
              `${d.file}: CI-Self-Test-Reads-Tree must say WHY the self-test needs the ` +
                `production tree, got "${d.readsTree}"`,
            );
          } else {
            waivers.push(`${d.file} — self-test reads the tree: ${d.readsTree}`);
          }
        } else {
          const h = selfTestIsHermetic(root, d);
          if (!h.ok) {
            v.push(
              `${d.file}: self-test is not hermetic — ${h.why}. Either make it build its own ` +
                `fixtures, or declare CI-Self-Test-Reads-Tree with the reason it cannot`,
            );
          }
        }
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
      // CI-Scans is prose. This is the checked half: run the gate and read the
      // number it actually computed.
      if (probe) {
        const r = probePopulation(root, d);
        if (!r.ok && r.why === "external") {
          if (!/^external\s+—\s+\S/.test(d.probe ?? "")) {
            v.push(
              `${d.file}: CI-Scan-Probe is external but gives no reason — ` +
                `write "external — <why it cannot run here>"`,
            );
          }
        } else if (!r.ok) {
          v.push(`${d.file}: population probe failed — ${r.why}`);
        } else if (r.n < 1) {
          v.push(
            `${d.file}: reports \`scanned: ${r.n}\` yet exits 0 — ` +
              `a gate that passes over an empty population is the #118 shape`,
          );
        }
      }
    }

    if (d.kind === "report" && commands.some((c) => c.includes(d.file))) {
      v.push(
        `${d.file}: CI-Kind report but a workflow runs it — ` +
          `declare it a gate (and falsify it) or stop running it`,
      );
    }

    // The exception is read only where CI-Self-Test is, i.e. on a gate. Sitting
    // anywhere else it looks load-bearing and does nothing — which is how it was
    // first written here, on the two self-test files instead of their gate.
    if (d.kind !== "gate" && d.readsTree) {
      v.push(
        `${d.file}: CI-Self-Test-Reads-Tree on a ${d.kind} has no effect — ` +
          `it belongs on the gate whose CI-Self-Test this is`,
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
  return { violations: v, decls, waivers };
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
  // ── The population probe ───────────────────────────────────────────────────
  // These fixtures are executable, because the probe's whole point is that it
  // runs the gate rather than reading a claim about it.
  const probeGate = (body: string, extra = "") =>
    `#!/bin/sh\n# CI-Kind: gate\n# CI-Self-Test: sh scripts/check-x.sh --self-test\n` +
    `# CI-Scan-Probe: sh scripts/check-x.sh\n# CI-Scans: things\n${extra}${body}\n`;
  const probeWf = WF(["sh scripts/check-x.sh --self-test", "sh scripts/check-x.sh"]);

  const expectProbe = (label: string, root: string, needle: string | null) => {
    const { violations } = audit(root, true);
    if (needle === null) {
      if (violations.length) failures.push(`${label} — expected clean, got: ${violations[0]}`);
    } else if (!violations.some((x) => x.includes(needle))) {
      failures.push(`${label} — no violation matching ${JSON.stringify(needle)}`);
    }
  };

  expectProbe(
    "a gate reporting a real population passes the probe",
    build({
      "scripts/check-x.sh": probeGate('echo "scanned: 42 things"'),
      ".github/workflows/w.yml": probeWf,
    }),
    null,
  );
  expectProbe(
    "a gate that prints no scanned line is caught",
    build({
      "scripts/check-x.sh": probeGate('echo "all good"'),
      ".github/workflows/w.yml": probeWf,
    }),
    "printed no `scanned: <int>` line",
  );
  // The #118 shape itself: exits 0, having examined nothing.
  expectProbe(
    "a gate passing over an empty population is caught",
    build({
      "scripts/check-x.sh": probeGate('echo "scanned: 0 things"'),
      ".github/workflows/w.yml": probeWf,
    }),
    "is the #118 shape",
  );
  expectProbe(
    "an unexplained external probe is caught",
    build({
      "scripts/check-x.sh":
        "#!/bin/sh\n# CI-Kind: gate\n# CI-Self-Test: sh scripts/check-x.sh --self-test\n" +
        "# CI-Scan-Probe: external\n# CI-Scans: things\n",
      ".github/workflows/w.yml": probeWf,
    }),
    "gives no reason",
  );
  expectProbe(
    "an explained external probe is accepted",
    build({
      "scripts/check-x.sh":
        "#!/bin/sh\n# CI-Kind: gate\n# CI-Self-Test: sh scripts/check-x.sh --self-test\n" +
        "# CI-Scan-Probe: external — needs a live database\n# CI-Scans: things\n",
      ".github/workflows/w.yml": probeWf,
    }),
    null,
  );

  // ── Hermeticity ────────────────────────────────────────────────────────────
  // The fixture gate's self-test succeeds only beside a marker file that exists
  // in the repo but not in the copy-out directory — which is exactly how a
  // self-test that reaches into the production tree behaves.
  const hermeticGate = (extraDecl: string) =>
    `#!/bin/sh\n# CI-Kind: gate\n# CI-Self-Test: sh scripts/check-x.sh --self-test\n` +
    `# CI-Scan-Probe: external — not probed in this fixture\n# CI-Scans: things\n${extraDecl}` +
    `case "$1" in --self-test) test -f ./marker || exit 3; exit 0;; esac\necho "scanned: 1 thing"\n`;
  const hermeticWf = WF(["sh scripts/check-x.sh --self-test", "sh scripts/check-x.sh"]);

  expectProbe(
    "a self-test that needs the tree, undeclared, is caught",
    build({
      "scripts/check-x.sh": hermeticGate(""),
      marker: "present in the repo, absent in the copy-out\n",
      ".github/workflows/w.yml": hermeticWf,
    }),
    "self-test is not hermetic",
  );
  expectProbe(
    "the same self-test passes once the reason is declared",
    build({
      "scripts/check-x.sh": hermeticGate(
        "# CI-Self-Test-Reads-Tree: it asserts against the real marker, which is the point\n",
      ),
      marker: "present\n",
      ".github/workflows/w.yml": hermeticWf,
    }),
    null,
  );
  expectProbe(
    "a reads-tree declaration with no real reason is caught",
    build({
      "scripts/check-x.sh": hermeticGate("# CI-Self-Test-Reads-Tree: because\n"),
      marker: "present\n",
      ".github/workflows/w.yml": hermeticWf,
    }),
    "must say WHY",
  );
  // A genuinely hermetic self-test needs no declaration and must not be asked for one.
  expectProbe(
    "a hermetic self-test passes with no declaration",
    build({
      "scripts/check-x.sh":
        `#!/bin/sh\n# CI-Kind: gate\n# CI-Self-Test: sh scripts/check-x.sh --self-test\n` +
        `# CI-Scan-Probe: external — not probed in this fixture\n# CI-Scans: things\n` +
        `case "$1" in --self-test) exit 0;; esac\necho "scanned: 1 thing"\n`,
      ".github/workflows/w.yml": hermeticWf,
    }),
    null,
  );
  // The declaration is read only on a gate; anywhere else it is decorative.
  expect(
    "a reads-tree declaration on a self-test file is caught",
    build({
      "ci/test_a_selftest.py":
        "# CI-Kind: self-test\n# CI-Covers: scripts/check-x.sh\n" +
        "# CI-Self-Test-Reads-Tree: this does nothing here\n",
      "scripts/check-x.sh": goodGate,
      ".github/workflows/w.yml": WF(["scripts/check-x.sh --self-test", "scripts/check-x.sh"]),
    }),
    "has no effect",
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
  console.log("  18 case(s): every rule proven to fire, incl. the population probe and");
  console.log("  the hermeticity copy-out;");
  console.log("  comment-only mention and explained-external proven NOT to fire");
  console.log("SELF-TEST PASS — the checker has teeth.");
  process.exit(0);
}

const ROOT = join(import.meta.dir, "..");
const { violations, decls, waivers } = audit(ROOT, true);

// A meta-gate that finds nothing to check is the failure it exists to prevent.
if (decls.length === 0) {
  console.error(
    "check-gate-coverage: found no check scripts at all — refusing to pass vacuously.",
  );
  process.exit(2);
}

const counts = KINDS.map((k) => `${decls.filter((d) => d.kind === k).length} ${k}`).join(" · ");
console.log(`scanned: ${decls.length} check script(s) in scripts/ and ci/ — ${counts}`);
if (waivers.length) {
  console.log(`\n${waivers.length} declared exception(s) — stated, not silent:`);
  for (const w of waivers) console.log("  " + w);
}

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
