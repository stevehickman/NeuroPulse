import {
  NPLimitsSet,
  NPIndividualProfile,
  resolveLimits,
  UNLIMITED_LIMITS,
} from '../types/limits';
import { parseNPPSLimits } from './nppsParser';
import { serializeNPPSLimits } from './nppsSerializer';

// ─── Storage keys ──────────────────────────────────────────────────────────────

const STORAGE_KEY_GLOBAL = 'np_global_limits';
const STORAGE_KEY_HELMET = 'np_helmet_limits';
const STORAGE_KEY_INDIVIDUAL = 'np_individual_limits';
const STORAGE_KEY_PROFILES = 'np_profiles';
const STORAGE_KEY_ACTIVE_PROFILE = 'np_active_profile_id';
const STORAGE_KEY_ACTIVE_HELMET = 'np_active_helmet_id';

function parseLimitsFromNPPS(text: string): NPLimitsSet | null {
  try {
    return parseNPPSLimits(text);
  } catch {
    return null;
  }
}

// ─── LimitsStore ───────────────────────────────────────────────────────────────

class LimitsStore extends EventTarget {
  private _globalLimits: NPLimitsSet | undefined = undefined;
  private _helmetLimits: Map<string, NPLimitsSet> = new Map();
  private _individualLimits: Map<string, NPLimitsSet> = new Map();
  private _profiles: NPIndividualProfile[] = [];
  private _activeProfileId: string | undefined = undefined;
  private _activeHelmetSerial: string | undefined = undefined;

  constructor() {
    super();
    this.load();
  }

  // ─── Getters ────────────────────────────────────────────────────────────────

  get globalLimits(): NPLimitsSet | undefined {
    return this._globalLimits;
  }

  get helmetLimits(): ReadonlyMap<string, NPLimitsSet> {
    return this._helmetLimits;
  }

  get individualLimits(): ReadonlyMap<string, NPLimitsSet> {
    return this._individualLimits;
  }

  get profiles(): readonly NPIndividualProfile[] {
    return this._profiles;
  }

  get activeProfileId(): string | undefined {
    return this._activeProfileId;
  }

  get activeHelmetSerial(): string | undefined {
    return this._activeHelmetSerial;
  }

  get activeProfile(): NPIndividualProfile | undefined {
    return this._profiles.find(p => p.id === this._activeProfileId);
  }

  get activeHelmetLimits(): NPLimitsSet | undefined {
    return this._activeHelmetSerial
      ? this._helmetLimits.get(this._activeHelmetSerial)
      : undefined;
  }

  get activeIndividualLimits(): NPLimitsSet | undefined {
    return this._activeProfileId
      ? this._individualLimits.get(this._activeProfileId)
      : undefined;
  }

  /**
   * The fully resolved limits for the current active context.
   * Applies: individual ?? helmet ?? global for each field.
   */
  get resolvedLimits(): NPLimitsSet {
    return resolveLimits(
      this._globalLimits,
      this.activeHelmetLimits,
      this.activeIndividualLimits
    );
  }

  // ─── Mutation ────────────────────────────────────────────────────────────────

  saveGlobalLimits(limits: NPLimitsSet): void {
    this._globalLimits = { ...limits, level: 'global', modifiedAt: new Date().toISOString() };
    this.persist();
    this.emit();
  }

  clearGlobalLimits(): void {
    this._globalLimits = undefined;
    this.persist();
    this.emit();
  }

  saveHelmetLimits(limits: NPLimitsSet, helmetId: string): void {
    this._helmetLimits.set(helmetId, {
      ...limits,
      level: 'helmet',
      helmetId,
      modifiedAt: new Date().toISOString(),
    });
    this.persist();
    this.emit();
  }

  deleteHelmetLimits(helmetId: string): void {
    this._helmetLimits.delete(helmetId);
    if (this._activeHelmetSerial === helmetId) this._activeHelmetSerial = undefined;
    this.persist();
    this.emit();
  }

  saveIndividualLimits(limits: NPLimitsSet, profileId: string): void {
    this._individualLimits.set(profileId, {
      ...limits,
      level: 'individual',
      individualId: profileId,
      modifiedAt: new Date().toISOString(),
    });
    this.persist();
    this.emit();
  }

  saveProfile(profile: NPIndividualProfile): void {
    const idx = this._profiles.findIndex(p => p.id === profile.id);
    if (idx >= 0) {
      this._profiles[idx] = profile;
    } else {
      this._profiles.push(profile);
    }
    this.persist();
    this.emit();
  }

  deleteProfile(id: string): void {
    this._profiles = this._profiles.filter(p => p.id !== id);
    this._individualLimits.delete(id);
    if (this._activeProfileId === id) this._activeProfileId = undefined;
    this.persist();
    this.emit();
  }

  setActiveProfile(id: string | undefined): void {
    this._activeProfileId = id;
    this.persist();
    this.emit();
  }

  setActiveHelmet(serial: string | undefined): void {
    this._activeHelmetSerial = serial;
    this.persist();
    this.emit();
  }

  // ─── NPPS import/export ──────────────────────────────────────────────────────

  exportLimitsAsNPPS(limits: NPLimitsSet): string {
    return serializeNPPSLimits(limits);
  }

  importLimitsFromNPPS(text: string): NPLimitsSet {
    const result = parseLimitsFromNPPS(text);
    if (!result) throw new Error('No limits block found in script');
    return result;
  }

  downloadLimitsFile(limits: NPLimitsSet): void {
    const text = this.exportLimitsAsNPPS(limits);
    const blob = new Blob([text], { type: 'text/plain' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = `${limits.name.toLowerCase().replace(/\s+/g, '-')}.npps`;
    a.click();
    URL.revokeObjectURL(url);
  }

  /**
   * Returns a human-readable description of the current active resolution chain.
   * e.g. "Individual (Alice) → Helmet (NP-001) → Global"
   */
  get resolutionChainDescription(): string {
    const parts: string[] = [];
    if (this.activeProfile) parts.push(`Individual (${this.activeProfile.name})`);
    if (this._activeHelmetSerial) parts.push(`Helmet (${this._activeHelmetSerial})`);
    if (this._globalLimits) parts.push('Global');
    if (parts.length === 0) return 'No limits configured — hardware limits only';
    return parts.join(' → ');
  }

  // ─── Private ─────────────────────────────────────────────────────────────────

  private emit(): void {
    this.dispatchEvent(new Event('change'));
  }

  private persist(): void {
    try {
      if (this._globalLimits) {
        localStorage.setItem(STORAGE_KEY_GLOBAL, JSON.stringify(this._globalLimits));
      } else {
        localStorage.removeItem(STORAGE_KEY_GLOBAL);
      }

      localStorage.setItem(
        STORAGE_KEY_HELMET,
        JSON.stringify(Object.fromEntries(this._helmetLimits))
      );
      localStorage.setItem(
        STORAGE_KEY_INDIVIDUAL,
        JSON.stringify(Object.fromEntries(this._individualLimits))
      );
      localStorage.setItem(STORAGE_KEY_PROFILES, JSON.stringify(this._profiles));

      if (this._activeProfileId) {
        localStorage.setItem(STORAGE_KEY_ACTIVE_PROFILE, this._activeProfileId);
      } else {
        localStorage.removeItem(STORAGE_KEY_ACTIVE_PROFILE);
      }

      if (this._activeHelmetSerial) {
        localStorage.setItem(STORAGE_KEY_ACTIVE_HELMET, this._activeHelmetSerial);
      } else {
        localStorage.removeItem(STORAGE_KEY_ACTIVE_HELMET);
      }
    } catch (e) {
      console.warn('LimitsStore persist failed:', e);
    }
  }

  private load(): void {
    try {
      const g = localStorage.getItem(STORAGE_KEY_GLOBAL);
      if (g) this._globalLimits = JSON.parse(g) as NPLimitsSet;

      const h = localStorage.getItem(STORAGE_KEY_HELMET);
      if (h) {
        const obj = JSON.parse(h) as Record<string, NPLimitsSet>;
        this._helmetLimits = new Map(Object.entries(obj));
      }

      const ind = localStorage.getItem(STORAGE_KEY_INDIVIDUAL);
      if (ind) {
        const obj = JSON.parse(ind) as Record<string, NPLimitsSet>;
        this._individualLimits = new Map(Object.entries(obj));
      }

      const p = localStorage.getItem(STORAGE_KEY_PROFILES);
      if (p) this._profiles = JSON.parse(p) as NPIndividualProfile[];

      const ap = localStorage.getItem(STORAGE_KEY_ACTIVE_PROFILE);
      this._activeProfileId = ap ?? undefined;

      const ah = localStorage.getItem(STORAGE_KEY_ACTIVE_HELMET);
      this._activeHelmetSerial = ah ?? undefined;
    } catch (e) {
      console.warn('LimitsStore load failed:', e);
    }
  }
}

// ─── Singleton export ──────────────────────────────────────────────────────────

export const limitsStore = new LimitsStore();

// Re-export for convenience
export { UNLIMITED_LIMITS };
