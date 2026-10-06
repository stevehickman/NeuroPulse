/**
 * The shared NPPS core (common/npps-core, OI-NPPS-CORE-01) as the web app and the simulator see it: one parser,
 * namespace builder and hub-descriptor compiler written once in Rust, built as WebAssembly by scripts/build-npps-wasm.sh from the C ABI of
 * common/npps-ffi (common/npps-ffi/include/neurone_npps.h). This file marshals and nothing else. What an
 * input means is decided in the core, so it means the same here as on every other runtime, and a refusal
 * carries the message the core gives.
 *
 * Loading. A browser cannot compile a module this size synchronously on the main thread, so the page
 * awaits `initNppsCore()` once before the first compile. Node can, so there the first call loads the
 * module itself and tests and scripts need no setup.
 */

/** The core refused the input. `message` is the refusal the core wrote. */
export class NppsRefusal extends Error {
  constructor(message: string) {
    super(message);
    this.name = 'NppsRefusal';
  }
}

interface CoreExports {
  memory: WebAssembly.Memory;
  npps_alloc(len: number): number;
  npps_free(ptr: number, len: number): void;
  npps_parse_json(src: number, srcLen: number, out: number, outLen: number): number;
  npps_compile_json(req: number, reqLen: number, out: number, outLen: number): number;
  npps_namespace_json(req: number, reqLen: number, out: number, outLen: number): number;
}

// common/generated/ is a git-ignored build output directory (CLAUDE.md §20). In the simulator's bundle
// (simulator/js/vendor/npps-runtime.js) this resolves to simulator/js/generated/, where the build script
// also writes the module.
const WASM_URL = new URL('../generated/neurone_npps.wasm', import.meta.url);

let instance: CoreExports | null = null;

function instantiate(module: WebAssembly.Module): CoreExports {
  // The core imports nothing: it has no clock, no randomness and no I/O.
  return new WebAssembly.Instance(module, {}).exports as unknown as CoreExports;
}

/** Load the core. Await this once at startup in a browser; Node loads it on first use without it. */
export async function initNppsCore(): Promise<void> {
  if (instance) return;
  const nodeFs = nodeFileSystem();
  const module = nodeFs
    ? new WebAssembly.Module(nodeFs.readFileSync(nodeFilePath()))
    : await WebAssembly.compileStreaming(fetch(WASM_URL));
  instance = instantiate(module);
}

function nodeFileSystem(): { readFileSync(path: string): Uint8Array<ArrayBuffer> } | null {
  const proc = (globalThis as { process?: { versions?: { node?: string }; getBuiltinModule?: (id: string) => unknown } }).process;
  if (!proc?.versions?.node || typeof proc.getBuiltinModule !== 'function') return null;
  return proc.getBuiltinModule('node:fs') as { readFileSync(path: string): Uint8Array<ArrayBuffer> };
}

function nodeFilePath(): string {
  const proc = (globalThis as { process: { getBuiltinModule(id: string): unknown } }).process;
  return (proc.getBuiltinModule('node:url') as { fileURLToPath(u: URL): string }).fileURLToPath(WASM_URL);
}

function core(): CoreExports {
  if (instance) return instance;
  const nodeFs = nodeFileSystem();
  if (!nodeFs) throw new Error('the NPPS core is not loaded: await initNppsCore() before compiling');
  try {
    instance = instantiate(new WebAssembly.Module(nodeFs.readFileSync(nodeFilePath())));
  } catch (e) {
    throw new Error(`the NPPS core could not be loaded (run scripts/build-npps-wasm.sh): ${(e as Error).message}`);
  }
  return instance;
}

// Return codes of neurone_npps.h.
const OK = 0, REFUSED = 1, INTERNAL = 2;

function call(entry: 'npps_parse_json' | 'npps_compile_json' | 'npps_namespace_json', input: string): Uint8Array {
  const wasm = core();
  const bytes = new TextEncoder().encode(input);
  // Never empty, so the pointer handed over is never null (an empty source is a valid input).
  const inPtr = wasm.npps_alloc(bytes.length + 1);
  const cells = wasm.npps_alloc(8);   // out pointer, out length
  try {
    new Uint8Array(wasm.memory.buffer, inPtr, bytes.length).set(bytes);
    let code: number;
    try {
      code = wasm[entry](inPtr, bytes.length, cells, cells + 4);
    } catch (e) {
      // A panic is a trap here (the module is built with panic=abort), and what it left behind
      // cannot be trusted: drop the instance so the next call starts clean.
      instance = null;
      throw new Error(`the NPPS core failed: ${(e as Error).message}`);
    }
    const view = new DataView(wasm.memory.buffer);
    const outPtr = view.getUint32(cells, true);
    const outLen = view.getUint32(cells + 4, true);
    const out = outPtr === 0 ? new Uint8Array(0) : new Uint8Array(wasm.memory.buffer, outPtr, outLen).slice();
    if (outPtr !== 0) wasm.npps_free(outPtr, outLen);
    switch (code) {
      case OK: return out;
      case REFUSED:
      case INTERNAL:
        throw new NppsRefusal(out.length > 0 ? new TextDecoder().decode(out) : 'the NPPS core refused the input');
      default:
        throw new NppsRefusal('the NPPS core was given an argument it cannot read');
    }
  } finally {
    wasm.npps_free(inPtr, bytes.length + 1);
    wasm.npps_free(cells, 8);
  }
}

/** Compile a request (see neurone_npps_core::api::compile_json) into the NP-FW-HUB-001 §4 descriptor. */
export function nppsCompile(request: object): Uint8Array {
  return call('npps_compile_json', JSON.stringify(request));
}

/**
 * Parse NPPS text into everything the file declares: `{entries, zones, conditions, wavelengthRules, limits}`
 * (neurone_npps_core::api::parse_json). Throws NppsRefusal carrying `Line N: …`.
 */
export function nppsParse(source: string): NppsParsed {
  return JSON.parse(new TextDecoder().decode(call('npps_parse_json', source)));
}

/** Fold parse results into one namespace and check its references (neurone_npps_core::api::namespace_json). */
export function nppsNamespace(files: readonly object[]): NppsNamespace {
  return JSON.parse(new TextDecoder().decode(call('npps_namespace_json', JSON.stringify({ files }))));
}

/** What the core's parse returns. Models are built from it by common/lib/nppsParser.ts. */
export interface NppsParsed {
  // eslint-disable-next-line @typescript-eslint/no-explicit-any
  entries: Array<{ kind: 'single'; protocol: any } | { kind: 'composite'; composite: any }>;
  // eslint-disable-next-line @typescript-eslint/no-explicit-any
  zones: any[]; conditions: any[]; wavelengthRules: any[]; limits: any[];
}

export interface NppsNamespace {
  // eslint-disable-next-line @typescript-eslint/no-explicit-any
  entries: any[]; zones: any[]; conditions: any[]; errors: string[]; referenceErrors: string[];
}
