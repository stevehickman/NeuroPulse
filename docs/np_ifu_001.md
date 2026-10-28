# Instructions for Use — Working Text

**Project:** NeurOne
**Document:** NP-IFU-001
**Revision:** 1
**Date:** 2026-10-28
**Status:** DRAFT — working text for the user-facing instructions; **not released, and nothing here may be shown to a user until the release gate on its row is met**
**Effective Date:** —
**Author:** NeurOne Systems Engineering
**Approved By:** — (pending)
**References:** `NP-QMS-DC-001` (labelling and IFU listed as *TBD*); `NP-SW-001` §6.3.6 (`SWR-SS-05`, `SWR-SS-06`, `SWR-SS-08`); `NP-PWRSRC-001` §10, `RISK-PWRSRC-01`, `OI-PWRSRC-11`; `OI-UI-KMP-09` in `docs/status/pending-decisions.md`; `NP-FW-HUB-001` §6.6 and `OI-FWHUB-13`
**Related Issues:** `OI-UI-KMP-09`
**Gate:** None. Labelling is a Year 1 item in `NP-QMS-DC-001`
**IEC 62304 Class:** —

---

> **What this document is.** `NP-QMS-DC-001` lists device labelling and the instructions for use (IFU)
> as *TBD*, so no user document exists. This is the first one. It holds the **text a user is to be
> given**, in plain language, second person, with no internal identifiers, together with the **release
> gate** each statement must meet before it is issued. It is working text, not a labelled IFU: it
> does not yet cover the device, and it exists so that a decision which tells users to *do* something
> reaches them. Statements already drafted elsewhere (`NP-FW-HUB-001` §6.6, `OI-FWHUB-13`) are listed in
> §3 and are carried here when this document is built out.

**Why a release gate on every statement.** A user will act on an instruction in this document. If the
instruction rests on a property nobody has measured, it is a safety claim made to a person who cannot
check it. So each row says what has to be true first, and a statement whose gate is open is **not**
issued.

---

## 1. How a statement is held

| Column | Meaning |
|---|---|
| ID | `IFU-nn`, append-only, never reused |
| Statement | The user-facing text, as it is to be shown |
| Source | The decision or requirement that the statement reports |
| Release gate | What must be true before the text reaches a user |
| Status | `Held` (gate open) or `Issued` |

## 2. Statements

### 2.1 Stopping a session

| ID | Statement | Source | Release gate | Status |
|---|---|---|---|---|
| **IFU-01** | **Stopping a session.** *To stop or pause a session, use the NeurOne app: choose Pause to take a break and carry on afterwards, or End session to finish. If the app does not respond, you can stop the session by unplugging the USB-C power cable from the headset. Your headset has no battery, so stimulation stops when power is removed. Unplugging the cable is a safe way to stop a session, and you can do it at any time. [What to expect after you unplug it: to be written, see the open points below.]* | `OI-UI-KMP-09`, principal decision 2026-10-28 (the cable pull is the accepted way to stop when the link to the app is unavailable); `SWR-SS-05`, `SWR-SS-06`, `SWR-SS-08` | **Open.** (1) The bench measurement of `OI-PWRSRC-11` shows, for each modality, how long the output takes to reach zero when the last source is removed and how it behaves on the way down; `RISK-PWRSRC-01` (**High, open**) records that no hold-up requirement exists and that stimulation could continue into a collapsing rail. (2) What happens when the cable is plugged in again is established. (3) The app's Pause and End session controls exist for the user (`SWR-SS-05` and `SWR-SS-06` are built at the helmet only). (4) The sentence "is a safe way to stop" is checked against the measurement of (1) and worded to what it shows | Held |

**Open points the text cannot be finished without.**

1. **What the output does when power is removed.** `NP-PWRSRC-001` finds the hub's 22 F supercapacitor
   specified for duty-cycle transients and not as a hold-up reserve, with no hold-up requirement and no
   measurement. For a tDCS, tACS or VNS output, removal of power bypasses the hardware-enforced 30 s ramp
   (`RISK-PWRSRC-01`). The text says the unplug is safe. That is a claim to be checked against the
   measurement, not assumed.
2. **What happens when the cable goes back in.** In Mode 3 the headset runs a pre-programmed session
   from a power bank. Whether power-up restarts that session, resumes it or waits for the user is not
   stated in any document read here. For cervical VNS (T2) the cardiac lockout and its confirmation are
   described in `NP-FW-CVNS-001`; whether they survive a power cycle is the subject of `OI-FAULTMSG-01`.
   The "what to expect" sentence is written from whatever those establish.
3. **Which cable, in Mode 3.** With a power bank the cable to unplug is the one between the bank and
   the headset. Whether it is reachable in use is not specified.
4. **Whether this is the only link-independent stop.** The principal accepted the cable pull *for now*;
   a physical control on the hub or headset, or a timeout, can be reopened (`OI-UI-KMP-09`). If one is
   added, this statement changes.

## 3. Statements held in other documents, to be carried here

| Source | What it is | Carrying item |
|---|---|---|
| `NP-FW-HUB-001` §6.6 | The per-session recording limit (about 49 hours per session; recording stops, the session does not), with proposed text | `OI-FWHUB-13` |

## 4. Where a statement must also appear

An instruction to a user is held in this document and, when the app shows it, in the app's own help.
In-app text is a key in the locale files, never text in code (`CLAUDE.md` §17), and it carries the same
release gate. **No help key for IFU-01 is added while the gate is open.** Adding one before then would
put the instruction in front of users.

## 5. Open items

| ID | Description | Owner | Blocking |
|---|---|---|---|
| **OI-IFU-01** | **Build this document out into the instructions for use.** It holds one statement. The device, its intended use, its warnings and its operating instructions are not written, and `NP-QMS-DC-001` lists labelling and IFU as *TBD (Year 1)*. | Regulatory + Systems Engineering | First release of any user-facing instruction |
| **OI-IFU-02** | **Meet the release gate of `IFU-01`.** The hold-up measurement (`OI-PWRSRC-11`), the behaviour on re-plugging, and the app's Pause and End controls (§2.1). | Safety + EE + FW + App | `IFU-01` |
| **OI-IFU-03** | **Carry `NP-FW-HUB-001` §6.6 here** (`OI-FWHUB-13`). | FW + Regulatory | IFU authoring |

## 6. Revision history

| Rev | Date | Author | Description |
|---|---|---|---|
| 1 | 2026-10-28 | NeurOne Systems Engineering | **Initial release, working text.** First user-facing document. Holds `IFU-01`, the instruction that the USB-C cable can be unplugged to stop a session when the app does not respond (`OI-UI-KMP-09`, principal decision 2026-10-28), with its release gate: the statement is held until the hold-up behaviour (`RISK-PWRSRC-01`, `OI-PWRSRC-11`) is measured and the re-plug behaviour is established. Raises `OI-IFU-01…03`. |
