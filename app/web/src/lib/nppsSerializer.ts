import {
  NPProtocolEntry,
  NPProtocolDefinition,
  NPCompositeProtocol,
  NPProtocolModality,
  NPCompositeLayer,
  NPIntervalConfig,
  NPModalityParams,
} from '../types/protocol';
import { NPLimitsSet } from '../types/limits';

const INDENT = '    ';

function esc(s: string): string {
  return s.replace(/\\/g, '\\\\').replace(/"/g, '\\"').replace(/\n/g, '\\n');
}
function str(s: string): string { return `"${esc(s)}"`; }
function strArr(arr: string[]): string {
  if (arr.length === 0) return '[]';
  return `[${arr.map(str).join(', ')}]`;
}
function numArr(arr: number[]): string {
  if (arr.length === 0) return '[]';
  return `[${arr.join(', ')}]`;
}
function serializeInterval(iv: NPIntervalConfig, level: number): string {
  const p = INDENT.repeat(level);
  const lines: string[] = [];
  lines.push(`${p}interval {`);
  lines.push(`${p}${INDENT}on: ${iv.intervalOnSeconds}`);
  lines.push(`${p}${INDENT}off: ${iv.intervalOffSeconds}`);
  if (iv.repeatCount !== undefined) {
    lines.push(`${p}${INDENT}repeat: ${iv.repeatCount}`);
  }
  lines.push(`${p}}`);
  return lines.join('\n');
}

function serializeModalityParams(mp: NPModalityParams, level: number): string {
  const p = INDENT.repeat(level);
  const lines: string[] = [];

  switch (mp.type) {
    case 'pbm_transcranial': {
      const params = mp.params;
      lines.push(`${p}type: "pbm_transcranial"`);
      lines.push(`${p}zones: "${params.zones}"`);
      if (params.customZones) lines.push(`${p}custom_zones: ${numArr(params.customZones)}`);
      lines.push(`${p}wavelength: "${params.wavelength}"`);
      lines.push(`${p}intensity_percent: ${params.intensityPercent}`);
      lines.push(`${p}frequency_hz: ${params.frequencyHz}`);
      lines.push(`${p}duty_cycle_percent: ${params.dutyCyclePercent}`);
      break;
    }
    case 'pbm_intranasal': {
      const params = mp.params;
      lines.push(`${p}type: "pbm_intranasal"`);
      lines.push(`${p}intensity_percent: ${params.intensityPercent}`);
      lines.push(`${p}frequency_hz: ${params.frequencyHz}`);
      lines.push(`${p}duty_cycle_percent: ${params.dutyCyclePercent}`);
      break;
    }
    case 'eeg_neurofeedback': {
      const params = mp.params;
      lines.push(`${p}type: "eeg_neurofeedback"`);
      lines.push(`${p}channels: "${params.channels}"`);
      if (params.customChannels) lines.push(`${p}custom_channels: ${strArr(params.customChannels)}`);
      lines.push(`${p}band: "${params.band}"`);
      lines.push(`${p}closed_loop_enabled: ${params.closedLoopEnabled}`);
      break;
    }
    case 'bes_tacs': {
      const params = mp.params;
      lines.push(`${p}type: "bes_tacs"`);
      lines.push(`${p}frequency_hz: ${params.frequencyHz}`);
      lines.push(`${p}intensity_milliamps: ${params.intensityMilliamps}`);
      lines.push(`${p}waveform: "${params.waveform}"`);
      break;
    }
    case 'tdcs': {
      const params = mp.params;
      lines.push(`${p}type: "tdcs"`);
      lines.push(`${p}intensity_milliamps: ${params.intensityMilliamps}`);
      const pairs = params.electrodePairs.map(([a, b]) => `[${str(a)}, ${str(b)}]`).join(', ');
      lines.push(`${p}electrode_pairs: [${pairs}]`);
      lines.push(`${p}ramp_seconds: ${params.rampSeconds}`);
      break;
    }
    case 'vns_hrv': {
      const params = mp.params;
      lines.push(`${p}type: "vns_hrv"`);
      lines.push(`${p}frequency_hz: ${params.frequencyHz}`);
      lines.push(`${p}intensity_milliamps: ${params.intensityMilliamps}`);
      lines.push(`${p}hrv_protocol: "${params.hrvProtocol}"`);
      lines.push(`${p}resonance_breathing_rate: ${params.resonanceBreathingRate}`);
      break;
    }
    case 'audio_entrainment': {
      const params = mp.params;
      lines.push(`${p}type: "audio_entrainment"`);
      if (params.binauralBeatsHz !== undefined) lines.push(`${p}binaural_beats_hz: ${params.binauralBeatsHz}`);
      if (params.isochronicTonesHz !== undefined) lines.push(`${p}isochronic_tones_hz: ${params.isochronicTonesHz}`);
      if (params.noiseType !== undefined) lines.push(`${p}noise_type: "${params.noiseType}"`);
      lines.push(`${p}carrier_hz: ${params.carrierHz}`);
      lines.push(`${p}volume_percent: ${params.volumePercent}`);
      lines.push(`${p}eeg_adaptive: ${params.eegAdaptive}`);
      lines.push(`${p}bone_conduction_pacer: ${params.boneConductionPacer}`);
      break;
    }
    case 'visual_stimulation': {
      const params = mp.params;
      lines.push(`${p}type: "visual_stimulation"`);
      lines.push(`${p}frequency_hz: ${params.frequencyHz}`);
      lines.push(`${p}mode: "${params.mode}"`);
      lines.push(`${p}emdr_cadence_hz: ${params.emdrCadenceHz}`);
      lines.push(`${p}enable_mode_f: ${params.enableModeF}`);
      break;
    }
    case 'qeeg_21ch': {
      const params = mp.params;
      lines.push(`${p}type: "qeeg_21ch"`);
      lines.push(`${p}montage: "${params.montage}"`);
      lines.push(`${p}sloreta_enabled: ${params.sloretaEnabled}`);
      lines.push(`${p}reference: "${params.reference}"`);
      break;
    }
    case 'tms': {
      const params = mp.params;
      lines.push(`${p}type: "tms"`);
      lines.push(`${p}tms_protocol: "${params.tmsProtocol}"`);
      lines.push(`${p}frequency_hz: ${params.frequencyHz}`);
      lines.push(`${p}intensity_percent_mt: ${params.intensityPercentMT}`);
      lines.push(`${p}target: "${params.target}"`);
      lines.push(`${p}pulse_count: ${params.pulseCount}`);
      break;
    }
    case 'pbm_deep_1170nm': {
      const params = mp.params;
      lines.push(`${p}type: "pbm_deep_1170nm"`);
      lines.push(`${p}intensity_mw_cm2: ${params.intensityMWcm2}`);
      lines.push(`${p}frequency_hz: ${params.frequencyHz}`);
      lines.push(`${p}duty_cycle_percent: ${params.dutyCyclePercent}`);
      break;
    }
    case 'clinical_tacs': {
      const params = mp.params;
      lines.push(`${p}type: "clinical_tacs"`);
      lines.push(`${p}frequency_hz: ${params.frequencyHz}`);
      lines.push(`${p}intensity_milliamps: ${params.intensityMilliamps}`);
      lines.push(`${p}channel_count: ${params.channelCount}`);
      lines.push(`${p}waveform: "${params.waveform}"`);
      break;
    }
    case 'hd_tdcs': {
      const params = mp.params;
      lines.push(`${p}type: "hd_tdcs"`);
      lines.push(`${p}target: "${params.target}"`);
      lines.push(`${p}montage: "${params.montage}"`);
      lines.push(`${p}intensity_milliamps: ${params.intensityMilliamps}`);
      break;
    }
    case 'cervical_vns': {
      const params = mp.params;
      lines.push(`${p}type: "cervical_vns"`);
      lines.push(`${p}frequency_hz: ${params.frequencyHz}`);
      lines.push(`${p}intensity_milliamps: ${params.intensityMilliamps}`);
      break;
    }
    case 'vibrotactile_40hz': {
      const params = mp.params;
      lines.push(`${p}type: "vibrotactile_40hz"`);
      lines.push(`${p}intensity_g: ${params.intensityG}`);
      lines.push(`${p}sync_to_audio: ${params.syncToAudio}`);
      lines.push(`${p}sync_to_visual: ${params.syncToVisual}`);
      break;
    }
    default: {
      const _exhaustive: never = mp;
      throw new Error(`Unknown modality type: ${JSON.stringify(_exhaustive)}`);
    }
  }
  return lines.join('\n');
}

function serializeModality(m: NPProtocolModality, level: number): string {
  const p = INDENT.repeat(level);
  const lines: string[] = [];
  lines.push(`${p}modality {`);
  lines.push(serializeModalityParams(m.modalityParams, level + 1));
  lines.push(`${p}${INDENT}enabled: ${m.enabled}`);
  lines.push(serializeInterval(m.interval, level + 1));
  lines.push(`${p}}`);
  return lines.join('\n');
}

function serializeSingleProtocol(proto: NPProtocolDefinition): string {
  const lines: string[] = [];
  lines.push('protocol {');
  lines.push(`${INDENT}name: ${str(proto.name)}`);
  lines.push(`${INDENT}description: ${str(proto.description)}`);
  lines.push(`${INDENT}author: ${str(proto.author)}`);
  lines.push(`${INDENT}version: ${str(proto.version)}`);
  lines.push(`${INDENT}tags: ${strArr(proto.tags)}`);

  // Timing
  lines.push(`${INDENT}timing {`);
  if (proto.timingMode.type === 'duration') {
    lines.push(`${INDENT}${INDENT}duration: ${proto.timingMode.seconds}`);
  } else {
    lines.push(`${INDENT}${INDENT}interval_count: ${proto.timingMode.count}`);
  }
  lines.push(`${INDENT}}`);

  // Modalities
  lines.push(`${INDENT}modalities [`);
  proto.modalities.forEach((m, i) => {
    lines.push(serializeModality(m, 2));
    if (i < proto.modalities.length - 1) lines.push('');
  });
  lines.push(`${INDENT}]`);

  lines.push('}');
  return lines.join('\n');
}

function serializeLayer(layer: NPCompositeLayer, level: number): string {
  const p = INDENT.repeat(level);
  const lines: string[] = [];
  lines.push(`${p}layer {`);
  lines.push(`${p}${INDENT}name: ${str(layer.protocolName)}`);
  lines.push(`${p}${INDENT}start: ${layer.startOffsetSeconds}`);
  if (layer.durationSeconds !== undefined) {
    lines.push(`${p}${INDENT}duration: ${layer.durationSeconds}`);
  }
  lines.push(`${p}${INDENT}intensity_scale: ${layer.intensityScale}`);
  lines.push(`${p}}`);
  return lines.join('\n');
}

function serializeComposite(composite: NPCompositeProtocol): string {
  const lines: string[] = [];
  lines.push('composite {');
  lines.push(`${INDENT}name: ${str(composite.name)}`);
  lines.push(`${INDENT}description: ${str(composite.description)}`);
  lines.push(`${INDENT}author: ${str(composite.author)}`);
  lines.push(`${INDENT}version: ${str(composite.version)}`);
  lines.push(`${INDENT}tags: ${strArr(composite.tags)}`);
  lines.push(`${INDENT}conflict_resolution: "${composite.conflictResolution}"`);
  lines.push(`${INDENT}layers [`);
  composite.layers.forEach((l, i) => {
    lines.push(serializeLayer(l, 2));
    if (i < composite.layers.length - 1) lines.push('');
  });
  lines.push(`${INDENT}]`);
  lines.push('}');
  return lines.join('\n');
}

export function serializeProtocol(entry: NPProtocolEntry): string {
  if (entry.kind === 'single') return serializeSingleProtocol(entry.protocol);
  return serializeComposite(entry.composite);
}

export function serializeNPPS(entries: NPProtocolEntry[]): string {
  return entries.map(serializeProtocol).join('\n\n');
}

// ─── Limits serialization ─────────────────────────────────────────────────────

export function serializeNPPSLimits(limits: NPLimitsSet): string {
  const lines: string[] = [];

  lines.push(`limits ${str(limits.name)} {`);
  lines.push(`${INDENT}level: ${limits.level}`);
  if (limits.helmetId) lines.push(`${INDENT}helmet_id: ${str(limits.helmetId)}`);
  if (limits.individualId) lines.push(`${INDENT}individual_id: ${str(limits.individualId)}`);
  if (limits.description) lines.push(`${INDENT}description: ${str(limits.description)}`);

  function writeModalityBlock(blockKey: string, obj: Record<string, unknown>): void {
    const entries = Object.entries(obj).filter(([, v]) => v !== undefined);
    if (entries.length === 0) return;
    lines.push(`${INDENT}${blockKey} {`);
    for (const [camelKey, v] of entries) {
      const snakeKey = camelKey.replace(/([A-Z])/g, '_$1').toLowerCase();
      if (Array.isArray(v)) {
        lines.push(`${INDENT}${INDENT}${snakeKey}: [${(v as string[]).map(s => str(String(s))).join(', ')}]`);
      } else if (typeof v === 'boolean') {
        lines.push(`${INDENT}${INDENT}${snakeKey}: ${v}`);
      } else {
        lines.push(`${INDENT}${INDENT}${snakeKey}: ${v}`);
      }
    }
    lines.push(`${INDENT}}`);
  }

  if (limits.pbmTranscranial) writeModalityBlock('pbm_transcranial', limits.pbmTranscranial as Record<string, unknown>);
  if (limits.pbmIntranasal) writeModalityBlock('pbm_intranasal', limits.pbmIntranasal as Record<string, unknown>);
  if (limits.eegNeurofeedback) writeModalityBlock('eeg_neurofeedback', limits.eegNeurofeedback as Record<string, unknown>);
  if (limits.besTacs) writeModalityBlock('bes_tacs', limits.besTacs as Record<string, unknown>);
  if (limits.tdcs) writeModalityBlock('tdcs', limits.tdcs as Record<string, unknown>);
  if (limits.vnsHrv) writeModalityBlock('vns_hrv', limits.vnsHrv as Record<string, unknown>);
  if (limits.audioEntrainment) writeModalityBlock('audio_entrainment', limits.audioEntrainment as Record<string, unknown>);
  if (limits.visualStimulation) writeModalityBlock('visual_stimulation', limits.visualStimulation as Record<string, unknown>);
  if (limits.tms) writeModalityBlock('tms', limits.tms as Record<string, unknown>);
  if (limits.pbmDeep1170nm) writeModalityBlock('pbm_deep_1170nm', limits.pbmDeep1170nm as Record<string, unknown>);
  if (limits.clinicalTacs) writeModalityBlock('clinical_tacs', limits.clinicalTacs as Record<string, unknown>);
  if (limits.hdTdcs) writeModalityBlock('hd_tdcs', limits.hdTdcs as Record<string, unknown>);
  if (limits.cervicalVns) writeModalityBlock('cervical_vns', limits.cervicalVns as Record<string, unknown>);
  if (limits.vibrotactile40hz) writeModalityBlock('vibrotactile_40hz', limits.vibrotactile40hz as Record<string, unknown>);

  lines.push('}');
  return lines.join('\n');
}
