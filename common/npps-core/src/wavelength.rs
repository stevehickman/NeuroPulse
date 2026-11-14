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
    /// The release (`major.minor`) the block states (`version:` in a `wavelength_rules` block, §7a.1). For the
    /// shipped file it is the release; for an edited set it names the release it was merged over. Never read to decide
    /// what a set delivers.
    pub version: Option<(u8, u8)>,
    pub channels: Vec<ChannelRule>,
}

/// `"1.0"` as (1, 0): two whole numbers, each 0 to 255, no sign, space or third part.
pub fn parse_release_version(text: &str) -> Result<(u8, u8), String> {
    let bad = || format!("version \"{text}\" is not a release such as \"1.0\": write major.minor, two whole numbers from 0 to 255");
    let (a, b) = text.split_once('.').ok_or_else(bad)?;
    let num = |s: &str| {
        if s.is_empty() || !s.chars().all(|c| c.is_ascii_digit()) {
            return Err(bad());
        }
        s.parse::<u8>().map_err(|_| bad())
    };
    Ok((num(a)?, num(b)?))
}

impl WavelengthRules {
    /// The shipped defaults: each channel's CLAUDE.md §3 band widened by 10 nm a side.
    pub fn default_rules() -> WavelengthRules {
        WavelengthRules {
            name: "NeurOne default wavelength mapping".into(),
            version: Some((1, 0)),
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

// ---------------------------------------------------------------------------------------------
// Version of the rules (`SWR-HV-03`; NP-NPPS-REF-001 §7a.1, `OI-WRV-01`, GitHub #564).
//
// The identity of a rule set is the SHA-256 of its canonical form over whole counts of 0.1 pm.
// The stamp carries the windows themselves, so a reader needs no store to say how a wavelength
// was mapped. Computed here only (`CLAUDE.md` §22).
// ---------------------------------------------------------------------------------------------

/// Domain line of the canonical form. It changes if the mapping algorithm ever changes.
pub const RULES_DOMAIN: &str = "NeurOne.WavelengthRules.1";
/// Counts of the 0.1 pm grid in one nanometre (Q5, ruled 2026-11-13; `UC-175`).
pub const COUNTS_PER_NM: f64 = 10_000.0;
pub const STAMP_LEN: usize = 39;
pub const WINDOWS_LEN: usize = 36;

/// `(nominal, min, max)` in 0.1 pm counts; `None` is a channel with no rule.
pub type Window = Option<(u32, u32, u32)>;

fn to_counts(nm: f64, what: &str) -> Result<u32, String> {
    let c = (nm * COUNTS_PER_NM).round();
    if !nm.is_finite() || nm < 0.0 || c > u32::MAX as f64 || (nm * COUNTS_PER_NM - c).abs() > 1e-6 {
        return Err(format!(
            "{what} {nm} nm is not on the 0.1 pm grid (a multiple of 0.0001 nm): the rules have no canonical form and are not stamped."
        ));
    }
    Ok(c as u32)
}

/// The three windows of a rule set in the order 660, 808, 1064. A value off the grid refuses.
pub fn windows_of(rules: &WavelengthRules) -> Result<[Window; 3], String> {
    let mut out: [Window; 3] = [None, None, None];
    for (i, ch) in Channel::ALL.iter().enumerate() {
        if let Some(r) = rules.channels.iter().find(|c| c.element == *ch) {
            let n = ch.name();
            out[i] = Some((
                to_counts(r.nominal_nm, &format!("{n} nominal"))?,
                to_counts(r.min_nm, &format!("{n} minimum"))?,
                to_counts(r.max_nm, &format!("{n} maximum"))?,
            ));
        }
    }
    Ok(out)
}

/// The 36 window bytes: per channel nominal, minimum, maximum as u32 little-endian; no rule is zeros.
pub fn windows_bytes(w: &[Window; 3]) -> [u8; WINDOWS_LEN] {
    let mut b = [0u8; WINDOWS_LEN];
    for (i, win) in w.iter().enumerate() {
        if let Some((nom, min, max)) = win {
            for (j, v) in [nom, min, max].iter().enumerate() {
                b[i * 12 + j * 4..i * 12 + j * 4 + 4].copy_from_slice(&v.to_le_bytes());
            }
        }
    }
    b
}

/// Inverse of [`windows_bytes`]. Twelve zero bytes read as a channel with no rule.
pub fn windows_from_bytes(b: &[u8; WINDOWS_LEN]) -> [Window; 3] {
    let mut out: [Window; 3] = [None, None, None];
    for (i, o) in out.iter_mut().enumerate() {
        let v = |j: usize| u32::from_le_bytes(b[i * 12 + j * 4..i * 12 + j * 4 + 4].try_into().unwrap());
        if b[i * 12..i * 12 + 12].iter().any(|x| *x != 0) {
            *o = Some((v(0), v(1), v(2)));
        }
    }
    out
}

/// The canonical bytes that are hashed.
pub fn canonical_bytes(w: &[Window; 3]) -> Vec<u8> {
    let mut s = format!("{RULES_DOMAIN}\n{{\"channels\":[");
    for (i, ch) in Channel::ALL.iter().enumerate() {
        if i > 0 {
            s.push(',');
        }
        let f = |x: Option<u32>| x.map_or("null".to_string(), |v| v.to_string());
        let (nom, min, max) = match w[i] {
            Some((n, lo, hi)) => (Some(n), Some(lo), Some(hi)),
            None => (None, None, None),
        };
        s.push_str(&format!(
            "{{\"element\":\"{}\",\"max_pm_x10\":{},\"min_pm_x10\":{},\"nominal_pm_x10\":{}}}",
            ch.name(),
            f(max),
            f(min),
            f(nom)
        ));
    }
    s.push_str("]}");
    s.into_bytes()
}

/// SHA-256 of the canonical form, 64 lowercase hex digits.
pub fn rules_hash_hex(w: &[Window; 3]) -> String {
    use sha2::{Digest, Sha256};
    Sha256::digest(canonical_bytes(w)).iter().map(|b| format!("{b:02x}")).collect()
}

/// The hash of a rule set; refuses a value off the grid.
pub fn rules_hash(rules: &WavelengthRules) -> Result<String, String> {
    Ok(rules_hash_hex(&windows_of(rules)?))
}

pub const ORIGIN_NOT_STATED: u8 = 0;
pub const ORIGIN_RELEASE: u8 = 1;
pub const ORIGIN_OWNER_SET: u8 = 2;

/// The 39-byte stamp: origin, release major, release minor, 36 window bytes.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct RulesStamp {
    pub origin: u8,
    pub major: u8,
    pub minor: u8,
    pub windows: [u8; WINDOWS_LEN],
}

impl RulesStamp {
    /// What the hub records for a descriptor that states none: origin 0, zero bytes, no invented version.
    pub fn not_stated() -> RulesStamp {
        RulesStamp { origin: ORIGIN_NOT_STATED, major: 0, minor: 0, windows: [0; WINDOWS_LEN] }
    }
    /// `origin` is [`ORIGIN_RELEASE`] or [`ORIGIN_OWNER_SET`] (then `major.minor` is the base release).
    pub fn for_rules(origin: u8, major: u8, minor: u8, rules: &WavelengthRules) -> Result<RulesStamp, String> {
        if origin != ORIGIN_RELEASE && origin != ORIGIN_OWNER_SET {
            return Err(format!("stamp origin {origin} is not a release (1) or an owner set (2)"));
        }
        Ok(RulesStamp { origin, major, minor, windows: windows_bytes(&windows_of(rules)?) })
    }
    pub fn to_bytes(&self) -> [u8; STAMP_LEN] {
        let mut b = [0u8; STAMP_LEN];
        b[0] = self.origin;
        b[1] = self.major;
        b[2] = self.minor;
        b[3..].copy_from_slice(&self.windows);
        b
    }
    pub fn from_bytes(b: &[u8; STAMP_LEN]) -> RulesStamp {
        RulesStamp { origin: b[0], major: b[1], minor: b[2], windows: b[3..].try_into().unwrap() }
    }
    /// A reader's hash, computed from the carried windows alone (none for origin 0).
    pub fn hash_hex(&self) -> Option<String> {
        (self.origin != ORIGIN_NOT_STATED).then(|| rules_hash_hex(&windows_from_bytes(&self.windows)))
    }
}

/// How a new rule set differs from an old one (§7a.1 major or minor line, as a floor).
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum RulesChange {
    None,
    Minor,
    Major,
}

/// Channel delivering `x2` (a wavelength in half-counts), nearest nominal, a tie to the earlier channel.
fn deliver_x2(w: &[Window; 3], x2: i64) -> Option<usize> {
    let mut best: Option<(usize, i64)> = None;
    for (i, win) in w.iter().enumerate() {
        let Some((nom, min, max)) = win else { continue };
        if x2 < 2 * *min as i64 || x2 > 2 * *max as i64 {
            continue;
        }
        let d = (x2 - 2 * *nom as i64).abs();
        if best.map_or(true, |(_, bd)| d < bd) {
            best = Some((i, d));
        }
    }
    best.map(|(i, _)| i)
}

/// Major when a wavelength once delivered is refused or moves channel; minor when the hash moves
/// and none does; none when the hash is the same.
pub fn classify_change(old: &[Window; 3], new: &[Window; 3]) -> RulesChange {
    if canonical_bytes(old) == canonical_bytes(new) {
        return RulesChange::None;
    }
    // The mapping is constant between breakpoints (edges and nominal midpoints), so testing each
    // breakpoint and its neighbours in half-counts covers every wavelength.
    let mut pts: Vec<i64> = Vec::new();
    for w in [old, new] {
        for win in w.iter().flatten() {
            let (n, lo, hi) = *win;
            for v in [n, lo, hi] {
                pts.push(2 * v as i64);
            }
        }
        let noms: Vec<i64> = w.iter().flatten().map(|x| x.0 as i64).collect();
        for a in &noms {
            for b in &noms {
                pts.push(a + b);
            }
        }
    }
    for p in pts {
        for x2 in [p - 1, p, p + 1] {
            if let Some(ch) = deliver_x2(old, x2) {
                if deliver_x2(new, x2) != Some(ch) {
                    return RulesChange::Major;
                }
            }
        }
    }
    RulesChange::Minor
}

/// One row of the register of released rule sets.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Release {
    pub major: u8,
    pub minor: u8,
    pub hash: String,
}

/// The register of released rule sets, oldest first: `common/npps/wavelength-rules-register.json`, compiled in so
/// every platform that reaches the core has the same list (`SWR-HV-03`, `OI-WRV-01`). It is append-only.
pub const REGISTER_JSON: &str = include_str!("../../npps/wavelength-rules-register.json");

pub fn parse_register(text: &str) -> Result<Vec<Release>, String> {
    let v: serde_json::Value = serde_json::from_str(text).map_err(|e| format!("the register is not JSON: {e}"))?;
    let rows = v["releases"].as_array().ok_or("the register has no `releases` array")?;
    let mut out = Vec::new();
    for (i, r) in rows.iter().enumerate() {
        let n = |k: &str| {
            r[k].as_u64().and_then(|x| u8::try_from(x).ok()).ok_or_else(|| format!("register row {}: {k} must be a whole number 0 to 255", i + 1))
        };
        let hash = r["hash"].as_str().ok_or_else(|| format!("register row {}: hash is missing", i + 1))?;
        if hash.len() != 64 || !hash.chars().all(|c| c.is_ascii_digit() || ('a'..='f').contains(&c)) {
            return Err(format!("register row {}: hash is not 64 lowercase hex digits", i + 1));
        }
        out.push(Release { major: n("major")?, minor: n("minor")?, hash: hash.to_string() });
    }
    if out.is_empty() {
        return Err("the register lists no release".into());
    }
    // A version never goes down and is never reused; a set is released once (append-only, §7a.1).
    for (i, w) in out.windows(2).enumerate() {
        if (w[1].major, w[1].minor) <= (w[0].major, w[0].minor) {
            return Err(format!(
                "register row {}: version {}.{} does not come after {}.{}: a version never goes down and is never reused",
                i + 2, w[1].major, w[1].minor, w[0].major, w[0].minor
            ));
        }
    }
    for (i, r) in out.iter().enumerate() {
        if out[..i].iter().any(|o| o.hash == r.hash) {
            return Err(format!("register row {}: the set was already released under an earlier version", i + 1));
        }
    }
    Ok(out)
}

/// The compiled-in register.
pub fn releases() -> &'static [Release] {
    static R: std::sync::OnceLock<Vec<Release>> = std::sync::OnceLock::new();
    R.get_or_init(|| parse_register(REGISTER_JSON).expect("the compiled-in wavelength-rules register is well formed"))
}

/// The stamp a compile states for the rules it used: a release when the hash is in [`releases`], whatever
/// the block's level says; otherwise an owner set over the latest release. Refuses a value off the grid.
pub fn stamp_for(rules: &WavelengthRules) -> Result<RulesStamp, String> {
    let w = windows_of(rules)?;
    let hash = rules_hash_hex(&w);
    let reg = releases();
    let (origin, major, minor) = match reg.iter().find(|r| r.hash == hash) {
        Some(r) => (ORIGIN_RELEASE, r.major, r.minor),
        None => {
            // The base is the release the block says it was merged over; with none stated, the latest.
            let (major, minor) = rules.version.unwrap_or_else(|| {
                let last = &reg[reg.len() - 1];
                (last.major, last.minor)
            });
            (ORIGIN_OWNER_SET, major, minor)
        }
    };
    Ok(RulesStamp { origin, major, minor, windows: windows_bytes(&w) })
}
