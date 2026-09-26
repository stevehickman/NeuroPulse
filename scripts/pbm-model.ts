/**
 * pbm-model.ts — the PBM power and thermal model constants, read from their one source.
 *
 * NP-PWRSRC-001 §11 item 5 / OI-PWRSRC-13 (GitHub #440). check-pbm-power.ts owns
 * the demand model and three scripts import it; the runtime governor
 * (OI-FWHUB-09) is a fourth consumer, in C. Two languages reading one model is a
 * divergence risk, and "the countermeasure is not care". It had already
 * happened inside TypeScript: the non-PBM overhead was 8.0 in
 * check-power-source-coverage.ts and 7.0 in check-power-envelope.ts and
 * check-thermal-bowl.ts, each a literal, each citing the same 6–8 W band.
 *
 * So every constant lives in hardware/np_pbm_model.json. This module reads it
 * for the TypeScript audits; scripts/check-pbm-model.ts emits the same values
 * into firmware/hub_control/include/np_pbm_model.generated.h and fails CI when
 * the two disagree or when a consumer re-declares a constant as a literal.
 *
 * This file is a library, not a check: it has no CI-Kind and prints nothing.
 * It throws on a malformed model, so a bad edit to the JSON fails every consumer
 * at import rather than producing a quietly wrong table.
 */
import { readFileSync } from "fs";
import { join } from "path";
import { parseModel, PBM_MODEL_PATH, type PbmModel } from "./check-pbm-model";

function load(): PbmModel {
  const path = join(import.meta.dir, "..", PBM_MODEL_PATH);
  const { model, problems } = parseModel(readFileSync(path, "utf8"));
  if (!model || problems.length) {
    throw new Error(`${PBM_MODEL_PATH} is malformed:\n  ${problems.join("\n  ")}`);
  }
  return model;
}

export const PBM_MODEL = load();

const byName = new Map(PBM_MODEL.constants.map((c) => [c.name, c.value]));
function get(name: string): number {
  const v = byName.get(name);
  if (v === undefined) throw new Error(`${PBM_MODEL_PATH} has no constant ${name}`);
  return v;
}

// ── Named exports, one per constant the TypeScript audits read ───────────────
// Units are in the names where the JSON names them; see hardware/np_pbm_model.json
// for each value's source and status. NONE of these is measured.

/** Per-tile electrical draw at 100 % intensity, full 150 mA drive, by wavelength set (W). */
export const TILE_W: Record<string, number> = {
  "660_808nm": get("TILE_W_660_808NM"),
  "1064nm": get("TILE_W_1064NM"),
  "660_808_1064nm": get("TILE_W_660_808_1064NM"),
};
export const AVAILABLE_W = get("AVAILABLE_W");
export const OVERHEAD_W_MIN = get("OVERHEAD_W_MIN");
export const OVERHEAD_W_MAX = get("OVERHEAD_W_MAX");
/** Midpoint of NP-HW-HEXTILE-001 §9.1's band — derived here, not stored, so it
 *  cannot drift from the two ends it is the midpoint of. */
export const OVERHEAD_W_MID = (OVERHEAD_W_MIN + OVERHEAD_W_MAX) / 2;
export const THERMAL_BUDGET_W = get("THERMAL_BUDGET_W");
export const FACE_LIMIT_C = get("FACE_LIMIT_C");
export const T_AMBIENT_NOMINAL_C = get("T_AMBIENT_NOMINAL_C");
export const TAU_FACE_MIN = get("TAU_FACE_LOW");
export const TAU_FACE_MAX = get("TAU_FACE_HIGH");
export const R_CAVITY_CONSERVATIVE = get("R_CAVITY_CONSERVATIVE");
export const R_CAVITY_OPTIMISTIC = get("R_CAVITY_OPTIMISTIC");
export const CALIB_FACE_RISE_C = get("CALIB_FACE_RISE_C");
export const CALIB_TILE_W = get("CALIB_TILE_W");
export const CEM43_REVIEW_LINE = get("CEM43_REVIEW_LINE");
export const CEM43_CONCERN_LINE = get("CEM43_CONCERN_LINE");
