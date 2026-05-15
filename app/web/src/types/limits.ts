// ─── Per-modality limit interfaces ────────────────────────────────────────────
// All fields optional: undefined = not set at this level

export interface PBMTranscranialLimits {
  maxIntensityPercent?: number;      // 0–100%
  maxFrequencyHz?: number;
  maxDutyCyclePercent?: number;      // ≤25 always (hardware cap)
  maxSessionDoseJCm2?: number;       // J/cm² per zone per session
  maxDailyDoseJCm2?: number;
}

export interface PBMIntranasalLimits {
  maxIntensityPercent?: number;
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
  maxVolumePercent?: number;
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

// Helper: merge two optional modality structs, taking individual ?? helmet ?? global
// for each field.
function mergeOptional<T extends object>(
  global: T | undefined,
  helmet: T | undefined,
  individual: T | undefined
): T | undefined {
  if (global == null && helmet == null && individual == null) return undefined;
  const result = {} as Record<string, unknown>;
  const allKeys = new Set<string>([
    ...Object.keys(global ?? {}),
    ...Object.keys(helmet ?? {}),
    ...Object.keys(individual ?? {}),
  ]);
  for (const k of allKeys) {
    const iv = individual != null ? (individual as Record<string, unknown>)[k] : undefined;
    const hv = helmet != null ? (helmet as Record<string, unknown>)[k] : undefined;
    const gv = global != null ? (global as Record<string, unknown>)[k] : undefined;
    const resolved = iv !== undefined ? iv : hv !== undefined ? hv : gv;
    if (resolved !== undefined) result[k] = resolved;
  }
  return result as T;
}

/**
 * Resolve three limit tiers into a single NPLimitsSet.
 * For each modality struct field: individual ?? helmet ?? global.
 * If a modality struct is undefined at all three levels, resolved is undefined.
 */
export function resolveLimits(
  global?: NPLimitsSet,
  helmet?: NPLimitsSet,
  individual?: NPLimitsSet
): NPLimitsSet {
  const now = new Date().toISOString();
  return {
    id: 'resolved',
    name: 'Resolved Limits',
    description: 'Three-tier resolved limits',
    createdAt: now,
    modifiedAt: now,
    level: 'global',

    pbmTranscranial: mergeOptional(global?.pbmTranscranial, helmet?.pbmTranscranial, individual?.pbmTranscranial),
    pbmIntranasal: mergeOptional(global?.pbmIntranasal, helmet?.pbmIntranasal, individual?.pbmIntranasal),
    eegNeurofeedback: mergeOptional(global?.eegNeurofeedback, helmet?.eegNeurofeedback, individual?.eegNeurofeedback),
    besTacs: mergeOptional(global?.besTacs, helmet?.besTacs, individual?.besTacs),
    tdcs: mergeOptional(global?.tdcs, helmet?.tdcs, individual?.tdcs),
    vnsHrv: mergeOptional(global?.vnsHrv, helmet?.vnsHrv, individual?.vnsHrv),
    audioEntrainment: mergeOptional(global?.audioEntrainment, helmet?.audioEntrainment, individual?.audioEntrainment),
    visualStimulation: mergeOptional(global?.visualStimulation, helmet?.visualStimulation, individual?.visualStimulation),
    tms: mergeOptional(global?.tms, helmet?.tms, individual?.tms),
    pbmDeep1170nm: mergeOptional(global?.pbmDeep1170nm, helmet?.pbmDeep1170nm, individual?.pbmDeep1170nm),
    clinicalTacs: mergeOptional(global?.clinicalTacs, helmet?.clinicalTacs, individual?.clinicalTacs),
    hdTdcs: mergeOptional(global?.hdTdcs, helmet?.hdTdcs, individual?.hdTdcs),
    cervicalVns: mergeOptional(global?.cervicalVns, helmet?.cervicalVns, individual?.cervicalVns),
    vibrotactile40hz: mergeOptional(global?.vibrotactile40hz, helmet?.vibrotactile40hz, individual?.vibrotactile40hz),
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
