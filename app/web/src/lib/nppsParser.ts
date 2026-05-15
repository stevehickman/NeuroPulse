import {
  NPProtocolEntry,
  NPProtocolDefinition,
  NPCompositeProtocol,
  NPProtocolModality,
  NPCompositeLayer,
  NPTimingMode,
  NPIntervalConfig,
  NPModalityParams,
  NPModalityTypeId,
  defaultParams,
  PBMTranscranialParams,
  PBMIntranasalParams,
  EEGNeurofeedbackParams,
  BESTacsParams,
  TDCSParams,
  VNSHRVParams,
  AudioEntrainmentParams,
  VisualStimParams,
  QEEG21chParams,
  TMSParams,
  DeepPBM1170Params,
  ClinicalTacsParams,
  HDTdcsParams,
  CervicalVnsParams,
  VibrotactileParams,
} from '../types/protocol';

// ─── Error ─────────────────────────────────────────────────────────────────────

export class NPPSParseError extends Error {
  line?: number;
  constructor(message: string, line?: number) {
    super(line != null ? `Line ${line}: ${message}` : message);
    this.name = 'NPPSParseError';
    this.line = line;
  }
}

// ─── Tokens ────────────────────────────────────────────────────────────────────

type TokenType =
  | 'KEYWORD'
  | 'IDENT'
  | 'STRING'
  | 'NUMBER'
  | 'BOOL'
  | 'LBRACE'
  | 'RBRACE'
  | 'LBRACKET'
  | 'RBRACKET'
  | 'COLON'
  | 'COMMA'
  | 'NEWLINE'
  | 'EOF';

interface Token {
  type: TokenType;
  value: string | number | boolean;
  line: number;
}

const KEYWORDS = new Set([
  'protocol', 'composite', 'name', 'description', 'author', 'version',
  'tags', 'timing', 'duration', 'interval_count', 'modalities', 'modality',
  'type', 'enabled', 'interval', 'on', 'off', 'repeat', 'layers', 'layer',
  'start', 'end', 'intensity_scale', 'conflict_resolution',
  'true', 'false',
]);

export function tokenize(text: string): Token[] {
  const tokens: Token[] = [];
  let pos = 0;
  let line = 1;

  while (pos < text.length) {
    // Skip whitespace (not newlines)
    if (text[pos] === ' ' || text[pos] === '\t' || text[pos] === '\r') {
      pos++;
      continue;
    }

    // Comments
    if (text[pos] === '#') {
      while (pos < text.length && text[pos] !== '\n') pos++;
      continue;
    }

    // Newlines
    if (text[pos] === '\n') {
      tokens.push({ type: 'NEWLINE', value: '\n', line });
      line++;
      pos++;
      continue;
    }

    // Braces and brackets
    if (text[pos] === '{') { tokens.push({ type: 'LBRACE', value: '{', line }); pos++; continue; }
    if (text[pos] === '}') { tokens.push({ type: 'RBRACE', value: '}', line }); pos++; continue; }
    if (text[pos] === '[') { tokens.push({ type: 'LBRACKET', value: '[', line }); pos++; continue; }
    if (text[pos] === ']') { tokens.push({ type: 'RBRACKET', value: ']', line }); pos++; continue; }
    if (text[pos] === ':') { tokens.push({ type: 'COLON', value: ':', line }); pos++; continue; }
    if (text[pos] === ',') { tokens.push({ type: 'COMMA', value: ',', line }); pos++; continue; }

    // Strings
    if (text[pos] === '"') {
      pos++;
      let str = '';
      while (pos < text.length && text[pos] !== '"') {
        if (text[pos] === '\\' && pos + 1 < text.length) {
          const next = text[pos + 1];
          if (next === '"') str += '"';
          else if (next === '\\') str += '\\';
          else if (next === 'n') str += '\n';
          else if (next === 't') str += '\t';
          else str += next;
          pos += 2;
        } else {
          if (text[pos] === '\n') line++;
          str += text[pos++];
        }
      }
      if (pos >= text.length) throw new NPPSParseError('Unterminated string', line);
      pos++; // closing quote
      tokens.push({ type: 'STRING', value: str, line });
      continue;
    }

    // Numbers (including negative)
    if (text[pos] === '-' || (text[pos] >= '0' && text[pos] <= '9')) {
      let numStr = '';
      if (text[pos] === '-') { numStr = '-'; pos++; }
      while (pos < text.length && ((text[pos] >= '0' && text[pos] <= '9') || text[pos] === '.')) {
        numStr += text[pos++];
      }
      if (numStr === '-') {
        throw new NPPSParseError('Stray minus sign', line);
      }
      tokens.push({ type: 'NUMBER', value: parseFloat(numStr), line });
      continue;
    }

    // Identifiers and keywords
    if ((text[pos] >= 'a' && text[pos] <= 'z') || (text[pos] >= 'A' && text[pos] <= 'Z') || text[pos] === '_') {
      let ident = '';
      while (pos < text.length && ((text[pos] >= 'a' && text[pos] <= 'z') || (text[pos] >= 'A' && text[pos] <= 'Z') || (text[pos] >= '0' && text[pos] <= '9') || text[pos] === '_')) {
        ident += text[pos++];
      }
      if (ident === 'true') {
        tokens.push({ type: 'BOOL', value: true, line });
      } else if (ident === 'false') {
        tokens.push({ type: 'BOOL', value: false, line });
      } else if (KEYWORDS.has(ident)) {
        tokens.push({ type: 'KEYWORD', value: ident, line });
      } else {
        tokens.push({ type: 'IDENT', value: ident, line });
      }
      continue;
    }

    throw new NPPSParseError(`Unexpected character: ${text[pos]}`, line);
  }

  tokens.push({ type: 'EOF', value: '', line });
  return tokens;
}

// ─── Parser ────────────────────────────────────────────────────────────────────

class Parser {
  private tokens: Token[];
  private pos: number = 0;

  constructor(tokens: Token[]) {
    this.tokens = tokens;
  }

  private get current(): Token {
    return this.tokens[this.pos];
  }

  // Returns token type without TypeScript control-flow narrowing
  private ct(): TokenType {
    return this.tokens[this.pos].type as TokenType;
  }

  private advance(): Token {
    const t = this.current;
    if (t.type !== 'EOF') this.pos++;
    return t;
  }

  private skipNewlines(): void {
    while (this.current.type === 'NEWLINE') this.advance();
  }

  private expect(type: TokenType, value?: string): Token {
    this.skipNewlines();
    const t = this.current;
    if (t.type !== type) {
      throw new NPPSParseError(`Expected ${value ?? type}, got ${t.type} (${String(t.value)})`, t.line);
    }
    if (value != null && t.value !== value) {
      throw new NPPSParseError(`Expected '${value}', got '${String(t.value)}'`, t.line);
    }
    return this.advance();
  }

  private expectIdent(name: string): void {
    this.skipNewlines();
    const t = this.current;
    if ((t.type !== 'KEYWORD' && t.type !== 'IDENT') || t.value !== name) {
      throw new NPPSParseError(`Expected '${name}', got '${String(t.value)}'`, t.line);
    }
    this.advance();
  }

  private tryKeyword(name: string): boolean {
    this.skipNewlines();
    const t = this.current;
    if ((t.type === 'KEYWORD' || t.type === 'IDENT') && t.value === name) {
      this.advance();
      return true;
    }
    return false;
  }

  private readString(): string {
    this.skipNewlines();
    const t = this.current;
    if (t.type === 'STRING') { this.advance(); return t.value as string; }
    if (t.type === 'IDENT' || t.type === 'KEYWORD') { this.advance(); return t.value as string; }
    throw new NPPSParseError(`Expected string, got ${t.type}`, t.line);
  }

  private readNumber(): number {
    this.skipNewlines();
    const t = this.current;
    if (t.type !== 'NUMBER') throw new NPPSParseError(`Expected number, got ${t.type} (${String(t.value)})`, t.line);
    this.advance();
    return t.value as number;
  }

  private readBool(): boolean {
    this.skipNewlines();
    const t = this.current;
    if (t.type === 'BOOL') { this.advance(); return t.value as boolean; }
    if (t.type === 'IDENT' || t.type === 'KEYWORD') {
      if (t.value === 'true') { this.advance(); return true; }
      if (t.value === 'false') { this.advance(); return false; }
    }
    throw new NPPSParseError(`Expected bool, got ${t.type} (${String(t.value)})`, t.line);
  }

  private readStringArray(): string[] {
    this.skipNewlines();
    this.expect('LBRACKET');
    const arr: string[] = [];
    this.skipNewlines();
    while (this.current.type !== 'RBRACKET' && this.current.type !== 'EOF') {
      arr.push(this.readString());
      this.skipNewlines();
      if (this.current.type === 'COMMA') { this.advance(); this.skipNewlines(); }
    }
    this.expect('RBRACKET');
    return arr;
  }

  private readKeyValue(): { key: string; valueLine: number } {
    this.skipNewlines();
    const t = this.current;
    if (t.type !== 'KEYWORD' && t.type !== 'IDENT') {
      throw new NPPSParseError(`Expected key, got ${t.type} (${String(t.value)})`, t.line);
    }
    const key = t.value as string;
    const valueLine = t.line;
    this.advance();
    this.skipNewlines();
    this.expect('COLON');
    return { key, valueLine };
  }

  parse(): NPProtocolEntry[] {
    const entries: NPProtocolEntry[] = [];
    this.skipNewlines();
    while (this.current.type !== 'EOF') {
      this.skipNewlines();
      if (this.ct() === 'EOF') break;
      if (this.tryKeyword('protocol')) {
        entries.push({ kind: 'single', protocol: this.parseProtocol() });
      } else if (this.tryKeyword('composite')) {
        entries.push({ kind: 'composite', composite: this.parseComposite() });
      } else {
        throw new NPPSParseError(
          `Expected 'protocol' or 'composite', got '${String(this.current.value)}'`,
          this.current.line
        );
      }
      this.skipNewlines();
    }
    return entries;
  }

  private parseProtocol(): NPProtocolDefinition {
    const startLine = this.current.line;
    this.skipNewlines();
    this.expect('LBRACE');

    let name = '';
    let description = '';
    let author = 'NeuroPulse';
    let version = '1.0';
    let tags: string[] = [];
    let timingMode: NPTimingMode = { type: 'duration', seconds: 1200 };
    let modalities: NPProtocolModality[] = [];
    const id = crypto.randomUUID();
    const now = new Date().toISOString();

    this.skipNewlines();
    while (!this.tryBrace()) {
      const { key } = this.readKeyValue();
      switch (key) {
        case 'name': name = this.readString(); break;
        case 'description': description = this.readString(); break;
        case 'author': author = this.readString(); break;
        case 'version': version = this.readString(); break;
        case 'tags': tags = this.readStringArray(); break;
        case 'timing': timingMode = this.parseTiming(); break;
        case 'modalities': modalities = this.parseModalitiesBlock(); break;
        default:
          throw new NPPSParseError(`Unknown protocol key: '${key}'`, startLine);
      }
      this.skipNewlines();
    }

    return {
      id, name, description, author, version, tags,
      createdAt: now, modifiedAt: now,
      isPredefined: false,
      timingMode,
      modalities,
    };
  }

  private tryBrace(): boolean {
    this.skipNewlines();
    if (this.current.type === 'RBRACE') { this.advance(); return true; }
    return false;
  }

  private parseTiming(): NPTimingMode {
    this.skipNewlines();
    this.expect('LBRACE');
    this.skipNewlines();

    let mode: NPTimingMode = { type: 'duration', seconds: 1200 };

    while (!this.tryBrace()) {
      const { key } = this.readKeyValue();
      if (key === 'duration') {
        mode = { type: 'duration', seconds: this.readNumber() };
      } else if (key === 'interval_count') {
        mode = { type: 'interval_count', count: this.readNumber() };
      } else {
        throw new NPPSParseError(`Unknown timing key: '${key}'`, this.current.line);
      }
      this.skipNewlines();
    }
    return mode;
  }

  private parseModalitiesBlock(): NPProtocolModality[] {
    this.skipNewlines();
    this.expect('LBRACKET');
    this.skipNewlines();
    const result: NPProtocolModality[] = [];
    while (this.ct() !== 'RBRACKET' && this.ct() !== 'EOF') {
      this.skipNewlines();
      if (this.ct() === 'RBRACKET') break;
      this.expectIdent('modality');
      result.push(this.parseModalityBlock());
      this.skipNewlines();
      if (this.ct() === 'COMMA') { this.advance(); this.skipNewlines(); }
    }
    this.expect('RBRACKET');
    return result;
  }

  private parseModalityBlock(): NPProtocolModality {
    this.skipNewlines();
    this.expect('LBRACE');
    this.skipNewlines();

    let typeId: NPModalityTypeId | null = null;
    let enabled = true;
    let interval: NPIntervalConfig = { intervalOnSeconds: 0, intervalOffSeconds: 0 };
    let rawParams: Record<string, unknown> = {};
    const id = crypto.randomUUID();

    while (!this.tryBrace()) {
      const { key } = this.readKeyValue();
      if (key === 'type') {
        const val = this.readString() as NPModalityTypeId;
        typeId = val;
      } else if (key === 'enabled') {
        enabled = this.readBool();
      } else if (key === 'interval') {
        interval = this.parseIntervalBlock();
      } else {
        // Collect as raw param
        rawParams[key] = this.readAnyValue();
      }
      this.skipNewlines();
    }

    if (!typeId) throw new NPPSParseError('Modality block missing type', this.current.line);

    const params = this.buildModalityParams(typeId, rawParams);
    return { id, modalityParams: params, interval, enabled };
  }

  private parseIntervalBlock(): NPIntervalConfig {
    this.skipNewlines();
    this.expect('LBRACE');
    this.skipNewlines();

    let intervalOnSeconds = 0;
    let intervalOffSeconds = 0;
    let repeatCount: number | undefined;

    while (!this.tryBrace()) {
      const { key } = this.readKeyValue();
      if (key === 'on') intervalOnSeconds = this.readNumber();
      else if (key === 'off') intervalOffSeconds = this.readNumber();
      else if (key === 'repeat') repeatCount = this.readNumber();
      else throw new NPPSParseError(`Unknown interval key: '${key}'`, this.current.line);
      this.skipNewlines();
    }

    return { intervalOnSeconds, intervalOffSeconds, repeatCount };
  }

  private readAnyValue(): unknown {
    this.skipNewlines();
    const t = this.current;
    if (t.type === 'STRING') { this.advance(); return t.value; }
    if (t.type === 'NUMBER') { this.advance(); return t.value; }
    if (t.type === 'BOOL') { this.advance(); return t.value; }
    if (t.type === 'IDENT' || t.type === 'KEYWORD') {
      if (t.value === 'true') { this.advance(); return true; }
      if (t.value === 'false') { this.advance(); return false; }
      this.advance(); return t.value;
    }
    if (t.type === 'LBRACKET') return this.readGenericArray();
    if (t.type === 'LBRACE') return this.readGenericObject();
    throw new NPPSParseError(`Unexpected token ${t.type} in value position`, t.line);
  }

  private readGenericArray(): unknown[] {
    this.expect('LBRACKET');
    const arr: unknown[] = [];
    this.skipNewlines();
    while (this.current.type !== 'RBRACKET' && this.current.type !== 'EOF') {
      arr.push(this.readAnyValue());
      this.skipNewlines();
      if (this.current.type === 'COMMA') { this.advance(); this.skipNewlines(); }
    }
    this.expect('RBRACKET');
    return arr;
  }

  private readGenericObject(): Record<string, unknown> {
    this.expect('LBRACE');
    this.skipNewlines();
    const obj: Record<string, unknown> = {};
    while (this.current.type !== 'RBRACE' && this.current.type !== 'EOF') {
      const { key } = this.readKeyValue();
      obj[key] = this.readAnyValue();
      this.skipNewlines();
      if (this.current.type === 'COMMA') { this.advance(); this.skipNewlines(); }
    }
    this.expect('RBRACE');
    return obj;
  }

  private buildModalityParams(typeId: NPModalityTypeId, raw: Record<string, unknown>): NPModalityParams {
    const def = defaultParams(typeId);

    function str(key: string, fallback: string): string {
      return typeof raw[key] === 'string' ? (raw[key] as string) : fallback;
    }
    function num(key: string, fallback: number): number {
      return typeof raw[key] === 'number' ? (raw[key] as number) : fallback;
    }
    function bool(key: string, fallback: boolean): boolean {
      return typeof raw[key] === 'boolean' ? (raw[key] as boolean) : fallback;
    }
    function optNum(key: string): number | undefined {
      return typeof raw[key] === 'number' ? (raw[key] as number) : undefined;
    }
    function optStr(key: string): string | undefined {
      return typeof raw[key] === 'string' ? (raw[key] as string) : undefined;
    }

    switch (typeId) {
      case 'pbm_transcranial': {
        const d = def as PBMTranscranialParams;
        const params: PBMTranscranialParams = {
          zones: str('zones', d.zones) as PBMTranscranialParams['zones'],
          wavelength: str('wavelength', d.wavelength) as PBMTranscranialParams['wavelength'],
          intensityPercent: num('intensity_percent', d.intensityPercent),
          frequencyHz: num('frequency_hz', d.frequencyHz),
          dutyCyclePercent: num('duty_cycle_percent', d.dutyCyclePercent),
        };
        const customZones = raw['custom_zones'];
        if (Array.isArray(customZones)) params.customZones = customZones as number[];
        return { type: 'pbm_transcranial', params };
      }
      case 'pbm_intranasal': {
        const d = def as PBMIntranasalParams;
        return {
          type: 'pbm_intranasal',
          params: {
            intensityPercent: num('intensity_percent', d.intensityPercent),
            frequencyHz: num('frequency_hz', d.frequencyHz),
            dutyCyclePercent: num('duty_cycle_percent', d.dutyCyclePercent),
          },
        };
      }
      case 'eeg_neurofeedback': {
        const d = def as EEGNeurofeedbackParams;
        const params: EEGNeurofeedbackParams = {
          channels: str('channels', d.channels) as EEGNeurofeedbackParams['channels'],
          band: str('band', d.band) as EEGNeurofeedbackParams['band'],
          closedLoopEnabled: bool('closed_loop_enabled', d.closedLoopEnabled),
        };
        const customCh = raw['custom_channels'];
        if (Array.isArray(customCh)) params.customChannels = customCh as string[];
        return { type: 'eeg_neurofeedback', params };
      }
      case 'bes_tacs': {
        const d = def as BESTacsParams;
        return {
          type: 'bes_tacs',
          params: {
            frequencyHz: num('frequency_hz', d.frequencyHz),
            intensityMilliamps: num('intensity_milliamps', d.intensityMilliamps),
            waveform: str('waveform', d.waveform) as BESTacsParams['waveform'],
          },
        };
      }
      case 'tdcs': {
        const d = def as TDCSParams;
        const rawPairs = raw['electrode_pairs'];
        const electrodePairs: [string, string][] = Array.isArray(rawPairs)
          ? (rawPairs as unknown[]).map(p => {
              if (Array.isArray(p) && p.length >= 2) return [String(p[0]), String(p[1])] as [string, string];
              return ['Fp1', 'Fp2'] as [string, string];
            })
          : d.electrodePairs;
        return {
          type: 'tdcs',
          params: {
            intensityMilliamps: num('intensity_milliamps', d.intensityMilliamps),
            electrodePairs,
            rampSeconds: num('ramp_seconds', d.rampSeconds),
          },
        };
      }
      case 'vns_hrv': {
        const d = def as VNSHRVParams;
        return {
          type: 'vns_hrv',
          params: {
            frequencyHz: num('frequency_hz', d.frequencyHz),
            intensityMilliamps: num('intensity_milliamps', d.intensityMilliamps),
            hrvProtocol: str('hrv_protocol', d.hrvProtocol) as VNSHRVParams['hrvProtocol'],
            resonanceBreathingRate: num('resonance_breathing_rate', d.resonanceBreathingRate),
          },
        };
      }
      case 'audio_entrainment': {
        const d = def as AudioEntrainmentParams;
        const params: AudioEntrainmentParams = {
          carrierHz: num('carrier_hz', d.carrierHz),
          volumePercent: num('volume_percent', d.volumePercent),
          eegAdaptive: bool('eeg_adaptive', d.eegAdaptive),
          boneConductionPacer: bool('bone_conduction_pacer', d.boneConductionPacer),
        };
        const bb = optNum('binaural_beats_hz');
        if (bb !== undefined) params.binauralBeatsHz = bb;
        const iso = optNum('isochronic_tones_hz');
        if (iso !== undefined) params.isochronicTonesHz = iso;
        const nt = optStr('noise_type');
        if (nt) params.noiseType = nt as AudioEntrainmentParams['noiseType'];
        return { type: 'audio_entrainment', params };
      }
      case 'visual_stimulation': {
        const d = def as VisualStimParams;
        return {
          type: 'visual_stimulation',
          params: {
            frequencyHz: num('frequency_hz', d.frequencyHz),
            mode: str('mode', d.mode) as VisualStimParams['mode'],
            emdrCadenceHz: num('emdr_cadence_hz', d.emdrCadenceHz),
            enableModeF: bool('enable_mode_f', d.enableModeF),
          },
        };
      }
      case 'qeeg_21ch': {
        const d = def as QEEG21chParams;
        return {
          type: 'qeeg_21ch',
          params: {
            montage: str('montage', d.montage) as QEEG21chParams['montage'],
            sloretaEnabled: bool('sloreta_enabled', d.sloretaEnabled),
            reference: str('reference', d.reference) as QEEG21chParams['reference'],
          },
        };
      }
      case 'tms': {
        const d = def as TMSParams;
        return {
          type: 'tms',
          params: {
            tmsProtocol: str('tms_protocol', d.tmsProtocol) as TMSParams['tmsProtocol'],
            frequencyHz: num('frequency_hz', d.frequencyHz),
            intensityPercentMT: num('intensity_percent_mt', d.intensityPercentMT),
            target: str('target', d.target) as TMSParams['target'],
            pulseCount: num('pulse_count', d.pulseCount),
          },
        };
      }
      case 'pbm_deep_1170nm': {
        const d = def as DeepPBM1170Params;
        return {
          type: 'pbm_deep_1170nm',
          params: {
            intensityMWcm2: num('intensity_mw_cm2', d.intensityMWcm2),
            frequencyHz: num('frequency_hz', d.frequencyHz),
            dutyCyclePercent: num('duty_cycle_percent', d.dutyCyclePercent),
          },
        };
      }
      case 'clinical_tacs': {
        const d = def as ClinicalTacsParams;
        return {
          type: 'clinical_tacs',
          params: {
            frequencyHz: num('frequency_hz', d.frequencyHz),
            intensityMilliamps: num('intensity_milliamps', d.intensityMilliamps),
            channelCount: num('channel_count', d.channelCount),
            waveform: str('waveform', d.waveform) as ClinicalTacsParams['waveform'],
          },
        };
      }
      case 'hd_tdcs': {
        const d = def as HDTdcsParams;
        return {
          type: 'hd_tdcs',
          params: {
            target: str('target', d.target) as HDTdcsParams['target'],
            montage: str('montage', d.montage) as HDTdcsParams['montage'],
            intensityMilliamps: num('intensity_milliamps', d.intensityMilliamps),
          },
        };
      }
      case 'cervical_vns': {
        const d = def as CervicalVnsParams;
        return {
          type: 'cervical_vns',
          params: {
            frequencyHz: num('frequency_hz', d.frequencyHz),
            intensityMilliamps: num('intensity_milliamps', d.intensityMilliamps),
          },
        };
      }
      case 'vibrotactile_40hz': {
        const d = def as VibrotactileParams;
        return {
          type: 'vibrotactile_40hz',
          params: {
            intensityG: num('intensity_g', d.intensityG),
            syncToAudio: bool('sync_to_audio', d.syncToAudio),
            syncToVisual: bool('sync_to_visual', d.syncToVisual),
          },
        };
      }
      default: {
        const _exhaustive: never = typeId;
        throw new NPPSParseError(`Unknown modality type: ${String(_exhaustive)}`);
      }
    }
  }

  private parseComposite(): NPCompositeProtocol {
    this.skipNewlines();
    this.expect('LBRACE');
    this.skipNewlines();

    let name = '';
    let description = '';
    let author = 'NeuroPulse';
    let version = '1.0';
    let tags: string[] = [];
    let layers: NPCompositeLayer[] = [];
    let conflictResolution: NPCompositeProtocol['conflictResolution'] = 'merge';
    const id = crypto.randomUUID();
    const now = new Date().toISOString();

    while (!this.tryBrace()) {
      const { key } = this.readKeyValue();
      switch (key) {
        case 'name': name = this.readString(); break;
        case 'description': description = this.readString(); break;
        case 'author': author = this.readString(); break;
        case 'version': version = this.readString(); break;
        case 'tags': tags = this.readStringArray(); break;
        case 'conflict_resolution': conflictResolution = this.readString() as NPCompositeProtocol['conflictResolution']; break;
        case 'layers': layers = this.parseLayersBlock(); break;
        default:
          throw new NPPSParseError(`Unknown composite key: '${key}'`, this.current.line);
      }
      this.skipNewlines();
    }

    return { id, name, description, author, version, tags, createdAt: now, modifiedAt: now, isPredefined: false, layers, conflictResolution };
  }

  private parseLayersBlock(): NPCompositeLayer[] {
    this.skipNewlines();
    this.expect('LBRACKET');
    this.skipNewlines();
    const result: NPCompositeLayer[] = [];
    while (this.ct() !== 'RBRACKET' && this.ct() !== 'EOF') {
      this.skipNewlines();
      if (this.ct() === 'RBRACKET') break;
      this.expectIdent('layer');
      result.push(this.parseLayerBlock());
      this.skipNewlines();
      if (this.ct() === 'COMMA') { this.advance(); this.skipNewlines(); }
    }
    this.expect('RBRACKET');
    return result;
  }

  private parseLayerBlock(): NPCompositeLayer {
    this.skipNewlines();
    this.expect('LBRACE');
    this.skipNewlines();

    let protocolName = '';
    let startOffsetSeconds = 0;
    let durationSeconds: number | undefined;
    let intensityScale = 1.0;
    const id = crypto.randomUUID();

    while (!this.tryBrace()) {
      const { key } = this.readKeyValue();
      switch (key) {
        case 'name': protocolName = this.readString(); break;
        case 'start': startOffsetSeconds = this.readNumber(); break;
        case 'end': durationSeconds = this.readNumber() - startOffsetSeconds; break;
        case 'duration': durationSeconds = this.readNumber(); break;
        case 'intensity_scale': intensityScale = this.readNumber(); break;
        default:
          throw new NPPSParseError(`Unknown layer key: '${key}'`, this.current.line);
      }
      this.skipNewlines();
    }

    return { id, protocolName, startOffsetSeconds, durationSeconds, intensityScale };
  }
}

// ─── Public API ────────────────────────────────────────────────────────────────

export function parseNPPS(text: string): NPProtocolEntry[] {
  const tokens = tokenize(text);
  const parser = new Parser(tokens);
  return parser.parse();
}
