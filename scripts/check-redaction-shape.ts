#!/usr/bin/env bun
/**
 * check-redaction-shape.ts — SHDR is written only by declared paths, and no
 * declared path can disclose a sensitive predicate by name or by a whole-word copy.
 *
 * CLAUDE.md §5.1 states the rule this enforces, and #272 is why it exists:
 *
 *   > A redaction applied conditionally on a sensitive predicate leaks that
 *   > predicate. It must be unconditional, or its pattern must not reveal the
 *   > predicate.
 *
 * CI-Kind: gate
 * CI-Self-Test: bun scripts/check-redaction-shape.ts --self-test
 * CI-Scans: every SHDR sink call in device firmware, the declared reporting paths, the classified status bits, and the open items that own the known violations
 * CI-Scan-Paths: firmware/** docs/status/pending-decisions.md
 *   (Declared here, not at the end: check-gate-coverage.ts reads only a file's
 *   first 80 lines.)
 *
 * The fault latch reported `count` unconditionally but zeroed `tick_ms` only for
 * NP_SAFETY_STATUS_CARDIAC, from two independent accessors. That made the
 * observable pair `count > 0 && tick_ms == 0` a one-bit cardiac oracle —
 * *strictly worse than not redacting at all*, because a bare relative SysTick
 * value is meaningless without a session record SHDR does not hold, whereas the
 * redaction PATTERN was self-interpreting.
 *
 * Until OI-PRIV-40 (NP-PRIV-ANALYSIS-004, #968) this gate inspected five
 * hand-named functions and nothing else, so every SHDR leak that review found
 * sat in a function it never looked at. The list is now derived from the code:
 * any function that calls an SHDR sink is a reporting path, and it must be
 * declared here or the gate fails.
 *
 * ── What is actually checked ─────────────────────────────────────────────────
 *
 *   1. Every NP_SAFETY_STATUS_* bit is classified sensitive or device-condition,
 *      both directions. "Is this predicate about the person or the device?" is
 *      CLAUDE.md §5.1's defining test, asked where a new status bit is added.
 *   2. Every call to an SHDR SINK (the byte-level writes, the public logger
 *      entries, the HAL hooks and the SHDR callbacks; SINKS below) in device
 *      firmware sits inside a function on REPORTING_PATHS. A function that
 *      writes SHDR and is not declared fails as an undeclared writer. A sink
 *      called from a preprocessor macro fails: a macro cannot be declared.
 *   3. Every function DEFINED in device firmware whose name says `shdr` is
 *      classified: a sink, a declared reporting path, or a non-writer. A new
 *      SHDR primitive cannot slip in without a decision here.
 *   4. Every declared path exists (a renamed path must not stop being guarded
 *      silently), and a declared WRITER still calls a sink (a stale declaration
 *      is removed, not kept as dead assurance).
 *   5. No declared path mentions a sensitive predicate, with comments and string
 *      literals stripped. Not "no branch": no reference.
 *   6. No declared path reads a whole safety status word (STATUS_SOURCES)
 *      unless the read is masked, in the same expression, by device-class bits
 *      only. A whole-word copy carries every bit in it, so a path that copies
 *      `status` reports IMPEDANCE and CARDIAC without naming either — the case
 *      rule 5 alone could never see (np_fault_latch_build_report).
 *
 * ── Known violations ─────────────────────────────────────────────────────────
 *
 * A violation that an open item already owns is declared in KNOWN_VIOLATIONS,
 * so the gate can land on `main` green while those items are worked. It is not
 * a waiver:
 *   - each entry names its open item WITH the review that raised it, because
 *     the `OI-PRIV-21` to `-28` IDs are duplicated on `main` (NP-PRIV-ANALYSIS-004
 *     and -005 both filed them), and the GitHub issue that disambiguates the row;
 *   - the entry fails once that row is no longer open in
 *     docs/status/pending-decisions.md (checked off, or moved away), and
 *   - the entry fails once the code no longer commits the violation it names.
 * So closing the item, or fixing the code, forces the entry out in the same change.
 *
 * ── The reach, stated narrowly ───────────────────────────────────────────────
 *
 * This reads function bodies, with a brace matcher, not a C parser. It does not
 * follow values: a writer that copies a sensitive value through a local of a
 * neutral name is not seen, and rule 6 knows only the status-word sources it
 * lists. It sees calls only by name, so a sink reached through a function
 * pointer of another name, or through a wrapper that is not itself a sink, is
 * seen at the wrapper's caller only if the wrapper is listed. It does not judge
 * whether a field is UHDR: the duration the session-end record carried until
 * OI-PRIV-21 (NP-PRIV-ANALYSIS-004) removed it was a §5.1 leak that no rule here
 * could recognise, which is why that writer was a known violation (undeclared)
 * and not a declared path until the record was reshaped. Declaring a path here
 * is a review claim — "this writer's record has a fixed shape and every field
 * has a §5.1 ruling" — that the gate makes checkable only in part.
 *
 * Simulator and host-test code (firmware/platform_sim/, any tests/ directory)
 * is not scanned: it is not built into a device image, and the simulator's
 * __wrap_np_log_shdr_* forward to the real logger, which is scanned.
 *
 * Branching on a sensitive predicate in a CONTROL path is required, not
 * forbidden — the cardiac interlock must act on NP_SAFETY_STATUS_CARDIAC.
 * A control path becomes a reporting path only by calling a sink.

 */
import { readFileSync, readdirSync, statSync, mkdtempSync, mkdirSync, writeFileSync, rmSync } from "fs";
import { join, relative } from "path";
import { tmpdir } from "os";

/** Where the status bits are defined. Classification completeness is checked
 *  against this file, so a new bit cannot be added without a decision here. */
const STATUS_HEADER = "firmware/common/include/np_spi_wire_types.h";

/** Where the open items that own KNOWN_VIOLATIONS live. */
const PENDING = "docs/status/pending-decisions.md";

type Sensitivity =
  /** Tells you something about the PERSON. Must never reach a reporting path. */
  | { kind: "sensitive"; why: string }
  /** Tells you something about the DEVICE. Free to appear anywhere. */
  | { kind: "device"; why: string };

/**
 * CLAUDE.md §5.1's defining test, applied bit by bit: does this record tell us
 * something about the person? If yes → sensitive.
 */
const PREDICATES: Record<string, Sensitivity> = {
  NP_SAFETY_STATUS_OK: { kind: "device", why: "absence of any fault" },
  NP_SAFETY_STATUS_FAULT: { kind: "device", why: "a fault occurred; kind unspecified" },
  NP_SAFETY_STATUS_WATCHDOG: { kind: "device", why: "SPI heartbeat lapsed — host-side liveness" },
  NP_SAFETY_STATUS_CUTOFF: { kind: "device", why: "stimulation was cut; says nothing about why" },
  NP_SAFETY_STATUS_IMPEDANCE: {
    kind: "sensitive",
    why:
      "electrode impedance is a tissue property. CLAUDE.md §5.1 routes raw VNS and EEG " +
      "impedance to UHDR and only the derived trend to SHDR, so a redaction conditioned " +
      "on this bit would leak a contact/tissue state the boundary keeps out. Whether the " +
      "bit itself may be reported is owed a ruling (OI-PRIV-40 (NP-PRIV-ANALYSIS-004), " +
      "docs/human-input/oi-priv-40-impedance-bit-in-fault-latch-status.md)",
  },
  NP_SAFETY_STATUS_THERMAL: { kind: "device", why: "NTC over-temperature — device condition" },
  NP_SAFETY_STATUS_CHARGE: { kind: "device", why: "charge-density ceiling reached — a dose limit, not a measurement of the wearer" },
  NP_SAFETY_STATUS_CARDIAC: {
    kind: "sensitive",
    why:
      "an HR change >15 BPM within 5 s of stimulation, i.e. a cardiac event in the wearer. " +
      "This is the #272 predicate. Note the hub deliberately publishes the cutoff itself to " +
      "SHDR as fault_log.fault_type 'CVNS_HR_CUTOFF' under the locked safety-interlock rule — " +
      "that disclosure is a separate decision. What is forbidden is a reporting path whose " +
      "SHAPE varies with this bit, which discloses it a second time and without the decision",
  },
  NP_SAFETY_STATUS_SIG_PENDING: { kind: "device", why: "signature verification in flight" },
};

/**
 * SHDR sinks: calling one of these IS an SHDR write. Matched as a called
 * identifier, so `s_ctx.shdr_cb(...)` matches `shdr_cb`.
 */
const SINKS: { name: string; why: string }[] = [
  { name: "shdr_write", why: "np_session_log.c byte-level SHDR append" },
  { name: "shdr_u8", why: "np_session_log.c one-byte SHDR append" },
  { name: "np_log_hal_shdr_append", why: "logger HAL: buffer → LittleFS SHDR partition" },
  { name: "np_log_session_start", why: "writes SHDR SESSION_OPEN" },
  { name: "np_log_session_end", why: "writes SHDR SESSION_END {session count, firmware version}" },
  { name: "np_log_shdr_fault", why: "SHDR fault_log entry" },
  { name: "np_log_shdr_zone_auth", why: "SHDR accessory authentication record" },
  { name: "np_log_shdr_pbm_socket_health", why: "SHDR PBM tile health record" },
  { name: "np_log_shdr_pbm_session_counts", why: "SHDR PBM counts record" },
  { name: "platform_shdr_write", why: "cervical VNS SHDR summary hook (np_cvns_session.c)" },
  { name: "np_platform_shdr_write_cvns", why: "the platform HAL that hook names" },
  { name: "np_pbm_hal_shdr_log_fault", why: "PBM HAL SHDR fault entry" },
  { name: "write_shdr_summary", why: "PBM session SHDR summary writer (np_pbm_session.c)" },
  { name: "write_combined_shdr", why: "T2 combined PBM SHDR writer (np_pbm_t2_combined.c)" },
  { name: "shdr_cb", why: "zone-announce SHDR callback" },
  { name: "np_shdr_write_accel_record", why: "accelerometer §G SHDR record" },
  { name: "np_shdr_write_accel_char_record", why: "accelerometer §H characterisation record" },
];

/**
 * Functions whose name says `shdr` and which write nothing to SHDR. Each is a
 * decision: a new `*shdr*` function that is neither here, a sink, nor a declared
 * path fails rule 3.
 */
const NON_WRITERS: { name: string; why: string }[] = [
  { name: "np_log_shdr_fault_dropped", why: "returns a RAM counter of refused faults" },
  { name: "np_log_hal_shdr_flush", why: "fsync of bytes already appended; adds none" },
  { name: "shdr_drain", why: "append-then-sync of the buffer shdr_write filled; adds no field" },
  { name: "np_shdr_crc32", why: "pure CRC over a record already built" },
  { name: "np_accel_shdr_evaluate", why: "decides whether a record is due; writes nothing" },
  { name: "np_factory_reset_hal_zero_shdr", why: "erases the partition; writes no record" },
  { name: "np_cvns_reenable_shdr_code", why: "maps a re-enable event to its SHDR code (the cutoff flag, else 0); writes nothing (REQ-FWHUB-91)" },
];

type Role =
  /** Calls a sink. Must still call one (rule 4). */
  | "writer"
  /** Fills or queues an SHDR record that another function writes. Calls no sink. */
  | "builder";

/**
 * The declared reporting paths. Every function that calls a sink must be here or
 * in KNOWN_VIOLATIONS. Each is scanned by rules 5 and 6.
 */
const REPORTING_PATHS: { file: string; fn: string; role: Role; note: string }[] = [
  {
    file: "firmware/safety_mcu/src/np_fault_latch.c",
    fn: "np_fault_latch_build_report",
    role: "builder",
    note: "#272's fixed-shape marshaller for any future fault-latch SPI read",
  },
  {
    file: "firmware/pbm/src/np_pbm_session.c",
    fn: "np_pbm_session_build_shdr_summary",
    role: "builder",
    note: "per-session PBM device metrics → SHDR",
  },
  // ── np_session_log.c: the hub logger ──
  {
    file: "firmware/hub_control/src/np_session_log.c",
    fn: "shdr_u8",
    role: "writer",
    note: "one-byte transport over shdr_write; carries a tag, no field",
  },
  {
    file: "firmware/hub_control/src/np_session_log.c",
    fn: "shdr_append_buffered",
    role: "writer",
    note: "hands the filled buffer to the HAL; adds no field",
  },
  {
    file: "firmware/hub_control/src/np_session_log.c",
    fn: "np_log_session_start",
    role: "writer",
    note: "dirty-session marker {tag, session_count} (OI-FMEA-09)",
  },
  {
    file: "firmware/hub_control/src/np_session_log.c",
    fn: "np_log_session_end",
    role: "writer",
    note: "SESSION_END {tag, session_count, firmware_version} and nothing else; ruled in data-architecture-detail.md §5.1 (REQ-FWHUB-89, OI-PRIV-21 (NP-PRIV-ANALYSIS-004))",
  },
  {
    file: "firmware/hub_control/src/np_session_log.c",
    fn: "np_log_shdr_fault",
    role: "builder",
    note: "queues {slot, type, code} for fault_queue_drain; discards session_ms for every caller (NP-FW-HUB-001 §6.3)",
  },
  {
    file: "firmware/hub_control/src/np_session_log.c",
    fn: "fault_queue_drain",
    role: "writer",
    note: "drains the fault queue: {tag, count, slot, type, code}",
  },
  {
    file: "firmware/hub_control/src/np_session_log.c",
    fn: "np_log_shdr_pbm_socket_health",
    role: "writer",
    note: "socket-lattice PBM health, idle only, no session count (F10–F14 rulings)",
  },
  {
    file: "firmware/hub_control/src/np_session_log.c",
    fn: "np_log_shdr_pbm_session_counts",
    role: "writer",
    note: "PBM counts, no location (F13)",
  },
  // ── accessory authentication: ruled by the principal 2026-11-23 (OI-PRIV-38 (NP-PRIV-ANALYSIS-004)) ──
  // The time of an insertion is not the time of use (a clinic fits a helmet for the day), so an
  // authentication event with no session count is SHDR. Fixed shape: {tag, slot, type, pass} (REQ-NASAL-16).
  {
    file: "firmware/hub_control/src/np_session_log.c",
    fn: "np_log_shdr_zone_auth",
    role: "writer",
    note: "0x83 {tag, slot, type, pass}, four bytes for every caller and result, no session count (REQ-NASAL-16 a)",
  },
  {
    file: "firmware/hub_control/modules/np_mod_intranasal.c",
    fn: "np_mod_intranasal_init",
    role: "writer",
    note: "intranasal insertion → np_log_shdr_zone_auth, pass or the init-time failure",
  },
  {
    file: "firmware/hub_control/modules/np_mod_intranasal.c",
    fn: "np_mod_intranasal_detect",
    role: "writer",
    note: "intranasal authentication failure, once per insertion (REQ-NASAL-16 b) → np_log_shdr_zone_auth",
  },
  {
    file: "firmware/hub_control/modules/np_mod_cvns.c",
    fn: "np_mod_cvns_init",
    role: "writer",
    note: "cervical VNS accessory insertion → np_log_shdr_zone_auth (pass)",
  },
  {
    file: "firmware/zone_announce/src/np_zone_announce.c",
    fn: "log_shdr",
    role: "writer",
    note: "zone authentication entry {zone_id, slot, pass/fail, debounce reads}, no session count; np_za_init() is not called by the hub",
  },
  // ── callers of the hub logger whose records are already ruled ──
  {
    file: "firmware/hub_control/src/np_pbm_socket_telem.c",
    fn: "np_pst_health_step",
    role: "writer",
    note: "idle pass over every occupied socket → np_log_shdr_pbm_socket_health",
  },
  {
    file: "firmware/hub_control/modules/np_mod_pbm.c",
    fn: "np_mod_pbm_control",
    role: "writer",
    note: "PBM thermal fault flag → np_log_shdr_fault",
  },
  {
    file: "firmware/hub_control/modules/np_mod_pbm.c",
    fn: "socket_drive_fault",
    role: "writer",
    note: "PBM drive-fault flag → np_log_shdr_fault",
  },
  {
    file: "firmware/hub_control/modules/np_mod_cvns.c",
    fn: "cvns_impedance_crossval_fail",
    role: "writer",
    note: "the ruled IMP_CROSSVAL divergence flag, no kΩ (OI-CVNS-HUB-11)",
  },
  {
    file: "firmware/hub_control/modules/np_mod_cvns.c",
    fn: "cvns_fault_cb",
    role: "writer",
    note: "interlock fault reason → np_log_shdr_fault (the locked safety-interlock rule)",
  },
  {
    file: "firmware/hub_control/src/np_session_runner.c",
    fn: "np_runner_run",
    role: "writer",
    note: "ruled records only: SESSION_OPEN; SESSION_END {session count, firmware version} (OI-PRIV-21); PBM session counts at every session end (F13); " +
      "the delivery-divergence flag with slot and modality, no magnitude, timestamp suppressed (OI-FMEA-09); fault entries with no slot (F11). " +
      "The visual record it feeds np_log_telemetry carries the ppx halt in UHDR and writes nothing to SHDR (REQ-FWHUB-88, OI-PRIV-22 (NP-PRIV-ANALYSIS-004))",
  },
  {
    file: "firmware/hub_control/src/np_session_runner.c",
    fn: "dispatch_command",
    role: "writer",
    note: "command refusal → np_log_shdr_fault(modality, gate code), no slot (F11 shape)",
  },
  {
    file: "firmware/hub_control/src/np_session_runner.c",
    fn: "gate_release",
    role: "writer",
    note: "unrestored socket after a hold → np_log_shdr_fault(PBM, code), no slot (F11 shape)",
  },
  // ── cervical VNS, reshaped by OI-PRIV-23 and OI-PRIV-28 (NP-PRIV-ANALYSIS-004) ──
  {
    file: "firmware/hub_control/src/np_hub_control_main.c",
    fn: "log_cvns_event",
    role: "writer",
    note: "the ruled cutoff flag only, via np_cvns_reenable_shdr_code; other lifecycle events are never written (REQ-FWHUB-91, OI-PRIV-28)",
  },
  {
    file: "firmware/cervical_vns/src/np_cvns_session.c",
    fn: "session_advance_to_fault",
    role: "writer",
    note: "the three-flag summary {impedance_check_pass, cutoff_occurred, fault_reason}, size-asserted (REQ-CVNS-15, OI-PRIV-23)",
  },
  {
    file: "firmware/cervical_vns/src/np_cvns_session.c",
    fn: "session_complete",
    role: "writer",
    note: "the same three-flag summary on normal completion, written on every session (REQ-CVNS-15, OI-PRIV-23)",
  },
  // ── PBM and accelerometer ──
  {
    file: "firmware/pbm/src/np_pbm_detect.c",
    fn: "np_pbm_detect_tick",
    role: "writer",
    note: "PBM detection fault entry → np_pbm_hal_shdr_log_fault",
  },
  {
    file: "firmware/pbm/src/np_pbm_drive.c",
    fn: "np_pbm_drive_poll_status",
    role: "writer",
    note: "PBM drive fault entry → np_pbm_hal_shdr_log_fault",
  },
  {
    file: "firmware/shdr/src/np_accel_shdr.c",
    fn: "np_accel_shdr_process_session_gap",
    role: "writer",
    note: "accelerometer §G/§H records (NP-FW-EMMC-002); budget fail-open is OI-PRIV-36, not a shape issue",
  },
];

/**
 * Violations an open item already owns. See the header: each entry fails once
 * its row is closed or the code stops committing the violation.
 */
type Rule = "undeclared-writer" | "sensitive-name" | "status-copy";
const KNOWN_VIOLATIONS: {
  rule: Rule;
  file: string;
  fn: string;
  /** Every owner must be an open row. The change that closes one removes it here. */
  owners: { oi: string; issue: number }[];
  why: string;
}[] = [
  {
    rule: "status-copy",
    file: "firmware/safety_mcu/src/np_fault_latch.c",
    fn: "np_fault_latch_build_report",
    owners: [{ oi: "OI-PRIV-40 (NP-PRIV-ANALYSIS-004)", issue: 968 }],
    why:
      "copies the whole latch status byte, IMPEDANCE included. No SHDR write uses the report " +
      "today; the IMPEDANCE ruling is owed before one does " +
      "(docs/human-input/oi-priv-40-impedance-bit-in-fault-latch-status.md)",
  },
  // Found by this gate (OI-PRIV-40): the PBM session SHDR summaries carry
  // duration_s and abort_reason, which §5.1 lists as UHDR ("session timestamps
  // and duration"). The HAL write is stubbed, so no byte reaches SHDR today.
  {
    rule: "undeclared-writer",
    file: "firmware/pbm/src/np_pbm_session.c",
    fn: "np_pbm_session_tick",
    owners: [{ oi: "OI-PRIV-40 (NP-PRIV-ANALYSIS-004)", issue: 968 }],
    why: "PBM session SHDR summary carries duration_s and abort_reason (stubbed write)",
  },
  {
    rule: "undeclared-writer",
    file: "firmware/pbm/src/np_pbm_session.c",
    fn: "np_pbm_session_abort",
    owners: [{ oi: "OI-PRIV-40 (NP-PRIV-ANALYSIS-004)", issue: 968 }],
    why: "PBM session SHDR summary carries duration_s and abort_reason (stubbed write)",
  },
  {
    rule: "undeclared-writer",
    file: "firmware/pbm/src/np_pbm_t2_combined.c",
    fn: "np_pbm_t2_tick",
    owners: [{ oi: "OI-PRIV-40 (NP-PRIV-ANALYSIS-004)", issue: 968 }],
    why: "T2 combined PBM SHDR summary carries duration_s and abort_reason (stubbed write)",
  },
  {
    rule: "undeclared-writer",
    file: "firmware/pbm/src/np_pbm_t2_combined.c",
    fn: "np_pbm_t2_abort",
    owners: [{ oi: "OI-PRIV-40 (NP-PRIV-ANALYSIS-004)", issue: 968 }],
    why: "T2 combined PBM SHDR summary carries abort_reason (stubbed write)",
  },
];

/**
 * Expressions that yield a whole safety status word. Rule 6 flags any of these
 * read in a declared path unless masked, in the same expression, by device bits.
 */
const STATUS_SOURCES: { re: RegExp; what: string }[] = [
  { re: /\bnp_fault_latch_get_status\s*\(/g, what: "np_fault_latch_get_status()" },
  { re: /\bnp_safety_spi_get_status\s*\(/g, what: "np_safety_spi_get_status()" },
  { re: /\bs_latch\s*\.\s*status\b/g, what: "s_latch.status" },
  { re: /\b\w*(?:mcu|safety|latch|heartbeat|hb)_status\b/g, what: "a safety status word" },
];

// ── Source handling ──────────────────────────────────────────────────────────

/**
 * Blank comments and string/char literals (and, with `pp`, preprocessor
 * directives) to spaces, keeping every newline and every offset, so positions in
 * the result are positions in the source.
 */
export function stripNonCode(src: string, pp = false): string {
  const out = src.split("");
  const blank = (a: number, b: number) => {
    for (let k = a; k < b && k < out.length; k++) if (out[k] !== "\n") out[k] = " ";
  };
  let i = 0;
  let lineStart = true;
  while (i < src.length) {
    const two = src.slice(i, i + 2);
    if (two === "/*") {
      const end = src.indexOf("*/", i + 2);
      const stop = end === -1 ? src.length : end + 2;
      blank(i, stop);
      i = stop;
    } else if (two === "//") {
      const end = src.indexOf("\n", i);
      const stop = end === -1 ? src.length : end;
      blank(i, stop);
      i = stop;
    } else if (src[i] === '"' || src[i] === "'") {
      const q = src[i]!;
      let j = i + 1;
      while (j < src.length && src[j] !== q && src[j] !== "\n") j += src[j] === "\\" ? 2 : 1;
      blank(i, j + 1);
      i = j + 1;
    } else if (pp && lineStart && src[i] === "#") {
      // A directive runs to an unescaped newline.
      let j = i;
      while (j < src.length && !(src[j] === "\n" && src[j - 1] !== "\\")) j++;
      blank(i, j);
      i = j;
    } else {
      if (src[i] === "\n") lineStart = true;
      else if (src[i] !== " " && src[i] !== "\t") lineStart = false;
      i++;
    }
  }
  return out.join("");
}

type Fn = { name: string; start: number; end: number; body: string };

/** Top-level function definitions in code that has been through stripNonCode(_, true). */
export function functionsIn(code: string): Fn[] {
  const fns: Fn[] = [];
  let depth = 0;
  let boundary = 0; // start of the current top-level declaration
  let open = -1;
  let name: string | null = null;
  let transparent = 0; // open `extern "C" {` blocks: their contents are top level
  for (let i = 0; i < code.length; i++) {
    const c = code[i];
    if (c === "{") {
      if (depth === 0 && /\bextern\s*$/.test(code.slice(boundary, i))) {
        transparent++;
        boundary = i + 1;
        continue;
      }
      if (depth === 0) {
        // Attributes are removed first with a pattern whose alternatives are
        // disjoint (no nested ambiguous quantifier, so no catastrophic
        // backtracking); then the header must end in `name(params)`.
        const header = code
          .slice(boundary, i)
          .replace(/__attribute__\s*\(\((?:[^()]|\([^()]*\))*\)\)/g, " ");
        const m = /\b([A-Za-z_]\w*)\s*\([^;{}=()]*(?:\([^;{}=()]*\)[^;{}=()]*)*\)\s*$/.exec(header);
        const kw = /^\s*(?:typedef|struct|union|enum)\b/.test(header) && !m;
        name = m && !/=/.test(header.slice(0, m.index)) && !kw ? m[1]! : null;
        if (name && /^(?:if|while|for|switch|return|sizeof)$/.test(name)) name = null;
        open = i;
      }
      depth++;
    } else if (c === "}") {
      if (depth === 0 && transparent > 0) {
        transparent--;
        boundary = i + 1;
        continue;
      }
      depth--;
      if (depth === 0) {
        if (name) fns.push({ name, start: open, end: i, body: code.slice(open + 1, i) });
        name = null;
        boundary = i + 1;
      }
      if (depth < 0) depth = 0;
    } else if (c === ";" && depth === 0) {
      boundary = i + 1;
    }
  }
  return fns;
}

/**
 * Body of a C function definition, brace-matched. Returns null when the
 * definition is not found — which the caller treats as a violation, never as
 * "nothing to check": a renamed function must not silently stop being guarded.
 */
export function functionBody(src: string, fn: string): string | null {
  return functionsIn(stripNonCode(src, true)).find((f) => f.name === fn)?.body ?? null;
}

const lineOf = (code: string, at: number) => code.slice(0, at).split("\n").length;

/** Every statement in a body that reads a whole status word without a device-only mask. */
export function unmaskedStatusReads(body: string, sensitiveBits: string[]): string[] {
  const hits: string[] = [];
  for (const raw of body.split(/[;{}]/)) {
    const stmt = raw.replace(/\s+/g, " ").trim();
    if (!stmt) continue;
    // The left side of a plain assignment is a destination, not a read.
    const asg = /^([^=!<>]*?[^=!<>+\-*/%&|^])=(?!=)(.*)$/.exec(stmt);
    const rhs = asg ? asg[2]! : stmt;
    for (const s of STATUS_SOURCES) {
      for (const m of rhs.matchAll(new RegExp(s.re.source, "g"))) {
        // A read ending in a call needs its argument list skipped first.
        let after = rhs.slice(m.index! + m[0].length);
        if (m[0].endsWith("(")) {
          let d = 1, k = 0;
          while (k < after.length && d > 0) { if (after[k] === "(") d++; else if (after[k] === ")") d--; k++; }
          after = after.slice(k);
        }
        const mask = /^\s*\)?\s*&\s*\(?\s*((?:NP_SAFETY_STATUS_\w+|\||\s)+)\)?/.exec(after);
        const bits = mask ? [...mask[1]!.matchAll(/NP_SAFETY_STATUS_\w+/g)].map((b) => b[0]) : [];
        const deviceOnly =
          bits.length > 0 && bits.every((b) => PREDICATES[b]?.kind === "device" && !sensitiveBits.includes(b));
        if (!deviceOnly) hits.push(`${s.what} in \`${stmt.slice(0, 80)}\``);
      }
    }
  }
  return hits;
}

/** Device-firmware C sources, relative to root: not the simulator, not tests. */
function firmwareSources(root: string): string[] {
  const out: string[] = [];
  const walk = (dir: string) => {
    let names: string[];
    try { names = readdirSync(dir); } catch { return; }
    for (const n of names) {
      const p = join(dir, n);
      const rel = relative(root, p);
      if (n === "tests" || rel === "firmware/platform_sim" || n.startsWith(".") || n === "build") continue;
      const st = statSync(p);
      if (st.isDirectory()) walk(p);
      else if (/\.(c|h)$/.test(n)) out.push(rel);
    }
  };
  walk(join(root, "firmware"));
  return out.sort();
}

/** Is the open-item row that owns an allow-list entry still open? */
export function rowState(pending: string, oi: string, issue: number): "open" | "closed" | "missing" {
  const id = oi.split(" ")[0]!;
  for (const line of pending.split("\n")) {
    const m = /^- \[([ xX])\] /.exec(line);
    if (!m) continue;
    if (!line.includes("`" + id + "`")) continue;
    if (!new RegExp(`#${issue}\\b`).test(line)) continue;
    return m[1] === " " ? "open" : "closed";
  }
  return "missing";
}

// ── Audit ────────────────────────────────────────────────────────────────────

type Finding = { rule: Rule | "config"; file: string; fn: string; msg: string };
type Audit = {
  violations: string[];
  tolerated: string[];
  pathsChecked: number;
  sinkCalls: number;
  sensitive: string[];
};

function audit(root: string): Audit {
  const findings: Finding[] = [];
  const cfg = (file: string, msg: string) => findings.push({ rule: "config", file, fn: "", msg });
  const sensitive = Object.entries(PREDICATES).filter(([, v]) => v.kind === "sensitive").map(([k]) => k);

  // 1. Classification completeness — both directions.
  let defined: string[] = [];
  try {
    const header = readFileSync(join(root, STATUS_HEADER), "utf8");
    defined = [...header.matchAll(/^#define\s+(NP_SAFETY_STATUS_\w+)/gm)].map((m) => m[1]!);
  } catch {
    cfg(STATUS_HEADER, "cannot be read — the predicate classification cannot be checked");
  }
  for (const bit of defined) {
    if (!PREDICATES[bit]) {
      cfg(bit, `defined but not classified — decide whether it tells you about the person (sensitive) or the device (device), per CLAUDE.md §5.1`);
    }
  }
  for (const bit of Object.keys(PREDICATES)) {
    if (defined.length && !defined.includes(bit)) cfg(bit, `classified here but no longer defined in ${STATUS_HEADER} — stale entry`);
  }

  // Parse every device-firmware source once.
  const parsed = new Map<string, { raw: string; code: string; fns: Fn[] }>();
  for (const rel of firmwareSources(root)) {
    const raw = readFileSync(join(root, rel), "utf8");
    const code = stripNonCode(raw, true);
    parsed.set(rel, { raw, code, fns: functionsIn(code) });
  }

  const sinkNames = new Set(SINKS.map((s) => s.name));
  const sinkRe = new RegExp(`\\b(${[...sinkNames].join("|")})\\s*\\(`, "g");
  const declared = new Map(REPORTING_PATHS.map((p) => [`${p.file}::${p.fn}`, p]));
  const callsSink = new Set<string>();
  let sinkCalls = 0;

  // 2. Every sink call sits in a declared path.
  for (const [rel, { raw, code, fns }] of parsed) {
    for (const m of code.matchAll(sinkRe)) {
      const at = m.index!;
      const inFn = fns.find((f) => at > f.start && at < f.end);
      if (!inFn) continue; // a prototype or a definition, not a call
      sinkCalls++;
      const key = `${rel}::${inFn.name}`;
      callsSink.add(key);
      if (!declared.has(key) && !findings.some((x) => x.rule === "undeclared-writer" && x.file === rel && x.fn === inFn.name)) {
        findings.push({
          rule: "undeclared-writer",
          file: rel,
          fn: inFn.name,
          msg: `${rel}:${lineOf(code, at)}: ${inFn.name}() writes SHDR through ${m[1]}() but is not a declared reporting path — declare it in REPORTING_PATHS once every field it writes has a §5.1 ruling and a fixed shape`,
        });
      }
    }
    // A sink called from a macro cannot be declared.
    const ppOnly = stripNonCode(raw, false);
    for (const m of ppOnly.matchAll(/^[ \t]*#[^\n]*(?:\\\n[^\n]*)*/gm)) {
      const s = new RegExp(sinkRe.source).exec(m[0]);
      if (s && !/^[ \t]*#\s*(?:include|pragma)/.test(m[0])) {
        findings.push({
          rule: "undeclared-writer",
          file: rel,
          fn: "#define",
          msg: `${rel}:${lineOf(ppOnly, m.index!)}: a preprocessor directive calls ${s[1]}() — an SHDR write in a macro cannot be declared; make it a function`,
        });
      }
    }
  }

  // 3. Every *shdr* function defined in device firmware is classified.
  const nonWriters = new Set(NON_WRITERS.map((n) => n.name));
  const declaredFns = new Set(REPORTING_PATHS.map((p) => p.fn));
  for (const [rel, { code, fns }] of parsed) {
    for (const f of fns) {
      if (!/shdr/i.test(f.name)) continue;
      if (sinkNames.has(f.name) || nonWriters.has(f.name) || declaredFns.has(f.name)) continue;
      if (KNOWN_VIOLATIONS.some((k) => k.fn === f.name && k.file === rel)) continue;
      cfg(rel, `${rel}:${lineOf(code, f.start)}: ${f.name}() names SHDR but is not classified — add it to SINKS, REPORTING_PATHS or NON_WRITERS`);
    }
  }

  // 4–6. Every declared path exists, a writer still writes, and its body is clean.
  let pathsChecked = 0;
  for (const p of REPORTING_PATHS) {
    const src = parsed.get(p.file);
    if (!src) {
      cfg(p.file, `${p.file}: declared reporting path ${p.fn}() but the file does not exist`);
      continue;
    }
    const f = src.fns.find((x) => x.name === p.fn);
    if (!f) {
      cfg(p.file, `${p.file}: ${p.fn}() is a declared reporting path but its definition was not found — if it was renamed, rename it here too; a guarded path must not stop being guarded silently`);
      continue;
    }
    pathsChecked++;
    if (p.role === "writer" && !callsSink.has(`${p.file}::${p.fn}`)) {
      cfg(p.file, `${p.file}: ${p.fn}() is declared a writer but calls no SHDR sink — remove the stale declaration`);
    }
    for (const bit of sensitive) {
      if (new RegExp(`\\b${bit}\\b`).test(f.body)) {
        findings.push({
          rule: "sensitive-name",
          file: p.file,
          fn: p.fn,
          msg: `${p.file}: ${p.fn}() references ${bit} — a reporting path whose shape can vary with a sensitive predicate leaks that predicate (CLAUDE.md §5.1, #272)`,
        });
      }
    }
    for (const hit of unmaskedStatusReads(f.body, sensitive)) {
      findings.push({
        rule: "status-copy",
        file: p.file,
        fn: p.fn,
        msg: `${p.file}: ${p.fn}() reads ${hit} — a whole status word carries every bit in it, ${sensitive.join(" and ")} included; mask it with device-class bits or rule the bits (CLAUDE.md §5.1)`,
      });
    }
  }

  // Known violations: tolerated only while their open item is open and the code matches.
  let pending = "";
  try { pending = readFileSync(join(root, PENDING), "utf8"); } catch {
    if (KNOWN_VIOLATIONS.length) cfg(PENDING, `cannot be read — the open items owning the known violations cannot be checked`);
  }
  const tolerated: string[] = [];
  const used = new Set<number>();
  const violations: string[] = [];
  for (const f of findings) {
    const k = f.rule === "config" ? -1 : KNOWN_VIOLATIONS.findIndex((e) => e.rule === f.rule && e.file === f.file && e.fn === f.fn);
    if (k >= 0) {
      used.add(k);
      const own = KNOWN_VIOLATIONS[k]!.owners.map((o) => `${o.oi}, #${o.issue}`).join("; ");
      if (!tolerated.some((t) => t.startsWith(`${f.file}::${f.fn} [${f.rule}]`))) {
        tolerated.push(`${f.file}::${f.fn} [${f.rule}] — ${f.msg.split(" — ")[0]}  [known: ${own}]`);
      }
    } else violations.push(f.msg);
  }
  KNOWN_VIOLATIONS.forEach((e, k) => {
    const tag = `known violation ${e.rule} ${e.file}::${e.fn}`;
    if (e.owners.length === 0) violations.push(`${tag}: names no open item`);
    if (!used.has(k)) {
      violations.push(`${tag}: the code no longer commits this violation — remove the entry (and declare the path if it still writes SHDR)`);
    }
    for (const o of e.owners) {
      if (!/^OI-[A-Z0-9]+-\d+ \(NP-[A-Z0-9-]+\)$/.test(o.oi)) {
        violations.push(`${tag}: ${o.oi} must be cited with the review that raised it, e.g. "OI-PRIV-40 (NP-PRIV-ANALYSIS-004)"`);
      }
      if (!pending) continue;
      const st = rowState(pending, o.oi, o.issue);
      if (st !== "open") {
        violations.push(`${tag}: its open item ${o.oi} is ${st === "closed" ? "checked off" : "no longer an open row citing #" + o.issue} in ${PENDING} — a closed item cannot excuse a violation; fix the code, or remove that owner if another still holds it`);
      }
    }
  });

  return { violations, tolerated, pathsChecked, sinkCalls, sensitive };
}

// ── Self-test ────────────────────────────────────────────────────────────────
if (process.argv.includes("--self-test")) {
  const failures: string[] = [];

  // The extractor is the part that can silently under-report: a body it fails to
  // find, or over-strips, would make the search vacuous.
  const SAMPLE = `
/* NP_SAFETY_STATUS_CARDIAC in a comment must not count */
#define NP_MACRO(x) do { x; } while (0)
static const int tbl[] = { 1, 2 };
extern "C" {
void np_x_report(np_report_t *out)
{
    const char *s = "NP_SAFETY_STATUS_CARDIAC in a string must not count";
    out->status = get_status();
    if (nested()) { out->slot = 1; }
}

void np_other(void) { NP_SAFETY_STATUS_CARDIAC; }
}
`;
  const body = functionBody(SAMPLE, "np_x_report");
  if (body === null) failures.push("extractor — failed to find a plain function body");
  else {
    if (/NP_SAFETY_STATUS_CARDIAC/.test(body)) failures.push("extractor — comment or string literal survived stripping");
    if (!/out->status/.test(body)) failures.push("extractor — live code was stripped");
    if (/np_other/.test(body)) failures.push("extractor — body ran past the closing brace");
    if (!/out->slot/.test(body)) failures.push("extractor — nested block truncated the body");
  }
  if (stripNonCode(SAMPLE, true).length !== SAMPLE.length) failures.push("extractor — stripping moved offsets");
  const names = functionsIn(stripNonCode(SAMPLE, true)).map((f) => f.name);
  if (names.join(",") !== "np_x_report,np_other") {
    failures.push(`extractor — expected functions np_x_report,np_other, got ${names.join(",")}`);
  }
  if (functionBody("static void np_attr(void) __attribute__((unused));\nstatic void np_attr(void) __attribute__((section(\".x\")))\n{\n    y = 1;\n}\n", "np_attr") === null) {
    failures.push("extractor — an __attribute__ after the parameter list hid the definition");
  }
  {
    const t0 = Date.now();
    functionsIn("int f(void)" + "))__attribute__((".repeat(5000) + "{ }");
    if (Date.now() - t0 > 2000) failures.push("extractor — header matching backtracks on repeated attributes");
  }
  if (functionBody(SAMPLE, "np_absent") !== null) failures.push("extractor — reported a body for a function that is not defined");
  if (functionBody("void f(void) {\n    np_x_report(&r);\n}\n", "np_x_report") !== null) {
    failures.push("extractor — treated a call site as a definition");
  }

  // The status-copy rule, in isolation.
  const sens = ["NP_SAFETY_STATUS_IMPEDANCE", "NP_SAFETY_STATUS_CARDIAC"];
  if (unmaskedStatusReads("out->status = np_fault_latch_get_status();", sens).length !== 1) {
    failures.push("status-copy — a whole-word copy was not seen");
  }
  if (unmaskedStatusReads("shdr_write(&mcu_status, 1U);", sens).length !== 1) {
    failures.push("status-copy — a whole status local passed to a write was not seen");
  }
  if (unmaskedStatusReads("out->status = np_fault_latch_get_status() & (NP_SAFETY_STATUS_THERMAL | NP_SAFETY_STATUS_CHARGE);", sens).length !== 0) {
    failures.push("status-copy — a device-only mask was not accepted");
  }
  if (unmaskedStatusReads("out->status = np_fault_latch_get_status() & (NP_SAFETY_STATUS_THERMAL | NP_SAFETY_STATUS_IMPEDANCE);", sens).length !== 1) {
    failures.push("status-copy — a mask that keeps a sensitive bit was accepted");
  }
  if (unmaskedStatusReads("mcu_status_out = 0U;", sens).length !== 0) {
    failures.push("status-copy — an assignment destination was counted as a read");
  }

  // The open-item lookup, including the duplicated-ID case.
  const PEND =
    "- [ ] **`OI-PRIV-21` (raised 2026-11-21, firmware privacy review; GitHub #949) — x.**\n" +
    "- [x] **`OI-PRIV-21` (raised 2026-11-21, NP-PRIV-ANALYSIS-005 F1) — y.**\n" +
    "- [x] **`OI-PRIV-22` (raised; GitHub #950) — z.**\n";
  if (rowState(PEND, "OI-PRIV-21 (NP-PRIV-ANALYSIS-004)", 949) !== "open") failures.push("rowState — the open row was not found by its issue");
  if (rowState(PEND, "OI-PRIV-22 (NP-PRIV-ANALYSIS-004)", 950) !== "closed") failures.push("rowState — a checked-off row read as open");
  if (rowState(PEND, "OI-PRIV-23 (NP-PRIV-ANALYSIS-004)", 951) !== "missing") failures.push("rowState — an absent row was found");

  // End-to-end, on fixture trees. Each declared path and each known violation
  // gets a body that satisfies (or commits) exactly what its declaration says.
  const box = mkdtempSync(join(tmpdir(), "np-redaction-"));
  const build = (files: Record<string, string>): string => {
    const root = mkdtempSync(join(box, "t-"));
    for (const [rel, body2] of Object.entries(files)) {
      mkdirSync(join(root, rel.split("/").slice(0, -1).join("/")), { recursive: true });
      writeFileSync(join(root, rel), body2);
    }
    return root;
  };
  const HEADER = Object.keys(PREDICATES).map((b, i) => `#define ${b} (1U << ${i})`).join("\n") + "\n";
  const openRows = (closed: number[] = []) =>
    [...new Set(KNOWN_VIOLATIONS.flatMap((k) => k.owners.map((o) => `${o.oi.split(" ")[0]}|${o.issue}`)))]
      .map((s) => {
        const [id, iss] = s.split("|");
        return `- [${closed.includes(Number(iss)) ? "x" : " "}] **\`${id}\` (GitHub #${iss}) — fixture.**`;
      })
      .join("\n") + "\n";
  type Body = (fn: string) => string;
  const writes: Body = (fn) => `void ${fn}(void)\n{\n    shdr_write(&c, 4U);\n}\n`;
  const builds: Body = (fn) => `void ${fn}(void)\n{\n    out->count = c;\n}\n`;
  const copies: Body = (fn) => `void ${fn}(void)\n{\n    out->status = np_fault_latch_get_status();\n}\n`;
  const leaky: Body = (fn) => `void ${fn}(void)\n{\n    if (s & NP_SAFETY_STATUS_CARDIAC) { out->t = 0U; }\n    shdr_write(&c, 4U);\n}\n`;
  const tree = (over: { path?: Body; extra?: Record<string, string>; skipKnown?: number; pending?: string } = {}) => {
    const byFile: Record<string, string[]> = {};
    REPORTING_PATHS.forEach((p) => {
      const known = KNOWN_VIOLATIONS.find((k) => k.file === p.file && k.fn === p.fn);
      const mk = over.path ?? (known?.rule === "status-copy" ? copies : p.role === "writer" ? writes : builds);
      (byFile[p.file] ??= []).push(mk(p.fn));
    });
    KNOWN_VIOLATIONS.forEach((k, i) => {
      if (i === over.skipKnown || k.rule !== "undeclared-writer") return;
      (byFile[k.file] ??= []).push(writes(k.fn));
    });
    const files: Record<string, string> = { [STATUS_HEADER]: HEADER, [PENDING]: over.pending ?? openRows() };
    for (const [f, bodies] of Object.entries(byFile)) files[f] = bodies.join("\n");
    return build({ ...files, ...(over.extra ?? {}) });
  };

  const expect = (label: string, root: string, needle: string | null) => {
    const { violations } = audit(root);
    if (needle === null) {
      if (violations.length) failures.push(`${label} — expected clean, got: ${violations[0]}`);
    } else if (!violations.some((x) => x.includes(needle))) {
      failures.push(`${label} — no violation matching ${JSON.stringify(needle)}; got: ${violations[0] ?? "none"}`);
    }
  };

  expect("the declared tree with its known violations passes", tree(), null);
  // The #272 defect itself.
  expect("a reporting path branching on a sensitive predicate is caught", tree({ path: leaky }), "leaks that predicate");
  expect(
    "an unclassified status bit is caught",
    tree({ extra: { [STATUS_HEADER]: HEADER + "#define NP_SAFETY_STATUS_NEWTHING (1U << 9)\n" } }),
    "defined but not classified",
  );
  const first = REPORTING_PATHS[0]!;
  const firstFile = REPORTING_PATHS.filter((p) => p.file === first.file && p.fn !== first.fn).map((p) => writes(p.fn)).join("\n");
  expect(
    "a renamed reporting path is caught rather than silently unguarded",
    tree({ extra: { [first.file]: firstFile + builds("np_renamed_away") } }),
    "definition was not found",
  );
  // OI-PRIV-40: the shapes the old gate could not see.
  expect(
    "an undeclared function writing SHDR is caught",
    tree({ extra: { "firmware/new/src/np_new.c": writes("np_new_summary") } }),
    "np_new_summary() writes SHDR through shdr_write() but is not a declared reporting path",
  );
  expect(
    "an SHDR write through a callback member is caught",
    tree({ extra: { "firmware/new/src/np_cb.c": "static void np_cb_log(void)\n{\n    s_ctx.shdr_cb(&e);\n}\n" } }),
    "np_cb_log() writes SHDR through shdr_cb()",
  );
  expect(
    "an SHDR write hidden in a macro is caught",
    tree({ extra: { "firmware/new/src/np_mac.c": "#define LOG_IT(x) np_log_shdr_fault(0U, 0U, (x), 0U)\n" } }),
    "an SHDR write in a macro cannot be declared",
  );
  expect(
    "an unclassified *shdr* function is caught",
    tree({ extra: { "firmware/new/src/np_s.c": "void np_new_shdr_thing(void)\n{\n    x = 1;\n}\n" } }),
    "names SHDR but is not classified",
  );
  expect(
    "a whole status word copied into a declared path is caught",
    tree({ path: (fn) => `void ${fn}(void)\n{\n    uint8_t b = mcu_status;\n    shdr_write(&b, 1U);\n    out->count = c;\n}\n` }),
    "a whole status word carries every bit in it",
  );
  const firstWriter = REPORTING_PATHS.find((p) => p.role === "writer")!;
  const firstWriterFile = REPORTING_PATHS.filter((p) => p.file === firstWriter.file)
    .map((p) => (p.fn === firstWriter.fn ? builds(p.fn) : writes(p.fn)))
    .concat(KNOWN_VIOLATIONS.filter((k) => k.file === firstWriter.file && k.rule === "undeclared-writer").map((k) => writes(k.fn)))
    .join("\n");
  expect(
    "a declared writer that no longer writes is caught as stale",
    tree({ extra: { [firstWriter.file]: firstWriterFile } }),
    "calls no SHDR sink — remove the stale declaration",
  );
  // The allow-list must not outlive its reason.
  const k0 = KNOWN_VIOLATIONS.findIndex((k) => k.rule === "undeclared-writer");
  expect("a known violation the code no longer commits is caught", tree({ skipKnown: k0 }), "the code no longer commits this violation");
  expect(
    "a known violation whose open item is checked off is caught",
    tree({ pending: openRows([KNOWN_VIOLATIONS[k0]!.owners[0]!.issue]) }),
    "is checked off in",
  );
  expect(
    "a known violation whose open item has gone is caught",
    tree({ pending: "" + "- [ ] **`OI-NONE-01` — nothing.**\n" }),
    "no longer an open row citing",
  );

  rmSync(box, { recursive: true, force: true });
  console.log("check-redaction-shape self-test");
  if (failures.length) {
    console.error(`\nSELF-TEST FAIL — ${failures.length} assertion(s):`);
    for (const f of failures) console.error("  " + f);
    process.exit(1);
  }
  console.log("  extractor proven on comments, strings, macros, initialisers, extern-C blocks, nesting, calls, absence;");
  console.log("  the #272 shape, an unclassified bit, a renamed path, an undeclared writer (direct, callback,");
  console.log("  macro), an unclassified *shdr* function, a whole status copy, a stale writer, and a known");
  console.log("  violation that is fixed, closed or orphaned all proven to fire");
  console.log("SELF-TEST PASS — the checker has teeth.");
  process.exit(0);
}

const ROOT = join(import.meta.dir, "..");
const result = audit(ROOT);

if (result.pathsChecked === 0 || result.sinkCalls === 0) {
  console.error("check-redaction-shape: no reporting path or SHDR sink call was found — refusing to pass vacuously.");
  process.exit(2);
}

console.log(
  `scanned: ${result.sinkCalls} SHDR sink call(s); ${result.pathsChecked} of ${REPORTING_PATHS.length} declared ` +
    `reporting path(s) checked against ${result.sensitive.length} sensitive predicate(s) ` +
    `(${Object.keys(PREDICATES).length} status bit(s) classified)`,
);
if (result.tolerated.length) {
  console.log(`\n${result.tolerated.length} known violation(s), each owned by an open item (they fail once it closes):`);
  for (const t of result.tolerated) console.log("  " + t);
}

if (result.violations.length) {
  console.error(`\n${result.violations.length} redaction-shape violation(s):\n`);
  for (const v of result.violations) console.error("  " + v);
  console.error(
    "\nCLAUDE.md §5.1: a redaction applied conditionally on a sensitive predicate\n" +
      "leaks that predicate. It must be unconditional, or its pattern must not reveal\n" +
      "the predicate. SHDR is written only by declared paths (OI-PRIV-40 (NP-PRIV-ANALYSIS-004)).",
  );
  process.exit(1);
}
console.log(
  "\nEvery SHDR write is in a declared path or is a known violation an open item owns, and no declared " +
    "path names a sensitive predicate or copies a whole status word. PASS",
);
process.exit(0);
