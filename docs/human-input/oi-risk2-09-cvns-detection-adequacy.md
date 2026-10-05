# Clinical question for review: OI-RISK2-09 (is the cVNS cardiac detection requirement adequate?)

**Status:** DRAFT, not sent. Not a controlled document; see `docs/human-input/README.md`.
**From:** NeurOne Safety / Quality (`NP-RISK-002`, `docs/np_risk_002.md` §4.3.5, §6 `OI-RISK2-09`)
**To:** Clinical reviewer (name to be assigned; the item's owner column reads "Clinical + Quality Lead")
**Response needed:** an answer to each of (a)–(e) in section 1, with rationale, the reviewer's name and role, and the date.
**Related:** `OI-RISK2-06` (`oi-risk2-06-clinical-question.md`) goes to the same reviewer and can be answered together.

## 1. The questions

The cervical VNS (cVNS) cardiac interlock is built to the requirement below. Nothing in the repo says
the requirement is clinically adequate, only that the firmware meets it on the host.

> **`REQ-CVNS-09`:** a heart-rate change of **more than 15 BPM within 5 s**, ending at **40 BPM or
> more**, is **detected within 18 s of onset**. The enable line then goes low within 100 ms of
> detection. Re-enable needs a 30 s lockout, app confirmation and a repeat impedance check.

Please answer each of these, yes or no with a short reason:

- **(a) Latency.** Is detection up to **18 s from onset** adequate for a vagally mediated
  bradycardia or arrhythmia during cervical stimulation? If not, what bound would be?
- **(b) Floor.** Is a change that **ends between 20 and 40 BPM** a hazard the interlock must detect?
  Today it is not guaranteed to be cut. Below 20 BPM a separate 3 s no-R-peak cutoff acts.
- **(c) Slow change.** Is a change **spread over more than 5 s** a hazard the interlock must detect?
  Today it is not cut, by design.
- **(d) Dropped beats.** Is **50 → 37.5 BPM with every 4th beat absent** a hazard the interlock must
  detect? Earlier firmware cut it incidentally. The current design does not.
- **(e) Single pause.** Is **one 2–3 s pause at 60 BPM or below** a hazard the interlock must detect?
  Same history as (d).

## 2. Why we are asking

The cardiac interlock was found in 2026-09 to miss most qualifying heart-rate changes, and was
redesigned on 2026-10-01 around an 18 s history. On the host it now cuts every change the requirement
names. But the requirement itself was written to what the detector could do. Its 100 ms figure was
never reachable from onset, because any detector on an 8-beat mean needs about seven beats after the
change before the mean has moved 15 BPM. Whether **that latency**, and **what the requirement leaves
out**, are safe for a person receiving cervical stimulation has never been answered.

## 3. What the device does and does not do

| Event | Cut by the interlock? |
|-------|-----------------------|
| Step or ramp of more than 15 BPM within 5 s, ending at 40 BPM or more (from 50–110 BPM) | Yes, in 100 % of simulated trials; longest latency 15.9 s |
| Rhythm below 20 BPM (3 s with no R-peak) | Yes, by the R-peak staleness cutoff |
| Ending between 20 and 40 BPM | Not guaranteed (b) |
| Change spread over more than 5 s | No, by design (c) |
| 50 → 37.5 BPM, every 4th beat absent | No (d) |
| One 2–3 s pause at 60 BPM or below | No (e) |

The heart-rate source is the PPG-derived R-peak signal on the clip, not an ECG. These figures are from
host simulation. The verification run on hardware (FAI-CV02) has not been done.

## 4. What a "yes" costs

Each "yes" is a **requirement change**, not a firmware tweak. It goes through `REQ-CVNS-09`
(`docs/np_hw_cvns_001.md` §3), `docs/reference/hardware-detail.md` §4.2 and `NP-FW-CVNS-001`, with a
derivation recorded under CLAUDE.md §18. Expect each to raise the nuisance-cutoff rate (hazard 25-e,
rated S1 × P5 and provisional on a rate nobody has measured). A wider net means more lost sessions,
and more motive for the profile-switching in 25-c. So a "yes" is most useful with a stated threshold
or event definition, not only "detect it".

A "no" needs the reason too. It is recorded as the derivation for leaving the case out.

## 5. What we need back

1. An answer to each of (a)–(e).
2. For each "yes": the event or threshold that must be caught, and the time by which.
3. A two-to-three-sentence rationale per answer. It is recorded as the derivation (CLAUDE.md §18).
4. Anything that would reopen an answer, such as evidence about the PPG source, or a measured
   nuisance rate.
5. Reviewer name, role and date.

## 6. What this decides, and what it doesn't

Until this closes, RISK-25 cannot move from P2 to its P1 target, and it blocks T2 clinical release.
Safety then updates `NP-RISK-002`, logs the answer in `docs/status/completed-decisions.md`, and for
any "yes" opens the `REQ-CVNS-09` change. The Quality Lead approves the re-rating (`NP-RM-001` §8.2).
Closing this item does **not** supply the hardware verification (FAI-CV02, C4 on silicon,
`OI-CVNSHW-03`, `OI-CVNS-15`), which P1 also waits on.

## References
`docs/np_risk_002.md` §4.3.5 (residuals i–iv), §6 `OI-RISK2-09` · `docs/np_hw_cvns_001.md`
`REQ-CVNS-09` · `docs/np_fw_cvns_001.md` §14.5, §14.6.1 · `docs/np_fmea_001.md` FMEA-M05-09
