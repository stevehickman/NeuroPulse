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

describe('a typed clinician limit in the Compose and iOS apps', () => {
  const swiftFields = read(IOS_LIMITS);
  const checker = read('app/ios/NeurOne/Protocol/NPLimitRangeCheck.swift');
  const compose = read(COMPOSE_LIMITS);

  // @verifies SWR-UI-01#f
  it('is never pulled into its range by the iOS limit fields', () => {
    for (const name of ['OptionalDoubleField', 'OptionalIntField']) {
      const body = swiftStruct(swiftFields, name);
      expect(body, `${name} clamps a typed value`).not.toMatch(/max\(range\.lowerBound,\s*min\(/);
      expect(body, `${name} marks a value outside its range`).toMatch(/isOutOfRange/);
    }
  });

  // @verifies SWR-UI-01#f
  it('is never pulled into its range by the Compose limit field, which marks it as an error instead', () => {
    expect(compose).not.toMatch(/coerceIn/);
    expect(compose).not.toMatch(/onFocusChanged/);
    expect(compose).toMatch(/isError = outOfRange/);
  });

  // @verifies SWR-UI-01#f
  it('blocks Save and a script import in iOS, and Save in Compose, while a limit is outside its range', () => {
    expect(swiftFields).toMatch(/\.disabled\(!outOfRange\.isEmpty\)/);
    expect(swiftFields).toMatch(/\.disabled\(parsedLimits == nil \|\| parsedOutOfRange\)/);
    expect(compose).toMatch(/enabled = !outOfRange/);
  });

  // @verifies SWR-UI-01#f
  it('is checked in iOS for every range the iOS limit fields read, so the check cannot drift from the fields', () => {
    const ref = /NPBounds\.Limit\.(\w+)\.(\w+)/g;
    const fields = new Set([...swiftFields.matchAll(ref)].map(m => `${m[1]}.${m[2]}`));
    const checked = new Set([...checker.matchAll(ref)].map(m => `${m[1]}.${m[2]}`));
    expect(fields.size).toBeGreaterThan(0);
    expect([...fields].filter(f => !checked.has(f))).toEqual([]);
    expect([...checked].filter(f => !fields.has(f))).toEqual([]);
  });
});

describe('a typed protocol duration in the Compose editor', () => {
  const screen = read('app/NeurOneUI/shared/src/commonMain/kotlin/life/neurone/shared/ui/ProtocolEditorScreen.kt');

  // @verifies SWR-UI-01#f
  it('is never replaced by a default: no 20 minutes for a blank field, no pull up to 1 minute', () => {
    expect(screen).not.toMatch(/\?:\s*20\b/);
    expect(screen).not.toMatch(/coerceAtLeast\(1\)/);
    expect(screen).not.toMatch(/IntervalCount\s*->\s*"20"/);
  });

  // @verifies SWR-UI-01#f
  it('is refused instead: the field is marked and Save is disabled, from the shared timing bound', () => {
    expect(screen).toMatch(/durationSecondsOrNull/);
    expect(screen).toMatch(/NPBounds\.ProtocolFields\.timingValue/);
    expect(screen).toMatch(/isError = durationRefused/);
    expect(screen).toMatch(/enabled = !durationRefused/);
  });
});

describe('the binaural and isochronic tones in the Compose editor', () => {
  const editor = read('app/NeurOneUI/shared/src/commonMain/kotlin/life/neurone/shared/ui/ModalityEditorScreen.kt');

  // @verifies SWR-UI-01#f
  it('are present or absent by a switch, as on iOS, and a typed 0 is stored as 0, never turned into none', () => {
    expect(editor).not.toMatch(/takeIf\s*\{\s*\w+\s*->\s*\w+\s*>\s*0\s*\}/);
    expect(editor).not.toMatch(/(binauralBeatsHz|isochronicTonesHz)\s*\?:\s*0\.0/);
    expect(editor).toMatch(/OptionalTone\(.*p\.binauralBeatsHz/);
    expect(editor).toMatch(/OptionalTone\(.*p\.isochronicTonesHz/);
    expect(editor).toMatch(/if \(on\) startHz else null/);
  });

  // @verifies SWR-UI-01#f
  it('start at the rates iOS starts them at when the switch is turned on', () => {
    const ios = read(IOS_EDITOR);
    expect(ios).toMatch(/binauralBeatsHz = \$0 \? 20 : nil/);
    expect(ios).toMatch(/isochronicTonesHz = \$0 \? 40 : nil/);
    expect(editor).toMatch(/BINAURAL_START_HZ = 20\.0/);
    expect(editor).toMatch(/ISOCHRONIC_START_HZ = 40\.0/);
  });
});
