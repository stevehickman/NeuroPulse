-- NeurOne SHDR Fleet Schema — LIVE CONSTRAINT TESTS
-- Document: NP-FW-EMMC-002 Rev 1 §G.4 / schema Rev D
-- Revision: A — 2026-08-11
--
-- Runs the Rev D schema against a real PostgreSQL and proves each integrity
-- constraint REJECTS the data it exists to reject. Loading a DDL without error
-- proves only that it parses; it says nothing about whether a CHECK bites, and
-- every constraint below guards a defect that is otherwise SILENT — a regressed
-- lifetime counter, a double-counted mate cycle, a sentinel UID accumulating
-- every empty socket in the fleet. None of those surfaces as a visibly wrong
-- query result, so each one has to be falsified here.
--
-- Each negative case is wrapped so the expected rejection is caught and
-- reported; an UNEXPECTED SUCCESS fails the run. Positive controls confirm the
-- valid form of the same row is accepted, so a constraint cannot pass every
-- rejection test by being uniformly hostile — and, in practice, they are also
-- what caught a malformed bytea literal that had been making reject-cases
-- "pass" for the wrong reason during authoring.
--
-- All bytea values are built with decode(hex,'hex') rather than '\x...'::bytea:
-- the escaped-literal form is sensitive to standard_conforming_strings and to
-- psql variable interpolation, and when it misparses it does so by failing the
-- whole statement — which a reject-case scores as a PASS.
--
-- Usage: ci/run_shdr_constraint_tests.sh   (starts a throwaway cluster)
--   or:  psql -v ON_ERROR_STOP=1 -f ci/shdr/shdr_fleet_schema.sql
--        psql -f ci/shdr/shdr_schema_constraint_tests.sql

\set ON_ERROR_STOP on
\pset pager off

CREATE TEMP TABLE _results (
    id       SERIAL PRIMARY KEY,
    name     TEXT NOT NULL,
    kind     TEXT NOT NULL CHECK (kind IN ('reject', 'accept')),
    passed   BOOLEAN NOT NULL,
    detail   TEXT
);

-- must_reject: the statement is expected to raise. Passing means it raised.
CREATE OR REPLACE FUNCTION must_reject(test_name TEXT, stmt TEXT) RETURNS VOID AS $fn$
BEGIN
    BEGIN
        EXECUTE stmt;
        INSERT INTO _results(name, kind, passed, detail)
        VALUES (test_name, 'reject', FALSE, 'UNEXPECTED SUCCESS - constraint did not fire');
    EXCEPTION WHEN others THEN
        INSERT INTO _results(name, kind, passed, detail)
        VALUES (test_name, 'reject', TRUE, left(SQLERRM, 90));
    END;
END;
$fn$ LANGUAGE plpgsql;

-- must_accept: the valid form of the same row must go in.
CREATE OR REPLACE FUNCTION must_accept(test_name TEXT, stmt TEXT) RETURNS VOID AS $fn$
BEGIN
    BEGIN
        EXECUTE stmt;
        INSERT INTO _results(name, kind, passed, detail) VALUES (test_name, 'accept', TRUE, 'ok');
    EXCEPTION WHEN others THEN
        INSERT INTO _results(name, kind, passed, detail)
        VALUES (test_name, 'accept', FALSE, 'UNEXPECTED REJECTION - ' || left(SQLERRM, 70));
    END;
END;
$fn$ LANGUAGE plpgsql;

-- Hex strings only; every use wraps them in decode(...) and renders with %L.
\set hexA '11111111111111111111111111111111111111111111111111111111111111AA'
\set hexB '22222222222222222222222222222222222222222222222222222222222222BB'
\set uidA 'AABBCCDDEEFF0011'
\set uidB 'AABBCCDDEEFF0022'

INSERT INTO devices (warranty_token, device_model, hw_revision, manufacture_date, factory_id)
VALUES (decode(:'hexA','hex'), 'NP-T1-HOME-STD', 'RevC', '2026-01-15', 'FAC01'),
       (decode(:'hexB','hex'), 'NP-T1-HOME-STD', 'RevC', '2026-01-15', 'FAC01');

\o /dev/null

-- ===========================================================================
-- SOCKET NUMBERING (NUMBER-1) — socket 0 does not exist
-- ===========================================================================
SELECT must_reject('socket_number = 0 rejected (NUMBER-1; a 0 means a firmware 0-based socket_id leaked past its conversion boundary)',
  format('INSERT INTO module_inventory (warranty_token, socket_number, module_uid) VALUES (%L, 0, %L)',
         decode(:'hexA','hex'), decode(:'uidA','hex')));

SELECT must_reject('socket_number = 129 rejected (above the 7-bit major field)',
  format('INSERT INTO module_inventory (warranty_token, socket_number, module_uid) VALUES (%L, 129, %L)',
         decode(:'hexA','hex'), decode(:'uidA','hex')));

SELECT must_accept('socket_number = 1 accepted (lowest real socket)',
  format('INSERT INTO module_inventory (warranty_token, socket_number, module_uid, module_part_type) VALUES (%L, 1, %L, %L)',
         decode(:'hexA','hex'), decode(:'uidA','hex'), 'T1-A-PBM-BASE'));

-- ===========================================================================
-- MODULE UID — width and degenerate values
-- ===========================================================================
SELECT must_reject('7-byte module_uid rejected (an off-length UID aliases one module onto another lifetime record)',
  format('INSERT INTO module_inventory (warranty_token, socket_number, module_uid) VALUES (%L, 2, %L)',
         decode(:'hexA','hex'), decode('AABBCCDDEEFF00','hex')));

SELECT must_reject('all-zero module_uid rejected (np_module_map empty-socket sentinel)',
  format('INSERT INTO module_inventory (warranty_token, socket_number, module_uid) VALUES (%L, 2, %L)',
         decode(:'hexA','hex'), decode('0000000000000000','hex')));

SELECT must_reject('all-ones module_uid rejected (floating bus / unprogrammed EEPROM)',
  format('INSERT INTO module_inventory (warranty_token, socket_number, module_uid) VALUES (%L, 2, %L)',
         decode(:'hexA','hex'), decode('FFFFFFFFFFFFFFFF','hex')));

SELECT must_accept('NULL module_uid accepted (an empty socket is NULL, never the sentinel)',
  format('INSERT INTO module_inventory (warranty_token, socket_number, module_uid) VALUES (%L, 3, NULL)',
         decode(:'hexA','hex')));

-- ===========================================================================
-- OCCUPANCY UNIQUENESS
-- ===========================================================================
SELECT must_reject('second row for the same (device, socket) rejected (the consumable_counts lesson, at 80x the row count)',
  format('INSERT INTO module_inventory (warranty_token, socket_number, module_uid) VALUES (%L, 1, %L)',
         decode(:'hexA','hex'), decode(:'uidB','hex')));

SELECT must_reject('one module in two sockets at once rejected (a ghost occupant double-counts every rollup)',
  format('INSERT INTO module_inventory (warranty_token, socket_number, module_uid) VALUES (%L, 4, %L)',
         decode(:'hexA','hex'), decode(:'uidA','hex')));

SELECT must_accept('the SAME module in the SAME socket on a DIFFERENT device accepted (modules move between devices)',
  format('INSERT INTO module_inventory (warranty_token, socket_number, module_uid, module_part_type) VALUES (%L, 1, %L, %L)',
         decode(:'hexB','hex'), decode(:'uidA','hex'), 'T1-A-PBM-BASE'));

SELECT must_accept('many empty sockets accepted (the partial index must not collide on NULL)',
  format('INSERT INTO module_inventory (warranty_token, socket_number, module_uid) VALUES (%L, 5, NULL), (%L, 6, NULL)',
         decode(:'hexA','hex'), decode(:'hexA','hex')));

-- ===========================================================================
-- RETENTION ANCHOR — a DEFAULT is not a constraint
-- ===========================================================================
SELECT must_reject('day-granular last_seen_month rejected (the DEFAULT only applies when the column is omitted)',
  format('INSERT INTO module_inventory (warranty_token, socket_number, module_uid, last_seen_month) VALUES (%L, 7, NULL, DATE ''2026-08-11'')',
         decode(:'hexA','hex')));

SELECT must_accept('month-truncated last_seen_month accepted',
  format('INSERT INTO module_inventory (warranty_token, socket_number, module_uid, last_seen_month) VALUES (%L, 8, NULL, DATE ''2026-08-01'')',
         decode(:'hexA','hex')));

-- ===========================================================================
-- module_life — partition, monotonicity, and the SUM rollup
-- ===========================================================================
SELECT must_accept('module_life partial for device A accepted',
  format('INSERT INTO module_life (warranty_token, module_uid, module_part_type, module_manufacture_date,
            module_session_count, module_mate_cycles_observed, distinct_socket_count)
          VALUES (%L, %L, %L, DATE ''2026-01-02'', 100, 4, 2)',
         decode(:'hexA','hex'), decode(:'uidA','hex'), 'T1-A-PBM-BASE'));

SELECT must_accept('module_life partial for device B, SAME module, accepted (the partition is what makes each row single-writer)',
  format('INSERT INTO module_life (warranty_token, module_uid, module_part_type, module_manufacture_date,
            module_session_count, module_mate_cycles_observed, distinct_socket_count)
          VALUES (%L, %L, %L, DATE ''2026-01-02'', 40, 2, 1)',
         decode(:'hexB','hex'), decode(:'uidA','hex'), 'T1-A-PBM-BASE'));

SELECT must_reject('duplicate (device, module) partial rejected',
  format('INSERT INTO module_life (warranty_token, module_uid, module_part_type, module_manufacture_date)
          VALUES (%L, %L, %L, DATE ''2026-01-02'')',
         decode(:'hexA','hex'), decode(:'uidA','hex'), 'T1-A-PBM-BASE'));

SELECT must_reject('all-zero module_uid rejected in module_life (would become a fake module accumulating every empty socket in the fleet)',
  format('INSERT INTO module_life (warranty_token, module_uid, module_part_type, module_manufacture_date)
          VALUES (%L, %L, %L, DATE ''2026-01-02'')',
         decode(:'hexA','hex'), decode('0000000000000000','hex'), 'T1-A-PBM-BASE'));

-- THE central invariant: accumulators are grow-only. A "+=" writer or an
-- out-of-order replay must be refused by the DATABASE, because a regressed
-- counter is invisible in the slope and RUL bucket derived from it.
SELECT must_reject('session_count going BACKWARDS rejected by the monotonicity trigger',
  format('UPDATE module_life SET module_session_count = 90 WHERE warranty_token = %L AND module_uid = %L',
         decode(:'hexA','hex'), decode(:'uidA','hex')));

SELECT must_reject('mate_cycles going BACKWARDS rejected by the monotonicity trigger',
  format('UPDATE module_life SET module_mate_cycles_observed = 1 WHERE warranty_token = %L AND module_uid = %L',
         decode(:'hexA','hex'), decode(:'uidA','hex')));

SELECT must_accept('GREATEST-style merge forward accepted',
  format('UPDATE module_life SET module_session_count = 130 WHERE warranty_token = %L AND module_uid = %L',
         decode(:'hexA','hex'), decode(:'uidA','hex')));

SELECT must_accept('idempotent re-merge of the SAME absolute total accepted (equal is not a regression)',
  format('UPDATE module_life SET module_session_count = 130 WHERE warranty_token = %L AND module_uid = %L',
         decode(:'hexA','hex'), decode(:'uidA','hex')));

DO $blk$
DECLARE total INT; partials INT;
BEGIN
    SELECT module_session_count_lower_bound, device_partial_count INTO total, partials
      FROM module_life_fleet_lower_bound WHERE module_uid = decode('AABBCCDDEEFF0011','hex');
    INSERT INTO _results(name, kind, passed, detail) VALUES (
        'fleet view SUMs the per-device partials (130 + 40 = 170 across 2 devices, order-free)',
        'accept', COALESCE(total = 170 AND partials = 2, FALSE),
        format('total=%s partials=%s', total, partials));
END $blk$;

-- ===========================================================================
-- module_placement_events — idempotent ingest, NO_RESPONSE vs REMOVED
-- ===========================================================================
SELECT must_accept('placement event accepted',
  format('INSERT INTO module_placement_events (warranty_token, socket_number, module_uid, event_kind,
            detection, power_cycle_index, placement_index, module_mate_cycles_observed, socket_mate_cycles_observed)
          VALUES (%L, 1, %L, ''INSERTED'', ''BOOT_DIFF'', 12, 3, 4, 9)',
         decode(:'hexA','hex'), decode(:'uidA','hex')));

SELECT must_reject('replayed upload of the SAME event rejected (would double-count mate cycles against the >=500 rating)',
  format('INSERT INTO module_placement_events (warranty_token, socket_number, module_uid, event_kind,
            detection, power_cycle_index, placement_index, module_mate_cycles_observed, socket_mate_cycles_observed)
          VALUES (%L, 1, %L, ''INSERTED'', ''BOOT_DIFF'', 12, 3, 4, 9)',
         decode(:'hexA','hex'), decode(:'uidA','hex')));

SELECT must_accept('NO_RESPONSE recorded distinctly from REMOVED (a cluster/mux fault must not forge a removal burst)',
  format('INSERT INTO module_placement_events (warranty_token, socket_number, module_uid, event_kind,
            detection, power_cycle_index, placement_index, module_mate_cycles_observed,
            socket_mate_cycles_observed, cluster_fault_suspected)
          VALUES (%L, 9, NULL, ''NO_RESPONSE'', ''BOOT_DIFF'', 13, 4, 0, 0, TRUE)',
         decode(:'hexA','hex')));

SELECT must_reject('unknown event_kind rejected',
  format('INSERT INTO module_placement_events (warranty_token, socket_number, module_uid, event_kind,
            detection, power_cycle_index, placement_index, module_mate_cycles_observed, socket_mate_cycles_observed)
          VALUES (%L, 1, %L, ''WIGGLED'', ''BOOT_DIFF'', 14, 5, 4, 9)',
         decode(:'hexA','hex'), decode(:'uidA','hex')));

SELECT must_reject('negative mate-cycle counter rejected',
  format('INSERT INTO module_placement_events (warranty_token, socket_number, module_uid, event_kind,
            detection, power_cycle_index, placement_index, module_mate_cycles_observed, socket_mate_cycles_observed)
          VALUES (%L, 1, %L, ''REMOVED'', ''HOTPLUG'', 15, 6, -1, 9)',
         decode(:'hexA','hex'), decode(:'uidA','hex')));

-- ===========================================================================
-- pbm_module_telemetry — idempotent ingest (Rev C had none, so a re-upload
-- silently duplicated every zone row and skewed the aging trend)
-- ===========================================================================
SELECT must_accept('PBM sample accepted',
  format('INSERT INTO pbm_module_telemetry (warranty_token, socket_number, module_uid, module_part_type,
            wavelength_nm, session_index, module_session_index, placement_index, pd1_pd2_ratio_final,
            cal_source, peak_ntc_celsius)
          VALUES (%L, 1, %L, %L, 660, 500, 130, 3, 0.94, ''FACTORY'', 41.2)',
         decode(:'hexA','hex'), decode(:'uidA','hex'), 'T1-A-PBM-BASE'));

SELECT must_reject('re-uploaded PBM sample for the same session rejected',
  format('INSERT INTO pbm_module_telemetry (warranty_token, socket_number, module_uid, module_part_type,
            wavelength_nm, session_index, module_session_index, placement_index, pd1_pd2_ratio_final,
            cal_source, peak_ntc_celsius)
          VALUES (%L, 1, %L, %L, 660, 500, 130, 3, 0.94, ''FACTORY'', 41.2)',
         decode(:'hexA','hex'), decode(:'uidA','hex'), 'T1-A-PBM-BASE'));

SELECT must_accept('a DIFFERENT wavelength in the same session accepted (one module emits several)',
  format('INSERT INTO pbm_module_telemetry (warranty_token, socket_number, module_uid, module_part_type,
            wavelength_nm, session_index, module_session_index, placement_index, pd1_pd2_ratio_final,
            cal_source, peak_ntc_celsius)
          VALUES (%L, 1, %L, %L, 808, 500, 130, 3, 0.91, ''FACTORY'', 43.0)',
         decode(:'hexA','hex'), decode(:'uidA','hex'), 'T1-A-PBM-BASE'));

SELECT must_accept('the SAME module reporting from a DIFFERENT socket accepted (this is the rotation case Rev C could not represent)',
  format('INSERT INTO pbm_module_telemetry (warranty_token, socket_number, module_uid, module_part_type,
            wavelength_nm, session_index, module_session_index, placement_index, pd1_pd2_ratio_final,
            cal_source, peak_ntc_celsius)
          VALUES (%L, 30, %L, %L, 660, 501, 131, 4, 0.93, ''FACTORY'', 40.1)',
         decode(:'hexA','hex'), decode(:'uidA','hex'), 'T1-A-PBM-BASE'));

SELECT must_reject('unsupported wavelength rejected',
  format('INSERT INTO pbm_module_telemetry (warranty_token, socket_number, module_uid, module_part_type,
            wavelength_nm, session_index, module_session_index, placement_index, pd1_pd2_ratio_final,
            cal_source, peak_ntc_celsius)
          VALUES (%L, 1, %L, %L, 532, 502, 132, 3, 0.94, ''FACTORY'', 41.2)',
         decode(:'hexA','hex'), decode(:'uidA','hex'), 'T1-A-PBM-BASE'));

SELECT must_reject('negative driver_fault_count rejected (Rev C omitted this CHECK while every sibling counter had one)',
  format('INSERT INTO pbm_module_telemetry (warranty_token, socket_number, module_uid, module_part_type,
            wavelength_nm, session_index, module_session_index, placement_index, pd1_pd2_ratio_final,
            cal_source, peak_ntc_celsius, driver_fault_count)
          VALUES (%L, 1, %L, %L, 660, 503, 133, 3, 0.94, ''FACTORY'', 41.2, -1)',
         decode(:'hexA','hex'), decode(:'uidA','hex'), 'T1-A-PBM-BASE'));

SELECT must_reject('out-of-range peak_ntc_celsius rejected',
  format('INSERT INTO pbm_module_telemetry (warranty_token, socket_number, module_uid, module_part_type,
            wavelength_nm, session_index, module_session_index, placement_index, pd1_pd2_ratio_final,
            cal_source, peak_ntc_celsius)
          VALUES (%L, 1, %L, %L, 660, 504, 134, 3, 0.94, ''FACTORY'', 250.0)',
         decode(:'hexA','hex'), decode(:'uidA','hex'), 'T1-A-PBM-BASE'));

-- ===========================================================================
-- socket_life / socket_part_type_wear — socket condition independent of occupancy
-- ===========================================================================
SELECT must_accept('socket_life row accepted for an EMPTY socket (wear history outlives occupancy)',
  format('INSERT INTO socket_life (warranty_token, socket_number, socket_mate_cycles_observed, distinct_module_count)
          VALUES (%L, 3, 47, 6)', decode(:'hexA','hex')));

SELECT must_reject('duplicate socket_life row rejected',
  format('INSERT INTO socket_life (warranty_token, socket_number) VALUES (%L, 3)', decode(:'hexA','hex')));

SELECT must_reject('remaining_life_bucket = 5 rejected (domain is 0..4)',
  format('INSERT INTO socket_life (warranty_token, socket_number, remaining_life_bucket) VALUES (%L, 10, 5)',
         decode(:'hexA','hex')));

SELECT must_accept('TWO module kinds sharing ONE socket accepted - this is what makes "do smart modules wear sockets faster" askable',
  format('INSERT INTO socket_part_type_wear (warranty_token, socket_number, module_part_type, mate_cycles,
            distinct_module_count, contact_resistance_delta_per_cycle)
          VALUES (%L, 3, %L, 30, 4, 0.02), (%L, 3, %L, 17, 2, 0.05)',
         decode(:'hexA','hex'), 'T1-A-PBM-BASE', decode(:'hexA','hex'), 'ZM-1064-SMART'));

SELECT must_reject('duplicate (device, socket, part type) wear row rejected',
  format('INSERT INTO socket_part_type_wear (warranty_token, socket_number, module_part_type, mate_cycles)
          VALUES (%L, 3, %L, 1)', decode(:'hexA','hex'), 'ZM-1064-SMART'));

-- ===========================================================================
-- module_rotation_advice
-- ===========================================================================
SELECT must_accept('rotation advice accepted',
  format('INSERT INTO module_rotation_advice (warranty_token, advice_index, module_uid, from_socket_number,
            to_socket_number, reason_code, predicted_life_gain_sessions, advice_model_phase, advice_model_version)
          VALUES (%L, 1, %L, 1, 12, ''DERATE_BALANCE'', 400, 1, ''ph1-2026.08'')',
         decode(:'hexA','hex'), decode(:'uidA','hex')));

SELECT must_reject('advice to move a module to the socket it already occupies rejected',
  format('INSERT INTO module_rotation_advice (warranty_token, advice_index, module_uid, from_socket_number,
            to_socket_number, reason_code, advice_model_phase, advice_model_version)
          VALUES (%L, 2, %L, 7, 7, ''DERATE_BALANCE'', 1, ''ph1-2026.08'')',
         decode(:'hexA','hex'), decode(:'uidA','hex')));

SELECT must_reject('a SECOND open recommendation for the same module rejected (else every dock appends another PENDING row and snooze accounting has no row to count against)',
  format('INSERT INTO module_rotation_advice (warranty_token, advice_index, module_uid, from_socket_number,
            to_socket_number, reason_code, advice_model_phase, advice_model_version)
          VALUES (%L, 2, %L, 12, 30, ''THERMAL_BALANCE'', 1, ''ph1-2026.08'')',
         decode(:'hexA','hex'), decode(:'uidA','hex')));

SELECT must_accept('resolving the open recommendation accepted',
  format('UPDATE module_rotation_advice SET outcome = ''APPLIED'', applied_placement_index = 3
          WHERE warranty_token = %L AND module_uid = %L AND advice_index = 1',
         decode(:'hexA','hex'), decode(:'uidA','hex')));

SELECT must_accept('...and the follow-up recommendation now inserts',
  format('INSERT INTO module_rotation_advice (warranty_token, advice_index, module_uid, from_socket_number,
            to_socket_number, reason_code, advice_model_phase, advice_model_version)
          VALUES (%L, 2, %L, 12, 30, ''SOCKET_CYCLE_LIMIT'', 1, ''ph1-2026.08'')',
         decode(:'hexA','hex'), decode(:'uidA','hex')));

SELECT must_reject('unknown reason_code rejected',
  format('INSERT INTO module_rotation_advice (warranty_token, advice_index, module_uid, from_socket_number,
            to_socket_number, reason_code, advice_model_phase, advice_model_version)
          VALUES (%L, 3, %L, 30, 40, ''VIBES'', 1, ''ph1-2026.08'')',
         decode(:'hexA','hex'), decode(:'uidA','hex')));

SELECT must_reject('advice_model_phase = 4 rejected (5.2 defines phases 1-3)',
  format('INSERT INTO module_rotation_advice (warranty_token, advice_index, module_uid, from_socket_number,
            to_socket_number, reason_code, advice_model_phase, advice_model_version)
          VALUES (%L, 3, %L, 30, 40, ''EOL_DEFER'', 4, ''ph9'')',
         decode(:'hexA','hex'), decode(:'uidA','hex')));

SELECT must_reject('unknown reminder_class rejected',
  format('INSERT INTO module_rotation_advice (warranty_token, advice_index, module_uid, from_socket_number,
            to_socket_number, reason_code, advice_model_phase, advice_model_version, reminder_class)
          VALUES (%L, 3, %L, 30, 40, ''EOL_DEFER'', 1, ''ph1'', ''URGENT'')',
         decode(:'hexA','hex'), decode(:'uidA','hex')));

-- ===========================================================================
-- Referential integrity + the no-join boundary
-- ===========================================================================
SELECT must_reject('telemetry for an unknown warranty_token rejected',
  format('INSERT INTO module_inventory (warranty_token, socket_number, module_uid) VALUES (%L, 1, NULL)',
         decode('DEADBEEF00000000000000000000000000000000000000000000000000000000','hex')));

-- ===========================================================================
-- Report
-- ===========================================================================
\o
\echo ''
\echo 'NeurOne SHDR Rev D - live constraint tests (real PostgreSQL)'
\echo ''

SELECT CASE WHEN passed THEN '  PASS  ' ELSE '  FAIL  ' END || rpad(kind, 8) || name ||
       CASE WHEN passed THEN '' ELSE E'\n          <<< ' || detail END AS result
FROM _results ORDER BY id;

\echo ''
SELECT format('RESULT: %s - %s/%s passed (%s reject-cases, %s accept-cases)',
              CASE WHEN bool_and(passed) THEN 'PASS' ELSE 'FAIL' END,
              count(*) FILTER (WHERE passed), count(*),
              count(*) FILTER (WHERE kind = 'reject'),
              count(*) FILTER (WHERE kind = 'accept')) AS summary
FROM _results;

-- Non-zero psql exit when anything failed, so CI can gate on it.
\set ON_ERROR_STOP on
DO $blk$
DECLARE bad INT;
BEGIN
    SELECT count(*) INTO bad FROM _results WHERE NOT passed;
    IF bad > 0 THEN
        RAISE EXCEPTION '% constraint test(s) failed', bad;
    END IF;
END $blk$;
