-- NeurOne SHDR fleet ingest — reference batch writer (OI-PRIV-76, REQ-SHDR-12)
-- Document: NP-INFRA-001 §3.4 (the batching is the deployment rule there)
-- Loads AFTER ci/shdr/shdr_fleet_schema.sql (schema Rev G), into the same database.
--
-- WHAT THIS REMOVES. PostgreSQL gives every row a transaction ID (xmin, assigned
-- in commit order across the whole database) and a physical position (ctid,
-- roughly insert order). An ingest that writes each upload in its own transaction
-- leaves xmin ordering every device's dockings within a month, and groups one
-- docking's rows across every table; an outside time anchor (ingest logs, WAL or
-- backup times) turns that order into roughly day-level time beside
-- warranty_token. Schema Rev G removed the sequences (REQ-SHDR-11 (c)); these
-- system columns cannot be removed, so the ingest must give them nothing to say.
--
-- HOW. The ingest service stages what it receives, and this writer moves one
-- closed calendar month of staged records into the SHDR tables:
--   * in ONE transaction, so every row the batch writes carries the same xmin;
--   * in RANDOM order (INSERT … SELECT … ORDER BY random()), so ctid order
--     carries nothing about which device docked first;
--   * with every month column set to the batch's month, so the batch says
--     nothing ingest_month does not already say;
--   * merging a device that docked more than once in the month by its own
--     counters, never by arrival order (the shuffle must not decide a merge);
--   * and dropping that month's staging tables in the same transaction.
--
-- WHY A CALENDAR MONTH AND NOT A COUNT. A batch's xmin tells a reader which batch
-- a row came in. If batches are calendar months, that is exactly ingest_month,
-- which every row already carries (REQ-SHDR-11 holds no time finer than a month),
-- so xmin adds nothing. A batch closed by a count would split a month at a moment
-- set by fleet traffic, and xmin would then say "before or after that moment",
-- which is finer than a month. The batch size is therefore derived from
-- REQ-SHDR-11's granularity, not chosen: it moves if that granularity moves.
-- A count adds nothing on the other side either: all rows of one batch share one
-- xmin whatever the count, so a month with few devices is no worse than its
-- ingest_month already is.
--
-- WHAT STAGING HOLDS, AND FOR HOW LONG. One UNLOGGED table per (target, month) in
-- schema np_stage, created when the month opens and DROPPED by the transaction
-- that writes the month's batch, so the record lives in staging only until its
-- batch is written and nowhere else in the database:
--   * UNLOGGED: staged rows write no WAL, so no WAL archive, streaming replica or
--     point-in-time backup holds them in arrival order, and pg_basebackup omits
--     unlogged data. (A staging transaction still writes a commit record with its
--     time and xid; the xid survives only in the staging rows, which are dropped,
--     so what the WAL keeps is when uploads arrived, never which device sent one.)
--   * DROPPED, not DELETEd: a DELETE leaves dead tuples, still carrying the record
--     and its arrival-order xmin, in the heap until VACUUM reuses the space; a
--     dropped table's file is unlinked at commit.
--   * THE COST. An unlogged table is emptied by crash recovery, so a server crash
--     loses the open month's staged records (the uploads were already answered
--     200, so the phones do not resend). Recorded in NP-INFRA-001 §3.4 and
--     OI-PRIV-76 as the price of keeping arrival order out of the WAL; a durable
--     alternative must keep it out by another route.
--   * WHAT STAGING STILL SHOWS. While a month is open, its staging rows carry
--     per-arrival xmin and ctid: whoever can read np_stage reads that month's
--     arrival order. np_stage is therefore closed to PUBLIC here and must be
--     readable only by the ingest role (NP-INFRA-001 §3.4).
--
-- WHAT IT DOES NOT COVER (OI-PRIV-76, left open):
--   * The tables whose merge rule is not order-free yet: module_inventory,
--     socket_life, socket_part_type_wear, consumable_counts and
--     module_rotation_advice (np_batch_unsupported_tables()). Each needs a rule
--     that picks the newer of two uploads in one month by a per-device counter,
--     not by arrival. np_stage_open() creates no staging table for them, so an
--     ingest cannot stage them through this writer, and an ingest that writes
--     them directly, one upload per transaction, reopens the side channel for
--     those tables.
--   * Parsing the SHDR record into rows: the ingest service does not exist
--     (OI-FLEET-01 (e)).
--   * log_statement = none and the audited read access are deployment settings.

CREATE SCHEMA IF NOT EXISTS np_stage;
REVOKE ALL ON SCHEMA np_stage FROM PUBLIC;

-- The tables this writer handles, and how. A test (ci/test_shdr_batch_writer.py)
-- requires every table in the SHDR schema to appear here or in the unsupported
-- list, so a table added to the schema cannot be silently left out.
CREATE OR REPLACE FUNCTION np_batch_tables()
RETURNS TABLE (target TEXT, kind TEXT, write_order INT)
LANGUAGE sql IMMUTABLE AS $$
    VALUES
        -- Merged: one row per key, written first because every other table
        -- references devices.
        ('devices',                     'merge',  1),
        ('module_life',                 'merge',  2),
        -- Appended: one row per record; a re-delivered record that hits a
        -- UNIQUE key is skipped (ON CONFLICT DO NOTHING).
        ('firmware_history',            'append', 3),
        ('ota_events',                  'append', 3),
        ('module_placement_events',     'append', 3),
        ('pbm_module_telemetry',        'append', 3),
        ('emf_shielding_telemetry',     'append', 3),
        ('module_thermal_telemetry',    'append', 3),
        ('hub_thermal_telemetry',       'append', 3),
        ('power_telemetry',             'append', 3),
        ('accessory_auth_log',          'append', 3),
        ('calibration_history',         'append', 3),
        ('eeg_impedance_trend',         'append', 3),
        ('shdr_accel_records',          'append', 3),
        ('shdr_accel_characterisation', 'append', 3),
        ('mode_f_telemetry',            'append', 3),
        ('fault_log',                   'append', 3),
        ('storage_health',              'append', 3)
$$;

CREATE OR REPLACE FUNCTION np_batch_unsupported_tables()
RETURNS TABLE (target TEXT, reason TEXT)
LANGUAGE sql IMMUTABLE AS $$
    VALUES
        ('module_inventory',       'current occupancy: the newer of two uploads must win by placement_index; rule not written'),
        ('socket_life',            'upserted per socket: no order-free merge rule written'),
        ('socket_part_type_wear',  'upserted per socket and part type: no order-free merge rule written'),
        ('consumable_counts',      'counts reset when a consumable is replaced, so GREATEST is wrong and no per-device order key exists'),
        ('module_rotation_advice', 'outcome changes after the advice is first uploaded (PENDING to APPLIED), so append is wrong')
$$;

CREATE OR REPLACE FUNCTION np_stage_name(p_target TEXT, p_month DATE)
RETURNS TEXT LANGUAGE sql IMMUTABLE AS $$
    SELECT format('%I.%I', 'np_stage', p_target || '__' || to_char(p_month, 'YYYY_MM'))
$$;

CREATE OR REPLACE FUNCTION np_check_month(p_month DATE)
RETURNS VOID LANGUAGE plpgsql IMMUTABLE AS $$
BEGIN
    IF p_month IS NULL OR p_month <> DATE_TRUNC('month', p_month)::date THEN
        RAISE EXCEPTION 'a batch month is the first of a month, got %', p_month;
    END IF;
END $$;

-- Creates the month's staging tables (idempotent). The ingest stages a record by
-- inserting into np_stage_name(<target>, <the current month>); the staging table
-- carries the target's NOT NULL and CHECK constraints, so a malformed record is
-- refused when it arrives, not when its month's batch is written.
CREATE OR REPLACE FUNCTION np_stage_open(p_month DATE)
RETURNS VOID LANGUAGE plpgsql AS $$
DECLARE
    t RECORD;
BEGIN
    PERFORM np_check_month(p_month);
    FOR t IN SELECT target FROM np_batch_tables() LOOP
        EXECUTE format(
            'CREATE UNLOGGED TABLE IF NOT EXISTS %s (LIKE public.%I INCLUDING DEFAULTS INCLUDING CONSTRAINTS)',
            np_stage_name(t.target, p_month), t.target);
        EXECUTE format('REVOKE ALL ON %s FROM PUBLIC', np_stage_name(t.target, p_month));
    END LOOP;
END $$;

-- The target's columns this writer copies: every column but the random key, which
-- the target draws afresh.
CREATE OR REPLACE FUNCTION np_batch_columns(p_target TEXT)
RETURNS TEXT[] LANGUAGE sql STABLE AS $$
    SELECT array_agg(column_name::text ORDER BY ordinal_position)
      FROM information_schema.columns
     WHERE table_schema = 'public' AND table_name = p_target AND column_name <> 'id'
$$;

-- Writes one closed month's batch. Run it as its own transaction (psql -1, or
-- BEGIN … COMMIT around the call): the batch is the transaction. All or nothing:
-- if any statement fails, nothing is written and the staging tables are kept for
-- a retry. Returns one row per target table.
CREATE OR REPLACE FUNCTION np_batch_flush(p_month DATE)
RETURNS TABLE (target TEXT, staged BIGINT, written BIGINT, unknown_device BIGINT)
LANGUAGE plpgsql AS $$
DECLARE
    t        RECORD;
    stage    TEXT;
    cols     TEXT[];
    sel      TEXT;
    n_staged BIGINT;
    n_known  BIGINT;
    n_out    BIGINT;
BEGIN
    PERFORM np_check_month(p_month);
    -- A batch is a whole calendar month: an open month is never written, so no
    -- batch boundary falls inside a month (see the header).
    IF p_month >= DATE_TRUNC('month', NOW())::date THEN
        RAISE EXCEPTION 'month % has not closed; a batch is written only after its month ends', p_month;
    END IF;
    IF to_regclass(np_stage_name('devices', p_month)) IS NULL THEN
        RAISE EXCEPTION 'no staging for month % (never opened, or already written)', p_month;
    END IF;

    FOR t IN SELECT b.target, b.kind FROM np_batch_tables() b ORDER BY b.write_order, b.target LOOP
        stage := np_stage_name(t.target, p_month);
        EXECUTE format('SELECT count(*) FROM %s', stage) INTO n_staged;

        IF t.target = 'devices' THEN
            -- A device that docked several times in the month: its counters are
            -- grow-only, so the month's value is the largest staged (GREATEST, as
            -- for module_life); device_transferred is sticky. The identity fields
            -- are set when the row is created and not changed by a later upload.
            -- Nothing here depends on which upload arrived first.
            EXECUTE format($q$
                INSERT INTO public.devices AS d
                    (warranty_token, device_model, hw_revision, manufacture_date, factory_id,
                     session_count, usb_c_insertion_count, supercap_cycle_count,
                     factory_reset_count, device_transferred, last_seen_month)
                SELECT warranty_token, min(device_model), min(hw_revision), min(manufacture_date),
                       min(factory_id), max(session_count), max(usb_c_insertion_count),
                       max(supercap_cycle_count), max(factory_reset_count),
                       bool_or(device_transferred), $1
                  FROM %s
                 GROUP BY warranty_token
                 ORDER BY random()
                ON CONFLICT (warranty_token) DO UPDATE SET
                    session_count         = GREATEST(d.session_count, EXCLUDED.session_count),
                    usb_c_insertion_count = GREATEST(d.usb_c_insertion_count, EXCLUDED.usb_c_insertion_count),
                    supercap_cycle_count  = GREATEST(d.supercap_cycle_count, EXCLUDED.supercap_cycle_count),
                    factory_reset_count   = GREATEST(d.factory_reset_count, EXCLUDED.factory_reset_count),
                    device_transferred    = d.device_transferred OR EXCLUDED.device_transferred,
                    last_seen_month       = GREATEST(d.last_seen_month, EXCLUDED.last_seen_month)
            $q$, stage) USING p_month;
            GET DIAGNOSTICS n_out = ROW_COUNT;
            n_known := n_staged;

        ELSIF t.target = 'module_life' THEN
            -- The schema's merge rule (GREATEST on the grow-only accumulators),
            -- applied inside the batch as well as against the stored row. The
            -- derived fields (trends, flags, bucket) come from the newest staged
            -- row by the module's own counters; on an exact tie the more
            -- conservative value wins (a set flag, a lower life bucket), so the
            -- result never depends on arrival order. Against the stored row they
            -- are replaced only by an upload at least as new.
            EXECUTE format($q$
                WITH known AS (
                    SELECT s.* FROM %1$s s
                     WHERE EXISTS (SELECT 1 FROM public.devices d WHERE d.warranty_token = s.warranty_token)
                ),
                newest AS (
                    SELECT DISTINCT ON (warranty_token, module_uid) *
                      FROM known
                     ORDER BY warranty_token, module_uid,
                              module_session_count DESC, module_mate_cycles_observed DESC,
                              distinct_socket_count DESC,
                              retirement_flag DESC, output_derate_flag DESC, mate_cycle_limit_flag DESC,
                              remaining_life_bucket ASC NULLS LAST,
                              pd_ratio_trend_per_session ASC NULLS LAST,
                              peak_ntc_trend_per_session DESC NULLS LAST,
                              module_part_type, module_hw_revision NULLS LAST, module_manufacture_date
                ),
                peak AS (
                    SELECT warranty_token, module_uid,
                           max(module_session_count) AS msc,
                           max(module_mate_cycles_observed) AS mmc,
                           max(distinct_socket_count) AS dsc
                      FROM known GROUP BY warranty_token, module_uid
                )
                INSERT INTO public.module_life AS m
                    (warranty_token, module_uid, module_part_type, module_hw_revision,
                     module_manufacture_date, module_session_count, module_mate_cycles_observed,
                     distinct_socket_count, pd_ratio_trend_per_session, peak_ntc_trend_per_session,
                     output_derate_flag, mate_cycle_limit_flag, retirement_flag,
                     remaining_life_bucket, last_seen_month)
                SELECT n.warranty_token, n.module_uid, n.module_part_type, n.module_hw_revision,
                       n.module_manufacture_date, p.msc, p.mmc, p.dsc,
                       n.pd_ratio_trend_per_session, n.peak_ntc_trend_per_session,
                       n.output_derate_flag, n.mate_cycle_limit_flag, n.retirement_flag,
                       n.remaining_life_bucket, $1
                  FROM newest n JOIN peak p USING (warranty_token, module_uid)
                 ORDER BY random()
                ON CONFLICT (warranty_token, module_uid) DO UPDATE SET
                    module_session_count        = GREATEST(m.module_session_count, EXCLUDED.module_session_count),
                    module_mate_cycles_observed = GREATEST(m.module_mate_cycles_observed, EXCLUDED.module_mate_cycles_observed),
                    distinct_socket_count       = GREATEST(m.distinct_socket_count, EXCLUDED.distinct_socket_count),
                    pd_ratio_trend_per_session  = CASE WHEN EXCLUDED.module_session_count >= m.module_session_count
                                                       THEN EXCLUDED.pd_ratio_trend_per_session ELSE m.pd_ratio_trend_per_session END,
                    peak_ntc_trend_per_session  = CASE WHEN EXCLUDED.module_session_count >= m.module_session_count
                                                       THEN EXCLUDED.peak_ntc_trend_per_session ELSE m.peak_ntc_trend_per_session END,
                    output_derate_flag          = CASE WHEN EXCLUDED.module_session_count >= m.module_session_count
                                                       THEN EXCLUDED.output_derate_flag ELSE m.output_derate_flag END,
                    mate_cycle_limit_flag       = CASE WHEN EXCLUDED.module_session_count >= m.module_session_count
                                                       THEN EXCLUDED.mate_cycle_limit_flag ELSE m.mate_cycle_limit_flag END,
                    retirement_flag             = CASE WHEN EXCLUDED.module_session_count >= m.module_session_count
                                                       THEN EXCLUDED.retirement_flag ELSE m.retirement_flag END,
                    remaining_life_bucket       = CASE WHEN EXCLUDED.module_session_count >= m.module_session_count
                                                       THEN EXCLUDED.remaining_life_bucket ELSE m.remaining_life_bucket END,
                    module_hw_revision          = COALESCE(m.module_hw_revision, EXCLUDED.module_hw_revision),
                    last_seen_month             = GREATEST(m.last_seen_month, EXCLUDED.last_seen_month)
            $q$, stage) USING p_month;
            GET DIAGNOSTICS n_out = ROW_COUNT;
            EXECUTE format(
                'SELECT count(*) FROM %s s WHERE EXISTS (SELECT 1 FROM public.devices d WHERE d.warranty_token = s.warranty_token)',
                stage) INTO n_known;

        ELSE
            cols := np_batch_columns(t.target);
            -- ingest_month is the batch's month whatever the staged row says.
            SELECT string_agg(CASE WHEN c = 'ingest_month' THEN '$1' ELSE format('s.%I', c) END, ', ')
              INTO sel FROM unnest(cols) AS c;
            EXECUTE format($q$
                INSERT INTO public.%I (%s)
                SELECT %s FROM %s s
                 WHERE EXISTS (SELECT 1 FROM public.devices d WHERE d.warranty_token = s.warranty_token)
                 ORDER BY random()
                ON CONFLICT DO NOTHING
            $q$, t.target,
                 (SELECT string_agg(format('%I', c), ', ') FROM unnest(cols) AS c),
                 sel, stage) USING p_month;
            GET DIAGNOSTICS n_out = ROW_COUNT;
            EXECUTE format(
                'SELECT count(*) FROM %s s WHERE EXISTS (SELECT 1 FROM public.devices d WHERE d.warranty_token = s.warranty_token)',
                stage) INTO n_known;
        END IF;

        target := t.target; staged := n_staged; written := n_out; unknown_device := n_staged - n_known;
        RETURN NEXT;
    END LOOP;

    -- The staged records end with the transaction that wrote them.
    FOR t IN SELECT b.target FROM np_batch_tables() b LOOP
        EXECUTE format('DROP TABLE %s', np_stage_name(t.target, p_month));
    END LOOP;
END $$;
