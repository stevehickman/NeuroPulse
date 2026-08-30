#!/usr/bin/env python3
"""
Self-test for the CHAR-01 characterisation-window gate — NP-FW-EMMC-002 §H.5.

CHAR-01 permits a narrow exception to §G.3: the shdr_accel_characterisation
table may carry impact_* columns that ACCEL-01 otherwise rejects outright. A
gate that GRANTS a permission is more dangerous than one that withholds it, so
it needs more falsification, not less — and a gate that has only ever been run
against a passing tree has demonstrated nothing (NP-CONV-001 §4.0.1a).

Three properties are falsified here, each by constructing the broken world and
confirming the gate rejects it:

  (a) A non-consented device carrying extended fields must FAIL.
      Expressed statically: a schema whose characterisation table would ADMIT a
      non-consented row — i.e. whose char_consent_granted lacks the CHECK that
      only TRUE satisfies — must FAIL. The live counterpart, an actual INSERT
      with char_consent_granted = FALSE being rejected by PostgreSQL, is in
      ci/shdr/shdr_schema_constraint_tests.sql.

  (b) An expired window still carrying the fields must FAIL.
      A firmware header with NP_ACCEL_CHAR_WINDOW_ENABLED = 0 plus a schema that
      still has the table must FAIL, so the data cannot outlive the programme.

  (c) The exemption is SELF-REVOKING.
      This is the property the design turns on and it needs its own probe: when
      CHAR-01 fails for ANY reason, characterisation_exempt_columns() must
      return nothing, so ACCEL-01 and ACCEL-02 immediately bite on the same
      impact_* columns they were permitting a moment earlier. A test that only
      checked CHAR-01's own verdict would pass even if the exemption were a
      static constant that never withdrew.

Plus a vacuity control: the real schema must actually be EXERCISING the
exemption. If it were not, every assertion above would be about a code path the
production tree never enters.

Runnable two ways:
  python3 ci/test_shdr_char_gate_selftest.py
  pytest ci/test_shdr_char_gate_selftest.py

CI-Kind: self-test
CI-Covers: ci/test_shdr_schema.py
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import test_shdr_schema as gate  # noqa: E402
from test_shdr_schema import (  # noqa: E402
    SCHEMA_DEFAULT,
    CHAR_TABLE,
    CHAR_EXTENDED_COLUMNS,
    parse_columns,
    parse_firmware_constants,
    check_characterisation_window,
    characterisation_exempt_columns,
    check_prohibited_accel_columns,
    check_accel_table_numeric_types,
)

# ---------------------------------------------------------------------------
# Fixtures
# ---------------------------------------------------------------------------

# A minimal well-formed characterisation table. Kept in sync with the real one
# by test_fixture_matches_production_shape below — a fixture that has drifted
# from the schema it stands in for tests nothing.
GOOD_TABLE = """
CREATE TABLE shdr_accel_characterisation (
    id                    BIGSERIAL  PRIMARY KEY,
    warranty_token        BYTEA      NOT NULL REFERENCES devices(warranty_token),
    char_programme_id     SMALLINT   NOT NULL CHECK (char_programme_id = 1),
    char_consent_granted  BOOLEAN    NOT NULL CHECK (char_consent_granted),
    char_consent_epoch    INTEGER    NOT NULL CHECK (char_consent_epoch >= 1),
    char_record_seq       INTEGER    NOT NULL CHECK (char_record_seq BETWEEN 1 AND 512),
    impact_g_bin_1        SMALLINT   NOT NULL DEFAULT 0 CHECK (impact_g_bin_1 >= 0),
    impact_event_count    SMALLINT   NOT NULL DEFAULT 0 CHECK (impact_event_count >= 0),
    gap_index             INTEGER    NOT NULL CHECK (gap_index >= 0),
    ingest_month          DATE       NOT NULL DEFAULT DATE_TRUNC('month', NOW())::date
);
"""


class FakeHeader:
    """Stands in for firmware/shdr/include/np_accel_shdr.h."""

    def __init__(self, tmp: Path, *, enabled=1, programme=1, budget=512, omit=None):
        lines = [
            "/* synthetic header for gate falsification */",
            f"#define NP_ACCEL_CHAR_WINDOW_ENABLED       {enabled}",
            f"#define NP_ACCEL_CHAR_PROGRAMME_ID         {programme}u",
            f"#define NP_ACCEL_CHAR_RECORD_BUDGET        {budget}u",
        ]
        if omit:
            lines = [ln for ln in lines if omit not in ln]
        self.path = tmp
        self.path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def with_header(path: Path):
    """Point the gate at a synthetic firmware header for one assertion."""
    gate.FIRMWARE_ACCEL_HEADER = path


def restore_header():
    gate.FIRMWARE_ACCEL_HEADER = (
        Path(__file__).resolve().parent.parent
        / "firmware" / "shdr" / "include" / "np_accel_shdr.h"
    )


def failures_for(sql: str) -> list:
    return check_characterisation_window(sql, parse_columns(sql))


# ---------------------------------------------------------------------------
# Falsification (a) — a non-consented device carrying extended fields
# ---------------------------------------------------------------------------

def test_a_missing_consent_check_fails(tmp_path_factory=None):
    tmp = Path(_tmpdir()) / "hdr_a.h"
    FakeHeader(tmp)
    with_header(tmp)
    try:
        # The consent CHECK removed: this schema would ACCEPT a row from a
        # device that never opted in.
        bad = GOOD_TABLE.replace(
            "char_consent_granted  BOOLEAN    NOT NULL CHECK (char_consent_granted),",
            "char_consent_granted  BOOLEAN    NOT NULL DEFAULT FALSE,",
        )
        assert bad != GOOD_TABLE, "fixture mutation did not apply"
        f = failures_for(bad)
        assert f, "a schema that admits a non-consented row must FAIL CHAR-01"
        assert any("char_consent_granted" in x.description for x in f), \
            f"failure must name the consent marker, got: {[x.description for x in f]}"

        # Control: the unmutated fixture passes, so the assertion above is about
        # the mutation and not about the fixture being broken all along.
        assert not failures_for(GOOD_TABLE), \
            "control: the well-formed table must PASS CHAR-01"
    finally:
        restore_header()


def test_a_consent_marker_dropped_entirely_fails():
    tmp = Path(_tmpdir()) / "hdr_a2.h"
    FakeHeader(tmp)
    with_header(tmp)
    try:
        bad = "\n".join(
            ln for ln in GOOD_TABLE.splitlines()
            if "char_consent_granted" not in ln
        )
        f = failures_for(bad)
        assert any("missing required marker 'char_consent_granted'" in x.description
                   for x in f), \
            f"a table with no consent column at all must FAIL, got: {[x.description for x in f]}"
    finally:
        restore_header()


# ---------------------------------------------------------------------------
# Falsification (b) — an expired window still carrying the fields
# ---------------------------------------------------------------------------

def test_b_closed_window_with_table_present_fails():
    tmp = Path(_tmpdir()) / "hdr_b.h"
    FakeHeader(tmp, enabled=0)          # the window has closed
    with_header(tmp)
    try:
        f = failures_for(GOOD_TABLE)    # ...but the table is still here
        assert f, "a closed window with the table still present must FAIL"
        assert any("WINDOW_ENABLED is 0" in x.description for x in f), \
            f"failure must name the closed window, got: {[x.description for x in f]}"

        # Control: the SAME schema with the window open passes. Without this the
        # assertion above could be satisfied by a table that never passes at all.
        FakeHeader(tmp, enabled=1)
        assert not failures_for(GOOD_TABLE), \
            "control: same table, open window, must PASS"
    finally:
        restore_header()


def test_b_closed_window_without_table_passes():
    """The retired end-state must be reachable: window 0 AND no table is fine."""
    tmp = Path(_tmpdir()) / "hdr_b2.h"
    FakeHeader(tmp, enabled=0)
    with_header(tmp)
    try:
        assert not failures_for("CREATE TABLE devices (\n    warranty_token BYTEA NOT NULL\n);\n"), \
            "window closed and table dropped is the correct retired state"
    finally:
        restore_header()


# ---------------------------------------------------------------------------
# (c) — the exemption is self-revoking
# ---------------------------------------------------------------------------

def test_c_exemption_withdraws_when_char01_fails():
    tmp = Path(_tmpdir()) / "hdr_c.h"
    FakeHeader(tmp)
    with_header(tmp)
    try:
        cols_ok = parse_columns(GOOD_TABLE)
        exempt_ok = characterisation_exempt_columns(GOOD_TABLE, cols_ok)
        assert exempt_ok, "control: a passing CHAR-01 must grant a non-empty exemption"
        assert not check_prohibited_accel_columns(cols_ok, exempt_ok), \
            "control: impact_* columns are permitted while the exemption holds"
        assert not check_accel_table_numeric_types(cols_ok, exempt_ok), \
            "control: SMALLINT histogram columns are permitted while it holds"

        # Now break CHAR-01 in a way unrelated to the impact_* columns
        # themselves: weaken the record_seq bound. The impact_* columns are
        # untouched — only the permission is.
        bad = GOOD_TABLE.replace(
            "CHECK (char_record_seq BETWEEN 1 AND 512)",
            "CHECK (char_record_seq >= 1)",
        )
        cols_bad = parse_columns(bad)
        assert failures_for(bad), "weakening the budget bound must fail CHAR-01"

        exempt_bad = characterisation_exempt_columns(bad, cols_bad)
        assert exempt_bad == set(), \
            "the exemption must WITHDRAW when CHAR-01 fails — this is what makes " \
            "the exception self-revoking rather than a standing permission"

        a1 = check_prohibited_accel_columns(cols_bad, exempt_bad)
        a2 = check_accel_table_numeric_types(cols_bad, exempt_bad)
        assert a1, "ACCEL-01 must bite on impact_* once the exemption lapses"
        assert a2, "ACCEL-02 must bite on the SMALLINT histogram once it lapses"
    finally:
        restore_header()


def test_c_marker_outside_registered_table_fails():
    """The exemption's own signal must not be paste-able onto another table."""
    tmp = Path(_tmpdir()) / "hdr_c2.h"
    FakeHeader(tmp)
    with_header(tmp)
    try:
        sql = GOOD_TABLE + """
CREATE TABLE some_other_table (
    id                    BIGSERIAL  PRIMARY KEY,
    char_consent_granted  BOOLEAN    NOT NULL CHECK (char_consent_granted),
    peak_g                REAL       NOT NULL
);
"""
        f = failures_for(sql)
        assert any("some_other_table" in x.location for x in f), \
            "a char_* marker outside the registered table must FAIL"
    finally:
        restore_header()


# ---------------------------------------------------------------------------
# Fail-closed on WRONG, not only on ERROR
# ---------------------------------------------------------------------------

def test_missing_firmware_header_fails():
    with_header(Path(_tmpdir()) / "does_not_exist.h")
    try:
        f = failures_for(GOOD_TABLE)
        assert f, "an unreadable firmware header must FAIL, never skip"
        assert any("could not be read" in x.description for x in f)
    finally:
        restore_header()


def test_renamed_constant_fails():
    """
    The valid-but-wrong case. A header that parses perfectly but no longer
    defines the constant under the expected name must FAIL. Guarding against the
    parse ERRORING is not the same as guarding against it being WRONG, and this
    is the shape that slips through — the file exists, nothing raises, and the
    check silently has nothing to compare.
    """
    tmp = Path(_tmpdir()) / "hdr_renamed.h"
    FakeHeader(tmp, omit="NP_ACCEL_CHAR_RECORD_BUDGET")
    with_header(tmp)
    try:
        f = failures_for(GOOD_TABLE)
        assert f, "a renamed/absent constant must FAIL"
        assert any("not parseable" in x.description for x in f), \
            f"got: {[x.description for x in f]}"
    finally:
        restore_header()


def test_schema_and_firmware_must_agree_on_budget():
    tmp = Path(_tmpdir()) / "hdr_budget.h"
    FakeHeader(tmp, budget=256)     # firmware says 256; the schema CHECK says 512
    with_header(tmp)
    try:
        f = failures_for(GOOD_TABLE)
        assert any("char_record_seq" in x.description for x in f), \
            "a schema/firmware budget disagreement must FAIL"
    finally:
        restore_header()


def test_schema_and_firmware_must_agree_on_programme_id():
    tmp = Path(_tmpdir()) / "hdr_prog.h"
    FakeHeader(tmp, programme=2)
    with_header(tmp)
    try:
        f = failures_for(GOOD_TABLE)
        assert any("char_programme_id" in x.description for x in f), \
            "a schema/firmware programme-id disagreement must FAIL"
    finally:
        restore_header()


# ---------------------------------------------------------------------------
# Vacuity + drift controls against the REAL tree
# ---------------------------------------------------------------------------

def test_production_schema_actually_exercises_the_exemption():
    """
    Everything above tests a permission. If the production schema never used it,
    all of it would be about a path the tree never enters.
    """
    restore_header()
    sql = SCHEMA_DEFAULT.read_text(encoding="utf-8")
    cols = parse_columns(sql)
    exempt = characterisation_exempt_columns(sql, cols)
    assert exempt, (
        "the production schema is not exercising the CHAR-01 exemption — either "
        "the characterisation table is gone (in which case this self-test is "
        "vacuous and should be retired with §H) or CHAR-01 is failing"
    )
    assert len(exempt) == len(CHAR_EXTENDED_COLUMNS), (
        f"expected all {len(CHAR_EXTENDED_COLUMNS)} extended columns exempted, "
        f"got {len(exempt)}"
    )


def test_production_firmware_window_is_open():
    restore_header()
    consts, reason = parse_firmware_constants()
    assert consts is not None, reason
    assert consts["window_enabled"] == 1, (
        "the production header has NP_ACCEL_CHAR_WINDOW_ENABLED = 0 while the "
        "schema still carries the characterisation table — CHAR-01 direction (b)"
    )


def test_fixture_matches_production_shape():
    """
    The fixture stands in for the real table. If the real one grows a marker the
    fixture lacks, every falsification above would be exercising a shape that no
    longer exists.
    """
    restore_header()
    sql = SCHEMA_DEFAULT.read_text(encoding="utf-8")
    real = {c.column for c in parse_columns(sql) if c.table == CHAR_TABLE}
    fixture = {c.column for c in parse_columns(GOOD_TABLE) if c.table == CHAR_TABLE}
    markers_real = {c for c in real if c.startswith("char_")}
    markers_fix = {c for c in fixture if c.startswith("char_")}
    assert markers_real == markers_fix, (
        f"fixture has drifted from the production table's markers: "
        f"real={sorted(markers_real)} fixture={sorted(markers_fix)}"
    )


def test_permitted_accel_columns_was_not_widened():
    """
    The brief's explicit prohibition. Admitting the §H fields by adding them to
    the global allowlist would have deleted the protection for the entire schema
    in order to permit them in one table.
    """
    assert gate.PERMITTED_ACCEL_COLUMNS == frozenset(
        {"drop_detected", "maintenance_alert"}
    ), "PERMITTED_ACCEL_COLUMNS must remain exactly the two §G.2 booleans"
    for name in CHAR_EXTENDED_COLUMNS:
        assert name not in gate.PERMITTED_ACCEL_COLUMNS


# ---------------------------------------------------------------------------
# Runner
# ---------------------------------------------------------------------------

_TMP: str | None = None


def _tmpdir() -> str:
    global _TMP
    if _TMP is None:
        import tempfile
        _TMP = tempfile.mkdtemp(prefix="np-char-gate-")
    return _TMP


def main() -> int:
    tests = [(n, f) for n, f in sorted(globals().items())
             if n.startswith("test_") and callable(f)]
    print("CHAR-01 characterisation-gate self-test — NP-FW-EMMC-002 §H.5")
    print(f"Schema: {SCHEMA_DEFAULT}\n")

    failed = 0
    for name, fn in tests:
        try:
            fn()
            print(f"  PASS  {name}")
        except AssertionError as e:
            print(f"  FAIL  {name}\n          <<< {e}")
            failed += 1
        except Exception as e:  # noqa: BLE001
            print(f"  ERROR {name}\n          <<< {type(e).__name__}: {e}")
            failed += 1

    print()
    if failed:
        print(f"RESULT: FAIL — {failed} of {len(tests)} falsification(s) did not hold")
        return 1
    print(f"RESULT: PASS — {len(tests)} falsifications held")
    return 0


if __name__ == "__main__":
    sys.exit(main())
