import {
  NPProtocolEntry,
  NPProtocolDefinition,
  NPCompositeProtocol,
  NPProtocolModality,
  NPCompositeLayer,
  NPIntervalConfig,
  NPModalityParams,
  NPProtocolReference,
  NPZoneDefinition,
  NPConditionDefinition,
  referenceUrl,
  referenceLabel,
} from '../types/protocol';
import { describeInvalid, socketRangeLabel, toSocketSet } from './socketSet';
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
function refArr(refs: NPProtocolReference[]): string {
  if (refs.length === 0) return '[]';
  return `[${refs.map(r =>
    typeof r === 'string' ? str(r) : `[${str(referenceLabel(r))}, ${str(referenceUrl(r))}]`
  ).join(', ')}]`;
}

function formatTime(seconds: number): string {
  if (seconds === 0) return '0s';
  if (seconds % 3600 === 0) return `${seconds / 3600}h`;
  if (seconds % 60 === 0) return `${seconds / 60}m`;
  return `${seconds}s`;
}

function formatHz(hz: number): string {
  if (hz === 0) return '0Hz';
  if (hz === Math.floor(hz)) return `${Math.floor(hz)}Hz`;
  return `${hz}Hz`;
}

function isContinuous(iv: NPIntervalConfig): boolean {
  return iv.intervalOnSeconds === 0 && iv.intervalOffSeconds === 0;
}

function serializeModalityFields(mp: NPModalityParams): string[] {
  switch (mp.type) {
    case 'pbm_transcranial': {
      const p = mp.params;
      const lines: string[] = [
        `intensity: ${p.intensityPercent}%`,
        `frequency: ${formatHz(p.frequencyHz)}`,
        `duty_cycle: ${p.dutyCyclePercent}%`,
      ];
      if (p.zones === 'named' && p.zoneRefs) {
        lines.push(`zones: ${strArr(p.zoneRefs)}`);
      } else {
        lines.push(`zones: ${p.zones}`);
      }
      lines.push(`wavelength: ${str(p.wavelength)}`);
      return lines;
    }
    case 'pbm_intranasal': {
      const p = mp.params;
      return [
        `intensity: ${p.intensityPercent}%`,
        `frequency: ${formatHz(p.frequencyHz)}`,
        `duty_cycle: ${p.dutyCyclePercent}%`,
      ];
    }
    case 'eeg_neurofeedback': {
      const p = mp.params;
      const lines: string[] = [`band: ${p.band}`];
      if (p.channels === 'custom' && p.customChannels) {
        lines.push(`custom_channels: ${strArr(p.customChannels)}`);
      } else {
        lines.push(`channels: ${p.channels}`);
      }
      lines.push(`closed_loop: ${p.closedLoopEnabled}`);
      return lines;
    }
    case 'bes_tacs': {
      const p = mp.params;
      return [
        `frequency: ${formatHz(p.frequencyHz)}`,
        `intensity: ${p.intensityMilliamps}mA`,
        `waveform: ${p.waveform}`,
      ];
    }
    case 'tdcs': {
      const p = mp.params;
      const pairs = p.electrodePairs.map(([a, b]) => `[${str(a)}, ${str(b)}]`).join(', ');
      return [
        `intensity: ${p.intensityMilliamps}mA`,
        `electrode_pairs: [${pairs}]`,
        `ramp: ${p.rampSeconds}s`,
      ];
    }
    case 'vns_hrv': {
      const p = mp.params;
      return [
        `frequency: ${formatHz(p.frequencyHz)}`,
        `intensity: ${p.intensityMilliamps}mA`,
        `hrv_protocol: ${p.hrvProtocol}`,
        `breathing_rate: ${p.resonanceBreathingRate}`,
      ];
    }
    case 'audio_entrainment': {
      const p = mp.params;
      const lines: string[] = [];
      if (p.binauralBeatsHz !== undefined) lines.push(`binaural_hz: ${formatHz(p.binauralBeatsHz)}`);
      if (p.isochronicTonesHz !== undefined) lines.push(`isochronic_hz: ${formatHz(p.isochronicTonesHz)}`);
      lines.push(p.noiseType !== undefined ? `noise: ${p.noiseType}` : `noise: none`);
      lines.push(`carrier_hz: ${formatHz(p.carrierHz)}`);
      lines.push(`volume: ${p.volumePercent}%`);
      lines.push(`eeg_adaptive: ${p.eegAdaptive}`);
      lines.push(`bone_conduction_pacer: ${p.boneConductionPacer}`);
      return lines;
    }
    case 'visual_stimulation': {
      const p = mp.params;
      return [
        `frequency: ${formatHz(p.frequencyHz)}`,
        `mode: ${p.mode}`,
        `emdr_cadence: ${formatHz(p.emdrCadenceHz)}`,
        `enable_mode_f: ${p.enableModeF}`,
      ];
    }
    case 'qeeg_21ch': {
      const p = mp.params;
      return [
        `montage: ${p.montage}`,
        `sloreta_enabled: ${p.sloretaEnabled}`,
        `reference: ${p.reference}`,
      ];
    }
    case 'tms': {
      const p = mp.params;
      return [
        `tms_protocol: ${p.tmsProtocol}`,
        `frequency: ${formatHz(p.frequencyHz)}`,
        `intensity_percent_mt: ${p.intensityPercentMT}`,
        `target: ${p.target}`,
        `pulse_count: ${p.pulseCount}`,
      ];
    }
    case 'pbm_deep_1170nm': {
      const p = mp.params;
      return [
        `intensity_mw_cm2: ${p.intensityMWcm2}`,
        `frequency: ${formatHz(p.frequencyHz)}`,
        `duty_cycle: ${p.dutyCyclePercent}%`,
      ];
    }
    case 'clinical_tacs': {
      const p = mp.params;
      return [
        `frequency: ${formatHz(p.frequencyHz)}`,
        `intensity: ${p.intensityMilliamps}mA`,
        `channel_count: ${p.channelCount}`,
        `waveform: ${p.waveform}`,
      ];
    }
    case 'hd_tdcs': {
      const p = mp.params;
      return [
        `target: ${p.target}`,
        `montage: ${p.montage}`,
        `intensity: ${p.intensityMilliamps}mA`,
      ];
    }
    case 'cervical_vns': {
      const p = mp.params;
      return [
        `frequency: ${formatHz(p.frequencyHz)}`,
        `intensity: ${p.intensityMilliamps}mA`,
      ];
    }
    case 'vibrotactile_40hz': {
      const p = mp.params;
      return [
        `intensity_g: ${p.intensityG}`,
        `sync_to_audio: ${p.syncToAudio}`,
        `sync_to_visual: ${p.syncToVisual}`,
      ];
    }
    default: {
      const _exhaustive: never = mp;
      throw new Error(`Unknown modality type: ${JSON.stringify(_exhaustive)}`);
    }
  }
}

function serializeModality(m: NPProtocolModality, level: number): string {
  const p = INDENT.repeat(level);
  const lines: string[] = [];
  lines.push(`${p}${m.modalityParams.type} {`);
  for (const field of serializeModalityFields(m.modalityParams)) {
    lines.push(`${p}${INDENT}${field}`);
  }
  if (!isContinuous(m.interval)) {
    lines.push(`${p}${INDENT}interval_on: ${formatTime(m.interval.intervalOnSeconds)}`);
    lines.push(`${p}${INDENT}interval_off: ${formatTime(m.interval.intervalOffSeconds)}`);
    if (m.interval.repeatCount !== undefined) {
      lines.push(`${p}${INDENT}repeat: ${m.interval.repeatCount}`);
    } else {
      lines.push(`${p}${INDENT}repeat: until_end`);
    }
  }
  if (!m.enabled) {
    lines.push(`${p}${INDENT}enabled: false`);
  }
  lines.push(`${p}}`);
  return lines.join('\n');
}

function serializeSingleProtocol(proto: NPProtocolDefinition): string {
  const lines: string[] = [];
  lines.push(`protocol ${str(proto.name)} {`);
  lines.push(`${INDENT}id: ${str(proto.id)}`);
  lines.push(`${INDENT}description: ${str(proto.description)}`);
  lines.push(`${INDENT}author: ${str(proto.author)}`);
  lines.push(`${INDENT}version: ${str(proto.version)}`);
  if (proto.isReadOnly) lines.push(`${INDENT}readonly: true`);
  lines.push(`${INDENT}tags: ${strArr(proto.tags)}`);
  if (proto.conditions && proto.conditions.length > 0) {
    lines.push(`${INDENT}conditions: ${strArr(proto.conditions)}`);
  }
  if (proto.references && proto.references.length > 0) {
    lines.push(`${INDENT}references: ${refArr(proto.references)}`);
  }
  if (proto.timingMode.type === 'duration') {
    lines.push(`${INDENT}duration: ${formatTime(proto.timingMode.seconds)}`);
  } else {
    lines.push(`${INDENT}interval_count: ${proto.timingMode.count}`);
  }
  proto.modalities.forEach(m => {
    lines.push('');
    lines.push(serializeModality(m, 1));
  });
  lines.push('}');
  return lines.join('\n');
}

function serializeLayer(layer: NPCompositeLayer, level: number): string {
  const p = INDENT.repeat(level);
  const lines: string[] = [];
  lines.push(`${p}layer ${str(layer.protocolName)} {`);
  lines.push(`${p}${INDENT}start: ${formatTime(layer.startOffsetSeconds)}`);
  if (layer.durationSeconds !== undefined) {
    lines.push(`${p}${INDENT}duration: ${formatTime(layer.durationSeconds)}`);
  }
  if (Math.abs(layer.intensityScale - 1.0) > 0.001) {
    lines.push(`${p}${INDENT}intensity_scale: ${layer.intensityScale}`);
  }
  lines.push(`${p}}`);
  return lines.join('\n');
}

function serializeComposite(composite: NPCompositeProtocol): string {
  const lines: string[] = [];
  lines.push(`composite ${str(composite.name)} {`);
  lines.push(`${INDENT}id: ${str(composite.id)}`);
  lines.push(`${INDENT}description: ${str(composite.description)}`);
  lines.push(`${INDENT}author: ${str(composite.author)}`);
  lines.push(`${INDENT}version: ${str(composite.version)}`);
  if (composite.isReadOnly) lines.push(`${INDENT}readonly: true`);
  lines.push(`${INDENT}tags: ${strArr(composite.tags)}`);
  if (composite.conditions && composite.conditions.length > 0) {
    lines.push(`${INDENT}conditions: ${strArr(composite.conditions)}`);
  }
  if (composite.references && composite.references.length > 0) {
    lines.push(`${INDENT}references: ${refArr(composite.references)}`);
  }
  lines.push(`${INDENT}conflict_resolution: ${composite.conflictResolution}`);
  composite.layers.forEach(l => {
    lines.push('');
    lines.push(serializeLayer(l, 1));
  });
  lines.push('}');
  return lines.join('\n');
}

export function serializeProtocol(entry: NPProtocolEntry): string {
  if (entry.kind === 'single') return serializeSingleProtocol(entry.protocol);
  return serializeComposite(entry.composite);
}

// ─── Zone / condition serialization ────────────────────────────────────────────

/**
 * Serialize a zone, canonicalising its socket list on the way out.
 *
 * The write path has to enforce the same contract as the read path, or the app
 * can emit a `.npps` file its own parser rejects — an in-memory zone carrying
 * `[0, 4, 4, 999]` used to serialize verbatim and then fail to load. Throwing
 * here is deliberate: a zone that cannot be written is a bug in whatever built
 * it, and silently dropping the bad ids would hide that.
 */
export function serializeZone(zone: NPZoneDefinition): string {
  const { sockets, invalid } = toSocketSet(zone.sockets);
  if (invalid.length > 0) {
    throw new Error(
      `cannot serialize zone "${zone.name}": ` +
      `${invalid.map(describeInvalid).join(', ')} ` +
      `${invalid.length === 1 ? 'is not a socket' : 'are not sockets'} on this ` +
      `helmet — ids are whole numbers ${socketRangeLabel()}`,
    );
  }

  const lines: string[] = [];
  lines.push(`zone ${str(zone.name)} {`);
  if (zone.id) lines.push(`${INDENT}id: ${str(zone.id)}`);
  if (zone.description) lines.push(`${INDENT}description: ${str(zone.description)}`);
  lines.push(`${INDENT}sockets: ${numArr(sockets)}`);
  if (zone.types) lines.push(`${INDENT}types: ${strArr(zone.types)}`);
  if (zone.excludeTypes) lines.push(`${INDENT}exclude_types: true`);
  lines.push('}');
  return lines.join('\n');
}

export function serializeCondition(cond: NPConditionDefinition): string {
  const lines: string[] = [];
  lines.push(`condition ${str(cond.name)} {`);
  if (cond.id) lines.push(`${INDENT}id: ${str(cond.id)}`);
  lines.push(`${INDENT}link: ${str(cond.link)}`);
  if (cond.code) lines.push(`${INDENT}code: ${str(cond.code)}`);
  if (cond.description) lines.push(`${INDENT}description: ${str(cond.description)}`);
  lines.push('}');
  return lines.join('\n');
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
