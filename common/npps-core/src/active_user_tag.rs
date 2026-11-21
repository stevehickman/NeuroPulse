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

#[cfg(test)]
mod tests {
    use super::*;

    const SECRET_A: [u8; 32] = [0x11; 32];
    const SECRET_B: [u8; 32] = [0x22; 32];

    // @verifies REQ-FAULTMSG-01#a
    #[test]
    fn derived_tag_differs_per_helmet_and_per_profile_and_is_stable() {
        let a1 = derive(&SECRET_A, b"NP-0001").unwrap();
        let a2 = derive(&SECRET_A, b"NP-0002").unwrap();
        let b1 = derive(&SECRET_B, b"NP-0001").unwrap();
        assert_ne!(a1, a2, "one profile, two helmets: two tags");
        assert_ne!(a1, b1, "two profiles, one helmet: two tags");
        assert_eq!(a1, derive(&SECRET_A, b"NP-0001").unwrap(), "the same secret on another phone gives the same tag");
    }

    // @verifies REQ-FAULTMSG-01#a
    #[test]
    fn derived_tag_matches_a_reference_hmac_and_is_not_the_uuid_prefix() {
        // Reference vector computed independently (Python `hmac.new(b"\x11"*32,
        // b"NeurOne.ActiveUserTag.1\x00\x07NP-0001", hashlib.sha256)`, first four bytes little-endian), so a
        // change to the key, the domain or the message layout fails here.
        assert_eq!(derive(&SECRET_A, b"NP-0001").unwrap(), 0x25DD_4FD5);
        assert_ne!(derive(&SECRET_A, b"NP-0001").unwrap(), legacy(&[0x11; 16]));
    }

    // @verifies REQ-FAULTMSG-01#a
    #[test]
    fn a_tag_with_no_helmet_is_refused() {
        assert!(derive(&SECRET_A, b"").is_err());
    }

    // @verifies REQ-FAULTMSG-01#b
    #[test]
    fn reserved_values_are_never_produced() {
        assert_eq!(avoid_reserved(USER_UNSPECIFIED), 1);
        assert_eq!(avoid_reserved(USER_ANY), 0xFFFF_FFFE);
        assert_eq!(avoid_reserved(0x1234_5678), 0x1234_5678);
        assert_eq!(legacy(&[0; 16]), 1);
        assert_eq!(legacy(&[0xFF; 16]), 0xFFFF_FFFE);
        for i in 0u8..=255 {
            let t = derive(&SECRET_A, &[i]).unwrap();
            assert!(t != USER_UNSPECIFIED && t != USER_ANY);
        }
    }

    // @verifies REQ-FAULTMSG-01#b
    #[test]
    fn legacy_matches_the_apps_old_derivation() {
        // Same vector as GATTParserTests.testActiveUserTagAvoidsReservedValues and CervicalFaultStatusTests:
        // UUID 04030201-… gives 0x01020304.
        let mut u = [0u8; 16];
        u[..4].copy_from_slice(&[0x04, 0x03, 0x02, 0x01]);
        assert_eq!(legacy(&u), 0x0102_0304);
    }

    // @verifies REQ-FAULTMSG-01#c
    #[test]
    fn migration_moves_only_when_nobody_on_the_helmet_is_held() {
        assert_eq!(migration_step(false, Some(true)), Step::NameLegacy, "someone held, maybe this person: stay");
        assert_eq!(migration_step(false, None), Step::NameLegacy, "no valid reading: stay");
        assert_eq!(migration_step(false, Some(false)), Step::NameDerived, "nobody held: no hold can be released");
        assert_eq!(migration_step(true, Some(true)), Step::NameDerived, "already moved: the derived tag holds it now");
        assert_eq!(migration_step(true, None), Step::NameDerived);
    }
}
