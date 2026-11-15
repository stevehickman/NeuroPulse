//! The version of the wavelength rules (`SWR-HV-03`, `OI-WRV-01`, `NP-NPPS-REF-001` §7a.1).
//!
//! **What a version is** (principal ruling, option D): a release version `major.minor` that NeurOne assigns to a set
//! it ships, an origin flag, and the three channel windows themselves. The identity of a set is the SHA-256 of its
//! canonical form, which this module computes and nobody assigns (`CLAUDE.md` §22). The identity is of the
//! **resolved** set, the shipped defaults with any overrides merged in, so it covers the nominal, the minimum and the
//! maximum of each channel, or the fact that a channel has no rule. Name, level and description are left out: they do
//! not change which channel delivers a wavelength.
//!
//! **The canonical form** is the domain line [`DOMAIN`], then one JSON object with no whitespace: three channels in
//! the order 660, 808, 1064, each with the keys `element`, `max_pm_x10`, `min_pm_x10`, `nominal_pm_x10` in that
//! order, a value being a whole count of 0.1 pm (10⁻⁴ nm), so 808 nm is `8080000`. A channel with no rule keeps its
//! element and has `null` for the rest. Integers only, so the hash does not depend on how a library prints a decimal.
//! A value that is not on the grid is refused, never rounded: a rounded window is not the window the owner set.
//!
//! **The stamp** is what the session record holds: 39 bytes, one origin byte, release major, release minor, then the
//! 36 bytes of windows (three channels of nominal, minimum and maximum as little-endian `u32`, twelve zero bytes for a
//! channel with no rule). Its length never depends on the rules. The windows are carried, and the hash is not, so a
//! reader can say how a wavelength was mapped with no store to look up.
//!
//! **Comparison is by content.** [`ResolvedSet`] equality is equality of the windows; release numbers only name a
//! set for a reader. [`delivery_change`] says whether a change is major (a wavelength once delivered is now refused or
//! moves channel) or minor, which is the floor of §7a.1, and NeurOne may still call a release major.
//!
//! **The register** is `common/npps/wavelength-rules-register.json`: every released (version, hash), newest last.

use crate::wavelength::{Channel, WavelengthRules};
use serde_json::Value;
use sha2::{Digest, Sha256};

/// Prefix of the hashed bytes. Changing the canonical form, or the mapping algorithm, changes this string.
pub const DOMAIN: &str = "NeurOne.WavelengthRules.1\n";
/// A count of 0.1 pm in a nanometre.
pub const UNITS_PER_NM: u32 = 10_000;
/// Bytes of the windows in a stamp.
pub const WINDOWS_LEN: usize = 36;
/// Bytes of a stamp: origin, release major, release minor, the windows.
pub const STAMP_LEN: usize = 3 + WINDOWS_LEN;

const REGISTER: &str = include_str!("../../npps/wavelength-rules-register.json");

/// One channel's window, in counts of 0.1 pm.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct Window {
    pub nominal: u32,
    pub min: u32,
    pub max: u32,
}

/// The set the compiler uses: one optional window per channel, in the order 660, 808, 1064.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct ResolvedSet {
    pub channels: [Option<Window>; 3],
}

/// A wavelength in nanometres as a whole count of 0.1 pm, or the refusal that names the value.
pub fn nm_to_units(what: &str, nm: f64) -> Result<u32, String> {
    let scaled = nm * f64::from(UNITS_PER_NM);
    let r = scaled.round();
    if !nm.is_finite() || r < 1.0 || r > f64::from(u32::MAX) {
        return Err(format!("{what} {nm} nm is not a usable wavelength."));
    }
    if (scaled - r).abs() > 1e-3 {
        return Err(format!(
            "{what} {nm} nm is not a multiple of 0.0001 nm (0.1 pm), so it has no canonical form. It is not rounded, because a rounded window is not the window that was set: write it to 0.0001 nm."
        ));
    }
    Ok(r as u32)
}

impl ResolvedSet {
    /// The resolved set of a rules table, or the refusal for a value off the 0.1 pm grid.
    pub fn from_rules(rules: &WavelengthRules) -> Result<ResolvedSet, String> {
        let mut channels = [None; 3];
        for (i, el) in Channel::ALL.into_iter().enumerate() {
            if let Some(r) = rules.channels.iter().find(|c| c.element == el) {
                let w = |k: &str, v: f64| nm_to_units(&format!("{} {k}", el.name()), v);
                channels[i] = Some(Window { nominal: w("nominal", r.nominal_nm)?, min: w("minimum", r.min_nm)?, max: w("maximum", r.max_nm)? });
            }
        }
        Ok(ResolvedSet { channels })
    }

    /// The canonical form, without the domain line.
    pub fn canonical(&self) -> String {
        let parts: Vec<String> = Channel::ALL
            .into_iter()
            .zip(self.channels)
            .map(|(el, w)| match w {
                Some(w) => format!(
                    "{{\"element\":\"{}\",\"max_pm_x10\":{},\"min_pm_x10\":{},\"nominal_pm_x10\":{}}}",
                    el.name(),
                    w.max,
                    w.min,
                    w.nominal
                ),
                None => format!("{{\"element\":\"{}\",\"max_pm_x10\":null,\"min_pm_x10\":null,\"nominal_pm_x10\":null}}", el.name()),
            })
            .collect();
        format!("{{\"channels\":[{}]}}", parts.join(","))
    }

    /// SHA-256 of the domain line and the canonical form, as 64 lowercase hex digits.
    pub fn hash(&self) -> String {
        let mut h = Sha256::new();
        h.update(DOMAIN.as_bytes());
        h.update(self.canonical().as_bytes());
        h.finalize().iter().map(|b| format!("{b:02x}")).collect()
    }

    /// The 36 bytes of windows.
    pub fn windows_bytes(&self) -> [u8; WINDOWS_LEN] {
        let mut out = [0u8; WINDOWS_LEN];
        for (i, w) in self.channels.iter().enumerate() {
            if let Some(w) = w {
                for (j, v) in [w.nominal, w.min, w.max].into_iter().enumerate() {
                    out[i * 12 + j * 4..i * 12 + j * 4 + 4].copy_from_slice(&v.to_le_bytes());
                }
            }
        }
        out
    }

    /// The set a stamp's 36 bytes hold. A channel is twelve zero bytes (no rule) or three non-zero values.
    pub fn from_windows_bytes(b: &[u8]) -> Result<ResolvedSet, String> {
        if b.len() != WINDOWS_LEN {
            return Err(format!("the windows are {WINDOWS_LEN} bytes, not {}", b.len()));
        }
        let mut channels = [None; 3];
        for (i, c) in b.chunks(12).enumerate() {
            let v: Vec<u32> = c.chunks(4).map(|x| u32::from_le_bytes([x[0], x[1], x[2], x[3]])).collect();
            if v.iter().all(|x| *x == 0) {
                continue;
            }
            if v.iter().any(|x| *x == 0) {
                return Err(format!("channel {} has some zero values and some not: a window has all three or none", Channel::ALL[i].name()));
            }
            channels[i] = Some(Window { nominal: v[0], min: v[1], max: v[2] });
        }
        Ok(ResolvedSet { channels })
    }
}

/// Where a stamp's set comes from.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Origin {
    /// The upload stated no stamp (an app built before the scheme). Release and windows are zero.
    NotStated = 0,
    /// A set NeurOne released; the release is the one stated.
    Release = 1,
    /// A set the warranty owner edited; the release is the one it was merged over.
    Owner = 2,
}

/// What the session record holds (`SWR-HV-02`, `SWR-HV-03`).
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct Stamp {
    pub origin: Origin,
    pub major: u8,
    pub minor: u8,
    pub windows: [u8; WINDOWS_LEN],
}

impl Stamp {
    /// The record for an upload that states none: origin `0` and zeros, never an invented version.
    pub const NOT_STATED: Stamp = Stamp { origin: Origin::NotStated, major: 0, minor: 0, windows: [0; WINDOWS_LEN] };

    pub fn encode(&self) -> [u8; STAMP_LEN] {
        let mut out = [0u8; STAMP_LEN];
        out[0] = self.origin as u8;
        out[1] = self.major;
        out[2] = self.minor;
        out[3..].copy_from_slice(&self.windows);
        out
    }

    pub fn decode(b: &[u8]) -> Result<Stamp, String> {
        if b.len() != STAMP_LEN {
            return Err(format!("a wavelength-rules stamp is {STAMP_LEN} bytes, not {}", b.len()));
        }
        let origin = match b[0] {
            0 => Origin::NotStated,
            1 => Origin::Release,
            2 => Origin::Owner,
            o => return Err(format!("unknown wavelength-rules stamp origin {o}")),
        };
        let mut windows = [0u8; WINDOWS_LEN];
        windows.copy_from_slice(&b[3..]);
        if origin == Origin::NotStated && (b[1] != 0 || b[2] != 0 || windows.iter().any(|x| *x != 0)) {
            return Err("a stamp that states nothing carries zero release and zero windows".into());
        }
        Ok(Stamp { origin, major: b[1], minor: b[2], windows })
    }

    /// The set the stamp's windows hold, or `None` when it states none.
    pub fn set(&self) -> Result<Option<ResolvedSet>, String> {
        match self.origin {
            Origin::NotStated => Ok(None),
            _ => ResolvedSet::from_windows_bytes(&self.windows).map(Some),
        }
    }
}

/// One row of the register.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Release {
    pub major: u8,
    pub minor: u8,
    pub hash: String,
}

/// A release version `major.minor`, both whole numbers that fit a release byte (the stamp's two bytes).
pub fn parse_release_version(v: &str) -> Result<(u8, u8), String> {
    let (a, b) = v.split_once('.').ok_or_else(|| format!("version '{v}' is not major.minor"))?;
    let n = |s: &str| {
        if s.is_empty() || !s.chars().all(|c| c.is_ascii_digit()) || (s.len() > 1 && s.starts_with('0')) {
            return Err(format!("version '{v}' is not major.minor in whole numbers"));
        }
        s.parse::<u8>().map_err(|_| format!("version '{v}' does not fit a release byte"))
    };
    Ok((n(a)?, n(b)?))
}

/// Every released set, oldest first, from the register.
pub fn releases() -> Result<Vec<Release>, String> {
    let v: Value = serde_json::from_str(REGISTER).map_err(|e| format!("the register is not JSON: {e}"))?;
    v["releases"]
        .as_array()
        .ok_or("the register has no releases array")?
        .iter()
        .map(|r| {
            let (major, minor) = parse_release_version(r["version"].as_str().ok_or("a register row has no version")?)?;
            let hash = r["hash"].as_str().ok_or("a register row has no hash")?.to_string();
            Ok(Release { major, minor, hash })
        })
        .collect()
}

/// Everything wrong with the register as a whole: a hash that is not 64 lowercase hex digits, a repeated hash,
/// a version that does not go up, or no rows at all.
pub fn register_problems() -> Vec<String> {
    let rows = match releases() {
        Ok(r) => r,
        Err(e) => return vec![e],
    };
    let mut out = Vec::new();
    if rows.is_empty() {
        out.push("the register has no release".to_string());
    }
    for (i, r) in rows.iter().enumerate() {
        if r.hash.len() != 64 || !r.hash.chars().all(|c| c.is_ascii_digit() || ('a'..='f').contains(&c)) {
            out.push(format!("row {} ({}.{}): the hash is not 64 lowercase hex digits", i + 1, r.major, r.minor));
        }
        if rows[..i].iter().any(|p| p.hash == r.hash) {
            out.push(format!("row {} ({}.{}): the hash is already released", i + 1, r.major, r.minor));
        }
        if i > 0 && (r.major, r.minor) <= (rows[i - 1].major, rows[i - 1].minor) {
            out.push(format!("row {} ({}.{}): a version never goes down and is never reused", i + 1, r.major, r.minor));
        }
    }
    out
}

/// The release a set is, if its hash is in the register (whatever its level says).
pub fn release_of(set: &ResolvedSet) -> Result<Option<(u8, u8)>, String> {
    let h = set.hash();
    Ok(releases()?.iter().find(|r| r.hash == h).map(|r| (r.major, r.minor)))
}

/// The record for a set in force: its release when the hash is in the register, otherwise an owner set merged over
/// the newest release.
pub fn stamp_for(set: &ResolvedSet) -> Result<Stamp, String> {
    let rows = releases()?;
    let latest = rows.last().ok_or("the register has no release")?;
    let h = set.hash();
    let (origin, major, minor) = match rows.iter().find(|r| r.hash == h) {
        Some(r) => (Origin::Release, r.major, r.minor),
        None => (Origin::Owner, latest.major, latest.minor),
    };
    Ok(Stamp { origin, major, minor, windows: set.windows_bytes() })
}

/// What a reader makes of a stamp (§7a.1, failure cases). The windows are always the truth.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Reading {
    /// The upload stated none.
    NotStated,
    /// The windows are exactly this release.
    Release { major: u8, minor: u8 },
    /// An owner set, merged over this release.
    OwnerSet { base_major: u8, base_minor: u8 },
    /// It claims a release this build does not know. Show the stated version and the windows, and say so.
    UnknownRelease { major: u8, minor: u8 },
    /// It claims a release the windows are not. Trust the windows, show an owner set, and note the mismatch.
    Mismatch { claimed_major: u8, claimed_minor: u8 },
}

pub fn read_stamp(s: &Stamp) -> Result<Reading, String> {
    if s.origin == Origin::NotStated {
        return Ok(Reading::NotStated);
    }
    let set = ResolvedSet::from_windows_bytes(&s.windows)?;
    let actual = release_of(&set)?;
    let stated = (s.major, s.minor);
    Ok(match (s.origin, actual) {
        (_, Some((major, minor))) if (major, minor) == stated || s.origin == Origin::Owner => Reading::Release { major, minor },
        (Origin::Owner, None) => Reading::OwnerSet { base_major: s.major, base_minor: s.minor },
        (_, Some(_)) => Reading::Mismatch { claimed_major: s.major, claimed_minor: s.minor },
        (_, None) if releases()?.iter().any(|r| (r.major, r.minor) == stated) => Reading::Mismatch { claimed_major: s.major, claimed_minor: s.minor },
        (_, None) => Reading::UnknownRelease { major: s.major, minor: s.minor },
    })
}

/// How a change of rules compares for delivery (§7a.1, the floor).
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Change {
    /// The resolved sets are equal (a name or description edit changes nothing here).
    None,
    /// Every wavelength the old set delivered, the new set delivers on the same channel. It may accept more.
    Minor,
    /// A wavelength the old set delivered is now refused or goes to another channel.
    Major,
}

const SCALE: u64 = 8;

fn channel_for(set: &ResolvedSet, x: u64) -> Option<usize> {
    let mut best: Option<(usize, u64)> = None;
    for (i, w) in set.channels.iter().enumerate() {
        let Some(w) = w else { continue };
        if x < u64::from(w.min) * SCALE || x > u64::from(w.max) * SCALE {
            continue;
        }
        let d = x.abs_diff(u64::from(w.nominal) * SCALE);
        // A tie goes to the earlier channel, so the answer never depends on the order rules were written in.
        if best.map_or(true, |(_, bd)| d < bd) {
            best = Some((i, d));
        }
    }
    best.map(|(i, _)| i)
}

/// The floor of §7a.1, computed exactly. Resolution is constant between its breakpoints (every minimum, every
/// maximum, and the midpoint of two nominals), so it is tested at each breakpoint of either set and between each
/// pair of neighbours.
pub fn delivery_change(old: &ResolvedSet, new: &ResolvedSet) -> Change {
    if old == new {
        return Change::None;
    }
    let mut pts: Vec<u64> = Vec::new();
    for s in [old, new] {
        for w in s.channels.iter().flatten() {
            pts.push(u64::from(w.min) * SCALE);
            pts.push(u64::from(w.max) * SCALE);
        }
        for (i, a) in s.channels.iter().enumerate() {
            for b in s.channels.iter().skip(i + 1) {
                if let (Some(a), Some(b)) = (a, b) {
                    pts.push((u64::from(a.nominal) + u64::from(b.nominal)) * (SCALE / 2));
                }
            }
        }
    }
    pts.sort_unstable();
    pts.dedup();
    let mut probes = pts.clone();
    probes.extend(pts.windows(2).map(|p| (p[0] + p[1]) / 2));
    for x in probes {
        if let Some(c) = channel_for(old, x) {
            if channel_for(new, x) != Some(c) {
                return Change::Major;
            }
        }
    }
    Change::Minor
}
