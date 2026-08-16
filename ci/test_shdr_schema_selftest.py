#!/usr/bin/env python3
"""
Self-test for the SHDR schema CI gate — OI-EMMC2-07
Document: NP-FW-EMMC-002 Rev 1 §G.5

The gate in ci/test_shdr_schema.py asserts that the SHDR fleet DB schema
contains NO raw accelerometer columns (only the two permitted booleans
drop_detected / maintenance_alert). That gate runs against the production
schema and is expected to PASS.

But a PASS from a negative check is only trustworthy if the check actually
has teeth. Two ways it could pass while being broken:

  1. The prohibited-column matcher silently matches nothing (bad regex,
     wrong field, etc.) — every future schema would pass regardless.
  2. The parser stops seeing the accelerometer table (table renamed, DDL
     shape changed, comment-stripping bug) — the check scans zero accel
     columns and passes VACUOUSLY.

This file is the test-of-the-test: it drives the gate's accelerometer
assertion with synthetic BAD schemas and confirms each prohibited raw
column is caught, then confirms the assertion is not vacuous on the real
schema. Pure test code — it imports and exercises the gate's own check
functions; it does not touch the schema.

Runnable two ways:
  python3 ci/test_shdr_schema_selftest.py     # plain runner, exits 0/1
  pytest ci/test_shdr_schema_selftest.py      # pytest discovery (test_*)
"""

from __future__ import annotations

import sys
from pathlib import Path

# Import the gate module regardless of the current working directory.
sys.path.insert(0, str(Path(__file__).resolve().parent))

from test_shdr_schema import (  # noqa: E402
    SCHEMA_DEFAULT,
    parse_columns,
    characterisation_exempt_columns,
    check_prohibited_accel_columns,
    check_accel_table_numeric_types,
    check_permitted_accel_columns_present,
)

# ---------------------------------------------------------------------------
# Fixtures
# ---------------------------------------------------------------------------

# Every one of these raw-accelerometer column definitions MUST be rejected by
# check_prohibited_accel_columns (ACCEL-01). This is the exact class of column
# NP-FW-EMMC-002 §G.3 forbids from ever reaching the fleet DB — each reveals a
# motor-control / impact profile that is health-inferrable.
PROHIBITED_ACCEL_COLUMN_DDL: list[str] = [
    "accel_x            REAL         NOT NULL",
    "accel_y            REAL         NOT NULL",
    "accel_z            REAL         NOT NULL",
    "g_force            REAL         NOT NULL",
    "peak_g             REAL         NOT NULL",
    "rms_g              REAL         NOT NULL",
    "average_g          REAL         NOT NULL",
    "raw_accel          BYTEA        NOT NULL",
    "orientation_x      REAL         NOT NULL",
    "drop_count         INTEGER      NOT NULL DEFAULT 0",
    "drop_timestamp     DATE         NOT NULL",
    "impact_severity    REAL         NOT NULL",
    "fall_detected      BOOLEAN      NOT NULL DEFAULT FALSE",
]


def _schema_with_accel_column(bad_col_ddl: str) -> str:
    """A minimal, otherwise-valid accel table carrying one prohibited column."""
    return (
        "CREATE TABLE shdr_accel_records (\n"
        "    id                  BIGSERIAL    PRIMARY KEY,\n"
        "    warranty_token      BYTEA        NOT NULL,\n"
        "    drop_detected       BOOLEAN      NOT NULL DEFAULT FALSE,\n"
        "    maintenance_alert   BOOLEAN      NOT NULL DEFAULT FALSE,\n"
        f"    {bad_col_ddl},\n"
        "    gap_index           INTEGER      NOT NULL CHECK (gap_index >= 0)\n"
        ");\n"
    )


# ---------------------------------------------------------------------------
# Tests — the prohibited-column assertion actually catches violations
# ---------------------------------------------------------------------------

def test_each_prohibited_raw_accel_column_is_rejected() -> None:
    """ACCEL-01 must flag every raw-accelerometer column, one at a time."""
    for bad_col_ddl in PROHIBITED_ACCEL_COLUMN_DDL:
        col_name = bad_col_ddl.split()[0]
        sql = _schema_with_accel_column(bad_col_ddl)
        columns = parse_columns(sql)

        # Sanity: the parser must have actually seen the injected column,
        # otherwise the check below would pass for the wrong reason.
        assert any(c.column == col_name for c in columns), (
            f"parser did not extract injected column '{col_name}' — "
            f"fixture or parser is broken, test would be vacuous"
        )

        failures = check_prohibited_accel_columns(columns)
        flagged = {f.location.split(".")[1].split(":")[0] for f in failures}
        assert col_name in flagged, (
            f"raw accelerometer column '{col_name}' was NOT rejected by "
            f"ACCEL-01 — the gate would admit health-inferrable motion data"
        )
        assert all(f.check_id == "ACCEL-01" for f in failures)


def test_numeric_column_in_accel_table_is_rejected() -> None:
    """ACCEL-02 must flag a numeric column in an accel-named table even if
    its name is innocuous (would allow encoding raw values under a rename)."""
    sql = _schema_with_accel_column("motion_magnitude   REAL         NOT NULL")
    columns = parse_columns(sql)
    failures = check_accel_table_numeric_types(columns)
    flagged = {f.location.split(".")[1].split(":")[0] for f in failures}
    assert "motion_magnitude" in flagged, (
        "numeric column in accel table was not rejected by ACCEL-02"
    )


def test_missing_required_boolean_is_rejected() -> None:
    """ACCEL-03 must flag an accel table missing a required boolean field."""
    sql = (
        "CREATE TABLE shdr_accel_records (\n"
        "    id                  BIGSERIAL    PRIMARY KEY,\n"
        "    warranty_token      BYTEA        NOT NULL,\n"
        "    drop_detected       BOOLEAN      NOT NULL DEFAULT FALSE,\n"  # maintenance_alert absent
        "    gap_index           INTEGER      NOT NULL CHECK (gap_index >= 0)\n"
        ");\n"
    )
    failures = check_permitted_accel_columns_present(parse_columns(sql))
    assert any(f.check_id == "ACCEL-03" for f in failures), (
        "missing required boolean 'maintenance_alert' was not caught by ACCEL-03"
    )


# ---------------------------------------------------------------------------
# Tests — the assertion is not vacuous on the real production schema
# ---------------------------------------------------------------------------

def test_production_schema_passes_accel_checks() -> None:
    """
    The real schema must PASS all three accelerometer assertions.

    The exemption argument is what run_all_checks passes, and it must be passed
    HERE and nowhere else in this file. Every other case above drives the checks
    with NO exemption, which is the correct way to test them: a synthetic bad
    schema must be rejected on its own terms, not by whatever CHAR-01 happened
    to grant. Only the production schema legitimately carries the §H
    characterisation columns, and only because CHAR-01 currently permits them —
    if it stops permitting them, characterisation_exempt_columns() returns the
    empty set and this assertion fails, which is the intended coupling.
    """
    sql = SCHEMA_DEFAULT.read_text(encoding="utf-8")
    columns = parse_columns(sql)
    exempt = characterisation_exempt_columns(sql, columns)
    assert not check_prohibited_accel_columns(columns, exempt)
    assert not check_accel_table_numeric_types(columns, exempt)
    assert not check_permitted_accel_columns_present(columns)


def test_production_accel_assertion_is_not_vacuous() -> None:
    """Guard against a vacuous PASS: the parser must actually be seeing the
    accelerometer table and its two permitted boolean columns. If it isn't,
    'no raw accel columns' is true only because nothing was scanned."""
    columns = parse_columns(SCHEMA_DEFAULT.read_text(encoding="utf-8"))
    accel_cols = {c.column for c in columns if c.table == "shdr_accel_records"}
    assert accel_cols, (
        "parser found NO columns in shdr_accel_records — the ACCEL-01 PASS on "
        "the production schema would be vacuous (nothing scanned)"
    )
    assert {"drop_detected", "maintenance_alert"} <= accel_cols, (
        "expected the two permitted accelerometer booleans to be present and "
        "parsed; the accelerometer assertion is not exercising real columns"
    )


# ---------------------------------------------------------------------------
# Plain runner (no pytest dependency required in CI)
# ---------------------------------------------------------------------------

def _main() -> int:
    tests = [
        test_each_prohibited_raw_accel_column_is_rejected,
        test_numeric_column_in_accel_table_is_rejected,
        test_missing_required_boolean_is_rejected,
        test_production_schema_passes_accel_checks,
        test_production_accel_assertion_is_not_vacuous,
    ]
    print("SHDR schema gate self-test — OI-EMMC2-07 (no raw accelerometer columns)")
    print()
    failed = 0
    for t in tests:
        try:
            t()
        except AssertionError as e:
            failed += 1
            print(f"  FAIL  {t.__name__}")
            print(f"        {e}")
        else:
            print(f"  PASS  {t.__name__}")
    print()
    if failed:
        print(f"RESULT: FAIL — {failed}/{len(tests)} self-test(s) failed")
        print("The accelerometer assertion in ci/test_shdr_schema.py is not sound.")
        return 1
    print(f"RESULT: PASS — {len(tests)}/{len(tests)} self-tests passed")
    print("The 'no raw accelerometer columns' assertion is proven to have teeth.")
    return 0


if __name__ == "__main__":
    sys.exit(_main())
