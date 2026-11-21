//! The active-user tag an app names to a helmet (`REQ-FAULTMSG-01`, NP-SW-FAULTMSG-001 §9.2,
//! `OI-PRIV-29` (NP-PRIV-ANALYSIS-004), GitHub #957).
//!
//! WHY THIS EXISTS. The safety MCU holds a cervical VNS cardiac cutoff for one person, named by an opaque
//! 32-bit tag the app sends over `ACTIVE_USER` (`np_safety_user_cmd_t`) and keeps in its flash
//! (`np_nv_state.c`). Both apps used to send the first four bytes of the profile UUID. That value is the same
//! on every helmet the person uses, sits in plaintext MCU flash, and joins to the profile id written into
//! exported files. Because it tells us about the person, it is UHDR-class (CLAUDE.md §5.1).
//!
//! WHAT THIS DOES. [`derive`] gives a tag that differs per (profile, helmet) and cannot be computed from the
//! profile id: HMAC-SHA-256 keyed with a per-profile secret over a domain string and the helmet's serial.
//! The same secret on another phone gives the same tag on the same helmet, so a hold follows the person, not
//! the phone, as long as the secret travels with the profile (it is profile data and is backed up with it).
//! [`legacy`] reproduces the old derivation for migration only. [`migration_step`] is the rule that keeps
//! a held cutoff held across the change of derivation (`REQ-FAULTMSG-01` (c)).
//!
//! WHAT THIS DOES NOT DO. It holds no secret and draws no entropy: the host supplies the 32-byte profile
//! secret from its own CSPRNG, as everywhere in this crate. It is not wired into either app yet: the wiring
//! waits on the Safety brief `docs/human-input/oi-priv-29-active-user-tag-safety.md`, whose answer may change
//! what the tag has to carry.

use hmac::{Hmac, Mac};
use sha2::Sha256;

/// Bytes in a profile tag secret. RFC 2104 §3: a key shorter than the hash output (32 bytes for SHA-256)
/// weakens the MAC, and a longer one adds nothing.
pub const PROFILE_SECRET_LEN: usize = 32;

/// Domain separation, so this secret's MAC can never be mistaken for one computed for another purpose.
pub const DOMAIN: &[u8] = b"NeurOne.ActiveUserTag.1";

/// `NP_SAFETY_USER_UNSPECIFIED`: no user named. Reserved; the MCU refuses it.
pub const USER_UNSPECIFIED: u32 = 0;
/// `NP_SAFETY_USER_ANY`: an unattributable cutoff. Reserved; the MCU refuses it.
pub const USER_ANY: u32 = 0xFFFF_FFFF;

/// Map a 32-bit value off the two reserved values, exactly as the legacy derivation did.
fn avoid_reserved(tag: u32) -> u32 {
    match tag {
        USER_UNSPECIFIED => 1,
        USER_ANY => 0xFFFF_FFFE,
        t => t,
    }
}

/// The tag for this profile on this helmet: the first four bytes, little-endian, of
/// HMAC-SHA-256(`profile_secret`, `DOMAIN` ‖ len(serial) ‖ serial), off the reserved values.
/// The serial is length-prefixed so that no two (serial) inputs share a message.
pub fn derive(profile_secret: &[u8; PROFILE_SECRET_LEN], helmet_serial: &[u8]) -> Result<u32, String> {
    if helmet_serial.is_empty() {
        return Err("a helmet serial is required: a tag with no helmet would be the same on every helmet".into());
    }
    let len = u16::try_from(helmet_serial.len()).map_err(|_| "helmet serial is too long".to_string())?;
    let mut mac = <Hmac<Sha256> as Mac>::new_from_slice(profile_secret).map_err(|e| e.to_string())?;
    mac.update(DOMAIN);
    mac.update(&len.to_be_bytes());
    mac.update(helmet_serial);
    let out = mac.finalize().into_bytes();
    Ok(avoid_reserved(u32::from_le_bytes([out[0], out[1], out[2], out[3]])))
}

/// The tag both apps sent before `OI-PRIV-29`: the profile UUID's first four bytes, little-endian, off the
/// reserved values. For migration only (`migration_step`); never a tag to give a profile that has none.
pub fn legacy(profile_uuid: &[u8; 16]) -> u32 {
    avoid_reserved(u32::from_le_bytes([profile_uuid[0], profile_uuid[1], profile_uuid[2], profile_uuid[3]]))
}

/// What the app names to a helmet for a profile that may have a cutoff held under its legacy tag.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Step {
    /// Keep naming the legacy tag: a cutoff may be held under it.
    NameLegacy,
    /// Name the derived tag. The app records the (profile, helmet) pair as migrated and never names the
    /// legacy tag to that helmet again.
    NameDerived,
}

/// The migration rule (`REQ-FAULTMSG-01` (c)). `migrated`: this (profile, helmet) pair already moved.
/// `device_outstanding`: the helmet's "a cutoff is outstanding for someone" flag
/// (`NP_SAFETY_NV_FLAG_OUTSTANDING`, bit 1 of the `CVNS_FAULT_STATUS` flags byte) from a frame the hub has
/// published from a valid safety-MCU report; `None` when no such reading is in hand.
///
/// Why the device-wide flag and not the per-user one: the helmet does not report which tag is its current
/// user, so a "user blocked" reading cannot be tied to the legacy tag. When nobody on the helmet has an
/// outstanding cutoff, no tag carries a hold and changing tag can release none. Any other reading keeps the
/// legacy tag: moving would present a tag with no hold and release the person's cutoff (`REQ-CVNS-09`).
pub fn migration_step(migrated: bool, device_outstanding: Option<bool>) -> Step {
    match (migrated, device_outstanding) {
        (true, _) => Step::NameDerived,
        (false, Some(false)) => Step::NameDerived,
        (false, _) => Step::NameLegacy,
    }
}

// Tests: common/npps-core/tests/active_user_tag.rs (req-trace scans tests/, not src/).
