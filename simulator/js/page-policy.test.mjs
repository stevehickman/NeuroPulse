/**
 * The simulator page asks no third party for anything, and its policy keeps it so
 * (OI-PRIV-44, REQ-SIMFW-27, NP-SIM-FW-001 §35).
 * Run: node --test js/page-policy.test.mjs
 */
import test from 'node:test';
import assert from 'node:assert';
import { readFileSync, readdirSync, statSync } from 'node:fs';
import { createHash } from 'node:crypto';
import { dirname, join, relative, sep } from 'node:path';
import { fileURLToPath } from 'node:url';

const SIM = join(dirname(fileURLToPath(import.meta.url)), '..');
const read = (p) => readFileSync(join(SIM, p), 'utf8');
const html = read('index.html');

// The stylesheets the page links, by their relative paths.
const stylesheets = [...html.matchAll(/<link\b[^>]*rel="stylesheet"[^>]*href="([^"]+)"/g)].map((m) => m[1]);
const importmapText = html.match(/<script type="importmap">([\s\S]*?)<\/script>/)[1];
const csp = html.match(/<meta http-equiv="Content-Security-Policy" content="([^"]+)">/)?.[1];
const directives = Object.fromEntries((csp ?? '').split(';').map((d) => d.trim()).filter(Boolean)
  .map((d) => { const [name, ...values] = d.split(/\s+/); return [name, values]; }));

// A reference that names another origin: a scheme, or a protocol-relative `//host`.
const foreign = (ref) => /^[a-z][a-z0-9+.-]*:/i.test(ref) || ref.startsWith('//');

// @verifies REQ-SIMFW-27#a
test('every script, stylesheet, font and import the page names is a same-origin path', () => {
  const refs = [
    ...[...html.matchAll(/\b(?:src|href)="([^"]+)"/g)].map((m) => ['index.html', m[1]]),
    ...Object.values(JSON.parse(importmapText).imports).map((u) => ['importmap', u]),
  ];
  for (const sheet of stylesheets) {
    const css = read(sheet);
    assert.doesNotMatch(css, /@import/, `${sheet} imports another stylesheet`);
    // A data: URI is inline content, not a request; it is the one scheme allowed (img-src data:).
    for (const m of css.matchAll(/url\(\s*["']?([^"')]+)/g)) if (!m[1].startsWith('data:')) refs.push([sheet, m[1]]);
  }
  assert.ok(refs.length > 5, 'the scan found the page\'s references');
  for (const [where, ref] of refs) assert.ok(!foreign(ref), `${where} names ${ref}`);
  // Every file the importmap and the stylesheets name is in the repository.
  for (const u of Object.values(JSON.parse(importmapText).imports)) {
    const p = u.endsWith('/') ? join(u, 'controls/OrbitControls.js') : u;
    assert.ok(statSync(join(SIM, p)).isFile(), `${u} is vendored`);
  }
});

// @verifies REQ-SIMFW-27#b
test('the CSP allows same-origin scripts, styles and fonts, and the device API WebSocket only', () => {
  assert.ok(csp, 'the page carries a Content-Security-Policy');
  assert.deepStrictEqual(directives['default-src'], ["'none'"]);
  assert.deepStrictEqual(directives['style-src'], ["'self'"]);
  assert.deepStrictEqual(directives['font-src'], ["'self'"]);
  assert.deepStrictEqual(directives['img-src'], ["'self'", 'data:']);
  for (const d of ['base-uri', 'form-action', 'object-src']) assert.deepStrictEqual(directives[d], ["'none'"], d);
  // Scripts: same origin, the inline importmap by its hash, and compiling the NPPS core's WebAssembly.
  const hash = createHash('sha256').update(importmapText, 'utf8').digest('base64');
  assert.deepStrictEqual(directives['script-src'], ["'self'", `'sha256-${hash}'`, "'wasm-unsafe-eval'"],
    'the importmap hash in the CSP is the hash of the importmap as written');
  // Connections: same origin (the protocol library) and the WebSocket js/api.js opens, which must be loopback.
  const wsUrl = read('js/api.js').match(/const WS_URL\s*=\s*'([^']+)'/)[1];
  assert.match(wsUrl, /^ws:\/\/(localhost|127\.0\.0\.1)(:\d+)?\/?$/);
  assert.deepStrictEqual(directives['connect-src'], ["'self'", wsUrl.replace(/\/$/, '')]);
  // No relaxation: no wildcard, no unsafe-inline, no unsafe-eval anywhere.
  assert.doesNotMatch(csp, /\*|'unsafe-inline'|'unsafe-eval'|https?:/);
});

// @verifies REQ-SIMFW-27#b
test('nothing on the page needs what the CSP refuses: no inline script but the importmap, no style attribute', () => {
  const inline = [...html.matchAll(/<script\b(?![^>]*\bsrc=)[^>]*>/g)].map((m) => m[0]);
  assert.deepStrictEqual(inline, ['<script type="importmap">']);
  assert.doesNotMatch(html, /\sstyle="|<style\b|\son[a-z]+="/);
  for (const f of readdirSync(join(SIM, 'js')).filter((f) => f.endsWith('.js'))) {
    assert.doesNotMatch(read(`js/${f}`), /\sstyle="|setAttribute\(\s*['"]style['"]/, `js/${f} writes a style attribute`);
  }
});

// @verifies REQ-SIMFW-27#c
test('the page sends no referrer', () => {
  assert.match(html, /<meta name="referrer" content="no-referrer">/);
});

// @verifies REQ-SIMFW-27#d
test('every vendored file is listed in VENDOR.md with its SHA-256, and matches it', () => {
  const root = join(SIM, 'vendor');
  const listed = new Map([...read('vendor/VENDOR.md').matchAll(/^\| `([^`]+)` \| `([0-9a-f]{64})` \|$/gm)]
    .map((m) => [m[1], m[2]]));
  const walk = (d) => readdirSync(d).flatMap((n) => {
    const p = join(d, n);
    return statSync(p).isDirectory() ? walk(p) : [relative(root, p).split(sep).join('/')];
  });
  const files = walk(root).filter((f) => f !== 'VENDOR.md').sort();
  assert.deepStrictEqual(files, [...listed.keys()].sort(), 'the files and the list agree');
  for (const f of files) {
    const sum = createHash('sha256').update(readFileSync(join(root, f))).digest('hex');
    assert.strictEqual(sum, listed.get(f), `${f} is the file recorded`);
  }
});
