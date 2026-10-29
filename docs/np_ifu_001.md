# Instructions for Use — Working Text

**Project:** NeurOne
**Document:** NP-IFU-001
**Revision:** 3
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

### 2.4 Operating the device

| ID | Statement | Source | Release gate | Status |
|---|---|---|---|---|
| **IFU-07** | **Starting a session.** *Connect the headset to power with the USB-C cable and open the NeurOne app. Choose a protocol and start it. The app sends the protocol to the headset, and the headset will not run a protocol that the app has not properly signed. You can stay connected to the app during the session, or send the protocol and disconnect, and the headset carries on by itself (see "Using a power bank"). The app will not start a session while a consumable that blocks sessions is due for replacement (see "Replacing consumables"). To pause or stop, see "Stopping a session".* | CLAUDE.md §4.2 (signed protocols), §4.6 Modes 1–3, `SWR-SS-05`, `SWR-SS-06`; `commercial-model.md` §2.3 (blocking consumables) | **Open.** (1) The app's start flow, and its refusal screens, are the ones the text describes. (2) Every other condition that refuses a start (for example an outstanding cervical cutoff, IFU-06) is listed in the text or cross-referred | Held |
| **IFU-08** | **Using a power bank, without your phone.** *Your headset has no battery. After you send a protocol from the app, you can run it from a USB-C power bank that can supply the power your protocol needs [bank rating: to be written, see the gate]. The headset keeps adjusting the session to your brain activity without your phone. It records the session, and the app downloads the recording the next time you connect the headset. The headset has no words of its own: the green light on the left means it has power, the amber light on the right means a session is running, and a red blinking light means something has gone wrong (see "The lights"). If the red light blinks, stop the session [what to do next for a fault that is not a gel pad or a cardiac cutoff: to be written].* | CLAUDE.md §4.1, §4.6 Modes 3 and 4, §4.7; `NP-PWRSRC-001` §13 (Mode 3 reachability) | **Open.** (1) The bank rating is taken from `NP-PWRSRC-001` §13's reachability column per protocol, not from CLAUDE.md §4.5's table, which carries electrical draw only; no runtime is stated, because the thermal bound binds first. (2) What happens when the cable is plugged in again (`IFU-01` open point 2). (3) What a wearer is told to do after a red blink that is neither a gel pad fault (`IFU-05`) nor a cardiac cutoff (`IFU-06`): `NP-SW-FAULTMSG-001` covers cervical faults only | Held |
| **IFU-09** | **Chargers and power.** *Use the charger that came with your headset, or any USB-C Power Delivery (PD) charger or power bank. Any PD charger works. If the app says a protocol needs more power than your supply gives, it tells you what the protocol needs. If the app says the headset cannot give off the heat a protocol would make, a more powerful charger will not fix it: use a shorter or lighter protocol.* | CLAUDE.md §2.2 (any PD charger must work; the app informs and never blocks); `NP-PWRSRC-001` §12.2 (failure classes F3-a, F3-b: **F3-b must never be presented as F3-a**) | **Open.** (1) The "power level: reduced" message is built and says what the headset does on a weaker supply. (2) The wattage of the charger in each box is not stated here until `NP-PWRSRC-001` §12.4 is resolved: a 65 W source unlocks no protocol that 45 W does not, so the ladder in CLAUDE.md §2.2 cannot yet be described to a user as a performance step | Held |
| **IFU-10** | **The lights.** *Left temple, green: the headset has power; it breathes slowly when idle. Right temple, amber: a session is running, and the light pulses at the session's frequency. Red blinking on the left: a fault. Stop the session. The app can turn the lights off (stealth mode). Stealth mode never hides a fault: the red light always shows.* | CLAUDE.md §4.7; `NP-SW-FAULTMSG-001` §1 (one fault state for every fault) | **Open.** (1) Stealth mode exists in the app. (2) The red-blink sentence is kept in step with `IFU-08` open point 3 | Held |

### 2.5 Warmth during light therapy

| ID | Statement | Source | Release gate | Status |
|---|---|---|---|---|
| **IFU-11** | **Warmth on your scalp.** *During light therapy (red and near-infrared) the part of the headset that touches your scalp can feel warm. It can reach [X] °C. The headset slows or reduces the light by itself if it gets too warm, and the app tells you when it has done so. If it feels uncomfortably hot, stop the session. [Skin-type and duration note: see the gate.]* | `NP-BIB-PBMIRR-001` §3.3 (IEC 60601-1 3rd ed.: any applied part above 41 °C is disclosed in the labelling and the instructions for use); CLAUDE.md §3 (42 °C limit), §4.2 (NTC per zone, hardware throttle); `ADAPT_EVENT_THERMALTHROTTLE` | **Open.** (1) **[X] is not stated.** It is the highest scalp-contact temperature a session can reach, from the `NP-THERM-*` thermal chain, and nothing here measures it. It is not filled with the 42 °C limit, which is a ceiling, not a measurement. (2) `OI-BIBPBM-03` fixes which figure is the limit (42 °C or the standard's 43 °C). (3) Darker skin showed about three times the heating risk in the literature on red light (`NP-BIB-PBMIRR-001` §4): whether the text says so is a clinical and human-factors decision. (4) The sentence that the app tells you needs the throttle event to reach the user | Held |

### 2.6 Replacing consumables

The app tells you when a consumable needs replacing, from the headset's own session counts. It never prompts by calendar date: the headset has no clock that survives being unplugged.

| ID | Statement | Source | Release gate | Status |
|---|---|---|---|---|
| **IFU-12** | **Replacement reminders.** *The app shows the state of your consumables under Consumable Status. When one is due, the app says so, and you can snooze some reminders a few times. A reminder marked "Action Required" blocks the next session until you replace the part: this is a hygiene rule. After you replace a part, tap Mark replaced in the app, so the headset starts counting again. If you replace a part while the headset is not connected, the app remembers and tells the headset the next time you connect.* | `commercial-model.md` §2.3 and §2.3 *Where the counts come from* (`OI-ACC-08`); CLAUDE.md §5.2 (reminder classes); locale keys `CONSUMABLE_*` | **Open.** (1) The snooze counts are the §5.2 figures (performance ×3, comfort and longevity ×5) as built in the app. (2) The sentence on *Mark replaced* offline matches the app's queueing behaviour on every platform. (3) A part replaced but not marked leaves the count running: the text says so once the app does not warn about it | Held |
| **IFU-13** | **Intranasal sleeves.** *Use a new sleeve for every session. The sleeve is a hygiene barrier and is for one session only. The app will not start a session until you have fitted a new sleeve and marked it replaced. Use only NeurOne sleeves: the headset checks that the sleeve is the right kind.* | `NP-HW-NASAL-001` §6.1; `commercial-model.md` §2.3 (threshold 1, blocking); CLAUDE.md §2.3 | **Open.** (1) The headset checks the sleeve's type, not the individual sleeve, so a sleeve refitted after *Mark replaced* is not caught (`OI-NASAL-04`); the text does not say the headset checks that the sleeve is new. (2) "Single use" is a commercial property with no mechanism behind it (`NP-HW-NASAL-001` §6.2); the sentence stays *"for one session only"* until that finding closes | Held |
| **IFU-14** | **Electrode tips and VNS clip pads.** *The app counts the sessions on your electrode hydrogel tips and your ear-clip pads and tells you when to replace them. Replace them when the app says. Replace a tip or a pad sooner if it is damaged, dry or dirty. The app cannot see that.* | `commercial-model.md` §2.3 (rows for hydrogel tips and VNS clip pads: exposure counts); `OI-ACC-09`, `OI-VNSCLIP-07` | **Open.** The session counts at which the app prompts (45 for tips, 30 for clip pads) are unvalidated placeholders. **No number appears in the text** and none may until `OI-ACC-09` and `OI-VNSCLIP-07` close. Whether the text may say "when the app says" for a count with no validated basis is an open question for Regulatory | Held |
| **IFU-15** | **Audio cup foam.** *The app tells you when the foam on the audio cups has lost its seal and needs replacing. Replacing foam for comfort or cleanliness is up to you, at any time.* | `commercial-model.md` §2.3 (principal decision 2026-09-23: the prompt covers loss of function only); `OI-AUDIOHW-08`, `OI-AUDIOHW-09`, `OI-ACC-04` | **Open.** The seal measurement (the in-cup microphone) is not specified or built, and the foam's session count (150) is an unvalidated placeholder. Until a seal measurement exists, no prompt can honestly say the seal is lost | Held |

### 2.7 Cervical VNS indications (T2 only)

| ID | Statement | Source | Release gate | Status |
|---|---|---|---|---|
| **IFU-16** | **What the cervical accessory is for.** *The NeurOne Pro Cervical VNS Accessory is for non-invasive stimulation of the vagus nerve in the neck in adults, for: (1) acute treatment of pain from episodic cluster headache; (2) adjunctive preventive treatment of episodic cluster headache; (3) acute treatment of migraine without aura.* | `NP-REG-CVNS-001` §7.1 (proposed) | **Open.** This is an indications-for-use claim. It cannot be issued before FDA agrees the intended use (Q-Sub question 1) and a 510(k) clears it; and the T2 tier identity is not in force (`OI-UPG-08`). It is held with `IFU-03…06`, which it frames | Held |

## 3. Instructions required but not yet drafted

Each row is a user-directed instruction that a controlled document says is needed, or that a device with
these modalities cannot be used safely without, and for which this document still has no text. The
sources below show where its facts are; writing it is design work with its own open item.

| What | Where the facts are | Carrying item |
|---|---|---|
| Fitting the headset (one adult size, 52–62 cm; occipital dial; bridge positions) and what a poor fit does to the EEG | CLAUDE.md §4.4; `hardware-detail.md` §4.4; `NP-HW-EEGNET-001` | `OI-IFU-06` |
| Using each T1 modality (PBM, EEG, BES/tACS, tDCS, auricular VNS, audio, visual), including electrode preparation | `modality-stack.md`; accessory specs `NP-HW-{AUDIO,NASAL,VNSCLIP}-001` | `OI-IFU-06` |
| Contraindications, warnings and the stop-at-any-time rule for the T1 electrical and light modalities (for example seizure and photosensitivity for flashing light and 3–30 Hz stimulation; implants for electrical stimulation) | CLAUDE.md §3 and §4.2 (the photoparoxysmal interlock and its clinician unlock); no controlled text yet states the T1 contraindications | `OI-IFU-07` (clinical review) |
| Cleaning, storage, durability and what to do when the headset needs service | `docs/reference/durability-maintenance.md`, `service-network.md` | `OI-IFU-06` |
| Privacy and data: what the headset and the app keep, backups, and the person-presence unlock | CLAUDE.md §5; `NP-APP-PRESENCE-001` | `OI-IFU-06` |

## 4. Where a statement must also appear

An instruction to a user is held in this document and, when the app shows it, in the app's own help.
In-app text is a key in the locale files, never text in code (`CLAUDE.md` §17), and it carries the same
release gate. **No help key for any statement in §2 is added while its gate is open.** Adding one before then would
put the instruction in front of users.

## 5. Open items

| ID | Description | Owner | Blocking |
|---|---|---|---|
| **OI-IFU-01** | **Build this document out into the instructions for use.** It holds sixteen statements (`IFU-01…16`), all held. The device, its intended use, its warnings and its operating instructions are not written, and `NP-QMS-DC-001` lists labelling and IFU as *TBD (Year 1)*. | Regulatory + Systems Engineering | First release of any user-facing instruction |
| **OI-IFU-02** | **Meet the release gate of `IFU-01`.** The hold-up measurement (`OI-PWRSRC-11`), the behaviour on re-plugging, and the app's Pause and End controls (§2.1). | Safety + EE + FW + App | `IFU-01` |
| **OI-IFU-02a** | **Meet the release gates of `IFU-02…16`** (§2.2 to §2.7), each as stated in its row. | FW + App + Regulatory | `IFU-02…06` |
| **OI-IFU-03** | ~~Carry `NP-FW-HUB-001` §6.6 here (`OI-FWHUB-13`).~~ **DISCHARGED Rev 2:** carried as `IFU-02`. `OI-FWHUB-13` stays open until `IFU-02` is issued. | FW + Regulatory | — |
| **OI-IFU-04** | ~~Draft the above-41 °C disclosure.~~ **DISCHARGED Rev 3:** drafted as `IFU-11`, with the figure [X] withheld. Measuring the scalp-contact temperature is part of `IFU-11`'s gate. | Regulatory + Optics | — |
| **OI-IFU-05** | ~~Draft the consumable replacement instructions.~~ **DISCHARGED Rev 3:** drafted as `IFU-12…15`, with no threshold number stated. | Regulatory + App | — |
| **OI-IFU-06** | **Draft the operating, fitting, per-modality, cleaning, service and data instructions** listed in §3 (the part of the device's operating instructions not drafted at Rev 3). | Regulatory + Systems Engineering | IFU authoring |
| **OI-IFU-07** | **Establish the T1 contraindications and warnings** for the electrical and light modalities. No controlled document states them; the cervical set (`IFU-03`, `IFU-04`) is the only one. This is a clinical-review item, and it is raised, not drafted, because the contraindications of a stimulation device are not to be written from general knowledge. | Clinical + Regulatory | T1 labelling |

## 6. Revision history

| Rev | Date | Author | Description |
|---|---|---|---|
| 1 | 2026-10-28 | NeurOne Systems Engineering | **Initial release, working text.** First user-facing document. Holds `IFU-01`, the instruction that the USB-C cable can be unplugged to stop a session when the app does not respond (`OI-UI-KMP-09`, principal decision 2026-10-28), with its release gate: the statement is held until the hold-up behaviour (`RISK-PWRSRC-01`, `OI-PWRSRC-11`) is measured and the re-plug behaviour is established. Raises `OI-IFU-01…03`. |
| 2 | 2026-10-29 | NeurOne Systems Engineering | **Every user-directed instruction in the document set is carried.** Adds `IFU-02` (recording limit, `NP-FW-HUB-001` §6.6), `IFU-03…05` (cervical VNS contraindications, warnings, gel pad use, `NP-REG-CVNS-001` §7.2–§7.4) and `IFU-06` (cutoff and fault messages, `NP-SW-FAULTMSG-001`), each held behind a release gate. §3 now lists the instructions that are required but have no text (above-41 °C disclosure, consumable replacement, operating instructions). Discharges `OI-IFU-03`; raises `OI-IFU-02a`, `OI-IFU-04`, `OI-IFU-05`. No statement is issued. |
| 3 | 2026-10-29 | NeurOne Systems Engineering | **Drafts the instructions §3 listed as having no text.** Adds `IFU-07…10` (starting a session, power bank use, chargers, the lights), `IFU-11` (warmth during light therapy, the temperature figure withheld), `IFU-12…15` (consumable replacement, no threshold stated) and `IFU-16` (cervical indications), all held behind release gates. §3 now lists what is still undrafted (fitting, per-modality use, T1 contraindications, cleaning and service, data). Discharges `OI-IFU-04`, `OI-IFU-05`; raises `OI-IFU-06`, `OI-IFU-07`. No statement is issued. |
