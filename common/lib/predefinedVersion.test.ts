// The release version of the shipped predefined protocols (SWR-PL-05; NP-NPPS-REF-001 "Release version of a
// predefined protocol").
//
// Every predefined protocol is at 1.0 until a release is done, and no release has been done. When the first release
// is recorded this test is the one to change: it then compares each file against the version that release recorded.

import { describe, it, expect } from 'vitest';
import fs from 'node:fs';
import path from 'node:path';

const DIR = path.resolve(__dirname, '../../protocols/predefined');
/** The wavelength rules state a release version of their own (SWR-HV-03, the register `common/npps/wavelength-rules-register.json`); it is not a protocol's, and `common/npps-core/tests/wavelength_version.rs` checks it. */
const RULES_FILE = '00-wavelength-rules.npps';

/** The metadata `version:` of a protocol or composite file, or null for a file that states none (zones, rules). */
function versionOf(text: string): string | null {
  const m = /^[ \t]+version:[ \t]*"([^"]*)"/m.exec(text);
  return m ? m[1] : null;
}

describe('predefined protocol release version', () => {
  const files = fs.readdirSync(DIR).filter(f => f.endsWith('.npps')).sort();
  const stated = files.filter(f => f !== RULES_FILE).map(f => ({ f, v: versionOf(fs.readFileSync(path.join(DIR, f), 'utf8')) })).filter(x => x.v !== null);

  // @verifies SWR-PL-05
  it('ships every predefined protocol and composite at 1.0, because no release has been done', () => {
    // The population is the protocols and composites: the three 00- files are not in it (zones and conditions state no version, and the rules' version is the rules' release, not a protocol's).
    expect(stated.length).toBeGreaterThan(70);
    expect(files.length - stated.length).toBe(3);
    const off = stated.filter(x => x.v !== '1.0').map(x => `${x.f} is ${x.v}`);
    expect(off).toEqual([]);
  });

  // @verifies SWR-PL-05
  it('writes each version as major.minor, both whole numbers', () => {
    for (const { f, v } of stated) {
      expect(v, f).toMatch(/^\d+\.\d+$/);
    }
  });
});
