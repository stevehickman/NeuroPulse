// Every range any app applies is in common/npps/bounds.json, and scripts/sync-constants.ts writes it into each app's
// language. The Compose and iOS apps state their ranges in their own source, so this reads it: a numeric range left in
// a view, or a volume range that does not come from the shared source, is the exact mistake the sweep of 2026-11-15
// found (a volume range above the 85 dB SPL ceiling, the removed 1 to 25 Hz VNS slider, and ranges that disagreed
// between the apps). A test of text is blunt, but it is the only check that runs without a Swift or Kotlin toolchain.
import { describe, it, expect } from 'vitest';
import { readFileSync, readdirSync } from 'node:fs';
import { join } from 'node:path';
import { NPHardwareLimits as hw } from '../../../../common/lib/constants.generated';
import { PARAM_BOUNDS, LIMIT_BOUNDS, allBounds } from './paramBounds';

const repo = join(import.meta.dirname, '..', '..', '..', '..');
const read = (p: string) => readFileSync(join(repo, p), 'utf8');

const IOS_DIR = 'app/ios/NeurOne/Views/Protocol';
const IOS_EDITOR = `${IOS_DIR}/ModalityEditorView.swift`;
const IOS_LIMITS = `${IOS_DIR}/LimitsSettingsView.swift`;
const COMPOSE_LIMITS = 'app/NeurOneUI/shared/src/commonMain/kotlin/life/neurone/shared/ui/LimitsTierEditor.kt';
const BOUNDS_JSON = 'common/npps/bounds.json';

/** The text of one Swift struct, from its declaration to the next struct. */
function swiftStruct(text: string, name: string): string {
  const start = text.indexOf(`struct ${name}`);
  expect(start, `${name} exists`).toBeGreaterThan(-1);
  const next = text.indexOf('\nstruct ', start + 1);
  return text.slice(start, next === -1 ? undefined : next);
}

describe('the one shared source of bounds', () => {
  // @verifies SWR-UI-01#a
  it('is what the web app reads: every bound in bounds.json is in the generated table, and no more', () => {
    const json = JSON.parse(read(BOUNDS_JSON)).bounds as { id: string }[];
    // allBounds() names a parameter bound `<group>.<field>`; the file names it `param.<group>.<field>`.
    const ids = allBounds().map(b => (/^(interval|protocol|limit)\./.test(b.key) ? b.key : `param.${b.key}`)).sort();
    expect(ids).toEqual(json.map(b => b.id).sort());
  });

  // @verifies SWR-UI-01#a
  it('is the only place an iOS view or the Compose limits editor states a numeric range', () => {
    const offenders: string[] = [];
    for (const f of readdirSync(join(repo, IOS_DIR)).filter(n => n.endsWith('.swift'))) {
      const lines = read(`${IOS_DIR}/${f}`).split('\n');
      lines.forEach((line, i) => {
        if (/\brange:\s*-?[0-9.]+\.\.\.|\bin:\s*-?[0-9.]+\.\.\./.test(line)) offenders.push(`${f}:${i + 1}: ${line.trim()}`);
      });
    }
    read(COMPOSE_LIMITS).split('\n').forEach((line, i) => {
      if (/LimitField\.(Num|Whole)\([^,]+,\s*"[^"]*",\s*-?[0-9.]+\.\./.test(line)) offenders.push(`LimitsTierEditor.kt:${i + 1}: ${line.trim().slice(0, 100)}`);
    });
    expect(offenders).toEqual([]);
  });
});

describe('the audio level ceiling in the Compose and iOS apps', () => {
  // @verifies SWR-UI-02#a
  it('is the shared ceiling for the volume control and the volume limit, in every app', () => {
    expect(PARAM_BOUNDS.audio.volume.max.value).toBe(hw.AUDIO_MAX_DB_SPL);
    expect(LIMIT_BOUNDS.audioEntrainment.maxVolumeDb.max.value).toBe(hw.AUDIO_MAX_DB_SPL);
  });

  // @verifies SWR-UI-02#a
  it('is read by the iOS volume slider and the iOS volume limit from the shared source', () => {
    const editor = read(IOS_EDITOR);
    const volume = editor.slice(editor.indexOf('VALIDATE_PARAM_VOLUME'));
    expect(volume.match(/range: ([^,\n]+)/)![1]).toBe('NPBounds.Param.Audio.volume.range');
    const limits = read(IOS_LIMITS);
    const limit = limits.slice(limits.indexOf('WEB_LIM_MAX_VOLUME'));
    expect(limit.match(/range: ([^)\n]+)\)/)![1]).toBe('NPBounds.Limit.AudioEntrainment.maxVolumeDb.range');
  });

  // @verifies SWR-UI-02#a
  it('is read by the Compose volume limit from the shared source', () => {
    const line = read(COMPOSE_LIMITS).split('\n').find(l => l.includes('web_lim_max_volume'))!;
    expect(line).toContain('NPBounds.Limit.AudioEntrainment.maxVolumeDb.range');
  });
});

describe('the auricular VNS frequency', () => {
  // @verifies SWR-UI-02#b
  it('has no stated bounds in the shared source', () => {
    const b = PARAM_BOUNDS.vns.frequency as { min?: { value: number }; max?: unknown };
    expect(b.max).toBeUndefined();
    expect(b.min?.value).toBe(0); // natural: not below 0, and nothing above it
  });

  // @verifies SWR-UI-02#b
  it('is a typed field in the iOS editor, not the removed 1 to 25 Hz slider', () => {
    const vns = swiftStruct(read(IOS_EDITOR), 'VNSHRVParamsView');
    expect(vns).not.toMatch(/range:\s*1\.\.\.25/);
    expect(vns).toMatch(/NumberFieldRow\(\s*label: String\(localized: "AND_MODALITY_FREQUENCY_HZ"\),\s*unit: "Hz",\s*value: \$params\.frequencyHz/);
    // The cervical VNS slider keeps its range: 1 to 25 Hz is that device's predicate range, now from the shared source.
    expect(swiftStruct(read(IOS_EDITOR), 'CervicalVnsParamsView')).toContain('NPBounds.Param.CervicalVns.frequency.range');
  });
});
