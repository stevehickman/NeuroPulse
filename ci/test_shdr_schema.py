#!/usr/bin/env python3
"""
SHDR Fleet DB Schema CI Gate — OI-EMMC2-07
Document: NP-FW-EMMC-002 Rev A §G.5
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

Usage:
  python3 ci/test_shdr_schema.py                  # exits 0 on PASS, 1 on FAIL
  python3 ci/test_shdr_schema.py --verbose        # show all checks
  python3 ci/test_shdr_schema.py --schema PATH    # override schema file path
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

# These are the only accelerometer-derived column names permitted anywhere.
PERMITTED_ACCEL_COLUMNS: frozenset[str] = frozenset({"drop_detected", "maintenance_alert"})

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

        col_m = col_re.match(line)
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


def check_prohibited_accel_columns(columns: list[ColumnDef]) -> list[Failure]:
    failures: list[Failure] = []
    for col in columns:
        for pattern, reason in PROHIBITED_COLUMN_PATTERNS:
            if re.search(pattern, col.column, re.IGNORECASE):
                failures.append(Failure(
                    check_id="ACCEL-01",
                    description=f"Prohibited accelerometer column '{col.column}' ({reason})",
                    location=f"{col.table}.{col.column}:{col.line_no}",
                ))
    return failures


def check_accel_table_numeric_types(columns: list[ColumnDef]) -> list[Failure]:
    """
    In any table whose name contains 'accel', numeric column types are
    prohibited (would enable encoding raw values even under renamed columns).
    """
    failures: list[Failure] = []
    for col in columns:
        if "accel" not in col.table:
            continue
        if col.column in PERMITTED_ACCEL_COLUMNS:
            continue
        # id, gap_index, created_at, warranty_token — well-known non-accel columns
        if col.column in {"id", "gap_index", "created_at", "warranty_token", "updated_at"}:
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

    run("ACCEL-01: no prohibited accelerometer columns",
        check_prohibited_accel_columns, columns)
    run("ACCEL-02: no numeric types in accel tables",
        check_accel_table_numeric_types, columns)
    run("ACCEL-03: required boolean accel columns present",
        check_permitted_accel_columns_present, columns)
    run("PII-01:   no personal-data columns",
        check_pii_columns, columns)
    run("NOJOIN-01: no cross-DB table references",
        check_cross_db_references, sql)
    run("TOKEN-01: warranty_token is BYTEA",
        check_warranty_token_type, columns)

    return result


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
    print(f"Checks: NP-FW-EMMC-002 Rev A §G.3, §G.4, §A.4")
    print()

    if args.verbose:
        print("Running checks:")

    result = run_all_checks(sql, verbose=args.verbose)

    if args.verbose:
        print()

    if result.failures:
        print(f"RESULT: FAIL — {len(result.failures)} violation(s) found\n")
        for f in result.failures:
            print(f"  [{f.check_id}] {f.description}")
            print(f"         at {f.location}")
        print()
        print("Schema freeze is BLOCKED until all violations are resolved.")
        print("Reference: NP-FW-EMMC-002 Rev A §G.5 (OI-EMMC2-07)")
        return 1

    checks_run = len(result.passed)
    print(f"RESULT: PASS — {checks_run} check(s) passed, 0 violations")
    print()
    print("SHDR fleet DB schema freeze gate is CLEARED.")
    print("OI-EMMC2-07: PASS — this result must be recorded in NP-COORD-001.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
