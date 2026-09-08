import { defineConfig, type Plugin } from 'vite';
import react from '@vitejs/plugin-react';
import { readFile, cp } from 'node:fs/promises';
import { execFileSync } from 'node:child_process';
import { join, resolve, normalize, sep } from 'node:path';
import { fileURLToPath } from 'node:url';

// Repo root, from app/web/vite.config.ts → app/web → app → <root>.
const REPO_ROOT = resolve(fileURLToPath(import.meta.url), '..', '..', '..');
const PROTOCOLS_DIR = join(REPO_ROOT, 'protocols');
const URL_PREFIX = '/protocols/';

/**
 * Serves the canonical `protocols/` tree at `/protocols/*` — NP-NPPS-REF-001.
 *
 * `protocols/` is the single source of truth for .npps definitions; it is also
 * read directly by scripts/sync-socket-map.ts and by the test suites. The web
 * app previously kept a byte-identical copy under `public/protocols/`, which meant every
 * protocol edit had to be made twice and silently rotted when it wasn't.
 *
 * Instead of copying into the repo:
 *   - dev   — a middleware streams files straight from `protocols/`, so the
 *             served content is always current with no copy step at all;
 *   - build — the tree is copied into the bundle output, since the deployed
 *             app has no repo to read from.
 *
 * Vite's own `publicDir` cannot do this: it is a single directory and does not
 * reach outside the project root.
 */
function canonicalProtocols(): Plugin {
  let outDir = 'dist';
  let isBuild = false;

  return {
    name: 'neurone-canonical-protocols',

    configResolved(config) {
      outDir = config.build.outDir;
      // `closeBundle` fires under vitest too, and vitest replaces build.outDir
      // with the literal `dummy-non-existing-folder`. Copying there materialised
      // a second, untracked copy of the whole protocols tree on every test run —
      // exactly the duplicate this plugin exists to abolish.
      isBuild = config.command === 'build' && !process.env.VITEST;
    },

    configureServer(server) {
      server.middlewares.use((req, res, next) => {
        const path = (req.url ?? '').split('?')[0];
        if (!path.startsWith(URL_PREFIX)) return next();

        const abs = normalize(
          join(PROTOCOLS_DIR, decodeURIComponent(path.slice(URL_PREFIX.length))),
        );

        // The request path is attacker-controllable in principle; refuse
        // anything that escapes the protocols tree rather than serving it.
        if (abs !== PROTOCOLS_DIR && !abs.startsWith(PROTOCOLS_DIR + sep)) {
          res.statusCode = 403;
          return res.end('Forbidden');
        }

        readFile(abs)
          .then(body => {
            res.setHeader(
              'Content-Type',
              abs.endsWith('.json') ? 'application/json' : 'text/plain; charset=utf-8',
            );
            // These change during development; never let a stale copy be cached.
            res.setHeader('Cache-Control', 'no-store');
            res.end(body);
          })
          .catch(next);
      });
    },

    async closeBundle() {
      if (!isBuild) return;
      const dest = join(REPO_ROOT, 'app', 'web', outDir, 'protocols');
      await cp(PROTOCOLS_DIR, dest, { recursive: true });
    },
  };
}

/**
 * Regenerates src/generated/locales/*.json from canonical locales/*.json —
 * CLAUDE.md §17.
 *
 * Same argument as canonicalProtocols above, one step further. The web copies
 * used to be committed, which made them a second place a string could be
 * edited; now they are build output and the repository carries locales/*.json
 * alone. Nothing else changes for the app: they are still ordinary module
 * imports, resolved by Vite and type-checked by tsc, so they have to exist on
 * disk inside the project before either runs.
 *
 * `buildStart` is the hook that covers every entry point that matters — `vite`,
 * `vite build` and `vitest` all fire it — and it runs before module resolution,
 * so the first import of a locale JSON already finds the file. The one path it
 * does not cover is a bare `tsc --noEmit`, which never loads this config; CI
 * therefore runs the generator as an explicit step before type-checking.
 *
 * The generator is spawned rather than imported: it is a Bun script
 * (import.meta.dir, top-level main()) in a Bun-only repository, and
 * reimplementing it here would put a third string pipeline in the tree to keep
 * in sync with the other two.
 */
function canonicalLocales(): Plugin {
  return {
    name: 'neurone-canonical-locales',

    // Vite may run several builds in one process (SSR passes, vitest projects).
    // Generating once per process is enough: canonical cannot change mid-run.
    buildStart: (() => {
      let done = false;
      return function generateOnce() {
        if (done) return;
        done = true;
        const script = join(REPO_ROOT, 'scripts', 'sync-locales.ts');
        try {
          execFileSync('bun', [script], { cwd: REPO_ROOT, stdio: 'inherit' });
        } catch (err) {
          // Failing loudly here is the point: a silent skip yields a build whose
          // every string is the raw key, which looks like an i18n bug anywhere
          // but the place that caused it.
          throw new Error(
            `sync-locales failed — locale files could not be generated from ` +
              `locales/*.json. Is bun on PATH? (${(err as Error).message})`,
          );
        }
      };
    })(),
  };
}

export default defineConfig({
  plugins: [react(), canonicalLocales(), canonicalProtocols()],
  test: {
    environment: 'node',
    include: ['src/**/*.test.ts'],
  },
});
