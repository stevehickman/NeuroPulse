//! The hub session-descriptor compiler (NP-FW-HUB-001 §4): a port of
//! app/web/src/lib/hubCompiler.ts. The wire format has one writer now. The caller supplies
//! what is not deterministic (the clock, the session UUID) and signs the raw region the
//! blob ends with a zeroed 64-byte slot for.

use crate::constants::hardware_limits as hw;
use crate::constants::socket_lattice::{NP_SOCKET_ID_MAX, NP_SOCKET_ID_MIN};
use crate::js::{js_round, num_string};
use crate::wavelength::{resolve_one_channel, Channel, WavelengthRules};
use serde_json::Value;
use std::collections::HashMap;

pub const SIG_LEN: usize = 64;
const MAGIC: u32 = 0x4E50_4850;
const VERSION: u16 = 3;
/// 68 bytes of fields, then the 39-byte rules stamp (`SWR-HV-03`, NP-FW-HUB-001 §4.2), under the signature.
const HEADER_LEN: usize = 107;
const STAMP_OFF: usize = 68;
const CMD_HDR_LEN: usize = 14;
const CMD_MAX: usize = 64;
const SOCKET_MASK_BYTES: usize = 16;
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

/// Full-scale irradiance per channel, mW/cm² (NP-HW-HEXTILE-001 §4.3; register rows UC-064, UC-065). The figures
/// are `common/npps/constants.json`'s, shared with the web's `pbmDrive.ts` (OI-NPPS-ABS-02): this file holds none.
const PBM_FULL_SCALE: [(Channel, f64); 3] = [
    (Channel::Led660, hw::PBM_FULL_SCALE_660_MW_CM2),
    (Channel::Led808, hw::PBM_FULL_SCALE_808_MW_CM2),
    (Channel::Led1064, hw::PBM_FULL_SCALE_1064_MW_CM2),
];
const INTRANASAL_FULL_SCALE: f64 = hw::INTRANASAL_FULL_SCALE_MW_CM2;
/// Acoustic output ceiling, dB SPL at the ear: absolute, not a fraction of any driver's range
/// (NP-HW-AUDIO-001 REQ-AUDIO-19). Equals `NP_AUDIO_MAX_LEVEL_HALF_DB` / 2 in `np_hub_config.h`.
const AUDIO_MAX_DB_SPL: f64 = hw::AUDIO_MAX_DB_SPL;
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
    /// The hardware version of the helmet the descriptor is compiled for (`SWR-HV-08`), 1 to 15. It is written into the
    /// header, inside the signed region, and the hub compares it with its own at load.
    pub hardware_version: u8,
    /// The ids of the cautions the author has acknowledged (`zones::Block::ack_id`). A protocol in the caution zone
    /// compiles only when every one of its cautions is here; one in the danger zone never compiles.
    pub acknowledged_cautions: &'a [String],
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

/// A parameter the descriptor cannot encode without. A missing or non-finite one is refused: `f64::min`/`max` return the
/// other operand for NaN, so a missing intensity clamped to its ceiling would otherwise compile to the ceiling.
fn required(v: &Value, k: &str, what: &str) -> R<f64> {
    let x = f(v, k);
    if x.is_finite() {
        Ok(x)
    } else {
        Err(format!("{what}: {k} is required and must be a finite number. The protocol is refused, not given a default."))
    }
}

/// `value × scale`, rounded, as a register that must not exceed `max`. Above it the protocol is refused, never clamped
/// (CLAUDE.md §3: a ceiling refuses a protocol, it does not reshape one).
fn scaled(v: &Value, k: &str, what: &str, scale: f64, max: f64, unit: &str) -> R<f64> {
    let x = required(v, k, what)?;
    let r = js_round(x * scale);
    if x < 0.0 || r > max {
        return Err(format!(
            "{what}: {k} {} {unit} is outside what the drive can deliver (0–{} {unit}). The protocol is refused, not reduced to fit.",
            num_string(x),
            num_string(max / scale)
        ));
    }
    below_one_step(x, r, k, what, 1.0 / scale, unit)?;
    Ok(r)
}

/// As [`scaled`], for a bound that is the width of the wire field and not a limit of the drive: `value × scale`, rounded,
/// must fit `0..=max`. A value the field cannot carry is refused, never capped at the field's largest value and never
/// wrapped modulo its width (REQ-NPPS-33).
fn carried(v: &Value, k: &str, what: &str, scale: f64, max: f64, unit: &str) -> R<f64> {
    let x = required(v, k, what)?;
    let r = js_round(x * scale);
    if x < 0.0 || r > max {
        return Err(format!(
            "{what}: {k} {} {unit} is outside what the descriptor can carry (0–{} {unit}). The protocol is refused, not reduced or wrapped to fit.",
            num_string(x),
            num_string(max / scale)
        ));
    }
    below_one_step(x, r, k, what, 1.0 / scale, unit)?;
    Ok(r)
}

/// A value above zero whose register would be 0. Zero on the wire means something else (off, continuous wave, a firmware
/// default), so the value is refused: raising it to the first step, or dropping it to zero, would each deliver a stimulus
/// nobody authored (REQ-NPPS-33 (c)). Rounding to the nearest step above that is the register's resolution, not a clamp.
fn below_one_step(x: f64, register: f64, k: &str, what: &str, step: f64, unit: &str) -> R<()> {
    if x > 0.0 && register < 1.0 {
        return Err(format!(
            "{what}: {k} {} {unit} is below the smallest step the descriptor can carry ({} {unit}). The protocol is refused, not raised to that step or dropped to zero.",
            num_string(x),
            num_string(step)
        ));
    }
    Ok(())
}

/// The PBM pulse-frequency code: whole Hz in one byte, 0 for continuous wave. A missing or negative frequency used to
/// become continuous wave and one above 255 Hz wrapped (300 Hz became 44); each is refused (REQ-NPPS-33).
fn freq_code(hz: f64, what: &str) -> R<u8> {
    if !hz.is_finite() {
        return Err(format!("{what}: frequencyHz is required and must be a finite number. The protocol is refused, not given a default."));
    }
    let r = js_round(hz);
    if hz < 0.0 || r > 255.0 {
        return Err(format!(
            "{what}: frequencyHz {} Hz is outside what the descriptor can carry (0 for continuous wave, or 1–255 Hz). The protocol is refused, not reduced or wrapped to fit.",
            num_string(hz)
        ));
    }
    if hz > 0.0 && r < 1.0 {
        return Err(format!(
            "{what}: frequencyHz {} Hz would be encoded as 0, which is continuous wave. The smallest pulse frequency the descriptor can carry is 1 Hz. The protocol is refused, not run as continuous wave.",
            num_string(hz)
        ));
    }
    Ok(r as u8)
}

/// The PBM duty register, 0.5 % per step. A duty above `PBM_DUTY_CYCLE_MAX_PERCENT` (25 %) is refused, as the validator
/// and the hub refuse it (REQ-FWHUB-78 (d), (e)); it used to be encoded as 25 %. Continuous wave is 100 % duty and is
/// refused with it. INTERIM: the limit goes with the pre-signing irradiance check of OI-HEXTILE-31, and not before.
fn duty_reg(hz: f64, pct: f64, what: &str) -> R<u8> {
    if !pct.is_finite() {
        return Err(format!("{what}: dutyCyclePercent is required and must be a finite number. The protocol is refused, not given a default."));
    }
    if hz <= 0.0 && pct != 100.0 {
        return Err(format!(
            "frequency 0 selects continuous wave, which has no duty cycle, but the block's duty is {}%. Set the duty to 100 % for CW, or give a pulse frequency above 0.",
            num_string(pct)
        ));
    }
    if pct > hw::PBM_DUTY_CYCLE_MAX_PERCENT {
        let cw = if hz <= 0.0 { " Continuous wave is 100 % duty, so it is refused until that limit is lifted (OI-HEXTILE-31)." } else { "" };
        return Err(format!(
            "{what}: duty cycle {}% exceeds firmware maximum of {}%.{cw} The protocol is refused, not reduced to fit.",
            num_string(pct),
            num_string(hw::PBM_DUTY_CYCLE_MAX_PERCENT)
        ));
    }
    if pct < 0.0 {
        return Err(format!("{what}: duty cycle {}% is below zero. The protocol is refused, not reshaped.", num_string(pct)));
    }
    let r = js_round(pct * 2.0);
    below_one_step(pct, r, "dutyCyclePercent", what, 0.5, "%")?;
    Ok(r as u8)
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
    // Register 0 is "channel not driven". An irradiance under half a step used to be raised to register 1, which at
    // 403 mW/cm² full scale is 1.58 mW/cm² whatever was authored; it is refused (REQ-NPPS-33 (c)). The upper bound needs
    // no clamp: the check above holds the register to 255.
    let r = js_round(irr / full_scale * 255.0);
    if r < 1.0 {
        return Err(format!(
            "PBM irradiance {} mW/cm² is below the smallest step {channel} delivers ({} mW/cm²). The protocol is refused, not raised to that step.",
            num_string(irr),
            num_string(js_round(full_scale / 255.0 * 100.0) / 100.0)
        ));
    }
    Ok(r as u8)
}

/// dB SPL at the ear → the wire byte, 2 × dB (0.5 dB steps). Refuses a level that is not positive and finite, or
/// above the ceiling; never reduces one to fit.
fn db_to_level_half_db(db: f64) -> R<u8> {
    if !db.is_finite() || db <= 0.0 || db > AUDIO_MAX_DB_SPL + EPS {
        return Err(format!(
            "Audio level {} dB SPL is outside 0–{} dB SPL, the acoustic output ceiling. The protocol is refused, not reduced to fit.",
            num_string(db),
            num_string(AUDIO_MAX_DB_SPL)
        ));
    }
    // Level byte 0 is "output off". A level under 0.25 dB used to be raised to 0.5 dB; it is refused (REQ-NPPS-33 (c)).
    // The upper bound needs no clamp: the check above holds the byte to 2 × the ceiling.
    let r = js_round(db * 2.0);
    if r < 1.0 {
        return Err(format!(
            "Audio level {} dB SPL is below the smallest step the descriptor can carry (0.5 dB). The protocol is refused, not raised to that step.",
            num_string(db)
        ));
    }
    Ok(r as u8)
}

/// `v` as an unsigned wire field of `max`: a whole number in range, or a refusal. The last check before a value is
/// written, behind the per-field checks that name the authored value in its own unit. It replaces the wrapping
/// conversions (`to_u8`, `to_u16`, `to_u32`) this file used to write with, which reduced a value modulo the field's width
/// (REQ-NPPS-33 (b)).
fn uint(v: f64, max: f64, what: &str) -> R<f64> {
    if v.is_finite() && v >= 0.0 && v <= max && v.fract() == 0.0 {
        Ok(v)
    } else {
        Err(format!(
            "{what}: {} is not a whole number in 0–{}, which is what the descriptor field carries. The protocol is refused, not wrapped or cut to fit.",
            num_string(v),
            num_string(max)
        ))
    }
}

fn put16(buf: &mut [u8], at: usize, v: f64, what: &str) -> R<()> {
    buf[at..at + 2].copy_from_slice(&(uint(v, 65535.0, what)? as u16).to_le_bytes());
    Ok(())
}

fn put32(buf: &mut [u8], at: usize, v: f64, what: &str) -> R<()> {
    buf[at..at + 4].copy_from_slice(&(uint(v, f64::from(u32::MAX), what)? as u32).to_le_bytes());
    Ok(())
}

/// A time on the session clock as the descriptor's 32-bit millisecond field. A negative time used to wrap to weeks, one
/// past 2^32 ms wrapped to its remainder, and a time above zero and under 1 ms became 0, which in a command's duration
/// means "until stopped" (REQ-NPPS-33 (b), (c)). Whole milliseconds are the field's resolution.
fn wire_ms(v: f64, what: &str) -> R<u32> {
    if !v.is_finite() || v < 0.0 || v > f64::from(u32::MAX) {
        return Err(format!(
            "{what} {} ms is outside what the descriptor can carry (0–{} ms). The protocol is refused, not wrapped or cut to fit.",
            num_string(v),
            u32::MAX
        ));
    }
    if v > 0.0 && v < 1.0 {
        return Err(format!(
            "{what} {} ms is under the descriptor's 1 ms resolution and would be written as 0. The protocol is refused, not rounded to zero.",
            num_string(v)
        ));
    }
    Ok(v.trunc() as u32)
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
                if i64::from(id) < NP_SOCKET_ID_MIN || i64::from(id) > NP_SOCKET_ID_MAX {
                    return Err(format!(
                        "Socket {id} is not a socket on this helmet (valid ids are {NP_SOCKET_ID_MIN}–{NP_SOCKET_ID_MAX}). Check the zone's socket list."
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

/// The zone gate (`docs/reference/safety-zones.md`): danger is refused, and caution is refused until it is acknowledged.
fn zone_gate(proto: &Value, opts: &CompileOptions) -> R<()> {
    use crate::zones::{evaluate, Zone};
    let mut unacknowledged = Vec::new();
    for b in evaluate(proto) {
        for a in &b.axes {
            match a.zone() {
                Zone::Danger => {
                    return Err(format!(
                        "{} block {}: {} {} {} is in the danger zone (limit {} {}). The protocol is refused.",
                        b.kind, b.index, a.name, num_string(js_round(a.value * 100.0) / 100.0), a.unit,
                        num_string(a.danger.unwrap_or(f64::NAN)), a.unit
                    ));
                }
                Zone::Caution => {
                    let id = b.ack_id(a);
                    if !opts.acknowledged_cautions.contains(&id) {
                        unacknowledged.push(format!(
                            "{} block {}: {} {} {} (caution from {} {}), acknowledge as \"{id}\"",
                            b.kind, b.index, a.name, num_string(js_round(a.value * 100.0) / 100.0), a.unit, num_string(a.caution), a.unit
                        ));
                    }
                }
                Zone::Safe => {}
            }
        }
    }
    if unacknowledged.is_empty() {
        Ok(())
    } else {
        Err(format!(
            "The protocol is in the caution zone and has not been acknowledged: {}. Pass each id in acknowledgedCautions to compile it.",
            unacknowledged.join("; ")
        ))
    }
}

pub fn compile_protocol(proto: &Value, opts: &CompileOptions) -> R<Compiled> {
    // SWR-HV-08 / UC-080: the version is a nibble of the helmet's tier byte, so 1 to 15; 0 is no version. A value
    // outside it is refused, never clamped, because a descriptor stamped for a layout that cannot exist is a mistake.
    if !(1..=15).contains(&opts.hardware_version) {
        return Err(format!(
            "The hardware version {} is not one a helmet can have (1 to 15). Give the version of the helmet the protocol is compiled for.",
            opts.hardware_version
        ));
    }
    let timing = &proto["timingMode"];
    let is_duration = timing["type"] == "duration";
    let session_ms = if is_duration { f(timing, "seconds") * 1000.0 } else { 0.0 };
    // SWR-SS-01: a session is finite and non-zero. A zero (or negative, or missing) duration is refused here
    // as well as in the validator, because this is the last place before the descriptor is signed.
    if is_duration && !(session_ms > 0.0) {
        return Err("Session duration must be greater than zero. Give the protocol a duration, or use interval_count with intervals that end.".into());
    }

    // An interval_count session runs each block's interval cycle `count` times unless the block states its own repeat.
    let default_repeats = if is_duration {
        None
    } else {
        let n = f(timing, "count");
        if !(n >= 1.0) || n.fract() != 0.0 {
            return Err(format!(
                "interval_count must be a whole number of 1 or more, got {}. Give the number of times each interval repeats, or use a duration.",
                num_string(n)
            ));
        }
        Some(n)
    };

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
        let generated = build_commands(ty, &m["params"], &m["interval"], session_ms, default_repeats, opts)?;
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

    // A parameter the descriptor cannot encode is refused above with its own message; the zone gate then judges the dose.
    zone_gate(proto, opts)?;

    merge_overlapping_pbm(&mut cmds, session_ms)?;

    if cmds.is_empty() {
        return Err("Protocol produces no commands — no enabled modalities".into());
    }

    cmds.sort_by(|a, b| a.start_ms.partial_cmp(&b.start_ms).unwrap_or(std::cmp::Ordering::Equal));

    // The length the descriptor carries. A duration protocol states it. An interval-count protocol does not, so it is
    // computed: the end of the last command (a stop is a command at its own start). A block that starts and never stops
    // (no interval) would run for ever in such a session, and sessions cannot be infinite (SWR-SS-01), so it is refused.
    let session_len_ms = if is_duration {
        session_ms
    } else {
        if let Some(open) = cmds.iter().find(|c| c.duration_ms == 0.0 && !c.params.is_empty()) {
            let ty = proto["modalities"][open.block]["type"].as_str().unwrap_or("a block");
            return Err(format!(
                "Block {} ({ty}) has no interval, so it never stops, and the protocol sets interval_count instead of a duration. A session cannot be infinite: give the protocol a duration, or give the block an interval_on.",
                open.block + 1
            ));
        }
        cmds.iter().map(|c| c.start_ms + c.duration_ms).fold(0.0, f64::max)
    };
    if !(session_len_ms > 0.0) {
        return Err("Session length is zero: the commands end at the moment they start. Give the protocol a duration, or give a block an interval_on greater than zero.".into());
    }

    finish(cmds, session_len_ms, is_t2, opts)
}

/// The descriptor of `cmds`: header, commands and a zeroed signature slot. Shared by a protocol and a composite.
fn finish(cmds: Vec<Cmd>, session_len_ms: f64, is_t2: bool, opts: &CompileOptions) -> R<Compiled> {
    let targets: Vec<SerializedTarget> = cmds.iter().map(|c| serialize_target(&c.target)).collect::<R<_>>()?;
    let times: Vec<(u32, u32)> = cmds
        .iter()
        .map(|c| {
            let n = c.block + 1;
            Ok((
                wire_ms(c.start_ms, &format!("Block {n}: a command's start (startOffsetSeconds and the interval)"))?,
                wire_ms(c.duration_ms, &format!("Block {n}: a command's duration (intervalOnSeconds)"))?,
            ))
        })
        .collect::<R<_>>()?;
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
        // The field is 32 bytes. A shorter serial is zero-filled (the field's padding); a longer one used to be cut to
        // its first 32 bytes, which names a different device, and is refused (REQ-NPPS-33 (d)).
        if serial.len() > 32 {
            return Err(format!(
                "The device serial is {} bytes and the descriptor carries at most 32. The protocol is refused, not signed for a serial cut to fit.",
                serial.len()
            ));
        }
        blob[28..28 + serial.len()].copy_from_slice(serial);
    }
    blob[60..64].copy_from_slice(&wire_ms(session_len_ms, "The session length")?.to_le_bytes());
    // The helmet hardware version, then three reserved bytes that stay zero (SWR-HV-08, NP-FW-HUB-001 §4.2).
    blob[64] = opts.hardware_version;
    // The wavelength-rules stamp of the rules this compile used (SWR-HV-03). A set the stamp cannot name (a value off
    // the 0.1 pm grid) refuses the compile, never a stamp of rounded windows.
    let default_rules = WavelengthRules::default_rules();
    let stamp = crate::wavelength::stamp_for(opts.wavelength_rules.unwrap_or(&default_rules))?;
    blob[STAMP_OFF..STAMP_OFF + crate::wavelength::STAMP_LEN].copy_from_slice(&stamp.to_bytes());

    let mut off = HEADER_LEN;
    for ((c, t), (start, duration)) in cmds.iter().zip(&targets).zip(&times) {
        blob[off] = c.mod_type;
        blob[off + 1] = t.slot_id;
        blob[off + 2..off + 6].copy_from_slice(&start.to_le_bytes());
        blob[off + 6..off + 10].copy_from_slice(&duration.to_le_bytes());
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

/// Compile a composite (`OI-NPPS-COMP-01`): its layers are laid on one timeline by `composite::plan`, and the descriptor
/// is built from that plan exactly as it is for a protocol (zone gate, PBM tile rule, command limit, signature slot).
/// `members` maps a protocol name to its definition. Two blocks of one modality type that overlap in time and come from
/// different layers are refused, except transcranial PBM on sockets that share none; nothing is summed, taken as a
/// maximum or reduced to fit (`CLAUDE.md` §3).
pub fn compile_composite(composite: &Value, members: &serde_json::Map<String, Value>, opts: &CompileOptions) -> R<Compiled> {
    if !(1..=15).contains(&opts.hardware_version) {
        return Err(format!(
            "The hardware version {} is not one a helmet can have (1 to 15). Give the version of the helmet the protocol is compiled for.",
            opts.hardware_version
        ));
    }
    let def = crate::composite::plan(composite, members)?;
    let session_ms = js_round(f(&def["timingMode"], "seconds") * 1000.0);
    let mods = def["modalities"].as_array().cloned().unwrap_or_default();
    let mut cmds: Vec<Cmd> = Vec::new();
    let mut is_t2 = false;
    let mut spans: Vec<(usize, &str, f64, f64, Option<Vec<u32>>)> = Vec::new();
    for (block, m) in mods.iter().enumerate() {
        let ty = s(m, "type");
        if matches!(ty, "qeeg_21ch" | "tms" | "pbm_deep_1170nm" | "clinical_tacs" | "hd_tdcs" | "cervical_vns") {
            is_t2 = true;
        }
        let generated = build_commands(ty, &m["params"], &m["interval"], session_ms, None, opts)?;
        let sockets = generated.iter().find(|c| !c.params.is_empty()).and_then(|c| match &c.target {
            Target::Sockets(v) => Some(v.clone()),
            Target::Slot(_) => None,
        });
        spans.push((
            m["layer"].as_u64().unwrap_or(0) as usize,
            ty,
            f(&m["interval"], "startOffsetSeconds"),
            f(&m["interval"], "endOffsetSeconds"),
            sockets,
        ));
        for mut c in generated {
            c.block = block;
            cmds.push(c);
            if cmds.len() > CMD_MAX {
                return Err(format!(
                    "Composite exceeds maximum command count ({CMD_MAX}). Shorten the layers or reduce interval repeats."
                ));
            }
        }
    }
    for i in 0..spans.len() {
        for j in (i + 1)..spans.len() {
            let (a, b) = (&spans[i], &spans[j]);
            if a.0 == b.0 || a.1 != b.1 || !(a.2 < b.3 && b.2 < a.3) {
                continue;
            }
            let disjoint_pbm = a.1 == "pbm_transcranial"
                && matches!((&a.4, &b.4), (Some(x), Some(y)) if !x.iter().any(|k| y.contains(k)));
            if !disjoint_pbm {
                return Err(format!(
                    "Layers {} and {} both drive {} from {}s to {}s. Two layers may overlap only on transcranial PBM sockets they do not share; the composite is refused rather than summed or reduced.",
                    a.0 + 1, b.0 + 1, a.1, num_string(a.2.max(b.2)), num_string(a.3.min(b.3))
                ));
            }
        }
    }
    zone_gate(&def, opts)?;
    merge_overlapping_pbm(&mut cmds, session_ms)?;
    if cmds.is_empty() {
        return Err("Composite produces no commands — no enabled modalities".into());
    }
    cmds.sort_by(|a, b| a.start_ms.partial_cmp(&b.start_ms).unwrap_or(std::cmp::Ordering::Equal));
    finish(cmds, session_ms, is_t2, opts)
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

fn build_commands(ty: &str, p: &Value, interval: &Value, session_ms: f64, default_repeats: Option<f64>, opts: &CompileOptions) -> R<Vec<Cmd>> {
    // A composite layer's block states where it ends (`endOffsetSeconds`, `composite::plan`). The block is generated as if
    // the session ended there, then anything still running at that moment is cut short and stopped.
    let Some(end_s) = interval["endOffsetSeconds"].as_f64() else {
        return build_commands_to(ty, p, interval, session_ms, default_repeats, opts);
    };
    let end_ms = js_round(end_s * 1000.0);
    let mut out = build_commands_to(ty, p, interval, end_ms, default_repeats, opts)?;
    let mut tail_stops = Vec::new();
    for c in out.iter_mut().filter(|c| !c.params.is_empty()) {
        if c.duration_ms == 0.0 || c.start_ms + c.duration_ms > end_ms {
            c.duration_ms = end_ms - c.start_ms;
            if end_ms < session_ms {
                tail_stops.push(Cmd { block: 0, mod_type: c.mod_type, target: c.target.clone(), start_ms: end_ms, duration_ms: 0.0, params: vec![] });
            }
        }
    }
    out.extend(tail_stops);
    Ok(out)
}

fn build_commands_to(ty: &str, p: &Value, interval: &Value, session_ms: f64, default_repeats: Option<f64>, opts: &CompileOptions) -> R<Vec<Cmd>> {
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
        None if default_repeats.is_some() => default_repeats.unwrap_or(1.0),
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
        "bes_tacs" => encode_bes(p),
        "tdcs" => encode_tdcs(p),
        "vns_hrv" => encode_vns(p),
        "audio_entrainment" => encode_audio(p),
        "visual_stimulation" => encode_visual(p),
        "qeeg_21ch" => Ok(encode_qeeg(p)),
        "tms" => encode_tms(p),
        "pbm_deep_1170nm" => encode_deep(p),
        "clinical_tacs" => encode_clinical_tacs(p),
        "hd_tdcs" => encode_hd_tdcs(p),
        "cervical_vns" => encode_cvns(p),
        "vibrotactile_40hz" => encode_vibro(p),
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
    let fc = freq_code(hz, "PBM")?;
    let duty = duty_reg(hz, f(p, "dutyCyclePercent"), "PBM")?;
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
    let fc = freq_code(hz, "Intranasal PBM")?;
    let duty = duty_reg(hz, f(p, "dutyCyclePercent"), "Intranasal PBM")?;
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

fn encode_bes(p: &Value) -> R<Encoded> {
    let freq_mhz = scaled(p, "frequencyHz", "BES/tACS", 1000.0, 65535.0, "Hz")?;
    let amp_ua = scaled(p, "intensityMilliamps", "BES/tACS", 1000.0, 1000.0, "mA")?;
    let wf = if s(p, "waveform") == "square" { 1 } else { 0 };
    let mut buf = vec![0u8; 7];
    put16(&mut buf, 1, freq_mhz, "BES/tACS frequencyHz")?;
    put16(&mut buf, 3, amp_ua, "BES/tACS intensityMilliamps")?;
    buf[5] = wf;
    Ok(slot(MOD_BES_TACS, SLOT_BES_TACS, buf))
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

fn encode_tdcs(p: &Value) -> R<Encoded> {
    let cur_ua = scaled(p, "intensityMilliamps", "tDCS", 1000.0, 2000.0, "mA")?;
    // The area is the denominator of the safety MCU's charge-density ceiling: a missing or oversized one is refused.
    let area_cm2 = required(p, "electrodeAreaCm2", "tDCS")?;
    let area = (area_cm2 * 1000.0).floor();
    // Under 0.001 cm² the field would be 0, "not declared", which the safety MCU never grants.
    if area_cm2 <= 0.0 || area < 1.0 || area > 65535.0 {
        return Err(format!(
            "tDCS: electrodeAreaCm2 {} is outside what the descriptor can declare (0.001 to 65.535 cm²). The protocol is refused, not reduced to fit.",
            num_string(area_cm2)
        ));
    }
    // The ramp travels as authored. One under the 30 s minimum (CLAUDE.md §3) used to be raised to 30 s, and a missing
    // one became 30 s; the hub refuses a short ramp (REQ-FWHUB-78 (b)) and so does this, because a longer ramp is also a
    // stimulus nobody authored.
    let ramp_s = required(p, "rampSeconds", "tDCS")?;
    if ramp_s < hw::TDCS_RAMP_SECONDS {
        return Err(format!(
            "tDCS: rampSeconds {} s is below the minimum of {} s. The protocol is refused, not lengthened to fit.",
            num_string(ramp_s),
            num_string(hw::TDCS_RAMP_SECONDS)
        ));
    }
    let ramp = carried(p, "rampSeconds", "tDCS", 1.0, 65535.0, "s")?;
    let mut buf = vec![0u8; 8];
    buf[0] = resolve_electrode_pair(p);
    put16(&mut buf, 1, cur_ua, "tDCS intensityMilliamps")?;
    buf[3] = 0;
    put16(&mut buf, 4, ramp, "tDCS rampSeconds")?;
    put16(&mut buf, 6, area, "tDCS electrodeAreaCm2")?;
    Ok(slot(MOD_TDCS, SLOT_TDCS, buf))
}

fn encode_vns(p: &Value) -> R<Encoded> {
    // The upper bound is the wire field's width (32-bit mHz), not a safety limit: the 25 Hz ceiling was an equivalence
    // bound and is removed (OI-VNSCLIP-08, the principal, 2026-10-29). The 1 Hz floor is the validator's.
    let freq_mhz = scaled(p, "frequencyHz", "VNS", 1000.0, u32::MAX as f64, "Hz")?;
    // 40 mA = hw::VNS_MAX_MILLIAMPS, derived from the per-phase charge limit (see its note in constants.json).
    let amp_ua = scaled(p, "intensityMilliamps", "VNS", 1000.0, hw::VNS_MAX_MILLIAMPS * 1000.0, "mA")?;
    // What the descriptor cannot yet deliver is refused, never run as something else: a movement-triggered burst run
    // continuously, or a threshold-relative amplitude run as an absolute one, would be a different and stronger stimulus
    // than the one authored (OI-NPPS-VNS-01).
    let trigger = p["trigger"].as_str().unwrap_or("continuous");
    if trigger != "continuous" {
        return Err(format!(
            "VNS: trigger '{trigger}' is not delivered yet: the hub has no movement-trigger input, so the protocol is refused, not run continuously. (OI-NPPS-VNS-01)"
        ));
    }
    if p["burstSeconds"].as_f64().is_some() {
        return Err("VNS: burst_seconds needs trigger: movement_cue; a continuous block has no bursts. The protocol is refused, not reshaped.".into());
    }
    let side = match p["side"].as_str().unwrap_or("left") {
        "left" => 0,
        "right" => 1,
        "bilateral" => 2,
        other => return Err(format!("VNS: side '{other}' is not left, right or bilateral.")),
    };
    // 0 on the wire is the firmware's default (250 µs); an authored width travels as written, inside the studied range.
    let pulse_width_us = match p.get("pulseWidthUs").filter(|v| !v.is_null()) {
        None => 0.0,
        Some(v) => {
            let w = v.as_f64().filter(|w| w.is_finite()).ok_or("VNS: pulseWidthUs must be a number.")?;
            if w < hw::VNS_MIN_PULSE_WIDTH_US || w > hw::VNS_MAX_PULSE_WIDTH_US {
                return Err(format!(
                    "VNS: pulseWidthUs {} µs is outside the authored range {}–{} µs. The protocol is refused, not reduced to fit.",
                    num_string(w),
                    num_string(hw::VNS_MIN_PULSE_WIDTH_US),
                    num_string(hw::VNS_MAX_PULSE_WIDTH_US)
                ));
            }
            js_round(w)
        }
    };
    // A biphasic pulse of width w at f Hz needs 2·w ≤ 1/f. This is geometry, not a limit anyone chose: a waveform that
    // cannot fit in its own period is not a different stimulus, it is not a stimulus. The width on the wire is 0 for the
    // firmware default, 250 µs.
    let effective_pw_us = if pulse_width_us == 0.0 { 250.0 } else { pulse_width_us };
    if 2.0 * effective_pw_us * 1e-6 * (freq_mhz / 1000.0) > 1.0 {
        return Err(format!(
            "VNS: {} Hz with a {} µs pulse does not fit: a biphasic pulse needs two phases inside one period. The protocol is refused, not reduced to fit.",
            num_string(freq_mhz / 1000.0),
            num_string(effective_pw_us)
        ));
    }
    let proto = match s(p, "hrvProtocol") {
        "combined_pbm" => 1,
        "tavns_sync" => 2,
        "eeg_biofeedback" => 3,
        _ => 0,
    };
    let mut buf = vec![0u8; 12];
    buf[0] = side;
    put32(&mut buf, 1, freq_mhz, "VNS frequencyHz")?;
    put16(&mut buf, 5, amp_ua, "VNS intensityMilliamps")?;
    put16(&mut buf, 7, pulse_width_us, "VNS pulseWidthUs")?;
    buf[9] = 1;
    buf[10] = 1;
    buf[11] = proto;
    Ok(slot(MOD_VNS_HRV, SLOT_VNS_HRV, buf))
}

fn encode_audio(p: &Value) -> R<Encoded> {
    // The beat and the carrier travel as authored. A beat above 65.535 Hz or a carrier above 65535 Hz used to be capped
    // at the field's largest value, and a missing carrier became 65535 Hz; each is refused (REQ-NPPS-33 (a)).
    let mut mode = 4u8;
    let mut beat_mhz = 0.0;
    if p["binauralBeatsHz"].as_f64().is_some() {
        mode = 0;
        beat_mhz = carried(p, "binauralBeatsHz", "Audio", 1000.0, 65535.0, "Hz")?;
    } else if p["isochronicTonesHz"].as_f64().is_some() {
        mode = 1;
        beat_mhz = carried(p, "isochronicTonesHz", "Audio", 1000.0, 65535.0, "Hz")?;
    } else if s(p, "noiseType") == "pink" {
        mode = 2;
    } else if s(p, "noiseType") == "brown" {
        mode = 3;
    }
    let vol = db_to_level_half_db(f(p, "volumeDb"))?;
    let carrier = carried(p, "carrierHz", "Audio", 1.0, 65535.0, "Hz")?;
    let mut buf = vec![0u8; 8];
    buf[0] = mode;
    put16(&mut buf, 1, carrier, "Audio carrierHz")?;
    put16(&mut buf, 3, beat_mhz, "Audio beat")?;
    buf[5] = vol;
    buf[6] = b(p, "boneConductionPacer") as u8;
    buf[7] = b(p, "eegAdaptive") as u8;
    Ok(slot(MOD_AUDIO, SLOT_AUDIO, buf))
}

fn encode_visual(p: &Value) -> R<Encoded> {
    let mode = match s(p, "mode") {
        "binocular" => Some(0u8),
        "emdr" => Some(1),
        "retinal_pbm" => Some(2),
        "mode_f" => Some(3),
        _ => None,
    };
    // The frequency and the EMDR cadence travel as authored. A frequency above 100 Hz used to be encoded as 100 Hz and a
    // cadence above 25.5 Hz as 25.5 Hz, and a missing one became that largest value; each is refused (REQ-NPPS-33 (a)).
    // 100 Hz is the validator's VISUAL_MAX_HZ; 25.5 Hz is the width of the cadence byte (0.1 Hz per step).
    let freq = scaled(p, "frequencyHz", "Visual", 1.0, hw::VISUAL_MAX_HZ, "Hz")?;
    let emdr = carried(p, "emdrCadenceHz", "Visual", 10.0, 255.0, "Hz")?;
    let shade = (mode == Some(0)) as u8;
    Ok(slot(
        MOD_VISUAL,
        SLOT_VISUAL,
        vec![mode.unwrap_or(0), freq as u8, 50, 0x02, 0xFF, 0x0F, emdr as u8, b(p, "enableModeF") as u8, shade],
    ))
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

fn encode_tms(p: &Value) -> R<Encoded> {
    let proto = match s(p, "tmsProtocol") {
        "TBS" => 1,
        "iTBS" => 2,
        _ => 0,
    };
    // The frequency, pulse count and inter-train interval travel as authored. Each used to be capped at 65535 (so a
    // missing frequency or pulse count became 65535), and a frequency under 0.1 Hz was raised to 0.1 Hz for the
    // interval; each is refused (REQ-NPPS-33 (a)).
    let freq_mhz = carried(p, "frequencyHz", "TMS", 1000.0, 65535.0, "Hz")?;
    let hz = f(p, "frequencyHz");
    let intensity = scaled(p, "intensityPercentMT", "TMS", 1.0, 255.0, "% MT")?;
    let pulses = required(p, "pulseCount", "TMS")?;
    if pulses < 0.0 || pulses > 65535.0 || pulses.fract() != 0.0 {
        return Err(format!(
            "TMS: pulseCount {} is not a whole number the descriptor can carry (0–65535). The protocol is refused, not reduced or cut to fit.",
            num_string(pulses)
        ));
    }
    let tbs = s(p, "tmsProtocol") != "rTMS";
    let inter = if tbs {
        200.0
    } else {
        // rTMS: the interval is one period, 1000 / f ms, in a 16-bit field.
        let inter = js_round(1000.0 / hz);
        if !(hz > 0.0) || inter > 65535.0 {
            return Err(format!(
                "TMS: frequencyHz {} Hz gives rTMS an inter-train interval the descriptor cannot carry (1000 / frequency must be at most 65535 ms, so at least 0.016 Hz). The protocol is refused, not run at another interval.",
                num_string(hz)
            ));
        }
        inter
    };
    let mut buf = vec![0u8; 10];
    buf[0] = proto;
    buf[1] = target_index(s(p, "target"));
    put16(&mut buf, 2, freq_mhz, "TMS frequencyHz")?;
    buf[4] = intensity as u8;
    put16(&mut buf, 5, pulses, "TMS pulseCount")?;
    put16(&mut buf, 7, inter, "TMS inter-train interval")?;
    buf[9] = if tbs { 50 } else { 0 };
    Ok(slot(MOD_TMS, SLOT_TMS, buf))
}

fn encode_deep(p: &Value) -> R<Encoded> {
    let intensity = scaled(p, "intensityMWcm2", "1170 nm PBM", 1.0, 1000.0, "mW/cm²")?;
    let hz = f(p, "frequencyHz");
    let fc = freq_code(hz, "1170 nm PBM")?;
    let duty = duty_reg(hz, f(p, "dutyCyclePercent"), "1170 nm PBM")?;
    let mut buf = vec![0u8; 5];
    put16(&mut buf, 0, intensity, "1170 nm PBM intensityMWcm2")?;
    buf[2] = fc;
    buf[3] = duty;
    Ok(slot(MOD_PBM_1170NM, SLOT_PBM_1170NM, buf))
}

fn encode_clinical_tacs(p: &Value) -> R<Encoded> {
    let freq_mhz = scaled(p, "frequencyHz", "Clinical tACS", 1000.0, 65535.0, "Hz")?;
    let amp_ua = scaled(p, "intensityMilliamps", "Clinical tACS", 1000.0, 4000.0, "mA")?;
    let wf = match s(p, "waveform") {
        "sinusoidal" => 0,
        "square" => 1,
        _ => 2,
    };
    let n = required(p, "channelCount", "Clinical tACS")?;
    let n = js_round(n);
    if !(1.0..=CLINICAL_TACS_MAX_CHANNELS).contains(&n) {
        return Err(format!(
            "Clinical tACS: channelCount {} is outside 1–{}. The protocol is refused, not reduced to fit.",
            num_string(n),
            num_string(CLINICAL_TACS_MAX_CHANNELS)
        ));
    }
    let mask: u32 = if n == 0.0 { 0 } else { (1u64 << (n as u64)) as u32 - 1 };
    let mut buf = vec![0u8; 8];
    put16(&mut buf, 0, freq_mhz, "Clinical tACS frequencyHz")?;
    put16(&mut buf, 2, amp_ua, "Clinical tACS intensityMilliamps")?;
    buf[4] = (mask & 0xFF) as u8;
    buf[5] = ((mask >> 8) & 0xFF) as u8;
    buf[6] = ((mask >> 16) & 0x1F) as u8;
    buf[7] = wf;
    Ok(slot(MOD_CLIN_TACS, SLOT_CLIN_TACS, buf))
}

fn encode_hd_tdcs(p: &Value) -> R<Encoded> {
    let montage = match s(p, "montage") {
        "bilateral_4x1" => 1,
        "standard_2_electrode" => 2,
        _ => 0,
    };
    let cur = scaled(p, "intensityMilliamps", "HD-tDCS", 1000.0, 2000.0, "mA")?;
    let mut buf = vec![0u8; 6];
    buf[0] = target_index(s(p, "target"));
    buf[1] = montage;
    put16(&mut buf, 2, cur, "HD-tDCS intensityMilliamps")?;
    put16(&mut buf, 4, 30.0, "HD-tDCS ramp")?;
    Ok(slot(MOD_HD_TDCS, SLOT_HD_TDCS, buf))
}

fn encode_cvns(p: &Value) -> R<Encoded> {
    let freq_mhz = scaled(p, "frequencyHz", "Cervical VNS", 1000.0, 25000.0, "Hz")?;
    let amp_ua = scaled(p, "intensityMilliamps", "Cervical VNS", 1000.0, 2000.0, "mA")?;
    let mut buf = vec![0u8; 10];
    put16(&mut buf, 1, freq_mhz, "Cervical VNS frequencyHz")?;
    put16(&mut buf, 3, amp_ua, "Cervical VNS intensityMilliamps")?;
    put16(&mut buf, 5, 0.0, "Cervical VNS pulse width")?;
    put16(&mut buf, 7, 10.0, "Cervical VNS ramp")?;
    buf[9] = 1;
    Ok(slot(MOD_CVNS, SLOT_CVNS, buf))
}

fn encode_vibro(p: &Value) -> R<Encoded> {
    let g = required(p, "intensityG", "Vibrotactile")?;
    if !(0.6..=1.2).contains(&g) {
        return Err(format!(
            "Vibrotactile: intensityG {} is outside 0.6–1.2 G. The protocol is refused, not reduced to fit.",
            num_string(g)
        ));
    }
    let gain = js_round((g - 0.6) / 0.6 * f64::from(0x7Fu8));
    let sync = (b(p, "syncToAudio") as u8) | ((b(p, "syncToVisual") as u8) << 1);
    let mut buf = vec![0u8; 4];
    buf[0] = uint(gain, f64::from(0x7Fu8), "Vibrotactile intensityG")? as u8;
    buf[1] = sync;
    put16(&mut buf, 2, 40000.0, "Vibrotactile frequency")?;
    Ok(slot(MOD_VIBROTACTILE, SLOT_VIBROTACTILE, buf))
}
