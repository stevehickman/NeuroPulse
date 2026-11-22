//! `REQ-FAULTMSG-01`: the per-(profile, helmet) active-user tag and its migration rule
//! (`OI-PRIV-29` (NP-PRIV-ANALYSIS-004), GitHub #957). Module: `src/active_user_tag.rs`.

use neurone_npps_core::active_user_tag::*;

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
    let mut u = [0u8; 16];
    u[..4].copy_from_slice(&0x1234_5678u32.to_le_bytes());
    assert_eq!(legacy(&u), 0x1234_5678, "an unreserved value is left alone");
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
