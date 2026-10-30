# Human-input requests

Documents here exist to **get an answer from a person**: a clinical ruling, a regulatory view, a
principal decision, a supplier confirmation. Each one poses a question an open item is waiting on,
gives the reviewer what they need to answer it, and says what to send back.

**They are not controlled documents and not design inputs.** They carry no `**Document:**` serial
and are not in `NP-DHF-001`. An answer takes effect only when it is recorded in the owning file and
logged in `docs/status/completed-decisions.md`. Never quote a draft here as a decision.

## Rules

1. **One file per open item**, named `<open-item-id-lowercase>-<short-topic>.md`.
2. **Start with the status line** (`DRAFT`, `SENT <date> to <name>`, `ANSWERED <date>`), the sender and the
   addressee. An unnamed addressee is stated as such.
3. **Ask a question the reviewer can answer yes or no or by picking an option**, then give the
   options and what each one costs. Say what is already known and what is assumed.
4. **Do not assume the answer.** If the file asks for a ruling, nothing in the repo may act as if it
   has been given.
5. **State what you need back**: the ruling, a short rationale (CLAUDE.md §18 wants a derivation),
   conditions that would reopen it, and the reviewer's name, role and date.
6. **Never include UHDR content or personal data** (CLAUDE.md §5).
7. **When answered**, record the answer in the owning file and in the decisions log, set the status
   line to `ANSWERED`, and leave the file in place as the trail.

## Index

Sorted by open item.

| Item | Document | Asks | Status |
|------|----------|------|--------|
| `OI-CHARGE-06` | `oi-charge-06-dc-charge-ceiling.md` | Regulatory: is the 150 mC/cm² DC ceiling acceptable for T1 wellness tDCS, does it hold for T2 HD-tDCS (it does not fit the 3.5 mm electrode as declared), and should a current-density criterion sit beside it? | DRAFT, ready to send |
| `OI-CONSENT-08` | `oi-consent-08-clinician-tiers.md` | Principal with a clinical adviser: which UHDR elements each clinician tier reaches, whether the tier sets stay derived, and whether the prices are decided. | DRAFT, ready to send |
| `OI-CVNS-13` | `oi-cvns-13-nuisance-cutoff-rate.md` | Clinical: acceptable false-cutoff rate. Engineering: measure missed and split R-peak rates on A13. Drives 25-c and 25-e. | DRAFT, not sent |
| `OI-I18N-04` | `oi-i18n-04-audit-string-translations.md` | Native-speaker verification of machine-drafted translations of 15 privacy-review strings in 9 locales (en-GB read-through). Drafts in `translation-drafts/`; nothing in `locales/`. | DRAFT, not sent |
| `OI-IFU-09` | `oi-ifu-09-body-contact-cleaning.md` | Regulatory + risk, with the principal for Q1: is a headset a single-person device; if shared use is allowed, which disinfection levels and agents; the nasal probe; the cervical gel pad's reuse. | DRAFT, ready to send; leans to option B on Q1 |
| `OI-NPPS-LIMITS-01` | `oi-npps-limits-01-stroke-protocol-30hz.md` | Clinical: the shipped taVNS stroke protocol authors 30 Hz against a 25 Hz ceiling. Re-cite to ≤25 Hz evidence, withdraw, or rule 25 Hz adequate? Waits on `OI-VNSCLIP-08`. | DRAFT, ready to send; recommends option A |
| `OI-NPPS-VNS-01` | `oi-npps-vns-01-right-and-bilateral-side.md` | Risk: allow right-ear and bilateral auricular VNS (refuse, allow with acknowledgement, or allow)? Literature summary from three full-text reviews and Hatik 2023; Kraus 2007 and Sinkovec 2021 abstract only; Hatik 2023 and Gauthey 2020 read in full. | DRAFT, ready to send; recommends option B |
| `OI-NPPS-VNS-01` (a) | `oi-npps-vns-01-movement-cue.md` | FW Hub + Safety + Clinical: where does the movement cue for paired stimulation come from (app, USB device, or a new hub input), what bounds the dose, and what must happen when a cue is missed, doubled or lost? | DRAFT, ready to send |
| `OI-PRES-06` | `oi-pres-06-android-device-run.md` | Run the Android presence gate on devices at API 29, 30 and 36 and report pass or fail per step. | DRAFT, not sent |
| `OI-PRES-07` | `oi-pres-07-ios-device-run.md` | Run the iOS presence gate on an iPhone, with the first device run of the shared iOS host (`OI-UI-KMP-06`), and report pass or fail per step. | DRAFT, not sent |
| `OI-PWRSRC-11` | `oi-pwrsrc-11-hub-hold-up.md` | Safety + EE: what the outputs do when the last power source goes (the headset has no battery), the acceptance criterion, the bench measurement, and what `IFU-01` may say. Gates the cable-pull instruction. | DRAFT, ready to send |
| `OI-RISK2-01` | `oi-risk2-01-rescore-carried-risks.md` | Commission the S × P re-score of the carried risks: scope, order, scorer and approver, provisional inputs, record form. | DRAFT, not sent |
| `OI-RISK2-02` | `oi-risk2-02-hazard-analysis-commission.md` | Commission the hazard analysis for A11, A12, A13 and A15: owners, register location, scope, order, DRAFT-spec handling. | DRAFT, not sent |
| `OI-RISK2-03` | `oi-risk2-03-second-reader.md` | Second reader: confirm or challenge the five RETIRED risk dispositions (five, per the table). | DRAFT, not sent |
| `OI-RISK2-04` | `oi-risk2-04-counsel-engagement.md` | Commission outside regulatory counsel on the PBM irradiance ceilings (RISK-03)? Four open choices. | DRAFT, not sent |
| `OI-RISK2-05` (closed) | `oi-risk2-05-closure-confirmation.md` | Confirm Quality Lead approval of the re-derived FMEA residuals and the host-only basis for P1. | DRAFT, not sent |
| `OI-RISK2-06` | `oi-risk2-06-clinical-question.md` | Should a cervical cardiac cutoff also withhold auricular VNS? | DRAFT, not sent |
| `OI-RISK2-07` | `oi-risk2-07-update-path-test.md` | Confirm the owed NV-pages-survive-update test, where it binds, and who carries it. Waits on the first safety-MCU update path. | DRAFT, not sent |
| `OI-RISK2-09` | `oi-risk2-09-cvns-detection-adequacy.md` | Is the `REQ-CVNS-09` cardiac detection requirement clinically adequate? Five sub-questions (a)–(e). | DRAFT, not sent |
| `OI-VNSCLIP-08` | `oi-vnsclip-08-frequency-ceiling.md` | Clinical + Risk: is the 25 Hz auricular VNS ceiling a hazard control or an equivalence bound? No source found states a safety reason for it. | ANSWERED 2026-10-29 (principal): option A; follow-up done |
| `OI-VNSCLIP-09` | `oi-vnsclip-09-current-ceiling.md` | Clinical + Risk: does the auricular VNS clip need a current ceiling, in what quantity, enforced where? The 40 mA figure is a stop-gap with no hazard named. | DRAFT, ready to send |
