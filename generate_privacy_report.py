"""
NeurOne Privacy Analysis Report Generator
NP-PRIV-001 Rev 1 — 2026-06-02
"""

from reportlab.lib.pagesizes import letter
from reportlab.lib.styles import getSampleStyleSheet, ParagraphStyle
from reportlab.lib.units import inch
from reportlab.lib import colors
from reportlab.platypus import (
    SimpleDocTemplate, Paragraph, Spacer, Table, TableStyle,
    HRFlowable, PageBreak, KeepTogether
)
from reportlab.lib.enums import TA_LEFT, TA_CENTER, TA_JUSTIFY
from datetime import date

OUTPUT = "docs/np_priv_001.pdf"

# ── Colour palette ────────────────────────────────────────────────────────────
C_CRITICAL  = colors.HexColor("#C0392B")
C_HIGH      = colors.HexColor("#E67E22")
C_MEDIUM    = colors.HexColor("#2980B9")
C_LOW       = colors.HexColor("#27AE60")
C_HEADER_BG = colors.HexColor("#1A1A2E")
C_SECTION   = colors.HexColor("#2C3E50")
C_RULE      = colors.HexColor("#BDC3C7")
C_GOOD_BG   = colors.HexColor("#EAF7EA")
C_CANT_BG   = colors.HexColor("#FEF9E7")
C_NEXT_BG   = colors.HexColor("#EBF5FB")

# ── Styles ────────────────────────────────────────────────────────────────────
def make_styles():
    base = getSampleStyleSheet()

    def s(name, **kw):
        return ParagraphStyle(name, parent=base["Normal"], **kw)

    return {
        "cover_title": s("cover_title",
            fontName="Helvetica-Bold", fontSize=26, textColor=colors.white,
            leading=32, spaceAfter=8),
        "cover_sub": s("cover_sub",
            fontName="Helvetica", fontSize=13, textColor=colors.HexColor("#BDC3C7"),
            leading=18, spaceAfter=4),
        "cover_meta": s("cover_meta",
            fontName="Helvetica", fontSize=10, textColor=colors.HexColor("#95A5A6"),
            leading=14),
        "section": s("section",
            fontName="Helvetica-Bold", fontSize=14, textColor=C_SECTION,
            spaceBefore=18, spaceAfter=6, leading=18),
        "finding_title": s("finding_title",
            fontName="Helvetica-Bold", fontSize=11, textColor=colors.white,
            leading=15),
        "label": s("label",
            fontName="Helvetica-Bold", fontSize=9, textColor=C_SECTION,
            spaceBefore=5, spaceAfter=1),
        "body": s("body",
            fontName="Helvetica", fontSize=9.5, textColor=colors.HexColor("#2C3E50"),
            leading=14, spaceAfter=4, alignment=TA_JUSTIFY),
        "bullet": s("bullet",
            fontName="Helvetica", fontSize=9.5, textColor=colors.HexColor("#2C3E50"),
            leading=14, leftIndent=12, spaceAfter=2),
        "code": s("code",
            fontName="Courier", fontSize=8.5, textColor=colors.HexColor("#2C3E50"),
            leading=13, leftIndent=12, backColor=colors.HexColor("#F4F6F7"),
            spaceAfter=4),
        "toc_item": s("toc_item",
            fontName="Helvetica", fontSize=9.5, textColor=colors.HexColor("#2C3E50"),
            leading=14, leftIndent=8),
        "good_body": s("good_body",
            fontName="Helvetica", fontSize=9.5, textColor=colors.HexColor("#1E8449"),
            leading=14, spaceAfter=3, leftIndent=8),
        "cant_body": s("cant_body",
            fontName="Helvetica", fontSize=9.5, textColor=colors.HexColor("#7D6608"),
            leading=14, spaceAfter=3, leftIndent=8),
        "next_body": s("next_body",
            fontName="Helvetica", fontSize=9.5, textColor=colors.HexColor("#1A5276"),
            leading=14, spaceAfter=3, leftIndent=8),
        "summary_body": s("summary_body",
            fontName="Helvetica", fontSize=10, textColor=colors.HexColor("#2C3E50"),
            leading=15, spaceAfter=5, alignment=TA_JUSTIFY),
    }

# ── Header / footer ───────────────────────────────────────────────────────────
def on_page(canvas, doc):
    canvas.saveState()
    w, h = letter
    # header bar
    canvas.setFillColor(C_HEADER_BG)
    canvas.rect(0, h - 28, w, 28, fill=1, stroke=0)
    canvas.setFont("Helvetica-Bold", 8)
    canvas.setFillColor(colors.white)
    canvas.drawString(0.5*inch, h - 18, "NeurOne — CONFIDENTIAL")
    canvas.drawRightString(w - 0.5*inch, h - 18, "NP-PRIV-001 Rev 1  |  2026-06-02")
    # footer
    canvas.setFillColor(C_RULE)
    canvas.rect(0.5*inch, 0.4*inch, w - inch, 0.5, fill=1, stroke=0)
    canvas.setFont("Helvetica", 7.5)
    canvas.setFillColor(colors.HexColor("#95A5A6"))
    canvas.drawString(0.5*inch, 0.25*inch, "Privacy Analysis and Repair  |  NeurOne Design Programme")
    canvas.drawRightString(w - 0.5*inch, 0.25*inch, f"Page {doc.page}")
    canvas.restoreState()

def on_first_page(canvas, doc):
    canvas.saveState()
    w, h = letter
    # full-bleed cover header band
    canvas.setFillColor(C_HEADER_BG)
    canvas.rect(0, h - 200, w, 200, fill=1, stroke=0)
    canvas.restoreState()

# ── Severity badge helper ─────────────────────────────────────────────────────
SEVERITY_COLORS = {
    "CRITICAL": C_CRITICAL,
    "HIGH":     C_HIGH,
    "MEDIUM":   C_MEDIUM,
    "LOW":      C_LOW,
}

def finding_block(st, severity, title, where, category, issue, reference, remediation):
    colour = SEVERITY_COLORS.get(severity, C_MEDIUM)
    elements = []

    # Title bar table
    badge_text = f"  {severity}  "
    title_data = [[
        Paragraph(badge_text, ParagraphStyle("badge",
            fontName="Helvetica-Bold", fontSize=9, textColor=colour,
            backColor=colors.white, leading=12)),
        Paragraph(title, st["finding_title"]),
    ]]
    title_table = Table(title_data, colWidths=[0.85*inch, 5.9*inch])
    title_table.setStyle(TableStyle([
        ("BACKGROUND", (0,0), (-1,-1), colour),
        ("BACKGROUND", (0,0), (0,0), colors.white),
        ("BOX",        (0,0), (0,0), 1.2, colour),
        ("VALIGN",     (0,0), (-1,-1), "MIDDLE"),
        ("LEFTPADDING",  (0,0), (0,0), 6),
        ("RIGHTPADDING", (0,0), (0,0), 6),
        ("TOPPADDING",   (0,0), (-1,-1), 6),
        ("BOTTOMPADDING",(0,0), (-1,-1), 6),
        ("LEFTPADDING",  (1,0), (1,0), 10),
    ]))

    # Detail rows
    def row(label, content):
        return [
            Paragraph(label, st["label"]),
            Paragraph(content, st["body"]),
        ]

    detail_data = [
        row("WHERE",       where),
        row("CATEGORY",    category),
        row("ISSUE",       issue),
        row("REFERENCE",   reference),
        row("REMEDIATION", remediation),
    ]
    detail_table = Table(detail_data, colWidths=[0.85*inch, 5.9*inch])
    detail_table.setStyle(TableStyle([
        ("BACKGROUND",   (0,0), (-1,-1), colors.HexColor("#FDFEFE")),
        ("BACKGROUND",   (0,0), (0,-1), colors.HexColor("#F2F3F4")),
        ("BOX",          (0,0), (-1,-1), 0.5, C_RULE),
        ("LINEBELOW",    (0,0), (-1,-2), 0.3, C_RULE),
        ("VALIGN",       (0,0), (-1,-1), "TOP"),
        ("LEFTPADDING",  (0,0), (-1,-1), 8),
        ("RIGHTPADDING", (0,0), (-1,-1), 8),
        ("TOPPADDING",   (0,0), (-1,-1), 5),
        ("BOTTOMPADDING",(0,0), (-1,-1), 5),
    ]))

    elements.append(KeepTogether([title_table, detail_table, Spacer(1, 10)]))
    return elements

# ── Content ───────────────────────────────────────────────────────────────────
def build_content(st):
    story = []
    SP = Spacer
    HR = lambda: HRFlowable(width="100%", thickness=0.5, color=C_RULE, spaceAfter=6)

    def section(text):
        story.append(SP(1, 8))
        story.append(Paragraph(text, st["section"]))
        story.append(HR())

    def body(text):
        story.append(Paragraph(text, st["body"]))

    def bullet(text):
        story.append(Paragraph(f"• {text}", st["bullet"]))

    def label(text):
        story.append(Paragraph(text, st["label"]))

    # ── Cover page ────────────────────────────────────────────────────────────
    story.append(SP(1, 140))  # push down into the dark band
    story.append(Paragraph("Privacy Analysis and Repair", st["cover_title"]))
    story.append(Paragraph("NeurOne Design Programme", st["cover_sub"]))
    story.append(SP(1, 16))
    story.append(Paragraph("NP-PRIV-001 Rev 1", st["cover_meta"]))
    story.append(Paragraph("Date: 2026-06-02", st["cover_meta"]))
    story.append(Paragraph("Classification: CONFIDENTIAL — Internal Use Only", st["cover_meta"]))
    story.append(Paragraph("Scope: CLAUDE.md Rev 14 · UHDR/SHDR architecture · Consent engine · Research data flows · Clinical platform", st["cover_meta"]))
    story.append(SP(1, 40))

    # Severity summary table on cover
    sev_data = [
        ["Severity", "Count", "Status"],
        ["CRITICAL", "2", "Action required immediately"],
        ["HIGH",     "6", "Resolve before T1 launch"],
        ["MEDIUM",   "7", "Resolve before T2 / EU launch"],
        ["LOW",      "3", "Address in next planning cycle"],
    ]
    sev_table = Table(sev_data, colWidths=[1.4*inch, 0.8*inch, 3.2*inch])
    sev_table.setStyle(TableStyle([
        ("BACKGROUND",   (0,0), (-1,0), C_HEADER_BG),
        ("TEXTCOLOR",    (0,0), (-1,0), colors.white),
        ("FONTNAME",     (0,0), (-1,0), "Helvetica-Bold"),
        ("FONTSIZE",     (0,0), (-1,-1), 9),
        ("BACKGROUND",   (0,1), (-1,1), colors.HexColor("#FADBD8")),
        ("BACKGROUND",   (0,2), (-1,2), colors.HexColor("#FDEBD0")),
        ("BACKGROUND",   (0,3), (-1,3), colors.HexColor("#D6EAF8")),
        ("BACKGROUND",   (0,4), (-1,4), colors.HexColor("#D5F5E3")),
        ("TEXTCOLOR",    (0,1), (0,1), C_CRITICAL),
        ("TEXTCOLOR",    (0,2), (0,2), C_HIGH),
        ("TEXTCOLOR",    (0,3), (0,3), C_MEDIUM),
        ("TEXTCOLOR",    (0,4), (0,4), C_LOW),
        ("FONTNAME",     (0,1), (0,-1), "Helvetica-Bold"),
        ("BOX",          (0,0), (-1,-1), 0.5, C_RULE),
        ("LINEBELOW",    (0,0), (-1,-2), 0.3, C_RULE),
        ("ALIGN",        (1,0), (1,-1), "CENTER"),
        ("VALIGN",       (0,0), (-1,-1), "MIDDLE"),
        ("LEFTPADDING",  (0,0), (-1,-1), 10),
        ("TOPPADDING",   (0,0), (-1,-1), 6),
        ("BOTTOMPADDING",(0,0), (-1,-1), 6),
    ]))
    story.append(sev_table)
    story.append(PageBreak())

    # ── Summary ───────────────────────────────────────────────────────────────
    section("Summary")
    body("The NeurOne privacy architecture is more sophisticated than almost any consumer health device reviewed. The UHDR/SHDR separation, biometric-key encryption, on-device anonymisation, and use-case-based clinical consent engine are genuine structural strengths. The 18 findings below are therefore concentrated in gaps and edge cases rather than foundations.")
    story.append(SP(1,4))
    body("<b>Jurisdictions in scope:</b> United States primary (HIPAA, FTC HBNR, CCPA/CPRA, FTC Act §5, state breach laws); EU/EEA secondary (GDPR, ePrivacy Directive) — EU market is anticipated and the charger policy already addresses it.")
    story.append(SP(1,4))
    body("<b>Highest-leverage single change:</b> Resolve the Warranty Owner ID Critical finding before any external beta or warranty registration goes live. If SHDR is effectively person-linked, the UHDR/SHDR privacy firewall — the core architectural claim — is undermined.")

    # ── CRITICAL ──────────────────────────────────────────────────────────────
    section("Critical Findings")

    story += finding_block(st,
        "CRITICAL",
        "Warranty Owner ID re-identifies SHDR",
        "CLAUDE.md §5.1 — SHDR definition; §5.1 boundary resolution table; NP-FW-EMMC-001 Rev 1 §7",
        "Pure privacy failure",
        "SHDR is documented as 'linked to device ID + warranty owner ID only — never to user identity.' "
        "But warranty registration collects name, email, postal address, and serial number. "
        "If 'warranty owner ID' is a pointer into that table, SHDR is person-linked and the privacy "
        "firewall collapses. NeurOne would hold session counts, LED usage ratios, temperature "
        "profiles, consumable patterns, and impact events tied to a named individual — health data by "
        "inference. Under HIPAA this likely qualifies as PHI; under GDPR Art. 9 inferrable health data "
        "requires explicit consent.",
        "Linkable Identifiers anti-pattern; Inference Without Consent anti-pattern; "
        "HIPAA Privacy Rule Minimum Necessary; GDPR Art. 9 (special-category data); LINDDUN — Identifiability",
        "Introduce an opaque 256-bit warranty token. Store (token → name, email, address) in the "
        "warranty system only. Store only the token in SHDR. The warranty database and SHDR fleet "
        "database are never joined in production. Add a SHDR boundary test: 'Could NeurOne identify "
        "a named individual from SHDR plus the warranty registration table?' If yes → the data is UHDR. "
        "Document in NP-FW-EMMC-001 Rev 2 and NP-RM-001."
    )

    story += finding_block(st,
        "CRITICAL",
        "No documented breach detection or response plan",
        "CLAUDE.md §13 (absent); NP-QMS-CAPA-001 Rev 1 — regulatory reporting mentioned but no breach runbook",
        "Pure privacy failure",
        "The QMS documents CAPA triggers but there is no breach detection, containment, or notification "
        "runbook. Hard regulatory deadlines apply: HIPAA Breach Notification Rule — 60 days from "
        "discovery for individual notification; FTC HBNR (applies to T1 as non-HIPAA wellness device) — "
        "updated 2024 to cover more PHR-adjacent vendors; US state laws — 30–90 days across all 50 "
        "states; GDPR Art. 33–34 — 72 hours to supervisory authority, high-risk breaches require "
        "individual notification without undue delay. A first breach response without a runbook will "
        "exhaust the clock.",
        "OWASP Top 10 Privacy Risks P3; Data Breach Notification Pattern; HIPAA 45 CFR §164.400–414; "
        "FTC HBNR 16 CFR Part 318; GDPR Art. 33–34",
        "Author NP-SEC-BR-001 (Breach Response Plan) as a Month 3 deliverable. Minimum contents: "
        "detection signals (anomalous SHDR upload volumes, unusual admin access to warranty DB); "
        "severity tiers triggering HIPAA / HBNR / GDPR Art. 33; escalation chain with named roles; "
        "pre-drafted notification templates; forensic log retention confirmed to outlast the longest "
        "notification window (HIPAA: 6 years); annual tabletop exercise before T1 launch."
    )

    # ── HIGH ──────────────────────────────────────────────────────────────────
    section("High Findings")

    story += finding_block(st,
        "HIGH",
        "SHDR impact-event data enables motor-health inference",
        "CLAUDE.md §5.1 boundary resolution table — 'impact events (g-force, orientation — between sessions only) → SHDR'",
        "Pure privacy failure",
        "The boundary rule assigns between-session impact events to SHDR on the logic that they reveal "
        "device condition rather than user biology. This fails the project's own test: 'does this tell us "
        "something about the person?' A user who drops the device repeatedly, or has a consistent "
        "high-g drop signature, is revealing motor control information. Parkinson's, essential tremor, "
        "post-stroke hemiplegia, and MS all manifest in device drop patterns. Combined with the warranty "
        "owner ID (Critical finding), NeurOne would hold health-inferrable data on named individuals.",
        "Inference Without Consent anti-pattern; Aggregation Creep anti-pattern; LINDDUN — Identifiability; "
        "GDPR Art. 9; FTC Act §5",
        "Option A (quick): coarsen to binary flag (drop_detected: true/false, no g-force value or "
        "orientation). Option B (recommended): move raw impact-event data to UHDR; expose only a "
        "derived SHDR metric — 'drop rate above maintenance-alert threshold: true/false.' "
        "Option C (strongest): on-device processing only, emit the binary alert to SHDR, "
        "raw accelerometer series never leaves the device. Adopt Option B as minimum."
    )

    story += finding_block(st,
        "HIGH",
        "k-anonymity alone is insufficient for research data",
        "CLAUDE.md §5.3 — 'k≥10 anonymisation'; §6.2 — L3 blanket consent; §6.3 — study descriptor",
        "Pure privacy failure",
        "The research anonymisation relies on k-anonymity (k≥10) with date-rounding and quasi-identifier "
        "suppression. k-anonymity has two known critical vulnerabilities: (1) homogeneity attack — if all "
        "10+ individuals share the same EEG band-power profile, the value is revealed exactly; "
        "(2) background knowledge attack — if an adversary knows a target person is in a study, they can "
        "infer a diagnosis even without a direct identifier. EEG patterns are biometric and are as "
        "identifying as fingerprints over time.",
        "k-anonymity failure modes (Sweeney 2002, Machanavajjhala 2006); Re-identification via Join "
        "anti-pattern; AOL search logs (2006); Netflix Prize (2007); GDPR Recital 26",
        "Upgrade to l-diversity (l≥3) in addition to k-anonymity. For any EEG waveform extract, "
        "require differential privacy (epsilon ≤ 1.0, delta ≤ 10e-5) via calibrated Laplace or Gaussian "
        "noise applied to spectral features. Prohibit raw EEG waveforms from research extracts "
        "regardless of k — only derived features with DP noise. Add language to L3 and per-project "
        "consent that 'anonymisation means statistical protection, not mathematical certainty' and "
        "names EEG as biometric-adjacent. Reference: NIST SP 800-226."
    )

    story += finding_block(st,
        "HIGH",
        "UHDR key lifecycle gap: biometric revocation and device transfer",
        "CLAUDE.md §5.1 UHDR encryption spec; NP-FW-EMMC-001 Rev 1 §6",
        "Pure privacy failure + security failure",
        "Two lifecycle gaps are unaddressed. (1) Biometric compromise or change: Argon2id is "
        "deterministic — if the biometric template changes, the derived key changes and previously "
        "encrypted UHDR becomes inaccessible. No key-rotation path or fallback is specified. "
        "(2) Device resale: if a user sells the device, UHDR ciphertext remains on the device, SHDR "
        "persists with the old user's session history, and the new owner may register a new warranty "
        "token on top of the old user's hardware fingerprint.",
        "Permanent Storage anti-pattern; GDPR Art. 17 (erasure must extend to all copies including "
        "device); HIPAA Security Rule §164.310(d)(1) — media and device disposal",
        "Key rotation: document in NP-FW-EMMC-001 a two-layer key scheme — UHDR master key (random, "
        "stored in Config partition encrypted with Argon2id key) so biometric change only re-encrypts "
        "the wrapper, not the full partition. Device transfer flow: (a) user initiates reset in app, "
        "(b) UHDR partition overwritten with eMMC SANITIZE, (c) SHDR reset to fresh device-ID-only "
        "state with session counter zeroed, (d) warranty owner token cleared from Config partition, "
        "(e) device generates new HKDF-based SHDR key with new TRNG salt."
    )

    story += finding_block(st,
        "HIGH",
        "Mode F autonomous retinal PBM — consent capture undefined",
        "CLAUDE.md §3 Visual Stimulation — 'Mode F (invisible NIR retinal walk): 808-830nm daily retinal PBM during normal-looking wear'",
        "Pure privacy failure (consent gap)",
        "Mode F delivers therapeutic PBM to the retina during 'normal-looking wear' — while the goggles "
        "appear inactive. This is the highest-sensitivity modality from a consent standpoint: it operates "
        "without an obvious session boundary, delivers NIR radiation to the retina (the one modality with "
        "a hardware MPE limit under IEC 62471), and generates UHDR data (eye-open/closed state) during "
        "apparently ambient wear. No documented consent capture exists for Mode F specifically.",
        "Obtaining Explicit Consent pattern; GDPR Art. 7 (consent must be specific and informed); "
        "GDPR Art. 9(2)(a) (explicit consent for health data); HIPAA Minimum Necessary; "
        "IEC 62471 (retinal MPE)",
        "Add Mode F as an explicitly named, separately consented feature. Default: OFF. "
        "User must take a deliberate action to enable it (Cavoukian Principle 2: Privacy as Default). "
        "Add a persistent ambient indicator when Mode F is active — e.g. the right temple status LED "
        "breathes a distinct pattern (per §4.7). Clarify in NP-FW-EMMC-001 §12 how Mode F data is "
        "classified and stamped in the session record."
    )

    story += finding_block(st,
        "HIGH",
        "Clinician consent revocation path not specified",
        "CLAUDE.md §6.1 — 'per-element, per-use-case, time-limited, audited, revocable'",
        "Pure privacy failure",
        "The consent engine states clinician access is 'revocable' but no mechanism is documented. "
        "Open questions: What happens to UHDR data already reviewed when a patient revokes? "
        "Can a patient revoke mid-subscription? Is revocation immediate or at the next billing cycle? "
        "Is the API access token invalidated immediately? For T2 multi-patient dashboards, are historical "
        "records removed from the dashboard view on revocation?",
        "Enable/Disable Functions pattern; GDPR Art. 7(3) — withdrawal must be as easy as granting "
        "consent and immediately effective; HIPAA §164.522 (individual right to restrict); "
        "Selective Access Control pattern",
        "Define revocation as an API-level capability — token invalidation within seconds of user action. "
        "Document the clinician UX when a patient revokes (record greyed/removed; data in clinician "
        "possession subject to their own HIPAA BAA obligations). Add a revocation log to SHDR "
        "(no UHDR content — just revocation flag, timestamp, tier). Clarify billing position in the "
        "BAA template: revocation terminates access regardless of remaining subscription term."
    )

    story += finding_block(st,
        "HIGH",
        "POA document upload is an unspecified high-risk data flow",
        "CLAUDE.md §6.2 — 'POA holder uploads executed healthcare POA → human review 3 business days'",
        "Security failure + pure privacy failure",
        "A healthcare Power of Attorney document contains the grantor's legal name, date of birth, "
        "address, health conditions that triggered the POA, and the identity of the attorney-in-fact. "
        "The upload flow is described in one sentence with no specification for: how the document is "
        "transmitted; where it is stored; which NeurOne employees can access it; retention period; "
        "whether document content is indexed; or what happens on acquisition. This is a God View risk "
        "for the POA processing team.",
        "God View anti-pattern; Least Privilege pattern; Permanent Storage anti-pattern; "
        "GDPR Art. 9(2)(c) (vital interests — specific safeguards required); "
        "HIPAA Security Rule §164.312 (access controls for sensitive PHI)",
        "Require end-to-end encrypted upload via signed URL (document goes directly to encrypted "
        "storage, not through application servers). Separate vault with field-level encryption; access "
        "restricted to named reviewers only (2-3 people max); every access logged. "
        "Delete the original document within 30 days of human review; retain only structured review "
        "output (jurisdiction code, scope summary, expiry date, verification timestamp). "
        "Author NP-PROC-POA-001 before T1 launch."
    )

    # ── MEDIUM ────────────────────────────────────────────────────────────────
    section("Medium Findings")

    story += finding_block(st,
        "MEDIUM",
        "FHIR R4 minimum population not bounded",
        "CLAUDE.md §3 T2 — 'HIPAA cloud + EHR: FHIR R4'; LSL streaming",
        "Pure privacy failure",
        "FHIR R4 Patient resources support name, DOB, address, phone, email, SSN, race, ethnicity, "
        "and more. Without a NeurOne FHIR profile specifying exactly which resource types and fields "
        "are populated, implementations default to 'populate what the spec allows' — bringing substantial "
        "unnecessary PII into T2. Additionally, LSL (Lab Streaming Layer) has no native encryption or "
        "authentication and streams on the local network in plaintext. In a clinical environment, an "
        "unencrypted LSL stream carrying real-time EEG or HRV from a named patient is a significant "
        "exposure.",
        "Collect Now, Decide Later anti-pattern; HIPAA Minimum Necessary Rule 45 CFR §164.502(b); "
        "HL7 FHIR R4 Privacy Module §3.3; LINDDUN — Disclosure of information",
        "Define a NeurOne FHIR ImplementationGuide before any T2 EHR integration is built. "
        "Permitted resources: Patient, Observation, DiagnosticReport, Procedure. Patient.identifier: "
        "opaque clinic-assigned MRN only. Patient.birthDate: year-only if clinically required. "
        "Explicitly prohibit: Patient.name, telecom, address, photo, contact. "
        "For LSL: require TLS tunnelling (stunnel or WireGuard VPN); state explicitly in setup guide "
        "and BAA that unencrypted LSL is not HIPAA-compliant. Add G3-09 to NP-COORD-001 gating "
        "T2 EHR integration on FHIR profile approval."
    )

    story += finding_block(st,
        "MEDIUM",
        "Scripting API data access scope undefined",
        "CLAUDE.md §2.1 Pro Full — 'scripting API'; §3 T2 — 'scripting API'",
        "Pure privacy failure",
        "The scripting API is a Pro Full feature with no specification of authentication model, data "
        "access scope, rate limits, or audit logging. In a multi-patient clinical environment, an API "
        "without explicit UHDR gating is a God View risk: a single compromised API key could enumerate "
        "sessions and access EEG or HRV features across every patient linked to the clinic account. "
        "This is OWASP API Security Top 10 items API1 (Broken Object Level Authorization) and API3 "
        "(Excessive Data Exposure) by design.",
        "God View anti-pattern; Least Privilege pattern; OWASP API Security Top 10 API1, API3; "
        "HIPAA Security Rule §164.312(a)(1) — access controls; HIPAA §164.312(b) — audit controls",
        "SHDR-only by default (device telemetry, session counts, protocol parameters). UHDR-derived "
        "data requires: active clinical consent grant at Assess or Full Clinical tier; explicit API "
        "access toggle in the consent engine (not inherited from dashboard access); data type within "
        "consented use-case element list. Audit log: every patient-data API call logs timestamp, key "
        "fingerprint, patient token, resource type, response byte count; retained 6 years. "
        "Rate limits: 1,000 patient-record requests/hour, 10,000/day. 256-bit random keys, 90-day "
        "expiry. Author NP-API-001 as T2 pre-launch deliverable."
    )

    story += finding_block(st,
        "MEDIUM",
        "Apple Watch sync app HealthKit access scope unspecified",
        "CLAUDE.md §3b Apple Watch Sync App; NP-APP-ROADMAP-001 Rev 1 Phases 1-4",
        "Pure privacy failure",
        "The Watch app reads HRV data and potentially other HealthKit quantities. The roadmap does not "
        "specify which HealthKit types are requested. If the app follows a broad permission model, it "
        "would hold a far richer health profile than any session warrants — including sleep stages, "
        "workout history, blood oxygen, ECG readings, reproductive health, and more. "
        "Apple App Store Guideline 5.1.1(iv) prohibits using HealthKit data for user profiling. "
        "FTC enforcement against GoodRx (2023), BetterHelp (2023), and Premom (2023) establishes "
        "that sharing health app analytics without adequate disclosure is unfair regardless of "
        "HIPAA coverage.",
        "Collect Now, Decide Later anti-pattern; Apple HealthKit Review Guideline 5.1.1(iv); "
        "FTC Act §5 (GoodRx, BetterHelp, Premom precedents); GDPR Art. 9 (health data)",
        "Phase 1 HealthKit permissions: HKQuantityTypeIdentifierHeartRateVariabilitySDNN and "
        "HKQuantityTypeIdentifierHeartRate only. No other types. HealthKit data stays on Watch/iPhone; "
        "used for real-time display only; not transmitted to NeurOne servers; not persisted beyond "
        "the session. Complete App Privacy Nutrition Label accurately before App Store submission. "
        "Add OI-WA-06: HealthKit permission review and privacy declaration sign-off as Phase 1 milestone."
    )

    story += finding_block(st,
        "MEDIUM",
        "Research Scratch partition anonymisation window",
        "NP-FW-EMMC-001 Rev 1 §15; CLAUDE.md §5.1 — 'Scratch: raw blocks, no filesystem, no encryption'",
        "Security failure",
        "The research anonymisation pipeline uses the Scratch partition as a working area. Scratch is "
        "explicitly unencrypted. Three exploitable windows: (1) unexpected power loss mid-anonymisation "
        "leaves plaintext UHDR data on Scratch until next boot; (2) cold-boot attack — NAND retains "
        "charge briefly after power-off; (3) partial eMMC block-level reads can access Scratch without "
        "decrypting UHDR since they share the same physical medium. For a device whose core security "
        "claim is 'NeurOne cannot read UHDR,' a cleartext UHDR window on the device is a meaningful "
        "vulnerability if a hostile party takes physical possession.",
        "OWASP Top 10 Privacy Risks P4; Cavoukian Principle 5 (end-to-end security); "
        "GDPR Art. 32; NIST SP 800-111 (storage encryption for portable devices)",
        "Derive a one-time session key K_scratch = AES-256(TRNG_256, session_nonce) before the "
        "anonymisation task begins. Write all working data to Scratch using AES-256-CTR with K_scratch. "
        "Store K_scratch only in on-chip SRAM; zero with memset_explicit on completion or reset. "
        "Design the pipeline as an atomic transaction — interrupted runs are discarded, never partially "
        "transmitted. Issue eMMC SANITIZE (CMD38) on Scratch blocks after extract is written and "
        "verified. Document in NP-FW-EMMC-001 Rev 2 §15."
    )

    story += finding_block(st,
        "MEDIUM",
        "EDF+ patient header field handling",
        "NP-FW-EMMC-001 Rev 1 §6 — session data stored as EDF+; CLAUDE.md §5.1 UHDR file structure",
        "Pure privacy failure",
        "EDF+ file headers (first 256 bytes) contain mandatory plaintext fields: local patient "
        "identification (name, sex, DOB, patient code) and local recording identification (start date, "
        "hospital code, technician, equipment). If NeurOne follows clinical convention, every UHDR "
        "EDF+ file contains plaintext PII even though the partition is AES-256-XTS encrypted. When a "
        "user exports EDF+ to share with a clinician, they may inadvertently export name and DOB "
        "embedded in the header. Research anonymisation that strips data fields but not EDF+ headers "
        "is the Fake Anonymisation anti-pattern applied to file metadata.",
        "Strip Invisible Metadata pattern; Fake Anonymisation anti-pattern; GDPR Art. 5(1)(c) data "
        "minimisation; HIPAA Minimum Necessary; EDF+ specification §2.1",
        "Define NeurOne EDF+ header policy in NP-FW-EMMC-001 Rev 2 §6. Patient code: opaque "
        "UHDR_TOKEN (16 chars). Sex: X (never populated). Birthdate: X. Patient name: X. "
        "Recording start date: preserved. Hospital code: 'NeurOne'. Technician: X. "
        "Equipment: 'NeurOne_v[FW_VER]'. For research anonymisation: add a header validation step "
        "that fails-closed if any non-X value appears in identity fields. For user export: "
        "display modal confirming file does not contain name or DOB."
    )

    story += finding_block(st,
        "MEDIUM",
        "Clinician consent revocation cascade to third-party processors not specified",
        "CLAUDE.md §5.1 UHDR backup; §6.1 — 'time-limited, audited, revocable'; §6.3 researcher data flow",
        "Pure privacy failure",
        "The consent engine describes user control over data at the device and NeurOne server level. "
        "But data shared with clinicians and researchers has no documented deletion-cascade path. "
        "When a user revokes clinician consent, NeurOne's access grant is terminated — but the "
        "clinician's own EHR system may retain UHDR-derived data indefinitely. "
        "This is the Sticky Third Parties anti-pattern: the user's data adheres to every system "
        "that touched it, and revocation only stops new flows.",
        "Sticky Third Parties anti-pattern; Obligation Management pattern; GDPR Art. 17(2) "
        "(controller must inform processors of erasure requests); HIPAA §164.528 (accounting of disclosures)",
        "Clinician BAA must include: 'On notification of patient consent withdrawal, clinician will "
        "delete all copies of NeurOne-sourced patient data from their EHR and derived records "
        "within 30 days.' User-facing disclosure at revocation: NeurOne cannot delete data already "
        "in the clinic's system; they are contractually required to delete within 30 days. "
        "The per-user SHDR audit log (already specified) should list contributed studies; app Privacy "
        "Dashboard shows each entry with study ID, date, data elements — implementing the Privacy "
        "Dashboard pattern for research data."
    )

    # ── LOW ───────────────────────────────────────────────────────────────────
    section("Low Findings")

    story += finding_block(st,
        "LOW",
        "App telemetry and crash reporting scope unspecified",
        "CLAUDE.md §10 iOS/Android app — Class B software; NP-SW-001 Rev 1",
        "Pure privacy failure",
        "The iOS/Android app will ship with analytics and crash reporting, but neither vendor selection "
        "nor data scope is documented. Analytics event names reconstruct health-management behaviour "
        "(session_started, modality_toggled:tDCS, protocol_selected:depression). Crash reporters "
        "capture stack-local variables that may include session parameters or EEG state. If SDKs "
        "initialise before consent is captured, data is collected pre-consent. FTC enforcement "
        "against GoodRx (2023) and BetterHelp (2023) established that sharing health-app analytics "
        "with analytics vendors constitutes an HBNR breach even without names.",
        "Third-party Free-for-all anti-pattern; Full Payload Logging anti-pattern; "
        "GDPR Art. 7 (consent timing); FTC HBNR 16 CFR §318.3; GoodRx and BetterHelp FTC actions (2023)",
        "Author NP-APP-TELEMETRY-001 before any SDK is added to the codebase. Contents: approved "
        "analytics vendor (maximum one, must provide DPA and BAA); approved crash reporter (maximum one, "
        "must support redacted payload mode); prohibited event properties (any health-inferrable string, "
        "modality name, protocol name, UHDR token); permitted properties (app version, OS version, "
        "coarsened device model, anonymised local session counter). SDK initialisation gate: SDKs "
        "initialise only after consent flow is complete on first launch. Crash reporter: disable "
        "automatic payload capture; verify configuration by intentionally crashing in staging."
    )

    story += finding_block(st,
        "LOW",
        "EU cross-border data transfer mechanism not specified",
        "CLAUDE.md §10 regulatory strategy — US only; §5.2 SHDR fleet telemetry; §6.1 T2 HIPAA cloud; §2.2 EU charger note",
        "Pure privacy failure (EU jurisdiction)",
        "The regulatory strategy covers FDA pathways only. The EU charger note and global consumer "
        "positioning make clear EU users are anticipated. SHDR fleet uploads, warranty registration, "
        "and T2 clinical cloud serving EU users with US-hosted infrastructure constitute cross-border "
        "personal data transfers under GDPR Chapter V. Without a valid transfer mechanism, each "
        "transfer is unlawful under GDPR Art. 44. Transfers of health data require not just a Chapter V "
        "mechanism but confirmation that the third country provides essentially equivalent protection.",
        "GDPR Art. 44–49 (transfers to third countries); GDPR Art. 9 (health data transfer safeguards); "
        "EU-US Data Privacy Framework — effective July 2023; SCCs EC Implementing Decision 2021/914",
        "Before EU T1 launch: add an EU regulatory section to CLAUDE.md and to NP-REG-001 (to be "
        "created). Fastest path: enroll in the EU-US Data Privacy Framework via self-certification on "
        "the DoC DPF website (4-6 weeks, minimal cost). Update privacy policy to reference DPF "
        "participation. For T2 clinical cloud: offer an EU-hosted deployment option (EEA cloud region) "
        "eliminating the transfer question for clinical data — also a competitive advantage with EU "
        "clinical buyers. Add GDPR Art. 13(1)(f) cross-border transfer disclosure to the app "
        "privacy notice."
    )

    story += finding_block(st,
        "LOW",
        "Apple Watch sync app HealthKit data residency",
        "CLAUDE.md §3b Apple Watch Sync App — Phases 1-4; NP-APP-ROADMAP-001 Rev 1",
        "Pure privacy failure",
        "Related to the HealthKit finding above: the roadmap does not explicitly state that HealthKit "
        "data accessed on the Watch stays on the Watch/iPhone and is not transmitted to NeurOne "
        "servers. Apple's guidelines require this, but it needs to be a binding engineering constraint "
        "documented in the spec — not just assumed. A future feature request to 'sync HRV trends to "
        "the NeurOne cloud' would constitute a HealthKit data transfer subject to both Apple "
        "guidelines and FTC HBNR.",
        "Apple HealthKit Review Guideline 5.1.1(iv); FTC HBNR 16 CFR §318.3; "
        "User-data confinement pattern",
        "Add to NP-APP-ROADMAP-001 Rev 2 a binding data residency constraint for all HealthKit data: "
        "'HealthKit data accessed by the Watch app is used for real-time session display only. "
        "It is not persisted, not cached, not transmitted to NeurOne servers or any third party, "
        "and not used for any purpose outside the active session.' Any future proposal to transmit "
        "HealthKit data requires a design change order with legal review and updated App Privacy "
        "Nutrition Label before implementation."
    )

    # ── What looks good ───────────────────────────────────────────────────────
    section("What Looks Good")
    good_items = [
        ("UHDR/SHDR separation is architecturally sound.",
         "User-owned, biometric-key-encrypted health data the manufacturer cannot decrypt, separated "
         "from device-owned telemetry. Very few medical device companies implement this at the schema "
         "and firmware level."),
        ("On-device research anonymisation is correct-by-design.",
         "NeurOne never holds the decryption key and anonymisation happens on-device before any "
         "extract leaves. This makes consent withdrawal genuinely effective — rare and right."),
        ("Boundary resolution rule has teeth.",
         "'When in doubt → UHDR' is the only default that actually protects users and making it a "
         "design principle — not just a policy page — is the right instinct."),
        ("The 4-layer research consent model is thoughtful.",
         "The distinction between contact consent (L1), category consent (L2), blanket consent (L3), "
         "and results opt-in (L4) maps well to the progressive nature of research relationships. "
         "The irreversibility notice at both L3 and per-project invitation is legally accurate."),
        ("Use-case-based clinical consent is better than field-based.",
         "Having clinicians select use cases with the system determining minimum necessary elements "
         "is a correct application of the HIPAA Minimum Necessary Rule."),
        ("SHDR fleet telemetry predictive maintenance model is legitimately privacy-preserving.",
         "Routing LED degradation ratios and temperature profiles to SHDR (not UHDR) while using "
         "fleet-level ML for maintenance predictions is the 'send the answer, not the data' architecture."),
        ("Consent withdrawal's forward-effectiveness guarantee is documented accurately.",
         "The disclosure that published study data cannot be retracted but withdrawal stops all future "
         "flows — including from historical sessions — is both legally accurate and user-honest."),
    ]
    for title, detail in good_items:
        story.append(KeepTogether([
            Paragraph(f"✓  <b>{title}</b>", st["good_body"]),
            Paragraph(f"   {detail}", st["body"]),
            Spacer(1, 4),
        ]))

    # ── What you couldn't review ──────────────────────────────────────────────
    section("What You Couldn't Review")
    body("The following areas were not visible in the current documentation and represent gaps in this review's coverage.")
    story.append(Spacer(1, 6))
    cant_items = [
        ("iOS/Android app source code or API spec",
         "All app-side findings are inferred from the roadmap. Actual telemetry configuration, "
         "third-party SDK list, cookie/storage handling, and consent flow implementation are unreviewed."),
        ("Warranty registration system",
         "The Critical finding on warranty owner ID depends on how that system is built. The warranty "
         "database schema, access controls, retention, and join capabilities with the SHDR fleet "
         "database are not in scope of what was reviewed."),
        ("Clinical cloud architecture (T2)",
         "The HIPAA cloud, FHIR R4 implementation, multi-patient dashboard, and scripting API are "
         "described at the feature level. The actual data model, access control implementation, "
         "and audit logging are unreviewed."),
        ("Vendor contracts",
         "No DPAs, BAAs, or processor agreements were reviewed. The privacy quality of the "
         "architecture is partially a function of the contractual obligations on every vendor "
         "who touches NeurOne data."),
        ("Penetration test / threat model output",
         "Security findings are based on architectural inference. A STRIDE+LINDDUN threat model "
         "pass on the hub firmware and clinical API, and an application penetration test on the "
         "iOS app and T2 cloud, would surface findings this review cannot."),
    ]
    for title, detail in cant_items:
        story.append(KeepTogether([
            Paragraph(f"⚠  <b>{title}</b>", st["cant_body"]),
            Paragraph(f"   {detail}", st["body"]),
            Spacer(1, 4),
        ]))

    # ── Recommended next steps ────────────────────────────────────────────────
    section("Recommended Next Steps")
    next_items = [
        ("1", "Resolve Warranty Owner ID (Critical) — before any external beta or warranty registration",
         "~1 week engineering, 1 week legal. Highest regulatory exposure. Define the opaque token "
         "architecture and confirm in NP-FW-EMMC-001 Rev 2."),
        ("2", "Author NP-SEC-BR-001 (Breach Response Plan) — Month 3 deliverable",
         "~3 days plus legal input. Hard deadlines (72 hours GDPR, 60 days HIPAA, 30 days most US "
         "state laws) cannot be met without a pre-existing runbook. Pre-T1-launch blocker."),
        ("3", "Author NP-PROC-POA-001 (POA Upload Procedure) — before research consent infrastructure goes live",
         "~1 week. Encrypted upload, restricted-access vault, 30-day document deletion policy. "
         "The POA flow handles the most sensitive documents in the system with no documented controls."),
        ("4", "Upgrade research anonymisation to l-diversity + differential privacy",
         "~2-4 weeks engineering in the on-device anonymisation engine. Protects against EEG "
         "re-identification attacks that k-anonymity alone cannot prevent."),
        ("5", "Define Mode F as explicitly consented, separately toggled, default-off",
         "~1 sprint. Highest consent-gap finding in the modality stack; also touches retinal safety "
         "(IEC 62471). Default-off with ambient LED indicator is both the right privacy posture and "
         "the defensible regulatory posture."),
    ]
    for num, title, detail in next_items:
        badge = Paragraph(f" {num} ", ParagraphStyle("nbadge",
            fontName="Helvetica-Bold", fontSize=10, textColor=colors.white,
            backColor=C_HEADER_BG, leading=14))
        row_data = [[badge, Paragraph(f"<b>{title}</b><br/>{detail}", st["next_body"])]]
        t = Table(row_data, colWidths=[0.35*inch, 6.4*inch])
        t.setStyle(TableStyle([
            ("BACKGROUND",   (0,0), (-1,-1), C_NEXT_BG),
            ("VALIGN",       (0,0), (-1,-1), "TOP"),
            ("LEFTPADDING",  (0,0), (0,0), 4),
            ("LEFTPADDING",  (1,0), (1,0), 10),
            ("TOPPADDING",   (0,0), (-1,-1), 7),
            ("BOTTOMPADDING",(0,0), (-1,-1), 7),
            ("BOX",          (0,0), (-1,-1), 0.5, C_RULE),
        ]))
        story.append(t)
        story.append(Spacer(1, 5))

    return story

# ── Build ─────────────────────────────────────────────────────────────────────
def main():
    st = make_styles()
    doc = SimpleDocTemplate(
        OUTPUT,
        pagesize=letter,
        leftMargin=0.55*inch,
        rightMargin=0.55*inch,
        topMargin=0.55*inch,
        bottomMargin=0.6*inch,
        title="NeurOne Privacy Analysis — NP-PRIV-001 Rev 1",
        author="PAI / SmartyPants",
        subject="Privacy Analysis and Repair",
    )
    story = build_content(st)
    doc.build(story, onFirstPage=on_first_page, onLaterPages=on_page)
    print(f"✓  Written: {OUTPUT}")

if __name__ == "__main__":
    main()
