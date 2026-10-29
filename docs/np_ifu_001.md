# Instructions for Use — Working Text

**Project:** NeurOne
**Document:** NP-IFU-001
**Revision:** 2
**Date:** 2026-10-29
**Status:** DRAFT — working text for the user-facing instructions; **not released, and nothing here may be shown to a user until the release gate on its row is met**
**Effective Date:** —
**Author:** NeurOne Systems Engineering
**Approved By:** — (pending)
**References:** `NP-QMS-DC-001` (labelling and IFU listed as *TBD*); `NP-SW-001` §6.3.6 (`SWR-SS-05`, `SWR-SS-06`, `SWR-SS-08`); `NP-PWRSRC-001` §10, `RISK-PWRSRC-01`, `OI-PWRSRC-11`; `OI-UI-KMP-09` in `docs/status/pending-decisions.md`; `NP-FW-HUB-001` §6.6 and `OI-FWHUB-13`; `NP-REG-CVNS-001` §7.2–§7.4; `NP-SW-FAULTMSG-001` §9; `NP-BIB-PBMIRR-001` §3.3; `docs/reference/commercial-model.md` §2.3
**Related Issues:** `OI-UI-KMP-09`
**Gate:** None. Labelling is a Year 1 item in `NP-QMS-DC-001`
**IEC 62304 Class:** —

---

> **What this document is.** `NP-QMS-DC-001` lists device labelling and the instructions for use (IFU)
> as *TBD*, so no user document exists. This is the first one. It holds the **text a user is to be
> given**, in plain language, second person, with no internal identifiers, together with the **release
> gate** each statement must meet before it is issued. It is working text, not a labelled IFU: it
> does not yet cover the device, and it exists so that a decision which tells users to *do* something
> reaches them. Every user-directed instruction drafted elsewhere is now carried in §2 (Rev 2); what is
> still undrafted is listed in §3.

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

### 2.2 Recording limit

| ID | Statement | Source | Release gate | Status |
|---|---|---|---|---|
| **IFU-02** | **How long a session can record.** *Your NeurOne saves each session's recording (brain-activity (EEG) data, heart-rate data and session events) in its own file on the device. A single session can record for about 49 hours. If one session runs longer than that, the device stops recording for the rest of that session. The session itself is not stopped, and your earlier sessions and your next session are not affected. Most sessions are far shorter than this limit. This is a limit on one session. The device's total storage is separate: the app tells you when the device's storage for your own data is nearly full.* | `NP-FW-HUB-001` §6.6, `OI-FWHUB-13`, `OI-LFS-11`, `EMMC-FS-01` | **Open.** (1) The 49-hour figure is re-derived if `file_max` or the EEG data rate changes. (2) The last sentence promises the storage-nearly-full notification (`EMMC-WE-02`); it is not issued until that notification reaches the user in the app | Held |

### 2.3 Cervical VNS accessory (T2 only)

These statements apply only to a unit whose tier identity says T2 (`REQ-UPG-01`). That identity is not in force (`OI-UPG-08`), so none of them can reach a user yet.

| ID | Statement | Source | Release gate | Status |
|---|---|---|---|---|
| **IFU-03** | **Who must not use cervical stimulation.** *Do not use cervical stimulation if you have an active implanted electronic device (pacemaker, implanted defibrillator, cochlear implant or drug pump), carotid artery disease (narrowing of the carotid artery, or carotid surgery within the last 3 months), or a cervical vagotomy. Do not use it if you are pregnant or are under 18.* | `NP-REG-CVNS-001` §7.2 (proposed, consistent with gammaCore labelling) | **Open.** Contraindications are proposed, not agreed with FDA (Q-Sub); wording follows the clinical and regulatory review | Held |
| **IFU-04** | **Warnings during cervical stimulation.** *The device watches your heart rate while it stimulates. If your heart rate changes by more than 15 beats per minute, it stops by itself. A change is noticed within about 18 seconds of when it began (for a heart rate that ends at 40 beats per minute or above), and stimulation stops within 0.1 seconds after that. If this happens, do not start again without talking to a doctor. Do not use the device while driving or operating heavy machinery. Do not use it over broken or irritated skin. Some discomfort at the stimulation site is expected and goes away; lower the current if it is too strong. Keep the gel pads away from your eyes and the inside of your mouth and nose.* | `NP-REG-CVNS-001` §7.3; detection and cutoff figures from `REQ-CVNS-09` (`NP-HW-CVNS-001` §3), always quoted together (CLAUDE.md §4.2) | **Open.** (1) Detection and cutoff figures re-read against `REQ-CVNS-09` at release. (2) Warnings are proposed text, pending the clinical and human-factors review | Held |
| **IFU-05** | **Gel pads: use and replacement.** *Each gel pad is designed to work for one session. You can use a pad again for as long as the device accepts it. Before every session, the device checks that each pad is making good contact with your skin. If a pad fails this check, stimulation does not start, and the NeurOne app tells you which side of your neck the pad is on. Replace that pad with a new one and try again. If a pad loses contact during a session, stimulation stops, and the app tells you which side. Replace that pad and start again. If you are using the device without the app and the red light on the device blinks, stop. Connect your phone and open the NeurOne app before you use the device again, then start the session from the app. If a gel pad is the problem, the device checks the pads again and the app tells you which side of your neck the pad is on. Throw a pad away if it is torn, dirty or no longer sticks to your skin, even if the device still accepts it. Do not share pads with another person.* | `NP-REG-CVNS-001` §7.4; `OI-ACC-03`, `OI-ACC-07`; `REQ-CVNS-06`, `REQ-CVNS-08` | **Open.** (1) The red-blink paragraph does not work for every fault: after a cardiac cutoff in Mode 3 the blink does not say the cutoff was cardiac. It is not issued until IFU-06 reaches the user (`OI-ACC-07`). (2) The reuse and do-not-share sentences follow the biocompatibility basis (`OI-CVNSHW-07`) and the human-factors review | Held |
| **IFU-06** | **After a cardiac cutoff, and warnings shown to every user of a device.** *When the app connects and a cutoff has happened (including one that happened while no phone was connected), it tells you that stimulation stopped because your heart rate changed, and not to use cervical stimulation again without medical advice. If anyone who uses this device has an outstanding cutoff, every user is shown a warning when the app connects and must confirm it before a cervical protocol can be sent. Other fault messages: lost heart-rate signal ("check the ear clip is on and its pads are in contact"); device fault ("contact support if it repeats"); pad contact (which side of the neck).* The exact wording is the app's locale keys; the text above is the intent only. | `NP-SW-FAULTMSG-001` §P4 table, §9.1 items 3–6; `OI-ACC-07` | **Open.** (1) App screens that show a logged cutoff exist (`NP-REG-CVNS-001` §7.4 records that they do not). (2) Wording drafted in locale keys for all eleven locales (CLAUDE.md §17) and reviewed with IFU-04. (3) The per-user model is verified on the target (`NP-SW-FAULTMSG-001` §9, not verified) | Held |

**What a user is not told by any of the above.** IFU-05 sends the wearer to the phone after a red blink.
Without IFU-06 that is the instruction a wearer would follow after a cardiac cutoff, which is why
IFU-05 is held behind it.

## 3. Instructions required but not yet drafted

Each row is a user-directed instruction that a controlled document says is needed. None has text, and
this document does not invent it: writing it is design work with its own open item.

| What | Source | Why it is needed | Carrying item |
|---|---|---|---|
| Disclosure of any applied part above 41 °C in labelling and instructions for use | `NP-BIB-PBMIRR-001` §3.3 (IEC 60601-1 3rd ed.); CLAUDE.md §3 limit of 42 °C (`OI-BIBPBM-03` on 42 vs 43 °C) | An external requirement on the PBM applied parts; the figure to state depends on `OI-BIBPBM-03` | `OI-IFU-04` |
| Consumable replacement: intranasal sleeve (single use, blocks session start), hydrogel tips, VNS clip pads, audio cup foam; the *Mark replaced* step in the app | `docs/reference/commercial-model.md` §2.3 and its Trigger column; `OI-ACC-08` | Every prompt is measurement-triggered (CLAUDE.md §2.3); the thresholds for tips, clip pads and cup foam are placeholders (`OI-ACC-09`, `OI-VNSCLIP-07`, `OI-ACC-04`), so no number is stated to a user | `OI-IFU-05` |
| Indications for use for the cervical accessory | `NP-REG-CVNS-001` §7.1 (proposed) | Labelling, not a process instruction; listed so it is not lost | `OI-IFU-01` |
| Starting a session, Mode 3 operation from a power bank, charger and PD guidance, and the status lights | CLAUDE.md §2.2, §4.5, §4.6, §4.7 | Operating instructions for the whole device | `OI-IFU-01` |

## 4. Where a statement must also appear

An instruction to a user is held in this document and, when the app shows it, in the app's own help.
In-app text is a key in the locale files, never text in code (`CLAUDE.md` §17), and it carries the same
release gate. **No help key for any statement in §2 is added while its gate is open.** Adding one before then would
put the instruction in front of users.

## 5. Open items

| ID | Description | Owner | Blocking |
|---|---|---|---|
| **OI-IFU-01** | **Build this document out into the instructions for use.** It holds six statements (`IFU-01…06`). The device, its intended use, its warnings and its operating instructions are not written, and `NP-QMS-DC-001` lists labelling and IFU as *TBD (Year 1)*. | Regulatory + Systems Engineering | First release of any user-facing instruction |
| **OI-IFU-02** | **Meet the release gate of `IFU-01`.** The hold-up measurement (`OI-PWRSRC-11`), the behaviour on re-plugging, and the app's Pause and End controls (§2.1). | Safety + EE + FW + App | `IFU-01` |
| **OI-IFU-02a** | **Meet the release gates of `IFU-02…06`** (§2.2, §2.3), each as stated in its row. | FW + App + Regulatory | `IFU-02…06` |
| **OI-IFU-03** | ~~Carry `NP-FW-HUB-001` §6.6 here (`OI-FWHUB-13`).~~ **DISCHARGED Rev 2:** carried as `IFU-02`. `OI-FWHUB-13` stays open until `IFU-02` is issued. | FW + Regulatory | — |
| **OI-IFU-04** | **Draft the above-41 °C disclosure** (§3), after `OI-BIBPBM-03` fixes the figure. | Regulatory + Optics | PBM labelling |
| **OI-IFU-05** | **Draft the consumable replacement instructions** (§3) once the triggers have measured thresholds. | Regulatory + App | Consumable labelling |

## 6. Revision history

| Rev | Date | Author | Description |
|---|---|---|---|
| 1 | 2026-10-28 | NeurOne Systems Engineering | **Initial release, working text.** First user-facing document. Holds `IFU-01`, the instruction that the USB-C cable can be unplugged to stop a session when the app does not respond (`OI-UI-KMP-09`, principal decision 2026-10-28), with its release gate: the statement is held until the hold-up behaviour (`RISK-PWRSRC-01`, `OI-PWRSRC-11`) is measured and the re-plug behaviour is established. Raises `OI-IFU-01…03`. |
| 2 | 2026-10-29 | NeurOne Systems Engineering | **Every user-directed instruction in the document set is carried.** Adds `IFU-02` (recording limit, `NP-FW-HUB-001` §6.6), `IFU-03…05` (cervical VNS contraindications, warnings, gel pad use, `NP-REG-CVNS-001` §7.2–§7.4) and `IFU-06` (cutoff and fault messages, `NP-SW-FAULTMSG-001`), each held behind a release gate. §3 now lists the instructions that are required but have no text (above-41 °C disclosure, consumable replacement, operating instructions). Discharges `OI-IFU-03`; raises `OI-IFU-02a`, `OI-IFU-04`, `OI-IFU-05`. No statement is issued. |
