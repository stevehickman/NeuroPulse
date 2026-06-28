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
import {
  NPLimitsSet,
  LimitLevel,
  PBMTranscranialLimits,
  PBMIntranasalLimits,
  EEGNeurofeedbackLimits,
  BESTacsLimits,
  TDCSLimits,
  VNSHRVLimits,
  AudioEntrainmentLimits,
  VisualStimLimits,
  TMSLimits,
  DeepPBMLimits,
  ClinicalTacsLimits,
  HDTdcsLimits,
  CervicalVnsLimits,
  VibrotactileLimits,
} from '../types/limits';

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
  unit?: string; // 's' | 'm' | 'Hz' | '%' | 'mA' — preserved from source for duration/unit-aware fields
}

const KEYWORDS = new Set([
  'protocol', 'composite', 'limits', 'description', 'author', 'version',
  'tags', 'duration', 'interval_count',
  'enabled', 'repeat', 'layer',
  'start', 'end', 'intensity_scale', 'conflict_resolution',
  'level', 'global', 'helmet', 'individual', 'helmet_id', 'individual_id',
  'pbm_transcranial', 'pbm_intranasal', 'eeg_neurofeedback', 'bes_tacs',
  'tdcs', 'vns_hrv', 'audio_entrainment', 'visual_stimulation', 'tms',
  'pbm_deep_1170nm', 'clinical_tacs', 'hd_tdcs', 'cervical_vns', 'vibrotactile_40hz',
  'max_intensity', 'max_frequency', 'min_frequency', 'max_duty_cycle',
  'max_session_dose', 'max_daily_dose', 'max_session_duration',
  'max_sessions_per_day', 'max_sessions_per_week',
  'max_intensity_pct_mt', 'max_pulses_per_session', 'max_pulses_per_day',
  'allowed_modes', 'allowed_protocols', 'allowed_targets', 'allowed_montages',
  'allowed_bands', 'require_closed_loop', 'block_high_risk_range',
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

    // Numbers (including negative) — optionally followed by unit suffix (Hz, %, mA, s, m)
    if (text[pos] === '-' || (text[pos] >= '0' && text[pos] <= '9')) {
      let numStr = '';
      if (text[pos] === '-') { numStr = '-'; pos++; }
      while (pos < text.length && ((text[pos] >= '0' && text[pos] <= '9') || text[pos] === '.')) {
        numStr += text[pos++];
      }
      if (numStr === '-') {
        throw new NPPSParseError('Stray minus sign', line);
      }
      // Consume optional unit suffix: Hz, %, mA, s, m
      // Store unit in the token so duration parsers can distinguish minutes vs seconds.
      let unit: string | undefined;
      if (pos < text.length) {
        if (text[pos] === '%') { unit = '%'; pos++; }
        else if (text[pos] === 's' && (pos + 1 >= text.length || !/[a-zA-Z_0-9]/.test(text[pos + 1]))) { unit = 's'; pos++; }
        else if (text[pos] === 'm' && (pos + 1 >= text.length || !/[a-zA-Z_0-9]/.test(text[pos + 1]))) { unit = 'm'; pos++; }
        else if (text.slice(pos, pos + 2) === 'Hz') { unit = 'Hz'; pos += 2; }
        else if (text.slice(pos, pos + 2) === 'mA') { unit = 'mA'; pos += 2; }
      }
      // If no unit suffix was consumed and the next char would start an identifier
      // (e.g. 660_808nm, 1064nm), extend into a full IDENT token rather than NUMBER.
      // Entry condition excludes bare '_' to avoid accepting malformed tokens like
      // "660_" or "660__nm" as valid identifiers.
      if (!unit && pos < text.length &&
          ((text[pos] >= 'a' && text[pos] <= 'z') || (text[pos] >= 'A' && text[pos] <= 'Z') || text[pos] === '_')) {
        let ident = numStr;
        while (pos < text.length &&
               ((text[pos] >= 'a' && text[pos] <= 'z') || (text[pos] >= 'A' && text[pos] <= 'Z') ||
                (text[pos] >= '0' && text[pos] <= '9') || text[pos] === '_')) {
          ident += text[pos++];
        }
        // Strip any trailing underscores — they are not valid in wavelength identifiers.
        while (ident.endsWith('_')) ident = ident.slice(0, -1);
        tokens.push({ type: 'IDENT', value: ident, line });
        continue;
      }
      const tok: Token = { type: 'NUMBER', value: parseFloat(numStr), line };
      if (unit) tok.unit = unit;
      tokens.push(tok);
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

  expectIdent(name: string): void {
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

  // Tags array: accepts both ["tag1", "tag2"] and [tag1, tag2] (unquoted idents)
  private readTagArray(): string[] {
    this.skipNewlines();
    this.expect('LBRACKET');
    const arr: string[] = [];
    this.skipNewlines();
    while (this.current.type !== 'RBRACKET' && this.current.type !== 'EOF') {
      // Accept string, ident, or keyword as tag value
      const t = this.current;
      if (t.type === 'STRING' || t.type === 'IDENT' || t.type === 'KEYWORD') {
        arr.push(t.value as string);
        this.advance();
      } else {
        throw new NPPSParseError(`Expected tag value, got ${t.type}`, t.line);
      }
      this.skipNewlines();
      if (this.current.type === 'COMMA') { this.advance(); this.skipNewlines(); }
    }
    this.expect('RBRACKET');
    return arr;
  }

  // Duration in seconds: "20m" → 1200, "300s" → 300, 1200 → 1200.
  // The lexer now stores the unit suffix in token.unit.
  private readDurationSeconds(): number {
    this.skipNewlines();
    const t = this.current;
    if (t.type !== 'NUMBER') throw new NPPSParseError(`Expected duration, got ${t.type}`, t.line);
    this.advance();
    const val = t.value as number;
    if (t.unit === 'm') return val * 60;
    return val; // 's' or bare number = seconds
  }

  // Skip over an unknown field value of any type
  private skipValue(): void {
    this.skipNewlines();
    const t = this.current;
    if (t.type === 'LBRACKET') { this.readGenericArray(); return; }
    if (t.type === 'LBRACE') { this.readGenericObject(); return; }
    if (t.type === 'STRING' || t.type === 'NUMBER' || t.type === 'BOOL' ||
        t.type === 'IDENT' || t.type === 'KEYWORD') {
      this.advance();
    }
  }

  // Map typed modality block names to NPModalityTypeId
  private blockNameToTypeId(name: string): NPModalityTypeId | null {
    const map: Record<string, NPModalityTypeId> = {
      pbm_transcranial: 'pbm_transcranial',
      pbm_intranasal: 'pbm_intranasal',
      eeg_neurofeedback: 'eeg_neurofeedback',
      bes_tacs: 'bes_tacs',
      tdcs: 'tdcs',
      vns_hrv: 'vns_hrv',
      audio_entrainment: 'audio_entrainment',
      visual_stimulation: 'visual_stimulation',
      qeeg_21ch: 'qeeg_21ch',
      tms: 'tms',
      pbm_deep_1170nm: 'pbm_deep_1170nm',
      clinical_tacs: 'clinical_tacs',
      hd_tdcs: 'hd_tdcs',
      cervical_vns: 'cervical_vns',
      vibrotactile_40hz: 'vibrotactile_40hz',
    };
    return map[name] ?? null;
  }

  // Map new short field names to canonical names expected by buildModalityParams.
  // 'intensity' is context-dependent and handled separately in parseTypedModalityBlock.
  private resolveFieldAlias(key: string): string {
    const aliases: Record<string, string> = {
      frequency: 'frequency_hz',
      duty_cycle: 'duty_cycle_percent',
      closed_loop: 'closed_loop_enabled',
      binaural_hz: 'binaural_beats_hz',
      isochronic_hz: 'isochronic_tones_hz',
      noise: 'noise_type',
      volume: 'volume_percent',
      breathing_rate: 'resonance_breathing_rate',
      ramp: 'ramp_seconds',
      emdr_cadence: 'emdr_cadence_hz',
    };
    return aliases[key] ?? key;
  }

  // Parse a typed modality block: pbm_transcranial { ... }
  // typeName already consumed; next token must be '{'.
  private parseTypedModalityBlock(typeName: string): NPProtocolModality {
    this.skipNewlines();
    this.expect('LBRACE');
    this.skipNewlines();

    const id = crypto.randomUUID();
    let enabled = true;
    let intervalOnSeconds = 0;
    let intervalOffSeconds = 0;
    let repeatCount: number | undefined;
    const raw: Record<string, unknown> = {};

    while (!this.tryBrace()) {
      this.skipNewlines();
      if (this.ct() === 'RBRACE') break;

      const keyTok = this.current;
      if (keyTok.type !== 'KEYWORD' && keyTok.type !== 'IDENT') {
        throw new NPPSParseError(`Expected field name, got ${keyTok.type} (${String(keyTok.value)})`, keyTok.line);
      }
      const key = keyTok.value as string;
      this.advance();
      this.skipNewlines();
      this.expect('COLON');

      if (key === 'interval_on') { intervalOnSeconds = this.readDurationSeconds(); this.skipNewlines(); continue; }
      if (key === 'interval_off') { intervalOffSeconds = this.readDurationSeconds(); this.skipNewlines(); continue; }
      if (key === 'repeat') {
        const t = this.current;
        if ((t.type === 'IDENT' || t.type === 'KEYWORD') && t.value === 'until_end') {
          this.advance(); // undefined repeatCount = run until session end
        } else {
          repeatCount = this.readNumber();
        }
        this.skipNewlines();
        continue;
      }
      if (key === 'enabled') { enabled = this.readBool(); this.skipNewlines(); continue; }

      // 'intensity' is ambiguous across modality types — defer to post-processing
      if (key === 'intensity') {
        raw['__intensity'] = this.readAnyValue();
        this.skipNewlines();
        continue;
      }

      const canonical = this.resolveFieldAlias(key);
      const val = this.readAnyValue();

      // 'noise: none' → omit noise_type (no noise)
      if (canonical === 'noise_type' && String(val) === 'none') {
        this.skipNewlines();
        continue;
      }

      raw[canonical] = val;
      this.skipNewlines();
    }

    // Resolve 'intensity' to the correct canonical name for this modality type
    if ('__intensity' in raw) {
      const intensityVal = raw['__intensity'];
      delete raw['__intensity'];
      const percentTypes = new Set([
        'pbm_transcranial', 'pbm_intranasal', 'visual_stimulation',
      ]);
      const mATypes = new Set([
        'bes_tacs', 'tdcs', 'vns_hrv', 'clinical_tacs', 'hd_tdcs', 'cervical_vns', 'tms',
      ]);
      if (percentTypes.has(typeName)) {
        raw['intensity_percent'] = intensityVal;
      } else if (mATypes.has(typeName)) {
        raw['intensity_milliamps'] = intensityVal;
      } else if (typeName === 'vibrotactile_40hz') {
        raw['intensity_g'] = intensityVal;
      } else {
        raw['intensity_percent'] = intensityVal; // safe fallback
      }
    }

    const typeId = this.blockNameToTypeId(typeName);
    if (!typeId) {
      throw new NPPSParseError(`Unknown modality block type: '${typeName}'`, this.current.line);
    }

    const params = this.buildModalityParams(typeId, raw);
    const interval: NPIntervalConfig = { intervalOnSeconds, intervalOffSeconds };
    if (repeatCount !== undefined) interval.repeatCount = repeatCount;

    return { id, modalityParams: params, interval, enabled };
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
      } else if (this.tryKeyword('limits')) {
        // Parse limits block but skip it in the protocol entry list
        // (limits are extracted via parseNPPSLimits standalone export)
        this.parseLimitsBlock();
      } else {
        throw new NPPSParseError(
          `Expected 'protocol', 'composite', or 'limits', got '${String(this.current.value)}'`,
          this.current.line
        );
      }
      this.skipNewlines();
    }
    return entries;
  }

  parseLimitsBlockPublic(): NPLimitsSet {
    return this.parseLimitsBlock();
  }

  private parseLimitsBlock(): NPLimitsSet {
    const startLine = this.current.line;

    // Name (optional — may be omitted)
    this.skipNewlines();
    let name = 'Untitled Limits';
    if (this.current.type === 'STRING') {
      name = this.current.value as string;
      this.advance();
    } else if (this.current.type === 'IDENT') {
      name = this.current.value as string;
      this.advance();
    }

    this.skipNewlines();
    this.expect('LBRACE');
    this.skipNewlines();

    const now = new Date().toISOString();
    let level: LimitLevel = 'global';
    let helmetId: string | undefined;
    let individualId: string | undefined;
    let description = '';

    let pbmTranscranial: PBMTranscranialLimits | undefined;
    let pbmIntranasal: PBMIntranasalLimits | undefined;
    let eegNeurofeedback: EEGNeurofeedbackLimits | undefined;
    let besTacs: BESTacsLimits | undefined;
    let tdcs: TDCSLimits | undefined;
    let vnsHrv: VNSHRVLimits | undefined;
    let audioEntrainment: AudioEntrainmentLimits | undefined;
    let visualStimulation: VisualStimLimits | undefined;
    let tms: TMSLimits | undefined;
    let pbmDeep1170nm: DeepPBMLimits | undefined;
    let clinicalTacs: ClinicalTacsLimits | undefined;
    let hdTdcs: HDTdcsLimits | undefined;
    let cervicalVns: CervicalVnsLimits | undefined;
    let vibrotactile40hz: VibrotactileLimits | undefined;

    while (!this.tryBrace()) {
      const { key } = this.readKeyValue();
      switch (key) {
        case 'level': {
          const v = this.readString();
          if (v === 'global' || v === 'helmet' || v === 'individual') level = v;
          break;
        }
        case 'helmet_id': helmetId = this.readString(); break;
        case 'individual_id': individualId = this.readString(); break;
        case 'description': description = this.readString(); break;
        case 'pbm_transcranial': pbmTranscranial = this.parsePBMTranscranialLimits(); break;
        case 'pbm_intranasal': pbmIntranasal = this.parsePBMIntranasalLimits(); break;
        case 'eeg_neurofeedback': eegNeurofeedback = this.parseEEGNeurofeedbackLimits(); break;
        case 'bes_tacs': besTacs = this.parseBESTacsLimits(); break;
        case 'tdcs': tdcs = this.parseTDCSLimits(); break;
        case 'vns_hrv': vnsHrv = this.parseVNSHRVLimits(); break;
        case 'audio_entrainment': audioEntrainment = this.parseAudioEntrainmentLimits(); break;
        case 'visual_stimulation': visualStimulation = this.parseVisualStimLimits(); break;
        case 'tms': tms = this.parseTMSLimits(); break;
        case 'pbm_deep_1170nm': pbmDeep1170nm = this.parseDeepPBMLimits(); break;
        case 'clinical_tacs': clinicalTacs = this.parseClinicalTacsLimits(); break;
        case 'hd_tdcs': hdTdcs = this.parseHDTdcsLimits(); break;
        case 'cervical_vns': cervicalVns = this.parseCervicalVnsLimits(); break;
        case 'vibrotactile_40hz': vibrotactile40hz = this.parseVibrotactileLimits(); break;
        default:
          throw new NPPSParseError(`Unknown limits key: '${key}'`, startLine);
      }
      this.skipNewlines();
    }

    const result: NPLimitsSet = {
      id: crypto.randomUUID(),
      name,
      description,
      createdAt: now,
      modifiedAt: now,
      level,
    };
    if (helmetId) result.helmetId = helmetId;
    if (individualId) result.individualId = individualId;
    if (pbmTranscranial) result.pbmTranscranial = pbmTranscranial;
    if (pbmIntranasal) result.pbmIntranasal = pbmIntranasal;
    if (eegNeurofeedback) result.eegNeurofeedback = eegNeurofeedback;
    if (besTacs) result.besTacs = besTacs;
    if (tdcs) result.tdcs = tdcs;
    if (vnsHrv) result.vnsHrv = vnsHrv;
    if (audioEntrainment) result.audioEntrainment = audioEntrainment;
    if (visualStimulation) result.visualStimulation = visualStimulation;
    if (tms) result.tms = tms;
    if (pbmDeep1170nm) result.pbmDeep1170nm = pbmDeep1170nm;
    if (clinicalTacs) result.clinicalTacs = clinicalTacs;
    if (hdTdcs) result.hdTdcs = hdTdcs;
    if (cervicalVns) result.cervicalVns = cervicalVns;
    if (vibrotactile40hz) result.vibrotactile40hz = vibrotactile40hz;

    return result;
  }

  // ─── Limits sub-block parsers ─────────────────────────────────────────────

  private parseLimitsSubBlock<T extends object>(
    fieldMap: Record<string, (raw: unknown) => Partial<T>>
  ): T {
    this.skipNewlines();
    this.expect('LBRACE');
    this.skipNewlines();
    const result: Partial<T> = {};
    while (!this.tryBrace()) {
      const { key } = this.readKeyValue();
      const handler = fieldMap[key];
      if (handler) {
        const val = handler(this.readAnyValue());
        Object.assign(result, val);
      } else {
        // Skip unknown fields gracefully
        this.readAnyValue();
      }
      this.skipNewlines();
    }
    return result as T;
  }

  private parsePBMTranscranialLimits(): PBMTranscranialLimits {
    return this.parseLimitsSubBlock<PBMTranscranialLimits>({
      max_intensity: v => ({ maxIntensityPercent: Number(v) }),
      max_frequency: v => ({ maxFrequencyHz: Number(v) }),
      max_duty_cycle: v => ({ maxDutyCyclePercent: Number(v) }),
      max_session_dose: v => ({ maxSessionDoseJCm2: Number(v) }),
      max_daily_dose: v => ({ maxDailyDoseJCm2: Number(v) }),
    });
  }

  private parsePBMIntranasalLimits(): PBMIntranasalLimits {
    return this.parseLimitsSubBlock<PBMIntranasalLimits>({
      max_intensity: v => ({ maxIntensityPercent: Number(v) }),
      max_session_dose: v => ({ maxSessionDoseJCm2: Number(v) }),
      max_session_duration: v => ({ maxSessionDurationSeconds: Number(v) }),
    });
  }

  private parseEEGNeurofeedbackLimits(): EEGNeurofeedbackLimits {
    return this.parseLimitsSubBlock<EEGNeurofeedbackLimits>({
      allowed_bands: v => ({ allowedBands: Array.isArray(v) ? (v as string[]) : [String(v)] }),
      require_closed_loop: v => ({ requireClosedLoop: Boolean(v) }),
    });
  }

  private parseBESTacsLimits(): BESTacsLimits {
    return this.parseLimitsSubBlock<BESTacsLimits>({
      max_intensity: v => ({ maxIntensityMilliamps: Number(v) }),
      max_frequency: v => ({ maxFrequencyHz: Number(v) }),
      min_frequency: v => ({ minFrequencyHz: Number(v) }),
      max_session_duration: v => ({ maxSessionDurationSeconds: Number(v) }),
      max_sessions_per_day: v => ({ maxSessionsPerDay: Number(v) }),
    });
  }

  private parseTDCSLimits(): TDCSLimits {
    return this.parseLimitsSubBlock<TDCSLimits>({
      max_intensity: v => ({ maxIntensityMilliamps: Number(v) }),
      max_session_duration: v => ({ maxSessionDurationSeconds: Number(v) }),
      max_sessions_per_day: v => ({ maxSessionsPerDay: Number(v) }),
    });
  }

  private parseVNSHRVLimits(): VNSHRVLimits {
    return this.parseLimitsSubBlock<VNSHRVLimits>({
      max_intensity: v => ({ maxIntensityMilliamps: Number(v) }),
      max_frequency: v => ({ maxFrequencyHz: Number(v) }),
      max_session_duration: v => ({ maxSessionDurationSeconds: Number(v) }),
      allowed_protocols: v => ({ allowedProtocols: Array.isArray(v) ? (v as string[]) : [String(v)] }),
    });
  }

  private parseAudioEntrainmentLimits(): AudioEntrainmentLimits {
    return this.parseLimitsSubBlock<AudioEntrainmentLimits>({
      max_intensity: v => ({ maxVolumePercent: Number(v) }),
      max_frequency: v => ({ maxBinauralBeatsHz: Number(v) }),
      max_binaural_beats: v => ({ maxBinauralBeatsHz: Number(v) }),
      max_isochronic_tones: v => ({ maxIsochronicTonesHz: Number(v) }),
    });
  }

  private parseVisualStimLimits(): VisualStimLimits {
    return this.parseLimitsSubBlock<VisualStimLimits>({
      max_frequency: v => ({ maxFrequencyHz: Number(v) }),
      min_frequency: v => ({ minFrequencyHz: Number(v) }),
      allowed_modes: v => ({ allowedModes: Array.isArray(v) ? (v as string[]) : [String(v)] }),
      block_high_risk_range: v => ({ blockHighRiskRange: Boolean(v) }),
    });
  }

  private parseTMSLimits(): TMSLimits {
    return this.parseLimitsSubBlock<TMSLimits>({
      max_intensity_pct_mt: v => ({ maxIntensityPercentMT: Number(v) }),
      max_pulses_per_session: v => ({ maxPulsesPerSession: Number(v) }),
      max_pulses_per_day: v => ({ maxPulsesPerDay: Number(v) }),
      max_sessions_per_week: v => ({ maxSessionsPerWeek: Number(v) }),
      allowed_protocols: v => ({ allowedProtocols: Array.isArray(v) ? (v as string[]) : [String(v)] }),
      allowed_targets: v => ({ allowedTargets: Array.isArray(v) ? (v as string[]) : [String(v)] }),
    });
  }

  private parseDeepPBMLimits(): DeepPBMLimits {
    return this.parseLimitsSubBlock<DeepPBMLimits>({
      max_intensity: v => ({ maxIntensityMWcm2: Number(v) }),
      max_session_duration: v => ({ maxSessionDurationSeconds: Number(v) }),
    });
  }

  private parseClinicalTacsLimits(): ClinicalTacsLimits {
    return this.parseLimitsSubBlock<ClinicalTacsLimits>({
      max_intensity: v => ({ maxIntensityMilliamps: Number(v) }),
      max_session_duration: v => ({ maxSessionDurationSeconds: Number(v) }),
    });
  }

  private parseHDTdcsLimits(): HDTdcsLimits {
    return this.parseLimitsSubBlock<HDTdcsLimits>({
      max_intensity: v => ({ maxIntensityMilliamps: Number(v) }),
      max_session_duration: v => ({ maxSessionDurationSeconds: Number(v) }),
      allowed_montages: v => ({ allowedMontages: Array.isArray(v) ? (v as string[]) : [String(v)] }),
    });
  }

  private parseCervicalVnsLimits(): CervicalVnsLimits {
    return this.parseLimitsSubBlock<CervicalVnsLimits>({
      max_intensity: v => ({ maxIntensityMilliamps: Number(v) }),
      max_session_duration: v => ({ maxSessionDurationSeconds: Number(v) }),
    });
  }

  private parseVibrotactileLimits(): VibrotactileLimits {
    return this.parseLimitsSubBlock<VibrotactileLimits>({
      max_intensity: v => ({ maxIntensityG: Number(v) }),
      max_session_duration: v => ({ maxSessionDurationSeconds: Number(v) }),
    });
  }

  private parseProtocol(): NPProtocolDefinition {
    this.skipNewlines();
    if (this.current.type !== 'STRING') {
      throw new NPPSParseError(`Expected protocol name string, got ${this.current.type}`, this.current.line);
    }
    const name = this.current.value as string;
    this.advance();

    this.skipNewlines();
    this.expect('LBRACE');

    let description = '';
    let author = 'NeuroPulse';
    let version = '1.0';
    let tags: string[] = [];
    let timingMode: NPTimingMode = { type: 'duration', seconds: 1200 };
    let modalities: NPProtocolModality[] = [];
    let parsedId: string | undefined;
    let isReadOnly: boolean | undefined;
    const now = new Date().toISOString();

    this.skipNewlines();
    while (!this.tryBrace()) {
      this.skipNewlines();
      if (this.ct() === 'RBRACE') break;

      const keyTok = this.current;
      if (keyTok.type !== 'KEYWORD' && keyTok.type !== 'IDENT') {
        throw new NPPSParseError(`Expected key, got ${keyTok.type} (${String(keyTok.value)})`, keyTok.line);
      }
      const key = keyTok.value as string;
      this.advance();
      this.skipNewlines();

      // Typed modality block
      if (this.ct() === 'LBRACE') {
        modalities.push(this.parseTypedModalityBlock(key));
        this.skipNewlines();
        continue;
      }

      this.expect('COLON');

      switch (key) {
        case 'id': parsedId = this.readString(); break;
        case 'description': description = this.readString(); break;
        case 'author': author = this.readString(); break;
        case 'version': version = this.readString(); break;
        case 'readonly': isReadOnly = this.readBool(); break;
        case 'tags': tags = this.readTagArray(); break;
        case 'duration': timingMode = { type: 'duration', seconds: this.readDurationSeconds() }; break;
        case 'interval_count': timingMode = { type: 'interval_count', count: this.readNumber() }; break;
        default:
          this.skipValue();
          break;
      }
      this.skipNewlines();
    }

    const id = parsedId ?? crypto.randomUUID();

    const proto: NPProtocolDefinition = {
      id, name, description, author, version, tags,
      createdAt: now, modifiedAt: now,
      isPredefined: false,
      timingMode,
      modalities,
    };
    if (isReadOnly !== undefined) proto.isReadOnly = isReadOnly;
    if (parsedId !== undefined) proto.isPredefined = true;
    return proto;
  }

  private tryBrace(): boolean {
    this.skipNewlines();
    if (this.current.type === 'RBRACE') { this.advance(); return true; }
    return false;
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
    if (this.current.type !== 'STRING') {
      throw new NPPSParseError(`Expected composite name string, got ${this.current.type}`, this.current.line);
    }
    const name = this.current.value as string;
    this.advance();

    this.skipNewlines();
    this.expect('LBRACE');
    this.skipNewlines();

    let description = '';
    let author = 'NeuroPulse';
    let version = '1.0';
    let tags: string[] = [];
    let layers: NPCompositeLayer[] = [];
    let conflictResolution: NPCompositeProtocol['conflictResolution'] = 'merge';
    let parsedId: string | undefined;
    let isReadOnly: boolean | undefined;
    const now = new Date().toISOString();

    while (!this.tryBrace()) {
      this.skipNewlines();
      if (this.ct() === 'RBRACE') break;

      const keyTok = this.current;
      if ((keyTok.type === 'KEYWORD' || keyTok.type === 'IDENT') && keyTok.value === 'layer') {
        this.advance();
        layers.push(this.parseLayerBlock());
        this.skipNewlines();
        continue;
      }

      const { key } = this.readKeyValue();
      switch (key) {
        case 'id': parsedId = this.readString(); break;
        case 'description': description = this.readString(); break;
        case 'author': author = this.readString(); break;
        case 'version': version = this.readString(); break;
        case 'readonly': isReadOnly = this.readBool(); break;
        case 'tags': tags = this.readTagArray(); break;
        case 'conflict_resolution': conflictResolution = this.readString() as NPCompositeProtocol['conflictResolution']; break;
        default:
          this.skipValue();
          break;
      }
      this.skipNewlines();
    }

    const id = parsedId ?? crypto.randomUUID();
    const composite: NPCompositeProtocol = {
      id, name, description, author, version, tags,
      createdAt: now, modifiedAt: now,
      isPredefined: false,
      layers, conflictResolution,
    };
    if (isReadOnly !== undefined) composite.isReadOnly = isReadOnly;
    if (parsedId !== undefined) composite.isPredefined = true;
    return composite;
  }

  private parseLayerBlock(): NPCompositeLayer {
    this.skipNewlines();
    if (this.current.type !== 'STRING') {
      throw new NPPSParseError(`Expected layer protocol name string, got ${this.current.type}`, this.current.line);
    }
    const protocolName = this.current.value as string;
    this.advance();

    this.skipNewlines();
    this.expect('LBRACE');
    this.skipNewlines();

    let startOffsetSeconds = 0;
    let durationSeconds: number | undefined;
    let intensityScale = 1.0;
    const id = crypto.randomUUID();

    while (!this.tryBrace()) {
      const { key } = this.readKeyValue();
      switch (key) {
        case 'start': startOffsetSeconds = this.readDurationSeconds(); break;
        case 'end': durationSeconds = this.readDurationSeconds() - startOffsetSeconds; break;
        case 'duration': durationSeconds = this.readDurationSeconds(); break;
        case 'intensity_scale': intensityScale = this.readNumber(); break;
        default:
          this.skipValue();
          break;
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

/**
 * Parse the first `limits` block found in the given NPPS text.
 * Returns null if no limits block is present.
 */
export function parseNPPSLimits(text: string): NPLimitsSet | null {
  const tokens = tokenize(text);
  const parser = new Parser(tokens);

  // Scan through tokens looking for a 'limits' keyword at the top level
  parser['skipNewlines']();
  while (parser['current'].type !== 'EOF') {
    parser['skipNewlines']();
    if (parser['ct']() === 'EOF') break;

    const t = parser['current'];
    if ((t.type === 'KEYWORD' || t.type === 'IDENT') && t.value === 'limits') {
      parser['advance']();
      return parser.parseLimitsBlockPublic();
    } else if ((t.type === 'KEYWORD' || t.type === 'IDENT') && t.value === 'protocol') {
      // Skip entire protocol block
      parser['advance']();
      parser['skipNewlines']();
      if (parser['current'].type === 'STRING') { parser['advance'](); } // skip inline name
      parser['skipNewlines']();
      parser['expect']('LBRACE');
      parser['skipNewlines']();
      let depth = 1;
      while (depth > 0 && (parser['current'].type as string) !== 'EOF') {
        if (parser['current'].type === 'LBRACE') depth++;
        else if (parser['current'].type === 'RBRACE') depth--;
        parser['advance']();
        parser['skipNewlines']();
      }
    } else if ((t.type === 'KEYWORD' || t.type === 'IDENT') && t.value === 'composite') {
      // Skip entire composite block
      parser['advance']();
      parser['skipNewlines']();
      if (parser['current'].type === 'STRING') { parser['advance'](); } // skip inline name
      parser['skipNewlines']();
      parser['expect']('LBRACE');
      parser['skipNewlines']();
      let depth = 1;
      while (depth > 0 && (parser['current'].type as string) !== 'EOF') {
        if (parser['current'].type === 'LBRACE') depth++;
        else if (parser['current'].type === 'RBRACE') depth--;
        parser['advance']();
        parser['skipNewlines']();
      }
    } else {
      // Unknown token — stop
      break;
    }
    parser['skipNewlines']();
  }

  return null;
}
