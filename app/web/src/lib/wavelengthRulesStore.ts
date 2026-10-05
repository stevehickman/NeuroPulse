// User-editable PBM wavelength rules (NP-NPPS-REF-001 §7a).
//
// The shipped defaults come from protocols/predefined/00-wavelength-rules.npps
// (loaded with the protocol library; DEFAULT_WAVELENGTH_RULES until then). A
// user may override any channel's window: loosen it (a researcher accepting
// 850 nm on the 808 nm channel) or tighten it (exact matches only). The
// override is stored per browser, and the resolved set is what both the
// eligibility check and the compiler use, so what is shown as runnable is what
// compiles.
//
// Storage holds configuration only, nothing about a person. localStorage is a
// per-browser convenience here, like limitsStore: every read is guarded, and a
// missing or unreadable value falls back to the defaults.

import {
  DEFAULT_WAVELENGTH_RULES,
  resolveWavelengthRules,
  validateWavelengthRules,
  type NPWavelengthChannelRule,
  type NPWavelengthRules,
} from '../../../../common/lib/wavelengthRules';
import { parseNPPSFile } from '../../../../common/lib/nppsParser';
import { serializeWavelengthRules } from './nppsSerializer';

const STORAGE_KEY_USER = 'np_wavelength_rules_user';

class WavelengthRulesStore extends EventTarget {
  private _defaults: NPWavelengthRules = DEFAULT_WAVELENGTH_RULES;
  private _user: NPWavelengthRules | null = null;

  constructor() {
    super();
    this.load();
  }

  /** The shipped defaults, as loaded from the protocol library. */
  get defaults(): NPWavelengthRules {
    return this._defaults;
  }

  /** The user's override, or null when the defaults are in force. */
  get userRules(): NPWavelengthRules | null {
    return this._user;
  }

  /** What eligibility and the compiler use: defaults with the user's channels on top. */
  get resolved(): NPWavelengthRules {
    return resolveWavelengthRules(this._defaults, this._user);
  }

  /** Called once the library has loaded 00-wavelength-rules.npps. */
  setDefaults(rules: NPWavelengthRules): void {
    if (validateWavelengthRules(rules).length > 0) return;   // keep the built-in copy
    this._defaults = rules;
    this.emit();
  }

  /**
   * Replace one channel's window in the user's override. Returns the problems
   * with the result, and saves nothing if there are any: an invalid window is
   * never stored, so it can never widen what a channel accepts by accident.
   */
  setChannel(rule: NPWavelengthChannelRule): string[] {
    const current = this._user ?? { name: 'My wavelength rules', level: 'user' as const, channels: [] };
    const next: NPWavelengthRules = {
      ...current,
      channels: [...current.channels.filter(c => c.element !== rule.element), rule],
    };
    const errors = validateWavelengthRules(next);
    if (errors.length > 0) return errors;
    this._user = next;
    this.persist();
    this.emit();
    return [];
  }

  /** Drop the override for one channel, or for all channels. */
  resetChannel(element?: NPWavelengthChannelRule['element']): void {
    if (!this._user) return;
    const channels = element ? this._user.channels.filter(c => c.element !== element) : [];
    this._user = channels.length > 0 ? { ...this._user, channels } : null;
    this.persist();
    this.emit();
  }

  exportUserRules(): string {
    return serializeWavelengthRules(this._user ?? { ...this.resolved, level: 'user' });
  }

  /** Import a `wavelength_rules` block as the user's override. Throws on anything invalid. */
  importUserRules(text: string): void {
    const [rules] = parseNPPSFile(text).wavelengthRules;
    if (!rules) throw new Error('No wavelength_rules block found.');
    this._user = { ...rules, level: 'user' };
    this.persist();
    this.emit();
  }

  private emit(): void {
    this.dispatchEvent(new Event('change'));
  }

  private persist(): void {
    try {
      if (this._user) localStorage.setItem(STORAGE_KEY_USER, JSON.stringify(this._user));
      else localStorage.removeItem(STORAGE_KEY_USER);
    } catch (e) {
      console.warn('WavelengthRulesStore persist failed:', e);
    }
  }

  private load(): void {
    try {
      const raw = localStorage.getItem(STORAGE_KEY_USER);
      if (!raw) return;
      const parsed = JSON.parse(raw) as NPWavelengthRules;
      // A stored override that no longer validates is discarded, not trusted.
      if (parsed && Array.isArray(parsed.channels) && validateWavelengthRules(parsed).length === 0) {
        this._user = { ...parsed, level: 'user' };
      }
    } catch (e) {
      console.warn('WavelengthRulesStore load failed:', e);
    }
  }
}

export const wavelengthRulesStore = new WavelengthRulesStore();
