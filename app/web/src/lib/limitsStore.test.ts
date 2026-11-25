import { describe, it, expect, beforeAll } from 'vitest';
import { initNppsCore } from '../../../../common/lib/nppsCore';
import { limitsFileName, limitsStore, newLimitsSet } from './limitsStore';

beforeAll(async () => { await initNppsCore(); });

describe('a limits set given to someone else names no helmet and no person', () => {
  // @verifies SWR-WO-07#g
  it('names a new set for its level, never after the helmet it is saved for', () => {
    const set = newLimitsSet('helmet');
    limitsStore.saveHelmetLimits(set, 'NP-SERIAL-0042');
    const saved = limitsStore.helmetLimits.get('NP-SERIAL-0042');
    expect(saved?.name).toBe('Helmet Limits');
    expect(limitsStore.shareLimitsAsNPPS(saved!)).not.toContain('NP-SERIAL-0042');
    limitsStore.deleteHelmetLimits('NP-SERIAL-0042');
  });

  // @verifies SWR-WO-07#g
  it('names the downloaded file for the level, whatever the set is called', () => {
    const set = { ...newLimitsSet('individual'), name: 'Alex Example NP-SERIAL-0042' };
    expect(limitsFileName(set)).toBe('individual-limits.npps');
    expect(limitsFileName(newLimitsSet('helmet'))).toBe('helmet-limits.npps');
    expect(limitsFileName(newLimitsSet('global'))).toBe('global-limits.npps');
  });
});
