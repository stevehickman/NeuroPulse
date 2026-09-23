/**
 * thermal-outward-path.ts — the outward thermal path, as-was and current, in ONE place.
 *
 * OI-THCOOL-21 (GitHub #407). The Layer 4 absorber station (3 mm carbon-loaded
 * foam) was DELETED on 2026-09-23 (`REQ-CAV-04`, GitHub #391), and the 3 mm
 * re-loft of the outer bowl moved with it as one binding change
 * (`NP-EMC-CAV-001` §8.2). Every thermal script used to carry its own copy of
 * the 0.41 m^2K/W outward path with the foam in it. They now import it from
 * here, so the as-was and current figures cannot fork between models, and so
 * the sibling outer-bowl heat budget (`OI-EMCCAV-07`, GitHub #403) reads the
 * same constants.
 *
 * Two cases, and the difference between them is the whole point:
 *
 *   AS-WAS (R1)  — what `NP-THERM-CFD-R1-001` §2 actually modelled. It is still
 *                  the CALIBRATION anchor: every published R1 temperature and
 *                  export fraction was produced with the foam in, so a model
 *                  that reproduces R1 must be run at this path. It is also the
 *                  baseline several documents' arguments are stated against
 *                  (e.g. "2 mm of gap beats deleting the whole absorber").
 *                  Label it HISTORICAL wherever it is printed.
 *
 *   CURRENT      — the design after the deletion + re-loft. Every capability
 *                  figure quoted as current must be computed here.
 *
 * Not a check script: it prints nothing and gates nothing, so it carries no
 * CI-Kind declaration (check-gate-coverage.ts scopes to check-* / test-* only).
 *
 * Units: area-normalised R" in m^2*K/W unless stated.
 */

// ---------------------------------------------------------------------------
// Published anchors — NP-THERM-CFD-R1-001 §2 (the as-was path)
// ---------------------------------------------------------------------------

/** NP-THERM-CFD-R1-001 §2: outward path total, fan off, WITH the Layer 4 foam.
 *  HISTORICAL since 2026-09-23 — retained as R1's calibration anchor. */
export const R_OUT_R1 = 0.41;
/** NP-THERM-CFD-R1-001 §2: the stagnant inter-bowl air gap within R_OUT_R1.
 *  UNCHANGED by the deletion: the Gap station does not move (NP-HELMET-GEOM-001 §2). */
export const R_GAP_STAGNANT = 0.23;
/** R1's cavity -> ambient leg (foam + shell + external film), by difference. */
export const R_CAV_AMB_R1 = R_OUT_R1 - R_GAP_STAGNANT; // 0.18

// ---------------------------------------------------------------------------
// The deleted station — NP-THERM-COOL-001 §2 / NP-EMC-CAV-001 §7
// ---------------------------------------------------------------------------

/** Layer 4 station thickness, m. NP-HELMET-GEOM-001 §2 station L2 (now 0). */
export const ABSORBER_T_M = 0.003;
/** Carbon-loaded open-cell foam, W/m.K — the value the decision was taken at
 *  (NP-THERM-COOL-001 §2, NP-EMC-CAV-001 §7). NP-THERM-SINK-001 §3.1's layer
 *  table used 0.05 for the same foam; see R_ABSORBER_SINK_TABLE. */
export const K_ABSORBER = 0.04;
/** Stagnant air, W/m.K — the same table's 6 mm gap term. */
export const K_AIR = 0.026;
/** The absorber's term on the outward path — what the deletion removes. */
export const R_ABSORBER = ABSORBER_T_M / K_ABSORBER; // 0.075
/** The same station at NP-THERM-SINK-001 §3.1's k = 0.05 (0.060). Kept so the
 *  sink model's R1 decomposition still reproduces; its film is not re-derived. */
export const R_ABSORBER_SINK_TABLE = ABSORBER_T_M / 0.05;
/** The station vacated WITHOUT the re-loft: stagnant air in its place. */
export const R_STATION_VACATED = ABSORBER_T_M / K_AIR; // 0.115
/** OI-EMCCAV-08's fallback: thin ceramic-filled pad, REQ-CAV-03 (NP-THERM-COOL-001 §6.3). */
export const R_STATION_SUBSTITUTED = 0.02;

// ---------------------------------------------------------------------------
// The outward path in each configuration (NP-EMC-CAV-001 §8.2's table)
// ---------------------------------------------------------------------------

/** CURRENT: station deleted, outer bowl re-lofted 3 mm inward. REQ-CAV-04. */
export const R_OUT_CURRENT = R_OUT_R1 - R_ABSORBER; // 0.335
/** Deleted but NOT re-lofted — the regression the re-loft exists to prevent. */
export const R_OUT_VACATED = R_OUT_R1 - R_ABSORBER + R_STATION_VACATED; // 0.450
/** OI-EMCCAV-08's documented return path, if the plungers cannot cover +/-0.80. */
export const R_OUT_SUBSTITUTED = R_OUT_R1 - R_ABSORBER + R_STATION_SUBSTITUTED; // 0.355
/** CURRENT cavity -> ambient leg: shell + external film only. */
export const R_CAV_AMB_CURRENT = R_OUT_CURRENT - R_GAP_STAGNANT; // 0.105

// ---------------------------------------------------------------------------
// Geometry the re-loft moves — NP-HELMET-GEOM-001 §2
// ---------------------------------------------------------------------------

/** The re-loft: the outer bowl moves 3 mm inward, globally. */
export const RELOFT_MM = 3;
/** Module-face plane to exterior skin, mm: gap + L2 + L3. 12 as-was (R1 and
 *  NP-THERM-SINK-001 §3.2), 9 after the re-loft. The exterior the helmet rejects
 *  through SHRINKS with it — the one place the deletion costs rejection area. */
export const SHELL_OFFSET_MM_R1 = 12;
export const SHELL_OFFSET_MM_CURRENT = SHELL_OFFSET_MM_R1 - RELOFT_MM;

/** Which outward path a model is being run at. */
export type OutwardCase = "current" | "r1";
export const outwardPath = (c: OutwardCase) => (c === "r1" ? R_OUT_R1 : R_OUT_CURRENT);
export const cavAmbLeg = (c: OutwardCase) => (c === "r1" ? R_CAV_AMB_R1 : R_CAV_AMB_CURRENT);
