/**
 * The passphrase normalisation through WebAssembly: the corpus of known NFKC answers
 * (app/NeurOneShared/TestData/nfkc-corpus.json) that the Rust core, Kotlin, Swift and C# run too. JavaScript strings are UTF-16
 * and the module takes UTF-8, so the cases outside the BMP (surrogate pairs) are the ones this proves (SWR-MG-02,
 * NP-SOUP-RUST-001).
 */
import { readFileSync } from 'node:fs';
import { describe, expect, it } from 'vitest';
import { passphraseSecret } from './bundleKey';
import { initNppsCore } from './nppsCore';

interface Case {
  name: string;
  input: string;
  expected?: string;
}
const corpus = JSON.parse(readFileSync(new URL('../../app/NeurOneShared/TestData/nfkc-corpus.json', import.meta.url), 'utf8')) as {
  padding: string;
  minChars: number;
  cases: Case[];
  exact: Case[];
  tooShort: Case[];
};

const hexOf = (text: string) => [...new TextEncoder().encode(text)].map((b) => b.toString(16).padStart(2, '0')).join('');

describe('the NFKC corpus', () => {
  // @verifies SWR-MG-02
  it('holds every known answer through the binding', async () => {
    await initNppsCore();
    const wrong: string[] = [];
    for (const c of corpus.cases) {
      const got = passphraseSecret(corpus.padding + c.input);
      const want = hexOf(corpus.padding + c.expected);
      if (!got.ok || got.secret !== want) wrong.push(`${c.name}: ${JSON.stringify(got)} != ${want}`);
    }
    for (const c of corpus.exact) {
      const got = passphraseSecret(c.input);
      if (!got.ok || got.secret !== hexOf(c.expected as string)) wrong.push(`${c.name}: the floor is counted wrongly`);
    }
    for (const c of corpus.tooShort) {
      const got = passphraseSecret(c.input);
      if (got.ok || got.reason !== 'too_short') wrong.push(`${c.name}: accepted under the floor`);
    }
    expect(wrong, wrong.join('\n')).toEqual([]);
    expect(corpus.minChars).toBe(12);
  });
});
