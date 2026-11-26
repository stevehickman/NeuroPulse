// The predefined library loads whole or not at all (REQ-NPPS-34, NP-NPPS-REF-001 §1.6). These tests run the
// real loader and the real core over the canonical protocols/ tree, served by a stubbed fetch, so a file that
// fails to load can be made to fail on purpose.

import { describe, it, expect, beforeEach, afterEach, vi } from 'vitest';
import fs from 'node:fs';
import path from 'node:path';

// The loader must await the core before it parses anything. In Node the core loads itself on first use, so
// the gate below stands in for a browser's WebAssembly compile that has not finished yet: a test can hold it
// shut and see whether anything is parsed meanwhile.
const gate = vi.hoisted(() => ({ hold: null as Promise<void> | null, parsedWhileShut: 0, shut: false }));

vi.mock('../../../../common/lib/nppsCore', async (importActual) => {
  const actual = await importActual<typeof import('../../../../common/lib/nppsCore')>();
  return {
    ...actual,
    initNppsCore: async () => {
      if (gate.hold) await gate.hold;
      await actual.initNppsCore();
    },
  };
});

vi.mock('../../../../common/lib/nppsParser', async (importActual) => {
  const actual = await importActual<typeof import('../../../../common/lib/nppsParser')>();
  return {
    ...actual,
    parseNPPSFile: (text: string) => {
      if (gate.shut) gate.parsedWhileShut++;
      return actual.parseNPPSFile(text);
    },
  };
});

import {
  loadPredefinedProtocols,
  getPredefinedNamespace,
  getCachedPredefinedProtocols,
  PredefinedLoadError,
  resetPredefinedProtocolsForTest,
} from './predefinedProtocols';
import { validateNamespaceReferences } from '../../../../common/lib/nppsParser';
import { entryIsPredefined } from '../../../../common/types/protocol';
import { ProtocolLibrary, type KeyValueStorage } from './protocolLibrary';

const DIR = path.resolve(__dirname, '../../../../protocols/predefined');
const PREFIX = '/protocols/predefined/';

type Override = (file: string) => Response | Error | undefined;

/** A fetch that serves protocols/predefined/ off disk, unless `override` answers for a file first. */
function serve(override: Override = () => undefined) {
  const requests: string[] = [];
  const fetchStub = vi.fn(async (url: string) => {
    requests.push(url);
    const file = url.startsWith(PREFIX) ? url.slice(PREFIX.length) : url;
    const forced = override(file);
    if (forced instanceof Error) throw forced;
    if (forced) return forced;
    const fp = path.join(DIR, file);
    if (!fs.existsSync(fp)) return new Response('<!doctype html><title>index</title>', { status: 404 });
    return new Response(fs.readFileSync(fp, 'utf8'), { status: 200 });
  });
  vi.stubGlobal('fetch', fetchStub);
  return { requests };
}

class MemoryStorage implements KeyValueStorage {
  private m = new Map<string, string>();
  getItem(k: string) { return this.m.get(k) ?? null; }
  setItem(k: string, v: string) { this.m.set(k, v); }
}

const failureOf = async (p: Promise<unknown>): Promise<PredefinedLoadError> => {
  const e = await p.then(() => null, (err: unknown) => err);
  expect(e).toBeInstanceOf(PredefinedLoadError);
  return e as PredefinedLoadError;
};

beforeEach(() => {
  resetPredefinedProtocolsForTest();
  gate.hold = null;
  gate.shut = false;
  gate.parsedWhileShut = 0;
  // A failed load is reported on the console by design; keep the test output to the results.
  vi.spyOn(console, 'error').mockImplementation(() => {});
});

afterEach(() => {
  vi.unstubAllGlobals();
  vi.restoreAllMocks();
});

describe('the predefined library loads whole or not at all (REQ-NPPS-34)', () => {
  // @verifies REQ-NPPS-34#a
  it('parses nothing until the core has loaded, then loads every zone and condition', async () => {
    serve();
    let open!: () => void;
    gate.hold = new Promise<void>(resolve => { open = resolve; });
    gate.shut = true;

    const loading = loadPredefinedProtocols();
    // Let every fetch the loader could make while the core is still loading complete.
    for (let i = 0; i < 20; i++) await new Promise(resolve => setTimeout(resolve, 0));
    expect(gate.parsedWhileShut).toBe(0);
    expect(getPredefinedNamespace()).toBeNull();

    gate.shut = false;
    open();
    const entries = await loading;
    expect(entries.length).toBeGreaterThan(0);
    const ns = getPredefinedNamespace()!;
    expect(ns.zones.has('Frontal')).toBe(true);
    expect(ns.conditions.size).toBeGreaterThan(0);
    expect(validateNamespaceReferences(ns)).toEqual([]);
  });

  // @verifies REQ-NPPS-34#b,c
  it.each([
    ['00-zones.npps', 'a server error', () => new Response('nope', { status: 500 }), 'HTTP 500'],
    ['00-conditions.npps', 'a network error', () => new TypeError('Failed to fetch'), 'Failed to fetch'],
    ['00-zones.npps', 'an index page served for a missing file', () => new Response('<!doctype html>', { status: 404 }), 'HTTP 404'],
  ])('a definitions file that fails to load (%s, %s) fails the load and leaves nothing loaded', async (file, _what, answer, why) => {
    serve(f => (f === file ? answer() : undefined));
    const e = await failureOf(loadPredefinedProtocols());
    expect(e.failures).toEqual([`${file}: ${why}`]);
    expect(getPredefinedNamespace()).toBeNull();
    expect(getCachedPredefinedProtocols()).toEqual([]);
    expect(console.error).toHaveBeenCalledWith(expect.stringContaining('failed to load'), e.failures);
  });

  // @verifies REQ-NPPS-34#b
  it('a file that does not parse fails the load and is named', async () => {
    serve(f => (f === '00-zones.npps' ? new Response('zone {{{ this is not NPPS', { status: 200 }) : undefined));
    const e = await failureOf(loadPredefinedProtocols());
    expect(e.failures).toHaveLength(1);
    expect(e.failures[0]).toMatch(/^00-zones\.npps: /);
    expect(getPredefinedNamespace()).toBeNull();
  });

  // @verifies REQ-NPPS-34#d
  it('starts again after a failure, and concurrent callers share one load', async () => {
    let failing = true;
    const { requests } = serve(f => (failing && f === '00-zones.npps' ? new Response('', { status: 503 }) : undefined));
    await failureOf(loadPredefinedProtocols());

    failing = false;
    requests.length = 0;
    const [a, b] = await Promise.all([loadPredefinedProtocols(), loadPredefinedProtocols()]);
    expect(a).toBe(b);
    expect(requests.filter(u => u.endsWith('manifest.json'))).toHaveLength(1);
    expect(getPredefinedNamespace()!.zones.has('Frontal')).toBe(true);
  });

  // @verifies REQ-NPPS-34#c,d
  it('the protocol library offers no predefined protocol after a failed load, says so, and retries on request', async () => {
    let failing = true;
    serve(f => (failing && f === '00-conditions.npps' ? new Response('', { status: 500 }) : undefined));
    const lib = new ProtocolLibrary(new MemoryStorage());
    await vi.waitFor(() => expect(lib.predefinedLoadFailed).toBe(true));
    expect(lib.isLoaded).toBe(false);
    expect(lib.allProtocols.filter(entryIsPredefined)).toEqual([]);

    failing = false;
    lib.retryPredefinedLoad();
    expect(lib.predefinedLoadFailed).toBe(false);
    await vi.waitFor(() => expect(lib.isLoaded).toBe(true));
    expect(lib.predefinedLoadFailed).toBe(false);
    expect(lib.allProtocols.length).toBeGreaterThan(0);
  });
});
