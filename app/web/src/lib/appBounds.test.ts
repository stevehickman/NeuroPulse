// Every range any app applies is in common/npps/bounds.json, and scripts/sync-constants.ts writes it into each app's
// language. The Compose app states its ranges in its own source, so this reads it: a numeric range left in
// a view, or a volume range that does not come from the shared source, is the exact mistake the sweep of 2026-11-15
// found (a volume range above the 85 dB SPL ceiling, the removed 1 to 25 Hz VNS slider, and ranges that disagreed
// between the apps). A test of text is blunt, but it is the only check that runs without a Kotlin toolchain.
import { describe, it, expect } from 'vitest';
import { readFileSync } from 'node:fs';
import { join } from 'node:path';
import { NPHardwareLimits as hw } from '../../../../common/lib/constants.generated';
import { PARAM_BOUNDS, LIMIT_BOUNDS, allBounds } from './paramBounds';

const repo = join(import.meta.dirname, '..', '..', '..', '..');
const read = (p: string) => readFileSync(join(repo, p), 'utf8');

const COMPOSE_LIMITS = 'app/NeurOneUI/shared/src/commonMain/kotlin/life/neurone/shared/ui/LimitsTierEditor.kt';
const BOUNDS_JSON = 'common/npps/bounds.json';

describe('the one shared source of bounds', () => {
  // @verifies SWR-UI-01#a
  it('is what the web app reads: every bound in bounds.json is in the generated table, and no more', () => {
    const json = JSON.parse(read(BOUNDS_JSON)).bounds as { id: string }[];
    // allBounds() names a parameter bound `<group>.<field>`; the file names it `param.<group>.<field>`.
    const ids = allBounds().map(b => (/^(interval|protocol|limit)\./.test(b.key) ? b.key : `param.${b.key}`)).sort();
    expect(ids).toEqual(json.map(b => b.id).sort());
  });

  // @verifies SWR-UI-01#a
  it('is the only place the Compose limits editor states a numeric range', () => {
    const offenders: string[] = [];
    read(COMPOSE_LIMITS).split('\n').forEach((line, i) => {
      if (/LimitField\.(Num|Whole)\([^,]+,\s*"[^"]*",\s*-?[0-9.]+\.\./.test(line)) offenders.push(`LimitsTierEditor.kt:${i + 1}: ${line.trim().slice(0, 100)}`);
    });
    expect(offenders).toEqual([]);
  });
});

describe('the audio level ceiling in the Compose app', () => {
  // @verifies SWR-UI-02#a
  it('is the shared ceiling for the volume control and the volume limit, in every app', () => {
    expect(PARAM_BOUNDS.audio.volume.max.value).toBe(hw.AUDIO_MAX_DB_SPL);
    expect(LIMIT_BOUNDS.audioEntrainment.maxVolumeDb.max.value).toBe(hw.AUDIO_MAX_DB_SPL);
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
});

describe('a typed clinician limit in the Compose app', () => {
  const compose = read(COMPOSE_LIMITS);

  // @verifies SWR-UI-01#f
  it('is never pulled into its range by the Compose limit field, which marks it as an error instead', () => {
    expect(compose).not.toMatch(/coerceIn/);
    expect(compose).not.toMatch(/onFocusChanged/);
    expect(compose).toMatch(/isError = outOfRange/);
  });

  // @verifies SWR-UI-01#f
  it('blocks Save in Compose while a limit is outside its range', () => {
    expect(compose).toMatch(/enabled = !outOfRange/);
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
  it('are present or absent by a switch, and a typed 0 is stored as 0, never turned into none', () => {
    expect(editor).not.toMatch(/takeIf\s*\{\s*\w+\s*->\s*\w+\s*>\s*0\s*\}/);
    expect(editor).not.toMatch(/(binauralBeatsHz|isochronicTonesHz)\s*\?:\s*0\.0/);
    expect(editor).toMatch(/OptionalTone\(.*p\.binauralBeatsHz/);
    expect(editor).toMatch(/OptionalTone\(.*p\.isochronicTonesHz/);
    expect(editor).toMatch(/if \(on\) startHz else null/);
  });

  // @verifies SWR-UI-01#f
  it('start at 20 Hz (binaural) and 40 Hz (isochronic) when the switch is turned on', () => {
    expect(editor).toMatch(/BINAURAL_START_HZ = 20\.0/);
    expect(editor).toMatch(/ISOCHRONIC_START_HZ = 40\.0/);
  });
});
