#!/usr/bin/env python3
"""
SHDR Fleet DB Schema CI Gate — OI-EMMC2-07
Document: NP-FW-EMMC-002 Rev 1 §G.5
Revision: A — 2026-06-03
BLOCKING: must PASS before SHDR fleet DB schema freeze.

Validates ci/shdr/shdr_fleet_schema.sql against all privacy constraints
from NP-FW-EMMC-002 §G.3 and §A.4:

  1. No prohibited accelerometer columns (raw g-force, orientation, drop_count,
     timestamps, impact_*, fall_*).
  2. Permitted accelerometer columns are drop_detected and maintenance_alert only.
  3. No personal-data columns (name, email, address, phone, postal).
  4. No columns of numeric types that could encode raw accelerometer values in
     tables whose name contains 'accel'.
  5. Warranty token is BYTEA — not a readable identifier.
  6. No foreign key references to a warranty registration DB table.
  7. No UHDR key-material columns (UKMD/WKMD ciphertext, Argon2id salt, nonce,
     tag) — the device-local UHDR key record (NP-FW-EMMC-002 §C) and especially
     its per-device salt must never reach the fleet DB (privacy review Finding 4).
  8. No wall-clock timestamp columns (TIME-01). No TIMESTAMP/TIMESTAMPTZ/TIME
     type anywhere; SHDR carries only a month-granular DATE retention anchor and
     integer counters for ordering. A per-row upload timestamp keyed to
     warranty_token is a docking-time side channel (§5.1 "no timestamps in SHDR").

Usage:
  python3 ci/test_shdr_schema.py                  # exits 0 on PASS, 1 on FAIL
  python3 ci/test_shdr_schema.py --verbose        # show all checks
  python3 ci/test_shdr_schema.py --schema PATH    # override schema file path

CI-Kind: gate
CI-Self-Test-Reads-Tree: ci/test_shdr_schema_selftest.py imports this module and drives its checks against the PRODUCTION schema — its vacuity guard asserts the real accel table is being seen, which is the half of the claim a fixture cannot make
CI-Self-Test: python3 ci/test_shdr_schema_selftest.py
CI-Scans: column definitions in the SHDR fleet schema
CI-Scan-Paths: ci/shdr/shdr_fleet_schema.sql firmware/shdr/include/np_accel_shdr.h
"""

from __future__ import annotations

import argparse
import re
import sys
from dataclasses import dataclass, field
from pathlib import Path

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------

SCHEMA_DEFAULT = Path(__file__).parent / "shdr" / "shdr_fleet_schema.sql"

# Column name patterns that must NEVER appear in the schema.
# Each entry is (pattern, human-readable reason).
PROHIBITED_COLUMN_PATTERNS: list[tuple[str, str]] = [
    (r"\bg_force\b",         "raw g-force value"),
    (r"\baccel_[xyz]\b",     "raw accelerometer axis"),
    (r"\baccel_x\b",         "raw accelerometer x-axis"),
    (r"\baccel_y\b",         "raw accelerometer y-axis"),
    (r"\baccel_z\b",         "raw accelerometer z-axis"),
    (r"\borientation_",      "orientation vector component"),
    (r"\bdrop_count\b",      "cumulative drop count (integer)"),
    (r"\bdrop_timestamp\b",  "individual drop event timestamp"),
    (r"\bimpact_",           "impact metric"),
    (r"\bfall_",             "fall detection metric"),
    (r"\brms_g\b",           "RMS g-force"),
    (r"\bpeak_g\b",          "peak g-force value"),
    (r"\baverage_g\b",       "average g-force"),
    (r"\braw_accel\b",       "raw accelerometer series"),
]

# Personal-data column name patterns prohibited in any SHDR table.
PROHIBITED_PII_PATTERNS: list[tuple[str, str]] = [
    # Word-boundary matches (standalone column names)
    (r"\bfirst_name\b",      "user first name"),
    (r"\blast_name\b",       "user last name"),
    (r"\bfull_name\b",       "user full name"),
    (r"\bowner_id\b",        "owner identifier (use warranty_token instead)"),
    (r"\buser_id\b",         "user identifier"),
    (r"\bcustomer_id\b",     "customer identifier"),
    (r"\bpatient_id\b",      "patient identifier"),
    (r"\bregistrant",        "registrant data"),
    # Substring matches — catch prefixed/suffixed variants like user_email, billing_address
    (r"email",               "email address"),
    (r"address",             "postal address"),
    (r"phone",               "phone number"),
    (r"postal",              "postal code"),
    (r"\bzip\b",             "ZIP code"),
]

# UHDR key-material column name patterns prohibited in any SHDR table.
# The two-layer UHDR key record (NP-FW-EMMC-002 §C) is strictly device-local;
# none of its fields — least of all the per-device Argon2id salt — may ever be
# copied into the fleet DB (privacy review 2026-07-08, Finding 4).  Patterns are
# specific so legitimate columns like 'coefficient_key' and 'warranty_token' are
# not matched.
PROHIBITED_KEY_MATERIAL_PATTERNS: list[tuple[str, str]] = [
    (r"\bukmd",           "UHDR master key material"),
    (r"\bwkmd",           "UHDR wrapper key material"),
    (r"argon2",           "Argon2id KDF field (salt/params)"),
    (r"\bsalt\b",         "per-device KDF salt (device correlator)"),
    (r"master_key",       "master key material"),
    (r"wrapper_key",      "wrapper key material"),
    (r"wrapped_key",      "wrapped key material"),
    (r"key_material",     "key material"),
    (r"ciphertext",       "encrypted key material"),
    (r"\bhuk\b",          "hardware unique key"),
    (r"\bhw_key\b",       "hardware key material"),
    (r"\bkdf_",           "KDF-derived field"),
]

# Column TYPES that must never appear anywhere in the SHDR schema.
# SHDR carries no wall clock finer than a month-granular DATE retention anchor
# (ingest_month / last_seen_month); per-row ordering is an integer counter, never
# a clock.  A per-row TIMESTAMP/TIMESTAMPTZ/TIME keyed to warranty_token
# reintroduces a docking-time side channel (uploads happen on USB-C connect),
# which the §5.1 "no timestamps in SHDR" rule and NP-FW-EMMC-002 §G forbid.
# DATE is permitted; DATE columns are expected to be month-truncated by their
# DEFAULT (DATE_TRUNC('month', NOW())::date) — see schema Rev B.
PROHIBITED_TIME_TYPE_PATTERNS: list[tuple[str, str]] = [
    (r"\bTIMESTAMPTZ\b",  "timestamptz — sub-day wall clock"),
    (r"\bTIMESTAMP\b",    "timestamp — sub-day wall clock"),
    (r"\bTIME\b",         "time-of-day column"),
]

# ---------------------------------------------------------------------------
# Rev D (2026-08-10) — socket + module re-key invariants
# ---------------------------------------------------------------------------

# ZONE-01: the retired fixed five-zone addressing must never come back.  It
# appeared in two shapes, and both are prohibited: as a KEY (pbm_zone_telemetry.
# zone_id CHECK BETWEEN 1 AND 5) and as a COLUMN SHAPE (thermal_profiles.
# zone1_peak_ntc_celsius .. zone5_...).  Zones are authored socket SETS in
# protocols/predefined/00-zones.npps and are re-cut with no hardware change
# (ZONE-1), so a zone identifier in a fleet table silently re-points historical
# rows on the next re-cut.  Address hardware by (socket_number, module_uid).
PROHIBITED_ZONE_COLUMN_PATTERNS: list[tuple[str, str]] = [
    (r"^zone_id$",       "fixed zone key (use socket_number + module_uid)"),
    (r"^zone_index$",    "fixed zone key (use socket_number + module_uid)"),
    (r"^zone_number$",   "fixed zone key (use socket_number + module_uid)"),
    (r"^zone\d+_",       "fixed per-zone column shape (use one row per socket+module)"),
    (r"_zone\d+$",       "fixed per-zone column shape (use one row per socket+module)"),
]

# DOSE-01: no per-socket dose / energy / irradiance / on-time / duty column
# anywhere in SHDR.  Per-zone PBM dose (J/cm²) is UHDR (CLAUDE.md §5.1).  This
# check exists because Rev D raises positional resolution from 5 zones to 80
# sockets: at that granularity an energy or duty figure per position
# reconstructs the user's treatment geography, and treatment geography implies
# indication.  SHDR records WHERE a module sits and HOW DEGRADED it is; never
# HOW MUCH ENERGY was delivered there.  Rotation duty optimisation therefore
# runs on-device against UHDR, and only the advice + outcome reach SHDR.
PROHIBITED_DOSE_COLUMN_PATTERNS: list[tuple[str, str]] = [
    (r"dose",          "delivered dose (UHDR — CLAUDE.md §5.1)"),
    (r"joule",         "delivered energy (UHDR)"),
    (r"_j_cm2",        "delivered fluence (UHDR)"),
    (r"irradiance",    "raw irradiance (UHDR — only the PD1/PD2 ratio is SHDR)"),
    (r"mw_cm2",        "raw irradiance (UHDR)"),
    (r"energy",        "delivered energy (UHDR)"),
    (r"on_time",       "per-position emitter on-time (duty proxy → treatment geography)"),
    (r"on_hours",      "per-position emitter on-hours (duty proxy → treatment geography)"),
    (r"duty_",         "per-position duty figure (→ treatment geography)"),
    (r"_duty$",        "per-position duty figure (→ treatment geography)"),
    (r"emitter_second", "per-position emitter seconds (duty proxy)"),
]

# SOCKET-01: socket numbers are 1-BASED project-wide (NUMBER-1, NP-HEX-ZM-001
# §3.3).  Socket 0 does not exist and is rejected, never clamped.  Firmware's
# np_hex_addr_t.socket_id is 0-based INDEX space and converts at its boundary;
# SHDR stores the NUMBER.  A CHECK admitting 0 means the boundary conversion was
# skipped somewhere, which for a table addressing PBM and tES elements is a
# wrong-site question, not a data-quality one.  Upper bound is the 7-bit major
# field (NP_HEXMAP_MAX_SOCKETS = 128); 80 sockets ship in v1.
SOCKET_COLUMN_SUFFIX = "socket_number"
SOCKET_CHECK_RE = re.compile(r"BETWEEN\s+1\s+AND\s+128", re.IGNORECASE)

# MODUID-01: a module UID is the 8-byte (NP_HEXMAP_UID_LEN) component
# identifier from np_module_map.  It must be opaque BYTEA — never a readable
# string that could carry a serial, a batch code, or anything user-linked — and
# must be length-constrained so a truncated or padded UID cannot alias one
# physical module onto another's lifetime record.
MODULE_UID_COLUMN = "module_uid"
MODULE_UID_LEN_CHECK_RE = re.compile(
    r"octet_length\s*\(\s*module_uid\s*\)\s*=\s*8", re.IGNORECASE
)

# These are the only accelerometer-derived column names permitted anywhere.
#
# DELIBERATELY NOT WIDENED BY THE Rev E CHARACTERISATION PROGRAMME.  Admitting
# the §H extended fields by adding them here would have deleted the protection
# for the whole schema in order to permit them in one table.  The exception is
# implemented instead as a NARROW, CONDITIONAL, SELF-REVOKING exemption computed
# by CHAR-01 below — see characterisation_exempt_columns().
PERMITTED_ACCEL_COLUMNS: frozenset[str] = frozenset({"drop_detected", "maintenance_alert"})

# ---------------------------------------------------------------------------
# Rev E (2026-08-12) — CHAR-01, the characterisation-window exception
# ---------------------------------------------------------------------------
#
# NP-FW-EMMC-002 §H admits a coarsened impact histogram to SHDR for the duration
# of a time-boxed, consented characterisation programme, because §G's two
# thresholds are unvalidated guesses and §G.3 bans every field that could
# validate them.
#
# THE SHAPE OF THE EXCEPTION IS THE POINT.  Three properties, each chosen against
# a specific failure mode:
#
#   1. It is ONE registered table.  Any OTHER table carrying a char_* marker is a
#      failure, so the marker cannot be sprayed around to launder columns
#      elsewhere.
#   2. It is CONDITIONAL on four structural markers AND on the firmware build
#      gate being open — and the gate reads the firmware constants by PARSING
#      the header, so schema and firmware cannot drift.  A missing or
#      unparseable header is a FAILURE, not a skip: guarding against the parse
#      ERRORING is not the same as guarding against it being WRONG, and a
#      renamed constant is the valid-but-wrong case.
#   3. It is SELF-REVOKING.  characterisation_exempt_columns() returns the
#      exempt set ONLY when check_characterisation_window() found nothing.
#      Weaken any marker and the exemption lapses in the same run, so the
#      impact_* columns immediately become ACCEL-01 violations again.  The
#      permission is derived from the check, never declared beside it.

FIRMWARE_ACCEL_HEADER = (
    Path(__file__).parent.parent / "firmware" / "shdr" / "include" / "np_accel_shdr.h"
)

# The single registered characterisation table.  Not a pattern — a name.
CHAR_TABLE = "shdr_accel_characterisation"

# Marker columns, and the CHECK each must carry on its own DDL line.  Asserted
# against the parsed column's raw_line, never by grepping the file: this schema's
# comment blocks discuss every one of these constraints at length, and a file-wide
# grep would be satisfied by the prose explaining why a constraint is required
# while the constraint itself was absent.
CHAR_MARKERS: list[tuple[str, str, str]] = [
    ("char_programme_id",
     r"CHECK\s*\(\s*char_programme_id\s*=\s*{programme_id}\s*\)",
     "programme id pinned by CHECK — consent to programme N must not authorise N+1"),
    ("char_consent_granted",
     r"CHECK\s*\(\s*char_consent_granted\s*\)",
     "consent CHECK satisfiable only by TRUE — a non-consented row must be "
     "UNSTORABLE, not merely unsent"),
    ("char_consent_epoch",
     r"CHECK\s*\(\s*char_consent_epoch\s*>=\s*1\s*\)",
     "consent epoch — a row must not outlive the grant that authorised it"),
    ("char_record_seq",
     r"CHECK\s*\(\s*char_record_seq\s+BETWEEN\s+1\s+AND\s+{budget}\s*\)",
     "record sequence bounded to the firmware budget — this bound IS the window"),
]

# Extended columns admitted in the registered table while CHAR-01 passes.  Every
# one of these deliberately matches a PROHIBITED_COLUMN_PATTERNS entry: naming
# them honestly is what makes this exemption visible in a diff instead of
# smuggling the data past under an innocuous name.
CHAR_EXTENDED_COLUMNS: frozenset[str] = frozenset(
    {f"impact_g_bin_{i}" for i in range(1, 9)} | {"impact_event_count"}
)

# Non-accel columns the registered table legitimately carries.
CHAR_STRUCTURAL_COLUMNS: frozenset[str] = frozenset({
    "id", "warranty_token", "gap_index", "ingest_month",
    "char_programme_id", "char_consent_granted", "char_consent_epoch",
    "char_record_seq",
})

# Tables containing 'accel' in their name must not have numeric data columns.
NUMERIC_TYPE_PATTERN = re.compile(
    r"\b(REAL|FLOAT|DOUBLE|NUMERIC|DECIMAL|INTEGER|INT|BIGINT|SMALLINT)\b",
    re.IGNORECASE,
)

# Prohibited cross-DB reference patterns (warranty registration table names).
PROHIBITED_CROSS_DB_PATTERNS: list[tuple[str, str]] = [
    (r"\bwarranty_registrations?\b",  "warranty_registration table (cross-DB join risk)"),
    (r"\bregistrants?\b",             "registrant table (cross-DB join risk)"),
    (r"\bwarranty_owners?\b",         "warranty_owner table (cross-DB join risk)"),
]


# ---------------------------------------------------------------------------
# Parser: extract column definitions from SQL DDL
# ---------------------------------------------------------------------------

@dataclass
class ColumnDef:
    table: str
    column: str
    col_type: str
    line_no: int
    raw_line: str


def parse_columns(sql: str) -> list[ColumnDef]:
    """
    Extract column definitions from CREATE TABLE statements.
    Handles multi-line CREATE TABLE blocks and ignores comments,
    constraints, and index lines.
    """
    columns: list[ColumnDef] = []
    current_table: str | None = None

    # Match CREATE TABLE name (
    create_re = re.compile(
        r"^\s*CREATE\s+TABLE\s+(?:IF\s+NOT\s+EXISTS\s+)?(\w+)\s*\(",
        re.IGNORECASE,
    )
    # Match a column definition: leading whitespace, identifier, type keyword
    col_re = re.compile(
        r"^\s{2,}(\w+)\s+([\w\s\(\),]+?)(?:\s+(?:NOT\s+NULL|DEFAULT|CHECK|REFERENCES|PRIMARY|UNIQUE).*)?$",
        re.IGNORECASE,
    )
    # Lines to skip: standalone constraint/keyword lines, closing parens, comments.
    # Use match() with a single anchored group so inline modifiers on column
    # definition lines (e.g. "... NOT NULL REFERENCES devices(...)") are NOT
    # matched — those are parsed by col_re, which already strips trailing modifiers.
    skip_re = re.compile(
        r"^\s*(?:--|CONSTRAINT\b|PRIMARY\s+KEY|FOREIGN\s+KEY|UNIQUE\b|CHECK\b|REFERENCES\b|ALTER\b|CREATE\s+INDEX|\);?\s*$)",
        re.IGNORECASE,
    )

    for line_no, line in enumerate(sql.splitlines(), start=1):
        stripped = line.strip()

        # Track current table
        m = create_re.match(line)
        if m:
            current_table = m.group(1).lower()
            continue

        if current_table is None:
            continue
        if not stripped or stripped.startswith("--"):
            continue
        if skip_re.match(line):
            # Closing paren resets table context
            if re.match(r"^\s*\);?\s*$", line):
                current_table = None
            continue

        # Strip any trailing inline comment BEFORE matching.  col_re's type char
        # class is [\w\s\(\),] — no '-' — and its trailing ".*" only engages
        # after a NOT NULL / DEFAULT / CHECK / REFERENCES / PRIMARY / UNIQUE
        # keyword.  So a NULLABLE column with an explanatory inline comment and
        # no other modifier — e.g.
        #     module_part_type  VARCHAR(24),  -- 'ZM-1064-SMART'
        # — matched NOTHING and was invisible to EVERY column check in this
        # file: PII-01, KEYMAT-01, TIME-01, TOKEN-01 included.  A PII column
        # would have passed the gate purely by being documented.  Found by
        # differential count against an independent enumeration of the DDL
        # while adding the Rev D tables, which introduced the first columns of
        # that shape.  raw_line keeps the original text, because SOCKET-01 and
        # MODUID-01 assert on the CHECK clause it carries.
        col_m = col_re.match(re.sub(r"--.*$", "", line).rstrip())
        if col_m:
            col_name = col_m.group(1).lower()
            col_type = col_m.group(2).strip().upper()
            # Skip SQL keywords that can appear as "column" names in the regex
            if col_name.upper() in {"CONSTRAINT", "PRIMARY", "FOREIGN", "UNIQUE", "CHECK", "INDEX"}:
                continue
            columns.append(ColumnDef(
                table=current_table,
                column=col_name,
                col_type=col_type,
                line_no=line_no,
                raw_line=line.rstrip(),
            ))

    return columns


# ---------------------------------------------------------------------------
# Check functions
# ---------------------------------------------------------------------------

@dataclass
class Failure:
    check_id: str
    description: str
    location: str  # "table.column:line_no" or "line:N"


def parse_firmware_constants(
    header: Path | None = None,
) -> tuple[dict[str, int] | None, str | None]:
    """
    Parse the three §H build-time constants out of firmware/shdr/np_accel_shdr.h.

    `header` defaults to None and resolves FIRMWARE_ACCEL_HEADER at CALL time,
    not at definition time.  Writing it as `header: Path = FIRMWARE_ACCEL_HEADER`
    binds the module global once, when the def is evaluated, so a test that
    swaps FIRMWARE_ACCEL_HEADER to point at a synthetic header silently keeps
    reading the real one — every falsification of the closed-window, renamed-
    constant and schema/firmware-disagreement cases then passes for the wrong
    reason.  That is exactly what happened on 2026-08-12: five of them failed
    loudly on first run because they asserted a rejection that never came.

    Returns (constants, None) or (None, reason).  A missing file, or any one
    constant that does not parse, is a REASON — never a silently-empty dict.
    That distinction is the whole point: 'the parse errored' and 'the parse
    found nothing because someone renamed a constant' look identical from the
    outside, and only the second is the case a fail-open guard misses.

    The values are read from the #define, not from the surrounding prose.  The
    header explains at length why the window is denominated in records, and a
    grep for the number would be satisfied by that explanation.
    """
    header = header if header is not None else FIRMWARE_ACCEL_HEADER
    if not header.exists():
        return None, f"firmware header not found: {header}"

    text = header.read_text(encoding="utf-8")
    wanted = {
        "window_enabled": r"^\s*#define\s+NP_ACCEL_CHAR_WINDOW_ENABLED\s+(\d+)\s*$",
        "programme_id":   r"^\s*#define\s+NP_ACCEL_CHAR_PROGRAMME_ID\s+(\d+)u?\s*$",
        "budget":         r"^\s*#define\s+NP_ACCEL_CHAR_RECORD_BUDGET\s+(\d+)u?\s*$",
    }
    out: dict[str, int] = {}
    for key, pattern in wanted.items():
        m = re.search(pattern, text, re.MULTILINE)
        if not m:
            return None, (
                f"NP_ACCEL_CHAR_{key.upper()} not parseable from {header.name} — "
                f"a renamed or reformatted constant must FAIL this gate, not skip it"
            )
        out[key] = int(m.group(1))
    return out, None


def check_characterisation_window(sql: str, columns: list[ColumnDef]) -> list[Failure]:
    """
    CHAR-01 — the characterisation-window exception (NP-FW-EMMC-002 §H).

    Runs in three situations and answers each differently:

      * No characterisation table, window closed  → PASS, nothing to permit.
      * No characterisation table, window OPEN    → PASS.  The firmware may be
        cut for the window before the schema lands; the reverse order is the
        dangerous one and is caught below.
      * Table present                             → every marker, the firmware
        agreement, AND the window being open are all required.

    The second-to-last of those is the falsification the brief asks for in
    direction (b): a build whose window has closed while the schema still
    carries the extended columns FAILS.  That is what stops the data outliving
    the programme by inattention.
    """
    failures: list[Failure] = []

    char_cols = [c for c in columns if c.table == CHAR_TABLE]

    # The marker must not appear anywhere but the registered table.  Without
    # this, the exemption's own signal could be pasted onto another table.
    for col in columns:
        if col.column.startswith("char_") and col.table != CHAR_TABLE:
            failures.append(Failure(
                check_id="CHAR-01",
                description=(
                    f"characterisation marker '{col.column}' appears in "
                    f"'{col.table}' — the §H exception is scoped to the single "
                    f"registered table '{CHAR_TABLE}' and nowhere else"
                ),
                location=f"{col.table}.{col.column}:{col.line_no}",
            ))

    if not char_cols:
        return failures  # nothing to permit; §G stands unqualified

    consts, reason = parse_firmware_constants()
    if consts is None:
        return failures + [Failure(
            check_id="CHAR-01",
            description=(
                f"{CHAR_TABLE} exists but the firmware constants could not be "
                f"read: {reason}. The schema may not carry §H columns that no "
                f"firmware build agrees to produce"
            ),
            location=str(FIRMWARE_ACCEL_HEADER),
        )]

    # Direction (b): the window has closed but the columns are still here.
    if consts["window_enabled"] == 0:
        failures.append(Failure(
            check_id="CHAR-01",
            description=(
                f"NP_ACCEL_CHAR_WINDOW_ENABLED is 0 — the characterisation "
                f"window is CLOSED — but '{CHAR_TABLE}' is still in the schema. "
                f"§H is retired whole when the window closes; the table goes "
                f"with it. Data must not outlive the programme that consented to it"
            ),
            location=f"{CHAR_TABLE}",
        ))

    by_name = {c.column: c for c in char_cols}

    for name, check_tpl, why in CHAR_MARKERS:
        col = by_name.get(name)
        if col is None:
            failures.append(Failure(
                check_id="CHAR-01",
                description=(
                    f"'{CHAR_TABLE}' is missing required marker '{name}' — {why}"
                ),
                location=CHAR_TABLE,
            ))
            continue
        if "NOT NULL" not in col.raw_line.upper():
            failures.append(Failure(
                check_id="CHAR-01",
                description=f"marker '{name}' is not NOT NULL — {why}",
                location=f"{CHAR_TABLE}.{name}:{col.line_no}",
            ))
        pattern = check_tpl.format(
            programme_id=consts["programme_id"], budget=consts["budget"]
        )
        if not re.search(pattern, col.raw_line, re.IGNORECASE):
            failures.append(Failure(
                check_id="CHAR-01",
                description=(
                    f"marker '{name}' lacks the required CHECK agreeing with the "
                    f"firmware (programme_id={consts['programme_id']}, "
                    f"budget={consts['budget']}) — {why}"
                ),
                location=f"{CHAR_TABLE}.{name}:{col.line_no}",
            ))

    # Positive assertion.  Without this the whole check passes on an EMPTY
    # registered table: every marker loop iterates zero times over the extended
    # set and reports nothing.  A gate that permits an exception must confirm
    # the exception is actually carrying what it was granted for.
    present_extended = {c.column for c in char_cols} & CHAR_EXTENDED_COLUMNS
    if not present_extended:
        failures.append(Failure(
            check_id="CHAR-01",
            description=(
                f"'{CHAR_TABLE}' exists but carries none of the §H extended "
                f"columns. Either it is vestigial and should be dropped, or the "
                f"columns were renamed out from under this gate"
            ),
            location=CHAR_TABLE,
        ))

    # Nothing beyond the enumerated extended set and the structural columns may
    # live in the exempted table — the exemption covers a list, not a location.
    for col in char_cols:
        if col.column in CHAR_EXTENDED_COLUMNS or col.column in CHAR_STRUCTURAL_COLUMNS:
            continue
        failures.append(Failure(
            check_id="CHAR-01",
            description=(
                f"unenumerated column '{col.column}' in '{CHAR_TABLE}'. The §H "
                f"exemption covers a fixed list of columns, not the table as a "
                f"whole; add it to CHAR_EXTENDED_COLUMNS only with a §H revision"
            ),
            location=f"{CHAR_TABLE}.{col.column}:{col.line_no}",
        ))

    return failures


def characterisation_exempt_columns(
    sql: str, columns: list[ColumnDef]
) -> set[tuple[str, str]]:
    """
    The (table, column) pairs ACCEL-01/ACCEL-02 skip — DERIVED, not declared.

    Returns the empty set whenever CHAR-01 found anything at all.  That is what
    makes the exception self-revoking: weaken a marker, close the window, or
    rename a firmware constant, and this returns nothing in the same run, so the
    impact_* columns are ACCEL-01 violations again with no separate step needed
    to withdraw the permission.
    """
    if check_characterisation_window(sql, columns):
        return set()
    return {
        (CHAR_TABLE, c.column)
        for c in columns
        if c.table == CHAR_TABLE and c.column in CHAR_EXTENDED_COLUMNS
    }


def check_prohibited_accel_columns(
    columns: list[ColumnDef], exempt: set[tuple[str, str]] | None = None
) -> list[Failure]:
    exempt = exempt or set()
    failures: list[Failure] = []
    for col in columns:
        if (col.table, col.column) in exempt:
            continue
        for pattern, reason in PROHIBITED_COLUMN_PATTERNS:
            if re.search(pattern, col.column, re.IGNORECASE):
                failures.append(Failure(
                    check_id="ACCEL-01",
                    description=f"Prohibited accelerometer column '{col.column}' ({reason})",
                    location=f"{col.table}.{col.column}:{col.line_no}",
                ))
    return failures


def check_accel_table_numeric_types(
    columns: list[ColumnDef], exempt: set[tuple[str, str]] | None = None
) -> list[Failure]:
    """
    In any table whose name contains 'accel', numeric column types are
    prohibited (would enable encoding raw values even under renamed columns).

    The §H characterisation table's name contains 'accel' and its histogram
    columns are SMALLINT, so it needs this exemption as well as ACCEL-01's — and
    it comes from the same derived set, so both lapse together.
    """
    exempt = exempt or set()
    failures: list[Failure] = []
    for col in columns:
        if "accel" not in col.table:
            continue
        if (col.table, col.column) in exempt:
            continue
        if col.table == CHAR_TABLE and col.column in CHAR_STRUCTURAL_COLUMNS:
            continue
        if col.column in PERMITTED_ACCEL_COLUMNS:
            continue
        # Well-known non-accel columns (identity, ordering counter, retention anchor)
        if col.column in {"id", "gap_index", "warranty_token",
                          "ingest_month", "last_seen_month",
                          "created_at", "updated_at"}:
            continue
        if NUMERIC_TYPE_PATTERN.search(col.col_type):
            failures.append(Failure(
                check_id="ACCEL-02",
                description=(
                    f"Numeric column '{col.column}' ({col.col_type}) in accel table "
                    f"'{col.table}' — only BOOLEAN permitted for accel-derived fields"
                ),
                location=f"{col.table}.{col.column}:{col.line_no}",
            ))
    return failures


def check_permitted_accel_columns_present(columns: list[ColumnDef]) -> list[Failure]:
    """Verify the required boolean accel columns exist in shdr_accel_records."""
    failures: list[Failure] = []
    accel_table_cols = {
        col.column for col in columns if col.table == "shdr_accel_records"
    }
    for required in PERMITTED_ACCEL_COLUMNS:
        if required not in accel_table_cols:
            failures.append(Failure(
                check_id="ACCEL-03",
                description=(
                    f"Required accelerometer column '{required}' missing from "
                    f"shdr_accel_records — per NP-FW-EMMC-002 §G.2"
                ),
                location="shdr_accel_records",
            ))
    return failures


def check_pii_columns(columns: list[ColumnDef]) -> list[Failure]:
    failures: list[Failure] = []
    for col in columns:
        for pattern, reason in PROHIBITED_PII_PATTERNS:
            if re.search(pattern, col.column, re.IGNORECASE):
                failures.append(Failure(
                    check_id="PII-01",
                    description=f"Personal-data column '{col.column}' ({reason}) in SHDR table",
                    location=f"{col.table}.{col.column}:{col.line_no}",
                ))
    return failures


def check_key_material_columns(columns: list[ColumnDef]) -> list[Failure]:
    """No UHDR key-material columns may appear in any SHDR table (Finding 4)."""
    failures: list[Failure] = []
    for col in columns:
        for pattern, reason in PROHIBITED_KEY_MATERIAL_PATTERNS:
            if re.search(pattern, col.column, re.IGNORECASE):
                failures.append(Failure(
                    check_id="KEYMAT-01",
                    description=(
                        f"UHDR key-material column '{col.column}' ({reason}) in SHDR "
                        f"table — the UKMD record is device-local (NP-FW-EMMC-002 §C)"
                    ),
                    location=f"{col.table}.{col.column}:{col.line_no}",
                ))
    return failures


def check_prohibited_time_types(columns: list[ColumnDef]) -> list[Failure]:
    """
    No wall-clock time columns anywhere in SHDR (TIME-01). Ordering is provided
    by integer counters (session_index / gap_index / *_at_* / *_count); retention
    uses a month-granular DATE anchor. A TIMESTAMP/TIMESTAMPTZ/TIME column keyed
    to warranty_token is a per-row docking-time correlator — prohibited per
    NP-FW-EMMC-002 §G and the §5.1 "no timestamps in SHDR" boundary rule.
    DATE (day/month granularity) is permitted.
    """
    failures: list[Failure] = []
    for col in columns:
        ctype = col.col_type.upper()
        for pattern, reason in PROHIBITED_TIME_TYPE_PATTERNS:
            if re.search(pattern, ctype):
                failures.append(Failure(
                    check_id="TIME-01",
                    description=(
                        f"Prohibited time-type column '{col.column}' ({col.col_type}) "
                        f"in '{col.table}' ({reason}) — use a month-granular DATE "
                        f"retention anchor plus an integer counter for ordering"
                    ),
                    location=f"{col.table}.{col.column}:{col.line_no}",
                ))
                break
    return failures


def check_prohibited_zone_columns(columns: list[ColumnDef]) -> list[Failure]:
    """ZONE-01 — the retired fixed five-zone addressing must not reappear."""
    failures: list[Failure] = []
    for col in columns:
        for pattern, reason in PROHIBITED_ZONE_COLUMN_PATTERNS:
            if re.search(pattern, col.column, re.IGNORECASE):
                failures.append(Failure(
                    check_id="ZONE-01",
                    description=(
                        f"Retired fixed-zone column '{col.column}' in '{col.table}' "
                        f"({reason}) — zones are authored socket sets in "
                        f"00-zones.npps and are re-cut without a hardware change"
                    ),
                    location=f"{col.table}.{col.column}:{col.line_no}",
                ))
                break
    return failures


def check_prohibited_dose_columns(columns: list[ColumnDef]) -> list[Failure]:
    """DOSE-01 — no per-position dose / energy / irradiance / duty in SHDR."""
    failures: list[Failure] = []
    for col in columns:
        for pattern, reason in PROHIBITED_DOSE_COLUMN_PATTERNS:
            if re.search(pattern, col.column, re.IGNORECASE):
                failures.append(Failure(
                    check_id="DOSE-01",
                    description=(
                        f"Delivered-energy column '{col.column}' in '{col.table}' "
                        f"({reason}) — at 80-socket resolution a per-position dose "
                        f"or duty figure reconstructs treatment geography"
                    ),
                    location=f"{col.table}.{col.column}:{col.line_no}",
                ))
                break
    return failures


def check_socket_number_domain(columns: list[ColumnDef]) -> list[Failure]:
    """
    SOCKET-01 — every socket_number column is CHECK-constrained to 1..128.

    Asserted on the parsed column's own DDL line, not by grepping the file: the
    Rev D comment blocks talk about socket numbering at length, and a file-wide
    grep for "BETWEEN 1 AND 128" would be satisfied by that prose while the
    actual column carried no constraint at all.
    """
    failures: list[Failure] = []
    for col in columns:
        if not col.column.endswith(SOCKET_COLUMN_SUFFIX):
            continue
        if "SMALLINT" not in col.col_type.upper():
            failures.append(Failure(
                check_id="SOCKET-01",
                description=(
                    f"socket column '{col.column}' in '{col.table}' is "
                    f"'{col.col_type}' — must be SMALLINT"
                ),
                location=f"{col.table}.{col.column}:{col.line_no}",
            ))
        if not SOCKET_CHECK_RE.search(col.raw_line):
            failures.append(Failure(
                check_id="SOCKET-01",
                description=(
                    f"socket column '{col.column}' in '{col.table}' lacks a "
                    f"CHECK (... BETWEEN 1 AND 128) — socket numbers are 1-based "
                    f"(NUMBER-1); socket 0 does not exist and a 0 lower bound "
                    f"means a firmware 0-based socket_id leaked past its boundary"
                ),
                location=f"{col.table}.{col.column}:{col.line_no}",
            ))
    return failures


def check_module_uid_columns(columns: list[ColumnDef]) -> list[Failure]:
    """
    MODUID-01 — every module_uid column is BYTEA and length-checked to 8 bytes.

    Length matters for correctness as well as opacity: module_life is keyed
    UNIQUE on module_uid, so a truncated or padded UID would collapse two
    physical modules onto one lifetime record, or split one across two.
    """
    failures: list[Failure] = []
    for col in columns:
        if col.column != MODULE_UID_COLUMN:
            continue
        if "BYTEA" not in col.col_type.upper():
            failures.append(Failure(
                check_id="MODUID-01",
                description=(
                    f"module_uid in '{col.table}' is '{col.col_type}' — must be "
                    f"BYTEA (opaque 8-byte component identifier, "
                    f"NP_HEXMAP_UID_LEN)"
                ),
                location=f"{col.table}.{col.column}:{col.line_no}",
            ))
        if not MODULE_UID_LEN_CHECK_RE.search(col.raw_line):
            failures.append(Failure(
                check_id="MODUID-01",
                description=(
                    f"module_uid in '{col.table}' lacks a CHECK on "
                    f"octet_length(module_uid) = 8 — an off-length UID aliases "
                    f"one physical module onto another's lifetime record"
                ),
                location=f"{col.table}.{col.column}:{col.line_no}",
            ))
    return failures


def check_module_life_partition(sql: str) -> list[Failure]:
    """
    LIFE-01 — module_life must be keyed (warranty_token, module_uid), never
    module_uid alone, and its accumulators must be monotonicity-enforced.

    This is the one structural invariant of Rev D that none of the other checks
    can see, and getting it wrong fails SILENTLY. A row keyed on module_uid
    alone is written by whichever device currently holds the module, and there
    is no ordering source to resolve competing writes: session_index and
    power_cycle_index are per-device and incomparable, ingest_month is
    month-granular, and a wall clock is forbidden by TIME-01. Last-writer-wins
    then regresses the lifetime accumulators whenever one device re-docks after
    another has advanced them — and because the degradation slope and
    remaining-life bucket are DERIVED from those counters, the corruption never
    surfaces as a visibly wrong value.

    The partition makes every row single-writer; fleet life is the SUM of the
    per-device partials, which is order-free because addition is commutative.
    The trigger is what makes the counters grow-only in the database rather than
    by convention in the ingest job.

    Checked against the parsed DDL text of the table, not by grepping the file:
    the header comment block discusses both keyings at length, so a file-wide
    grep for "UNIQUE (warranty_token, module_uid)" would be satisfied by the
    prose explaining why it is required while the constraint itself was absent.
    """
    failures: list[Failure] = []
    m = re.search(r"CREATE\s+TABLE\s+module_life\s*\((.*?)\n\);", sql,
                  re.IGNORECASE | re.DOTALL)
    if not m:
        return [Failure(
            check_id="LIFE-01",
            description="module_life table not found — per-module lifetime accumulators are the point of Rev D",
            location="shdr_fleet_schema.sql",
        )]
    body = re.sub(r"--.*$", "", m.group(1), flags=re.MULTILINE)

    if not re.search(r"UNIQUE\s*\(\s*warranty_token\s*,\s*module_uid\s*\)", body, re.IGNORECASE):
        failures.append(Failure(
            check_id="LIFE-01",
            description=(
                "module_life lacks UNIQUE (warranty_token, module_uid) — a row "
                "keyed on module_uid alone has multiple writers and no ordering "
                "source under TIME-01, so its accumulators regress silently"
            ),
            location="module_life",
        ))
    if re.search(r"UNIQUE\s*\(\s*module_uid\s*\)", body, re.IGNORECASE):
        failures.append(Failure(
            check_id="LIFE-01",
            description=(
                "module_life is keyed UNIQUE (module_uid) alone — this is the "
                "multi-writer register that cannot be resolved without a clock; "
                "partition by warranty_token and SUM the partials instead"
            ),
            location="module_life",
        ))
    if not re.search(r"CREATE\s+TRIGGER\s+trg_module_life_monotonic", sql, re.IGNORECASE):
        failures.append(Failure(
            check_id="LIFE-01",
            description=(
                "module_life has no monotonicity trigger — the SUM-of-partials "
                "rollup is only correct while each partial is grow-only, and a "
                "delta-style writer would violate that without any query looking wrong"
            ),
            location="module_life",
        ))
    return failures


def check_cross_db_references(sql: str) -> list[Failure]:
    """Detect any reference to warranty registration DB table names."""
    failures: list[Failure] = []
    for line_no, line in enumerate(sql.splitlines(), start=1):
        stripped = line.strip()
        if stripped.startswith("--"):
            continue
        for pattern, reason in PROHIBITED_CROSS_DB_PATTERNS:
            if re.search(pattern, stripped, re.IGNORECASE):
                failures.append(Failure(
                    check_id="NOJOIN-01",
                    description=f"Cross-DB reference: {reason}",
                    location=f"line:{line_no}",
                ))
    return failures


def check_warranty_token_type(columns: list[ColumnDef]) -> list[Failure]:
    """Warranty token must be BYTEA (opaque binary) in all tables."""
    failures: list[Failure] = []
    for col in columns:
        if col.column == "warranty_token":
            if "BYTEA" not in col.col_type.upper():
                failures.append(Failure(
                    check_id="TOKEN-01",
                    description=(
                        f"warranty_token in '{col.table}' is '{col.col_type}' — "
                        f"must be BYTEA (opaque 32-byte binary per NP-FW-EMMC-002 §A.2)"
                    ),
                    location=f"{col.table}.{col.column}:{col.line_no}",
                ))
    return failures


# ---------------------------------------------------------------------------
# Runner
# ---------------------------------------------------------------------------

@dataclass
class CheckResult:
    passed: list[str] = field(default_factory=list)
    failures: list[Failure] = field(default_factory=list)
    # What the parser actually saw. Printed on both the PASS and FAIL paths so a
    # result can never be read without the population it was computed over.
    scanned: dict[str, int] = field(default_factory=dict)


def check_parse_not_vacuous(columns: list[ColumnDef]) -> list[Failure]:
    """
    VACUITY-01 — the parser must actually be seeing the schema.

    Most checks here are negative: they pass when nothing prohibited is found,
    iterating the parsed column list to do it. An empty list satisfies all of
    them. That is precisely how TOKEN-01 in this file passed vacuously until
    #118 — skip_re had one unanchored alternation branch and the call site used
    re.search(), so every column with an inline REFERENCES modifier was dropped.

    ci/test_shdr_schema_selftest.py already asserts the accel population is
    non-empty. This puts the same guard in the gate itself, so it holds on every
    run rather than only where the self-test happens to look.
    """
    if columns:
        return []
    return [Failure(
        check_id="VACUITY-01",
        description=(
            "parse_columns() returned 0 columns — every column-iterating check "
            "in this gate is passing over nothing"
        ),
        location="shdr_fleet_schema.sql:parse_columns",
    )]


def run_all_checks(sql: str, verbose: bool = False) -> CheckResult:
    result = CheckResult()
    columns = parse_columns(sql)

    def run(name: str, fn, *args) -> None:
        found = fn(*args)
        if found:
            result.failures.extend(found)
            if verbose:
                print(f"  FAIL  {name}: {len(found)} violation(s)")
        else:
            result.passed.append(name)
            if verbose:
                print(f"  PASS  {name}")

    # CHAR-01 runs FIRST because ACCEL-01/ACCEL-02 consume its verdict.  The
    # exemption is the output of the check, not a constant beside it — so a
    # CHAR-01 failure withdraws the permission in the same run.
    exempt = characterisation_exempt_columns(sql, columns)

    run("CHAR-01: §H characterisation window is bounded and consented",
        check_characterisation_window, sql, columns)
    run("ACCEL-01: no prohibited accelerometer columns",
        check_prohibited_accel_columns, columns, exempt)
    run("ACCEL-02: no numeric types in accel tables",
        check_accel_table_numeric_types, columns, exempt)
    run("ACCEL-03: required boolean accel columns present",
        check_permitted_accel_columns_present, columns)
    run("PII-01:   no personal-data columns",
        check_pii_columns, columns)
    run("KEYMAT-01: no UHDR key-material columns",
        check_key_material_columns, columns)
    run("TIME-01:  no wall-clock timestamp columns",
        check_prohibited_time_types, columns)
    run("NOJOIN-01: no cross-DB table references",
        check_cross_db_references, sql)
    run("TOKEN-01: warranty_token is BYTEA",
        check_warranty_token_type, columns)
    run("ZONE-01:  no retired fixed-zone columns",
        check_prohibited_zone_columns, columns)
    run("DOSE-01:  no per-position dose/energy/duty columns",
        check_prohibited_dose_columns, columns)
    run("SOCKET-01: socket_number is SMALLINT CHECK 1..128",
        check_socket_number_domain, columns)
    run("MODUID-01: module_uid is BYTEA, length-checked to 8",
        check_module_uid_columns, columns)
    run("LIFE-01:  module_life partitioned by device + monotonic",
        check_module_life_partition, sql)
    run("VACUITY-01: parser sees the schema",
        check_parse_not_vacuous, columns)

    result.scanned = {
        "columns": len(columns),
        "tables": len({c.table for c in columns}),
    }
    return result


def _scanned_line(result: CheckResult) -> str:
    """The machine-readable population line. `scanned: <int>` leading the line is
    the contract scripts/check-gate-coverage.ts probes for — a PASS that names no
    population is the shape #118 shipped in."""
    s = result.scanned
    if not s:
        return "scanned: 0 columns (not recorded)"
    return f"scanned: {s['columns']} column(s) across {s['tables']} table(s)"


def main() -> int:
    parser = argparse.ArgumentParser(description="SHDR schema CI gate (OI-EMMC2-07)")
    parser.add_argument("--schema", type=Path, default=SCHEMA_DEFAULT,
                        help="Path to SHDR fleet DB schema SQL file")
    parser.add_argument("--verbose", "-v", action="store_true",
                        help="Show all check results including PASS")
    args = parser.parse_args()

    if not args.schema.exists():
        print(f"ERROR: Schema file not found: {args.schema}")
        return 1

    sql = args.schema.read_text(encoding="utf-8")

    print(f"NeurOne SHDR Schema CI Gate — OI-EMMC2-07")
    print(f"Schema: {args.schema}")
    print(f"Checks: NP-FW-EMMC-002 Rev 1 §G.3, §G.4, §A.4")
    print()

    if args.verbose:
        print("Running checks:")

    result = run_all_checks(sql, verbose=args.verbose)

    if args.verbose:
        print()

    if result.failures:
        print(f"RESULT: FAIL — {len(result.failures)} violation(s) found")
        print(_scanned_line(result) + "\n")
        for f in result.failures:
            print(f"  [{f.check_id}] {f.description}")
            print(f"         at {f.location}")
        print()
        print("Schema freeze is BLOCKED until all violations are resolved.")
        print("Reference: NP-FW-EMMC-002 Rev 1 §G.5 (OI-EMMC2-07)")
        return 1

    checks_run = len(result.passed)
    print(f"RESULT: PASS — {checks_run} check(s) passed, 0 violations")
    print(_scanned_line(result))
    print()
    print("SHDR fleet DB schema freeze gate is CLEARED.")
    print("OI-EMMC2-07: PASS — this result must be recorded in NP-COORD-001.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
