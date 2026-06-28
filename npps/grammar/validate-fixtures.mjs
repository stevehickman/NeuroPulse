#!/usr/bin/env node
/**
 * Validates that the PEG-generated parser can parse all .npps fixtures.
 * Run: node npps/grammar/validate-fixtures.mjs
 *
 * Exit 0 = all fixtures parse (or correctly fail for error_ fixtures).
 * Exit 1 = unexpected parse failure or unexpected parse success on error fixtures.
 */

import { parse } from './npps-parser.mjs';
import { readFileSync, readdirSync } from 'fs';
import { join, dirname } from 'path';
import { fileURLToPath } from 'url';

const __dirname = dirname(fileURLToPath(import.meta.url));
const fixturesDir = join(__dirname, '..', 'fixtures');

const files = readdirSync(fixturesDir).filter(f => f.endsWith('.npps'));
let pass = 0;
let fail = 0;

for (const f of files) {
  const input = readFileSync(join(fixturesDir, f), 'utf8');
  const isErrorFixture = f.startsWith('error_');

  try {
    const result = parse(input);
    if (isErrorFixture) {
      console.error(`FAIL: ${f} — expected parse error but got ${result.length} entries`);
      fail++;
    } else {
      console.log(`PASS: ${f} (${result.length} entries)`);
      pass++;
    }
  } catch (e) {
    if (isErrorFixture) {
      console.log(`PASS: ${f} (expected error: ${e.message.slice(0, 60)})`);
      pass++;
    } else {
      console.error(`FAIL: ${f} — ${e.message}`);
      fail++;
    }
  }
}

console.log(`\n${pass} passed, ${fail} failed`);
process.exit(fail > 0 ? 1 : 0);
