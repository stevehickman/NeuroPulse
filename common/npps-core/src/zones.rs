//! The zone model: where a protocol's electrical dose falls, as safe, caution or danger (`docs/reference/safety-zones.md`).
//!
//! Each charge-balanced or DC block is reduced to a handful of **axes**, quantities with a damage or heating
//! mechanism behind them. An axis has a caution boundary and sometimes a danger boundary. **The zone of a block is its
//! worst axis, and the zone of a protocol is its worst block.**
//!
//! * **Danger** is a boundary the protocol may not cross: the compiler refuses it and the validator reports an error.
//! * **Caution** is a boundary it may cross only after the author acknowledges it: the validator reports a warning, and
//!   the compiler refuses the protocol unless the request carries that caution's acknowledgement id.
//! * **Safe** is everything else, and runs without a word.
//!
//! The danger boundaries on charge per phase and on session charge are the existing hardware ceilings (the safety MCU
//! enforces the same numbers). Every other boundary here is a PLACEHOLDER (`UC-073` to `UC-077`): no derivation stands
//! behind it, and it is held in `common/npps/constants.json` so it can move without a code change. **The app-side zone
//! is advisory to the safety MCU, never a substitute**: a caution boundary is always below a boundary the MCU enforces.

use crate::constants::{hardware_limits as hw, safety_zones as z};
use serde_json::Value;

#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord)]
pub enum Zone {
    Safe,
    Caution,
    Danger,
}

impl Zone {
    pub fn name(self) -> &'static str {
        match self {
            Zone::Safe => "safe",
            Zone::Caution => "caution",
            Zone::Danger => "danger",
        }
    }
}

/// One quantity of a block's dose, with its boundaries.
#[derive(Clone, Debug)]
pub struct Axis {
    /// Stable id, also the suffix of an acknowledgement id.
    pub id: &'static str,
    /// Locale key of the axis's name (`ZONE_AXIS_…`).
    pub name_key: &'static str,
    /// English name, for compiler diagnostics (compiler text is deliberately literal, `CLAUDE.md` §17).
    pub name: &'static str,
    pub unit: &'static str,
    pub value: f64,
    pub caution: f64,
    pub danger: Option<f64>,
}

impl Axis {
    pub fn zone(&self) -> Zone {
        if self.danger.map_or(false, |d| self.value >= d) {
            Zone::Danger
        } else if self.value >= self.caution {
            Zone::Caution
        } else {
            Zone::Safe
        }
    }

    /// True for the axes whose danger boundary is an existing hardware ceiling the validator already reports.
    pub fn danger_is_hardware_ceiling(&self) -> bool {
        matches!(self.id, "phaseChargeDensity" | "sessionChargeDensity")
    }
}

#[derive(Clone, Debug)]
pub struct Block {
    pub index: usize,
    pub kind: String,
    pub axes: Vec<Axis>,
}

impl Block {
    pub fn zone(&self) -> Zone {
        self.axes.iter().map(Axis::zone).max().unwrap_or(Zone::Safe)
    }

    /// What an author acknowledges. It names the block, the axis and the value, so it stops matching the moment any of
    /// them changes: an acknowledgement is of this dose, not of the protocol in general.
    pub fn ack_id(&self, axis: &Axis) -> String {
        format!("{}:{}:{}={:.3}", self.index, self.kind, axis.id, axis.value)
    }
}

fn num(v: &Value, k: &str) -> f64 {
    v[k].as_f64().unwrap_or(f64::NAN)
}

/// The fraction of the time stimulation is on, from the block's interval: 1 for a block that runs throughout.
fn duty(interval: &Value) -> f64 {
    let on = interval["intervalOnSeconds"].as_f64().unwrap_or(0.0);
    let off = interval["intervalOffSeconds"].as_f64().unwrap_or(0.0);
    if on > 0.0 && off >= 0.0 { on / (on + off) } else { 1.0 }
}

struct Dose {
    /// Charge per phase, µC.
    q_uc: f64,
    /// RMS and mean-rectified current while a train is on, mA, already scaled by the on-fraction.
    rms_ma: f64,
    mean_ma: f64,
    area_cm2: f64,
    /// The phase width when the waveform is a rectangular pulse, seconds (the Shannon axis applies to it).
    rect_phase_s: Option<f64>,
}

/// A sinusoid delivers 2/π of a rectangular phase of the same duration and peak, as the safety MCU assumes.
fn dose(kind: &str, p: &Value, interval: &Value) -> Option<Dose> {
    let i = num(p, "intensityMilliamps");
    let f = num(p, "frequencyHz");
    let d = duty(interval);
    let (area, width) = match kind {
        "bes_tacs" | "clinical_tacs" => (hw::BES_ELECTRODE_AREA_CM2, None),
        "vns_hrv" => {
            let w = p["pulseWidthUs"].as_f64().filter(|w| w.is_finite() && *w > 0.0).map_or(hw::VNS_DEFAULT_PULSE_WIDTH_SECONDS, |us| us * 1e-6);
            (hw::VNS_ELECTRODE_AREA_CM2, Some(w))
        }
        "cervical_vns" => (hw::CERVICAL_VNS_ELECTRODE_AREA_CM2, Some(hw::VNS_DEFAULT_PULSE_WIDTH_SECONDS)),
        _ => return None,
    };
    if !(i.is_finite() && i > 0.0 && f.is_finite() && f > 0.0 && area > 0.0) {
        return None;
    }
    Some(match width {
        // A train of rectangular biphasic pulses: two phases of `w` per pulse, `f` pulses a second.
        Some(w) => Dose {
            q_uc: i * 1000.0 * w,
            rms_ma: i * (2.0 * w * f * d).sqrt(),
            mean_ma: 2.0 * i * w * f * d,
            area_cm2: area,
            rect_phase_s: Some(w),
        },
        None if p["waveform"].as_str() == Some("sinusoidal") => Dose {
            q_uc: i * 1000.0 / (std::f64::consts::PI * f),
            rms_ma: i / std::f64::consts::SQRT_2 * d.sqrt(),
            mean_ma: 2.0 / std::f64::consts::PI * i * d,
            area_cm2: area,
            rect_phase_s: None,
        },
        // A charge-balanced square wave: the phase is the half period, so the current is on throughout.
        None => Dose { q_uc: i * 1000.0 / (2.0 * f), rms_ma: i * d.sqrt(), mean_ma: i * d, area_cm2: area, rect_phase_s: Some(1.0 / (2.0 * f)) },
    })
}

fn axes_of(kind: &str, p: &Value, interval: &Value, session_seconds: Option<f64>) -> Vec<Axis> {
    let mut axes = Vec::new();
    if let Some(d) = dose(kind, p, interval) {
        let density = d.q_uc / d.area_cm2;
        axes.push(Axis {
            id: "phaseChargeDensity",
            name_key: "ZONE_AXIS_PHASE_CHARGE_DENSITY",
            name: "charge density per phase",
            unit: "µC/cm²",
            value: density,
            caution: z::ZONE_CAUTION_PHASE_CHARGE_DENSITY_UC_CM2,
            danger: Some(hw::PULSED_MAX_PHASE_CHARGE_DENSITY_UC_CM2),
        });
        if d.rect_phase_s.map_or(false, |w| w <= z::ZONE_SHANNON_MAX_PHASE_SECONDS) && density > 0.0 && d.q_uc > 0.0 {
            axes.push(Axis {
                id: "shannonK",
                name_key: "ZONE_AXIS_SHANNON_K",
                name: "Shannon k",
                unit: "",
                value: density.log10() + d.q_uc.log10(),
                caution: z::ZONE_CAUTION_SHANNON_K,
                danger: None,
            });
        }
        axes.push(Axis {
            id: "rmsCurrentDensity",
            name_key: "ZONE_AXIS_RMS_CURRENT_DENSITY",
            name: "rms current density",
            unit: "mA/cm²",
            value: d.rms_ma / d.area_cm2,
            caution: z::ZONE_CAUTION_RMS_CURRENT_DENSITY_MA_CM2,
            danger: Some(z::ZONE_DANGER_RMS_CURRENT_DENSITY_MA_CM2),
        });
        axes.push(Axis {
            id: "meanCurrentDensity",
            name_key: "ZONE_AXIS_MEAN_CURRENT_DENSITY",
            name: "mean current density",
            unit: "µA/cm²",
            value: d.mean_ma * 1000.0 / d.area_cm2,
            caution: z::ZONE_CAUTION_MEAN_CURRENT_DENSITY_UA_CM2,
            danger: Some(z::ZONE_DANGER_MEAN_CURRENT_DENSITY_UA_CM2),
        });
    }
    if kind == "tdcs" {
        let (i, area) = (num(p, "intensityMilliamps"), num(p, "electrodeAreaCm2"));
        if let (true, true, Some(t)) = (i.is_finite() && i > 0.0, area.is_finite() && area > 0.0, session_seconds.filter(|t| *t > 0.0)) {
            axes.push(Axis {
                id: "sessionChargeDensity",
                name_key: "ZONE_AXIS_SESSION_CHARGE_DENSITY",
                name: "session charge density",
                unit: "mC/cm²",
                value: i * t / area,
                caution: z::ZONE_CAUTION_DC_CHARGE_DENSITY_MC_CM2,
                danger: Some(hw::TDCS_MAX_SESSION_CHARGE_DENSITY_MC_CM2),
            });
        }
    }
    axes
}

/// Every enabled block of a protocol, with its axes. A block that drives no electrode, or lacks what an axis needs, has
/// none: the validator reports a missing parameter separately.
pub fn evaluate(def: &Value) -> Vec<Block> {
    let session_seconds = if def["timingMode"]["type"] == "duration" { def["timingMode"]["seconds"].as_f64() } else { None };
    def["modalities"]
        .as_array()
        .map(|mods| {
            mods.iter()
                .enumerate()
                .filter(|(_, m)| m["enabled"] != false)
                .map(|(index, m)| {
                    let kind = m["type"].as_str().unwrap_or("").to_string();
                    Block { index, axes: axes_of(&kind, &m["params"], &m["interval"], session_seconds), kind }
                })
                .filter(|b| !b.axes.is_empty())
                .collect()
        })
        .unwrap_or_default()
}

/// The worst zone of a protocol.
pub fn protocol_zone(blocks: &[Block]) -> Zone {
    blocks.iter().map(Block::zone).max().unwrap_or(Zone::Safe)
}

/// The blocks as the validator's reply carries them: each axis with its value, its boundaries, its zone and, for a
/// caution, the id an author acknowledges.
pub fn to_json(blocks: &[Block]) -> Value {
    use serde_json::json;
    Value::Array(
        blocks
            .iter()
            .map(|b| {
                json!({
                    "index": b.index,
                    "modality": b.kind,
                    "zone": b.zone().name(),
                    "axes": b.axes.iter().map(|a| json!({
                        "id": a.id, "nameKey": a.name_key, "unit": a.unit, "value": a.value,
                        "caution": a.caution, "danger": a.danger, "zone": a.zone().name(),
                        "ackId": if a.zone() == Zone::Caution { Value::String(b.ack_id(a)) } else { Value::Null },
                    })).collect::<Vec<_>>(),
                })
            })
            .collect(),
    )
}
