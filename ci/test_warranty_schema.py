#!/usr/bin/env python3
"""
Warranty registry schema gate — OI-PRIV-54 (NP-PRIV-ANALYSIS-005 F4)
Requirements: REQ-WARR-01 to REQ-WARR-04 (docs/reference/data-architecture-detail.md §5.7)

ci/test_warranty_nojoin.py checks that the warranty registry and the SHDR fleet
database do not name each other's tables. It never looked at the registry's own
columns, and the registry is the one database in the design that holds a name,
an email and a postal address. This gate checks those columns:

  CONTACT-01   support_contact_ok exists, is BOOLEAN and defaults to FALSE.   REQ-WARR-01 (a)
  CONTACT-02   no BOOLEAN column in the registry defaults to TRUE.            REQ-WARR-01 (b)
  CLAIM-01     warranty_claims.claim_type is an enumerated CHECK (... IN ...). REQ-WARR-02 (a)
  FREETEXT-01  every character column outside the notes table is enumerated
               or is one of the listed identity and purchase fields.          REQ-WARR-02 (b)
  FREETEXT-02  no column outside the notes table is named as free text
               (description, note, comment, detail, narrative, message ...).  REQ-WARR-02 (b)
  NOTES-01     the notes table holds no identity field and no warranty_token,
               and does not reference warranty_registrations directly.        REQ-WARR-02 (c)
  NOTES-02     the notes table is reached only through its own role: PUBLIC
               revoked, no other role granted on it, that role granted on
               nothing else, no GRANT ... ON ALL TABLES.                      REQ-WARR-02 (d)
  TIME-01      no TIMESTAMP, TIMESTAMPTZ, TIME or INTERVAL anywhere.          REQ-WARR-03 (a)
  TIME-02      no column named as a clock reading (*_at, timestamp, epoch).   REQ-WARR-03 (b)
  RETAIN-01    every table carries retain_until DATE.                         REQ-WARR-04
  TABLESET-01  the tables the schema declares are exactly the ones this gate
               classifies, so a new table cannot arrive unexamined.
  STRUCT-01    every column is declared inside CREATE TABLE (no ALTER TABLE
               ... ADD), so the column checks above see all of them.
  VACUITY-01   the parser sees every classified table and a warranty_token.

What it does not check: the retention periods, what an erasure request removes,
and what happens to a previous owner's registration on transfer. Those are the
principal's decision (OI-PRIV-54; docs/human-input/oi-priv-54-warranty-retention-and-erasure.md).
RETAIN-01 checks that the column exists, never what goes in it.

The column parser is ci/test_warranty_nojoin.py's, imported rather than copied:
both gates read the same file, and a third copy of the parser would be a third
place for the 2026-06-06 vacuous-pass defect to come back.

Usage:
  python3 ci/test_warranty_schema.py                 # exits 0 on PASS, 1 on FAIL
  python3 ci/test_warranty_schema.py --verbose
  python3 ci/test_warranty_schema.py --self-test     # prove every check can fail
  python3 ci/test_warranty_schema.py --schema PATH

CI-Kind: gate
CI-Self-Test: python3 ci/test_warranty_schema.py --self-test
CI-Self-Test-Reads-Tree: every mutant is a one-line edit of the PRODUCTION warranty schema, so a check proven red here is proven red against the file it guards, not against a fixture that only restates the rule
CI-Scans: column definitions, defaults, CHECK constraints and GRANT/REVOKE statements in the warranty registration schema
CI-Scan-Paths: ci/warranty/warranty_registration_schema.sql ci/test_warranty_nojoin.py
"""

from __future__ import annotations

import argparse
import re
import sys
from dataclasses import dataclass, field
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from test_warranty_nojoin import ColumnDef, parse_columns, parse_references, parse_table_names  # noqa: E402

SCHEMA_DEFAULT = Path(__file__).parent / "warranty" / "warranty_registration_schema.sql"

REGISTRATIONS = "warranty_registrations"
CLAIMS = "warranty_claims"
NOTES_TABLE = "warranty_claim_notes"
NOTES_ROLE = "warranty_claim_notes_access"

# Every table the schema may declare, so that TABLESET-01 makes a new table a
# deliberate edit here. The column rules below are generic over all tables; the
# classification only decides which one may hold free text.
KNOWN_TABLES: frozenset[str] = frozenset({REGISTRATIONS, CLAIMS, NOTES_TABLE})

# Character columns allowed to hold a value that is not drawn from a fixed set.
# Each is identity or purchase data that §A.4 of NP-FW-EMMC-002 says the
# registry collects, and none of them is a place to describe a fault or a body.
# A new entry is a minimisation decision, not a detail.
FREE_CHARACTER_COLUMNS: frozenset[tuple[str, str]] = frozenset({
    (REGISTRATIONS, "serial_number"),
    (REGISTRATIONS, "device_model"),
    (REGISTRATIONS, "registered_name"),
    (REGISTRATIONS, "registered_email"),
    (REGISTRATIONS, "registered_address"),
    (REGISTRATIONS, "purchase_retailer"),
})

CHARACTER_TYPE_RE = re.compile(
    r"^(TEXT|CITEXT|VARCHAR|CHARACTER\s+VARYING|CHARACTER|CHAR|BPCHAR|NAME|JSONB?|XML|HSTORE|TSVECTOR)\b"
    r"|\[\]",  # any array: an array of anything can carry a list of statements
    re.IGNORECASE,
)

# Names that say a column holds prose.
FREE_TEXT_NAME_RE = re.compile(
    r"descr|note|comment|detail|narrative|message|remark|free_?text|story|summary|reason|symptom|explan",
    re.IGNORECASE,
)

# Identity in the notes table, by name. The registry's own identity column names
# are included by construction (every FREE_CHARACTER_COLUMNS name), so renaming
# one there cannot open a hole here.
IDENTITY_NAME_RE = re.compile(r"name|email|address|phone|postal|serial|retailer|warranty_token", re.IGNORECASE)

TIME_TYPE_RE = re.compile(r"\b(TIMESTAMPTZ|TIMESTAMP|TIMETZ|TIME|INTERVAL)\b", re.IGNORECASE)
CLOCK_NAME_RE = re.compile(r"(_at$|^at_|timestamp|epoch|_time$|^time_|_ts$|datetime)", re.IGNORECASE)


# ---------------------------------------------------------------------------
# Small parsing helpers on top of the shared column parser
# ---------------------------------------------------------------------------

def strip_comments(sql: str) -> str:
    """The schema with every `--` comment removed (no `--` occurs inside a string literal here)."""
    return "\n".join(re.sub(r"--.*$", "", line) for line in sql.splitlines())


def code_of(col: ColumnDef) -> str:
    return re.sub(r"--.*$", "", col.raw_line)


def is_character(col: ColumnDef) -> bool:
    return bool(CHARACTER_TYPE_RE.search(col.col_type.strip().rstrip(",")))


def is_enumerated(col: ColumnDef) -> bool:
    return bool(re.search(rf"\bCHECK\s*\(\s*{re.escape(col.column)}\s+IN\s*\(", code_of(col), re.IGNORECASE))


def default_of(col: ColumnDef) -> str | None:
    m = re.search(r"\bDEFAULT\s+([^\s,]+)", code_of(col), re.IGNORECASE)
    return m.group(1).upper() if m else None


@dataclass
class Grant:
    kind: str           # GRANT or REVOKE
    privileges: str
    target: str         # lowercased object, with its class word (TABLE / SEQUENCE / ALL TABLES IN SCHEMA)
    grantees: list[str]
    line_no: int


def parse_grants(sql: str) -> list[Grant]:
    """GRANT ... ON ... TO ... and REVOKE ... ON ... FROM ..., one statement per line."""
    out: list[Grant] = []
    stmt_re = re.compile(
        r"^\s*(GRANT|REVOKE)\s+(.+?)\s+ON\s+(.+?)\s+(?:TO|FROM)\s+(.+?)\s*;?\s*$",
        re.IGNORECASE,
    )
    for line_no, line in enumerate(strip_comments(sql).splitlines(), start=1):
        m = stmt_re.match(line)
        if not m:
            continue
        target = re.sub(r"^TABLE\s+", "", m.group(3).strip(), flags=re.IGNORECASE).lower()
        grantees = [g.strip().lower() for g in re.sub(r"\bWITH\s+GRANT\s+OPTION\b", "", m.group(4), flags=re.IGNORECASE).split(",")]
        out.append(Grant(m.group(1).upper(), m.group(2).strip(), target, [g for g in grantees if g], line_no))
    return out


# ---------------------------------------------------------------------------
# Checks
# ---------------------------------------------------------------------------

@dataclass
class Failure:
    check_id: str
    description: str
    location: str


def check_contact_default(cols: list[ColumnDef]) -> list[Failure]:
    """CONTACT-01 — support_contact_ok exists, is BOOLEAN NOT NULL and defaults to FALSE."""
    found = [c for c in cols if c.table == REGISTRATIONS and c.column == "support_contact_ok"]
    if not found:
        return [Failure("CONTACT-01", "warranty_registrations.support_contact_ok is missing; the opt-in has no column to live in",
                        f"{REGISTRATIONS}.support_contact_ok")]
    c = found[0]
    failures = []
    if not c.col_type.upper().startswith("BOOL"):
        failures.append(Failure("CONTACT-01", f"support_contact_ok is '{c.col_type}', not BOOLEAN", f"{c.table}.{c.column}:{c.line_no}"))
    if default_of(c) != "FALSE":
        failures.append(Failure("CONTACT-01",
                                f"support_contact_ok defaults to {default_of(c) or 'nothing'}; contact must be opt-in (DEFAULT FALSE, "
                                "GDPR Art. 25(2), Recital 32)", f"{c.table}.{c.column}:{c.line_no}"))
    if not re.search(r"\bNOT\s+NULL\b", code_of(c), re.IGNORECASE):
        failures.append(Failure("CONTACT-01", "support_contact_ok is nullable; an unknown answer must read as no",
                                f"{c.table}.{c.column}:{c.line_no}"))
    return failures


def check_no_true_boolean_default(cols: list[ColumnDef]) -> list[Failure]:
    """CONTACT-02 — no BOOLEAN column defaults to TRUE (an opt-out default, wherever it is)."""
    return [
        Failure("CONTACT-02", f"BOOLEAN column '{c.table}.{c.column}' defaults to {default_of(c)}: an opt-out default",
                f"{c.table}.{c.column}:{c.line_no}")
        for c in cols
        if c.col_type.upper().startswith("BOOL") and default_of(c) in {"TRUE", "'T'", "'TRUE'", "'Y'", "'YES'", "'1'", "'ON'"}
    ]


def check_claim_type_enumerated(cols: list[ColumnDef]) -> list[Failure]:
    """CLAIM-01 — a claim's kind is drawn from a fixed set the database enforces."""
    found = [c for c in cols if c.table == CLAIMS and c.column == "claim_type"]
    if not found:
        return [Failure("CLAIM-01", "warranty_claims.claim_type is missing", f"{CLAIMS}.claim_type")]
    c = found[0]
    if not is_enumerated(c):
        return [Failure("CLAIM-01", "warranty_claims.claim_type has no CHECK (claim_type IN (...)); any text is accepted",
                        f"{c.table}.{c.column}:{c.line_no}")]
    return []


def check_character_columns(cols: list[ColumnDef]) -> list[Failure]:
    """FREETEXT-01 — outside the notes table, a character column is enumerated or a listed identity field."""
    failures = []
    for c in cols:
        if c.table == NOTES_TABLE or not is_character(c):
            continue
        if (c.table, c.column) in FREE_CHARACTER_COLUMNS or is_enumerated(c):
            continue
        failures.append(Failure(
            "FREETEXT-01",
            f"'{c.table}.{c.column}' ({c.col_type.rstrip(',')}) can hold any text beside the owner's identity; enumerate it "
            f"with CHECK ({c.column} IN (...)) or move it to {NOTES_TABLE}",
            f"{c.table}.{c.column}:{c.line_no}"))
    return failures


def check_free_text_names(cols: list[ColumnDef]) -> list[Failure]:
    """FREETEXT-02 — no column outside the notes table is named as prose, whatever its type."""
    return [
        Failure("FREETEXT-02", f"'{c.table}.{c.column}' is named as free text; free text lives only in {NOTES_TABLE}",
                f"{c.table}.{c.column}:{c.line_no}")
        for c in cols
        if c.table != NOTES_TABLE and FREE_TEXT_NAME_RE.search(c.column)
    ]


def check_notes_hold_no_identity(cols: list[ColumnDef], sql: str) -> list[Failure]:
    """NOTES-01 — the notes table holds no identity and no token, and reaches a registration only through a claim."""
    failures = []
    identity_names = {name for _, name in FREE_CHARACTER_COLUMNS}
    for c in cols:
        if c.table != NOTES_TABLE:
            continue
        if c.column in identity_names or IDENTITY_NAME_RE.search(c.column):
            failures.append(Failure("NOTES-01", f"the notes table carries '{c.column}', an identity field or the warranty token",
                                    f"{c.table}.{c.column}:{c.line_no}"))
    # A REFERENCES on a line inside the notes table's CREATE block that targets the registry.
    in_notes = False
    for line_no, line in enumerate(strip_comments(sql).splitlines(), start=1):
        if re.match(rf"^\s*CREATE\s+TABLE\s+(?:IF\s+NOT\s+EXISTS\s+)?{NOTES_TABLE}\s*\(", line, re.IGNORECASE):
            in_notes = True
            continue
        if in_notes and re.match(r"^\s*\);?\s*$", line):
            in_notes = False
        if in_notes and re.search(rf"\bREFERENCES\s+{REGISTRATIONS}\b", line, re.IGNORECASE):
            failures.append(Failure("NOTES-01", "the notes table references warranty_registrations directly; a note reaches a "
                                    "registration only through its claim", f"line:{line_no}"))
    return failures


def check_notes_access(sql: str) -> list[Failure]:
    """NOTES-02 — the notes table has its own role, and only that role."""
    failures = []
    grants = parse_grants(sql)
    own_sequence = f"sequence {NOTES_TABLE}_id_seq"

    if not re.search(rf"^\s*CREATE\s+ROLE\s+{NOTES_ROLE}\b", strip_comments(sql), re.IGNORECASE | re.MULTILINE):
        failures.append(Failure("NOTES-02", f"no CREATE ROLE {NOTES_ROLE}: the notes table has no role of its own", "schema"))
    if not any(g.kind == "REVOKE" and g.target == NOTES_TABLE and "public" in g.grantees for g in grants):
        failures.append(Failure("NOTES-02", f"no REVOKE ALL ON {NOTES_TABLE} FROM PUBLIC", "schema"))
    if not any(g.kind == "GRANT" and g.target == NOTES_TABLE and NOTES_ROLE in g.grantees for g in grants):
        failures.append(Failure("NOTES-02", f"{NOTES_ROLE} is granted nothing on {NOTES_TABLE}", "schema"))

    for g in grants:
        if g.kind != "GRANT":
            continue
        if g.target.startswith("all "):
            failures.append(Failure("NOTES-02", f"GRANT ... ON {g.target.upper()} reaches the notes table with every other",
                                    f"line:{g.line_no}"))
            continue
        on_notes = g.target in {NOTES_TABLE, own_sequence}
        for who in g.grantees:
            if on_notes and who != NOTES_ROLE:
                failures.append(Failure("NOTES-02", f"'{who}' is granted on {g.target}; only {NOTES_ROLE} may be",
                                        f"line:{g.line_no}"))
            if who == NOTES_ROLE and not on_notes:
                failures.append(Failure("NOTES-02", f"{NOTES_ROLE} is granted on {g.target}; the notes role reaches nothing else",
                                        f"line:{g.line_no}"))
    # Membership: GRANT <notes role> TO <someone> has no ON and so is not parsed above.
    for line_no, line in enumerate(strip_comments(sql).splitlines(), start=1):
        if re.match(rf"^\s*GRANT\s+{NOTES_ROLE}\s+TO\b", line, re.IGNORECASE):
            failures.append(Failure("NOTES-02", f"{NOTES_ROLE} is granted to another role in the schema; who holds it is an "
                                    "operational decision, not a schema default", f"line:{line_no}"))
    return failures


def check_no_time_types(sql: str) -> list[Failure]:
    """TIME-01 — no time-of-day type anywhere in the schema (CREATE, ALTER or cast)."""
    failures = []
    for line_no, line in enumerate(strip_comments(sql).splitlines(), start=1):
        for m in TIME_TYPE_RE.finditer(line):
            failures.append(Failure("TIME-01", f"'{m.group(1)}' holds a time finer than a day; use DATE", f"line:{line_no}"))
    return failures


def check_no_clock_names(cols: list[ColumnDef]) -> list[Failure]:
    """TIME-02 — no column whose name says it holds a clock reading, whatever its type."""
    return [
        Failure("TIME-02", f"'{c.table}.{c.column}' is named as a clock reading; a day is '<event>_on' or '<event>_date'",
                f"{c.table}.{c.column}:{c.line_no}")
        for c in cols
        if CLOCK_NAME_RE.search(c.column)
    ]


def check_retention_column(cols: list[ColumnDef], sql: str) -> list[Failure]:
    """RETAIN-01 — every table carries retain_until DATE. The value is not checked: it is the principal's (OI-PRIV-54)."""
    failures = []
    for table in sorted(parse_table_names(sql)):
        found = [c for c in cols if c.table == table and c.column == "retain_until"]
        if not found:
            failures.append(Failure("RETAIN-01", f"'{table}' has no retain_until column; a deletion job cannot find its expired rows",
                                    f"{table}.retain_until"))
        elif not found[0].col_type.upper().rstrip(",").startswith("DATE"):
            failures.append(Failure("RETAIN-01", f"'{table}.retain_until' is '{found[0].col_type}', not DATE",
                                    f"{table}.retain_until:{found[0].line_no}"))
    return failures


def check_table_set(sql: str) -> list[Failure]:
    """TABLESET-01 — the declared tables are exactly the classified ones."""
    declared = parse_table_names(sql)
    return (
        [Failure("TABLESET-01", f"table '{t}' is not classified in KNOWN_TABLES; decide whether it may hold free text first",
                 f"test_warranty_schema.py:KNOWN_TABLES") for t in sorted(declared - KNOWN_TABLES)]
        + [Failure("TABLESET-01", f"KNOWN_TABLES lists '{t}', which the schema no longer declares", "test_warranty_schema.py:KNOWN_TABLES")
           for t in sorted(KNOWN_TABLES - declared)]
    )


def check_columns_declared_in_create(sql: str) -> list[Failure]:
    """STRUCT-01 — no ALTER TABLE ... ADD [COLUMN]: the column parser reads CREATE TABLE only."""
    return [
        Failure("STRUCT-01", "ALTER TABLE ... ADD puts a column where the column checks cannot see it; declare it in CREATE TABLE",
                f"line:{n}")
        for n, line in enumerate(strip_comments(sql).splitlines(), start=1)
        if re.search(r"\bALTER\s+TABLE\b.*\bADD\b", line, re.IGNORECASE)
    ]


def check_not_vacuous(cols: list[ColumnDef]) -> list[Failure]:
    """VACUITY-01 — the parser sees every classified table and a warranty_token."""
    failures = []
    seen = {c.table for c in cols}
    for t in sorted(KNOWN_TABLES - seen):
        failures.append(Failure("VACUITY-01", f"parser sees no column of '{t}'; every column check passes vacuously for it", t))
    if not any(c.column == "warranty_token" for c in cols):
        failures.append(Failure("VACUITY-01", "parser sees no warranty_token column", "warranty_token"))
    return failures


# ---------------------------------------------------------------------------
# Runner
# ---------------------------------------------------------------------------

@dataclass
class CheckResult:
    passed: list[str] = field(default_factory=list)
    failures: list[Failure] = field(default_factory=list)
    scanned: dict[str, int] = field(default_factory=dict)


def checks_for(sql: str) -> list[tuple[str, list[Failure]]]:
    cols = parse_columns(sql)
    return [
        ("CONTACT-01:   support_contact_ok defaults to FALSE", check_contact_default(cols)),
        ("CONTACT-02:   no BOOLEAN defaults to TRUE", check_no_true_boolean_default(cols)),
        ("CLAIM-01:     claim_type is enumerated", check_claim_type_enumerated(cols)),
        ("FREETEXT-01:  character columns are enumerated or listed identity", check_character_columns(cols)),
        ("FREETEXT-02:  no free-text column name outside the notes table", check_free_text_names(cols)),
        ("NOTES-01:     notes hold no identity and no token", check_notes_hold_no_identity(cols, sql)),
        ("NOTES-02:     notes reached only through their own role", check_notes_access(sql)),
        ("TIME-01:      no time-of-day type", check_no_time_types(sql)),
        ("TIME-02:      no clock-reading column name", check_no_clock_names(cols)),
        ("RETAIN-01:    every table carries retain_until DATE", check_retention_column(cols, sql)),
        ("TABLESET-01:  declared tables are the classified ones", check_table_set(sql)),
        ("STRUCT-01:    every column declared in CREATE TABLE", check_columns_declared_in_create(sql)),
        ("VACUITY-01:   parser sees every classified table", check_not_vacuous(cols)),
    ]


def run_all_checks(sql: str, verbose: bool = False) -> CheckResult:
    result = CheckResult()
    for name, found in checks_for(sql):
        if found:
            result.failures.extend(found)
        else:
            result.passed.append(name)
        if verbose:
            print(f"  {'FAIL' if found else 'PASS'}  {name}" + (f": {len(found)} violation(s)" if found else ""))
    cols = parse_columns(sql)
    result.scanned = {
        "columns": len(cols),
        "tables": len({c.table for c in cols}),
        "character": sum(1 for c in cols if is_character(c)),
        "grants": len(parse_grants(sql)),
    }
    return result


def failures_of(check_id: str, sql: str) -> list[Failure]:
    return [f for _, found in checks_for(sql) for f in found if f.check_id == check_id]


# ---------------------------------------------------------------------------
# Self-test — prove each check can fail, on a one-line edit of the real schema
# ---------------------------------------------------------------------------

def _production() -> str:
    return SCHEMA_DEFAULT.read_text(encoding="utf-8")


def _mutate(old: str, new: str) -> str:
    sql = _production()
    assert old in sql, f"self-test fixture is stale: {old!r} is not in the schema"
    return sql.replace(old, new, 1)


def _fires(check_id: str, sql: str) -> None:
    assert failures_of(check_id, sql), f"{check_id} accepted a schema it exists to reject"


# @verifies REQ-WARR-01#a
def test_contact_default_true_fails():
    _fires("CONTACT-01", _mutate("support_contact_ok   BOOLEAN      NOT NULL DEFAULT FALSE",
                                 "support_contact_ok   BOOLEAN      NOT NULL DEFAULT TRUE"))
    _fires("CONTACT-01", _mutate("support_contact_ok   BOOLEAN      NOT NULL DEFAULT FALSE,",
                                 "support_contact_ok   BOOLEAN,"))


# @verifies REQ-WARR-01#a
def test_contact_column_removed_fails():
    _fires("CONTACT-01", _mutate("    support_contact_ok   BOOLEAN      NOT NULL DEFAULT FALSE,\n", ""))


# @verifies REQ-WARR-01#b
def test_any_true_boolean_default_fails():
    _fires("CONTACT-02", _mutate("    retain_until         DATE\n);\n\nCREATE TABLE warranty_claims",
                                 "    marketing_ok         BOOLEAN      NOT NULL DEFAULT TRUE,\n    retain_until         DATE\n);\n\n"
                                 "CREATE TABLE warranty_claims"))


# @verifies REQ-WARR-02#a
def test_claim_type_unenumerated_fails():
    sql = _production()
    line = next(l for l in sql.splitlines() if l.strip().startswith("claim_type"))
    _fires("CLAIM-01", sql.replace(line, "    claim_type           VARCHAR(32)  NOT NULL,", 1))


# @verifies REQ-WARR-02#b
def test_free_text_beside_identity_fails():
    # The column F4 found, put back.
    restored = _mutate("    claim_status         VARCHAR(16)",
                       "    claim_description    TEXT,\n    claim_status         VARCHAR(16)")
    _fires("FREETEXT-01", restored)
    _fires("FREETEXT-02", restored)
    # A neutrally named, unbounded column is still caught by type.
    _fires("FREETEXT-01", _mutate("    claim_status         VARCHAR(16)",
                                  "    extra                JSONB,\n    claim_status         VARCHAR(16)"))
    # A prose name is caught whatever the type.
    _fires("FREETEXT-02", _mutate("    claim_status         VARCHAR(16)",
                                  "    owner_remarks        INTEGER,\n    claim_status         VARCHAR(16)"))


# @verifies REQ-WARR-02#c
def test_identity_in_notes_fails():
    _fires("NOTES-01", _mutate("    note                 TEXT         NOT NULL,",
                               "    note                 TEXT         NOT NULL,\n    registered_email     VARCHAR(254),"))
    _fires("NOTES-01", _mutate("    note                 TEXT         NOT NULL,",
                               "    note                 TEXT         NOT NULL,\n    warranty_token       BYTEA,"))
    _fires("NOTES-01", _mutate("    claim_id             BIGINT       NOT NULL REFERENCES warranty_claims(id),",
                               "    claim_id             BIGINT       NOT NULL REFERENCES warranty_claims(id),\n"
                               "    registration_id      BIGINT       REFERENCES warranty_registrations(id),"))


# @verifies REQ-WARR-02#d
def test_notes_access_widened_fails():
    _fires("NOTES-02", _mutate("REVOKE ALL ON warranty_claim_notes FROM PUBLIC;\n", ""))
    _fires("NOTES-02", _mutate("GRANT SELECT, INSERT ON warranty_claim_notes TO warranty_claim_notes_access;",
                               "GRANT SELECT, INSERT ON warranty_claim_notes TO warranty_claim_notes_access, warranty_support;"))
    _fires("NOTES-02", _production() + "\nGRANT SELECT ON warranty_registrations TO warranty_claim_notes_access;\n")
    _fires("NOTES-02", _production() + "\nGRANT SELECT ON ALL TABLES IN SCHEMA public TO warranty_support;\n")
    _fires("NOTES-02", _production() + "\nGRANT warranty_claim_notes_access TO warranty_support;\n")
    _fires("NOTES-02", _mutate("CREATE ROLE warranty_claim_notes_access NOLOGIN;\n", ""))


# @verifies REQ-WARR-03#a
def test_time_of_day_type_fails():
    _fires("TIME-01", _mutate("    opened_on            DATE         NOT NULL DEFAULT CURRENT_DATE,",
                              "    opened_on            TIMESTAMPTZ  NOT NULL DEFAULT NOW(),"))
    _fires("TIME-01", _production() + "\nALTER TABLE warranty_claims ALTER COLUMN opened_on TYPE TIMESTAMP;\n")


# @verifies REQ-WARR-03#b
def test_clock_reading_name_fails():
    _fires("TIME-02", _mutate("    opened_on            DATE", "    created_at           DATE"))
    _fires("TIME-02", _mutate("    opened_on            DATE", "    opened_epoch         BIGINT"))


# @verifies REQ-WARR-04
def test_retention_column_missing_fails():
    sql = _production()
    i = sql.index("CREATE TABLE warranty_claim_notes")
    tail = sql[i:].replace("    written_on           DATE         NOT NULL DEFAULT CURRENT_DATE,\n    retain_until         DATE\n",
                           "    written_on           DATE         NOT NULL DEFAULT CURRENT_DATE\n", 1)
    assert tail != sql[i:], "self-test fixture is stale: the notes table's retain_until line moved"
    _fires("RETAIN-01", sql[:i] + tail)
    _fires("RETAIN-01", _mutate("    retain_until         DATE\n);\n\nCREATE TABLE warranty_claims",
                                "    retain_until         VARCHAR(16)\n);\n\nCREATE TABLE warranty_claims"))


# @verifies-none harness self-check: proves the table classification cannot drift silently
def test_unclassified_table_fails():
    _fires("TABLESET-01", _production() + "\nCREATE TABLE warranty_contacts (\n    id  BIGINT,\n    retain_until  DATE\n);\n")


# @verifies-none harness self-check: proves a column added outside CREATE TABLE is refused, so the column checks see every column
def test_alter_add_column_fails():
    _fires("STRUCT-01", _production() + "\nALTER TABLE warranty_claims ADD COLUMN detail TEXT;\n")


# @verifies-none harness self-check: proves an empty or truncated parse is refused rather than passing vacuously
def test_vacuous_parse_fails():
    assert check_not_vacuous([]), "VACUITY-01 accepted an empty parse"
    cols = [c for c in parse_columns(_production()) if c.table != NOTES_TABLE]
    assert check_not_vacuous(cols), "VACUITY-01 accepted a parse that lost a whole table"


# @verifies-none harness self-check: the production schema passes every check, so the mutants above are red for their edit alone
def test_production_schema_passes():
    failures = [f for _, found in checks_for(_production()) for f in found]
    assert not failures, "production schema rejected: " + "; ".join(f"[{f.check_id}] {f.description}" for f in failures[:3])


def _self_test() -> int:
    tests = [(n, f) for n, f in sorted(globals().items()) if n.startswith("test_") and callable(f)]
    print("Warranty registry schema gate self-test (OI-PRIV-54)")
    print(f"Schema: {SCHEMA_DEFAULT}\n")
    failed = 0
    for name, fn in tests:
        try:
            fn()
            print(f"  PASS  {name}")
        except AssertionError as e:
            print(f"  FAIL  {name}\n          <<< {e}")
            failed += 1
    print()
    if failed:
        print(f"SELF-TEST FAIL — {failed} of {len(tests)} falsification(s) did not hold")
        return 1
    print(f"SELF-TEST PASS — {len(tests)} falsifications held; the gate has teeth.")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description="Warranty registry schema gate (OI-PRIV-54)")
    parser.add_argument("--self-test", action="store_true", help="Prove every check fails on a mutated schema, then exit")
    parser.add_argument("--schema", type=Path, default=SCHEMA_DEFAULT, help="Path to the warranty registration schema")
    parser.add_argument("--verbose", "-v", action="store_true", help="Show every check, PASS included")
    args = parser.parse_args()

    if args.self_test:
        return _self_test()
    if not args.schema.exists():
        print(f"ERROR: schema not found: {args.schema}")
        return 1

    sql = args.schema.read_text(encoding="utf-8")
    print("NeurOne Warranty Registry Schema Gate — OI-PRIV-54")
    print(f"Schema: {args.schema}")
    print("Requirements: REQ-WARR-01 to REQ-WARR-04 (docs/reference/data-architecture-detail.md §5.7)\n")
    result = run_all_checks(sql, verbose=args.verbose)
    s = result.scanned
    scanned = (f"scanned: {s['columns']} column(s) across {s['tables']} table(s), "
               f"{s['character']} character column(s), {s['grants']} GRANT/REVOKE statement(s)")
    if args.verbose:
        print()
    if result.failures:
        print(f"RESULT: FAIL — {len(result.failures)} violation(s)\n{scanned}\n")
        for f in result.failures:
            print(f"  [{f.check_id}] {f.description}\n         at {f.location}")
        return 1
    print(f"RESULT: PASS — {len(result.passed)} check(s) passed, 0 violations\n{scanned}")
    print("Retention periods, erasure and transfer are NOT checked: pending the principal (OI-PRIV-54).")
    return 0


if __name__ == "__main__":
    sys.exit(main())
