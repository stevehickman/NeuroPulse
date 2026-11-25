-- NeurOne Warranty Registration Database Schema
-- SEPARATE from SHDR fleet database (separate cloud project, no shared IAM roles).
-- Links warranty_token (opaque 32-byte TRNG value) to owner PII for support purposes.
-- Per NP-FW-EMMC-002 §A.3: the warranty_token is the sole linkage to the SHDR fleet DB.
-- The no-join rule (OI-EMMC2-06) prohibits any cross-DB query joining this schema
-- with the SHDR fleet schema.
--
-- Minimisation (OI-PRIV-54, NP-PRIV-ANALYSIS-005 F4; REQ-WARR-01 to REQ-WARR-04 in
-- docs/reference/data-architecture-detail.md §5.7; gated by ci/test_warranty_schema.py):
--   * contact is opt-in: support_contact_ok defaults to FALSE (REQ-WARR-01);
--   * no free text sits beside the owner's identity: a claim's kind is a fixed set the
--     database enforces, and any free-text note is in warranty_claim_notes, under a role
--     of its own (REQ-WARR-02);
--   * no column holds a time finer than a day (REQ-WARR-03);
--   * every table carries retain_until (REQ-WARR-04).
--
-- NOT DECIDED (the principal's; docs/human-input/oi-priv-54-warranty-retention-and-erasure.md):
-- how long a registration, a claim and a note are kept; what an owner's erasure request
-- removes; what happens to a previous owner's registration when the device is transferred.
-- retain_until therefore has no DEFAULT and may be NULL, and nothing here computes it. The
-- foreign keys below keep PostgreSQL's default action (NO ACTION), so deleting a
-- registration that still has claims, or a claim that still has notes, is refused; that is
-- the default left in place, not an erasure rule, and it changes with the answer.

CREATE TABLE warranty_registrations (
    id                   BIGSERIAL    PRIMARY KEY,
    warranty_token       BYTEA        NOT NULL UNIQUE,  -- 32-byte TRNG token
    -- The serial format is not yet defined. If it encodes the factory, line or date
    -- of manufacture, it narrows the SHDR devices row it belongs to (factory_id,
    -- hw_revision, month of manufacture) without any shared identifier; that is
    -- why SHDR keeps only the month (schema Rev G, OI-PRIV-58, beside OI-EMMC2-08).
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
    -- Opt-in (REQ-WARR-01): the owner is contacted for support only after choosing it.
    -- GDPR Art. 25(2) (data protection by default) and Recital 32 (silence or a
    -- pre-ticked box is not consent). Was DEFAULT TRUE (UC-199, now retired).
    support_contact_ok   BOOLEAN      NOT NULL DEFAULT FALSE,
    -- created_at and updated_at (TIMESTAMPTZ) were removed (REQ-WARR-03):
    -- registration_date already records the day of registration, and no function
    -- needed the time of an edit.
    retain_until         DATE
);

CREATE TABLE warranty_claims (
    id                   BIGSERIAL    PRIMARY KEY,
    warranty_token       BYTEA        NOT NULL REFERENCES warranty_registrations(warranty_token),
    -- What failed, as a fixed set (REQ-WARR-02 (a)). Every value names a part or a
    -- function of the product, never an effect on a body: an injury or a symptom is not
    -- a warranty category. The list itself is a placeholder (UC-213).
    -- claim_description (free text) was removed: it sat beside name, email and address
    -- and would have collected health statements (GDPR Art. 9).
    claim_type           VARCHAR(32)  NOT NULL CHECK (claim_type IN ('HUB_POWER_OR_CHARGING', 'CONNECTIVITY', 'PBM_MODULE', 'INTRANASAL_PROBE', 'EEG_ELECTRODE', 'STIM_ELECTRODE', 'VNS_CLIP', 'AUDIO', 'VISUAL_LENS', 'FIT_SYSTEM', 'FIRMWARE_OR_UPDATE', 'COSMETIC', 'OTHER_FAULT')),
    claim_status         VARCHAR(16)  NOT NULL DEFAULT 'OPEN' CHECK (claim_status IN ('OPEN', 'IN_PROGRESS', 'RESOLVED', 'DENIED')),
    -- Was created_at TIMESTAMPTZ (REQ-WARR-03). A claim needs the day it was opened,
    -- to be tested against warranty_expires; it needs no time of day.
    opened_on            DATE         NOT NULL DEFAULT CURRENT_DATE,
    retain_until         DATE
);

-- Free-text notes on a claim, kept apart from identity (REQ-WARR-02 (c), (d)). The
-- table holds no identity field and no warranty_token: a note reaches a person only
-- through its claim, and only a holder of warranty_claim_notes_access can read it.
-- Whether notes are kept at all is the principal's (the brief, Q4).
CREATE TABLE warranty_claim_notes (
    id                   BIGSERIAL    PRIMARY KEY,
    claim_id             BIGINT       NOT NULL REFERENCES warranty_claims(id),
    note                 TEXT         NOT NULL,
    written_on           DATE         NOT NULL DEFAULT CURRENT_DATE,
    retain_until         DATE
);

CREATE INDEX idx_warranty_reg_token    ON warranty_registrations(warranty_token);
CREATE INDEX idx_warranty_reg_email    ON warranty_registrations(registered_email);
CREATE INDEX idx_warranty_claims_token ON warranty_claims(warranty_token);
CREATE INDEX idx_warranty_notes_claim  ON warranty_claim_notes(claim_id);

-- The notes table's own access role (REQ-WARR-02 (d)). No other role is granted on the
-- notes table, and this role is granted on nothing but the notes table and its sequence,
-- so a support role that reads registrations and claims does not read notes.
CREATE ROLE warranty_claim_notes_access NOLOGIN;
REVOKE ALL ON warranty_claim_notes FROM PUBLIC;
GRANT SELECT, INSERT ON warranty_claim_notes TO warranty_claim_notes_access;
GRANT USAGE ON SEQUENCE warranty_claim_notes_id_seq TO warranty_claim_notes_access;
