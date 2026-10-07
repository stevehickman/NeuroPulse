/*
 * The cautions a protocol asks its author to acknowledge (docs/reference/safety-zones.md): one per axis in its caution
 * band, each carrying the id the compiler checks (`acknowledgedCautions`). An id names the dose, so a changed dose is a
 * new caution. Pure data: the acknowledgement screens (web, iOS, Android) render it, and nothing here stores anything.
 */
import type { NppsValidation, NppsZoneBlock } from './nppsCore';

export interface ZoneCaution {
  /** The id to pass in `acknowledgedCautions`. */
  ackId: string;
  /** Zero-based block index within the protocol. */
  blockIndex: number;
  /** The block's modality token, e.g. `tdcs`. */
  modality: string;
  /** Locale key of the axis name (`ZONE_AXIS_…`). */
  axisNameKey: string;
  unit: string;
  value: number;
  /** Where the caution band starts. */
  caution: number;
}

export function cautionsOf(zones: readonly NppsZoneBlock[]): ZoneCaution[] {
  const out: ZoneCaution[] = [];
  for (const b of zones) {
    for (const a of b.axes) {
      if (a.zone === 'caution' && a.ackId) {
        out.push({ ackId: a.ackId, blockIndex: b.index, modality: b.modality, axisNameKey: a.nameKey, unit: a.unit, value: a.value, caution: a.caution });
      }
    }
  }
  return out;
}

/** True when the protocol is in the danger zone: no acknowledgement can run it, so no screen is offered. */
export const isDanger = (v: Pick<NppsValidation, 'zone'>): boolean => v.zone === 'danger';
