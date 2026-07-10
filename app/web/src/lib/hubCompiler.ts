/*
 * NeurOne Hub Protocol Compiler
 * Document: NP-FW-HUB-001 Rev A
 *
 * Converts an NPProtocolDefinition (high-level modality params + timing) into
 * the binary wire format consumed by the hub firmware (np_protocol.c).
 *
 * Binary layout:
 *   [64 bytes]  np_proto_header_t  (little-endian, packed)
 *   [N × (12 + params_len) bytes]  np_proto_cmd_hdr_t + params[]
 *   [64 bytes]  Ed25519 signature placeholder (zeroed; real signing done server-side)
 *
 * Header offsets (64 bytes, no padding):
 *   0:  uint32 magic             (NP_HUB_PROTO_MAGIC = 0x4E504850)
 *   4:  uint16 version           (NP_HUB_PROTO_VERSION = 0x0001)
 *   6:  uint8  flags             (bit0=T2_tier, bit1=autonomous)
 *   7:  uint8  cmd_count
 *   8:  uint8[16] session_uuid
 *   24: uint32 compiled_at_unix
 *   28: uint8[32] device_serial
 *   60: uint32 session_duration_ms
 *
 * Command header offsets (12 bytes):
 *   0:  uint8  mod_type
 *   1:  uint8  slot_mask
 *   2:  uint32 start_ms
 *   6:  uint32 duration_ms
 *   10: uint16 params_len
 */

import {
  NPProtocolDefinition,
  NPModalityParams,
  NPIntervalConfig,
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

// ─── Wire format constants (mirrors np_hub_config.h) ─────────────────────────

const PROTO_MAGIC = 0x4E504850;
const PROTO_VERSION = 0x0001;
const PROTO_UUID_LEN = 16;
const PROTO_SERIAL_LEN = 32;
const PROTO_SIG_LEN = 64;
const PROTO_CMD_MAX = 64;
const HEADER_LEN = 64;
const CMD_HDR_LEN = 12;

// ─── Module type IDs (mirrors np_hub_types.h np_hub_mod_type_t) ──────────────

const NP_MOD_NONE         = 0x00;
const NP_MOD_PBM_BASE     = 0x01;
const NP_MOD_PBM_SMART    = 0x02;
const NP_MOD_INTRANASAL   = 0x03;
const NP_MOD_EEG          = 0x04;
const NP_MOD_BES_TACS     = 0x05;
const NP_MOD_TDCS         = 0x06;
const NP_MOD_VNS_HRV      = 0x07;
const NP_MOD_AUDIO        = 0x08;
const NP_MOD_VISUAL       = 0x09;
const NP_MOD_CVNS         = 0x0A;
const NP_MOD_QEEG_21CH    = 0x0B;
const NP_MOD_TMS          = 0x0C;
const NP_MOD_PBM_1170NM   = 0x0D;
const NP_MOD_CLIN_TACS    = 0x0E;
const NP_MOD_HD_TDCS      = 0x0F;
const NP_MOD_VIBROTACTILE = 0x10;

// ─── Protocol flags ───────────────────────────────────────────────────────────

const PROTO_FLAG_T2_TIER    = 1 << 0;

// ─── Slot mask conventions ────────────────────────────────────────────────────
// Zone modules (PBM): slot_mask is a per-zone bitmask (bits 0–4).
// All other modules: slot_mask = 0x01 (single instance; routing is by mod_type).

const SLOT_MASK_ALL_ZONES = 0x1F;  // zones 0–4

// ─── T2 modality type set ─────────────────────────────────────────────────────

const T2_MODALITY_TYPES = new Set([
  'qeeg_21ch', 'tms', 'pbm_deep_1170nm', 'clinical_tacs', 'hd_tdcs', 'cervical_vns',
]);

// ─── Internal session command ─────────────────────────────────────────────────

interface SessionCmd {
  modType: number;
  slotMask: number;
  startMs: number;
  durationMs: number;
  params: Uint8Array;   // empty = stop signal (params_len 0)
}

// ─── Compiled protocol output ─────────────────────────────────────────────────

export interface CompiledProtocol {
  /** Full protocol blob: header + commands + 64-byte zeroed signature placeholder. */
  blob: Uint8Array;
  /** Session UUID embedded in the blob (for UHDR key association). */
  sessionUuid: Uint8Array;
  /** True if any T2 modality is present (NP_PROTO_FLAG_T2_TIER was set). */
  isT2: boolean;
  /** Total command count including interval stop commands. */
  cmdCount: number;
}

// ─── Public API ───────────────────────────────────────────────────────────────

/**
 * Compile an NPProtocolDefinition into a binary hub protocol blob.
 *
 * @param proto          Protocol definition from the app's protocol library.
 * @param deviceSerial   32-byte device serial (replay guard); pass zeros for dev/test.
 * @returns              CompiledProtocol with the complete blob ready for transmission.
 *                       The Ed25519 signature (last 64 bytes) is zeroed — attach real
 *                       signature from signing service before sending to hub.
 */
export function compileProtocol(
  proto: NPProtocolDefinition,
  deviceSerial?: Uint8Array,
): CompiledProtocol {
  const sessionDurationMs = proto.timingMode.type === 'duration'
    ? proto.timingMode.seconds * 1000
    : 0;  // interval_count protocols run until repeat exhausted

  // Collect commands from all enabled modalities
  const cmds: SessionCmd[] = [];
  let isT2 = false;

  for (const modality of proto.modalities) {
    if (!modality.enabled) continue;

    const mp = modality.modalityParams;
    if (T2_MODALITY_TYPES.has(mp.type)) isT2 = true;

    const generated = buildCommands(mp, modality.interval, sessionDurationMs);
    for (const cmd of generated) {
      cmds.push(cmd);
      // Fail loudly rather than silently truncating: dropping commands here
      // would produce a signed session missing entire later modalities with no
      // error surfaced to the clinician. The check is inside the loop so an
      // unbounded interval expansion cannot run away before it is caught.
      if (cmds.length > PROTO_CMD_MAX) {
        throw new Error(
          `Protocol exceeds maximum command count (${PROTO_CMD_MAX}). ` +
          `Reduce interval repeats or the number of enabled modalities.`
        );
      }
    }
  }

  if (cmds.length === 0) {
    throw new Error('Protocol produces no commands — no enabled modalities');
  }

  // Sort by start_ms (hub requires sorted order; ties: modality declaration order)
  cmds.sort((a, b) => a.startMs - b.startMs);

  // Allocate blob
  const cmdPayloadLen = cmds.reduce((sum, c) => sum + CMD_HDR_LEN + c.params.length, 0);
  const totalLen = HEADER_LEN + cmdPayloadLen + PROTO_SIG_LEN;
  const blob = new Uint8Array(totalLen);
  const dv = new DataView(blob.buffer);

  // Session UUID — 16 random bytes
  const sessionUuid = new Uint8Array(PROTO_UUID_LEN);
  crypto.getRandomValues(sessionUuid);

  // Device serial — 32 bytes (zeroed if not provided)
  const serial = new Uint8Array(PROTO_SERIAL_LEN);
  if (deviceSerial) serial.set(deviceSerial.slice(0, PROTO_SERIAL_LEN));

  // ── Header ────────────────────────────────────────────────────────────────
  let flags = 0;
  if (isT2) flags |= PROTO_FLAG_T2_TIER;

  dv.setUint32(0, PROTO_MAGIC, true);
  dv.setUint16(4, PROTO_VERSION, true);
  dv.setUint8(6, flags);
  dv.setUint8(7, cmds.length);
  blob.set(sessionUuid, 8);
  dv.setUint32(24, Math.floor(Date.now() / 1000), true);
  blob.set(serial, 28);
  dv.setUint32(60, sessionDurationMs, true);

  // ── Commands ──────────────────────────────────────────────────────────────
  let offset = HEADER_LEN;
  for (const cmd of cmds) {
    dv.setUint8(offset + 0, cmd.modType);
    dv.setUint8(offset + 1, cmd.slotMask);
    dv.setUint32(offset + 2, cmd.startMs, true);
    dv.setUint32(offset + 6, cmd.durationMs, true);
    dv.setUint16(offset + 10, cmd.params.length, true);
    offset += CMD_HDR_LEN;
    if (cmd.params.length > 0) {
      blob.set(cmd.params, offset);
      offset += cmd.params.length;
    }
  }

  // ── Signature placeholder (zeroed — signing done server-side) ─────────────
  // offset now == HEADER_LEN + cmdPayloadLen == totalLen - PROTO_SIG_LEN

  return { blob, sessionUuid, isT2, cmdCount: cmds.length };
}

// ─── Command generation ────────────────────────────────────────────────────────

/**
 * Generate the sequence of on/stop SessionCmd records for one modality.
 * Continuous (intervalOn == 0): single command with duration_ms = 0 (until end).
 * Interval: alternating ON + stop commands for each repeat.
 */
function* buildCommands(
  mp: NPModalityParams,
  interval: NPIntervalConfig,
  sessionDurationMs: number,
): Iterable<SessionCmd> {
  const { modType, slotMask, params } = encodeParams(mp);
  if (modType === NP_MOD_NONE) return;

  const isContinuous = interval.intervalOnSeconds === 0;
  if (isContinuous) {
    yield { modType, slotMask, startMs: 0, durationMs: 0, params };
    return;
  }

  const onMs  = interval.intervalOnSeconds  * 1000;
  const offMs = interval.intervalOffSeconds * 1000;
  const periodMs = onMs + offMs;
  const maxRepeats = interval.repeatCount !== undefined
    ? interval.repeatCount
    : sessionDurationMs > 0 ? Math.ceil(sessionDurationMs / periodMs) : 1;

  const STOP = new Uint8Array(0);
  for (let i = 0; i < maxRepeats; i++) {
    const startMs = i * periodMs;
    if (sessionDurationMs > 0 && startMs >= sessionDurationMs) break;
    yield { modType, slotMask, startMs, durationMs: onMs, params };
    const stopMs = startMs + onMs;
    if (sessionDurationMs === 0 || stopMs < sessionDurationMs) {
      yield { modType, slotMask, startMs: stopMs, durationMs: 0, params: STOP };
    }
  }
}

// ─── Parameter encoding ────────────────────────────────────────────────────────

interface EncodedParams {
  modType: number;
  slotMask: number;
  params: Uint8Array;
}

function encodeParams(mp: NPModalityParams): EncodedParams {
  switch (mp.type) {

    // ── T1 modalities ────────────────────────────────────────────────────────

    case 'pbm_transcranial':     return encodePBMTranscranial(mp.params);
    case 'pbm_intranasal':       return encodePBMIntranasal(mp.params);
    case 'eeg_neurofeedback':    return encodeEEG(mp.params);
    case 'bes_tacs':             return encodeBESTacs(mp.params);
    case 'tdcs':                 return encodeTDCS(mp.params);
    case 'vns_hrv':              return encodeVNSHRV(mp.params);
    case 'audio_entrainment':    return encodeAudio(mp.params);
    case 'visual_stimulation':   return encodeVisual(mp.params);

    // ── T2 modalities ────────────────────────────────────────────────────────

    case 'qeeg_21ch':            return encodeQEEG21ch(mp.params);
    case 'tms':                  return encodeTMS(mp.params);
    case 'pbm_deep_1170nm':      return encodePBM1170nm(mp.params);
    case 'clinical_tacs':        return encodeClinicalTacs(mp.params);
    case 'hd_tdcs':              return encodeHDTdcs(mp.params);
    case 'cervical_vns':         return encodeCervicalVNS(mp.params);

    // ── Accessory ────────────────────────────────────────────────────────────

    case 'vibrotactile_40hz':    return encodeVibrotactile(mp.params);

    default: {
      const _exhaustive: never = mp;
      throw new Error(`Unknown modality type in hub compiler: ${JSON.stringify(_exhaustive)}`);
    }
  }
}

// ─── Encoding helpers ─────────────────────────────────────────────────────────

/** Convert frequency Hz to the hub's freq_code byte (0x00 = CW). */
function freqCode(hz: number): number {
  return hz <= 0 ? 0x00 : (Math.round(hz) & 0xFF);
}

/** Convert duty cycle percent to hub duty register (0x32 = 25% max, enforced). */
function dutyReg(pct: number): number {
  return Math.min(Math.round(pct * 2), 0x32);
}

/** Convert intensity percent 0–100 to 0–255 LED current register. */
function intensityReg(pct: number): number {
  return Math.min(Math.round(pct / 100 * 255), 255);
}

/** Convert zone spec to slot_mask bitmask for zone slots 0–4. */
function zoneSlotMask(p: PBMTranscranialParams): number {
  if (p.zones === 'all')    return 0x1F;
  if (p.zones === 'front')  return 0x07;  // zones 0–2
  if (p.zones === 'rear')   return 0x18;  // zones 3–4
  if (p.zones === 'custom' && p.customZones) {
    return p.customZones.reduce((mask, z) => mask | (1 << (z & 0x1F)), 0) & 0x1F;
  }
  return SLOT_MASK_ALL_ZONES;
}

/** Map TMS / HD-tDCS target string to 3-bit index. */
function targetIndex(t: TMSParams['target']): number {
  const map: Record<TMSParams['target'], number> = {
    DLPFC_L: 0, DLPFC_R: 1, VLPFC_L: 2, ACC: 3, MPFC: 4, M1_L: 5, M1_R: 6,
  };
  return map[t] ?? 0;
}

// ─── T1 encoders ─────────────────────────────────────────────────────────────

function encodePBMTranscranial(p: PBMTranscranialParams): EncodedParams {
  const useSmart = p.wavelength !== '660_808nm';
  const modType  = useSmart ? NP_MOD_PBM_SMART : NP_MOD_PBM_BASE;
  const slotMask = zoneSlotMask(p);
  const cur      = intensityReg(p.intensityPercent);
  const duty     = dutyReg(p.dutyCyclePercent);
  const fc       = freqCode(p.frequencyHz);

  let params: Uint8Array;
  if (useSmart) {
    // np_mod_pbm_smart_params_t: 6 bytes
    const chMask = p.wavelength === '1064nm' ? 0x04 : 0x07;  // 0x07 = all 3 channels
    params = new Uint8Array([fc, duty, cur, cur, cur, chMask]);
  } else {
    // np_mod_pbm_base_params_t: 4 bytes
    params = new Uint8Array([fc, duty, cur, cur]);
  }
  return { modType, slotMask, params };
}

function encodePBMIntranasal(p: PBMIntranasalParams): EncodedParams {
  // np_mod_intranasal_params_t: 5 bytes
  // side=0 (bilateral), freq_code, duty, cur_660, cur_808
  const cur  = intensityReg(p.intensityPercent);
  const duty = dutyReg(p.dutyCyclePercent);
  const fc   = freqCode(p.frequencyHz);
  return {
    modType: NP_MOD_INTRANASAL,
    slotMask: 0x01,
    params: new Uint8Array([0x00, fc, duty, cur, cur]),
  };
}

function encodeEEG(p: EEGNeurofeedbackParams): EncodedParams {
  // np_mod_eeg_params_t: 5 bytes
  // channel_mask, gain, notch, ref_mode, adaptive_out
  const chMap: Record<EEGNeurofeedbackParams['channels'], number> = {
    all:     0xFF,
    front:   0x03,  // Fp1+Fp2
    central: 0x3C,  // F3+F4+C3+C4
    custom:  0xFF,
  };
  let chMask = chMap[p.channels] ?? 0xFF;
  if (p.channels === 'custom' && p.customChannels) {
    const labels = ['Fp1','Fp2','F3','F4','C3','C4','P3','P4'];
    chMask = p.customChannels.reduce((m, ch) => {
      const i = labels.indexOf(ch);
      return i >= 0 ? m | (1 << i) : m;
    }, 0);
  }
  const adaptiveOut = p.closedLoopEnabled ? 0x03 : 0x00;  // 3 = drive both PBM + audio
  return {
    modType: NP_MOD_EEG,
    slotMask: 0x01,
    params: new Uint8Array([chMask, 6, 2, 0, adaptiveOut]),  // gain=24×, notch=60Hz, ref=linked_ear
  };
}

function encodeBESTacs(p: BESTacsParams): EncodedParams {
  // np_mod_bes_tacs_params_t: 8 bytes (electrode_pair, freq_mhz×2, amplitude_ua×2, waveform, phase_deg)
  const freqMhz = Math.min(Math.round(p.frequencyHz * 1000), 0xFFFF);
  const ampUa   = Math.min(Math.round(p.intensityMilliamps * 1000), 1000);
  const wf      = p.waveform === 'sinusoidal' ? 0 : p.waveform === 'square' ? 1 : 0;
  const buf = new Uint8Array(8);
  const dv  = new DataView(buf.buffer);
  dv.setUint8(0, 0);                  // electrode_pair = F3-F4
  dv.setUint16(1, freqMhz, true);
  dv.setUint16(3, ampUa, true);
  dv.setUint8(5, wf);
  dv.setUint8(6, 0);                  // phase_deg = 0
  dv.setUint8(7, 0);                  // padding (struct is 8 bytes with uint16 alignment)
  return { modType: NP_MOD_BES_TACS, slotMask: 0x01, params: buf };
}

function encodeTDCS(p: TDCSParams): EncodedParams {
  // np_mod_tdcs_params_t: 6 bytes (electrode_pair, current_ua×2, polarity, ramp_s×2)
  const curUa  = Math.min(Math.round(p.intensityMilliamps * 1000), 2000);
  const rampS  = Math.max(p.rampSeconds, 30);  // firmware enforces ≥30s
  const buf = new Uint8Array(6);
  const dv  = new DataView(buf.buffer);
  dv.setUint8(0, resolveElectrodePair(p.electrodePairs[0]));
  dv.setUint16(1, curUa, true);
  dv.setUint8(3, 0);                  // polarity = anode at pair-A
  dv.setUint16(4, rampS, true);
  return { modType: NP_MOD_TDCS, slotMask: 0x01, params: buf };
}

/** Map a string electrode pair to the hub's 0-based montage index. */
function resolveElectrodePair(pair?: [string, string]): number {
  if (!pair) return 0;
  const [a, b] = [pair[0].toUpperCase(), pair[1].toUpperCase()];
  if ((a === 'F3' && b === 'F4') || (a === 'F4' && b === 'F3')) return 0;
  if ((a === 'P3' && b === 'P4') || (a === 'P4' && b === 'P3')) return 1;
  if ((a === 'FZ' && b === 'PZ') || (a === 'PZ' && b === 'FZ')) return 2;
  return 0;
}

function encodeVNSHRV(p: VNSHRVParams): EncodedParams {
  // np_mod_vns_hrv_params_t: 8 bytes (side, freq_mhz×2, amplitude_ua×2, pulse_width_us, ppg_enable, eeg_ref_enable, hrv_proto)
  const freqMhz = Math.min(Math.round(p.frequencyHz * 1000), 25000);
  const ampUa   = Math.min(Math.round(p.intensityMilliamps * 1000), 2000);
  const protoMap: Record<VNSHRVParams['hrvProtocol'], number> = {
    standalone:      0,
    combined_pbm:    1,
    tavns_sync:      2,
    eeg_biofeedback: 3,
  };
  const buf = new Uint8Array(9);
  const dv  = new DataView(buf.buffer);
  dv.setUint8(0, 0);                  // side = left (standard)
  dv.setUint16(1, freqMhz, true);
  dv.setUint16(3, ampUa, true);
  dv.setUint8(5, 0);                  // pulse_width_us = default 250µs
  dv.setUint8(6, 1);                  // ppg_enable = true (HRV monitoring on)
  dv.setUint8(7, 1);                  // eeg_ref_enable = true (A1/A2 to EEG)
  dv.setUint8(8, protoMap[p.hrvProtocol] ?? 0);
  return { modType: NP_MOD_VNS_HRV, slotMask: 0x01, params: buf };
}

function encodeAudio(p: AudioEntrainmentParams): EncodedParams {
  // np_mod_audio_params_t: 8 bytes (mode, carrier_hz×2, beat_mhz×2, volume_pct, bone_conduct_en, eeg_adaptive)
  let mode = 4;  // off
  let beatMhz = 0;
  if (p.binauralBeatsHz !== undefined)   { mode = 0; beatMhz = Math.round(p.binauralBeatsHz * 1000); }
  else if (p.isochronicTonesHz !== undefined) { mode = 1; beatMhz = Math.round(p.isochronicTonesHz * 1000); }
  else if (p.noiseType === 'pink')  mode = 2;
  else if (p.noiseType === 'brown') mode = 3;

  const volPct = Math.min(p.volumePercent, 100);
  const buf = new Uint8Array(8);
  const dv  = new DataView(buf.buffer);
  dv.setUint8(0, mode);
  dv.setUint16(1, Math.min(p.carrierHz, 0xFFFF), true);
  dv.setUint16(3, Math.min(beatMhz, 0xFFFF), true);
  dv.setUint8(5, volPct);
  dv.setUint8(6, p.boneConductionPacer ? 1 : 0);
  dv.setUint8(7, p.eegAdaptive ? 1 : 0);
  return { modType: NP_MOD_AUDIO, slotMask: 0x01, params: buf };
}

function encodeVisual(p: VisualStimParams): EncodedParams {
  // np_mod_visual_params_t: 9 bytes
  // mode, freq_hz, duty_pct, wl_select, zone_mask_lo, zone_mask_hi, emdr_rate, mode_f_enable, shade_req
  const modeMap: Record<VisualStimParams['mode'], number> = {
    binocular:   0,
    emdr:        1,
    retinal_pbm: 2,
    mode_f:      3,
  };
  const freqHz    = Math.min(Math.round(p.frequencyHz), 100);
  // emdr_rate stored in deci-Hz (0.1Hz units) to fit uint8_t: 1Hz → 10, 2Hz → 20
  const emdrRate  = Math.min(Math.round(p.emdrCadenceHz * 10), 255);
  const shadeReq  = modeMap[p.mode] === 0 ? 1 : 0;  // S1 opaque required for binocular photic
  return {
    modType: NP_MOD_VISUAL,
    slotMask: 0x01,
    params: new Uint8Array([
      modeMap[p.mode] ?? 0,
      freqHz,
      50,          // duty_pct = 50%
      0x02,        // wl_select = both 660+808nm
      0xFF,        // zone_mask_lo = all 8 zones
      0x0F,        // zone_mask_hi = zones 8–11
      emdrRate,
      p.enableModeF ? 1 : 0,
      shadeReq,
    ]),
  };
}

// ─── T2 encoders ──────────────────────────────────────────────────────────────

function encodeQEEG21ch(p: QEEG21chParams): EncodedParams {
  // np_mod_qeeg_21ch_params_t: 8 bytes (channel_mask×4, gain, notch, ref_mode, sloreta_en)
  const chMask    = p.montage === 'standard_1020' ? 0x1FFFFF : 0x1FFFFF;  // all 21 channels
  const refMap: Record<QEEG21chParams['reference'], number> = {
    linked_ear: 0, cz: 1, average: 2,
  };
  const buf = new Uint8Array(8);
  const dv  = new DataView(buf.buffer);
  dv.setUint32(0, chMask, true);
  dv.setUint8(4, 6);                  // gain = 24× (clinical standard)
  dv.setUint8(5, 2);                  // notch = 60Hz
  dv.setUint8(6, refMap[p.reference] ?? 0);
  dv.setUint8(7, p.sloretaEnabled ? 1 : 0);
  return { modType: NP_MOD_QEEG_21CH, slotMask: 0x01, params: buf };
}

function encodeTMS(p: TMSParams): EncodedParams {
  // np_mod_tms_params_t: 10 bytes
  // protocol, target, freq_mhz×2, intensity_pct_mt, pulse_count×2, inter_train_ms×2, tbs_burst_hz
  const protoMap: Record<TMSParams['tmsProtocol'], number> = {
    rTMS: 0, TBS: 1, iTBS: 2,
  };
  const freqMhz     = Math.min(Math.round(p.frequencyHz * 1000), 0xFFFF);
  const pulseCount  = Math.min(p.pulseCount, 0xFFFF);
  const isTbs       = p.tmsProtocol !== 'rTMS';
  const interTrain  = isTbs ? 200 : Math.round(1000 / Math.max(p.frequencyHz, 0.1));
  const tbsBurstHz  = isTbs ? 50 : 0;

  const buf = new Uint8Array(10);
  const dv  = new DataView(buf.buffer);
  dv.setUint8(0, protoMap[p.tmsProtocol] ?? 0);
  dv.setUint8(1, targetIndex(p.target));
  dv.setUint16(2, freqMhz, true);
  dv.setUint8(4, Math.min(Math.round(p.intensityPercentMT), 255));
  dv.setUint16(5, pulseCount, true);
  dv.setUint16(7, Math.min(interTrain, 0xFFFF), true);
  dv.setUint8(9, tbsBurstHz);
  return { modType: NP_MOD_TMS, slotMask: 0x01, params: buf };
}

function encodePBM1170nm(p: DeepPBM1170Params): EncodedParams {
  // np_mod_pbm_1170nm_params_t: 5 bytes (intensity_mw_cm2×2, freq_code, duty, tec_target_c)
  const intMwCm2 = Math.min(Math.round(p.intensityMWcm2), 1000);
  const duty     = dutyReg(p.dutyCyclePercent);
  const fc       = freqCode(p.frequencyHz);
  const buf = new Uint8Array(5);
  const dv  = new DataView(buf.buffer);
  dv.setUint16(0, intMwCm2, true);
  dv.setUint8(2, fc);
  dv.setUint8(3, duty);
  dv.setUint8(4, 0);                  // tec_target_c = 0 (auto)
  return { modType: NP_MOD_PBM_1170NM, slotMask: 0x01, params: buf };
}

function encodeClinicalTacs(p: ClinicalTacsParams): EncodedParams {
  // np_mod_clin_tacs_params_t: 7 bytes
  // freq_mhz×2, amplitude_ua×2, channel_mask_lo, channel_mask_hi, waveform
  const freqMhz = Math.min(Math.round(p.frequencyHz * 1000), 0xFFFF);
  const ampUa   = Math.min(Math.round(p.intensityMilliamps * 1000), 4000);
  const wf      = p.waveform === 'sinusoidal' ? 0 : p.waveform === 'square' ? 1 : 2;
  // Activate first channelCount channels
  const n       = Math.min(p.channelCount, 16);
  const fullMask = n === 16 ? 0xFFFF : (1 << n) - 1;
  const buf = new Uint8Array(7);
  const dv  = new DataView(buf.buffer);
  dv.setUint16(0, freqMhz, true);
  dv.setUint16(2, ampUa, true);
  dv.setUint8(4, fullMask & 0xFF);
  dv.setUint8(5, (fullMask >> 8) & 0xFF);
  dv.setUint8(6, wf);
  return { modType: NP_MOD_CLIN_TACS, slotMask: 0x01, params: buf };
}

function encodeHDTdcs(p: HDTdcsParams): EncodedParams {
  // np_mod_hd_tdcs_params_t: 6 bytes (target, montage, current_ua×2, ramp_s×2)
  const montageMap: Record<HDTdcsParams['montage'], number> = {
    ring_4x1: 0, bilateral_4x1: 1, standard_2el: 2,
  };
  const curUa = Math.min(Math.round(p.intensityMilliamps * 1000), 2000);
  const buf = new Uint8Array(6);
  const dv  = new DataView(buf.buffer);
  dv.setUint8(0, targetIndex(p.target));
  dv.setUint8(1, montageMap[p.montage] ?? 0);
  dv.setUint16(2, curUa, true);
  dv.setUint16(4, 30, true);          // ramp_s = 30 (firmware enforces ≥30s)
  return { modType: NP_MOD_HD_TDCS, slotMask: 0x01, params: buf };
}

function encodeCervicalVNS(p: CervicalVnsParams): EncodedParams {
  // np_mod_cvns_params_t: 9 bytes (side, freq_mhz×2, amplitude_ua×2, pulse_width_us, ramp_s×2, baseline_req)
  const freqMhz = Math.min(Math.round(p.frequencyHz * 1000), 25000);
  const ampUa   = Math.min(Math.round(p.intensityMilliamps * 1000), 2000);
  const buf = new Uint8Array(9);
  const dv  = new DataView(buf.buffer);
  dv.setUint8(0, 0);                  // side = right (default)
  dv.setUint16(1, freqMhz, true);
  dv.setUint16(3, ampUa, true);
  dv.setUint8(5, 0);                  // pulse_width_us = default
  dv.setUint16(6, 10, true);          // ramp_s = 10 (firmware enforces ≥10s)
  dv.setUint8(8, 1);                  // baseline_req = true (cardiac interlock required)
  return { modType: NP_MOD_CVNS, slotMask: 0x01, params: buf };
}

// ─── Accessory encoder ────────────────────────────────────────────────────────

function encodeVibrotactile(p: VibrotactileParams): EncodedParams {
  // np_mod_vibrotactile_params_t: 4 bytes (gain_reg, sync_flags, freq_mhz×2)
  // gain_reg: 0.6G → 0x00, 1.2G → 0x7F; linear map over [0.6, 1.2]
  const clamped = Math.max(0.6, Math.min(1.2, p.intensityG));
  const gainReg = Math.round((clamped - 0.6) / 0.6 * 0x7F);
  const syncFlags = (p.syncToAudio ? 0x01 : 0) | (p.syncToVisual ? 0x02 : 0);
  const buf = new Uint8Array(4);
  const dv  = new DataView(buf.buffer);
  dv.setUint8(0, gainReg);
  dv.setUint8(1, syncFlags);
  dv.setUint16(2, 40000, true);       // freq_mhz = 40Hz locked; firmware enforces ±500
  return { modType: NP_MOD_VIBROTACTILE, slotMask: 0x01, params: buf };
}
