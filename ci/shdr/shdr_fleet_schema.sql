-- NeurOne SHDR Fleet Database Schema
-- Document: NP-FW-EMMC-002 Rev 2 §G.4, §H.2 / NP-FW-EMMC-001 Rev 1 §7
-- Revision: E
-- Effective date: 2026-08-12
-- Status: ACTIVE — BLOCKING for schema freeze (OI-EMMC2-07 must PASS)
--
-- (Schema revisions keep LETTERS. NP-CONV-001 §4.2: the integer-revision rule
--  binds document revisions; a database schema revision is an on-disk format
--  identity and renaming it would invalidate the migration record.)
--
-- Rev E (2026-08-12): ACCELEROMETER CHARACTERISATION WINDOW. Adds ONE table,
--   shdr_accel_characterisation, and changes nothing else. §G's prohibition
--   stands unaltered for every device that has not opted in — which is the whole
--   fleet by default, since the firmware build gate ships off.
--
--   WHY. shdr_accel_records carries two booleans derived from two numbers
--   (15.0 g; 3 drops per rolling window) that were guessed before any hardware
--   existed, and §G.3 bans every field that could validate them. The spec
--   forecloses its own evidence. Principal decision 2026-08-12: admit the
--   evidence under a time-boxed, consented, purpose-bound programme rather than
--   ship thresholds nobody can check. Pattern taken from NP-MOD-ID-001 §7.
--
--   The new table's own comment block carries the four structural markers, the
--   record-denominated window, the consent argument and the deletion-at-close
--   rule. NO MIGRATION DML: as Rev D recorded, no SHDR records exist anywhere in
--   the programme, so the schema is still free to re-layout at zero cost.
--
--   Also recorded, not fixed: OI-EMMC2-11 — shdr_accel_records' per-gap rows
--   already reconstruct both quantities §G.3 bans by name, as aggregates. See
--   that table's comment.
--
-- Rev D (2026-08-10): SOCKET+MODULE RE-KEY. Retires the fixed five-zone model.
--   Two tables are REPLACED, not edited:
--     * pbm_zone_telemetry  (zone_id SMALLINT CHECK BETWEEN 1 AND 5)
--         → pbm_module_telemetry, keyed (socket_number, module_uid).
--     * thermal_profiles    (zone1..zone5_peak_ntc_celsius fixed columns)
--         → module_thermal_telemetry (per socket+module row)
--         + hub_thermal_telemetry    (the device-level hub NTC, which has no
--           socket and no module and was conflated into the same row).
--   NO MIGRATION DML ACCOMPANIES THIS REVISION, and that is a decision, not an
--   omission. Rev C rows carry no module identity, so they are not convertible
--   to the new key even in principle — but the reason none is needed is that
--   NO SHDR RECORDS EXIST ANYWHERE IN THE PROGRAMME as of 2026-08-10 (no
--   hardware, no fleet, no field data). The schema is therefore still free to
--   re-layout at zero cost. That window is time-boxed: it closes the moment the
--   first device ships, after which any further re-key needs real migration.
--
--   Six tables are NEW:
--     * module_inventory        — current occupancy per (device, socket)
--     * module_placement_events — insertion/removal events + mate-cycle wear
--     * module_life             — per-PHYSICAL-MODULE lifetime accumulators
--     * socket_life             — per-SOCKET condition, independent of occupancy
--     * socket_part_type_wear   — socket wear ATTRIBUTED BY MODULE KIND
--     * module_rotation_advice  — wear-levelling recommendations + outcomes
--
--   THE CONNECTOR HAS TWO HALVES, AND THEY WEAR SEPARATELY. Module-side wear
--   leaves with the module; socket-side wear stays with the helmet and is
--   CUMULATIVE ACROSS EVERY MODULE EVER INSERTED THERE. Neither is recoverable
--   from the other: six module_life rows do not tell you the socket that hosted
--   all six is worn, and an empty socket still has its full history. Hence
--   socket_life is keyed on the socket alone, holds no module_uid, and is not
--   derived from occupancy. socket_part_type_wear splits that count by module
--   KIND, because whether some kinds (e.g. the ATtiny402-bearing 1064 smart
--   module vs a passive base tile — different pin loading, insertion force and
--   contact thermal cycling on the same 20-pin FPC) degrade sockets faster is
--   an open empirical question that a single summed counter has already thrown
--   the attribution away for. Splitting it is what makes the question askable
--   fleet-wide, and a kind that indicts itself changes both the rotation policy
--   and the connector qualification (OI-HEXTILE-11).
--
--   Note the loop this closes: rotation is the REMEDY for module wear and the
--   CAUSE of socket wear, so a socket can reach end of life before any module
--   in it does. socket_life.rotation_eligible_flag is what lets the optimiser
--   stop using a near-rating socket as a destination while it still works fine
--   in place.
--
--   WHY. The shipped hardware is an 80-socket hexagonal lattice of TYPE-AGNOSTIC
--   snap-in modules (NP-HEX-ZM-001 §3.4) discovered by UID auto-inventory
--   (np_module_map). zone_id conflated three things that move independently — a
--   physical position, a replaceable part, and an authored zone — so it was the
--   wrong key for all three. Modules are user-movable, so wear belongs to the
--   PART: a module that changed socket previously abandoned its degradation
--   history and inherited a stranger's. Nothing counted connector mating cycles
--   at all. And no table could express which module should move where, so the
--   §5.2 predictive-maintenance models had no wear-levelling surface to learn on.
--
--   IDENTITY IS MODALITY-AGNOSTIC. Every module type carries a UID, not just PBM
--   tiles: any type inserts in any socket (np_module_map.h), so an EEG/tES tile
--   loses its history on a move for exactly the same reason a PBM tile does.
--   module_inventory / module_placement_events / module_life are therefore above
--   the modality layer; pbm_module_telemetry is the PBM-specific optical layer
--   on top of it.
--
--   PRECEDENT, not a new direction: OI-HUB-C06 already requires PBM calibration
--   coefficients (K_PD1 / K_PD2 / K_ratio_nom) to be keyed to module UID via
--   np_module_map "because modules are swappable", and NP-SES-1064-001 Rev 2
--   (2026-08-02) already retired the fixed zone[5] arrays on the firmware side
--   in favour of per-socket entries (np_pbm_shdr_summary_t). Rev D is the
--   fleet-DB completion of a move the firmware has already made.
--
--   NEW PRIVACY INVARIANT (DOSE-01, CI-enforced). Socket resolution rises 5 → 80.
--   zone_id was already positional, so this is a resolution increase inside an
--   existing category — but at 80-socket granularity a per-socket DOSE or DUTY
--   figure would reconstruct the user's treatment geography, and treatment
--   geography implies indication. Per-zone PBM dose is UHDR (§5.1). SHDR may
--   record WHERE a module sits and HOW DEGRADED it is; never HOW MUCH ENERGY
--   was delivered there. Duty-based rotation optimisation runs on-device against
--   UHDR; only the advice and its outcome reach SHDR.
--
--   ⚠⚠ OPEN ITEM OI-EMMC2-08 — BLOCKING. Principal sign-off required before
--   schema freeze. DO NOT treat Rev D's choice as settled.
--
--   A persistent 64-bit hardware UID per socket DEFEATS THE FACTORY-RESET
--   DE-LINKING MECHANISM this schema is built on. devices.device_transferred
--   (see that column) documents the intent: factory reset clears the warranty
--   token, and the device becomes a new pseudonymous identity with no path back
--   to the old one. But after a reset the device re-uploads module_inventory
--   carrying the same ~80 module UIDs. Joining the old token's inventory to the
--   new token's on module_uid re-links them with ~80-way corroboration. The
--   same join links a clinic's devices to each other, and a resold device to
--   its previous owner's token. NEITHER CI GATE CAN SEE THIS: both are name-and-
--   type matchers, and the no-join gate guards SHDR↔warranty joins while saying
--   nothing about SHDR↔SHDR self-joins across tokens.
--
--   Mitigations applied in Rev D: module_life is keyed (warranty_token,
--   module_uid) so no row spans devices; the linkage is device↔device, never
--   device↔person, since warranty_token→person still requires the consented
--   warranty_db the no-join rule blocks.
--
--   THE CONSERVATIVE ALTERNATIVE, costed and NOT taken here: store a per-device
--   derived handle — module_ref = HMAC(device-local key, uid) — stable within a
--   device, incomparable across devices. That closes the re-linking channel
--   completely. What it costs is exactly the capability that motivated Rev D:
--   a module's history could no longer follow it between devices, so clinic-pool
--   modules would restart their life record on every transfer. Rev D takes the
--   raw UID because per-part predictive maintenance is the stated requirement,
--   and records the cost here in full rather than burying it. If the principal
--   judges the re-linking channel unacceptable, module_ref is a drop-in column
--   swap and the schema is otherwise unchanged.
--
--   Related, same class, also unenforceable in SQL: mate-cycle counters are
--   cumulative physical-handling counts, structurally the same object as the
--   drop_count this file bans by name (see PROHIBITED patterns and the
--   shdr_accel_records note, which collapsed handling data to two booleans for
--   precisely this reason). Connector mate cycles genuinely are device-condition
--   data, but the placement-event RATE distinguishes a clinic swapping modules
--   between patients from a home user who never touches them. The precedent in
--   this very file cuts the other way; this is a judgment made explicitly, not
--   inherited.
--
-- Rev C (2026-07-14): freeze-prep integrity fix. consumable_counts documents a
--   "one upserted row per device" semantic but had no uniqueness enforcement on
--   warranty_token (PK was the BIGSERIAL id; the index was non-unique). Added a
--   UNIQUE (warranty_token) constraint so ON CONFLICT upsert has a valid arbiter
--   and duplicate per-device rows cannot accumulate. Removed the now-redundant
--   idx_consumable_token (the UNIQUE constraint provides the lookup index).
--   mode_f_telemetry was reviewed in the same pass and intentionally LEFT
--   append-per-upload (audit trail for predictive maintenance) — no UNIQUE
--   constraint; its BIGSERIAL id PK is correct and warranty_token stays
--   non-unique. No privacy-surface change; both CI gates (OI-EMMC2-07 schema,
--   OI-EMMC2-06 no-join) remain PASS.
--
-- Rev B (2026-07-13): timestamp minimization. Per the directive "use the largest
--   granularity that maintains needed functionality; prefer a counter/boolean;
--   remove the timestamp if no functionality needs it," every per-row
--   TIMESTAMPTZ wall clock has been removed:
--     * Per-session/event tables now carry `ingest_month DATE` (month-granular
--       retention anchor only). Row ordering was already provided by an integer
--       counter (session_index / gap_index / *_at_* / *_count), so no clock is
--       needed for ordering.
--     * `devices.updated_at` → `last_seen_month DATE` (liveness + retention).
--     * `devices.created_at` removed (device age = manufacture_date; activation
--       date lives only in the consented warranty_db, which SHDR cannot join).
--     * `consumable_counts.updated_at` removed (reminders are count-triggered;
--       one upserted row per device — no functionality needed the timestamp).
--   This removes a per-row docking-time correlator keyed to warranty_token and
--   brings the schema into line with the §5.1 "no timestamps in SHDR" rule.
--
-- VERIFICATION. Two gates cover this file, and they answer different questions:
--   * ci/test_shdr_schema.py — STATIC. Reads the DDL as text and enforces the
--     privacy invariants (no PII, no wall clock, no dose columns, correct
--     socket/UID declarations). It cannot tell whether this file is valid SQL,
--     nor whether a CHECK actually fires — a constraint with a typo'd predicate
--     parses fine and passes every name-and-type check while silently admitting
--     the rows it was written to block.
--   * ci/run_shdr_constraint_tests.sh — LIVE. Executes this schema against a
--     real PostgreSQL and then runs ci/shdr/shdr_schema_constraint_tests.sql,
--     which proves each constraint rejects what it exists to reject (28
--     reject-cases) and still accepts the valid form (20 accept-cases, so a
--     uniformly hostile constraint cannot pass by refusing everything).
--   Both are wired into .github/workflows/shdr-schema-ci.yml.
--
-- PRIVACY RULES enforced by CI test (ci/test_shdr_schema.py, OI-EMMC2-07):
--   1. No raw accelerometer columns (g-force, orientation, drop_count, timestamps)
--   2. No personal-data columns (name, email, address, phone, postal)
--   3. No-join rule: this schema must not contain foreign keys to the warranty
--      registration database. Warranty linkage is via opaque warranty_token only.
--      The warrant_db and shdr_db live in separate cloud projects with no shared
--      IAM roles. A separate CI test (OI-EMMC2-06) confirms the join fails.
--   4. No sub-day timestamps (TIME-01). SHDR carries no wall clock finer than a
--      month-granular DATE retention anchor (ingest_month / last_seen_month).
--      Per-row ordering is an integer counter, never a clock. Enforced by CI:
--      no TIMESTAMP / TIMESTAMPTZ / TIME columns anywhere in the schema.
--
-- PERMITTED accelerometer columns per NP-FW-EMMC-002 §G.2:
--   drop_detected BOOLEAN, maintenance_alert BOOLEAN  — and nothing else.
--
--   ONE EXCEPTION, Rev E: the shdr_accel_characterisation table, under
--   NP-FW-EMMC-002 §H. It is not a widening of the permitted set — the permitted
--   set is unchanged and still binds everywhere else. It is a separate table
--   whose extended columns are admitted by CHAR-01 only while all four of its
--   structural markers hold AND the firmware build gate is open, and the
--   ACCEL-01/ACCEL-02 exemption is COMPUTED from that check rather than declared
--   beside it, so it lapses the moment any condition fails.
--
-- WHAT THE STATIC GATE DOES AND DOES NOT PROVE. A PASS from ci/test_shdr_schema.py
-- asserts "no column is named or typed like a known leak". It does NOT assert
-- "SHDR is not health-inferrable" — no check here reasons about what a query
-- over accumulated rows can recover, which is exactly how OI-EMMC2-11 went
-- unnoticed. Read the PASS for what it is.
--
-- Column naming conventions:
--   All column names are snake_case.
--   Prohibited patterns checked by CI: g_force, accel_*, orientation_*,
--   drop_count, drop_timestamp, impact_*, fall_*, name, email, address,
--   phone, postal.

-- ---------------------------------------------------------------------------
-- Table: devices
-- One row per unique device (keyed by warranty_token only — never by owner ID).
-- ---------------------------------------------------------------------------
CREATE TABLE devices (
    -- Primary key: opaque 256-bit TRNG token generated on first device activation.
    -- Never derived from any user-identifying input. Sole linkage to warranty_db.
    warranty_token          BYTEA        NOT NULL PRIMARY KEY,  -- 32 bytes

    -- Hardware identity
    device_model            VARCHAR(32)  NOT NULL,   -- 'NP-T1-HOME-STD', etc.
    hw_revision             VARCHAR(16)  NOT NULL,   -- PCB revision string

    -- Manufacture record
    manufacture_date        DATE         NOT NULL,
    factory_id              VARCHAR(16)  NOT NULL,

    -- Lifecycle counters (no timestamps — session count is unsigned integer only)
    session_count           INTEGER      NOT NULL DEFAULT 0 CHECK (session_count >= 0),
    usb_c_insertion_count   INTEGER      NOT NULL DEFAULT 0 CHECK (usb_c_insertion_count >= 0),
    supercap_cycle_count    INTEGER      NOT NULL DEFAULT 0 CHECK (supercap_cycle_count >= 0),
    factory_reset_count     SMALLINT     NOT NULL DEFAULT 0,

    -- Transfer flag: set when factory reset clears the warranty token
    device_transferred      BOOLEAN      NOT NULL DEFAULT FALSE,

    -- Liveness + retention anchor at MONTH granularity — largest granularity that
    -- still supports active-fleet counting and TTL. No first-seen timestamp:
    -- device age comes from manufacture_date; activation date lives only in the
    -- consented warranty_db (no-join). Replaces created_at + updated_at.
    last_seen_month         DATE         NOT NULL DEFAULT DATE_TRUNC('month', NOW())::date
);

-- ---------------------------------------------------------------------------
-- Table: firmware_history
-- One row per firmware version installed on a device.
-- ---------------------------------------------------------------------------
CREATE TABLE firmware_history (
    id                  BIGSERIAL    PRIMARY KEY,
    warranty_token      BYTEA        NOT NULL REFERENCES devices(warranty_token),

    fw_version          VARCHAR(32)  NOT NULL,
    safety_mcu_version  VARCHAR(32)  NOT NULL,
    install_method      VARCHAR(16)  NOT NULL CHECK (install_method IN ('OTA', 'DFU', 'FACTORY')),
    signature_verified  BOOLEAN      NOT NULL DEFAULT TRUE,

    -- Session counter at install time — provides ordering, no wall clock
    session_at_install  INTEGER      NOT NULL CHECK (session_at_install >= 0),

    -- Month-granular retention anchor only (ordering via session_at_install)
    ingest_month        DATE         NOT NULL DEFAULT DATE_TRUNC('month', NOW())::date
);

-- ---------------------------------------------------------------------------
-- Table: ota_events
-- One row per OTA attempt (success or failure).
-- ---------------------------------------------------------------------------
CREATE TABLE ota_events (
    id                  BIGSERIAL    PRIMARY KEY,
    warranty_token      BYTEA        NOT NULL REFERENCES devices(warranty_token),

    from_version        VARCHAR(32)  NOT NULL,
    to_version          VARCHAR(32)  NOT NULL,
    outcome             VARCHAR(16)  NOT NULL CHECK (outcome IN ('SUCCESS', 'ROLLBACK', 'ABORTED', 'FAILED')),
    failure_reason      VARCHAR(64),

    session_at_attempt  INTEGER      NOT NULL CHECK (session_at_attempt >= 0),
    -- Month-granular retention anchor only (ordering via session_at_attempt)
    ingest_month        DATE         NOT NULL DEFAULT DATE_TRUNC('month', NOW())::date
);

-- ===========================================================================
-- MODULE IDENTITY LAYER (Rev D)
--
-- Three tables, all MODALITY-AGNOSTIC. Every snap-in module carries a unique
-- 64-bit UID regardless of type — PBM tiles, EEG/tES electrode tiles, NTC-only
-- tiles, everything. This is a hardware property, not a PBM one: tiles are
-- type-agnostic (any type inserts in any socket, np_module_map.h), so identity
-- must live above the modality layer or a moved electrode tile loses its
-- history exactly the way a moved PBM tile used to.
--
-- The two-level address is (socket_number : element), per NP-HEX-ZM-001 §3 and
-- firmware/hub_control/include/np_module_map.h:
--
--   socket_number — WHERE. A physical helmet location. Changes only when the
--                   inner bowl is re-tooled. 1-BASED (NUMBER-1): socket 0 does
--                   not exist and is rejected, never clamped. Domain 1..128
--                   (the full 7-bit major field); 80 sockets shipped in v1.
--                   NOTE: firmware's np_hex_addr_t.socket_id is 0-based INDEX
--                   space and converts at its boundary. SHDR stores the NUMBER.
--   module_uid    — WHICH. The 8-byte (NP_HEXMAP_UID_LEN) module UID reported
--                   at power-on inventory poll. A COMPONENT identifier, SHDR
--                   class by the same reasoning as accessory authentication
--                   (np_module_map.h "Privacy" note): it describes a part, not
--                   a person.
--
-- ── Why this replaces zone_id (the Rev C model) ────────────────────────────
-- `zone_id SMALLINT CHECK (zone_id BETWEEN 1 AND 5)` conflated three separate
-- things: a physical position, a replaceable part, and an authored zone. All
-- three moved independently, so the key was wrong for all three:
--   * Degradation belongs to the PART. A module that moves sockets took none of
--     its history with it and inherited another module's.
--   * Mate-cycle wear belongs to the PART and the SOCKET, and nothing counted it.
--   * Zones are authored socket SETS in protocols/predefined/00-zones.npps and
--     are re-cut without any hardware change (ZONE-1), which silently re-pointed
--     historical rows. The schema therefore models NO zone concept at all.
--
-- ── Cross-device linkage: a deliberate, flagged consequence (OI-EMMC2-NEW) ──
-- module_life is keyed on module_uid ALONE and holds no warranty_token. That is
-- the privacy-preserving half of the choice: the part's lifetime accumulators
-- carry no device link. But placement rows do, and a module UID that appears
-- under two warranty_tokens DOES link those two devices — something zone_id
-- could not do. This is the price of per-part predictive maintenance when parts
-- move between devices (clinic pools are the expected case), and it is recorded
-- here rather than buried: it links DEVICES, not people; warranty_token→person
-- still requires the consented warranty_db, which the no-join rule (§3) blocks.
-- Flagged for principal sign-off before schema freeze — not silently adopted.
--
-- ── New privacy invariant enforced by CI (DOSE-01) ─────────────────────────
-- Socket resolution rises 5 → 80. `zone_id` was already positional, so this is
-- a resolution increase inside an existing category, not a new one — but at
-- 80-socket granularity a per-socket DOSE or DUTY figure would reconstruct the
-- user's treatment geography, and treatment geography implies indication.
-- Per-zone PBM dose (J/cm²) is UHDR (CLAUDE.md §5.1). So: SHDR may record WHERE
-- a module sits and HOW DEGRADED it is; it may never record HOW MUCH ENERGY was
-- delivered there. CI check DOSE-01 rejects any dose / energy / irradiance /
-- on-time / duty column anywhere in this schema.
-- ===========================================================================

-- ---------------------------------------------------------------------------
-- Table: module_inventory
-- Current OCCUPANCY of each socket, and nothing else. One UPSERTED row per
-- (device, socket) — the "what is plugged in where right now" snapshot, so
-- callers never have to replay the placement event log. Mirrors the NVRAM
-- inventory np_module_map keeps.
--
-- Socket CONDITION is deliberately not here: see socket_life. Occupancy and
-- condition have different lifetimes — a socket that is currently empty still
-- has a wear history, and this row's module_uid is NULL in exactly that case.
-- ---------------------------------------------------------------------------
CREATE TABLE module_inventory (
    id                      BIGSERIAL    PRIMARY KEY,
    warranty_token          BYTEA        NOT NULL REFERENCES devices(warranty_token),

    -- 1-BASED socket number (NUMBER-1). Lower bound is 1, never 0.
    socket_number           SMALLINT     NOT NULL CHECK (socket_number BETWEEN 1 AND 128),

    -- Current occupant. NULL = socket is empty (distinct from the all-zero UID
    -- the hub reports for an empty socket on the wire; normalised to NULL here).
    module_uid              BYTEA        CHECK (module_uid IS NULL OR octet_length(module_uid) = 8),
    module_part_type        VARCHAR(24),  -- 'T1-A-PBM-BASE', 'ZM-1064-SMART', 'T1-B-DUAL-ELEC'
    module_fw_version       VARCHAR(32),  -- on-module ATtiny402 image, smart modules only

    -- Monotonic per-device placement generation; increments on every accepted
    -- inventory change. Provides ordering with no wall clock.
    placement_index         INTEGER      NOT NULL DEFAULT 0 CHECK (placement_index >= 0),

    last_seen_month         DATE         NOT NULL DEFAULT DATE_TRUNC('month', NOW())::date,

    CONSTRAINT ck_module_inventory_month
        CHECK (last_seen_month = DATE_TRUNC('month', last_seen_month)::date),
    -- The all-zero UID is the empty-socket sentinel; empty is expressed as NULL.
    CONSTRAINT ck_module_inventory_uid_nonzero
        CHECK (module_uid IS NULL OR module_uid NOT IN ('\x0000000000000000'::bytea, '\xFFFFFFFFFFFFFFFF'::bytea)),

    -- One row per socket per device — the arbiter for ON CONFLICT upsert.
    -- (This is the consumable_counts lesson from Rev C, at 80× the row count:
    -- a documented "one upserted row per X" with no uniqueness has no valid
    -- ON CONFLICT arbiter and quietly accumulates duplicates.)
    CONSTRAINT uq_module_inventory_socket UNIQUE (warranty_token, socket_number)
);

-- A module occupies at most ONE socket at a time. Without this, a partially
-- applied upsert leaves a ghost occupant in the old socket, the module appears
-- in two rows, and every per-module rollup double-counts its sessions.
-- Partial: NULL module_uid means "empty socket", and many sockets may be empty.
CREATE UNIQUE INDEX uq_module_inventory_one_socket_per_module
    ON module_inventory (warranty_token, module_uid) WHERE module_uid IS NOT NULL;

-- ---------------------------------------------------------------------------
-- Table: socket_life
-- Per-SOCKET condition, tracked INDEPENDENTLY of what is plugged into it.
-- One UPSERTED row per (device, socket). UNIQUE (warranty_token, socket_number).
--
-- The connector has two halves and they wear independently, on different
-- schedules and against different owners:
--   * The MODULE half leaves with the module (module_life.module_mate_cycles).
--   * The SOCKET half stays with the helmet, and its wear is CUMULATIVE ACROSS
--     EVERY MODULE EVER INSERTED THERE. A socket that has hosted six different
--     modules has six modules' worth of insertions on its springs, and none of
--     the six module_life rows records that. A socket that is empty right now
--     still has its whole history.
--
-- Consequence for wear-levelling: a socket can reach end of life before any
-- module in it does — and rotation, which is the remedy for module wear, is the
-- CAUSE of socket wear. The optimiser must be able to see a socket that has
-- become the expensive one to keep cycling. Rating for the hex-tile pogo socket
-- is ≥500 mating cycles (NP-HW-HEXTILE-001 contact spec; qualification is
-- OI-HEXTILE-11 — ≤50 mΩ over ≥500 cycles on the EEG signal path, pin 13). A
-- fleet-wide cumulative counter is the natural field evidence for that OI.
-- ---------------------------------------------------------------------------
CREATE TABLE socket_life (
    id                      BIGSERIAL    PRIMARY KEY,
    warranty_token          BYTEA        NOT NULL REFERENCES devices(warranty_token),

    socket_number           SMALLINT     NOT NULL CHECK (socket_number BETWEEN 1 AND 128),

    -- FALSE = this socket is not wired in this shell variant
    -- (np_socket_geom_t.present_in_helmet).
    socket_present_in_shell BOOLEAN      NOT NULL DEFAULT TRUE,

    -- Cumulative across ALL modules and ALL module kinds ever inserted here.
    -- This is the counter compared against the ≥500-cycle rating — and it is an
    -- _observed LOWER BOUND for the same reason the module-side one is: a
    -- same-socket reseat while powered off produces no UID diff and so no event.
    socket_mate_cycles_observed INTEGER  NOT NULL DEFAULT 0 CHECK (socket_mate_cycles_observed >= 0),

    -- How many DISTINCT modules this socket has hosted. High churn with low
    -- cycles-per-module is the rotation signature; it distinguishes a socket
    -- worn by wear-levelling from one worn by repeated module failures.
    distinct_module_count   SMALLINT     NOT NULL DEFAULT 0 CHECK (distinct_module_count >= 0),

    -- DERIVED contact degradation only — raw contact resistance stays on-device,
    -- same boundary rule as EEG impedance (§5.1: raw → UHDR, trend slope → SHDR).
    contact_resistance_trend_per_cycle REAL,
    contact_degraded_flag   BOOLEAN      NOT NULL DEFAULT FALSE,

    -- Socket-side verdicts. A socket at its cycle rating must stop being a
    -- rotation DESTINATION even while it remains perfectly usable in place.
    mate_cycle_limit_flag   BOOLEAN      NOT NULL DEFAULT FALSE,
    rotation_eligible_flag  BOOLEAN      NOT NULL DEFAULT TRUE,
    socket_disabled_flag    BOOLEAN      NOT NULL DEFAULT FALSE,

    -- Coarse RUL bucket (0 = end of life … 4 = as-new), matching module_life.
    remaining_life_bucket   SMALLINT     CHECK (remaining_life_bucket BETWEEN 0 AND 4),

    last_seen_month         DATE         NOT NULL DEFAULT DATE_TRUNC('month', NOW())::date,

    CONSTRAINT ck_socket_life_month
        CHECK (last_seen_month = DATE_TRUNC('month', last_seen_month)::date),
    CONSTRAINT uq_socket_life_socket UNIQUE (warranty_token, socket_number)
);

-- ---------------------------------------------------------------------------
-- Table: socket_part_type_wear
-- Mate-cycle wear on a socket, ATTRIBUTED BY MODULE KIND.
-- One UPSERTED row per (device, socket, module_part_type).
--
-- WHY THIS IS A SEPARATE TABLE. Module kinds are not mechanically identical at
-- the connector: a 1064 nm smart module carries an on-module ATtiny402 and
-- three FET channels on the same 20-pin FPC as a passive base tile, so its
-- pin loading, insertion force and thermal cycling at the contact differ. It is
-- an open empirical question whether some kinds wear sockets faster — and it
-- cannot be answered from socket_life alone, because a single cumulative
-- counter has already summed the kinds together and thrown the attribution
-- away. Splitting the count by part type is what makes the question askable:
-- across the fleet, cycles-of-kind-X per unit of contact-resistance rise is
-- directly comparable between kinds, and a kind that indicts itself changes
-- both the rotation policy and the connector qualification (OI-HEXTILE-11).
--
-- Note this is COUNTS BY KIND, not a dwell or duty figure — see DOSE-01.
-- ---------------------------------------------------------------------------
CREATE TABLE socket_part_type_wear (
    id                      BIGSERIAL    PRIMARY KEY,
    warranty_token          BYTEA        NOT NULL REFERENCES devices(warranty_token),

    socket_number           SMALLINT     NOT NULL CHECK (socket_number BETWEEN 1 AND 128),
    module_part_type        VARCHAR(24)  NOT NULL,

    -- Insertions of THIS KIND of module into THIS socket.
    mate_cycles             INTEGER      NOT NULL DEFAULT 0 CHECK (mate_cycles >= 0),
    distinct_module_count   SMALLINT     NOT NULL DEFAULT 0 CHECK (distinct_module_count >= 0),

    -- Contact-resistance rise attributed to this kind's share of the cycles.
    -- Derived, per-cycle, and unitless-per-cycle — never a raw resistance value.
    contact_resistance_delta_per_cycle REAL,

    -- Number of sessions run while a module of this kind occupied this socket.
    -- A COUNT, deliberately: a duration or on-time here would be a duty figure
    -- and is prohibited by DOSE-01.
    occupancy_session_count INTEGER      NOT NULL DEFAULT 0 CHECK (occupancy_session_count >= 0),

    last_seen_month         DATE         NOT NULL DEFAULT DATE_TRUNC('month', NOW())::date,

    CONSTRAINT ck_socket_part_wear_month
        CHECK (last_seen_month = DATE_TRUNC('month', last_seen_month)::date),
    CONSTRAINT uq_socket_part_type_wear UNIQUE (warranty_token, socket_number, module_part_type)
);

-- ---------------------------------------------------------------------------
-- Table: module_placement_events
-- APPEND-ONLY. One row per detected insertion or removal.
--
-- HOW EVENTS ARE DETECTED (np_module_map_apply_poll): at every power-on the hub
-- polls every socket for its module UID. Only when a socket's reported UID does
-- NOT match the UID last stored for that socket has something changed — that
-- socket was vacated, filled, or swapped. The diff is the event source; there
-- is no per-boot row for unchanged sockets (that would be ~80 rows per boot and
-- a boot-frequency correlator for nothing).
--
-- WHY THIS TABLE EXISTS AT ALL: mating cycles are destructive. Every insertion
-- and removal wears the FPC connector on both halves. Rotation (see
-- module_rotation_advice) SPENDS mate cycles to buy LED life, so a rotation
-- policy that ignores connector wear will destroy connectors faster than it
-- saves emitters. Both stocks must be countable or the trade is unfalsifiable.
-- ---------------------------------------------------------------------------
CREATE TABLE module_placement_events (
    id                  BIGSERIAL    PRIMARY KEY,
    warranty_token      BYTEA        NOT NULL REFERENCES devices(warranty_token),

    socket_number       SMALLINT     NOT NULL CHECK (socket_number BETWEEN 1 AND 128),

    -- NULL on a REMOVED event where the socket is now empty.
    module_uid          BYTEA        CHECK (module_uid IS NULL OR octet_length(module_uid) = 8),
    module_part_type    VARCHAR(24),

    -- NO_RESPONSE is NOT a removal, and conflating the two was a real hazard.
    -- Rev C hub topology reaches modules through a cluster controller's PCA9548A
    -- fan-out, and np_module_map_apply_poll is fail-closed: an inventory failure
    -- leaves module_present == false, which is byte-identical to an empty
    -- socket. A single cluster-controller or mux fault therefore forges a burst
    -- of ~8-10 simultaneous "removals" — nulling a whole cluster's occupancy,
    -- inflating every affected module's mate cycles, and triggering rotation
    -- advice for hardware nobody touched. Recording NO_RESPONSE separately, plus
    -- the cluster-fault marker below, makes a burst recognisable as ONE fault
    -- rather than ten handling events.
    event_kind          VARCHAR(16)  NOT NULL CHECK (event_kind IN ('INSERTED', 'REMOVED', 'NO_RESPONSE')),

    -- TRUE when several sockets stopped answering in the same poll — the
    -- signature of a cluster-level fault rather than physical handling.
    -- Consumers must NOT accrue mate cycles from rows with this set.
    cluster_fault_suspected BOOLEAN   NOT NULL DEFAULT FALSE,

    -- BOOT_DIFF: inferred from the power-on inventory diff. The hub sees only
    --   the resulting state, so an unknown number of intermediate swaps may have
    --   occurred while powered down — mate-cycle counts from BOOT_DIFF are a
    --   LOWER BOUND, and models must treat them as such. Same-socket reseats are
    --   invisible entirely (see module_life.module_mate_cycles_observed).
    -- HOTPLUG:   observed live. Exact.
    detection           VARCHAR(12)  NOT NULL CHECK (detection IN ('BOOT_DIFF', 'HOTPLUG')),

    -- TRUE when the same UID was found at a different socket in the same
    -- reconciliation pass: the module moved within this device rather than
    -- leaving it. This is the signal a rotation was actually carried out.
    relocated_within_device BOOLEAN   NOT NULL DEFAULT FALSE,

    -- Ordering: monotonic integer counters, never a clock.
    power_cycle_index   INTEGER      NOT NULL CHECK (power_cycle_index >= 0),
    placement_index     INTEGER      NOT NULL CHECK (placement_index >= 0),

    -- Wear counters AS OF this event. Tracked on BOTH halves of the connector,
    -- because they wear independently and against different owners. Both are
    -- _observed lower bounds — see module_life.module_mate_cycles_observed.
    module_mate_cycles_observed INTEGER NOT NULL CHECK (module_mate_cycles_observed >= 0),
    socket_mate_cycles_observed INTEGER NOT NULL CHECK (socket_mate_cycles_observed >= 0),

    -- Contact degradation flag (derived; raw contact resistance stays on-device)
    contact_resistance_flag BOOLEAN  NOT NULL DEFAULT FALSE,

    -- Fail-closed inventory result: FALSE when the module could not be
    -- inventoried cleanly and was left unusable (np_module_map fail-closed path).
    inventory_ok        BOOLEAN      NOT NULL DEFAULT TRUE,

    ingest_month        DATE         NOT NULL DEFAULT DATE_TRUNC('month', NOW())::date,

    CONSTRAINT ck_placement_month CHECK (ingest_month = DATE_TRUNC('month', ingest_month)::date),
    CONSTRAINT ck_placement_uid_nonzero
        CHECK (module_uid IS NULL OR module_uid NOT IN ('\x0000000000000000'::bytea, '\xFFFFFFFFFFFFFFFF'::bytea)),

    -- IDEMPOTENT INGEST. This table is append-only with no natural key, so a
    -- retried or duplicated upload would append the same event again and
    -- double-count mate cycles — silently inflating the wear figure that the
    -- rotation policy and the ≥500-cycle rating are both read against.
    CONSTRAINT uq_placement_event UNIQUE (warranty_token, socket_number,
                                          power_cycle_index, event_kind)
);

-- ---------------------------------------------------------------------------
-- Table: module_life
-- Per-module lifetime accumulators, PARTITIONED BY REPORTING DEVICE.
-- UNIQUE (warranty_token, module_uid) — deliberately NOT UNIQUE (module_uid).
--
-- This is the table the whole redesign exists for. Predictive maintenance is
-- per-part: a module's remaining life is a function of what that module has
-- been through, which follows it across every socket it has occupied. Under
-- Rev C this was unrepresentable.
--
-- ⚠ WHY THIS IS NOT ONE ROW PER MODULE FLEET-WIDE — read before "fixing" it.
-- A single fleet-wide row per module UID is UNSATISFIABLE under TIME-01, and
-- the failure is silent. Every ingest path here is a per-device upload; a row
-- keyed on module_uid alone is therefore written by whichever device currently
-- holds the module. For a module that moves A → B → A, the competing upserts
-- have NO ORDERING RELATION:
--   * session_index and power_cycle_index are per-device counters and are not
--     comparable across devices;
--   * ingest_month is month-granular, so two writes in a month are unordered;
--   * the only thing that WOULD order them is a wall clock, which TIME-01
--     forbids for good reason.
-- Last-writer-wins then REGRESSES module_session_count and module_mate_cycles
-- whenever device A re-docks after device B has advanced them — and a lifetime
-- accumulator that can silently run backwards is worse than none, because
-- pd_ratio_trend_per_session and remaining_life_bucket are derived from it.
--
-- The partition removes the conflict instead of trying to order it. Each row is
-- the part's history WITH ONE DEVICE, written only by that device, so there is
-- exactly one writer per row and no cross-device race. True fleet-wide life is
--     SELECT module_uid, SUM(module_session_count), SUM(module_mate_cycles) …
--     FROM module_life GROUP BY module_uid
-- which is ADDITION — commutative, associative, and therefore correct without
-- any ordering at all. The lifetime figure is recovered as a query rather than
-- maintained as a racy row.
--
-- ⚠ THE MERGE RULE IS LOAD-BEARING — GREATEST, never "+=".
-- The partition makes each row single-writer; it does NOT by itself make ingest
-- idempotent. If the uploader applies a DELTA (partial = partial + delta), a
-- retried or duplicated upload double-counts, and avoiding that needs
-- exactly-once delivery — which is an ordering requirement, i.e. exactly the
-- thing the partition was adopted to escape. The device must therefore upload
-- its ABSOLUTE per-device total and ingest must merge with
--     ON CONFLICT (warranty_token, module_uid) DO UPDATE
--       SET module_session_count = GREATEST(module_life.module_session_count,
--                                           EXCLUDED.module_session_count), …
-- which is idempotent, commutative, and order-free. These accumulators form a
-- grow-only counter per device; fleet life is their SUM, and a sum of monotone
-- counters converges regardless of upload order, duplication, or delay.
-- Monotonicity is enforced below by trigger rather than left to ingest code: it
-- is a property of the artifact, and no behavioural test of a query result
-- would reveal its absence.
--
-- Cardinality: UPSERT-IN-PLACE, one row per (device, module) — contrast
-- module_placement_events, which is append-per-event. Do not add a BIGSERIAL-
-- keyed duplicate row per upload, and do not "simplify" the key to module_uid.
-- ---------------------------------------------------------------------------
CREATE TABLE module_life (
    id                      BIGSERIAL    PRIMARY KEY,
    warranty_token          BYTEA        NOT NULL REFERENCES devices(warranty_token),

    module_uid              BYTEA        NOT NULL CHECK (octet_length(module_uid) = 8),
    module_part_type        VARCHAR(24)  NOT NULL,
    module_hw_revision      VARCHAR(16),

    -- Calendar-life anchor. The stated goal of rotation is to maximise average
    -- module CALENDAR lifespan, which needs a birth date to measure against.
    -- DATE, not TIMESTAMP: a manufacture date is a part property, not a clock
    -- reading about the user, and carries no sub-day granularity.
    module_manufacture_date DATE         NOT NULL,

    -- Lifetime accumulators, additive across rows for the same module_uid.
    -- These survive every socket move — that is the point of keying on the UID.
    module_session_count    INTEGER      NOT NULL DEFAULT 0 CHECK (module_session_count >= 0),

    -- _observed, and the suffix is load-bearing: this is a LOWER BOUND, never a
    -- true count. Mate cycles are derived from the power-on UID diff, and
    -- np_module_map_apply_poll only reports a change when the reported UID
    -- DIFFERS from the stored one. A module pulled and reseated in the SAME
    -- socket while powered off produces zero events and two real mate cycles;
    -- swap two modules and swap them back within one power-off window and it is
    -- zero events against four real cycles. Same-socket reseating (cleaning,
    -- checking seating) is the single most common handling action, and this
    -- metric is structurally blind to it. Anything comparing against the ≥500
    -- cycle rating must treat the gap as unknown, not zero.
    module_mate_cycles_observed INTEGER  NOT NULL DEFAULT 0 CHECK (module_mate_cycles_observed >= 0),

    distinct_socket_count   SMALLINT     NOT NULL DEFAULT 0 CHECK (distinct_socket_count BETWEEN 0 AND 128),

    -- DERIVED degradation only — never raw output. Same boundary rule already
    -- applied to EEG impedance (§5.1: "raw impedance → UHDR; trend slope → SHDR").
    pd_ratio_trend_per_session  REAL,
    peak_ntc_trend_per_session  REAL,

    -- Predictive-maintenance verdicts
    output_derate_flag      BOOLEAN      NOT NULL DEFAULT FALSE,
    mate_cycle_limit_flag   BOOLEAN      NOT NULL DEFAULT FALSE,
    retirement_flag         BOOLEAN      NOT NULL DEFAULT FALSE,

    -- Coarse RUL bucket (0 = end of life … 4 = as-new). A bucket, not a
    -- predicted date: a date would be a wall clock, and the reminder engine is
    -- measurement-triggered rather than calendar-triggered (§5.2).
    remaining_life_bucket   SMALLINT     CHECK (remaining_life_bucket BETWEEN 0 AND 4),

    last_seen_month         DATE         NOT NULL DEFAULT DATE_TRUNC('month', NOW())::date,

    -- DEFAULT is not a constraint: it only applies when the ingest job omits the
    -- column, and TIME-01 checks the column TYPE, never its values. Without this
    -- CHECK any writer can land a day-granular DATE and re-create the docking-
    -- time correlator Rev B removed.
    CONSTRAINT ck_module_life_month CHECK (last_seen_month = DATE_TRUNC('month', last_seen_month)::date),

    -- DEGENERATE UIDs. The realistic 64-bit collision is not birthday maths — it
    -- is unprogrammed EEPROM and a floating bus, which report all-zeros and
    -- all-ones respectively. All-zero is additionally np_module_map's explicit
    -- EMPTY-SOCKET SENTINEL (np_module_uid_is_zero). Admitted here, either value
    -- becomes a fake fleet-wide "module" into whose row every unprogrammed or
    -- unresponsive module in the fleet accumulates. This is the only collision
    -- mode that actually occurs, and excluding it costs nothing.
    CONSTRAINT ck_module_life_uid_nonzero CHECK (module_uid NOT IN ('\x0000000000000000'::bytea, '\xFFFFFFFFFFFFFFFF'::bytea)),

    -- See the header note: keyed per (device, module), NOT per module alone.
    CONSTRAINT uq_module_life_device_uid UNIQUE (warranty_token, module_uid)
);

-- ---------------------------------------------------------------------------
-- Monotonicity enforcement for the grow-only accumulators.
--
-- A lifetime counter that can move backwards is worse than no counter, because
-- pd_ratio_trend_per_session and remaining_life_bucket are computed from it and
-- a regression is invisible in the derived value. The GREATEST merge documented
-- above is what keeps them monotone — this trigger is what makes that a
-- property of the DATABASE rather than a convention the ingest job is trusted
-- to follow. A delta-style "+=" writer or an out-of-order replay is rejected
-- here instead of silently corrupting the fleet rollup.
-- ---------------------------------------------------------------------------
CREATE OR REPLACE FUNCTION np_reject_accumulator_regression() RETURNS TRIGGER AS $$
BEGIN
    IF NEW.module_session_count < OLD.module_session_count
       OR NEW.module_mate_cycles_observed < OLD.module_mate_cycles_observed
       OR NEW.distinct_socket_count < OLD.distinct_socket_count THEN
        RAISE EXCEPTION
            'module_life accumulators are grow-only: refusing % -> % sessions, % -> % mate cycles (merge with GREATEST, never +=)',
            OLD.module_session_count, NEW.module_session_count,
            OLD.module_mate_cycles_observed, NEW.module_mate_cycles_observed;
    END IF;
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_module_life_monotonic
    BEFORE UPDATE ON module_life
    FOR EACH ROW EXECUTE FUNCTION np_reject_accumulator_regression();

-- ---------------------------------------------------------------------------
-- View: module_life_fleet_lower_bound
-- Fleet-wide per-module life, as the SUM of the per-device partials.
--
-- THE NAME IS THE WARNING. This is a LOWER BOUND, not the life. A module that
-- moved A → B leaves a partial with device A that is frozen at A's last sync;
-- if A is sold, bricked, or simply never docks again, that partial never
-- advances and the total permanently UNDERSTATES the module's real life. For a
-- figure feeding retirement and wear limits, under-count is the NON-conservative
-- direction — it says a worn module has life left. Absence of an uploaded
-- partial is not absence of accrued life, so no consumer may read this as "the"
-- lifetime; device_partial_count is exposed so callers can see how many devices
-- the figure was assembled from.
--
-- Also note module_mate_cycles_observed is itself a lower bound for an unrelated
-- reason (same-socket reseats are undetectable). The two floors compound.
-- ---------------------------------------------------------------------------
CREATE VIEW module_life_fleet_lower_bound AS
SELECT module_uid,
       MIN(module_part_type)                     AS module_part_type,
       MIN(module_manufacture_date)              AS module_manufacture_date,
       SUM(module_session_count)                 AS module_session_count_lower_bound,
       SUM(module_mate_cycles_observed)          AS mate_cycles_lower_bound,
       MAX(distinct_socket_count)                AS max_distinct_socket_count,
       COUNT(*)                                  AS device_partial_count,
       BOOL_OR(retirement_flag)                  AS any_retirement_flag,
       MIN(remaining_life_bucket)                AS worst_remaining_life_bucket,
       MAX(last_seen_month)                      AS last_seen_month
FROM module_life
GROUP BY module_uid;

-- ---------------------------------------------------------------------------
-- Table: pbm_module_telemetry   (replaces pbm_zone_telemetry, Rev C)
-- Photodiode dose-metering and LED aging data per MODULE per session, recorded
-- against the socket the module occupied at the time.
-- Per NP-FW-EMMC-001 Rev 1 §12: PD1/PD2 ratio → SHDR; raw irradiance → UHDR.
-- ---------------------------------------------------------------------------
CREATE TABLE pbm_module_telemetry (
    id                  BIGSERIAL    PRIMARY KEY,
    warranty_token      BYTEA        NOT NULL REFERENCES devices(warranty_token),

    -- WHERE it was, and WHICH module it was. Both, always — the whole defect in
    -- Rev C was that one stood in for the other.
    socket_number       SMALLINT     NOT NULL CHECK (socket_number BETWEEN 1 AND 128),
    module_uid          BYTEA        NOT NULL CHECK (octet_length(module_uid) = 8),
    module_part_type    VARCHAR(24)  NOT NULL,

    wavelength_nm       SMALLINT     NOT NULL CHECK (wavelength_nm IN (660, 808, 1064, 1170)),

    -- Two independent ordering counters, both integer, neither a clock:
    --   session_index        — device lifetime. Aligns this row with every other
    --                          per-session SHDR table for the same session.
    --   module_session_index — MODULE lifetime. Survives socket moves and device
    --                          moves, so a degradation trend stays continuous
    --                          across a rotation instead of resetting.
    session_index        INTEGER     NOT NULL CHECK (session_index >= 0),
    module_session_index INTEGER     NOT NULL CHECK (module_session_index >= 0),

    -- Placement generation this row was recorded under; ties the row to the
    -- module_placement_events row that put this module in this socket.
    placement_index     INTEGER      NOT NULL CHECK (placement_index >= 0),

    -- PD ratio: separates LED aging from PDMS fouling (RISK-14 Option B)
    -- Only the ratio is SHDR; raw irradiance values are UHDR only.
    pd1_pd2_ratio_final REAL         NOT NULL CHECK (pd1_pd2_ratio_final > 0),
    fouling_flag        BOOLEAN      NOT NULL DEFAULT FALSE,
    aging_flag          BOOLEAN      NOT NULL DEFAULT FALSE,

    -- Calibration provenance. Calibration now travels WITH the module (a smart
    -- module carries its own factory K coefficients), so a socket's calibration
    -- provenance changes when its occupant changes.
    cal_source          VARCHAR(16)  NOT NULL CHECK (cal_source IN ('FACTORY', 'DEFAULT')),

    -- Driver health. (Rev C had no >= 0 CHECK here while every sibling counter
    -- in this file had one; corrected on the way through rather than copied.)
    driver_fault_count  SMALLINT     NOT NULL DEFAULT 0 CHECK (driver_fault_count >= 0),

    -- Peak junction temperature (°C) — no timestamp; thermal management only
    peak_ntc_celsius    REAL         NOT NULL CHECK (peak_ntc_celsius BETWEEN -20.0 AND 120.0),

    -- Month-granular retention anchor only (ordering via session_index)
    ingest_month        DATE         NOT NULL DEFAULT DATE_TRUNC('month', NOW())::date,

    CONSTRAINT ck_pbm_module_month CHECK (ingest_month = DATE_TRUNC('month', ingest_month)::date),
    CONSTRAINT ck_pbm_module_uid_nonzero CHECK (module_uid NOT IN ('\x0000000000000000'::bytea, '\xFFFFFFFFFFFFFFFF'::bytea)),

    -- IDEMPOTENT INGEST. Rev C's pbm_zone_telemetry had no uniqueness, so a
    -- re-uploaded session silently duplicated every zone row and skewed the
    -- aging trend. Fixed here rather than carried forward.
    CONSTRAINT uq_pbm_module_sample UNIQUE (warranty_token, session_index,
                                            socket_number, module_uid, wavelength_nm)
);

-- ---------------------------------------------------------------------------
-- Table: emf_shielding_telemetry
-- EMF shielding attenuation ratio per session (fleet-level moat verification).
-- ---------------------------------------------------------------------------
CREATE TABLE emf_shielding_telemetry (
    id                      BIGSERIAL    PRIMARY KEY,
    warranty_token          BYTEA        NOT NULL REFERENCES devices(warranty_token),
    session_index           INTEGER      NOT NULL CHECK (session_index >= 0),

    -- Passive shielding attenuation (dB) — ratio against calibration baseline
    passive_attenuation_db  REAL         NOT NULL,
    active_attenuation_db   REAL         NOT NULL,

    -- Palladium fabric integrity (binary flag — no continuous resistance value)
    fabric_integrity_ok     BOOLEAN      NOT NULL DEFAULT TRUE,

    -- Month-granular retention anchor only (ordering via session_index)
    ingest_month            DATE         NOT NULL DEFAULT DATE_TRUNC('month', NOW())::date
);

-- ---------------------------------------------------------------------------
-- Table: module_thermal_telemetry   (replaces thermal_profiles, Rev C — part 1)
-- NTC thermistor data per MODULE per session.
--
-- Rev C modelled this as five fixed columns zone1_peak_ntc_celsius ..
-- zone5_peak_ntc_celsius. That is the same retired fixed-five-zone model as
-- pbm_zone_telemetry, expressed as a column shape instead of a key: it caps the
-- lattice at five thermal readings, cannot name WHICH module ran hot, and loses
-- a module's thermal history the moment it moves socket. Thermal load is a
-- primary aging driver (IEC 60601 42°C limit, hardware throttle at 62°C
-- junction), so per-module thermal history is a direct input to both the
-- retirement prediction and the rotation optimiser — it has to follow the part.
--
-- Rows are one per (socket, module, session), so the row count scales with how
-- many tiles are populated rather than being pinned at five.
-- ---------------------------------------------------------------------------
CREATE TABLE module_thermal_telemetry (
    id                      BIGSERIAL    PRIMARY KEY,
    warranty_token          BYTEA        NOT NULL REFERENCES devices(warranty_token),

    socket_number           SMALLINT     NOT NULL CHECK (socket_number BETWEEN 1 AND 128),
    module_uid              BYTEA        NOT NULL CHECK (octet_length(module_uid) = 8),

    session_index           INTEGER      NOT NULL CHECK (session_index >= 0),
    module_session_index    INTEGER      NOT NULL CHECK (module_session_index >= 0),
    placement_index         INTEGER      NOT NULL CHECK (placement_index >= 0),

    -- Peak temperature for this module this session (°C); no time series —
    -- the time series is UHDR. A peak is a device-condition scalar.
    peak_ntc_celsius        REAL         NOT NULL CHECK (peak_ntc_celsius BETWEEN -20.0 AND 120.0),

    -- Throttle engagement is device condition (did the hardware limiter fire),
    -- NOT a dose or duty figure — no seconds, no energy. See DOSE-01.
    thermal_throttle_flag   BOOLEAN      NOT NULL DEFAULT FALSE,

    -- Calibration drift flag (triggered when this module's NTC deviates >1.5°C
    -- from the hub reference — see hub_thermal_telemetry)
    ntc_drift_flag          BOOLEAN      NOT NULL DEFAULT FALSE,

    -- Month-granular retention anchor only (ordering via session_index)
    ingest_month            DATE         NOT NULL DEFAULT DATE_TRUNC('month', NOW())::date,

    CONSTRAINT ck_module_thermal_month CHECK (ingest_month = DATE_TRUNC('month', ingest_month)::date),
    CONSTRAINT ck_module_thermal_uid_nonzero CHECK (module_uid NOT IN ('\x0000000000000000'::bytea, '\xFFFFFFFFFFFFFFFF'::bytea)),
    CONSTRAINT uq_module_thermal_sample UNIQUE (warranty_token, session_index,
                                                socket_number, module_uid)
);

-- ---------------------------------------------------------------------------
-- Table: hub_thermal_telemetry   (replaces thermal_profiles, Rev C — part 2)
-- The hub NTC has no socket and no module: it lives in the control hub, where
-- it cross-calibrates the headset NTCs and feeds supercapacitor aging
-- estimation. Rev C carried it in the same row as the five zone readings, which
-- conflated a device-level scalar with per-position ones. Splitting it out is
-- what lets module_thermal_telemetry be NOT NULL on socket and module.
-- ---------------------------------------------------------------------------
CREATE TABLE hub_thermal_telemetry (
    id                      BIGSERIAL    PRIMARY KEY,
    warranty_token          BYTEA        NOT NULL REFERENCES devices(warranty_token),
    session_index           INTEGER      NOT NULL CHECK (session_index >= 0),

    hub_ntc_celsius         REAL         NOT NULL CHECK (hub_ntc_celsius BETWEEN -20.0 AND 120.0),

    -- Set when one or more module NTCs disagreed with this hub reference beyond
    -- tolerance — the device-level view of the per-module ntc_drift_flag.
    ntc_reference_drift_flag BOOLEAN     NOT NULL DEFAULT FALSE,

    -- Month-granular retention anchor only (ordering via session_index)
    ingest_month            DATE         NOT NULL DEFAULT DATE_TRUNC('month', NOW())::date,

    CONSTRAINT ck_hub_thermal_month CHECK (ingest_month = DATE_TRUNC('month', ingest_month)::date),
    CONSTRAINT uq_hub_thermal_sample UNIQUE (warranty_token, session_index)
);

-- ---------------------------------------------------------------------------
-- Table: module_rotation_advice
-- WEAR-LEVELLING. The predictive-maintenance output that says WHEN to rotate a
-- module and WHICH SOCKET to move it to.
--
-- ── Why rotation exists ────────────────────────────────────────────────────
-- Sockets are not driven equally: a module in a heavily-used socket accumulates
-- optical and thermal degradation far faster than one in a rarely-driven
-- socket. Left alone, the helmet's usable life is set by its most-punished
-- position, and modules elsewhere retire with most of their life unspent.
-- Cycling modules between high- and low-duty sockets levels the wear and
-- maximises the AVERAGE calendar lifespan of the module population — which is
-- the quantity the user actually pays for.
--
-- ── Rotation is not free, and the schema has to say so ─────────────────────
-- Every rotation costs mating cycles on both connector halves. The hex-tile
-- pogo socket is rated ≥500 mating cycles (NP-HW-HEXTILE-001 §, contact spec;
-- qualification is OI-HEXTILE-11, ≤50 mΩ over ≥500 cycles on the EEG signal
-- path). A naive "rotate often" policy therefore destroys connectors faster
-- than it saves emitters. Both stocks are recorded — degradation in module_life,
-- mate cycles in module_life and module_placement_events — so the trade is
-- measurable rather than assumed, and predicted_life_gain_sessions can be
-- scored against what actually happened.
--
-- ── Advice is PER DEVICE, and must never be derived across devices ─────────
-- Counters do not need ordering; EVENTS do. module_placement_events carries a
-- per-device monotonic sequence (power_cycle_index, placement_index), which is
-- a total order WITHIN a device and only a PARTIAL order across devices — there
-- is no wall clock to linearise them, by design (TIME-01). So for a module that
-- has lived in several devices, "the module's most recent socket" is not a
-- well-defined fleet-wide fact. Rotation advice is therefore computed by and
-- for ONE device, over that device's own placement sequence; anything that
-- ranked a module's placements across devices would be derived from an ordering
-- that does not exist. warranty_token is NOT NULL here for exactly this reason.
--
-- (Server ingest-arrival time would order arrival, not occurrence, and is
-- deliberately not used: it would be a per-token wall clock in all but name,
-- which is the docking-time correlator Rev B removed.)
--
-- ── What is deliberately NOT here ──────────────────────────────────────────
-- No per-socket duty, dose, or on-time figure, even though the optimiser needs
-- one. Duty is computed ON-DEVICE from UHDR, where per-zone dose already lives
-- (CLAUDE.md §5.1); only the resulting advice and its outcome reach SHDR. This
-- is the same split already used for EEG impedance (raw → UHDR, trend → SHDR),
-- and it is what keeps an 80-socket duty map — which would reconstruct the
-- user's treatment geography, and so their indication — out of the fleet DB.
-- ---------------------------------------------------------------------------
CREATE TABLE module_rotation_advice (
    id                  BIGSERIAL    PRIMARY KEY,
    warranty_token      BYTEA        NOT NULL REFERENCES devices(warranty_token),

    -- Monotonic per-device advice counter — ordering with no wall clock.
    advice_index        INTEGER      NOT NULL CHECK (advice_index >= 0),

    module_uid          BYTEA        NOT NULL CHECK (octet_length(module_uid) = 8),

    -- The recommendation itself: move THIS module FROM here TO there.
    from_socket_number  SMALLINT     NOT NULL CHECK (from_socket_number BETWEEN 1 AND 128),
    to_socket_number    SMALLINT     NOT NULL CHECK (to_socket_number BETWEEN 1 AND 128),

    reason_code         VARCHAR(24)  NOT NULL CHECK (reason_code IN (
                            'DERATE_BALANCE',    -- output decayed faster than fleet norm
                            'THERMAL_BALANCE',   -- running hot in a high-load position
                            'MATE_CYCLE_LIMIT',  -- module near its connector cycle rating
                            'SOCKET_CYCLE_LIMIT',-- SOCKET near its rating; stop using it as a
                                                 -- destination even though it still works
                            'EOL_DEFER',         -- park a near-EOL module in a low-duty socket
                            'FAULT_ISOLATE')),   -- move a suspect module to aid diagnosis

    -- Closed-loop scoring input: what the model expected to gain. Compared
    -- against the module's realised module_session_count at retirement, this is
    -- what trains the Phase 2/3 models (§5.2).
    predicted_life_gain_sessions INTEGER CHECK (predicted_life_gain_sessions >= 0),

    -- §5.2 predictive-maintenance phase that produced this advice:
    --   1 = population-average survival analysis
    --   2 = fleet-trained LSTM on SHDR trajectories
    --   3 = Bayesian personalisation
    advice_model_phase  SMALLINT     NOT NULL CHECK (advice_model_phase IN (1, 2, 3)),
    advice_model_version VARCHAR(24) NOT NULL,

    -- §5.2 reminder engine class. Rotation is longevity, not safety: it is
    -- snoozable up to 5× and must never block a session start.
    reminder_class      VARCHAR(24)  NOT NULL DEFAULT 'COMFORT_LONGEVITY'
                        CHECK (reminder_class IN ('SAFETY_CRITICAL',
                                                  'PERFORMANCE_CRITICAL',
                                                  'COMFORT_LONGEVITY')),
    snooze_count        SMALLINT     NOT NULL DEFAULT 0 CHECK (snooze_count >= 0),

    -- Outcome — without this the fleet models can never learn whether rotation
    -- works, only that it was suggested.
    outcome             VARCHAR(16)  NOT NULL DEFAULT 'PENDING'
                        CHECK (outcome IN ('PENDING', 'APPLIED', 'DECLINED', 'SUPERSEDED')),

    -- Placement generation in which the advice was carried out (NULL until
    -- APPLIED); joins to the module_placement_events rows that executed it.
    applied_placement_index INTEGER  CHECK (applied_placement_index >= 0),

    -- Month-granular retention anchor only (ordering via advice_index)
    ingest_month        DATE         NOT NULL DEFAULT DATE_TRUNC('month', NOW())::date,

    CONSTRAINT ck_rotation_advice_month CHECK (ingest_month = DATE_TRUNC('month', ingest_month)::date),
    CONSTRAINT ck_rotation_uid_nonzero CHECK (module_uid NOT IN ('\x0000000000000000'::bytea, '\xFFFFFFFFFFFFFFFF'::bytea)),
    -- A module cannot be advised to stay where it is.
    CONSTRAINT ck_rotation_advice_moves CHECK (from_socket_number <> to_socket_number),
    -- One live advice row per (device, module, advice generation).
    CONSTRAINT uq_rotation_advice_gen UNIQUE (warranty_token, module_uid, advice_index)
);

-- At most ONE open recommendation per module per device. Without this, every
-- dock appends another PENDING row for the same unactioned advice and the
-- reminder engine's snooze accounting has no single row to count against.
CREATE UNIQUE INDEX uq_rotation_advice_one_pending
    ON module_rotation_advice (warranty_token, module_uid) WHERE outcome = 'PENDING';

-- ---------------------------------------------------------------------------
-- Table: power_telemetry
-- USB-C PD negotiation log and supercapacitor health.
-- ---------------------------------------------------------------------------
CREATE TABLE power_telemetry (
    id                      BIGSERIAL    PRIMARY KEY,
    warranty_token          BYTEA        NOT NULL REFERENCES devices(warranty_token),
    session_index           INTEGER      NOT NULL CHECK (session_index >= 0),

    -- PD negotiation outcome (no charger identity or user-correlating data)
    pd_negotiated_watts     SMALLINT     NOT NULL CHECK (pd_negotiated_watts IN (5, 15, 30, 45, 65, 100)),
    pd_negotiation_ok       BOOLEAN      NOT NULL DEFAULT TRUE,

    -- Supercapacitor health
    supercap_capacitance_f  REAL         NOT NULL CHECK (supercap_capacitance_f > 0),
    supercap_esr_ohm        REAL,
    supercap_eol_flag       BOOLEAN      NOT NULL DEFAULT FALSE,  -- PRE_EOL_INFO=0x03

    -- Fan health (hub only)
    fan_rpm                 SMALLINT,
    fan_fault               BOOLEAN      NOT NULL DEFAULT FALSE,

    -- Month-granular retention anchor only (ordering via session_index)
    ingest_month            DATE         NOT NULL DEFAULT DATE_TRUNC('month', NOW())::date
);

-- ---------------------------------------------------------------------------
-- Table: consumable_counts
-- Consumable session counts per consumable type.
-- Used for replacement reminders (measurement-triggered per spec).
-- ---------------------------------------------------------------------------
CREATE TABLE consumable_counts (
    id                        BIGSERIAL    PRIMARY KEY,
    warranty_token            BYTEA        NOT NULL REFERENCES devices(warranty_token),

    -- Counts are unsigned integers; no timestamps; no user activity inference.
    -- Reminders are count-triggered, so no timestamp is needed. This is one
    -- upserted row per device, retained for device lifetime; the former
    -- updated_at served no functionality and has been removed (Rev B).
    electrode_tip_sessions    INTEGER      NOT NULL DEFAULT 0 CHECK (electrode_tip_sessions >= 0),
    vns_pad_sessions          INTEGER      NOT NULL DEFAULT 0 CHECK (vns_pad_sessions >= 0),
    intranasal_sleeve_uses    INTEGER      NOT NULL DEFAULT 0 CHECK (intranasal_sleeve_uses >= 0),
    audio_cup_sessions        INTEGER      NOT NULL DEFAULT 0 CHECK (audio_cup_sessions >= 0),
    audio_cup_mesh_sessions   INTEGER      NOT NULL DEFAULT 0 CHECK (audio_cup_mesh_sessions >= 0),

    -- Rev C: one row per device is UPSERTED (see comment above). A UNIQUE
    -- constraint on warranty_token is required so ON CONFLICT (warranty_token)
    -- DO UPDATE has a valid arbiter; without it the "one upserted row per
    -- device" invariant is unenforced and duplicate rows accumulate. The
    -- constraint auto-creates the lookup index (former idx_consumable_token
    -- removed as redundant).
    CONSTRAINT uq_consumable_counts_device UNIQUE (warranty_token)
);

-- ---------------------------------------------------------------------------
-- Table: accessory_auth_log
-- Accessory authentication pass/fail per accessory type.
-- Only authenticated consumables (intranasal sleeves) generate entries.
-- ---------------------------------------------------------------------------
CREATE TABLE accessory_auth_log (
    id                  BIGSERIAL    PRIMARY KEY,
    warranty_token      BYTEA        NOT NULL REFERENCES devices(warranty_token),

    accessory_type      VARCHAR(32)  NOT NULL,   -- 'intranasal_sleeve', etc.
    auth_result         VARCHAR(8)   NOT NULL CHECK (auth_result IN ('PASS', 'FAIL')),
    session_index       INTEGER      NOT NULL CHECK (session_index >= 0),

    -- Month-granular retention anchor only (ordering via session_index)
    ingest_month        DATE         NOT NULL DEFAULT DATE_TRUNC('month', NOW())::date
);

-- ---------------------------------------------------------------------------
-- Table: calibration_history
-- Calibration coefficient history per sensor.
-- Factory-written K coefficients for PBM photodiodes; EEG ADS1299 gain/offset.
-- ---------------------------------------------------------------------------
CREATE TABLE calibration_history (
    id                  BIGSERIAL    PRIMARY KEY,
    warranty_token      BYTEA        NOT NULL REFERENCES devices(warranty_token),

    sensor_id           VARCHAR(32)  NOT NULL,   -- 'EEG_CH1', 'PBM_ZM01_660', etc.
    cal_type            VARCHAR(16)  NOT NULL CHECK (cal_type IN ('FACTORY', 'FIELD', 'AUTO')),
    coefficient_key     VARCHAR(32)  NOT NULL,   -- 'K_PD1_660', 'GAIN_CH1', etc.
    coefficient_value   DOUBLE PRECISION NOT NULL,

    session_at_cal      INTEGER      NOT NULL CHECK (session_at_cal >= 0),
    -- Month-granular retention anchor only (ordering via session_at_cal)
    ingest_month        DATE         NOT NULL DEFAULT DATE_TRUNC('month', NOW())::date
);

-- ---------------------------------------------------------------------------
-- Table: eeg_impedance_trend
-- EEG electrode impedance trend slope — SHDR only (raw impedance series → UHDR).
-- Per NP-FW-EMMC-001 Rev 1 §12: "raw EEG impedance → UHDR; derived trend slope → SHDR"
-- ---------------------------------------------------------------------------
CREATE TABLE eeg_impedance_trend (
    id                  BIGSERIAL    PRIMARY KEY,
    warranty_token      BYTEA        NOT NULL REFERENCES devices(warranty_token),

    electrode_id        VARCHAR(8)   NOT NULL,   -- 'Fp1', 'C3', etc.
    trend_slope_ohm_per_session REAL NOT NULL,   -- derived metric only
    replacement_flag    BOOLEAN      NOT NULL DEFAULT FALSE,

    session_index       INTEGER      NOT NULL CHECK (session_index >= 0),
    -- Month-granular retention anchor only (ordering via session_index)
    ingest_month        DATE         NOT NULL DEFAULT DATE_TRUNC('month', NOW())::date
);

-- ---------------------------------------------------------------------------
-- Table: shdr_accel_records
-- ACCELEROMETER DATA — only two boolean fields permitted per NP-FW-EMMC-002 §G.
--
-- PROHIBITED columns (enforced by CI test OI-EMMC2-07):
--   g_force, accel_x, accel_y, accel_z, orientation_*, drop_count,
--   drop_timestamp, impact_*, fall_*, rms_g, peak_g, average_g
--
-- Permitted columns per NP-FW-EMMC-002 §G.2:
--   drop_detected BOOLEAN — true if peak g-force exceeded threshold (on-device only)
--   maintenance_alert BOOLEAN — true if rolling drop rate exceeded threshold
--
-- BOTH THRESHOLDS ARE UNVALIDATED PLACEHOLDERS (NP-FW-EMMC-002 Rev 2 §G.2):
-- NP_ACCEL_DROP_THRESHOLD_G = 15.0f and NP_ACCEL_MAINT_THRESHOLD = 3 were chosen
-- before any hardware existed and have never been compared against a device that
-- failed. What a row in this table MEANS is therefore not yet established. That
-- is what shdr_accel_characterisation (below, §H) exists to fix.
--
-- KNOWN LIMIT OF THIS TABLE'S PRIVACY CONTROL — recorded 2026-08-12, OI-EMMC2-11.
-- This table is append-per-gap with a NON-UNIQUE warranty_token and an index on
-- it, so both quantities §G.3 bans by name survive as AGGREGATES over the rows:
--   SELECT count(*) FILTER (WHERE drop_detected) WHERE warranty_token = ?
--     — this IS "drop count as an integer".
--   the gap_index values where drop_detected is true
--     — this IS the inter-drop spacing that "timestamp of individual drop
--       events" was banned to prevent.
-- A column-level control cannot express a row-set-level hazard. Not fixed here:
-- collapsing to one upserted row per device would destroy the per-gap sequencing
-- the §H programme is built on, so the two have to be decided together.
-- ---------------------------------------------------------------------------
CREATE TABLE shdr_accel_records (
    id                  BIGSERIAL    PRIMARY KEY,
    warranty_token      BYTEA        NOT NULL REFERENCES devices(warranty_token),

    -- ONLY THESE TWO ACCELEROMETER-DERIVED COLUMNS ARE PERMITTED
    drop_detected       BOOLEAN      NOT NULL DEFAULT FALSE,
    maintenance_alert   BOOLEAN      NOT NULL DEFAULT FALSE,

    -- Between-session gap index — provides ordering with no wall clock
    gap_index           INTEGER      NOT NULL CHECK (gap_index >= 0),

    -- Month-granular retention anchor only (ordering via gap_index). The former
    -- per-row created_at wall clock (a docking-time correlator) is removed.
    ingest_month        DATE         NOT NULL DEFAULT DATE_TRUNC('month', NOW())::date
);

-- ---------------------------------------------------------------------------
-- Table: shdr_accel_characterisation           (Rev E — NP-FW-EMMC-002 §H)
--
-- THE ONE PLACE IN THIS SCHEMA WHERE §G.3's PROHIBITION IS RELAXED, and it is
-- relaxed narrowly, temporarily, with consent, and self-revocably.
--
-- WHY IT EXISTS. shdr_accel_records' two booleans are computed from two numbers
-- nobody has validated, and §G.3 bans every field that could validate them. The
-- spec forecloses the evidence needed to make the spec correct. §H is the
-- bounded exception that unblocks it, on the pattern NP-MOD-ID-001 §7 set for
-- the DOSE-01 relaxation.
--
-- THE COLUMNS ARE NAMED HONESTLY. impact_* is a pattern ACCEL-01 rejects, and
-- these columns keep it deliberately. An innocuous name would have let this
-- data past the gate without anyone deciding to admit it; carrying the banned
-- prefix makes the CHAR-01 exemption load-bearing and visible in every diff.
--
-- THE FOUR MARKERS are not documentation. ci/test_shdr_schema.py requires all
-- four, checks them against the firmware constants in
-- firmware/shdr/include/np_accel_shdr.h, and DERIVES the ACCEL-01/ACCEL-02
-- exemption from the result. Weaken any marker and the exemption lapses, so the
-- impact_* columns immediately become ACCEL-01 violations again. The exception
-- revokes itself rather than needing to be revoked.
--
--   char_programme_id      — pinned by CHECK. Consent to programme N does not
--                            authorise N+1; a closed programme's rows can never
--                            be silently re-attributed to an open one.
--   char_consent_granted   — CHECK satisfiable ONLY by TRUE, so a row for a
--                            non-consented device is not merely unsent, it is
--                            unstorable. PostgreSQL is the enforcement, not the
--                            uploader.
--   char_consent_epoch     — bumped on each grant, so a row cannot outlive the
--                            consent decision that authorised it.
--   char_record_seq        — bounded to NP_ACCEL_CHAR_RECORD_BUDGET. This IS
--                            the window (see below).
--
-- THE WINDOW HAS NO CALENDAR. NP-MOD-ID-001 §7.3 bounds its programme at "24
-- months from first fleet upload"; that is not implementable here. The headset
-- has no battery, coin cell or VBAT rail — it is USB-C powered (CLAUDE.md §4.5)
-- — so the RT1062's SNVS RTC has no backup domain: wall time is lost on every
-- disconnect and re-supplied by the phone. A calendar expiry would be both
-- losable and settable backwards. So the window is denominated in the records it
-- admits: char_record_seq counts up to a build-time per-device budget, advances
-- only when a record is emitted, and cannot be un-advanced. Exhausting the
-- budget and answering the question are the same act.
--
-- CONSENT SUBJECT is the WARRANTY OWNER (CLAUDE.md §6.0) — a separate,
-- affirmative, granular, revocable opt-in, never folded into warranty
-- registration. It is sufficient in every device configuration because there is
-- no identifiable subject: SHDR holds no user identifier, the warranty DB (which
-- SHDR cannot join) names the REGISTRANT, and nothing anywhere records the
-- registrant↔wearer relationship (NP-MOD-ID-001 §7.5.1). The argument is
-- STRONGER here than in the precedent it follows: a duty map at least requires
-- the device to be WORN, whereas a drop does not — anyone handling the headset
-- can drop it.
--
-- PURPOSE BINDING: predictive-maintenance model training only (§H.5). Rows are
-- DELETED from this table at window close; the fitted model survives, the
-- evidence does not.
--
-- WHAT IS STILL PROHIBITED HERE, exactly as in shdr_accel_records: any raw
-- series, any per-event g value, any orientation, any per-event timestamp. What
-- is admitted is a COARSENED per-gap histogram — counts per fixed bin — which
-- carries the distribution the review gate needs while destroying the per-event
-- trajectory that made the raw series health-inferrable.
-- ---------------------------------------------------------------------------
CREATE TABLE shdr_accel_characterisation (
    id                    BIGSERIAL  PRIMARY KEY,
    warranty_token        BYTEA      NOT NULL REFERENCES devices(warranty_token),

    -- ── The four CHAR-01 structural markers ──────────────────────────────
    char_programme_id     SMALLINT   NOT NULL CHECK (char_programme_id = 1),
    char_consent_granted  BOOLEAN    NOT NULL CHECK (char_consent_granted),
    char_consent_epoch    INTEGER    NOT NULL CHECK (char_consent_epoch >= 1),
    char_record_seq       INTEGER    NOT NULL CHECK (char_record_seq BETWEEN 1 AND 512),

    -- ── Coarsened impact histogram — counts per fixed bin, per gap ────────
    -- Bin lower edges in g, from np_accel_char_g_bin_edges:
    --   1:[2.0,3.0) 2:[3.0,4.5) 3:[4.5,6.5) 4:[6.5,9.5)
    --   5:[9.5,14.0) 6:[14.0,21.0) 7:[21.0,31.0) 8:[31.0,∞)
    -- The 15.0 g placeholder sits inside bin 6, with 5 and 7 either side, so the
    -- review gate can resolve both directions from the number it is testing.
    impact_g_bin_1        SMALLINT   NOT NULL DEFAULT 0 CHECK (impact_g_bin_1 >= 0),
    impact_g_bin_2        SMALLINT   NOT NULL DEFAULT 0 CHECK (impact_g_bin_2 >= 0),
    impact_g_bin_3        SMALLINT   NOT NULL DEFAULT 0 CHECK (impact_g_bin_3 >= 0),
    impact_g_bin_4        SMALLINT   NOT NULL DEFAULT 0 CHECK (impact_g_bin_4 >= 0),
    impact_g_bin_5        SMALLINT   NOT NULL DEFAULT 0 CHECK (impact_g_bin_5 >= 0),
    impact_g_bin_6        SMALLINT   NOT NULL DEFAULT 0 CHECK (impact_g_bin_6 >= 0),
    impact_g_bin_7        SMALLINT   NOT NULL DEFAULT 0 CHECK (impact_g_bin_7 >= 0),
    impact_g_bin_8        SMALLINT   NOT NULL DEFAULT 0 CHECK (impact_g_bin_8 >= 0),

    -- Σ of the bins. Explicit so bin saturation is detectable rather than silent.
    impact_event_count    SMALLINT   NOT NULL DEFAULT 0 CHECK (impact_event_count >= 0),

    -- Between-session gap index — ordering with no wall clock, as elsewhere.
    gap_index             INTEGER    NOT NULL CHECK (gap_index >= 0),

    -- Month-granular retention anchor only.
    ingest_month          DATE       NOT NULL DEFAULT DATE_TRUNC('month', NOW())::date
);

-- ---------------------------------------------------------------------------
-- Table: mode_f_telemetry
-- Mode F (retinal NIR PBM) device-side metrics.
-- Per NP-FW-EMMC-002 §F.3: mode_f_enabled_flag (bool) → SHDR; dose → UHDR.
--
-- Cardinality: APPEND-PER-UPLOAD (audit trail), NOT one-row-per-device. Each
-- row is an immutable snapshot of Mode F device state (enabled flag,
-- regulatory-clearance flag, cumulative session count) captured at upload. The
-- history of state changes feeds predictive maintenance, so BIGSERIAL id is the
-- correct PK and warranty_token is intentionally NON-unique. Do NOT add a
-- UNIQUE (warranty_token) constraint here — contrast consumable_counts, which
-- IS upsert-in-place. Reviewed for schema freeze 2026-07-14 (Rev C).
-- ---------------------------------------------------------------------------
CREATE TABLE mode_f_telemetry (
    id                      BIGSERIAL    PRIMARY KEY,
    warranty_token          BYTEA        NOT NULL REFERENCES devices(warranty_token),

    mode_f_enabled          BOOLEAN      NOT NULL DEFAULT FALSE,
    mode_f_session_count    INTEGER      NOT NULL DEFAULT 0 CHECK (mode_f_session_count >= 0),

    -- Regulatory gate status (reflects NP_MODE_F_REGULATORY_CLEARED build flag)
    regulatory_cleared      BOOLEAN      NOT NULL DEFAULT FALSE,

    -- Month-granular retention anchor only (ordering via mode_f_session_count)
    ingest_month            DATE         NOT NULL DEFAULT DATE_TRUNC('month', NOW())::date
);

-- ---------------------------------------------------------------------------
-- Table: fault_log
-- Safety interlock and firmware fault events.
-- Per NP-FW-EMMC-002 §G.3: safety interlock log → SHDR (no HR values, no
-- raw impedance — only the binary cutoff flag and fault type).
-- ---------------------------------------------------------------------------
CREATE TABLE fault_log (
    id                  BIGSERIAL    PRIMARY KEY,
    warranty_token      BYTEA        NOT NULL REFERENCES devices(warranty_token),

    fault_type          VARCHAR(64)  NOT NULL,   -- 'WATCHDOG_TIMEOUT', 'CVNS_HR_CUTOFF', etc.
    fault_source        VARCHAR(16)  NOT NULL CHECK (fault_source IN ('SAFETY_MCU', 'MAIN_PROC', 'APP')),
    fault_cleared       BOOLEAN      NOT NULL DEFAULT FALSE,

    session_index       INTEGER      NOT NULL CHECK (session_index >= 0),
    -- Month-granular retention anchor only (ordering via session_index). Precise
    -- fault event timing, where it exists at all, is UHDR under the user's key.
    ingest_month        DATE         NOT NULL DEFAULT DATE_TRUNC('month', NOW())::date
);

-- ---------------------------------------------------------------------------
-- Table: storage_health
-- eMMC write endurance monitoring (PRE_EOL_INFO, wear indicator, LittleFS usage).
-- ---------------------------------------------------------------------------
CREATE TABLE storage_health (
    id                          BIGSERIAL    PRIMARY KEY,
    warranty_token              BYTEA        NOT NULL REFERENCES devices(warranty_token),

    pre_eol_info                SMALLINT     NOT NULL CHECK (pre_eol_info IN (1, 2, 3)),
    wear_indicator_slc          SMALLINT     NOT NULL CHECK (wear_indicator_slc BETWEEN 0 AND 255),
    littlefs_block_utilisation  REAL         NOT NULL CHECK (littlefs_block_utilisation BETWEEN 0.0 AND 1.0),
    eol_block_flag              BOOLEAN      NOT NULL DEFAULT FALSE,

    session_index               INTEGER      NOT NULL CHECK (session_index >= 0),
    -- Month-granular retention anchor only (ordering via session_index)
    ingest_month                DATE         NOT NULL DEFAULT DATE_TRUNC('month', NOW())::date
);

-- ---------------------------------------------------------------------------
-- Indexes
-- ---------------------------------------------------------------------------
CREATE INDEX idx_firmware_history_token    ON firmware_history(warranty_token);
CREATE INDEX idx_ota_events_token          ON ota_events(warranty_token);
CREATE INDEX idx_pbm_module_token_session  ON pbm_module_telemetry(warranty_token, session_index);
CREATE INDEX idx_emf_token_session         ON emf_shielding_telemetry(warranty_token, session_index);
CREATE INDEX idx_module_thermal_token_sess ON module_thermal_telemetry(warranty_token, session_index);
CREATE INDEX idx_hub_thermal_token_session ON hub_thermal_telemetry(warranty_token, session_index);

-- Module-identity layer. The UID indexes are the ones that matter: per-part
-- predictive maintenance queries by module, not by device, and must stay fast
-- when a module has moved sockets (and devices) several times.
CREATE INDEX idx_module_inventory_token    ON module_inventory(warranty_token, socket_number);
CREATE INDEX idx_module_inventory_uid      ON module_inventory(module_uid);
CREATE INDEX idx_placement_events_token    ON module_placement_events(warranty_token, placement_index);
CREATE INDEX idx_placement_events_uid      ON module_placement_events(module_uid);
CREATE INDEX idx_placement_events_socket   ON module_placement_events(warranty_token, socket_number);
CREATE INDEX idx_module_life_part_type     ON module_life(module_part_type);
CREATE INDEX idx_module_life_retirement    ON module_life(retirement_flag) WHERE retirement_flag;

-- Socket-side condition. The part-type index is the fleet query that answers
-- "does module kind X wear sockets faster than kind Y", so it leads on the kind.
CREATE INDEX idx_socket_life_token         ON socket_life(warranty_token, socket_number);
CREATE INDEX idx_socket_life_worn          ON socket_life(warranty_token) WHERE mate_cycle_limit_flag;
CREATE INDEX idx_socket_part_wear_kind     ON socket_part_type_wear(module_part_type);
CREATE INDEX idx_socket_part_wear_token    ON socket_part_type_wear(warranty_token, socket_number);

-- Per-module telemetry trends: the module UID is the trend axis, because the
-- trend must stay continuous across a socket change.
CREATE INDEX idx_pbm_module_uid_session    ON pbm_module_telemetry(module_uid, module_session_index);
CREATE INDEX idx_module_thermal_uid_sess   ON module_thermal_telemetry(module_uid, module_session_index);

-- Rotation advice: the hot query is "what is still outstanding for this device".
CREATE INDEX idx_rotation_advice_token     ON module_rotation_advice(warranty_token, advice_index);
CREATE INDEX idx_rotation_advice_uid       ON module_rotation_advice(module_uid);
CREATE INDEX idx_rotation_advice_pending   ON module_rotation_advice(warranty_token) WHERE outcome = 'PENDING';
CREATE INDEX idx_power_token_session       ON power_telemetry(warranty_token, session_index);
CREATE INDEX idx_accessory_auth_token      ON accessory_auth_log(warranty_token);
CREATE INDEX idx_calibration_token         ON calibration_history(warranty_token, sensor_id);
CREATE INDEX idx_eeg_impedance_token       ON eeg_impedance_trend(warranty_token, electrode_id);
CREATE INDEX idx_accel_records_token       ON shdr_accel_records(warranty_token);
CREATE INDEX idx_mode_f_token              ON mode_f_telemetry(warranty_token);
CREATE INDEX idx_fault_log_token           ON fault_log(warranty_token, fault_type);
CREATE INDEX idx_storage_health_token      ON storage_health(warranty_token);
