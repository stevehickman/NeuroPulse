//! Stage 2 of vetting a protocol: can it run on the modules fitted in the sockets right now (`OI-HWFIT-01`).
//!
//! Stage 1 (is the protocol legal) is the parser, validator and compiler. This takes the **compiled descriptor** and a
//! socket inventory, and says per socket which element types the descriptor needs that the fitted module does not
//! carry, with the module designs that would supply each one. It reads the same bytes the hub reads, and derives the
//! need the way the hub's placement check does (`np_socket_dispatch.c`, `required_emitters`): a base PBM command needs the
//! emitter whose current register is non-zero, a smart one needs the channels in its `ch_mask`. Only a socket-addressed
//! PBM command names sockets; every other modality is slot-addressed and has nothing to fit here.
//!
//! **The answer is advice.** The hub's placement check stays the authority and fails closed when the inventory changes
//! between this check and the session start. The module designs are `common/npps/module-designs.json`.

use crate::compiler::{CMD_HDR_LEN, HEADER_LEN, MAGIC, MOD_PBM_BASE, MOD_PBM_SMART, TARGET_SOCKET_MASK};
use serde_json::Value;
use std::collections::{BTreeMap, BTreeSet, HashMap};

const DESIGNS: &str = include_str!("../../npps/module-designs.json");

/// One element type a socket needs and does not have, and the module designs that carry it.
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct Missing {
    pub element: String,
    pub candidate_parts: Vec<String>,
}

/// A socket the protocol drives that cannot supply everything it asks for.
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct Shortfall {
    pub socket: u32,
    /// True when the socket holds no module at all (it is absent from the inventory, or its element list is empty).
    pub empty: bool,
    pub missing: Vec<Missing>,
}

/// The element names carried by the module design behind a hub tile type (`wireTypes` in `module-designs.json`). A type with
/// no entry (absent, unknown, or one this build does not know) carries none, so a socket holding it is treated as empty.
pub fn elements_of_wire_type(wire_type: u64) -> Vec<String> {
    let d = designs();
    d["wireTypes"][wire_type.to_string()]
        .as_str()
        .and_then(|part| d["modules"][part]["elements"].as_array())
        .map(|l| l.iter().filter_map(|e| e.as_str().map(String::from)).collect())
        .unwrap_or_default()
}

/// Sockets that fall short in the same way, as one group: forty empty sockets that all lack a 1064 nm emitter are one line
/// for a person to read, not forty. Groups are in order of their lowest socket; sockets within one are ascending.
pub fn group_shortfalls(shortfalls: &[Shortfall]) -> Vec<(Vec<u32>, Vec<Missing>)> {
    let mut groups: Vec<(Vec<u32>, Vec<Missing>)> = vec![];
    for s in shortfalls {
        match groups.iter_mut().find(|(_, m)| *m == s.missing) {
            Some((sockets, _)) => sockets.push(s.socket),
            None => groups.push((vec![s.socket], s.missing.clone())),
        }
    }
    groups
}

fn designs() -> Value {
    serde_json::from_str(DESIGNS).expect("module-designs.json is valid JSON")
}

/// The element names a PBM command asks the socket's module to light.
fn emitters(mod_type: u8, params: &[u8]) -> Result<Vec<&'static str>, String> {
    let at = |i: usize| params.get(i).copied().ok_or_else(|| "A PBM command's parameters are shorter than the descriptor format.".to_string());
    let mut out = vec![];
    if mod_type == MOD_PBM_BASE {
        if at(2)? != 0 { out.push("led_660"); }
        if at(3)? != 0 { out.push("led_808"); }
    } else {
        let mask = at(5)?;
        if mask & !0x07 != 0 {
            return Err("A smart PBM command names a channel no module has.".into());
        }
        for (bit, name) in [(0x01, "led_660"), (0x02, "led_808"), (0x04, "led_1064")] {
            if mask & bit != 0 { out.push(name); }
        }
    }
    Ok(out)
}

/// The element types each socket must carry for the whole descriptor to run, from the descriptor's socket-addressed PBM
/// commands. A STOP command (no parameters) needs nothing.
pub fn needed_elements(blob: &[u8]) -> Result<BTreeMap<u32, BTreeSet<&'static str>>, String> {
    let bad = || "The descriptor is shorter than its commands say.".to_string();
    if blob.len() < HEADER_LEN || u32::from_le_bytes(blob[0..4].try_into().unwrap()) != MAGIC {
        return Err("Not a protocol descriptor.".into());
    }
    let mut need: BTreeMap<u32, BTreeSet<&'static str>> = BTreeMap::new();
    let mut off = HEADER_LEN;
    for _ in 0..blob[7] {
        let hdr = blob.get(off..off + CMD_HDR_LEN).ok_or_else(bad)?;
        let (mod_type, params_len, kind, block_len) = (hdr[0], u16::from_le_bytes([hdr[10], hdr[11]]) as usize, hdr[12], hdr[13] as usize);
        off += CMD_HDR_LEN;
        let block = blob.get(off..off + block_len).ok_or_else(bad)?;
        off += block_len;
        let params = blob.get(off..off + params_len).ok_or_else(bad)?;
        off += params_len;
        if kind != TARGET_SOCKET_MASK || params.is_empty() || !matches!(mod_type, MOD_PBM_BASE | MOD_PBM_SMART) {
            continue;
        }
        let lit = emitters(mod_type, params)?;
        for (i, byte) in block.iter().enumerate() {
            for bit in 0..8 {
                if byte >> bit & 1 == 1 {
                    // The compiler writes socket id `n` at bit `n - 1`.
                    need.entry((i * 8 + bit + 1) as u32).or_default().extend(lit.iter().copied());
                }
            }
        }
    }
    Ok(need)
}

/// Per socket, what the descriptor needs that the inventory does not supply. `inventory` maps a socket id to the element
/// names its fitted module carries; a socket not in it is empty. An empty result means every socket can run it.
pub fn check_fit(blob: &[u8], inventory: &HashMap<u32, Vec<String>>) -> Result<Vec<Shortfall>, String> {
    let designs = designs();
    let modules = designs["modules"].as_object().expect("module-designs.json has modules");
    let mut out = vec![];
    for (socket, need) in needed_elements(blob)? {
        let fitted = inventory.get(&socket).map(Vec::as_slice).unwrap_or(&[]);
        let missing: Vec<Missing> = need
            .iter()
            .filter(|e| !fitted.iter().any(|f| f == *e))
            .map(|e| Missing {
                element: (*e).to_string(),
                candidate_parts: modules
                    .iter()
                    .filter(|(_, m)| m["elements"].as_array().map_or(false, |l| l.iter().any(|x| x == e)))
                    .map(|(part, _)| part.clone())
                    .collect(),
            })
            .collect();
        if !missing.is_empty() {
            out.push(Shortfall { socket, empty: fitted.is_empty(), missing });
        }
    }
    Ok(out)
}
