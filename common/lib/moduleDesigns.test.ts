import { describe, expect, it } from 'vitest';
import { readFileSync } from 'fs';
import { join } from 'path';
import designs from '../npps/module-designs.json';

// @verifies-none OI-HWFIT-01 drift guard; the requirement row waits on the open item's decisions (CLAUDE.md §18, §21)
describe('module-designs.json', () => {
  it('has the firmware np_elem_type_t ordinals', () => {
    const header = readFileSync(join(__dirname, '../../firmware/hub_control/include/np_module_map.h'), 'utf8');
    const fw: Record<string, number> = {};
    for (const m of header.matchAll(/NP_ELEM_([A-Z0-9_]+)\s*=\s*(\d+)/g)) fw[m[1]!.toLowerCase()] = Number(m[2]);
    delete fw.none;
    delete fw.type_count;
    expect(designs.elements).toEqual(fw);
  });

  it('gives every module only elements that exist', () => {
    for (const [pn, m] of Object.entries(designs.modules)) {
      for (const e of m.elements) expect(designs.elements, `${pn}: ${e}`).toHaveProperty(e);
    }
  });
});
