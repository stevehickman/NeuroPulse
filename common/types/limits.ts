import { nppsResolveLimits } from '../lib/nppsCore';
import { ProfileLimits } from '../lib/constants.generated';
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

// What the limits store keeps about a person, and nothing else (NP-SW-001 REQ-SW03-32; the per-field rulings are in
// docs/reference/data-architecture-detail.md §5.1). `id` is random and names the individual tier of the limits. `name` is how
// the person choosing a profile recognises it. `notes` is a short label that tells two profiles apart ("left chair"); it is
// not a record about the person, nothing reads it but the profile list, and it holds at most
// ProfileLimits.PROFILE_NOTES_MAX_CHARS UTF-16 code units. There is no date of birth and no creation time: no limit or protocol rule ever read
// either (OI-PRIV-11; the creation time by the principal's ruling of 2026-11-18). `stealth` is the person's choice to keep the
// helmet's lights dark while the profile is active (SWR-IND-01, OI-PRIV-37), set in the Compose app; this app has no control for
// it and keeps it so a migration through here does not drop it. UHDR-class; absent means false.
export interface NPIndividualProfile {
  id: string;
  name: string;
  notes: string;
  stealth?: boolean;
  /**
   * How the profile is named to a helmet (REQ-FAULTMSG-01, OI-PRIV-29 (NP-PRIV-ANALYSIS-004)); the Kotlin store assigns it and
   * the web app never names a helmet. Kept as read, never made or changed here: dropping it would give the person a new tag
   * on every helmet, and a helmet holding a cardiac cutoff under the old one would no longer apply it to them. UHDR-class.
   */
  activeUserTag?: ProfileTagState;
}

/** See NPIndividualProfile.activeUserTag and the Kotlin ProfileTagState (common/core NPLimitsStore.kt). */
export interface ProfileTagState {
  /** 32 random bytes as 64 lowercase hex digits. */
  secret: string;
  /** The profile existed before the per-helmet tag, so a helmet may know it by its old tag. */
  legacy: boolean;
  /** The per-helmet tags the profile has moved to. */
  migrated: number[];
}

/** A tag state as read back, or null if it is not one (then the field is not kept). */
function storedTagState(raw: unknown): ProfileTagState | null {
  if (typeof raw !== 'object' || raw === null) return null;
  const r = raw as Record<string, unknown>;
  if (typeof r.secret !== 'string' || !/^[0-9a-f]{64}$/.test(r.secret) || typeof r.legacy !== 'boolean') return null;
  const migrated = r.migrated === undefined ? [] : r.migrated;
  if (!Array.isArray(migrated) || !migrated.every(t => Number.isInteger(t) && t >= 1 && t <= 0xfffffffe)) return null;
  return { secret: r.secret, legacy: r.legacy, migrated: [...migrated] };
}

/** True when `notes` is within the bound a profile note may hold. */
export function profileNotesFit(notes: string): boolean {
  return notes.length <= ProfileLimits.PROFILE_NOTES_MAX_CHARS;
}

/**
 * A profile as read from storage or a file, reduced to the fields a profile has: any other field it carries (the date of
 * birth and the creation time an earlier build kept) is dropped, and a note over the bound is cut to it. Null when it has no id or no name.
 */
export function storedProfile(raw: unknown): NPIndividualProfile | null {
  if (typeof raw !== 'object' || raw === null) return null;
  const r = raw as Record<string, unknown>;
  if (typeof r.id !== 'string' || r.id === '' || typeof r.name !== 'string' || r.name === '') return null;
  const profile: NPIndividualProfile = {
    id: r.id,
    name: r.name,
    notes: typeof r.notes === 'string' ? r.notes.slice(0, ProfileLimits.PROFILE_NOTES_MAX_CHARS) : '',
    // Only true is kept: false is the default and is not written, as the Kotlin store does not write it.
    ...(r.stealth === true ? { stealth: true } : {}),
  };
  const tag = storedTagState(r.activeUserTag);
  if (tag) profile.activeUserTag = tag;
  return profile;
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
