#!/usr/bin/env python3
"""
Warranty Token No-Join CI Gate — OI-EMMC2-06
Document: NP-FW-EMMC-002 Rev 1 §A.5
Revision: A — 2026-06-06
BLOCKING: must PASS before warranty registration system build and SHDR fleet
DB schema freeze.

Validates that the warranty registration DB (ci/warranty/warranty_registration_schema.sql)
and the SHDR fleet DB (ci/shdr/shdr_fleet_schema.sql) are kept completely
separate, per NP-FW-EMMC-002 §A.4 and §A.5:

  1. NOJOIN-SHDR-01     — SHDR schema does not reference warranty registration tables.
  2. NOJOIN-SHDR-02     — SHDR schema has no PII columns that would enable implicit joins.
  3. NOJOIN-WARRANTY-01 — Warranty schema does not reference SHDR fleet tables.
  4. TOKEN-SHDR-01      — warranty_token is BYTEA in the SHDR schema.
  5. TOKEN-WARRANTY-01  — warranty_token is BYTEA in the warranty schema.
  6. SCHEMA-ISOLATION-01 — Neither schema's REFERENCES clauses point at a table
                            that lives in the other schema (both load independently).

The warranty_token is the sole permitted linkage between the two databases
(NP-FW-EMMC-002 §A.3). It is opaque (32-byte TRNG BYTEA) and carries no
re-identifying content, so its presence in both schemas is required, not a
violation. The no-join rule forbids every OTHER shared identifier or cross-DB
foreign key.

Usage:
  python3 ci/test_warranty_nojoin.py                  # exits 0 on PASS, 1 on FAIL
  python3 ci/test_warranty_nojoin.py --verbose        # show all checks
  python3 ci/test_warranty_nojoin.py --shdr-schema PATH --warranty-schema PATH
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

SHDR_SCHEMA_DEFAULT = Path(__file__).parent / "shdr" / "shdr_fleet_schema.sql"
WARRANTY_SCHEMA_DEFAULT = Path(__file__).parent / "warranty" / "warranty_registration_schema.sql"

# Warranty registration table-name patterns that must NEVER appear in the SHDR
# schema (a reference to any of these would enable a cross-DB join).
PROHIBITED_WARRANTY_TABLE_PATTERNS: list[tuple[str, str]] = [
    (r"\bwarranty_registrations?\b",  "warranty_registration table (cross-DB join risk)"),
    (r"\bregistrants?\b",             "registrant table (cross-DB join risk)"),
    (r"\bwarranty_owners?\b",         "warranty_owner table (cross-DB join risk)"),
]

# Personal-data column name patterns prohibited in any SHDR table. These would
# let SHDR rows be joined to warranty rows by matching PII content (§A.4).
PROHIBITED_PII_PATTERNS: list[tuple[str, str]] = [
    (r"name",     "name (PII — implicit join key)"),
    (r"email",    "email address (PII — implicit join key)"),
    (r"address",  "postal address (PII — implicit join key)"),
    (r"phone",    "phone number (PII — implicit join key)"),
    (r"postal",   "postal code (PII — implicit join key)"),
]

# SHDR fleet table names. A reference to any of these from the warranty schema
# would enable a cross-DB join in the other direction.
SHDR_FLEET_TABLE_NAMES: frozenset[str] = frozenset({
    "devices",
    "firmware_history",
    "ota_events",
    # Rev D: the fixed five-zone tables were replaced by socket+module-keyed
    # ones. pbm_zone_telemetry → pbm_module_telemetry; thermal_profiles →
    # module_thermal_telemetry + hub_thermal_telemetry. The retired names are
    # deliberately absent: a stale entry here would silently stop guarding a
    # table that still exists. TABLESET-01 (below) pins this set to the schema.
    "module_inventory",
    "module_placement_events",
    "module_life",
    "socket_life",
    "socket_part_type_wear",
    "pbm_module_telemetry",
    "emf_shielding_telemetry",
    "module_thermal_telemetry",
    "hub_thermal_telemetry",
    "module_rotation_advice",
    "power_telemetry",
    "consumable_counts",
    "accessory_auth_log",
    "calibration_history",
    "eeg_impedance_trend",
    "shdr_accel_records",
    "mode_f_telemetry",
    "fault_log",
    "storage_health",
})


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
    # Lines to skip: table-level constraints, index directives, closing parens,
    # comments. Keywords are anchored to the start of the stripped line so that
    # an inline column modifier (e.g. "... NOT NULL UNIQUE" or
    # "... REFERENCES devices(...)") does NOT cause the whole column line to be
    # dropped — only a line that BEGINS with a constraint/index keyword is a
    # table-level clause rather than a column definition.
    skip_re = re.compile(
        r"^\s*(--|CONSTRAINT\b|PRIMARY\s+KEY\b|FOREIGN\s+KEY\b|UNIQUE\s*\(|CHECK\s*\(|ALTER\b|CREATE\s+INDEX\b|\);?\s*$)",
        re.IGNORECASE,
    )
    # Closing paren (with optional trailing semicolon) ends a CREATE TABLE block.
    close_re = re.compile(r"^\s*\);?\s*$")

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
        # Closing paren resets table context regardless of any trailing tokens.
        if close_re.match(line):
            current_table = None
            continue
        if skip_re.match(line):
            continue

        # Strip any trailing inline comment before matching — a nullable column
        # carrying an explanatory "-- ..." comment and no other modifier matched
        # nothing, making it invisible to NOJOIN-SHDR-02 (the PII check) and
        # TOKEN-*-01.  Same defect and same fix as ci/test_shdr_schema.py; both
        # gates carry a copy of this parser, so both had it.
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


def parse_table_names(sql: str) -> set[str]:
    """Return the lowercase names of every table defined in the schema."""
    create_re = re.compile(
        r"^\s*CREATE\s+TABLE\s+(?:IF\s+NOT\s+EXISTS\s+)?(\w+)\s*\(",
        re.IGNORECASE,
    )
    names: set[str] = set()
    for line in sql.splitlines():
        m = create_re.match(line)
        if m:
            names.add(m.group(1).lower())
    return names


@dataclass
class Reference:
    referenced_table: str
    line_no: int
    raw_line: str


def parse_references(sql: str) -> list[Reference]:
    """
    Extract every REFERENCES target table from the schema. Comment lines are
    ignored. Matches both inline column REFERENCES and FOREIGN KEY ... REFERENCES.
    """
    ref_re = re.compile(r"\bREFERENCES\s+(\w+)\s*\(", re.IGNORECASE)
    refs: list[Reference] = []
    for line_no, line in enumerate(sql.splitlines(), start=1):
        if line.strip().startswith("--"):
            continue
        for m in ref_re.finditer(line):
            refs.append(Reference(
                referenced_table=m.group(1).lower(),
                line_no=line_no,
                raw_line=line.rstrip(),
            ))
    return refs


# ---------------------------------------------------------------------------
# Check functions
# ---------------------------------------------------------------------------

@dataclass
class Failure:
    check_id: str
    description: str
    location: str  # "table.column:line_no" or "line:N"


def check_shdr_no_warranty_tables(shdr_sql: str) -> list[Failure]:
    """NOJOIN-SHDR-01 — SHDR schema must not name a warranty registration table."""
    failures: list[Failure] = []
    for line_no, line in enumerate(shdr_sql.splitlines(), start=1):
        if line.strip().startswith("--"):
            continue
        for pattern, reason in PROHIBITED_WARRANTY_TABLE_PATTERNS:
            if re.search(pattern, line, re.IGNORECASE):
                failures.append(Failure(
                    check_id="NOJOIN-SHDR-01",
                    description=f"SHDR schema references {reason}",
                    location=f"line:{line_no}",
                ))
    return failures


def check_shdr_no_pii_columns(columns: list[ColumnDef]) -> list[Failure]:
    """NOJOIN-SHDR-02 — SHDR columns must not contain PII substrings."""
    failures: list[Failure] = []
    for col in columns:
        # warranty_token is the sole permitted linkage and is opaque binary; it
        # is not PII and never matches any pattern below, so it is correctly
        # left unflagged without a special case.
        for pattern, reason in PROHIBITED_PII_PATTERNS:
            if re.search(pattern, col.column, re.IGNORECASE):
                failures.append(Failure(
                    check_id="NOJOIN-SHDR-02",
                    description=f"SHDR column '{col.column}' contains {reason}",
                    location=f"{col.table}.{col.column}:{col.line_no}",
                ))
    return failures


def check_warranty_no_shdr_tables(warranty_sql: str) -> list[Failure]:
    """NOJOIN-WARRANTY-01 — Warranty schema must not name an SHDR fleet table."""
    failures: list[Failure] = []
    for line_no, line in enumerate(warranty_sql.splitlines(), start=1):
        if line.strip().startswith("--"):
            continue
        for table_name in SHDR_FLEET_TABLE_NAMES:
            if re.search(rf"\b{re.escape(table_name)}\b", line, re.IGNORECASE):
                failures.append(Failure(
                    check_id="NOJOIN-WARRANTY-01",
                    description=(
                        f"Warranty schema references SHDR fleet table "
                        f"'{table_name}' (cross-DB join risk)"
                    ),
                    location=f"line:{line_no}",
                ))
    return failures


def check_warranty_token_type(columns: list[ColumnDef], check_id: str) -> list[Failure]:
    """warranty_token must be BYTEA (opaque binary) in every table that has it."""
    failures: list[Failure] = []
    for col in columns:
        if col.column == "warranty_token":
            if "BYTEA" not in col.col_type.upper():
                failures.append(Failure(
                    check_id=check_id,
                    description=(
                        f"warranty_token in '{col.table}' is '{col.col_type}' — "
                        f"must be BYTEA (opaque 32-byte binary per NP-FW-EMMC-002 §A.2)"
                    ),
                    location=f"{col.table}.{col.column}:{col.line_no}",
                ))
    return failures


def check_schema_isolation(shdr_sql: str, warranty_sql: str) -> list[Failure]:
    """
    SCHEMA-ISOLATION-01 — Neither schema may have a REFERENCES clause whose
    target table is defined in the other schema. Each schema must load on its
    own with no cross-schema foreign keys.
    """
    failures: list[Failure] = []

    shdr_tables = parse_table_names(shdr_sql)
    warranty_tables = parse_table_names(warranty_sql)

    # SHDR references must not target a warranty-schema table.
    for ref in parse_references(shdr_sql):
        if ref.referenced_table in warranty_tables:
            failures.append(Failure(
                check_id="SCHEMA-ISOLATION-01",
                description=(
                    f"SHDR schema REFERENCES warranty-schema table "
                    f"'{ref.referenced_table}' — cross-schema foreign key"
                ),
                location=f"shdr:line:{ref.line_no}",
            ))

    # Warranty references must not target an SHDR-schema table.
    for ref in parse_references(warranty_sql):
        if ref.referenced_table in shdr_tables:
            failures.append(Failure(
                check_id="SCHEMA-ISOLATION-01",
                description=(
                    f"Warranty schema REFERENCES SHDR-schema table "
                    f"'{ref.referenced_table}' — cross-schema foreign key"
                ),
                location=f"warranty:line:{ref.line_no}",
            ))

    return failures


# ---------------------------------------------------------------------------
# Runner
# ---------------------------------------------------------------------------

@dataclass
class CheckResult:
    passed: list[str] = field(default_factory=list)
    failures: list[Failure] = field(default_factory=list)


def check_shdr_table_set_current(shdr_sql: str) -> list[Failure]:
    """
    TABLESET-01 — SHDR_FLEET_TABLE_NAMES must equal the set of tables the SHDR
    schema actually declares.

    That frozenset is what NOJOIN-WARRANTY-01 scans the warranty schema for, so
    it fails silently in BOTH directions if it drifts:
      * a table added to the schema but not to the set is never guarded — the
        warranty schema could reference it and the gate would still report PASS;
      * a name left in the set after the table was renamed guards nothing, while
        making the set look complete.

    Rev D renamed two tables and added five, which is exactly the drift this
    catches. The check parses CREATE TABLE out of the DDL rather than grepping
    for the names — a grep would be satisfied by the very comment above the
    frozenset that lists the retired names.
    """
    declared = {
        m.group(1).lower()
        for m in re.finditer(
            r"^\s*CREATE\s+TABLE\s+(?:IF\s+NOT\s+EXISTS\s+)?(\w+)\s*\(",
            shdr_sql,
            re.IGNORECASE | re.MULTILINE,
        )
    }
    failures: list[Failure] = []
    for missing in sorted(declared - SHDR_FLEET_TABLE_NAMES):
        failures.append(Failure(
            check_id="TABLESET-01",
            description=(
                f"SHDR table '{missing}' is declared in the schema but absent from "
                f"SHDR_FLEET_TABLE_NAMES — NOJOIN-WARRANTY-01 is not guarding it"
            ),
            location=f"shdr_fleet_schema.sql:{missing}",
        ))
    for stale in sorted(SHDR_FLEET_TABLE_NAMES - declared):
        failures.append(Failure(
            check_id="TABLESET-01",
            description=(
                f"SHDR_FLEET_TABLE_NAMES lists '{stale}', which no longer exists in "
                f"the schema — stale entry guards nothing"
            ),
            location=f"test_warranty_nojoin.py:SHDR_FLEET_TABLE_NAMES",
        ))
    return failures


def run_all_checks(shdr_sql: str, warranty_sql: str, verbose: bool = False) -> CheckResult:
    result = CheckResult()
    shdr_columns = parse_columns(shdr_sql)
    warranty_columns = parse_columns(warranty_sql)

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

    run("NOJOIN-SHDR-01:     SHDR has no warranty table references",
        check_shdr_no_warranty_tables, shdr_sql)
    run("NOJOIN-SHDR-02:     SHDR has no PII columns",
        check_shdr_no_pii_columns, shdr_columns)
    run("NOJOIN-WARRANTY-01: warranty has no SHDR table references",
        check_warranty_no_shdr_tables, warranty_sql)
    run("TOKEN-SHDR-01:      SHDR warranty_token is BYTEA",
        check_warranty_token_type, shdr_columns, "TOKEN-SHDR-01")
    run("TOKEN-WARRANTY-01:  warranty warranty_token is BYTEA",
        check_warranty_token_type, warranty_columns, "TOKEN-WARRANTY-01")
    run("SCHEMA-ISOLATION-01: schemas load independently",
        check_schema_isolation, shdr_sql, warranty_sql)
    run("TABLESET-01:        SHDR table list matches the schema",
        check_shdr_table_set_current, shdr_sql)

    return result


def main() -> int:
    parser = argparse.ArgumentParser(description="Warranty token no-join CI gate (OI-EMMC2-06)")
    parser.add_argument("--shdr-schema", type=Path, default=SHDR_SCHEMA_DEFAULT,
                        help="Path to SHDR fleet DB schema SQL file")
    parser.add_argument("--warranty-schema", type=Path, default=WARRANTY_SCHEMA_DEFAULT,
                        help="Path to warranty registration DB schema SQL file")
    parser.add_argument("--verbose", "-v", action="store_true",
                        help="Show all check results including PASS")
    args = parser.parse_args()

    if not args.shdr_schema.exists():
        print(f"ERROR: SHDR schema file not found: {args.shdr_schema}")
        return 1
    if not args.warranty_schema.exists():
        print(f"ERROR: Warranty schema file not found: {args.warranty_schema}")
        return 1

    shdr_sql = args.shdr_schema.read_text(encoding="utf-8")
    warranty_sql = args.warranty_schema.read_text(encoding="utf-8")

    print(f"NeurOne Warranty No-Join CI Gate — OI-EMMC2-06")
    print(f"SHDR schema:    {args.shdr_schema}")
    print(f"Warranty schema: {args.warranty_schema}")
    print(f"Checks: NP-FW-EMMC-002 Rev 1 §A.4, §A.5")
    print()

    if args.verbose:
        print("Running checks:")

    result = run_all_checks(shdr_sql, warranty_sql, verbose=args.verbose)

    if args.verbose:
        print()

    if result.failures:
        print(f"RESULT: FAIL — {len(result.failures)} violation(s) found\n")
        for f in result.failures:
            print(f"  [{f.check_id}] {f.description}")
            print(f"         at {f.location}")
        print()
        print("Warranty token no-join gate is BLOCKED until all violations are resolved.")
        print("Reference: NP-FW-EMMC-002 Rev 1 §A.5 (OI-EMMC2-06)")
        return 1

    checks_run = len(result.passed)
    print(f"RESULT: PASS — {checks_run} check(s) passed, 0 violations")
    print()
    print("Warranty token no-join gate is CLEARED.")
    print("OI-EMMC2-06: PASS — this result must be recorded in NP-COORD-001.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
