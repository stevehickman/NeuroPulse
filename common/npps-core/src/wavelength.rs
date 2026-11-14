//! PBM wavelength mapping (NP-NPPS-REF-001 §4.1a, §7a): which emitter channel delivers a
//! requested wavelength. A port of common/lib/wavelengthRules.ts.

use crate::table::table;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Channel {
    Led660,
    Led808,
    Led1064,
}

impl Channel {
    pub const ALL: [Channel; 3] = [Channel::Led660, Channel::Led808, Channel::Led1064];
    pub fn name(self) -> &'static str {
        match self {
            Channel::Led660 => "led_660",
            Channel::Led808 => "led_808",
            Channel::Led1064 => "led_1064",
        }
    }
}

#[derive(Debug, Clone)]
pub struct ChannelRule {
    pub element: Channel,
    pub nominal_nm: f64,
    pub min_nm: f64,
    pub max_nm: f64,
}

#[derive(Debug, Clone)]
pub struct WavelengthRules {
    pub name: String,
    pub channels: Vec<ChannelRule>,
}

impl WavelengthRules {
    /// The shipped defaults: each channel's CLAUDE.md §3 band widened by 10 nm a side.
    pub fn default_rules() -> WavelengthRules {
        WavelengthRules {
            name: "NeurOne default wavelength mapping".into(),
            channels: vec![
                ChannelRule { element: Channel::Led660, nominal_nm: 660.0, min_nm: 650.0, max_nm: 680.0 },
                ChannelRule { element: Channel::Led808, nominal_nm: 808.0, min_nm: 798.0, max_nm: 840.0 },
                ChannelRule { element: Channel::Led1064, nominal_nm: 1064.0, min_nm: 1054.0, max_nm: 1074.0 },
            ],
        }
    }
}

pub enum Parsed {
    Retired,
    Single(f64),
    Invalid,
}

/// Classify a `wavelength` value: one `"<N>nm"`, a retired channel name, or neither.
pub fn parse_pbm_wavelength(value: &str) -> Parsed {
    if table().retired_wavelength(value).is_some() {
        return Parsed::Retired;
    }
    if let Some(num) = value.strip_suffix("nm") {
        let ok = !num.is_empty()
            && num.chars().all(|c| c.is_ascii_digit() || c == '.')
            && !num.starts_with('.')
            && !num.ends_with('.')
            && num.matches('.').count() <= 1;
        if ok {
            if let Ok(nm) = num.parse::<f64>() {
                if nm.is_finite() && nm > 0.0 {
                    return Parsed::Single(nm);
                }
            }
        }
    }
    Parsed::Invalid
}

pub fn retired_message(value: &str) -> String {
    let blocks = table()
        .retired_wavelength(value)
        .unwrap_or_default()
        .iter()
        .map(|w| format!("\"{w}\""))
        .collect::<Vec<_>>()
        .join(" and ");
    format!(
        "wavelength \"{value}\" is retired: it welded independent emitters into one block. Write one block per wavelength ({blocks}), each with its own irradiance_mw_cm2."
    )
}

pub enum Resolution {
    Ok(Channel),
    Retired,
    Invalid,
    Unmapped,
}

pub fn resolve(value: &str, rules: &WavelengthRules) -> Resolution {
    match parse_pbm_wavelength(value) {
        Parsed::Retired => Resolution::Retired,
        Parsed::Invalid => Resolution::Invalid,
        Parsed::Single(nm) => {
            let mut best: Option<&ChannelRule> = None;
            for el in Channel::ALL {
                let Some(rule) = rules.channels.iter().find(|c| c.element == el) else { continue };
                if nm < rule.min_nm || nm > rule.max_nm {
                    continue;
                }
                if best.map_or(true, |b| (nm - rule.nominal_nm).abs() < (nm - b.nominal_nm).abs()) {
                    best = Some(rule);
                }
            }
            match best {
                Some(r) => Resolution::Ok(r.element),
                None => Resolution::Unmapped,
            }
        }
    }
}

/// The one channel that delivers `wavelength`, or the refusal the reference compiler writes.
pub fn resolve_one_channel(
    wavelength: &str,
    rules: &WavelengthRules,
    allowed: &[Channel],
    what: &str,
) -> Result<Channel, String> {
    match resolve(wavelength, rules) {
        Resolution::Retired => Err(format!("{what}: {}", retired_message(wavelength))),
        Resolution::Invalid => Err(format!(
            "{what} wavelength '{wavelength}' is not a wavelength: write one value such as \"810nm\"."
        )),
        Resolution::Unmapped => Err(format!(
            "No emitter channel delivers {wavelength} under the wavelength rules in force (\"{}\"). The protocol is refused, not moved to the nearest channel; edit the rules if that mapping is intended.",
            rules.name
        )),
        Resolution::Ok(ch) => {
            if !allowed.contains(&ch) {
                return Err(format!(
                    "{what} cannot be delivered on {}: {wavelength} maps to a channel it does not carry.",
                    ch.name()
                ));
            }
            Ok(ch)
        }
    }
}
