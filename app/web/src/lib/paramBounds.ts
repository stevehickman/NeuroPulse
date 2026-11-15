/*
 * paramBounds.ts — EVERY bound the web app's parameter controls apply, in this one file.
 *
 * To find or change a bound, edit it here. The controls (ParamInputs.tsx) and the clinician-limit
 * fields (LimitsSettings.tsx) read their ranges from this file and carry none of their own.
 *
 * What a bound may rest on (the principal, 2026-11-15; docs/status/completed-decisions.md):
 *   natural        cannot be otherwise: a frequency or a time is not below 0.
 *   hardware       set by the hardware or its wire format: a driver ceiling, a descriptor field's width.
 *   practical      set by what a person can perceive or do: the ear does not respond above 20 kHz.
 *   safety         a health or safety limit, with its source.
 *   unestablished  the form has always carried this figure and no source for it is on record. It is kept
 *                  so behaviour does not change, and each one is a question for the principal.
 *
 * A bound with none of those reasons is arbitrary and is not here: that side is left open (`unbounded`
 * explains why). A bound is only ever shown or checked. Nothing in the controls clamps a typed value, and
 * the validator, the compiler and the hub stay the places a value is refused.
 *
 * Which control a range gets follows from its bounds alone (controlKind below):
 *   bounded on both sides      a slider, with an editable value field and a magnifier for fine changes.
 *   bounded on one side or none  a typed field only. A range with no end has no track, even a logarithmic
 *                              one (the principal chose a typed field, 2026-11-15).
 *
 * The figures that are hardware or safety limits come from NPHardwareLimits (common/npps/constants.json),
 * so there is still one copy of each.
 */
import { NPHardwareLimits as hw } from '../../../../common/lib/constants.generated';

export type Basis = 'natural' | 'hardware' | 'practical' | 'safety' | 'unestablished';

export interface Edge {
  value: number;
  basis: Basis;
  /** Where the figure comes from, or why it is kept without one. */
  source: string;
}

export interface ParamBound {
  min?: Edge;
  max?: Edge;
  /** The smallest change a person can make with the control, in the control's own unit. */
  step: number;
  /** For a side left open on purpose: why it is open for now. */
  unbounded?: string;
}

const natural = (value: number, source: string): Edge => ({ value, basis: 'natural', source });
const hardware = (value: number, source: string): Edge => ({ value, basis: 'hardware', source });
const safety = (value: number, source: string): Edge => ({ value, basis: 'safety', source });
const unestablished = (value: number, source: string): Edge => ({ value, basis: 'unestablished', source });

const NONNEG = (what: string) => natural(0, `${what} cannot be below 0.`);

// ─── Slider geometry ──────────────────────────────────────────────────────────

/** Width of a slider track in pixels. A range with more steps than this cannot be set to every step by
 *  dragging, so its slider offers the magnifier. */
export const SLIDER_TRACK_PX = 200;

// ─── Interval timing (ModalityEditor, IntervalControls) ───────────────────────

const NO_ARBITRARY_TIME_MAX =
  'No maximum: the former 3,600 s (and 1,000 repeats) was a form figure with no hardware, practical or safety source. The protocol timing is checked by the validator.';

export const INTERVAL_BOUNDS = {
  onSeconds: { min: NONNEG('A time'), step: 1, unbounded: NO_ARBITRARY_TIME_MAX },
  offSeconds: { min: NONNEG('A time'), step: 1, unbounded: NO_ARBITRARY_TIME_MAX },
  repeatCount: { min: NONNEG('A count'), step: 1, unbounded: NO_ARBITRARY_TIME_MAX },
} satisfies Record<string, ParamBound>;

// ─── Modality parameters (ModalityEditor) ─────────────────────────────────────

const PBM_FREQUENCY_NOTE =
  'No maximum: the former 100 Hz (40 Hz for deep PBM) is register UC-082, with no derivation. The descriptor carries up to 255 Hz and the driver limit is not established, so no hardware figure is applied yet.';
const PBM_DUTY_SOURCE = 'The hub, the compiler and the validator refuse a duty above 25 % (REQ-NPPS-33(a)).';

export const PARAM_BOUNDS = {
  pbmTranscranial: {
    irradiance: {
      min: NONNEG('An irradiance'),
      max: safety(hw.PBM_PULSED_PEAK_MW_CM2, 'CLAUDE.md §3: 400 mW/cm² peak is a hazard control (OI-BIBPBM-03).'),
      step: 1,
    },
    frequency: { min: NONNEG('A frequency'), step: 0.5, unbounded: PBM_FREQUENCY_NOTE },
    duty: {
      min: NONNEG('A duty cycle'),
      max: safety(hw.PBM_DUTY_CYCLE_MAX_PERCENT, PBM_DUTY_SOURCE),
      step: 1,
    },
  },
  pbmIntranasal: {
    irradiance: {
      min: NONNEG('An irradiance'),
      max: unestablished(400, 'Copied from the scalp figure; the intranasal probe has no owning specification (OI-ART-04).'),
      step: 1,
    },
    frequency: { min: NONNEG('A frequency'), step: 0.5, unbounded: PBM_FREQUENCY_NOTE },
    duty: {
      min: NONNEG('A duty cycle'),
      max: safety(hw.PBM_DUTY_CYCLE_MAX_PERCENT, PBM_DUTY_SOURCE),
      step: 1,
    },
  },
  pbmDeep: {
    intensity: {
      min: unestablished(100, 'Form figure, no source on record.'),
      max: safety(hw.PBM_DEEP_MAX_MW_CM2, 'CLAUDE.md §3: 1,000 mW/cm², 1170 nm, TEC-stabilised.'),
      step: 50,
    },
    frequency: { min: NONNEG('A frequency'), step: 0.5, unbounded: PBM_FREQUENCY_NOTE },
    duty: {
      min: NONNEG('A duty cycle'),
      max: safety(hw.PBM_DUTY_CYCLE_MAX_PERCENT, PBM_DUTY_SOURCE),
      step: 1,
    },
  },
  bes: {
    frequency: {
      min: unestablished(hw.BES_TACS_MIN_HZ, 'CLAUDE.md §3 locked table; no derivation (UC-143).'),
      max: unestablished(hw.BES_TACS_MAX_HZ, 'CLAUDE.md §3 locked table; no derivation (UC-143).'),
      step: 0.5,
    },
    intensity: {
      min: unestablished(0.1, 'Form figure, no source on record.'),
      max: safety(hw.BES_TACS_MAX_MILLIAMPS, 'CLAUDE.md §3: 1 mA for BES / tACS (T1), with the 40 µC/cm² per-phase charge limit.'),
      step: 0.1,
    },
  },
  tdcs: {
    intensity: {
      min: safety(hw.TDCS_MIN_MILLIAMPS, 'CLAUDE.md §3: tDCS 0.1 to 2 mA.'),
      max: safety(hw.TDCS_MAX_MILLIAMPS, 'CLAUDE.md §3: tDCS 0.1 to 2 mA; 150 mC/cm² per session.'),
      step: 0.1,
    },
    ramp: {
      min: safety(hw.TDCS_RAMP_SECONDS, 'The hardware-enforced minimum ramp (CLAUDE.md §3: 30 s).'),
      step: 1,
      unbounded: 'No maximum: the former 120 s was a form figure with no hardware, practical or safety source.',
    },
    electrodeArea: {
      min: hardware(0.001, 'The descriptor writes an area under 0.001 cm² as 0, which the compiler refuses (REQ-NPPS-33(c)).'),
      max: hardware(hw.TDCS_MAX_ELECTRODE_AREA_CM2, 'Width of the descriptor field (np_mod_tdcs_params_t.electrode_area_mcm2, uint16).'),
      step: 0.1,
    },
  },
  vns: {
    frequency: {
      min: NONNEG('A frequency'),
      step: 0.5,
      unbounded: 'No floor above 0 and no ceiling: the former 1 Hz floor and 25 Hz ceiling were equivalence bounds with no derivation and were removed (OI-VNSCLIP-08).',
    },
    intensity: {
      min: unestablished(0.1, 'Form figure, no source on record.'),
      max: unestablished(2, 'Form figure, below the 40 mA hardware stop-gap (VNS_MAX_MILLIAMPS); no source on record.'),
      step: 0.1,
    },
    breathingRate: {
      min: unestablished(hw.BREATHING_MIN_BPM, 'Authorable range of NP-NPPS-REF-001 §4.6, no derivation (UC-082).'),
      max: unestablished(hw.BREATHING_MAX_BPM, 'Authorable range of NP-NPPS-REF-001 §4.6, no derivation (UC-082).'),
      step: 0.5,
    },
  },
  audio: {
    beatRate: {
      min: NONNEG('A rate'),
      step: 0.5,
      unbounded: 'No maximum: the former 100 Hz is register UC-082, with no derivation. A binaural beat has a perceptual limit (NP-HW-AUDIO-001 §8.2.7 A), which is not a source for 100.',
    },
    carrier: {
      min: NONNEG('A frequency'),
      step: 1,
      unbounded: 'No maximum in the form: the validator holds the 1,000 Hz tone bound (REQ-AUDIO-25). The headphone driver, once selected, and the 20 kHz hearing limit are both above it.',
    },
    volume: {
      min: NONNEG('A level'),
      max: safety(hw.AUDIO_MAX_DB_SPL, 'NP-HW-AUDIO-001 REQ-AUDIO-19: 85 dB SPL at the ear.'),
      step: 0.5,
    },
  },
  visual: {
    frequency: {
      min: unestablished(0.5, 'Form figure, no source on record.'),
      max: hardware(hw.VISUAL_MAX_HZ, 'VISUAL_MAX_HZ, the hardware ceiling (constants.json).'),
      step: 0.5,
    },
    emdrCadence: {
      min: NONNEG('A cadence'),
      step: 0.1,
      unbounded: 'No maximum: the former 4 Hz was a form figure with no hardware, practical or safety source.',
    },
  },
  tms: {
    frequency: {
      min: unestablished(hw.TMS_MIN_HZ, 'Authorable range of NP-NPPS-REF-001 §4.10, no derivation (UC-082).'),
      max: unestablished(hw.TMS_MAX_HZ, 'Authorable range of NP-NPPS-REF-001 §4.10, no derivation (UC-082).'),
      step: 0.5,
    },
    intensity: {
      min: unestablished(80, 'Form figure, no source on record.'),
      max: unestablished(130, 'Form figure, no source on record (the validator warns above 120 % MT, TMS_HIGH_INTENSITY_PERCENT_MT).'),
      step: 1,
    },
    pulseCount: {
      min: unestablished(100, 'Form figure, no source on record.'),
      max: unestablished(6000, 'Form figure, no source on record. A pulse count is a dose, so its status is a question for the principal.'),
      step: 100,
    },
  },
  clinicalTacs: {
    frequency: {
      min: unestablished(hw.CLINICAL_TACS_MIN_HZ, 'Authorable range of NP-NPPS-REF-001 §4.12, no derivation (UC-082).'),
      max: unestablished(hw.CLINICAL_TACS_MAX_HZ, 'Authorable range of NP-NPPS-REF-001 §4.12, no derivation (UC-082).'),
      step: 0.5,
    },
    intensity: {
      min: unestablished(0.1, 'Form figure, no source on record.'),
      max: safety(hw.CLINICAL_TACS_MAX_MILLIAMPS, 'CLAUDE.md §3: clinical tACS up to 4 mA (T2).'),
      step: 0.1,
    },
    channelCount: {
      min: natural(2, 'A current needs two electrodes.'),
      max: hardware(hw.CLINICAL_TACS_MAX_CHANNELS, 'One driver channel per T2 cap electrode (NP-FW-HD-001 §6.4).'),
      step: 1,
    },
  },
  hdTdcs: {
    intensity: {
      min: unestablished(0.1, 'Form figure, no source on record.'),
      max: safety(hw.HD_TDCS_MAX_MILLIAMPS_PER_ELECTRODE, 'HD-tDCS per-electrode current ceiling (constants.json).'),
      step: 0.1,
    },
  },
  cervicalVns: {
    frequency: {
      min: hardware(hw.CERVICAL_VNS_MIN_HZ, 'Cervical VNS range 1 to 25 Hz, from the gammaCore predicate (K163334).'),
      max: hardware(hw.CERVICAL_VNS_MAX_HZ, 'Cervical VNS range 1 to 25 Hz, from the gammaCore predicate (K163334).'),
      step: 0.5,
    },
    intensity: {
      min: unestablished(0.1, 'Form figure, no source on record.'),
      max: safety(hw.CERVICAL_VNS_MAX_MILLIAMPS, 'Cervical VNS current ceiling (constants.json); cardiac interlock always active.'),
      step: 0.1,
    },
  },
  vibrotactile: {
    intensity: {
      min: hardware(hw.VIBROTACTILE_MIN_G, 'VIBROTACTILE_MIN_G, the accessory range (constants.json; the accessory is provisional).'),
      max: hardware(hw.VIBROTACTILE_MAX_G, 'VIBROTACTILE_MAX_G, the accessory range (constants.json; the accessory is provisional).'),
      step: 0.05,
    },
  },
} satisfies Record<string, Record<string, ParamBound>>;

// ─── Protocol-level fields (ProtocolEditor, ProtocolComposer) ─────────────────

export const PROTOCOL_BOUNDS = {
  timingValue: { min: unestablished(1, 'Form figure: at least 1 second or 1 repeat; no source on record.'), step: 1, unbounded: 'No maximum: no hardware, practical or safety source.' },
  layerStartOffsetSeconds: { min: NONNEG('A time'), step: 1, unbounded: 'No maximum: no hardware, practical or safety source.' },
  layerDurationSeconds: { min: unestablished(60, 'Form figure, no source on record.'), step: 1, unbounded: 'No maximum: no hardware, practical or safety source.' },
  layerIntensityScale: {
    min: unestablished(0.1, 'Form figure, no source on record.'),
    max: unestablished(2, 'Form figure, no source on record. The modality ceilings still apply to the scaled value.'),
    step: 0.05,
  },
} satisfies Record<string, ParamBound>;

// ─── Clinician limit fields (LimitsSettings) ──────────────────────────────────
// A limit is the most a clinician allows a parameter to reach, so its range sits inside the parameter's own.

export const LIMIT_BOUNDS = {
  pbmTranscranial: {
    maxIrradianceMWcm2: { min: NONNEG('A limit'), max: unestablished(400, 'Form figure for a clinician limit, no source on record.'), step: 5 },
    maxFrequencyHz: { min: NONNEG('A limit'), max: unestablished(100, 'Form figure for a clinician limit, no source on record.'), step: 1 },
    maxDutyCyclePercent: { min: NONNEG('A limit'), max: safety(hw.PBM_DUTY_CYCLE_MAX_PERCENT, 'The hub, the compiler and the validator refuse a duty above 25 %.'), step: 1 },
    maxSessionDoseJCm2: { min: NONNEG('A limit'), max: unestablished(200, 'Form figure for a clinician limit, no source on record.'), step: 5 },
    maxDailyDoseJCm2: { min: NONNEG('A limit'), max: unestablished(500, 'Form figure for a clinician limit, no source on record.'), step: 10 },
  },
  pbmIntranasal: {
    maxIrradianceMWcm2: { min: NONNEG('A limit'), max: unestablished(400, 'Form figure for a clinician limit, no source on record.'), step: 5 },
    maxSessionDurationSeconds: { min: unestablished(60, 'Form figure for a clinician limit, no source on record.'), max: unestablished(3600, 'Form figure for a clinician limit, no source on record.'), step: 60 },
  },
  besTacs: {
    maxIntensityMilliamps: { min: NONNEG('A limit'), max: safety(hw.BES_TACS_MAX_MILLIAMPS, 'CLAUDE.md §3: 1 mA for BES / tACS (T1).'), step: 0.1 },
    minFrequencyHz: { min: unestablished(hw.BES_TACS_MIN_HZ, 'CLAUDE.md §3 locked table; no derivation (UC-143).'), max: unestablished(hw.BES_TACS_MAX_HZ, 'CLAUDE.md §3 locked table; no derivation (UC-143).'), step: 0.5 },
    maxFrequencyHz: { min: unestablished(hw.BES_TACS_MIN_HZ, 'CLAUDE.md §3 locked table; no derivation (UC-143).'), max: unestablished(hw.BES_TACS_MAX_HZ, 'CLAUDE.md §3 locked table; no derivation (UC-143).'), step: 0.5 },
    maxSessionDurationSeconds: { min: unestablished(60, 'Form figure for a clinician limit, no source on record.'), max: unestablished(7200, 'Form figure for a clinician limit, no source on record.'), step: 60 },
    maxSessionsPerDay: { min: unestablished(1, 'Form figure for a clinician limit, no source on record.'), max: unestablished(10, 'Form figure for a clinician limit, no source on record.'), step: 1 },
  },
  tdcs: {
    maxIntensityMilliamps: { min: safety(hw.TDCS_MIN_MILLIAMPS, 'CLAUDE.md §3: tDCS 0.1 to 2 mA.'), max: safety(hw.TDCS_MAX_MILLIAMPS, 'CLAUDE.md §3: tDCS 0.1 to 2 mA.'), step: 0.1 },
    maxSessionDurationSeconds: { min: unestablished(60, 'Form figure for a clinician limit, no source on record.'), max: unestablished(3600, 'Form figure for a clinician limit, no source on record.'), step: 60 },
    maxSessionsPerDay: { min: unestablished(1, 'Form figure for a clinician limit, no source on record.'), max: unestablished(5, 'Form figure for a clinician limit, no source on record.'), step: 1 },
  },
  vnsHrv: {
    maxIntensityMilliamps: { min: NONNEG('A limit'), max: safety(hw.VNS_MAX_MILLIAMPS, 'Auricular VNS 40 mA stop-gap derived from the charge limit (OI-VNSCLIP-09).'), step: 0.1 },
    maxFrequencyHz: { min: NONNEG('A limit'), step: 0.1, unbounded: 'No ceiling: the hardware states none (OI-VNSCLIP-08).' },
  },
  audioEntrainment: {
    maxVolumeDb: { min: unestablished(40, 'Form figure for a clinician limit, no source on record.'), max: unestablished(90, 'Form figure for a clinician limit, no source on record.'), step: 1 },
    maxBinauralBeatsHz: { min: unestablished(0.5, 'Form figure for a clinician limit, no source on record.'), max: unestablished(100, 'Form figure for a clinician limit, no source on record.'), step: 0.5 },
    maxIsochronicTonesHz: { min: unestablished(0.5, 'Form figure for a clinician limit, no source on record.'), max: unestablished(100, 'Form figure for a clinician limit, no source on record.'), step: 0.5 },
  },
  visualStimulation: {
    minFrequencyHz: { min: NONNEG('A limit'), max: unestablished(100, 'Form figure for a clinician limit, no source on record.'), step: 0.5 },
    maxFrequencyHz: { min: NONNEG('A limit'), max: hardware(hw.VISUAL_MAX_HZ, 'VISUAL_MAX_HZ, the hardware ceiling (constants.json).'), step: 0.5 },
  },
  tms: {
    maxIntensityPercentMT: { min: unestablished(80, 'Form figure for a clinician limit, no source on record.'), max: unestablished(200, 'Form figure for a clinician limit, no source on record.'), step: 5 },
    maxPulsesPerSession: { min: unestablished(100, 'Form figure for a clinician limit, no source on record.'), max: unestablished(10000, 'Form figure for a clinician limit, no source on record.'), step: 100 },
    maxPulsesPerDay: { min: unestablished(100, 'Form figure for a clinician limit, no source on record.'), max: unestablished(30000, 'Form figure for a clinician limit, no source on record.'), step: 100 },
    maxSessionsPerWeek: { min: unestablished(1, 'Form figure for a clinician limit, no source on record.'), max: unestablished(7, 'Form figure for a clinician limit, no source on record.'), step: 1 },
  },
  clinicalTacs: {
    maxIntensityMilliamps: { min: NONNEG('A limit'), max: safety(hw.CLINICAL_TACS_MAX_MILLIAMPS, 'CLAUDE.md §3: clinical tACS up to 4 mA (T2).'), step: 0.5 },
  },
  hdTdcs: {
    maxIntensityMilliamps: { min: NONNEG('A limit'), max: safety(hw.HD_TDCS_MAX_MILLIAMPS_PER_ELECTRODE, 'HD-tDCS per-electrode current ceiling (constants.json).'), step: 0.1 },
  },
  cervicalVns: {
    maxIntensityMilliamps: { min: NONNEG('A limit'), max: safety(hw.CERVICAL_VNS_MAX_MILLIAMPS, 'Cervical VNS current ceiling (constants.json).'), step: 0.1 },
    maxSessionDurationSeconds: { min: unestablished(60, 'Form figure for a clinician limit, no source on record.'), max: unestablished(1800, 'Form figure for a clinician limit, no source on record.'), step: 60 },
  },
  vibrotactile40hz: {
    maxIntensityG: { min: hardware(hw.VIBROTACTILE_MIN_G, 'VIBROTACTILE_MIN_G, the accessory range (provisional accessory).'), max: hardware(hw.VIBROTACTILE_MAX_G, 'VIBROTACTILE_MAX_G, the accessory range (provisional accessory).'), step: 0.05 },
  },
} satisfies Record<string, Record<string, ParamBound>>;

// ─── What a bound means for a control ─────────────────────────────────────────

export type ControlKind = 'slider' | 'field';

/** Both sides bounded: a slider. Anything else: a typed field (a range with no end has no track). */
export function controlKind(b: ParamBound): ControlKind {
  return b.min !== undefined && b.max !== undefined ? 'slider' : 'field';
}

/** How many steps a slider spans. Infinity for a range with an open side. */
export function stepsAcross(b: ParamBound): number {
  if (b.min === undefined || b.max === undefined) return Infinity;
  return Math.round((b.max.value - b.min.value) / b.step);
}

/** True when dragging the slider cannot reach every step: more steps than pixels on the track. */
export function needsMagnifier(b: ParamBound, trackPx: number = SLIDER_TRACK_PX): boolean {
  return stepsAcross(b) > trackPx;
}

/** The window the magnifier shows: `trackPx` steps centred on `value`, kept inside the bounds and on the
 *  step grid. At one step per pixel every step is reachable. A range that already fits returns whole. */
export function fineRange(value: number, b: ParamBound, trackPx: number = SLIDER_TRACK_PX): { min: number; max: number } {
  const lo = b.min!.value;
  const hi = b.max!.value;
  if (!needsMagnifier(b, trackPx)) return { min: lo, max: hi };
  const span = trackPx * b.step;
  let start = value - span / 2;
  start = Math.min(Math.max(start, lo), hi - span);
  start = lo + Math.round((start - lo) / b.step) * b.step;
  const end = Math.min(start + span, hi);
  return { min: round(start, b.step), max: round(end, b.step) };
}

/** True when `v` is inside the bounds the range states (a side left open never fails). */
export function inBounds(v: number, b: ParamBound): boolean {
  if (b.min !== undefined && v < b.min.value) return false;
  if (b.max !== undefined && v > b.max.value) return false;
  return true;
}

function round(v: number, step: number): number {
  const decimals = Math.max(0, Math.ceil(-Math.log10(step)) + 1);
  return Number(v.toFixed(decimals));
}

/** Every bound in the file, flattened, for the tests and for anyone auditing the list. */
export function allBounds(): { key: string; bound: ParamBound }[] {
  const out: { key: string; bound: ParamBound }[] = [];
  for (const [k, b] of Object.entries(INTERVAL_BOUNDS)) out.push({ key: `interval.${k}`, bound: b });
  for (const [m, fields] of Object.entries(PARAM_BOUNDS)) {
    for (const [f, b] of Object.entries(fields)) out.push({ key: `${m}.${f}`, bound: b as ParamBound });
  }
  for (const [k, b] of Object.entries(PROTOCOL_BOUNDS)) out.push({ key: `protocol.${k}`, bound: b });
  for (const [m, fields] of Object.entries(LIMIT_BOUNDS)) {
    for (const [f, b] of Object.entries(fields)) out.push({ key: `limit.${m}.${f}`, bound: b as ParamBound });
  }
  return out;
}
