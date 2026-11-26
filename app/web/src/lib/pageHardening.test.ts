/// <reference types="node" />
// The React web app's page policy and its dev server's reach (REQ-SW03-39; NP-PRIV-ANALYSIS-006 F-14,
// F-16). The policy is read from index.html as shipped; whether the built page still loads under it
// was checked by loading `vite build` output in Chromium (NP-PRIV-ANALYSIS-006 §8), which a unit test
// cannot do.

import { describe, it, expect } from 'vitest';
import { readFileSync } from 'node:fs';
import { join, dirname, relative, sep } from 'node:path';
import { fileURLToPath } from 'node:url';

const webRoot = join(dirname(fileURLToPath(import.meta.url)), '..', '..');
const repoRoot = join(webRoot, '..', '..');
const indexHtml = readFileSync(join(webRoot, 'index.html'), 'utf-8');

/** The directives of the page's meta policy, by name. */
function policy(): Map<string, string[]> {
  const m = /<meta http-equiv="Content-Security-Policy" content="([^"]+)"/.exec(indexHtml);
  expect(m, 'index.html carries a Content-Security-Policy').not.toBeNull();
  return new Map(
    m![1].split(';').map(d => d.trim()).filter(Boolean).map(d => {
      const [name, ...sources] = d.split(/\s+/);
      return [name, sources] as [string, string[]];
    }),
  );
}

describe('page policy', () => {
  // @verifies REQ-SW03-39#a
  it('runs only same-origin script and WebAssembly, and nothing injected into the markup', () => {
    const p = policy();
    expect(p.get('default-src')).toEqual(["'self'"]);
    expect(p.get('script-src')).toEqual(["'self'", "'wasm-unsafe-eval'"]);
    expect(p.get('connect-src')).toEqual(["'self'"]);
    expect(p.get('object-src')).toEqual(["'none'"]);
    expect(p.get('base-uri')).toEqual(["'none'"]);
    for (const [name, sources] of p) {
      expect(sources, `${name} allows no inline code`).not.toContain("'unsafe-inline'");
      expect(sources, `${name} allows no eval`).not.toContain("'unsafe-eval'");
      expect(sources.some(s => s === '*' || s.startsWith('http')), `${name} names no other origin`).toBe(false);
    }
  });

  // @verifies REQ-SW03-39#b
  it('sends no referrer', () => {
    expect(indexHtml).toContain('<meta name="referrer" content="no-referrer" />');
  });
});

describe('dev server', () => {
  // @verifies REQ-SW03-39#c
  it('serves this project and the shared code it imports, never the repository root', async () => {
    // The config as Vite reads it. Imported by a computed path: vite.config.ts belongs to the node project
    // (tsconfig.node.json), which this one does not type-check.
    const configPath = join(webRoot, 'vite.config.ts');
    const config = (await import(/* @vite-ignore */ configPath)).default as { server: { fs: { allow: string[] } } };
    const allowed = config.server.fs.allow.map((p: string) => relative(repoRoot, p).split(sep).join('/'));
    expect(allowed).toEqual(['app/web', 'common/lib', 'common/types', 'common/generated']);
    expect(allowed).not.toContain('');
  });
});
