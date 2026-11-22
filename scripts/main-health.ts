#!/usr/bin/env bun
/**
 * main-health.ts — when `main` goes red, say so loudly, and hold merging until it is green.
 *
 * NP-SW-CI-001 §6.20, OI-SWCI-10, OI-SWCI-54 Q5. Run by .github/workflows/main-health.yml.
 *
 * The `Safety` ruleset no longer requires a branch to be up to date before it merges. A pull request
 * that passed against an older `main` can merge, and two that each pass alone can leave `main` red
 * together (#983 and #995, §6.18.1). This file is what replaces that requirement:
 *
 *   --post-status   The commit status `main is green, or this PR is up to date with main`, posted on the
 *                   head of every open pull request into main (or of one, with --pr N). It is a required
 *                   context. While `main` is green it is `success` everywhere. While `main` is red it is
 *                   `success` only on a pull request whose head already contains main's tip: that pull
 *                   request's other checks ran on the broken `main`, so it can pass them only by fixing
 *                   it. Every other pull request is held until `main` is green again. That is the
 *                   up-to-date requirement, in force exactly while `main` is red, switched by CI with
 *                   no admin token (the setting itself needs one, and CI holds none).
 *
 *   --alert         On a run that finished on `main`: an email when that workflow turns red, and one
 *                   when it turns green again. Only on a change of colour, so one breakage is one email.
 *                   The red one carries `Importance: High` and `X-Priority: 1` and a fixed subject
 *                   prefix, `[NEURONE MAIN RED]`, that a mail rule can match.
 *
 *   --self-test     The decisions below, against fixtures. No network.
 *
 * "Red" is a decisive failing conclusion of the latest completed run: failure, timed_out or
 * startup_failure. A cancelled, skipped or neutral run decides nothing, and the run before it is used.
 * Which workflows hold merging is NP_GATE_WORKFLOWS in the workflow: the four that hold every required
 * context, because a freeze bites only through a required check. Which workflows alert is the
 * workflow's `workflow_run` list: every workflow that runs on main. scripts/check-ci-scope.ts
 * (correspondence F) keeps both lists complete.
 *
 * `main` stays red until it is fixed, not until a commit arrives that touches nothing in scope: on main,
 * scripts/ci-changed-scope.sh diffs from the last green head (§6.20), so the revision App's docs-only
 * commit that follows most merges re-runs the failing job instead of skipping it.
 *
 * Fail-closed: if main's state cannot be read, every status posted is `error`, which does not satisfy
 * a required check. The script never reads a pull request's code; it reads metadata through the API,
 * which is why the workflow may run it on `pull_request_target`.
 *
 * Environment: GITHUB_TOKEN, GITHUB_REPOSITORY, GITHUB_EVENT_PATH (--alert), GITHUB_SERVER_URL,
 * NP_STATUS_CONTEXT and NP_GATE_WORKFLOWS (--post-status), and for --alert the secrets
 * NP_ALERT_SMTP_USER, NP_ALERT_SMTP_PASSWORD, NP_ALERT_TO and optionally NP_ALERT_SMTP_URL
 * (default smtp://smtp.gmail.com:587, STARTTLS). The recipient is a secret, not a literal here: the
 * repository is public.
 *
 * Usage: bun scripts/main-health.ts --self-test | --post-status [--pr N] | --alert
 */

import { mkdtempSync, readFileSync, rmSync, writeFileSync } from "fs";
import { join } from "path";
import { tmpdir } from "os";

// ── Decisions (pure) ─────────────────────────────────────────────────────────

export type Run = {
  id: number;
  name: string;
  path: string;
  conclusion: string | null;
  head_sha: string;
  html_url: string;
  created_at: string;
  event: string;
};

const RED = new Set(["failure", "timed_out", "startup_failure"]);
const DECISIVE = new Set([...RED, "success"]);

/** red | green | undecided (a run that is not finished, or was cancelled, skipped or neutral). */
export function colour(conclusion: string | null | undefined): "red" | "green" | "undecided" {
  if (!conclusion || !DECISIVE.has(conclusion)) return "undecided";
  return RED.has(conclusion) ? "red" : "green";
}

/** The latest run with a decisive conclusion, from runs listed newest first. */
export function latestDecisive(runs: Run[]): Run | null {
  return runs.find((r) => colour(r.conclusion) !== "undecided") ?? null;
}

export type Transition = "turned-red" | "turned-green" | "none";

/** What this run changed, given the decisive run before it of the same workflow (none: green before). */
export function transition(current: Run, previous: Run | null): Transition {
  const now = colour(current.conclusion);
  if (now === "undecided") return "none";
  const before = previous ? colour(previous.conclusion) : "green";
  if (now === "red" && before !== "red") return "turned-red";
  if (now === "green" && before === "red") return "turned-green";
  return "none";
}

export type MainState = { ok: true; red: Run[] } | { ok: false; why: string };
export type Status = { state: "success" | "failure" | "error"; description: string; target_url?: string };

const MAX_DESCRIPTION = 140; // the Statuses API's limit
const clip = (s: string): string => (s.length <= MAX_DESCRIPTION ? s : s.slice(0, MAX_DESCRIPTION - 1) + "…");

/** The status for one pull request. `upToDate`: its head contains main's tip. */
export function statusFor(main: MainState, upToDate: boolean | null): Status {
  if (!main.ok) return { state: "error", description: clip(`could not read main's state (${main.why}); merging is held until it can be read`) };
  if (main.red.length === 0) return { state: "success", description: "main is green" };
  const names = main.red.map((r) => r.name).join(", ");
  const target_url = main.red[0]!.html_url;
  if (upToDate === null) return { state: "error", description: clip(`main is red (${names}) and this PR's position could not be read`), target_url };
  if (upToDate) return { state: "success", description: clip(`main is red (${names}); this PR is up to date, so its checks ran on it`), target_url };
  return { state: "failure", description: clip(`main is red (${names}): update this branch from main and let its checks run`), target_url };
}

/** RFC 2047 for a header that is not plain ASCII. */
export function encodeHeader(s: string): string {
  return /^[\x20-\x7e]*$/.test(s) ? s : `=?UTF-8?B?${Buffer.from(s, "utf8").toString("base64")}?=`;
}

export const RED_PREFIX = "[NEURONE MAIN RED]";
export const GREEN_PREFIX = "[NEURONE MAIN GREEN]";

export type Mail = { subject: string; headers: string[]; body: string };

/** The email for a change of colour. `stillRed`: other workflows red on main after this run. */
export function composeMail(t: Exclude<Transition, "none">, run: Run, repo: string, commitTitle: string, stillRed: string[]): Mail {
  const short = run.head_sha.slice(0, 7);
  const red = t === "turned-red";
  const subject = red
    ? `${RED_PREFIX} ${run.name} failed on main (${short})`
    : `${GREEN_PREFIX} ${run.name} passed on main (${short})${stillRed.length ? ` - still red: ${stillRed.join(", ")}` : " - main is green"}`;
  const lines = [
    red ? `main is RED in ${repo}.` : `${run.name} is green again on main in ${repo}.`,
    "",
    `Workflow:   ${run.name}`,
    `Conclusion: ${run.conclusion}`,
    `Commit:     ${run.head_sha}  ${commitTitle}`,
    `Run:        ${run.html_url}`,
    "",
  ];
  if (red) {
    lines.push(
      "Until main is green, a pull request merges only if it is up to date with main",
      "(the 'main is green, or this PR is up to date with main' status). Fix main first:",
      "a pull request that contains main's tip and passes its checks is the fix.",
      "",
      "Subject prefix for a mail rule: " + RED_PREFIX,
    );
  } else {
    lines.push(stillRed.length
      ? `Still red on main: ${stillRed.join(", ")}. Merging stays held until they pass.`
      : "Nothing is red on main. Pull requests merge without updating again.");
  }
  lines.push("", "Sent by .github/workflows/main-health.yml (NP-SW-CI-001 §6.20).");
  const headers = red ? ["Importance: High", "X-Priority: 1 (Highest)", "Priority: urgent", "X-MSMail-Priority: High"] : [];
  return { subject, headers, body: lines.join("\n") + "\n" };
}

/** The whole message, CRLF line endings, as SMTP wants it. */
export function renderMessage(m: Mail, from: string, to: string, date: Date): string {
  const head = [
    `From: NeurOne CI <${from}>`,
    `To: <${to}>`,
    `Subject: ${encodeHeader(m.subject)}`,
    `Date: ${date.toUTCString().replace("GMT", "+0000")}`,
    `Message-ID: <main-health.${date.getTime()}.${Math.random().toString(36).slice(2)}@neurone-ci>`,
    "MIME-Version: 1.0",
    "Content-Type: text/plain; charset=utf-8",
    "Content-Transfer-Encoding: 8bit",
    "X-NeurOne-Main-Health: 1",
    ...m.headers,
  ];
  return [...head, "", m.body].join("\r\n").replace(/\r?\n/g, "\r\n");
}

// ── GitHub ───────────────────────────────────────────────────────────────────

function env(name: string): string {
  const v = process.env[name];
  if (!v) { console.error(`main-health: ${name} is not set`); process.exit(2); }
  return v;
}

async function gh(path: string, init?: { method?: string; body?: unknown }): Promise<any> {
  const res = await fetch(`https://api.github.com/${path}`, {
    method: init?.method ?? "GET",
    headers: {
      Authorization: `Bearer ${env("GITHUB_TOKEN")}`,
      Accept: "application/vnd.github+json",
      "X-GitHub-Api-Version": "2022-11-28",
      "User-Agent": "neurone-main-health",
    },
    body: init?.body === undefined ? undefined : JSON.stringify(init.body),
  });
  if (!res.ok) throw new Error(`${init?.method ?? "GET"} ${path}: HTTP ${res.status} ${(await res.text()).slice(0, 200)}`);
  return res.status === 204 ? null : res.json();
}

async function runsOf(repo: string, workflow: string | number, event: string): Promise<Run[]> {
  const r = await gh(`repos/${repo}/actions/workflows/${workflow}/runs?branch=main&event=${event}&status=completed&per_page=30`);
  return (r.workflow_runs ?? []) as Run[];
}

/** Red gate workflows on main: the latest decisive push run of each. */
async function readMain(repo: string, gateFiles: string[]): Promise<MainState> {
  try {
    const red: Run[] = [];
    for (const f of gateFiles) {
      const last = latestDecisive(await runsOf(repo, f, "push"));
      if (last && colour(last.conclusion) === "red") red.push(last);
    }
    return { ok: true, red };
  } catch (e) {
    return { ok: false, why: String((e as Error).message).slice(0, 80) };
  }
}

type Pull = { number: number; head: { sha: string } };

async function openPulls(repo: string): Promise<Pull[]> {
  const all: Pull[] = [];
  for (let page = 1; ; page++) {
    const batch = (await gh(`repos/${repo}/pulls?state=open&base=main&per_page=100&page=${page}`)) as Pull[];
    all.push(...batch);
    if (batch.length < 100) return all;
  }
}

async function postStatus(): Promise<void> {
  const repo = env("GITHUB_REPOSITORY");
  const context = env("NP_STATUS_CONTEXT");
  const gateFiles = env("NP_GATE_WORKFLOWS").split(/\s+/).filter(Boolean);
  // A run of a workflow that holds nothing cannot change the status: skip the sweep.
  const trigger = process.env.NP_TRIGGER_PATH;
  if (trigger && !gateFiles.includes(trigger.split("/").pop()!)) { console.log(`${trigger} holds no required context: nothing to post`); return; }
  const pi = process.argv.indexOf("--pr");
  const only = pi >= 0 ? Number(process.argv[pi + 1]) : null;

  const main = await readMain(repo, gateFiles);
  console.log(main.ok ? `main: ${main.red.length ? `RED (${main.red.map((r) => r.name).join(", ")})` : "green"}` : `main: unreadable (${main.why})`);

  let pulls: Pull[];
  if (only !== null) pulls = [(await gh(`repos/${repo}/pulls/${only}`)) as Pull];
  else pulls = await openPulls(repo);

  let tip: string | null = null;
  if (main.ok && main.red.length) {
    try { tip = (await gh(`repos/${repo}/branches/main`)).commit.sha; } catch { tip = null; }
  }
  let failed = 0;
  for (const p of pulls) {
    let upToDate: boolean | null = null;
    if (main.ok && main.red.length && tip) {
      try { upToDate = (await gh(`repos/${repo}/compare/${tip}...${p.head.sha}`)).behind_by === 0; } catch { upToDate = null; }
    }
    const s = statusFor(main, upToDate);
    try {
      // Statuses accumulate (GitHub keeps at most 1000 per commit and context), so post only a change.
      const combined = await gh(`repos/${repo}/commits/${p.head.sha}/status?per_page=100`);
      const cur = (combined.statuses ?? []).find((x: any) => x.context === context);
      if (cur && cur.state === s.state && cur.description === s.description) { console.log(`#${p.number} ${s.state} (unchanged)`); continue; }
      await gh(`repos/${repo}/statuses/${p.head.sha}`, { method: "POST", body: { context, ...s } });
      console.log(`#${p.number} ${s.state}: ${s.description}`);
    } catch (e) {
      failed++;
      console.error(`::error::#${p.number}: could not post the status (${(e as Error).message})`);
    }
  }
  if (failed) process.exit(1);
}

async function alert(): Promise<void> {
  const repo = env("GITHUB_REPOSITORY");
  const event = JSON.parse(readFileSync(env("GITHUB_EVENT_PATH"), "utf8"));
  const wr = event.workflow_run;
  if (!wr) { console.error("main-health --alert: not a workflow_run event"); process.exit(2); }
  const run: Run = { id: wr.id, name: wr.name, path: wr.path, conclusion: wr.conclusion, head_sha: wr.head_sha, html_url: wr.html_url, created_at: wr.created_at, event: wr.event };
  const earlier = (await runsOf(repo, wr.workflow_id, wr.event)).filter((r) => r.id !== run.id && r.created_at <= run.created_at);
  const t = transition(run, latestDecisive(earlier));
  console.log(`${run.name} on main: ${run.conclusion} (${t})`);
  if (t === "none") return;

  let stillRed: string[] = [];
  if (t === "turned-green") {
    try {
      const recent = (await gh(`repos/${repo}/actions/runs?branch=main&status=completed&per_page=100`)).workflow_runs as Run[];
      const seen = new Set<string>();
      for (const r of recent) {
        if (seen.has(r.path) || colour(r.conclusion) === "undecided" || !["push", "schedule"].includes(r.event)) continue;
        seen.add(r.path);
        if (r.path !== run.path && colour(r.conclusion) === "red") stillRed.push(r.name);
      }
    } catch { stillRed = ["(could not be read)"]; }
  }
  const title = String(wr.head_commit?.message ?? "").split("\n")[0]!.slice(0, 120);
  const mail = composeMail(t, run, repo, title, stillRed);

  const user = process.env.NP_ALERT_SMTP_USER, pass = process.env.NP_ALERT_SMTP_PASSWORD, to = process.env.NP_ALERT_TO;
  if (!user || !pass || !to) {
    console.error(`::error::main-health: '${mail.subject}' was not sent: the secrets NP_ALERT_SMTP_USER, NP_ALERT_SMTP_PASSWORD and NP_ALERT_TO are not all set (NP-SW-CI-001 §6.20.3)`);
    process.exit(1);
  }
  const url = process.env.NP_ALERT_SMTP_URL || "smtp://smtp.gmail.com:587";
  const dir = mkdtempSync(join(tmpdir(), "main-health-"));
  try {
    writeFileSync(join(dir, "mail.eml"), renderMessage(mail, user, to, new Date()));
    // The password goes in a netrc file, not on the command line, where any process could read it.
    const host = new URL(url).hostname;
    writeFileSync(join(dir, "netrc"), `machine ${host} login ${user} password ${pass}\n`, { mode: 0o600 });
    const r = Bun.spawnSync(["curl", "--silent", "--show-error", "--ssl-reqd", "--url", url, "--netrc-file", join(dir, "netrc"),
      "--mail-from", user, "--mail-rcpt", to, "--upload-file", join(dir, "mail.eml")], { stdout: "pipe", stderr: "pipe" });
    if (r.exitCode !== 0) {
      console.error(`::error::main-health: sending '${mail.subject}' failed: ${new TextDecoder().decode(r.stderr).trim()}`);
      process.exit(1);
    }
    console.log(`sent: ${mail.subject}`);
  } finally {
    rmSync(dir, { recursive: true, force: true });
  }
}

// ── Self-test ────────────────────────────────────────────────────────────────

function selfTest(): never {
  let failed = 0, cases = 0;
  const eq = (label: string, got: unknown, want: unknown): void => {
    cases++;
    if (JSON.stringify(got) !== JSON.stringify(want)) { failed++; console.error(`FAIL ${label}: got ${JSON.stringify(got)}, want ${JSON.stringify(want)}`); }
  };
  const ok = (label: string, cond: boolean): void => { cases++; if (!cond) { failed++; console.error(`FAIL ${label}`); } };
  // html_url is an opaque token: the cases check it is carried through to the status and the email, not that it parses.
  const R = (id: number, conclusion: string | null, name = "Tooling CI"): Run => ({
    id, name, path: ".github/workflows/tooling-ci.yml", conclusion, head_sha: `${id}`.padStart(40, "a"),
    html_url: `run-link-${id}`, created_at: `2026-11-22T00:00:${String(id).padStart(2, "0")}Z`, event: "push",
  });

  // Colour: only failure, timed_out and startup_failure are red; only success is green.
  eq("colour-failure", colour("failure"), "red");
  eq("colour-timed-out", colour("timed_out"), "red");
  eq("colour-startup", colour("startup_failure"), "red");
  eq("colour-success", colour("success"), "green");
  for (const c of ["cancelled", "skipped", "neutral", "action_required", "stale", null]) eq(`colour-${c}`, colour(c), "undecided");
  // A cancelled run decides nothing; the run before it does.
  eq("latest-skips-cancelled", latestDecisive([R(3, "cancelled"), R(2, "failure"), R(1, "success")])?.id, 2);
  eq("latest-none", latestDecisive([R(2, "cancelled"), R(1, null)]), null);

  // Transitions: one email per change of colour, none while the colour holds.
  eq("t-green-to-red", transition(R(2, "failure"), R(1, "success")), "turned-red");
  eq("t-first-run-red", transition(R(1, "failure"), null), "turned-red");
  eq("t-red-stays-red", transition(R(2, "failure"), R(1, "timed_out")), "none");
  eq("t-red-to-green", transition(R(2, "success"), R(1, "failure")), "turned-green");
  eq("t-green-stays", transition(R(2, "success"), R(1, "success")), "none");
  eq("t-first-run-green", transition(R(1, "success"), null), "none");
  eq("t-cancelled", transition(R(2, "cancelled"), R(1, "success")), "none");

  // The status: the freeze is exactly "up to date while red", and an unreadable state holds merging.
  const green: MainState = { ok: true, red: [] };
  const red: MainState = { ok: true, red: [R(5, "failure", "Simulator CI")] };
  eq("s-green-behind", statusFor(green, false).state, "success");
  eq("s-green-unknown-position", statusFor(green, null).state, "success");
  eq("s-red-behind", statusFor(red, false).state, "failure");
  eq("s-red-up-to-date", statusFor(red, true).state, "success");
  eq("s-red-position-unknown", statusFor(red, null).state, "error");
  eq("s-unreadable", statusFor({ ok: false, why: "HTTP 502" }, true).state, "error");
  ok("s-names-the-red-workflow", statusFor(red, false).description.includes("Simulator CI"));
  ok("s-links-the-red-run", statusFor(red, false).target_url === "run-link-5");
  const many: MainState = { ok: true, red: Array.from({ length: 12 }, (_, i) => R(i + 1, "failure", `A long workflow name number ${i}`)) };
  ok("s-description-within-limit", statusFor(many, false).description.length <= MAX_DESCRIPTION);

  // The email: a fixed, matchable subject, high importance when red, normal when green.
  const m = composeMail("turned-red", R(7, "failure", "Firmware Cross-Compile (ARM)"), "o/r", "Fix the thing", []);
  ok("mail-red-prefix", m.subject.startsWith(RED_PREFIX + " "));
  ok("mail-red-importance", m.headers.includes("Importance: High") && m.headers.includes("X-Priority: 1 (Highest)"));
  ok("mail-red-names-run", m.body.split("\n").includes("Run:        run-link-7") && m.body.includes("Fix the thing"));
  const g = composeMail("turned-green", R(8, "success"), "o/r", "x", []);
  ok("mail-green-prefix", g.subject.startsWith(GREEN_PREFIX + " ") && g.subject.endsWith("main is green"));
  ok("mail-green-not-urgent", g.headers.length === 0);
  ok("mail-green-still-red", composeMail("turned-green", R(8, "success"), "o/r", "x", ["Web App CI"]).subject.includes("still red: Web App CI"));
  const msg = renderMessage(m, "ci@example.invalid", "me@example.invalid", new Date(Date.UTC(2026, 10, 22, 12, 0, 0)));
  ok("msg-crlf", !/[^\r]\n/.test(msg));
  ok("msg-headers-before-body", msg.indexOf("Importance: High") < msg.indexOf("\r\n\r\n"));
  ok("msg-subject", msg.includes(`Subject: ${RED_PREFIX} Firmware Cross-Compile (ARM) failed on main`));
  ok("msg-date", msg.includes("Date: Sun, 22 Nov 2026 12:00:00 +0000"));
  eq("header-ascii-unchanged", encodeHeader("plain (ascii)"), "plain (ascii)");
  ok("header-utf8-encoded", encodeHeader("a — b").startsWith("=?UTF-8?B?") && Buffer.from(encodeHeader("a — b").slice(10, -2), "base64").toString() === "a — b");

  if (failed) { console.error(`main-health: ${failed} of ${cases} self-test case(s) FAILED`); process.exit(1); }
  console.log(`main-health: self-test OK (${cases} cases)`);
  process.exit(0);
}

const mode = process.argv[2];
if (mode === "--self-test") selfTest();
else if (mode === "--post-status") await postStatus();
else if (mode === "--alert") await alert();
else { console.error("usage: bun scripts/main-health.ts --self-test | --post-status [--pr N] | --alert"); process.exit(2); }
