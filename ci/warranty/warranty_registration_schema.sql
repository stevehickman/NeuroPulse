-- NeuroPulse Warranty Registration Database Schema
-- SEPARATE from SHDR fleet database (separate cloud project, no shared IAM roles).
-- Links warranty_token (opaque 32-byte TRNG value) to owner PII for support purposes.
-- Per NP-FW-EMMC-002 §A.3: the warranty_token is the sole linkage to the SHDR fleet DB.
-- The no-join rule (OI-EMMC2-06) prohibits any cross-DB query joining this schema
-- with the SHDR fleet schema.

CREATE TABLE warranty_registrations (
    id                   BIGSERIAL    PRIMARY KEY,
    warranty_token       BYTEA        NOT NULL UNIQUE,  -- 32-byte TRNG token
    serial_number        VARCHAR(32)  NOT NULL,
    device_model         VARCHAR(32)  NOT NULL,
    registered_name      VARCHAR(128) NOT NULL,
    registered_email     VARCHAR(254) NOT NULL,
    registered_address   TEXT,
    registration_date    DATE         NOT NULL DEFAULT CURRENT_DATE,
    purchase_date        DATE,
    purchase_retailer    VARCHAR(128),
    warranty_tier        VARCHAR(16)  NOT NULL DEFAULT 'STANDARD' CHECK (warranty_tier IN ('STANDARD', 'PREMIUM', 'PRO_ENTRY', 'PRO_FULL')),
    warranty_expires     DATE,
    support_contact_ok   BOOLEAN      NOT NULL DEFAULT TRUE,
    created_at           TIMESTAMPTZ  NOT NULL DEFAULT NOW(),
    updated_at           TIMESTAMPTZ  NOT NULL DEFAULT NOW()
);

CREATE TABLE warranty_claims (
    id                   BIGSERIAL    PRIMARY KEY,
    warranty_token       BYTEA        NOT NULL REFERENCES warranty_registrations(warranty_token),
    claim_type           VARCHAR(32)  NOT NULL,
    claim_description    TEXT,
    claim_status         VARCHAR(16)  NOT NULL DEFAULT 'OPEN' CHECK (claim_status IN ('OPEN', 'IN_PROGRESS', 'RESOLVED', 'DENIED')),
    created_at           TIMESTAMPTZ  NOT NULL DEFAULT NOW()
);

CREATE INDEX idx_warranty_reg_token    ON warranty_registrations(warranty_token);
CREATE INDEX idx_warranty_reg_email    ON warranty_registrations(registered_email);
CREATE INDEX idx_warranty_claims_token ON warranty_claims(warranty_token);
