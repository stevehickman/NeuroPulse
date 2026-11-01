# Principal decision for review: OI-SINK-10, the 3 mm re-loft makes the one scalp-side thermal figure slightly worse

**Status:** DRAFT, ready to send; not yet sent (no addressee named). Not a controlled document; see `docs/human-input/README.md`.
**From:** NeurOne Systems Engineering (`OI-SINK-10`, raised 2026-09-23 by `OI-THCOOL-21`, GitHub #407; `docs/status/pending-decisions.md`)
**To:** The principal, with Thermal and Mechanical Engineering (the item's owner column reads "Principal (Thermal + ME)")
**Response needed:** a choice among the options in section 3, a short rationale, conditions that would reopen it, name, role and date.
**Why now:** shell tooling has not been cut, so option B (below) is still free of rework. Nothing is blocked on this item, and it does **not** reopen `REQ-CAV-04` on its own.

## 1. The question

Layer 4 (the absorber foam) was deleted and the 3 mm outer-bowl re-loft binds (`REQ-CAV-04`, taken 2026-09-23).
That decision was taken on a per-area comparison, and `NP-THERM-SINK-001` shows that the one model which also
carries the **helmet exterior** gets a slightly worse scalp-side result. Do you accept that, or change the
re-loft?

## 2. What is known

All figures are from `NP-THERM-SINK-001` Rev 3 §3a and `NP-EMC-CAV-001` §8.2, as recorded in the open item. They are model outputs,
not measurements. `THERM-1b` (`OI-R1-02`) is the test that would measure them.

- `REQ-CAV-04` rested on outward heat flux per unit area, 0.410 to 0.335, which is correct per unit area and is all
  any other thermal script models.
- The re-loft moves the exterior skin 3 mm inward. Rejecting area falls **4.6 %**, and `SPEC-SINK-01` rises
  **1.08 to 1.14 K/W**.
- Net effect at N = 6 on the library floor with the S3 spreader, 25 °C ambient:

| Quantity | Before | After |
|---|---|---|
| Maximum face temperature | 40.7 °C | 40.9 °C |
| Occlusion fraction at which that case crosses 42 °C | 0.72 | 0.64 |
| Bare-shell floor ceiling | 2 tiles | 1 tile |
| Aggregate PBM ceiling | 31.4 W | 30.5 W |

- Attribution: removing the foam alone moves the face by **-0.02 K**. The smaller exterior alone moves it by **+0.19 K**.
- It is **not a control gap.** The 42 °C interlock and Path B1's face NTC observe the temperature exactly as before, and no limit has been adjusted.

**Not known:** whether 0.2 °C and the lower occlusion margin matter in use, which needs `THERM-1b`; and whether option B's mass and tooling cost is small.

## 3. Options

- **A. Accept.** Record that the deletion is thermally neutral to slightly adverse at the scalp, not favourable. No hardware change. The
  helmet's thermal margins in the document set are restated from the "after" column.
- **B. Take the re-loft on the inner face only, holding the exterior where it was** (a thicker outer bowl). Cost: mass, and a
  `MECH-2` / `NP-TOOL-SHELL-001` re-scope question. Nothing is cut yet, so no tooling is wasted.
- **C. Other**, for example waiting for `THERM-1b` before deciding. Cost: the decision stays open while tooling approaches.

The sender expresses no preference: the trade is 0.2 K and a tile of floor margin against mass and a tooling re-scope, and neither side of it is measured.

## 4. What the sender needs back

1. The option (A, B or C) and, for C, what to do instead.
2. A rationale that names what fails if the figure is worse (CLAUDE.md §18), for example the 42 °C margin at which a user would notice a derate.
3. Whether the answer is conditional on `THERM-1b`, and if so what result reverses it.
4. Name, role and date.

If you answer A, the sender records it in the owning files and in `docs/status/completed-decisions.md`. If B, it opens the tooling re-scope item.
