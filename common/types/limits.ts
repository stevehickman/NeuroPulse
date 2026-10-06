import { nppsResolveLimits } from '../lib/nppsCore';
// ─── Per-modality limit interfaces ────────────────────────────────────────────
// All fields optional: undefined = not set at this level

export interface PBMTranscranialLimits {
  maxIrradianceMWcm2?: number;       // mW/cm², per wavelength block
  maxFrequencyHz?: number;
  maxDutyCyclePercent?: number;      // ≤25 always (hardware cap)
  maxSessionDoseJCm2?: number;       // J/cm² per zone per session
  maxDailyDoseJCm2?: number;
}

export interface PBMIntranasalLimits {
  maxIrradianceMWcm2?: number;
  maxSessionDoseJCm2?: number;
  maxSessionDurationSeconds?: number;
}

export interface EEGNeurofeedbackLimits {
  allowedBands?: string[];           // EEG band rawValue whitelist
  requireClosedLoop?: boolean;
}

export interface BESTacsLimits {
  maxIntensityMilliamps?: number;
  maxFrequencyHz?: number;
  minFrequencyHz?: number;
  maxSessionDurationSeconds?: number;
  maxSessionsPerDay?: number;
}

export interface TDCSLimits {
  maxIntensityMilliamps?: number;
  maxSessionDurationSeconds?: number;
  maxSessionsPerDay?: number;
}

export interface VNSHRVLimits {
  maxIntensityMilliamps?: number;
  maxFrequencyHz?: number;
  maxSessionDurationSeconds?: number;
  allowedProtocols?: string[];       // HRV protocol rawValue whitelist
}

export interface AudioEntrainmentLimits {
  maxVolumeDb?: number;              // dB SPL
  maxBinauralBeatsHz?: number;
  maxIsochronicTonesHz?: number;
}

export interface VisualStimLimits {
  maxFrequencyHz?: number;
  minFrequencyHz?: number;
  allowedModes?: string[];           // VisualMode rawValue whitelist
  blockHighRiskRange?: boolean;      // true = error if 3–30 Hz (not just warning)
}

export interface TMSLimits {
  maxIntensityPercentMT?: number;
  maxPulsesPerSession?: number;
  maxPulsesPerDay?: number;
  maxSessionsPerWeek?: number;
  allowedProtocols?: string[];
  allowedTargets?: string[];
}

export interface DeepPBMLimits {
  maxIntensityMWcm2?: number;
  maxSessionDurationSeconds?: number;
}

export interface ClinicalTacsLimits {
  maxIntensityMilliamps?: number;
  maxSessionDurationSeconds?: number;
}

export interface HDTdcsLimits {
  maxIntensityMilliamps?: number;    // per electrode
  maxSessionDurationSeconds?: number;
  allowedMontages?: string[];
}

export interface CervicalVnsLimits {
  maxIntensityMilliamps?: number;
  maxSessionDurationSeconds?: number;
}

export interface VibrotactileLimits {
  maxIntensityG?: number;
  maxSessionDurationSeconds?: number;
}

// ─── Limit level ───────────────────────────────────────────────────────────────

export type LimitLevel = 'global' | 'helmet' | 'individual';

// ─── Limits set ────────────────────────────────────────────────────────────────

export interface NPLimitsSet {
  id: string;
  name: string;
  description: string;
  createdAt: string;
  modifiedAt: string;
  level: LimitLevel;
  helmetId?: string;       // required when level === 'helmet'
  individualId?: string;   // required when level === 'individual'

  pbmTranscranial?: PBMTranscranialLimits;
  pbmIntranasal?: PBMIntranasalLimits;
  eegNeurofeedback?: EEGNeurofeedbackLimits;
  besTacs?: BESTacsLimits;
  tdcs?: TDCSLimits;
  vnsHrv?: VNSHRVLimits;
  audioEntrainment?: AudioEntrainmentLimits;
  visualStimulation?: VisualStimLimits;
  tms?: TMSLimits;
  pbmDeep1170nm?: DeepPBMLimits;
  clinicalTacs?: ClinicalTacsLimits;
  hdTdcs?: HDTdcsLimits;
  cervicalVns?: CervicalVnsLimits;
  vibrotactile40hz?: VibrotactileLimits;
}

// ─── Individual profile ────────────────────────────────────────────────────────

export interface NPIndividualProfile {
  id: string;
  name: string;
  notes: string;
  dateOfBirth?: string;  // ISO date string
  createdAt: string;
}

// ─── Validation types ──────────────────────────────────────────────────────────

export type LimitSource = 'hardware' | 'global' | 'helmet' | 'individual';

export interface NPValidationIssue {
  id: string;
  severity: 'error' | 'warning';
  modality?: string;                 // NPModalityTypeId, undefined = protocol-level
  parameterKey: string;
  parameterDisplayName: string;
  actualValueDescription: string;
  limitValueDescription: string;
  limitSource: LimitSource;
  message: string;
}

export interface NPValidationResult {
  issues: NPValidationIssue[];
  isValid: boolean;
  hasWarnings: boolean;
  errors: NPValidationIssue[];
  warnings: NPValidationIssue[];
}

// ─── Three-tier limit resolution ───────────────────────────────────────────────

/**
 * Resolve three limit tiers into a single NPLimitsSet: for each modality struct field, individual ?? helmet ?? global.
 * If a modality struct is undefined at all three levels, the resolved one is undefined.
 *
 * The rule is the shared NPPS core's (common/npps-core/src/resolve.rs, OI-NPPS-CORE-01), the same one iOS and Android
 * call; this only gives the result the identity the app's models carry. In a browser `await initNppsCore()` first.
 */
export function resolveLimits(
  global?: NPLimitsSet,
  helmet?: NPLimitsSet,
  individual?: NPLimitsSet
): NPLimitsSet {
  return resolveLimitsWithSources(global, helmet, individual).limits;
}

/** The tier each resolved value came from: `{<modalityProperty>: {<limitField>: tier}}`. */
export type NPLimitSourceMap = Record<string, Record<string, 'individual' | 'helmet' | 'global'>>;

/** As `resolveLimits`, with the source map the validator attributes each configured limit by. */
export function resolveLimitsWithSources(
  global?: NPLimitsSet,
  helmet?: NPLimitsSet,
  individual?: NPLimitsSet
): { limits: NPLimitsSet; sources: NPLimitSourceMap } {
  const resolved = nppsResolveLimits({ global, helmet, individual });
  const now = new Date().toISOString();
  return {
    limits: {
      id: 'resolved',
      name: 'Resolved Limits',
      description: 'Three-tier resolved limits',
      createdAt: now,
      modifiedAt: now,
      ...resolved.limits,
      level: 'global',
    } as NPLimitsSet,
    sources: resolved.sources,
  };
}

// ─── Unlimited limits (no restrictions) ───────────────────────────────────────

export const UNLIMITED_LIMITS: NPLimitsSet = {
  id: 'unlimited',
  name: 'No Limits',
  description: 'No dosage limits configured — hardware limits only apply.',
  createdAt: new Date(0).toISOString(),
  modifiedAt: new Date(0).toISOString(),
  level: 'global',
  // All modality limits are undefined = no dosage restrictions
};
