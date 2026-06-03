-- NeuroPulse SHDR Fleet Database Schema
-- Document: NP-FW-EMMC-002 Rev A §G.4 / NP-FW-EMMC-001 Rev A §7
-- Revision: A
-- Effective date: 2026-06-03
-- Status: ACTIVE — BLOCKING for schema freeze (OI-EMMC2-07 must PASS)
--
-- PRIVACY RULES enforced by CI test (ci/test_shdr_schema.py, OI-EMMC2-07):
--   1. No raw accelerometer columns (g-force, orientation, drop_count, timestamps)
--   2. No personal-data columns (name, email, address, phone, postal)
--   3. No-join rule: this schema must not contain foreign keys to the warranty
--      registration database. Warranty linkage is via opaque warranty_token only.
--      The warrant_db and shdr_db live in separate cloud projects with no shared
--      IAM roles. A separate CI test (OI-EMMC2-06) confirms the join fails.
--
-- PERMITTED accelerometer columns per NP-FW-EMMC-002 §G.2:
--   drop_detected BOOLEAN, maintenance_alert BOOLEAN  — and nothing else.
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

    created_at              TIMESTAMPTZ  NOT NULL DEFAULT NOW(),
    updated_at              TIMESTAMPTZ  NOT NULL DEFAULT NOW()
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

    -- Session counter at install time — no wall-clock timestamp in SHDR
    session_at_install  INTEGER      NOT NULL CHECK (session_at_install >= 0),

    created_at          TIMESTAMPTZ  NOT NULL DEFAULT NOW()
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
    created_at          TIMESTAMPTZ  NOT NULL DEFAULT NOW()
);

-- ---------------------------------------------------------------------------
-- Table: pbm_zone_telemetry
-- Photodiode dose-metering and LED aging data per zone per session.
-- Per NP-FW-EMMC-001 Rev A §12: PD1/PD2 ratio → SHDR; raw irradiance → UHDR.
-- ---------------------------------------------------------------------------
CREATE TABLE pbm_zone_telemetry (
    id                  BIGSERIAL    PRIMARY KEY,
    warranty_token      BYTEA        NOT NULL REFERENCES devices(warranty_token),

    zone_id             SMALLINT     NOT NULL CHECK (zone_id BETWEEN 1 AND 5),
    wavelength_nm       SMALLINT     NOT NULL CHECK (wavelength_nm IN (660, 808, 1064, 1170)),
    session_index       INTEGER      NOT NULL CHECK (session_index >= 0),

    -- PD ratio: separates LED aging from PDMS fouling (RISK-14 Option B)
    -- Only the ratio is SHDR; raw irradiance values are UHDR only.
    pd1_pd2_ratio_final REAL         NOT NULL CHECK (pd1_pd2_ratio_final > 0),
    fouling_flag        BOOLEAN      NOT NULL DEFAULT FALSE,
    aging_flag          BOOLEAN      NOT NULL DEFAULT FALSE,

    -- Calibration provenance
    cal_source          VARCHAR(16)  NOT NULL CHECK (cal_source IN ('FACTORY', 'DEFAULT')),

    -- Driver health
    driver_fault_count  SMALLINT     NOT NULL DEFAULT 0,

    -- Peak junction temperature (°C) — no timestamp; thermal management only
    peak_ntc_celsius    REAL         NOT NULL CHECK (peak_ntc_celsius BETWEEN -20.0 AND 120.0),

    created_at          TIMESTAMPTZ  NOT NULL DEFAULT NOW()
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

    created_at              TIMESTAMPTZ  NOT NULL DEFAULT NOW()
);

-- ---------------------------------------------------------------------------
-- Table: thermal_profiles
-- NTC thermistor data: zone NTCs + hub NTC. No timestamps.
-- Hub NTC cross-calibrates headset NTCs and monitors supercap aging.
-- ---------------------------------------------------------------------------
CREATE TABLE thermal_profiles (
    id                      BIGSERIAL    PRIMARY KEY,
    warranty_token          BYTEA        NOT NULL REFERENCES devices(warranty_token),
    session_index           INTEGER      NOT NULL CHECK (session_index >= 0),

    -- Peak temperature per zone (°C); no time series — time series is UHDR
    zone1_peak_ntc_celsius  REAL,
    zone2_peak_ntc_celsius  REAL,
    zone3_peak_ntc_celsius  REAL,
    zone4_peak_ntc_celsius  REAL,
    zone5_peak_ntc_celsius  REAL,
    hub_ntc_celsius         REAL,

    -- Calibration drift flag (triggered when NTC deviates >1.5°C from hub ref)
    ntc_drift_flag          BOOLEAN      NOT NULL DEFAULT FALSE,

    created_at              TIMESTAMPTZ  NOT NULL DEFAULT NOW()
);

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

    created_at              TIMESTAMPTZ  NOT NULL DEFAULT NOW()
);

-- ---------------------------------------------------------------------------
-- Table: consumable_counts
-- Consumable session counts per consumable type.
-- Used for replacement reminders (measurement-triggered per spec).
-- ---------------------------------------------------------------------------
CREATE TABLE consumable_counts (
    id                        BIGSERIAL    PRIMARY KEY,
    warranty_token            BYTEA        NOT NULL REFERENCES devices(warranty_token),

    -- Counts are unsigned integers; no timestamps; no user activity inference
    electrode_tip_sessions    INTEGER      NOT NULL DEFAULT 0 CHECK (electrode_tip_sessions >= 0),
    vns_pad_sessions          INTEGER      NOT NULL DEFAULT 0 CHECK (vns_pad_sessions >= 0),
    intranasal_sleeve_uses    INTEGER      NOT NULL DEFAULT 0 CHECK (intranasal_sleeve_uses >= 0),
    audio_cup_sessions        INTEGER      NOT NULL DEFAULT 0 CHECK (audio_cup_sessions >= 0),
    audio_cup_mesh_sessions   INTEGER      NOT NULL DEFAULT 0 CHECK (audio_cup_mesh_sessions >= 0),

    updated_at                TIMESTAMPTZ  NOT NULL DEFAULT NOW()
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

    created_at          TIMESTAMPTZ  NOT NULL DEFAULT NOW()
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
    created_at          TIMESTAMPTZ  NOT NULL DEFAULT NOW()
);

-- ---------------------------------------------------------------------------
-- Table: eeg_impedance_trend
-- EEG electrode impedance trend slope — SHDR only (raw impedance series → UHDR).
-- Per NP-FW-EMMC-001 Rev A §12: "raw EEG impedance → UHDR; derived trend slope → SHDR"
-- ---------------------------------------------------------------------------
CREATE TABLE eeg_impedance_trend (
    id                  BIGSERIAL    PRIMARY KEY,
    warranty_token      BYTEA        NOT NULL REFERENCES devices(warranty_token),

    electrode_id        VARCHAR(8)   NOT NULL,   -- 'Fp1', 'C3', etc.
    trend_slope_ohm_per_session REAL NOT NULL,   -- derived metric only
    replacement_flag    BOOLEAN      NOT NULL DEFAULT FALSE,

    session_index       INTEGER      NOT NULL CHECK (session_index >= 0),
    created_at          TIMESTAMPTZ  NOT NULL DEFAULT NOW()
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
-- ---------------------------------------------------------------------------
CREATE TABLE shdr_accel_records (
    id                  BIGSERIAL    PRIMARY KEY,
    warranty_token      BYTEA        NOT NULL REFERENCES devices(warranty_token),

    -- ONLY THESE TWO ACCELEROMETER-DERIVED COLUMNS ARE PERMITTED
    drop_detected       BOOLEAN      NOT NULL DEFAULT FALSE,
    maintenance_alert   BOOLEAN      NOT NULL DEFAULT FALSE,

    -- Between-session gap index (no wall-clock timestamps for individual events)
    gap_index           INTEGER      NOT NULL CHECK (gap_index >= 0),

    created_at          TIMESTAMPTZ  NOT NULL DEFAULT NOW()
);

-- ---------------------------------------------------------------------------
-- Table: mode_f_telemetry
-- Mode F (retinal NIR PBM) device-side metrics.
-- Per NP-FW-EMMC-002 §F.3: mode_f_enabled_flag (bool) → SHDR; dose → UHDR.
-- ---------------------------------------------------------------------------
CREATE TABLE mode_f_telemetry (
    id                      BIGSERIAL    PRIMARY KEY,
    warranty_token          BYTEA        NOT NULL REFERENCES devices(warranty_token),

    mode_f_enabled          BOOLEAN      NOT NULL DEFAULT FALSE,
    mode_f_session_count    INTEGER      NOT NULL DEFAULT 0 CHECK (mode_f_session_count >= 0),

    -- Regulatory gate status (reflects NP_MODE_F_REGULATORY_CLEARED build flag)
    regulatory_cleared      BOOLEAN      NOT NULL DEFAULT FALSE,

    created_at              TIMESTAMPTZ  NOT NULL DEFAULT NOW()
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
    created_at          TIMESTAMPTZ  NOT NULL DEFAULT NOW()
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
    created_at                  TIMESTAMPTZ  NOT NULL DEFAULT NOW()
);

-- ---------------------------------------------------------------------------
-- Indexes
-- ---------------------------------------------------------------------------
CREATE INDEX idx_firmware_history_token    ON firmware_history(warranty_token);
CREATE INDEX idx_ota_events_token          ON ota_events(warranty_token);
CREATE INDEX idx_pbm_zone_token_session    ON pbm_zone_telemetry(warranty_token, session_index);
CREATE INDEX idx_emf_token_session         ON emf_shielding_telemetry(warranty_token, session_index);
CREATE INDEX idx_thermal_token_session     ON thermal_profiles(warranty_token, session_index);
CREATE INDEX idx_power_token_session       ON power_telemetry(warranty_token, session_index);
CREATE INDEX idx_consumable_token          ON consumable_counts(warranty_token);
CREATE INDEX idx_accessory_auth_token      ON accessory_auth_log(warranty_token);
CREATE INDEX idx_calibration_token         ON calibration_history(warranty_token, sensor_id);
CREATE INDEX idx_eeg_impedance_token       ON eeg_impedance_trend(warranty_token, electrode_id);
CREATE INDEX idx_accel_records_token       ON shdr_accel_records(warranty_token);
CREATE INDEX idx_mode_f_token              ON mode_f_telemetry(warranty_token);
CREATE INDEX idx_fault_log_token           ON fault_log(warranty_token, fault_type);
CREATE INDEX idx_storage_health_token      ON storage_health(warranty_token);
