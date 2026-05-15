import { NPProtocolEntry, NPProtocolDefinition, NPCompositeProtocol, NPProtocolModality } from '../types/protocol';

function makeId(slug: string): string { return `predefined_${slug}`; }
const NOW = '2026-05-15T00:00:00.000Z';

function makeModality(
  typeId: NPProtocolModality['modalityParams']['type'],
  params: NPProtocolModality['modalityParams']['params'],
  overrides: Partial<Omit<NPProtocolModality, 'id' | 'modalityParams'>> = {}
): NPProtocolModality {
  return {
    id: crypto.randomUUID(),
    modalityParams: { type: typeId, params } as NPProtocolModality['modalityParams'],
    interval: { intervalOnSeconds: 0, intervalOffSeconds: 0 },
    enabled: true,
    ...overrides,
  };
}

// ─── 1. Gamma Focus (40Hz multimodal) ─────────────────────────────────────────

const gammaFocus: NPProtocolDefinition = {
  id: makeId('gamma_focus'),
  name: 'Gamma Focus',
  description: 'Simultaneous 40Hz entrainment across audio, visual, and PBM channels for cognitive enhancement and attention. Evidence-backed GENUS-inspired multi-sensory 40Hz protocol.',
  author: 'NeuroPulse',
  version: '1.0',
  tags: ['focus', 'gamma', '40hz', 'cognitive', 'multimodal'],
  createdAt: NOW, modifiedAt: NOW, isPredefined: true,
  timingMode: { type: 'duration', seconds: 1200 },
  modalities: [
    makeModality('pbm_transcranial', {
      zones: 'all', wavelength: '660_808nm',
      intensityPercent: 80, frequencyHz: 40, dutyCyclePercent: 25,
    }),
    makeModality('audio_entrainment', {
      binauralBeatsHz: 40, carrierHz: 200, volumePercent: 70,
      eegAdaptive: false, boneConductionPacer: false,
    }),
    makeModality('visual_stimulation', {
      frequencyHz: 40, mode: 'binocular', emdrCadenceHz: 1, enableModeF: false,
    }),
    makeModality('eeg_neurofeedback', {
      channels: 'front', band: 'gamma', closedLoopEnabled: true,
    }),
  ],
};

// ─── 2. Alpha Calm ─────────────────────────────────────────────────────────────

const alphaCalm: NPProtocolDefinition = {
  id: makeId('alpha_calm'),
  name: 'Alpha Calm',
  description: 'Promotes relaxed alertness via 10Hz alpha entrainment with PBM and HRV biofeedback. Ideal for stress reduction and pre-sleep wind-down.',
  author: 'NeuroPulse',
  version: '1.0',
  tags: ['calm', 'alpha', 'relaxation', 'stress', 'hrv'],
  createdAt: NOW, modifiedAt: NOW, isPredefined: true,
  timingMode: { type: 'duration', seconds: 1200 },
  modalities: [
    makeModality('pbm_transcranial', {
      zones: 'all', wavelength: '660_808nm',
      intensityPercent: 60, frequencyHz: 10, dutyCyclePercent: 50,
    }),
    makeModality('audio_entrainment', {
      binauralBeatsHz: 10, carrierHz: 200, volumePercent: 65,
      eegAdaptive: true, boneConductionPacer: false, noiseType: 'pink',
    }),
    makeModality('vns_hrv', {
      frequencyHz: 25, intensityMilliamps: 0.4,
      hrvProtocol: 'standalone', resonanceBreathingRate: 6,
    }),
    makeModality('eeg_neurofeedback', {
      channels: 'all', band: 'alpha', closedLoopEnabled: true,
    }),
  ],
};

// ─── 3. Deep Sleep ─────────────────────────────────────────────────────────────

const deepSleep: NPProtocolDefinition = {
  id: makeId('deep_sleep'),
  name: 'Deep Sleep',
  description: 'Slow-wave sleep induction using 2Hz delta stimulation with calming PBM, dim visual entrainment, and resonance breathing. Use within 30 minutes of bedtime.',
  author: 'NeuroPulse',
  version: '1.0',
  tags: ['sleep', 'delta', '2hz', 'relaxation', 'bedtime'],
  createdAt: NOW, modifiedAt: NOW, isPredefined: true,
  timingMode: { type: 'duration', seconds: 1800 },
  modalities: [
    makeModality('pbm_transcranial', {
      zones: 'all', wavelength: '660_808nm',
      intensityPercent: 40, frequencyHz: 2, dutyCyclePercent: 50,
    }),
    makeModality('pbm_intranasal', {
      intensityPercent: 50, frequencyHz: 2, dutyCyclePercent: 50,
    }),
    makeModality('audio_entrainment', {
      binauralBeatsHz: 2, carrierHz: 100, volumePercent: 40,
      eegAdaptive: false, boneConductionPacer: true, noiseType: 'brown',
    }),
    makeModality('visual_stimulation', {
      frequencyHz: 2, mode: 'binocular', emdrCadenceHz: 0.5, enableModeF: false,
    }),
    makeModality('vns_hrv', {
      frequencyHz: 10, intensityMilliamps: 0.3,
      hrvProtocol: 'standalone', resonanceBreathingRate: 5,
    }),
  ],
};

// ─── 4. Memory Boost (Theta) ───────────────────────────────────────────────────

const memoryBoost: NPProtocolDefinition = {
  id: makeId('memory_boost'),
  name: 'Memory Boost',
  description: 'Theta (6Hz) hippocampal entrainment for memory consolidation and learning. Combines PBM, theta-band EEG neurofeedback, and tACS.',
  author: 'NeuroPulse',
  version: '1.0',
  tags: ['memory', 'theta', '6hz', 'learning', 'hippocampal'],
  createdAt: NOW, modifiedAt: NOW, isPredefined: true,
  timingMode: { type: 'duration', seconds: 1200 },
  modalities: [
    makeModality('pbm_transcranial', {
      zones: 'rear', wavelength: '660_808nm',
      intensityPercent: 70, frequencyHz: 6, dutyCyclePercent: 50,
    }),
    makeModality('eeg_neurofeedback', {
      channels: 'central', band: 'theta', closedLoopEnabled: true,
    }),
    makeModality('bes_tacs', {
      frequencyHz: 6, intensityMilliamps: 0.5, waveform: 'sinusoidal',
    }),
    makeModality('audio_entrainment', {
      binauralBeatsHz: 6, carrierHz: 200, volumePercent: 60,
      eegAdaptive: true, boneConductionPacer: false,
    }),
  ],
};

// ─── 5. Anxiety Relief ────────────────────────────────────────────────────────

const anxietyRelief: NPProtocolDefinition = {
  id: makeId('anxiety_relief'),
  name: 'Anxiety Relief',
  description: 'Multi-modal anxiety reduction combining VNS, HRV coherence training, alpha PBM, and resonance breathing. Based on 24-RCT meta-analysis (d=0.83 anxiety reduction).',
  author: 'NeuroPulse',
  version: '1.0',
  tags: ['anxiety', 'stress', 'hrv', 'vns', 'alpha', 'calm'],
  createdAt: NOW, modifiedAt: NOW, isPredefined: true,
  timingMode: { type: 'duration', seconds: 1200 },
  modalities: [
    makeModality('vns_hrv', {
      frequencyHz: 25, intensityMilliamps: 0.5,
      hrvProtocol: 'eeg_biofeedback', resonanceBreathingRate: 6,
    }),
    makeModality('pbm_transcranial', {
      zones: 'front', wavelength: '660_808nm',
      intensityPercent: 65, frequencyHz: 10, dutyCyclePercent: 50,
    }),
    makeModality('audio_entrainment', {
      binauralBeatsHz: 10, carrierHz: 200, volumePercent: 60,
      eegAdaptive: true, boneConductionPacer: true, noiseType: 'pink',
    }),
    makeModality('eeg_neurofeedback', {
      channels: 'front', band: 'alpha', closedLoopEnabled: true,
    }),
  ],
};

// ─── 6. Flow State ────────────────────────────────────────────────────────────

const flowState: NPProtocolDefinition = {
  id: makeId('flow_state'),
  name: 'Flow State',
  description: 'Gamma-theta coupled protocol targeting the flow state signature: frontal gamma + parieto-occipital theta. Split-zone PBM with closed-loop EEG adaptation.',
  author: 'NeuroPulse',
  version: '1.0',
  tags: ['flow', 'gamma', 'theta', 'performance', 'creative', 'multimodal'],
  createdAt: NOW, modifiedAt: NOW, isPredefined: true,
  timingMode: { type: 'duration', seconds: 1500 },
  modalities: [
    makeModality('pbm_transcranial', {
      zones: 'custom', customZones: [0, 1], wavelength: '660_808nm',
      intensityPercent: 80, frequencyHz: 40, dutyCyclePercent: 25,
    }),
    makeModality('pbm_transcranial', {
      zones: 'custom', customZones: [2, 3, 4], wavelength: '660_808nm',
      intensityPercent: 75, frequencyHz: 6, dutyCyclePercent: 50,
    }),
    makeModality('eeg_neurofeedback', {
      channels: 'all', band: 'gamma_theta', closedLoopEnabled: true,
    }),
    makeModality('audio_entrainment', {
      binauralBeatsHz: 40, isochronicTonesHz: 6, carrierHz: 200, volumePercent: 65,
      eegAdaptive: true, boneConductionPacer: false,
    }),
  ],
};

// ─── 7. Vascular Baseline (CW PBM) ────────────────────────────────────────────

const vascularBaseline: NPProtocolDefinition = {
  id: makeId('vascular_baseline'),
  name: 'Vascular Baseline',
  description: 'Continuous-wave PBM for cerebrovascular support. Low-intensity, long-duration daily maintenance protocol. No electrical stimulation.',
  author: 'NeuroPulse',
  version: '1.0',
  tags: ['pbm', 'vascular', 'maintenance', 'cw', 'daily', 'cerebrovascular'],
  createdAt: NOW, modifiedAt: NOW, isPredefined: true,
  timingMode: { type: 'duration', seconds: 1800 },
  modalities: [
    makeModality('pbm_transcranial', {
      zones: 'all', wavelength: '660_808nm',
      intensityPercent: 50, frequencyHz: 0, dutyCyclePercent: 100,
    }),
    makeModality('pbm_intranasal', {
      intensityPercent: 55, frequencyHz: 0, dutyCyclePercent: 100,
    }),
    makeModality('audio_entrainment', {
      noiseType: 'pink', carrierHz: 200, volumePercent: 30,
      eegAdaptive: false, boneConductionPacer: false,
    }),
  ],
};

// ─── 8. ADHD Focus ────────────────────────────────────────────────────────────

const adhdFocus: NPProtocolDefinition = {
  id: makeId('adhd_focus'),
  name: 'ADHD Focus',
  description: 'EEG neurofeedback theta/beta ratio training with tDCS cortical priming. Based on 21 RCTs (n=1,261) of EEG neurofeedback for ADHD.',
  author: 'NeuroPulse',
  version: '1.0',
  tags: ['adhd', 'focus', 'theta', 'beta', 'neurofeedback', 'tdcs'],
  createdAt: NOW, modifiedAt: NOW, isPredefined: true,
  timingMode: { type: 'duration', seconds: 2400 },
  modalities: [
    makeModality('eeg_neurofeedback', {
      channels: 'front', band: 'alpha_theta', closedLoopEnabled: true,
    }),
    makeModality('tdcs', {
      intensityMilliamps: 1.5,
      electrodePairs: [['F3', 'Fp2']],
      rampSeconds: 30,
    }),
    makeModality('pbm_transcranial', {
      zones: 'front', wavelength: '660_808nm',
      intensityPercent: 70, frequencyHz: 20, dutyCyclePercent: 25,
    }),
    makeModality('audio_entrainment', {
      isochronicTonesHz: 20, carrierHz: 200, volumePercent: 55,
      eegAdaptive: true, boneConductionPacer: false,
    }),
  ],
};

// ─── 9. Retinal Health (Mode F) ───────────────────────────────────────────────

const retinalHealth: NPProtocolDefinition = {
  id: makeId('retinal_health'),
  name: 'Retinal Health',
  description: 'Invisible 808–830nm near-infrared retinal PBM during normal-looking wear. Supports mitochondrial function in retinal photoreceptors.',
  author: 'NeuroPulse',
  version: '1.0',
  tags: ['retinal', 'pbm', 'nir', 'mode_f', 'ocular', 'maintenance'],
  createdAt: NOW, modifiedAt: NOW, isPredefined: true,
  timingMode: { type: 'duration', seconds: 180 },
  modalities: [
    makeModality('visual_stimulation', {
      frequencyHz: 0, mode: 'mode_f', emdrCadenceHz: 1, enableModeF: true,
    }),
  ],
};

// ─── 10. PTSD EMDR ────────────────────────────────────────────────────────────

const ptsdEmdr: NPProtocolDefinition = {
  id: makeId('ptsd_emdr'),
  name: 'PTSD EMDR Support',
  description: 'EMDR bilateral visual stimulation with VNS vagal support and calming audio. Intended for use alongside therapeutic EMDR sessions. WHO-recommended protocol basis.',
  author: 'NeuroPulse',
  version: '1.0',
  tags: ['ptsd', 'emdr', 'bilateral', 'trauma', 'vns', 'clinical'],
  createdAt: NOW, modifiedAt: NOW, isPredefined: true,
  timingMode: { type: 'duration', seconds: 3600 },
  modalities: [
    makeModality('visual_stimulation', {
      frequencyHz: 1, mode: 'emdr', emdrCadenceHz: 1, enableModeF: false,
    }),
    makeModality('vns_hrv', {
      frequencyHz: 25, intensityMilliamps: 0.4,
      hrvProtocol: 'standalone', resonanceBreathingRate: 6,
    }),
    makeModality('audio_entrainment', {
      noiseType: 'brown', carrierHz: 200, volumePercent: 45,
      eegAdaptive: false, boneConductionPacer: true,
    }),
    makeModality('eeg_neurofeedback', {
      channels: 'all', band: 'alpha', closedLoopEnabled: true,
    }),
  ],
};

// ─── 11. HRV Coherence Training ───────────────────────────────────────────────

const hrvCoherence: NPProtocolDefinition = {
  id: makeId('hrv_coherence'),
  name: 'HRV Coherence Training',
  description: 'Resonance frequency breathing with synchronised taVNS. Maximises heart rate variability and coherence score. Evidence: meta-analysis 24 RCTs (d=0.83 anxiety, d=0.65 depression).',
  author: 'NeuroPulse',
  version: '1.0',
  tags: ['hrv', 'coherence', 'breathing', 'vns', 'anxiety', 'depression'],
  createdAt: NOW, modifiedAt: NOW, isPredefined: true,
  timingMode: { type: 'duration', seconds: 1200 },
  modalities: [
    makeModality('vns_hrv', {
      frequencyHz: 25, intensityMilliamps: 0.5,
      hrvProtocol: 'tavns_sync', resonanceBreathingRate: 6,
    }),
    makeModality('audio_entrainment', {
      noiseType: 'pink', carrierHz: 200, volumePercent: 50,
      eegAdaptive: false, boneConductionPacer: true,
    }),
  ],
};

// ─── Composite 1: Full Multi-Modal RCT Replication ────────────────────────────

const fullRctComposite: NPCompositeProtocol = {
  id: makeId('composite_full_rct'),
  name: 'Full Multi-Modal RCT',
  description: 'Replicates the 2025 nationally-conducted RCT protocol combining PBM + qEEG neurofeedback + HRV biofeedback simultaneously. Sequence: baseline EEG capture → combined active phase → cool-down HRV.',
  author: 'NeuroPulse',
  version: '1.0',
  tags: ['rct', 'multimodal', 'pbm', 'eeg', 'hrv', 'research', 'depression'],
  createdAt: NOW, modifiedAt: NOW, isPredefined: true,
  conflictResolution: 'merge',
  layers: [
    {
      id: crypto.randomUUID(),
      protocolName: 'Vascular Baseline',
      startOffsetSeconds: 0,
      durationSeconds: 300,
      intensityScale: 0.5,
    },
    {
      id: crypto.randomUUID(),
      protocolName: 'Gamma Focus',
      startOffsetSeconds: 300,
      durationSeconds: 1200,
      intensityScale: 1.0,
    },
    {
      id: crypto.randomUUID(),
      protocolName: 'HRV Coherence Training',
      startOffsetSeconds: 300,
      durationSeconds: 1200,
      intensityScale: 1.0,
    },
    {
      id: crypto.randomUUID(),
      protocolName: 'Alpha Calm',
      startOffsetSeconds: 1500,
      durationSeconds: 600,
      intensityScale: 0.6,
    },
  ],
};

// ─── Composite 2: Sleep Optimisation Stack ────────────────────────────────────

const sleepOptimisationComposite: NPCompositeProtocol = {
  id: makeId('composite_sleep_stack'),
  name: 'Sleep Optimisation Stack',
  description: 'Evening stack: alpha wind-down → deep sleep induction → delta maintenance. Runs for 90 minutes starting 2 hours before target sleep time.',
  author: 'NeuroPulse',
  version: '1.0',
  tags: ['sleep', 'evening', 'wind-down', 'delta', 'alpha', 'stack'],
  createdAt: NOW, modifiedAt: NOW, isPredefined: true,
  conflictResolution: 'sequential',
  layers: [
    {
      id: crypto.randomUUID(),
      protocolName: 'Alpha Calm',
      startOffsetSeconds: 0,
      durationSeconds: 1200,
      intensityScale: 0.8,
    },
    {
      id: crypto.randomUUID(),
      protocolName: 'HRV Coherence Training',
      startOffsetSeconds: 600,
      durationSeconds: 600,
      intensityScale: 0.7,
    },
    {
      id: crypto.randomUUID(),
      protocolName: 'Deep Sleep',
      startOffsetSeconds: 1200,
      durationSeconds: 1800,
      intensityScale: 1.0,
    },
  ],
};

// ─── Export ────────────────────────────────────────────────────────────────────

export const PREDEFINED_PROTOCOLS: NPProtocolEntry[] = [
  { kind: 'single', protocol: gammaFocus },
  { kind: 'single', protocol: alphaCalm },
  { kind: 'single', protocol: deepSleep },
  { kind: 'single', protocol: memoryBoost },
  { kind: 'single', protocol: anxietyRelief },
  { kind: 'single', protocol: flowState },
  { kind: 'single', protocol: vascularBaseline },
  { kind: 'single', protocol: adhdFocus },
  { kind: 'single', protocol: retinalHealth },
  { kind: 'single', protocol: ptsdEmdr },
  { kind: 'single', protocol: hrvCoherence },
  { kind: 'composite', composite: fullRctComposite },
  { kind: 'composite', composite: sleepOptimisationComposite },
];
