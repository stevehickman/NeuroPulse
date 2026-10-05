#!/usr/bin/env bun
/**
 * gen-hub-descriptor-golden.ts — the web compiler's own descriptors, as the Android
 * compiler's golden vectors (OI-AND-WIRE-01).
 *
 *   bun scripts/gen-hub-descriptor-golden.ts   # rewrites app/NeurOneShared/TestData/hub-descriptor-golden.json
 *
 * The session UUID and compile time are pinned (0xAB x16, 1700000000) so the bytes are
 * reproducible. Run it when hubCompiler.ts changes, then update HubDescriptorCompilerTests.
 * Needs `bun scripts/sync-locales.ts` first (hubCompiler.ts reaches i18n).
 */
import fs from 'node:fs';
// Loaded by computed specifier: the web sources reach generated locales (git-ignored), which
// the scripts/ type-check (tsconfig.scripts.json) has not built, so it must not follow them.
const WEB = new URL('../app/web/src/lib/', import.meta.url).pathname;
const { compileProtocol } = (await import(WEB + 'hubCompiler.ts')) as any;
const { parseNPPSFile } = (await import(new URL('../common/lib/', import.meta.url).pathname + 'nppsParser.ts')) as any;
const zones = new Map(parseNPPSFile(fs.readFileSync(new URL('../protocols/predefined/00-zones.npps', import.meta.url).pathname,'utf8')).zones.map((z:any)=>[z.name,z]));
Date.now = () => 1_700_000_000_000;
(globalThis.crypto as any).getRandomValues = (a: Uint8Array) => { a.fill(0xAB); return a; };
const C = { intervalOnSeconds: 0, intervalOffSeconds: 0 };
const mod = (type: string, params: any, interval: any = C) => ({ enabled: true, modalityParams: { type, params }, interval });
const def = (secs: number, ...mods: any[]) => ({ name: 'g', timingMode: { type: 'duration', seconds: secs }, modalities: mods }) as any;
const pbm = (wl: string, irr: number, extra: any = {}) => mod('pbm_transcranial', { zones: 'named', zoneRefs: ['Frontal'], wavelength: wl, irradianceMWcm2: irr, frequencyHz: 40, dutyCyclePercent: 25, ...extra });
const cases: Record<string, any> = {
  pbm660: def(1200, pbm('660nm', 200)),
  pbm808: def(1200, pbm('808nm', 322)),
  pbm1064: def(1200, pbm('1064nm', 28)),
  pbmMerged: def(600, pbm('660nm', 100), pbm('808nm', 200)),
  pbmClin: def(600, mod('pbm_transcranial', { zones: 'clinician_selected', wavelength: '808nm', irradianceMWcm2: 100, frequencyHz: 0, dutyCyclePercent: 100 })),
  nasal: def(600, mod('pbm_intranasal', { wavelength: '808nm', irradianceMWcm2: 60, frequencyHz: 40, dutyCyclePercent: 25 })),
  eegAll: def(600, mod('eeg_neurofeedback', { channels: 'all', band: 'alpha', closedLoopEnabled: true })),
  eegCustom: def(600, mod('eeg_neurofeedback', { channels: 'custom', customChannels: ['Fp1','P4'], band: 'alpha', closedLoopEnabled: false })),
  bes: def(600, mod('bes_tacs', { frequencyHz: 10, intensityMilliamps: 0.8, waveform: 'sinusoidal' })),
  tdcs: def(600, mod('tdcs', { intensityMilliamps: 1, electrodePairs: [['P3','P4']], rampSeconds: 30, electrodeAreaCm2: 25.5 })),
  vns: def(600, mod('vns_hrv', { frequencyHz: 25, intensityMilliamps: 1.5, hrvProtocol: 'tavns_sync', resonanceBreathingRate: 6 })),
  audioBin: def(600, mod('audio_entrainment', { binauralBeatsHz: 20, carrierHz: 440, volumeDb: 75, eegAdaptive: true, boneConductionPacer: false })),
  audioPink: def(600, mod('audio_entrainment', { noiseType: 'pink', carrierHz: 440, volumeDb: 60, eegAdaptive: false, boneConductionPacer: true })),
  visual: def(600, mod('visual_stimulation', { frequencyHz: 40, mode: 'binocular', emdrCadenceHz: 1.5, enableModeF: false })),
  qeeg: def(600, mod('qeeg_21ch', { montage: 'standard_1020', sloretaEnabled: true, reference: 'average' })),
  tms: def(600, mod('tms', { tmsProtocol: 'iTBS', frequencyHz: 10, intensityPercentMT: 80, target: 'M1_R', pulseCount: 600 })),
  deep: def(600, mod('pbm_deep_1170nm', { intensityMWcm2: 500, frequencyHz: 40, dutyCyclePercent: 25 })),
  ctacs: def(600, mod('clinical_tacs', { frequencyHz: 40, intensityMilliamps: 2, channelCount: 21, waveform: 'square' })),
  hdtdcs: def(600, mod('hd_tdcs', { target: 'DLPFC_R', montage: 'bilateral_4x1', intensityMilliamps: 1.5 })),
  cvns: def(600, mod('cervical_vns', { frequencyHz: 25, intensityMilliamps: 1.5 })),
  vibro: def(600, mod('vibrotactile_40hz', { frequencyHz: 40, intensityG: 0.9, syncToAudio: true, syncToVisual: false })),
  interval: def(100, mod('bes_tacs', { frequencyHz: 10, intensityMilliamps: 0.8, waveform: 'square' }, { intervalOnSeconds: 20, intervalOffSeconds: 10 }),
                     mod('tdcs', { intensityMilliamps: 1, electrodePairs: [['F3','F4']], rampSeconds: 45, electrodeAreaCm2: 10 }, { intervalOnSeconds: 0, intervalOffSeconds: 0, startOffsetSeconds: 30 })),
};
const out: Record<string,string> = {};
for (const [k, d] of Object.entries(cases)) {
  const r = compileProtocol(d, { zones, clinicianSockets: [7, 3] });
  out[k] = Buffer.from(r.blob).toString('hex');
}
fs.writeFileSync(process.argv[2] ?? new URL('../app/NeurOneShared/TestData/hub-descriptor-golden.json', import.meta.url).pathname, JSON.stringify(out, null, 1));
console.log(Object.keys(out).length);
