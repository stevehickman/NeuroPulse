//! The hub session-descriptor compiler (NP-FW-HUB-001 §4): a port of
//! app/web/src/lib/hubCompiler.ts. The wire format has one writer now. The caller supplies
//! what is not deterministic (the clock, the session UUID) and signs the raw region the
//! blob ends with a zeroed 64-byte slot for.

use crate::js::{js_round, num_string, to_u16, to_u32, to_u8};
use crate::wavelength::{resolve_one_channel, Channel, WavelengthRules};
use serde_json::Value;
use std::collections::HashMap;

pub const SIG_LEN: usize = 64;
const MAGIC: u32 = 0x4E50_4850;
const VERSION: u16 = 1;
const HEADER_LEN: usize = 64;
const CMD_HDR_LEN: usize = 14;
const CMD_MAX: usize = 64;
const SOCKET_MASK_BYTES: usize = 16;
const SOCKET_COUNT: u32 = 80;
const TARGET_SLOT: u8 = 0x00;
const TARGET_SOCKET_MASK: u8 = 0x01;
const SLOT_FIRST_VALID: u32 = 5;
const SLOT_MAX: u32 = 19;
const SLOT_NONE: u8 = 0xFF;

const SLOT_EEG: u32 = 5;
const SLOT_AUDIO: u32 = 6;
const SLOT_VISUAL: u32 = 7;
const SLOT_VNS_HRV: u32 = 8;
const SLOT_INTRANASAL: u32 = 9;
const SLOT_CVNS: u32 = 10;
const SLOT_QEEG: u32 = 11;
const SLOT_TMS: u32 = 12;
const SLOT_PBM_1170NM: u32 = 13;
const SLOT_CLIN_TACS: u32 = 14;
const SLOT_HD_TDCS: u32 = 15;
const SLOT_VIBROTACTILE: u32 = 16;
const SLOT_BES_TACS: u32 = 17;
const SLOT_TDCS: u32 = 18;

const MOD_PBM_BASE: u8 = 0x01;
const MOD_PBM_SMART: u8 = 0x02;
const MOD_INTRANASAL: u8 = 0x03;
const MOD_EEG: u8 = 0x04;
const MOD_BES_TACS: u8 = 0x05;
const MOD_TDCS: u8 = 0x06;
const MOD_VNS_HRV: u8 = 0x07;
const MOD_AUDIO: u8 = 0x08;
const MOD_VISUAL: u8 = 0x09;
const MOD_CVNS: u8 = 0x0A;
const MOD_QEEG_21CH: u8 = 0x0B;
const MOD_TMS: u8 = 0x0C;
const MOD_PBM_1170NM: u8 = 0x0D;
const MOD_CLIN_TACS: u8 = 0x0E;
const MOD_HD_TDCS: u8 = 0x0F;
const MOD_VIBROTACTILE: u8 = 0x10;

const FLAG_T2_TIER: u8 = 1;
const FLAG_AUTONOMOUS: u8 = 1 << 1;

/// Full-scale irradiance per channel, mW/cm² (NP-HW-HEXTILE-001 §4.3; register rows UC-064..066).
const PBM_FULL_SCALE: [(Channel, f64); 3] =
    [(Channel::Led660, 403.0), (Channel::Led808, 403.0), (Channel::Led1064, 28.0)];
const INTRANASAL_FULL_SCALE: f64 = 100.0;
const AUDIO_DB_AT_ZERO: f64 = 40.0;
const AUDIO_DB_PER_PERCENT: f64 = 0.5;
const EPS: f64 = 1e-9;
const CLINICAL_TACS_MAX_CHANNELS: f64 = 21.0;

pub type CompileError = String;
type R<T> = Result<T, CompileError>;

pub struct CompileOptions<'a> {
    pub device_serial: Option<&'a [u8]>,
    /// Zone name → its 1-based socket ids.
    pub zones: Option<&'a HashMap<String, Vec<u32>>>,
    pub clinician_sockets: Option<&'a [u32]>,
    pub wavelength_rules: Option<&'a WavelengthRules>,
    /// Mode 3: sets NP_PROTO_FLAG_AUTONOMOUS (NP-FW-HUB-001 §4.1). No firmware reader yet.
    pub autonomous: bool,
    pub now_unix: u32,
    pub session_uuid: [u8; 16],
}

pub struct Compiled {
    /// Header, commands and a zeroed 64-byte signature slot.
    pub blob: Vec<u8>,
    pub is_t2: bool,
    pub cmd_count: usize,
}

#[derive(Clone, PartialEq)]
enum Target {
    Slot(u32),
    Sockets(Vec<u32>),
}

#[derive(Clone)]
struct Cmd {
    block: usize,
    mod_type: u8,
    target: Target,
    start_ms: f64,
    duration_ms: f64,
    params: Vec<u8>,
}

struct Encoded {
    mod_type: u8,
    target: Target,
    params: Vec<u8>,
}

fn f(v: &Value, k: &str) -> f64 {
    v[k].as_f64().unwrap_or(f64::NAN)
}
fn s<'a>(v: &'a Value, k: &str) -> &'a str {
    v[k].as_str().unwrap_or("")
}
fn b(v: &Value, k: &str) -> bool {
    v[k].as_bool().unwrap_or(false)
}

fn freq_code(hz: f64) -> u8 {
    if hz <= 0.0 { 0 } else { to_u8(js_round(hz)) }
}

fn duty_reg(hz: f64, pct: f64) -> R<u8> {
    if hz <= 0.0 && pct != 100.0 {
        return Err(format!(
            "frequency 0 selects continuous wave, which has no duty cycle, but the block's duty is {}%. Set the duty to 100 % for CW, or give a pulse frequency above 0.",
            num_string(pct)
        ));
    }
    Ok(to_u8(js_round(pct * 2.0).min(f64::from(0x32u8))))
}

fn irradiance_to_register(irr: f64, full_scale: f64, channel: &str) -> R<u8> {
    if !irr.is_finite() || irr <= 0.0 {
        return Err(format!("PBM irradiance must be positive, got {} mW/cm².", num_string(irr)));
    }
    if irr > full_scale * (1.0 + EPS) {
        return Err(format!(
            "PBM irradiance {} mW/cm² exceeds what {channel} delivers at full drive ({} mW/cm²). The protocol is refused, not reduced to fit.",
            num_string(irr),
            num_string(full_scale)
        ));
    }
    Ok(js_round(irr / full_scale * 255.0).clamp(1.0, 255.0) as u8)
}

fn db_to_volume_percent(db: f64) -> R<u8> {
    let pct = (db - AUDIO_DB_AT_ZERO) / AUDIO_DB_PER_PERCENT;
    if !db.is_finite() || pct < -EPS || pct > 100.0 + EPS {
        return Err(format!(
            "Audio level {} dB SPL is outside the range the drive can deliver ({}–{} dB SPL). The protocol is refused, not reduced to fit.",
            num_string(db),
            num_string(AUDIO_DB_AT_ZERO),
            num_string(AUDIO_DB_AT_ZERO + 100.0 * AUDIO_DB_PER_PERCENT)
        ));
    }
    Ok(js_round(pct).clamp(0.0, 100.0) as u8)
}

fn put16(buf: &mut [u8], at: usize, v: f64) {
    buf[at..at + 2].copy_from_slice(&to_u16(v).to_le_bytes());
}

// ── targets ──────────────────────────────────────────────────────────────────

struct SerializedTarget {
    kind: u8,
    slot_id: u8,
    block: Vec<u8>,
}

fn serialize_target(t: &Target) -> R<SerializedTarget> {
    match t {
        Target::Slot(slot) => {
            if *slot < SLOT_FIRST_VALID || *slot >= SLOT_MAX {
                return Err(format!(
                    "Slot {slot} is not addressable: slots below {SLOT_FIRST_VALID} are the retired zone-module slots (cranial targets use sockets), and {SLOT_MAX} is the end of the slot domain."
                ));
            }
            Ok(SerializedTarget { kind: TARGET_SLOT, slot_id: *slot as u8, block: vec![] })
        }
        Target::Sockets(sockets) => {
            if sockets.is_empty() {
                return Err("Command targets no sockets — resolve the zone to at least one socket.".into());
            }
            let mut mask = vec![0u8; SOCKET_MASK_BYTES];
            for &id in sockets {
                if id < 1 || id > SOCKET_COUNT {
                    return Err(format!(
                        "Socket {id} is not a socket on this helmet (valid ids are 1–{SOCKET_COUNT}). Check the zone's socket list."
                    ));
                }
                let bit = (id - 1) as usize;
                mask[bit >> 3] |= 1 << (bit & 7);
            }
            Ok(SerializedTarget { kind: TARGET_SOCKET_MASK, slot_id: SLOT_NONE, block: mask })
        }
    }
}

// ── public entry ─────────────────────────────────────────────────────────────

pub fn compile_protocol(proto: &Value, opts: &CompileOptions) -> R<Compiled> {
    let timing = &proto["timingMode"];
    let session_ms = if timing["type"] == "duration" { f(timing, "seconds") * 1000.0 } else { 0.0 };

    let mut cmds: Vec<Cmd> = Vec::new();
    let mut is_t2 = false;
    let empty = Vec::new();
    let mods = proto["modalities"].as_array().unwrap_or(&empty);
    for (block, m) in mods.iter().enumerate() {
        if m["enabled"] == false {
            continue;
        }
        let ty = s(m, "type");
        if matches!(ty, "qeeg_21ch" | "tms" | "pbm_deep_1170nm" | "clinical_tacs" | "hd_tdcs" | "cervical_vns") {
            is_t2 = true;
        }
        let generated = build_commands(ty, &m["params"], &m["interval"], session_ms, opts)?;
        for mut c in generated {
            c.block = block;
            cmds.push(c);
            if cmds.len() > CMD_MAX {
                return Err(format!(
                    "Protocol exceeds maximum command count ({CMD_MAX}). Reduce interval repeats or the number of enabled modalities."
                ));
            }
        }
    }

    merge_overlapping_pbm(&mut cmds, session_ms)?;

    if cmds.is_empty() {
        return Err("Protocol produces no commands — no enabled modalities".into());
    }

    cmds.sort_by(|a, b| a.start_ms.partial_cmp(&b.start_ms).unwrap_or(std::cmp::Ordering::Equal));

    let targets: Vec<SerializedTarget> = cmds.iter().map(|c| serialize_target(&c.target)).collect::<R<_>>()?;
    let payload: usize =
        cmds.iter().zip(&targets).map(|(c, t)| CMD_HDR_LEN + t.block.len() + c.params.len()).sum();
    let mut blob = vec![0u8; HEADER_LEN + payload + SIG_LEN];

    let mut flags = 0u8;
    if is_t2 {
        flags |= FLAG_T2_TIER;
    }
    if opts.autonomous {
        flags |= FLAG_AUTONOMOUS;
    }
    blob[0..4].copy_from_slice(&MAGIC.to_le_bytes());
    blob[4..6].copy_from_slice(&VERSION.to_le_bytes());
    blob[6] = flags;
    blob[7] = cmds.len() as u8;
    blob[8..24].copy_from_slice(&opts.session_uuid);
    blob[24..28].copy_from_slice(&opts.now_unix.to_le_bytes());
    if let Some(serial) = opts.device_serial {
        let n = serial.len().min(32);
        blob[28..28 + n].copy_from_slice(&serial[..n]);
    }
    blob[60..64].copy_from_slice(&to_u32(session_ms).to_le_bytes());

    let mut off = HEADER_LEN;
    for (c, t) in cmds.iter().zip(&targets) {
        blob[off] = c.mod_type;
        blob[off + 1] = t.slot_id;
        blob[off + 2..off + 6].copy_from_slice(&to_u32(c.start_ms).to_le_bytes());
        blob[off + 6..off + 10].copy_from_slice(&to_u32(c.duration_ms).to_le_bytes());
        blob[off + 10..off + 12].copy_from_slice(&(c.params.len() as u16).to_le_bytes());
        blob[off + 12] = t.kind;
        blob[off + 13] = t.block.len() as u8;
        off += CMD_HDR_LEN;
        blob[off..off + t.block.len()].copy_from_slice(&t.block);
        off += t.block.len();
        blob[off..off + c.params.len()].copy_from_slice(&c.params);
        off += c.params.len();
    }
    Ok(Compiled { blob, is_t2, cmd_count: cmds.len() })
}

// ── parallel PBM blocks on one tile ──────────────────────────────────────────

fn cur_offsets(mod_type: u8) -> &'static [usize] {
    match mod_type {
        MOD_PBM_BASE => &[2, 3],
        MOD_PBM_SMART => &[2, 3, 4],
        _ => &[3, 4],
    }
}

fn timing_offsets(mod_type: u8) -> (usize, usize) {
    if mod_type == MOD_INTRANASAL { (1, 2) } else { (0, 1) }
}

fn is_pbm_on(c: &Cmd) -> bool {
    matches!(c.mod_type, MOD_PBM_BASE | MOD_PBM_SMART | MOD_INTRANASAL) && !c.params.is_empty()
}

fn sockets_of(c: &Cmd) -> Vec<u32> {
    match &c.target {
        Target::Sockets(s) => s.clone(),
        Target::Slot(slot) => vec![10_000 + slot],
    }
}

fn merge_overlapping_pbm(cmds: &mut Vec<Cmd>, session_ms: f64) -> R<()> {
    let end_of = |c: &Cmd| -> f64 {
        if c.duration_ms > 0.0 {
            c.start_ms + c.duration_ms
        } else if session_ms > 0.0 {
            session_ms
        } else {
            f64::INFINITY
        }
    };
    let mut drop = vec![false; cmds.len()];
    for i in 0..cmds.len() {
        if !is_pbm_on(&cmds[i]) || drop[i] {
            continue;
        }
        for j in (i + 1)..cmds.len() {
            if !is_pbm_on(&cmds[j]) || drop[j] || cmds[i].block == cmds[j].block {
                continue;
            }
            let sa_vec = sockets_of(&cmds[i]);
            let sa: std::collections::HashSet<u32> = sa_vec.iter().copied().collect();
            let sb = sockets_of(&cmds[j]);
            let shared: Vec<u32> = sb.iter().copied().filter(|x| sa.contains(x)).collect();
            let overlap = cmds[i].start_ms < end_of(&cmds[j]) && cmds[j].start_ms < end_of(&cmds[i]);
            if shared.is_empty() || !overlap {
                continue;
            }
            let (a, bb) = (&cmds[i], &cmds[j]);
            let same_sockets = shared.len() == sa.len() && sb.len() == sa.len();
            let same_window = a.start_ms == bb.start_ms && a.duration_ms == bb.duration_ms;
            let (fo, dofs) = timing_offsets(a.mod_type);
            let same_timing = a.mod_type == bb.mod_type
                && a.params.get(fo) == bb.params.get(fo)
                && a.params.get(dofs) == bb.params.get(dofs);
            let curs = cur_offsets(a.mod_type);
            let disjoint = a.mod_type == bb.mod_type
                && curs.iter().all(|&k| a.params.get(k) == Some(&0) || bb.params.get(k) == Some(&0));
            if !(same_sockets && same_window && same_timing && disjoint) {
                let plural = if shared.len() == 1 { "" } else { "s" };
                let list = shared.iter().take(6).map(u32::to_string).collect::<Vec<_>>().join(", ");
                let more = if shared.len() > 6 { ", …" } else { "" };
                return Err(format!(
                    "Two PBM blocks overlap on socket{plural} {list}{more} from {}s. A tile takes one frequency, one duty and one schedule for all its channels, so blocks sharing a tile must match in timing, frequency and duty and drive different wavelengths. The protocol is refused rather than reshaped.",
                    num_string(a.start_ms.max(bb.start_ms) / 1000.0)
                ));
            }
            let mut merged = a.params.clone();
            for &k in curs {
                merged[k] = if a.params[k] != 0 { a.params[k] } else { bb.params[k] };
            }
            if a.mod_type == MOD_PBM_SMART {
                merged[5] = a.params[5] | bb.params[5];
            }
            let (bblock, bmod, bend) = (bb.block, bb.mod_type, end_of(bb));
            cmds[i].params = merged;
            drop[j] = true;
            if let Some(k) = cmds.iter().position(|c| {
                c.block == bblock && c.params.is_empty() && c.mod_type == bmod && c.start_ms == bend
            }) {
                drop[k] = true;
            }
        }
    }
    let mut k = cmds.len();
    while k > 0 {
        k -= 1;
        if drop[k] {
            cmds.remove(k);
        }
    }
    Ok(())
}

// ── command generation ───────────────────────────────────────────────────────

fn build_commands(ty: &str, p: &Value, interval: &Value, session_ms: f64, opts: &CompileOptions) -> R<Vec<Cmd>> {
    let enc = encode(ty, p, opts)?;
    let offset_ms = js_round(interval["startOffsetSeconds"].as_f64().unwrap_or(0.0) * 1000.0);
    if session_ms > 0.0 && offset_ms >= session_ms {
        return Err(format!(
            "A block starts at {}s, at or after the session's end ({}s). It would never run; remove it or lengthen the session.",
            num_string(offset_ms / 1000.0),
            num_string(session_ms / 1000.0)
        ));
    }
    let mk = |start: f64, dur: f64, params: Vec<u8>| Cmd {
        block: 0,
        mod_type: enc.mod_type,
        target: enc.target.clone(),
        start_ms: start,
        duration_ms: dur,
        params,
    };
    let on_s = f(interval, "intervalOnSeconds");
    if on_s == 0.0 {
        return Ok(vec![mk(offset_ms, 0.0, enc.params.clone())]);
    }
    let on_ms = on_s * 1000.0;
    let off_ms = f(interval, "intervalOffSeconds") * 1000.0;
    let period = on_ms + off_ms;
    let max_repeats = match interval["repeatCount"].as_f64() {
        Some(r) => r,
        None if session_ms > 0.0 => (session_ms / period).ceil(),
        None => 1.0,
    };
    let mut out = Vec::new();
    let mut i = 0.0;
    while i < max_repeats {
        let start = offset_ms + i * period;
        if session_ms > 0.0 && start >= session_ms {
            break;
        }
        out.push(mk(start, on_ms, enc.params.clone()));
        let stop = start + on_ms;
        if session_ms == 0.0 || stop < session_ms {
            out.push(mk(stop, 0.0, vec![]));
        }
        if out.len() > CMD_MAX + 2 {
            // The caller refuses past CMD_MAX; stop expanding so an unbounded repeat cannot run away.
            break;
        }
        i += 1.0;
    }
    Ok(out)
}

// ── parameter encoding ───────────────────────────────────────────────────────

fn slot(mod_type: u8, slot: u32, params: Vec<u8>) -> Encoded {
    Encoded { mod_type, target: Target::Slot(slot), params }
}

fn encode(ty: &str, p: &Value, opts: &CompileOptions) -> R<Encoded> {
    match ty {
        "pbm_transcranial" => encode_pbm_transcranial(p, opts),
        "pbm_intranasal" => encode_pbm_intranasal(p, opts),
        "eeg_neurofeedback" => Ok(encode_eeg(p)),
        "bes_tacs" => Ok(encode_bes(p)),
        "tdcs" => Ok(encode_tdcs(p)),
        "vns_hrv" => Ok(encode_vns(p)),
        "audio_entrainment" => encode_audio(p),
        "visual_stimulation" => Ok(encode_visual(p)),
        "qeeg_21ch" => Ok(encode_qeeg(p)),
        "tms" => Ok(encode_tms(p)),
        "pbm_deep_1170nm" => encode_deep(p),
        "clinical_tacs" => Ok(encode_clinical_tacs(p)),
        "hd_tdcs" => Ok(encode_hd_tdcs(p)),
        "cervical_vns" => Ok(encode_cvns(p)),
        "vibrotactile_40hz" => Ok(encode_vibro(p)),
        other => Err(format!("Unknown modality type in hub compiler: {other}")),
    }
}

fn resolve_pbm_sockets(p: &Value, opts: &CompileOptions) -> R<Vec<u32>> {
    match s(p, "zones") {
        "named" => {
            let empty = Vec::new();
            let refs: Vec<&str> =
                p["zoneRefs"].as_array().unwrap_or(&empty).iter().filter_map(Value::as_str).collect();
            if refs.is_empty() {
                return Err("PBM transcranial: zones is 'named' but no zoneRefs were given.".into());
            }
            let Some(zones) = opts.zones else {
                return Err(format!(
                    "PBM transcranial targets named zones ({}) but no zone namespace was passed to compileProtocol — pass NPNamespace.zones as opts.zones.",
                    refs.join(", ")
                ));
            };
            let mut sockets = Vec::new();
            for r in refs {
                let Some(z) = zones.get(r) else {
                    return Err(format!("PBM transcranial references zone \"{r}\", which is not in the zone namespace."));
                };
                sockets.extend(z);
            }
            Ok(sockets)
        }
        "clinician_selected" => match opts.clinician_sockets {
            Some(c) if !c.is_empty() => Ok(c.to_vec()),
            _ => Err("PBM transcranial target is 'clinician_selected': the operator must choose the sockets before this protocol can run. Pass them as opts.clinicianSockets.".into()),
        },
        other => Err(format!("PBM transcranial has an unhandled zone target: {other}")),
    }
}

fn encode_pbm_transcranial(p: &Value, opts: &CompileOptions) -> R<Encoded> {
    let target = Target::Sockets(resolve_pbm_sockets(p, opts)?);
    let hz = f(p, "frequencyHz");
    let duty = duty_reg(hz, f(p, "dutyCyclePercent"))?;
    let fc = freq_code(hz);
    let default_rules = WavelengthRules::default_rules();
    let rules = opts.wavelength_rules.unwrap_or(&default_rules);
    let ch = resolve_one_channel(s(p, "wavelength"), rules, &Channel::ALL, "PBM")?;
    let full = PBM_FULL_SCALE.iter().find(|(c, _)| *c == ch).map(|(_, v)| *v).unwrap();
    let cur = irradiance_to_register(f(p, "irradianceMWcm2"), full, ch.name())?;
    Ok(match ch {
        Channel::Led660 => Encoded { mod_type: MOD_PBM_BASE, target, params: vec![fc, duty, cur, 0] },
        Channel::Led808 => Encoded { mod_type: MOD_PBM_BASE, target, params: vec![fc, duty, 0, cur] },
        Channel::Led1064 => Encoded { mod_type: MOD_PBM_SMART, target, params: vec![fc, duty, 0, 0, cur, 0x04] },
    })
}

fn encode_pbm_intranasal(p: &Value, opts: &CompileOptions) -> R<Encoded> {
    let default_rules = WavelengthRules::default_rules();
    let rules = opts.wavelength_rules.unwrap_or(&default_rules);
    let ch = resolve_one_channel(s(p, "wavelength"), rules, &[Channel::Led660, Channel::Led808], "Intranasal PBM")?;
    let cur = irradiance_to_register(f(p, "irradianceMWcm2"), INTRANASAL_FULL_SCALE, "the intranasal probe")?;
    let hz = f(p, "frequencyHz");
    let duty = duty_reg(hz, f(p, "dutyCyclePercent"))?;
    let fc = freq_code(hz);
    Ok(slot(
        MOD_INTRANASAL,
        SLOT_INTRANASAL,
        vec![0x00, fc, duty, if ch == Channel::Led660 { cur } else { 0 }, if ch == Channel::Led808 { cur } else { 0 }],
    ))
}

fn encode_eeg(p: &Value) -> Encoded {
    let mut mask: u32 = match s(p, "channels") {
        "front" => 0x03,
        "central" => 0x3C,
        _ => 0xFF,
    };
    if s(p, "channels") == "custom" {
        if let Some(list) = p["customChannels"].as_array() {
            let labels = ["Fp1", "Fp2", "F3", "F4", "C3", "C4", "P3", "P4"];
            mask = list.iter().fold(0u32, |m, ch| {
                match labels.iter().position(|l| Some(*l) == ch.as_str()) {
                    Some(i) => m | (1 << i),
                    None => m,
                }
            });
        }
    }
    let adaptive = if b(p, "closedLoopEnabled") { 0x03 } else { 0x00 };
    slot(MOD_EEG, SLOT_EEG, vec![mask as u8, 6, 2, 0, adaptive])
}

fn encode_bes(p: &Value) -> Encoded {
    let freq_mhz = js_round(f(p, "frequencyHz") * 1000.0).min(65535.0);
    let amp_ua = js_round(f(p, "intensityMilliamps") * 1000.0).min(1000.0);
    let wf = if s(p, "waveform") == "square" { 1 } else { 0 };
    let mut buf = vec![0u8; 7];
    put16(&mut buf, 1, freq_mhz);
    put16(&mut buf, 3, amp_ua);
    buf[5] = wf;
    slot(MOD_BES_TACS, SLOT_BES_TACS, buf)
}

fn resolve_electrode_pair(p: &Value) -> u8 {
    let pair = p["electrodePairs"].get(0).and_then(Value::as_array);
    let Some(pair) = pair else { return 0 };
    let a = pair.first().and_then(Value::as_str).unwrap_or("").to_uppercase();
    let bb = pair.get(1).and_then(Value::as_str).unwrap_or("").to_uppercase();
    let is = |x: &str, y: &str| (a == x && bb == y) || (a == y && bb == x);
    if is("F3", "F4") {
        0
    } else if is("P3", "P4") {
        1
    } else if is("FZ", "PZ") {
        2
    } else {
        0
    }
}

fn encode_tdcs(p: &Value) -> Encoded {
    let cur_ua = js_round(f(p, "intensityMilliamps") * 1000.0).min(2000.0);
    let ramp = f(p, "rampSeconds").max(30.0);
    let area = (f(p, "electrodeAreaCm2") * 1000.0).floor().min(65535.0);
    let mut buf = vec![0u8; 8];
    buf[0] = resolve_electrode_pair(p);
    put16(&mut buf, 1, cur_ua);
    buf[3] = 0;
    put16(&mut buf, 4, ramp);
    put16(&mut buf, 6, area);
    slot(MOD_TDCS, SLOT_TDCS, buf)
}

fn encode_vns(p: &Value) -> Encoded {
    let freq_mhz = js_round(f(p, "frequencyHz") * 1000.0).min(25000.0);
    let amp_ua = js_round(f(p, "intensityMilliamps") * 1000.0).min(2000.0);
    let proto = match s(p, "hrvProtocol") {
        "combined_pbm" => 1,
        "tavns_sync" => 2,
        "eeg_biofeedback" => 3,
        _ => 0,
    };
    let mut buf = vec![0u8; 9];
    put16(&mut buf, 1, freq_mhz);
    put16(&mut buf, 3, amp_ua);
    buf[5] = 0;
    buf[6] = 1;
    buf[7] = 1;
    buf[8] = proto;
    slot(MOD_VNS_HRV, SLOT_VNS_HRV, buf)
}

fn encode_audio(p: &Value) -> R<Encoded> {
    let mut mode = 4u8;
    let mut beat_mhz = 0.0;
    if let Some(bb) = p["binauralBeatsHz"].as_f64() {
        mode = 0;
        beat_mhz = js_round(bb * 1000.0);
    } else if let Some(iso) = p["isochronicTonesHz"].as_f64() {
        mode = 1;
        beat_mhz = js_round(iso * 1000.0);
    } else if s(p, "noiseType") == "pink" {
        mode = 2;
    } else if s(p, "noiseType") == "brown" {
        mode = 3;
    }
    let vol = db_to_volume_percent(f(p, "volumeDb"))?;
    let mut buf = vec![0u8; 8];
    buf[0] = mode;
    put16(&mut buf, 1, f(p, "carrierHz").min(65535.0));
    put16(&mut buf, 3, beat_mhz.min(65535.0));
    buf[5] = vol;
    buf[6] = b(p, "boneConductionPacer") as u8;
    buf[7] = b(p, "eegAdaptive") as u8;
    Ok(slot(MOD_AUDIO, SLOT_AUDIO, buf))
}

fn encode_visual(p: &Value) -> Encoded {
    let mode = match s(p, "mode") {
        "binocular" => Some(0u8),
        "emdr" => Some(1),
        "retinal_pbm" => Some(2),
        "mode_f" => Some(3),
        _ => None,
    };
    let freq = js_round(f(p, "frequencyHz")).min(100.0);
    let emdr = js_round(f(p, "emdrCadenceHz") * 10.0).min(255.0);
    let shade = (mode == Some(0)) as u8;
    slot(
        MOD_VISUAL,
        SLOT_VISUAL,
        vec![mode.unwrap_or(0), to_u8(freq), 50, 0x02, 0xFF, 0x0F, to_u8(emdr), b(p, "enableModeF") as u8, shade],
    )
}

fn encode_qeeg(p: &Value) -> Encoded {
    let reference = match s(p, "reference") {
        "cz" => 1,
        "average" => 2,
        _ => 0,
    };
    let mut buf = vec![0u8; 8];
    buf[0..4].copy_from_slice(&0x1F_FFFFu32.to_le_bytes());
    buf[4] = 6;
    buf[5] = 2;
    buf[6] = reference;
    buf[7] = b(p, "sloretaEnabled") as u8;
    slot(MOD_QEEG_21CH, SLOT_QEEG, buf)
}

fn target_index(t: &str) -> u8 {
    match t {
        "DLPFC_L" => 0,
        "DLPFC_R" => 1,
        "VLPFC_L" => 2,
        "ACC" => 3,
        "MPFC" => 4,
        "M1_L" => 5,
        "M1_R" => 6,
        _ => 0,
    }
}

fn encode_tms(p: &Value) -> Encoded {
    let proto = match s(p, "tmsProtocol") {
        "TBS" => 1,
        "iTBS" => 2,
        _ => 0,
    };
    let hz = f(p, "frequencyHz");
    let freq_mhz = js_round(hz * 1000.0).min(65535.0);
    let pulses = f(p, "pulseCount").min(65535.0);
    let tbs = s(p, "tmsProtocol") != "rTMS";
    let inter = if tbs { 200.0 } else { js_round(1000.0 / hz.max(0.1)) };
    let mut buf = vec![0u8; 10];
    buf[0] = proto;
    buf[1] = target_index(s(p, "target"));
    put16(&mut buf, 2, freq_mhz);
    buf[4] = to_u8(js_round(f(p, "intensityPercentMT")).min(255.0));
    put16(&mut buf, 5, pulses);
    put16(&mut buf, 7, inter.min(65535.0));
    buf[9] = if tbs { 50 } else { 0 };
    slot(MOD_TMS, SLOT_TMS, buf)
}

fn encode_deep(p: &Value) -> R<Encoded> {
    let intensity = js_round(f(p, "intensityMWcm2")).min(1000.0);
    let hz = f(p, "frequencyHz");
    let duty = duty_reg(hz, f(p, "dutyCyclePercent"))?;
    let fc = freq_code(hz);
    let mut buf = vec![0u8; 5];
    put16(&mut buf, 0, intensity);
    buf[2] = fc;
    buf[3] = duty;
    Ok(slot(MOD_PBM_1170NM, SLOT_PBM_1170NM, buf))
}

fn encode_clinical_tacs(p: &Value) -> Encoded {
    let freq_mhz = js_round(f(p, "frequencyHz") * 1000.0).min(65535.0);
    let amp_ua = js_round(f(p, "intensityMilliamps") * 1000.0).min(4000.0);
    let wf = match s(p, "waveform") {
        "sinusoidal" => 0,
        "square" => 1,
        _ => 2,
    };
    let n = js_round(f(p, "channelCount")).max(0.0).min(CLINICAL_TACS_MAX_CHANNELS);
    let mask: u32 = if n == 0.0 { 0 } else { (1u64 << (n as u64)) as u32 - 1 };
    let mut buf = vec![0u8; 8];
    put16(&mut buf, 0, freq_mhz);
    put16(&mut buf, 2, amp_ua);
    buf[4] = (mask & 0xFF) as u8;
    buf[5] = ((mask >> 8) & 0xFF) as u8;
    buf[6] = ((mask >> 16) & 0x1F) as u8;
    buf[7] = wf;
    slot(MOD_CLIN_TACS, SLOT_CLIN_TACS, buf)
}

fn encode_hd_tdcs(p: &Value) -> Encoded {
    let montage = match s(p, "montage") {
        "bilateral_4x1" => 1,
        "standard_2_electrode" => 2,
        _ => 0,
    };
    let cur = js_round(f(p, "intensityMilliamps") * 1000.0).min(2000.0);
    let mut buf = vec![0u8; 6];
    buf[0] = target_index(s(p, "target"));
    buf[1] = montage;
    put16(&mut buf, 2, cur);
    put16(&mut buf, 4, 30.0);
    slot(MOD_HD_TDCS, SLOT_HD_TDCS, buf)
}

fn encode_cvns(p: &Value) -> Encoded {
    let freq_mhz = js_round(f(p, "frequencyHz") * 1000.0).min(25000.0);
    let amp_ua = js_round(f(p, "intensityMilliamps") * 1000.0).min(2000.0);
    let mut buf = vec![0u8; 10];
    put16(&mut buf, 1, freq_mhz);
    put16(&mut buf, 3, amp_ua);
    put16(&mut buf, 5, 0.0);
    put16(&mut buf, 7, 10.0);
    buf[9] = 1;
    slot(MOD_CVNS, SLOT_CVNS, buf)
}

fn encode_vibro(p: &Value) -> Encoded {
    let clamped = f(p, "intensityG").min(1.2).max(0.6);
    let gain = js_round((clamped - 0.6) / 0.6 * f64::from(0x7Fu8));
    let sync = (b(p, "syncToAudio") as u8) | ((b(p, "syncToVisual") as u8) << 1);
    let mut buf = vec![0u8; 4];
    buf[0] = to_u8(gain);
    buf[1] = sync;
    put16(&mut buf, 2, 40000.0);
    slot(MOD_VIBROTACTILE, SLOT_VIBROTACTILE, buf)
}
