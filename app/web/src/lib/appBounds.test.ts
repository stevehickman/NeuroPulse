// The safety-relevant ranges the Compose and iOS apps carry must agree with the ones the web app takes from
// paramBounds.ts. Those apps state their ranges in their own source, so this reads it. A test of text is a blunt
// check, but it is the only one that runs without a Swift or Kotlin toolchain, and it fails on the exact mistakes
// the sweep of 2026-11-15 found: a volume range above the 85 dB SPL ceiling, and the removed 1 to 25 Hz VNS slider.
import { describe, it, expect } from 'vitest';
import { readFileSync } from 'node:fs';
import { join } from 'node:path';
import { NPHardwareLimits as hw } from '../../../../common/lib/constants.generated';

const repo = join(import.meta.dirname, '..', '..', '..', '..');
const read = (p: string) => readFileSync(join(repo, p), 'utf8');

const IOS_EDITOR = 'app/ios/NeurOne/Views/Protocol/ModalityEditorView.swift';
const IOS_LIMITS = 'app/ios/NeurOne/Views/Protocol/LimitsSettingsView.swift';
const COMPOSE_LIMITS = 'app/NeurOneUI/shared/src/commonMain/kotlin/life/neurone/shared/ui/LimitsTierEditor.kt';

/** The text of one Swift struct, from its declaration to the next struct. */
function swiftStruct(text: string, name: string): string {
  const start = text.indexOf(`struct ${name}`);
  expect(start, `${name} exists`).toBeGreaterThan(-1);
  const next = text.indexOf('\nstruct ', start + 1);
  return text.slice(start, next === -1 ? undefined : next);
}

describe('the audio level ceiling in the Compose and iOS apps', () => {
  // @verifies SWR-UI-02#a
  it('is one definition on iOS and it equals the shared ceiling', () => {
    const m = read(IOS_EDITOR).match(/enum NPAudioLimits \{\s*static let maxLevelDbSpl: Double = ([0-9.]+)\s*\}/);
    expect(m, 'NPAudioLimits.maxLevelDbSpl is defined').not.toBeNull();
    expect(Number(m![1])).toBe(hw.AUDIO_MAX_DB_SPL);
  });

  // @verifies SWR-UI-02#a
  it('ends the iOS volume slider and the iOS volume limit at that ceiling', () => {
    const editor = read(IOS_EDITOR);
    const volume = editor.slice(editor.indexOf('VALIDATE_PARAM_VOLUME'));
    expect(volume.match(/range: ([^,\n]+)/)![1]).toBe('0...NPAudioLimits.maxLevelDbSpl');
    const limits = read(IOS_LIMITS);
    const limit = limits.slice(limits.indexOf('WEB_LIM_MAX_VOLUME'));
    expect(limit.match(/range: ([^)\n]+)\)/)![1]).toBe('0...NPAudioLimits.maxLevelDbSpl');
  });

  // @verifies SWR-UI-02#a
  it('ends the Compose volume limit at the generated ceiling', () => {
    const line = read(COMPOSE_LIMITS).split('\n').find(l => l.includes('web_lim_max_volume'))!;
    expect(line).toContain('0.0..NPHardwareLimits.AUDIO_MAX_DB_SPL');
  });
});

describe('the auricular VNS frequency in the iOS editor', () => {
  // @verifies SWR-UI-02#b
  it('is a typed field with no range, not the removed 1 to 25 Hz slider', () => {
    const vns = swiftStruct(read(IOS_EDITOR), 'VNSHRVParamsView');
    expect(vns).not.toMatch(/range:\s*1\.\.\.25/);
    expect(vns).toMatch(/NumberFieldRow\(\s*label: String\(localized: "AND_MODALITY_FREQUENCY_HZ"\),\s*unit: "Hz",\s*value: \$params\.frequencyHz/);
    // The cervical VNS slider keeps its range: 1 to 25 Hz is that device's predicate range.
    expect(swiftStruct(read(IOS_EDITOR), 'CervicalVnsParamsView')).toMatch(/range:\s*1\.\.\.25/);
  });
});
