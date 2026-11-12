#!/usr/bin/env bun
/**
 * check-sim-host-only.ts — the simulator's sim platform layer never reaches a device image.
 *
 * CI-Kind: gate
 * CI-Self-Test: bun scripts/check-sim-host-only.ts --self-test
 * CI-Scans: every build and source file under firmware/ outside firmware/platform_sim/, for a mention of the sim layer (platform_sim, np_sim_, NP_BUILD_SIM); firmware/CMakeLists.txt for the one guarded add_subdirectory(platform_sim); firmware/platform_sim/CMakeLists.txt for its cross-compile guard; with --cross-probe, a real CMake configure under a cross toolchain
 * CI-Scan-Paths: firmware/** scripts/check-sim-host-only.ts
 *
 * NP-SIM-FW-001 REQ-SIMFW-01 (OI-SIMFW-14, the check half). The sim layer answers every hardware seam
 * plausibly, so linked into an image that targets the device it would turn a missing driver into a
 * defeated interlock. Until this gate the only guard was a CMake `return()`, and nothing failed if a
 * device target started linking the layer another way.
 *
 * ── What it checks ───────────────────────────────────────────────────────────────────────────────
 *
 *   1. NAMED NOWHERE ELSE. No build file under firmware/ outside firmware/platform_sim/ mentions
 *      `platform_sim`, `np_sim_` or `NP_BUILD_SIM`, and no source, header, linker script or makefile
 *      there #includes or names the layer. The one exception is firmware/CMakeLists.txt (rule 2).
 *      A device target can only link the layer by naming it (a target, a source path or a
 *      subdirectory), so every route is a mention. REQ-SIMFW-03's "the build files name the sim layer
 *      and nothing else" is the same property seen from the sources.
 *   2. ONE GUARDED ENTRY. firmware/CMakeLists.txt adds the layer exactly once, as
 *      `add_subdirectory(platform_sim)` inside `if(NP_BUILD_SIM)` inside `if(NP_BUILD_TESTS)`, in the
 *      branches that run when the condition is true. The host test branch is the only branch a device
 *      configure never runs as a target set (the device images are configured from their own
 *      subdirectories under a cross toolchain file).
 *   3. SELF-GUARDED. firmware/platform_sim/CMakeLists.txt returns under `CMAKE_CROSSCOMPILING` and
 *      again unless both NP_BUILD_TESTS and NP_BUILD_SIM are set, before it creates anything.
 *   4. (--cross-probe) THE GUARD WORKS. Configures the super-project twice with a throwaway toolchain
 *      file that makes CMake cross-compile (no ARM compiler or binutils needed): once with the host toolchain and
 *      NP_BUILD_SIM=ON, where the sim targets must exist (the positive control, so the probe cannot
 *      pass because it looked at nothing), and once as a cross build with NP_BUILD_SIM=ON, where no
 *      `np_sim_` target may exist.
 *
 * ── What it does NOT check, and why ──────────────────────────────────────────────────────────────
 *
 * REQ-SIMFW-02's seam accounting. That is scripts/check-sim-seam-routes.ts, which compares the SW-02
 * platform contract with firmware/platform_sim/np_sim_seam_routes.txt and the sim layer (NP-SIM-FW-001
 * §30; OI-SIMFW-02 and OI-SIMFW-14, closed). It asserts no count: the census constant counts the
 * contract, and the split by disposition is printed, not fixed.
 *
 * Usage: bun scripts/check-sim-host-only.ts [--cross-probe] [--self-test]
 */
import {
  readFileSync, readdirSync, statSync, mkdtempSync, mkdirSync, writeFileSync, rmSync, existsSync,
} from "fs";
import { join } from "path";
import { tmpdir } from "os";
import { spawnSync } from "child_process";

const FIRMWARE = "firmware";
const SIM_DIR = "firmware/platform_sim";
const TOP_CMAKE = "firmware/CMakeLists.txt";
const SIM_CMAKE = "firmware/platform_sim/CMakeLists.txt";
const SIM_TOKEN = /platform_sim|np_sim_|NP_BUILD_SIM/;
const SCANNED = /(^|\/)(CMakeLists\.txt|Makefile)$|\.(cmake|mk|c|h|cpp|hpp|S|s|ld|json|cfg)$/;

// ── CMake command model ──────────────────────────────────────────────────────────────────────────

type Cmd = { name: string; args: string; line: number };

/** `#` comments stripped (outside quotes), then the file split into commands. */
function commands(text: string): Cmd[] {
  const noComment = text
    .split("\n")
    .map((l) => {
      let q = false;
      for (let i = 0; i < l.length; i++) {
        if (l[i] === '"') q = !q;
        else if (l[i] === "#" && !q) return l.slice(0, i);
      }
      return l;
    })
    .join("\n");
  const out: Cmd[] = [];
  const re = /(^|\n)[ \t]*([A-Za-z_][A-Za-z0-9_]*)[ \t]*\(([^()]*(?:\([^()]*\)[^()]*)*)\)/g;
  let m: RegExpExecArray | null;
  while ((m = re.exec(noComment))) {
    const line = noComment.slice(0, m.index + m[1]!.length).split("\n").length;
    out.push({ name: m[2]!.toLowerCase(), args: m[3]!.replace(/\s+/g, " ").trim(), line });
  }
  return out;
}

const mentionsSim = (c: Cmd): boolean => SIM_TOKEN.test(c.name + " " + c.args);

/** The if() stack (condition text, and whether we are in the else part) at each command. */
function withNesting(cmds: Cmd[]): { cmd: Cmd; stack: { cond: string; inElse: boolean }[] }[] {
  const stack: { cond: string; inElse: boolean }[] = [];
  const out: { cmd: Cmd; stack: { cond: string; inElse: boolean }[] }[] = [];
  for (const cmd of cmds) {
    if (cmd.name === "if") stack.push({ cond: cmd.args, inElse: false });
    else if (cmd.name === "elseif" || cmd.name === "else") {
      if (stack.length) stack[stack.length - 1] = { cond: stack[stack.length - 1]!.cond, inElse: true };
    } else if (cmd.name === "endif") stack.pop();
    out.push({ cmd, stack: stack.map((s) => ({ ...s })) });
  }
  return out;
}

// ── Rules ────────────────────────────────────────────────────────────────────────────────────────

function walk(root: string, dir: string, acc: string[]): void {
  for (const e of readdirSync(join(root, dir))) {
    const rel = `${dir}/${e}`;
    if (rel === SIM_DIR) continue;
    const st = statSync(join(root, rel));
    if (st.isDirectory()) walk(root, rel, acc);
    else if (SCANNED.test(rel)) acc.push(rel);
  }
}

let scannedFiles = 0;

export function rule1(root: string): string[] {
  const errs: string[] = [];
  if (!existsSync(join(root, FIRMWARE))) return [`${FIRMWARE}/ not found: the gate would pass on zero files`];
  const files: string[] = [];
  walk(root, FIRMWARE, files);
  scannedFiles = files.length;
  if (files.length === 0) return [`no file under ${FIRMWARE}/ was scanned: the gate would pass on zero files`];
  for (const f of files) {
    if (f === TOP_CMAKE) continue; // rule 2
    const text = readFileSync(join(root, f), "utf8");
    if (/(^|\/)CMakeLists\.txt$|\.cmake$/.test(f)) {
      for (const c of commands(text)) {
        if (mentionsSim(c)) errs.push(`${f}:${c.line}: ${c.name}(${c.args}) names the sim layer; a device target can link it this way (REQ-SIMFW-01)`);
      }
    } else {
      text.split("\n").forEach((l, i) => {
        // An #include of the layer, or a build/linker line naming it. A prose mention is not a link.
        if (/^\s*#\s*include\b/.test(l) && SIM_TOKEN.test(l)) errs.push(`${f}:${i + 1}: includes the sim layer (REQ-SIMFW-01, REQ-SIMFW-03)`);
        else if (/(^|\/)(Makefile)$|\.(mk|ld)$/.test(f) && SIM_TOKEN.test(l) && !/^\s*#/.test(l)) errs.push(`${f}:${i + 1}: a build or linker file names the sim layer (REQ-SIMFW-01)`);
      });
    }
  }
  return errs;
}

export function rule2(root: string): string[] {
  const p = join(root, TOP_CMAKE);
  if (!existsSync(p)) return [`${TOP_CMAKE} not found`];
  const errs: string[] = [];
  let entries = 0;
  for (const { cmd, stack } of withNesting(commands(readFileSync(p, "utf8")))) {
    if (!mentionsSim(cmd)) continue;
    if (cmd.name === "option" && /^NP_BUILD_SIM\b/.test(cmd.args)) continue;
    if (cmd.name === "if" && cmd.args === "NP_BUILD_SIM") continue;
    if (cmd.name === "add_subdirectory" && cmd.args === "platform_sim") {
      entries++;
      const ok = stack.length === 2 &&
        stack[0]!.cond === "NP_BUILD_TESTS" && !stack[0]!.inElse &&
        stack[1]!.cond === "NP_BUILD_SIM" && !stack[1]!.inElse;
      if (!ok) errs.push(`${TOP_CMAKE}:${cmd.line}: add_subdirectory(platform_sim) is not directly inside if(NP_BUILD_TESTS) then if(NP_BUILD_SIM), in the true branches (REQ-SIMFW-01)`);
      continue;
    }
    errs.push(`${TOP_CMAKE}:${cmd.line}: ${cmd.name}(${cmd.args}) names the sim layer outside the one guarded add_subdirectory (REQ-SIMFW-01)`);
  }
  if (entries !== 1) errs.push(`${TOP_CMAKE}: expected exactly one add_subdirectory(platform_sim), found ${entries}`);
  return errs;
}

const CREATES = /^(add_library|add_executable|add_custom_target|add_custom_command|add_test|add_subdirectory|add_dependencies|target_[a-z_]+|install|include|find_package|file|configure_file)$/;

export function rule3(root: string): string[] {
  const p = join(root, SIM_CMAKE);
  if (!existsSync(p)) return [`${SIM_CMAKE} not found`];
  const errs: string[] = [];
  const nested = withNesting(commands(readFileSync(p, "utf8")));
  const firstCreate = nested.findIndex((n) => CREATES.test(n.cmd.name));
  const before = firstCreate < 0 ? nested : nested.slice(0, firstCreate);
  const guards = (cond: RegExp): boolean =>
    before.some((n, i) => n.cmd.name === "return" && n.stack.length === 1 && cond.test(n.stack[0]!.cond) && !n.stack[0]!.inElse);
  if (!guards(/^CMAKE_CROSSCOMPILING$/)) errs.push(`${SIM_CMAKE}: no \`if(CMAKE_CROSSCOMPILING) return()\` before the first target (REQ-SIMFW-01)`);
  if (!guards(/^NOT NP_BUILD_TESTS OR NOT NP_BUILD_SIM$/)) errs.push(`${SIM_CMAKE}: no \`if(NOT NP_BUILD_TESTS OR NOT NP_BUILD_SIM) return()\` before the first target (REQ-SIMFW-01)`);
  return errs;
}

// ── The cross probe ──────────────────────────────────────────────────────────────────────────────

/** Pure verdict over `ninja -t targets all` of the two configures. */
export function probeVerdict(hostTargets: string, crossTargets: string): string[] {
  const errs: string[] = [];
  const sim = /^np_sim_[A-Za-z0-9_]+(:|\s)/m;
  if (!sim.test(hostTargets)) errs.push("positive control failed: the host configure with NP_BUILD_SIM=ON defines no np_sim_ target, so the probe proves nothing");
  const m = /^(np_sim_[A-Za-z0-9_]+)(:|\s)/m.exec(crossTargets);
  if (m) errs.push(`the cross configure with NP_BUILD_SIM=ON defines ${m[1]}: the sim layer reaches a cross-compiled build (REQ-SIMFW-01)`);
  return errs;
}

function ninjaTargets(src: string, build: string, extra: string[]): { out: string; err: string | null } {
  // The device's CMake files call find_program(... REQUIRED) for the ARM binutils (objcopy, size, nm) at configure time.
  // This probe only lists targets and never builds, so those are pinned to a stub: the probe then needs no ARM tool on the
  // machine (the CI runner has none at this step) and configures the same everywhere. A tool found by name would also be
  // found on a developer's machine and missed in CI, which is how the first CI run failed.
  const stubs = ["-DOBJCOPY=/bin/true", "-DSIZE=/bin/true", "-DNP_ARM_NM=/bin/true"];
  const c = spawnSync("cmake", ["-S", src, "-B", build, "-G", "Ninja", "-DNP_BUILD_TESTS=ON", "-DNP_BUILD_SIM=ON", ...stubs, ...extra], { encoding: "utf8" });
  if (c.status !== 0) return { out: "", err: `cmake configure failed:\n${c.stdout}${c.stderr}` };
  const t = spawnSync("ninja", ["-C", build, "-t", "targets", "all"], { encoding: "utf8" });
  if (t.status !== 0) return { out: "", err: `ninja -t targets failed:\n${t.stderr}` };
  return { out: t.stdout, err: null };
}

function crossProbe(root: string): string[] {
  const tmp = mkdtempSync(join(tmpdir(), "np-sim-probe-"));
  try {
    const tc = join(tmp, "cross.cmake");
    // Generic + try-compile as a static library: CMake treats it as a cross build (so
    // CMAKE_CROSSCOMPILING is set by CMake itself, which a -D flag cannot do) with the host compiler.
    writeFileSync(tc, "set(CMAKE_SYSTEM_NAME Generic)\nset(CMAKE_SYSTEM_PROCESSOR arm)\nset(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)\n");
    const src = join(root, FIRMWARE);
    const host = ninjaTargets(src, join(tmp, "host"), ["-DCMAKE_CROSSCOMPILING=OFF"]);
    if (host.err) return [host.err];
    const cross = ninjaTargets(src, join(tmp, "cross"), [`-DCMAKE_TOOLCHAIN_FILE=${tc}`]);
    if (cross.err) return [cross.err];
    return probeVerdict(host.out, cross.out);
  } finally {
    rmSync(tmp, { recursive: true, force: true });
  }
}

// ── Driver ───────────────────────────────────────────────────────────────────────────────────────

function run(root: string, probe: boolean): number {
  const errs = [...rule1(root), ...rule2(root), ...rule3(root), ...(probe ? crossProbe(root) : [])];
  for (const e of errs) console.error(`FAIL ${e}`);
  console.log(`scanned: ${scannedFiles} firmware file(s) outside ${SIM_DIR}/`);
  if (errs.length === 0) {
    console.log(`PASS the sim layer is named only by its one guarded entry${probe ? " and is absent from a cross configure (positive control present)" : ""} (REQ-SIMFW-01)`);
    return 0;
  }
  console.error(`\n${errs.length} violation(s). NP-SIM-FW-001 REQ-SIMFW-01: the sim platform layer is never linked into an image that targets the device.`);
  return 1;
}

if (process.argv.includes("--self-test")) {
  const root = mkdtempSync(join(tmpdir(), "np-sim-host-only-"));
  const write = (rel: string, body: string) => {
    mkdirSync(join(root, rel, ".."), { recursive: true });
    writeFileSync(join(root, rel), body);
  };
  const GOOD_TOP = `option(NP_BUILD_TESTS "t" OFF)
option(NP_BUILD_SIM "s" OFF)
if(NP_BUILD_TESTS)
    add_subdirectory(crypto)
    if(NP_BUILD_SIM)
        add_subdirectory(platform_sim) # host only
    endif()
endif()
`;
  const GOOD_SIM = `cmake_minimum_required(VERSION 3.20)
if(CMAKE_CROSSCOMPILING)
    return()
endif()
if(NOT NP_BUILD_TESTS OR NOT NP_BUILD_SIM)
    return()
endif()
add_library(np_sim_pbm STATIC src/a.c)
`;
  const clean = () => {
    rmSync(root, { recursive: true, force: true });
    mkdirSync(root, { recursive: true });
    write(TOP_CMAKE, GOOD_TOP);
    write(SIM_CMAKE, GOOD_SIM);
    write("firmware/crypto/CMakeLists.txt", "add_library(np_crypto STATIC a.c)\n");
    write("firmware/safety_mcu/CMakeLists.txt", "add_executable(np_safety_mcu main.c)\n# the simulator links this directory's sources, from platform_sim; a comment is not a link\n");
    write("firmware/hub_control/src/a.c", '#include "np_hub.h"\n/* np_sim_ is prose here */\nint x;\n');
    write("firmware/platform_sim/src/np_sim_a.c", "int sim;\n");
  };
  let failures = 0;
  const expect = (label: string, want: number, got: number) => {
    const ok = want === got;
    if (!ok) failures++;
    console.log(`  ${ok ? "ok  " : "FAIL"}  ${label} (want rc=${want}, got rc=${got})`);
  };
  const quiet = (): number => {
    const e = console.error, l = console.log;
    console.error = () => {}; console.log = () => {};
    try { return run(root, false); } finally { console.error = e; console.log = l; }
  };

  console.log("self-test:");
  clean();
  expect("clean tree passes", 0, quiet());

  clean();
  write("firmware/application/CMakeLists.txt", "add_executable(np_app main.c)\ntarget_link_libraries(np_app PRIVATE np_sim_pbm)\n");
  expect("a device target linking np_sim_pbm is caught", 1, quiet());

  clean();
  write("firmware/bootloader/CMakeLists.txt", "add_subdirectory(../platform_sim platform_sim_dev)\n");
  expect("a second add_subdirectory of the layer is caught", 1, quiet());

  clean();
  write("firmware/application/CMakeLists.txt", "target_sources(np_app PRIVATE ../platform_sim/src/np_sim_a.c)\n");
  expect("a device target taking a sim source by path is caught", 1, quiet());

  clean();
  write("firmware/hub_control/src/b.c", '#include "np_sim_bus.h"\n');
  expect("a firmware source including the sim layer is caught", 1, quiet());

  clean();
  write("firmware/application/link.ld", "INCLUDE platform_sim/np_sim.ld\n");
  expect("a linker script naming the layer is caught", 1, quiet());

  clean();
  write(TOP_CMAKE, GOOD_TOP.replace("    if(NP_BUILD_SIM)\n        add_subdirectory(platform_sim) # host only\n    endif()\n", "    add_subdirectory(platform_sim)\n"));
  expect("the entry outside if(NP_BUILD_SIM) is caught", 1, quiet());

  clean();
  write(TOP_CMAKE, "option(NP_BUILD_SIM \"s\" OFF)\nif(NP_BUILD_SIM)\n    add_subdirectory(platform_sim)\nendif()\n");
  expect("the entry outside if(NP_BUILD_TESTS) is caught", 1, quiet());

  clean();
  write(TOP_CMAKE, GOOD_TOP.replace("add_subdirectory(crypto)", "add_subdirectory(crypto)\n    if(CMAKE_CROSSCOMPILING)\n    else()\n    endif()").replace("endif()\nendif()", "else()\n        add_subdirectory(platform_sim)\n    endif()\nendif()"));
  expect("the entry in an else branch is caught", 1, quiet());

  clean();
  write(TOP_CMAKE, GOOD_TOP + "target_link_libraries(np_safety_mcu PRIVATE ${NP_BUILD_SIM})\n");
  expect("another command naming NP_BUILD_SIM in the top file is caught", 1, quiet());

  clean();
  write(SIM_CMAKE, GOOD_SIM.replace("if(CMAKE_CROSSCOMPILING)\n    return()\nendif()\n", ""));
  expect("a sim CMakeLists without the cross guard is caught", 1, quiet());

  clean();
  write(SIM_CMAKE, GOOD_SIM.replace("if(NOT NP_BUILD_TESTS OR NOT NP_BUILD_SIM)\n    return()\nendif()\n", ""));
  expect("a sim CMakeLists without the opt-in guard is caught", 1, quiet());

  clean();
  write(SIM_CMAKE, GOOD_SIM.replace("cmake_minimum_required(VERSION 3.20)\n", "cmake_minimum_required(VERSION 3.20)\nadd_library(np_sim_early STATIC src/a.c)\n"));
  expect("a target created before the guards is caught", 1, quiet());

  clean();
  write(SIM_CMAKE, GOOD_SIM.replace("if(CMAKE_CROSSCOMPILING)", "# if(CMAKE_CROSSCOMPILING)"));
  expect("a commented-out guard is not a guard", 1, quiet());

  // Vacuity: nothing to scan must fail, not pass.
  rmSync(root, { recursive: true, force: true });
  mkdirSync(root, { recursive: true });
  expect("an empty tree fails rather than passing on zero files", 1, quiet());

  // The probe's verdict, over fabricated `ninja -t targets all` text.
  const HOST = "np_sim_pbm: phony\nnp_sim_run_tests: phony\ncrypto/all: phony\n";
  const CLEAN_CROSS = "platform_sim/test: phony\nplatform_sim/all: phony\ncrypto/all: phony\n";
  const verdict = (h: string, c: string) => (probeVerdict(h, c).length ? 1 : 0);
  expect("probe: host has the sim, cross has none: passes", 0, verdict(HOST, CLEAN_CROSS));
  expect("probe: a cross configure that defines np_sim_pbm is caught", 1, verdict(HOST, CLEAN_CROSS + "np_sim_pbm: phony\n"));
  expect("probe: a host configure with no sim target fails the positive control", 1, verdict(CLEAN_CROSS, CLEAN_CROSS));

  rmSync(root, { recursive: true, force: true });
  console.log(failures ? `\nself-test: ${failures} FAILED` : "\nself-test: all passed");
  process.exit(failures ? 1 : 0);
}

process.exit(run(process.cwd(), process.argv.includes("--cross-probe")));
