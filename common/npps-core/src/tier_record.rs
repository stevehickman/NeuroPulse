//! The tier-identity record and the host-checkable stages of the factory step (`REQ-UPG-04`,
//! `NP-REG-UPG-001` §7.9, `OI-UPG-08` (ii) and (iii), GitHub #569 package 8).
//!
//! WHAT THIS IS. Pure functions over bytes: compose the signed body, compose the message the tier authority
//! signs, assemble the 72-byte record from a signature the authority returned, verify a record exactly as the
//! safety MCU does (`firmware/safety_mcu/src/np_tier_identity.c`, SW01-M10), and decide the factory step's
//! refusals (a non-blank window, an unreleased or mismatched hardware version, a unit whose own tier report
//! does not read `OK` with the build record's tier and version).
//!
//! WHAT THIS IS NOT. It holds no secret and contains no signer. Who signs, where the key lives, the list of
//! released hardware versions, the board-revision to hardware-version map, how the programming fixture reads
//! and writes OTP, and the read-out-protection setting are all undecided (`OI-UPG-08`; §7.9 "Not decided"), so
//! each is an INPUT here, never a choice made here. Signing appears only in the tests, with the test
//! authority's seed. No production key may be placed in this crate.
//!
//! The record layout is the C header's, restated (`np_safety_config.h`); `tests` pins this module to vectors
//! produced by the firmware's own Monocypher signer, so a drift in either side fails a test.

use ed25519_dalek::{Signature, Verifier, VerifyingKey};

/// Bytes in the whole record, OTP offset `0x40` (`NP_TIER_RECORD_LEN`).
pub const RECORD_LEN: usize = 72;
/// Signed body: magic, version, byte 5, two reserved bytes (`NP_TIER_RECORD_BODY_LEN`).
pub const BODY_LEN: usize = 8;
/// Factory UID of the STM32G0 part (`NP_DEVICE_UID_LEN`).
pub const UID_LEN: usize = 12;
/// Signing-domain string (`NP_TIER_SIG_DOMAIN`).
pub const DOMAIN: &[u8; 16] = b"NeurOne.TierId.2";
/// Domain ‖ body ‖ UID (`NP_TIER_SIG_MSG_LEN`).
pub const SIG_MSG_LEN: usize = 16 + BODY_LEN + UID_LEN;
/// Record layout version (`NP_TIER_RECORD_VERSION`).
pub const RECORD_VERSION: u8 = 0x02;
pub const TIER_T1: u8 = 0x01;
pub const TIER_T2: u8 = 0x02;
/// At most 16 hardware versions, `0` meaning none (`UC-080`).
pub const HW_VERSION_MAX: u8 = 15;
/// STM32G0 OTP programs in 64-bit double words, so the record is nine of them.
pub const DOUBLE_WORDS: usize = RECORD_LEN / 8;
/// Tier report the unit sends (`np_safety_tier_report_t`).
pub const REPORT_LEN: usize = 7;
const REPORT_MAGIC: u8 = 0x7D;

/// `NP_TIER_REASON_*`: why the unit reports the tier it does.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Reason {
    Ok,
    NoAuthority,
    Blank,
    Format,
    Signature,
}

impl Reason {
    pub fn code(self) -> u8 {
        match self {
            Reason::Ok => 0,
            Reason::NoAuthority => 1,
            Reason::Blank => 2,
            Reason::Format => 3,
            Reason::Signature => 4,
        }
    }
    pub fn from_code(c: u8) -> Option<Reason> {
        Some(match c {
            0 => Reason::Ok,
            1 => Reason::NoAuthority,
            2 => Reason::Blank,
            3 => Reason::Format,
            4 => Reason::Signature,
            _ => return None,
        })
    }
}

/// What the safety MCU concludes from a window and a UID: the tier it enforces, why, and the hardware version
/// (1, the default, unless a record verified for this unit).
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct Verdict {
    pub tier: u8,
    pub reason: Reason,
    pub hw_version: u8,
}

/// Why the factory step refuses a unit. Each ends the unit's run; nothing is retried on the same window.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Refusal {
    /// A tier code other than T1 or T2.
    TierNotValid,
    /// Hardware version 0 or above 15.
    VersionOutOfRange,
    /// Step 2: the OTP window is not in the erased state; the unit is quarantined.
    WindowNotBlank,
    /// Step 3: the build record names a hardware version the programme has not released.
    VersionNotReleased,
    /// Step 3: the build record's hardware version disagrees with the version the board's revision maps to.
    VersionBoardMismatch,
    /// Step 3: a T2 record is signed only against a T2 build record.
    TierNotInBuildRecord,
    /// Step 4: the authority's signature is not 64 bytes.
    SignatureLength,
    /// Step 5: a double word read back differs from what was written.
    ReadBackMismatch,
    /// Step 6: the report is not a well-formed 7-byte report (length, magic, checksum, codes).
    ReportMalformed,
    /// Step 6: the unit's reason is not `OK`.
    ReasonNotOk(Reason),
    /// Step 6: the unit reports a different tier than the build record's.
    TierMismatch,
    /// Step 6: the unit reports a different hardware version than the build record's.
    HwVersionMismatch,
}

/// What the build record says about the unit. The tier and version are the programme's; the board's version
/// is what the hardware design's board-revision map gives (the map is undecided, §7.9 step 3, so a caller
/// passes its result).
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct BuildRecord {
    pub tier: u8,
    pub hw_version: u8,
}

fn check_fields(tier: u8, hw: u8) -> Result<(), Refusal> {
    if tier != TIER_T1 && tier != TIER_T2 {
        return Err(Refusal::TierNotValid);
    }
    if hw == 0 || hw > HW_VERSION_MAX {
        return Err(Refusal::VersionOutOfRange);
    }
    Ok(())
}

/// Step 4, first half: bytes 0 to 7 of the record.
pub fn record_body(tier: u8, hw_version: u8) -> Result<[u8; BODY_LEN], Refusal> {
    check_fields(tier, hw_version)?;
    Ok([b'N', b'P', b'T', b'I', RECORD_VERSION, (hw_version << 4) | tier, 0, 0])
}

/// Step 4, second half: what the authority signs. The UID is the one read from the unit in this run.
pub fn signing_message(body: &[u8; BODY_LEN], uid: &[u8; UID_LEN]) -> [u8; SIG_MSG_LEN] {
    let mut m = [0u8; SIG_MSG_LEN];
    m[..16].copy_from_slice(DOMAIN);
    m[16..16 + BODY_LEN].copy_from_slice(body);
    m[16 + BODY_LEN..].copy_from_slice(uid);
    m
}

/// Join the body and the 64-byte signature the authority returned into the 72 bytes to program.
pub fn assemble(body: &[u8; BODY_LEN], signature: &[u8]) -> Result<[u8; RECORD_LEN], Refusal> {
    if signature.len() != 64 {
        return Err(Refusal::SignatureLength);
    }
    let mut r = [0u8; RECORD_LEN];
    r[..BODY_LEN].copy_from_slice(body);
    r[BODY_LEN..].copy_from_slice(signature);
    Ok(r)
}

/// Step 2: the window must be the erased state (all `0xFF`) and exactly 72 bytes.
pub fn check_blank(window: &[u8]) -> Result<(), Refusal> {
    if window.len() == RECORD_LEN && window.iter().all(|&b| b == 0xFF) {
        Ok(())
    } else {
        Err(Refusal::WindowNotBlank)
    }
}

/// Step 3: the build record against the released list and the board. `released` and `board_hw_version` are
/// inputs because who maintains the list and how the board's revision is read are undecided.
pub fn check_build_record(
    build: BuildRecord,
    released: &[u8],
    board_hw_version: u8,
) -> Result<(), Refusal> {
    check_fields(build.tier, build.hw_version)?;
    if !released.contains(&build.hw_version) {
        return Err(Refusal::VersionNotReleased);
    }
    if build.hw_version != board_hw_version {
        return Err(Refusal::VersionBoardMismatch);
    }
    Ok(())
}

/// Step 5: the nine 64-bit double words, in programming order.
pub fn double_words(record: &[u8; RECORD_LEN]) -> [[u8; 8]; DOUBLE_WORDS] {
    let mut out = [[0u8; 8]; DOUBLE_WORDS];
    for (i, w) in out.iter_mut().enumerate() {
        w.copy_from_slice(&record[i * 8..i * 8 + 8]);
    }
    out
}

/// Step 5: the window as read back must equal the record written.
pub fn check_read_back(record: &[u8; RECORD_LEN], read_back: &[u8]) -> Result<(), Refusal> {
    if read_back == record.as_slice() {
        Ok(())
    } else {
        Err(Refusal::ReadBackMismatch)
    }
}

fn t1_verdict(reason: Reason) -> Verdict {
    Verdict { tier: TIER_T1, reason, hw_version: 1 }
}

/// What the safety MCU reaches from `authority` (None, or all zero: the image has no key), the window and the
/// unit's UID. The order of the checks is the firmware's, so the reason is the first thing wrong. Fail closed:
/// everything but a record that verifies for this UID is T1 at the default version.
pub fn verify_record(authority: Option<&[u8; 32]>, window: &[u8], uid: &[u8; UID_LEN]) -> Verdict {
    let key = match authority {
        Some(k) if k.iter().any(|&b| b != 0) => k,
        _ => return t1_verdict(Reason::NoAuthority),
    };
    if window.len() != RECORD_LEN {
        return t1_verdict(Reason::Format);
    }
    if window.iter().all(|&b| b == 0xFF) {
        return t1_verdict(Reason::Blank);
    }
    let tier = window[5] & 0x0F;
    let hw = window[5] >> 4;
    if window[..4] != *b"NPTI"
        || window[4] != RECORD_VERSION
        || window[6] != 0
        || window[7] != 0
        || (tier != TIER_T1 && tier != TIER_T2)
        || hw == 0
    {
        return t1_verdict(Reason::Format);
    }
    let mut body = [0u8; BODY_LEN];
    body.copy_from_slice(&window[..BODY_LEN]);
    let msg = signing_message(&body, uid);
    let ok = VerifyingKey::from_bytes(key).ok().is_some_and(|vk| {
        let mut sig = [0u8; 64];
        sig.copy_from_slice(&window[BODY_LEN..]);
        // Plain `verify`, as Monocypher's `crypto_ed25519_check` (no extra rejections of `verify_strict`).
        vk.verify(&msg, &Signature::from_bytes(&sig)).is_ok()
    });
    if !ok {
        return t1_verdict(Reason::Signature);
    }
    Verdict { tier, reason: Reason::Ok, hw_version: hw }
}

/// The unit's tier report, parsed. Window bytes 20 to 26: magic, tier, flags, reason, hardware version, and a
/// little-endian additive checksum of bytes 0 to 4.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct TierReport {
    pub tier: u8,
    pub flags: u8,
    pub reason: Reason,
    pub hw_version: u8,
}

pub fn parse_report(b: &[u8]) -> Result<TierReport, Refusal> {
    if b.len() != REPORT_LEN || b[0] != REPORT_MAGIC {
        return Err(Refusal::ReportMalformed);
    }
    let sum: u16 = b[..5].iter().map(|&x| u16::from(x)).sum();
    if u16::from_le_bytes([b[5], b[6]]) != sum {
        return Err(Refusal::ReportMalformed);
    }
    let reason = Reason::from_code(b[3]).ok_or(Refusal::ReportMalformed)?;
    Ok(TierReport { tier: b[1], flags: b[2], reason, hw_version: b[4] })
}

/// Step 6: accept only if the unit's own report reads `OK`, the build record's tier and its hardware version.
/// A blank report, a T1 reading where T2 was written, or a wrong version refuses the unit.
pub fn accept(report: &[u8], build: BuildRecord) -> Result<(), Refusal> {
    check_fields(build.tier, build.hw_version)?;
    let r = parse_report(report)?;
    if r.reason != Reason::Ok {
        return Err(Refusal::ReasonNotOk(r.reason));
    }
    if r.tier != build.tier {
        return Err(Refusal::TierMismatch);
    }
    if r.hw_version != build.hw_version {
        return Err(Refusal::HwVersionMismatch);
    }
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;
    use ed25519_dalek::{Signer, SigningKey};

    // Test authority only: the seed 0xA0, 0xA1, … 0xBF that `np_tier_test_key.h` documents. Never a production key.
    fn test_key() -> SigningKey {
        let mut seed = [0u8; 32];
        for (i, s) in seed.iter_mut().enumerate() {
            *s = 0xA0 + i as u8;
        }
        SigningKey::from_bytes(&seed)
    }
    fn pk() -> [u8; 32] {
        test_key().verifying_key().to_bytes()
    }
    const UID: [u8; 12] = [0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC];

    fn hex(s: &str) -> Vec<u8> {
        (0..s.len() / 2).map(|i| u8::from_str_radix(&s[2 * i..2 * i + 2], 16).unwrap()).collect()
    }
    fn signed(tier: u8, hw: u8, uid: &[u8; 12]) -> [u8; RECORD_LEN] {
        let body = record_body(tier, hw).unwrap();
        let sig = test_key().sign(&signing_message(&body, uid));
        assemble(&body, &sig.to_bytes()).unwrap()
    }
    fn report(tier: u8, reason: u8, hw: u8) -> [u8; 7] {
        let mut r = [REPORT_MAGIC, tier, 0, reason, hw, 0, 0];
        let s: u16 = r[..5].iter().map(|&x| u16::from(x)).sum();
        r[5..].copy_from_slice(&s.to_le_bytes());
        r
    }

    // Signatures produced by the firmware's own Monocypher (`crypto_ed25519_sign`) for the same seed, UID and
    // bodies; Ed25519 is deterministic, so the Rust side must reproduce them byte for byte.
    const C_PK: &str = "4fd099ccd47d7893dfe9ec24414ecb0d9b5420232aad30d91c465be33cbe65c4";
    const C_VECTORS: [(u8, u8, &str); 3] = [
        (TIER_T1, 1, "f65f0581f93b84b1b20bc4b45f2b313a0f232908e25a6b68fafbf8b03a2657b17e3165c81026b9cc4f4aa8fe24162c63a130184569db351ee64b2ad43314d201"),
        (TIER_T2, 1, "721e7d5f5ca6f6bcef26c63ee461c194f4436299808af1498807e8e98737cdb620539005f8f74638aca603389f655a1a2cfc3f18d9f25c8a1a6b04a190d1ce0a"),
        (TIER_T2, 15, "8597f4fe72890222631e64122bcaeeb86beee61e7a18bb2dd7c098b7f6191831e641d0731dfc1e73d64f5deede8f6840e85745544115a9fd8e8031692c8c6b07"),
    ];

    // @verifies REQ-UPG-04
    #[test]
    fn record_matches_the_firmware_signer_byte_for_byte() {
        assert_eq!(pk().to_vec(), hex(C_PK));
        for (tier, hw, sig) in C_VECTORS {
            let rec = signed(tier, hw, &UID);
            assert_eq!(&rec[..5], b"NPTI\x02");
            assert_eq!(rec[5], (hw << 4) | tier);
            assert_eq!(rec[BODY_LEN..].to_vec(), hex(sig), "tier {tier} hw {hw}");
            let v = verify_record(Some(&pk()), &rec, &UID);
            assert_eq!(v, Verdict { tier, reason: Reason::Ok, hw_version: hw });
        }
    }

    // @verifies REQ-UPG-04
    #[test]
    fn a_record_for_another_unit_or_key_or_domain_or_bit_flip_is_t1() {
        let rec = signed(TIER_T2, 1, &UID);
        let mut other = UID;
        other[11] ^= 1;
        assert_eq!(verify_record(Some(&pk()), &rec, &other).reason, Reason::Signature);
        assert_eq!(verify_record(Some(&pk()), &rec, &other).tier, TIER_T1);
        for bit in 0..RECORD_LEN * 8 {
            let mut bad = rec;
            bad[bit / 8] ^= 1 << (bit % 8);
            let v = verify_record(Some(&pk()), &bad, &UID);
            assert_ne!(v.reason, Reason::Ok, "bit {bit}");
            assert_eq!(v.tier, TIER_T1);
            assert_eq!(v.hw_version, 1);
        }
        let rogue = SigningKey::from_bytes(&[7u8; 32]).verifying_key().to_bytes();
        assert_eq!(verify_record(Some(&rogue), &rec, &UID).reason, Reason::Signature);
        // The right fields signed without the domain string.
        let body = record_body(TIER_T2, 1).unwrap();
        let mut nodomain = body.to_vec();
        nodomain.extend_from_slice(&UID);
        let w = assemble(&body, &test_key().sign(&nodomain).to_bytes()).unwrap();
        assert_eq!(verify_record(Some(&pk()), &w, &UID).reason, Reason::Signature);
    }

    // @verifies REQ-UPG-04
    #[test]
    fn blank_format_and_no_authority_report_the_first_thing_wrong() {
        let blank = [0xFFu8; RECORD_LEN];
        assert_eq!(verify_record(Some(&pk()), &blank, &UID).reason, Reason::Blank);
        assert_eq!(verify_record(None, &blank, &UID).reason, Reason::NoAuthority);
        assert_eq!(verify_record(Some(&[0u8; 32]), &blank, &UID).reason, Reason::NoAuthority);
        let mut rec = signed(TIER_T2, 1, &UID);
        rec[5] = 0x02; // hardware version 0
        assert_eq!(verify_record(Some(&pk()), &rec, &UID).reason, Reason::Format);
        let mut rec = signed(TIER_T2, 1, &UID);
        rec[5] = 0x13; // tier code 3
        assert_eq!(verify_record(Some(&pk()), &rec, &UID).reason, Reason::Format);
        let mut rec = signed(TIER_T2, 1, &UID);
        rec[6] = 1;
        assert_eq!(verify_record(Some(&pk()), &rec, &UID).reason, Reason::Format);
        let mut rec = signed(TIER_T2, 1, &UID);
        rec[4] = 1; // the old layout, never written
        assert_eq!(verify_record(Some(&pk()), &rec, &UID).reason, Reason::Format);
        assert_eq!(verify_record(Some(&pk()), &rec[..71], &UID).reason, Reason::Format);
    }

    // @verifies REQ-UPG-04
    #[test]
    fn step_two_refuses_a_window_that_is_not_blank() {
        assert_eq!(check_blank(&[0xFF; RECORD_LEN]), Ok(()));
        for i in 0..RECORD_LEN {
            let mut w = [0xFFu8; RECORD_LEN];
            w[i] = 0xFE;
            assert_eq!(check_blank(&w), Err(Refusal::WindowNotBlank), "byte {i}");
        }
        assert_eq!(check_blank(&[0u8; RECORD_LEN]), Err(Refusal::WindowNotBlank));
        assert_eq!(check_blank(&[0xFF; 71]), Err(Refusal::WindowNotBlank));
    }

    // @verifies REQ-UPG-04
    #[test]
    fn step_three_refuses_a_wrong_version_before_anything_is_signed() {
        let b = BuildRecord { tier: TIER_T2, hw_version: 1 };
        assert_eq!(check_build_record(b, &[1], 1), Ok(()));
        assert_eq!(check_build_record(b, &[2], 1), Err(Refusal::VersionNotReleased));
        assert_eq!(check_build_record(b, &[], 1), Err(Refusal::VersionNotReleased));
        assert_eq!(check_build_record(b, &[1, 2], 2), Err(Refusal::VersionBoardMismatch));
        let z = BuildRecord { tier: TIER_T2, hw_version: 0 };
        assert_eq!(check_build_record(z, &[0], 0), Err(Refusal::VersionOutOfRange));
        let hi = BuildRecord { tier: TIER_T1, hw_version: 16 };
        assert_eq!(check_build_record(hi, &[16], 16), Err(Refusal::VersionOutOfRange));
        let t = BuildRecord { tier: 3, hw_version: 1 };
        assert_eq!(check_build_record(t, &[1], 1), Err(Refusal::TierNotValid));
        assert_eq!(record_body(0, 1), Err(Refusal::TierNotValid));
        assert_eq!(record_body(TIER_T1, 16), Err(Refusal::VersionOutOfRange));
        assert_eq!(assemble(&[0; 8], &[0; 63]), Err(Refusal::SignatureLength));
    }

    // @verifies REQ-UPG-04
    #[test]
    fn step_five_is_nine_double_words_that_read_back_equal() {
        let rec = signed(TIER_T1, 1, &UID);
        let dw = double_words(&rec);
        assert_eq!(dw.len(), 9);
        assert_eq!(dw.concat(), rec.to_vec());
        assert_eq!(check_read_back(&rec, &rec), Ok(()));
        let mut rb = rec;
        rb[70] ^= 0x80;
        assert_eq!(check_read_back(&rec, &rb), Err(Refusal::ReadBackMismatch));
        assert_eq!(check_read_back(&rec, &rec[..64]), Err(Refusal::ReadBackMismatch));
    }

    // @verifies REQ-UPG-04
    #[test]
    fn step_six_accepts_only_the_units_own_ok_report_for_the_build_record() {
        let build = BuildRecord { tier: TIER_T2, hw_version: 3 };
        assert_eq!(accept(&report(TIER_T2, 0, 3), build), Ok(()));
        // A T1 reading where T2 was written (a blank or bad record reads T1).
        assert_eq!(accept(&report(TIER_T1, 0, 3), build), Err(Refusal::TierMismatch));
        assert_eq!(
            accept(&report(TIER_T1, 2, 1), build),
            Err(Refusal::ReasonNotOk(Reason::Blank))
        );
        // A T1 blank reads as correct behaviour; only the reason shows it.
        let t1 = BuildRecord { tier: TIER_T1, hw_version: 1 };
        assert_eq!(
            accept(&report(TIER_T1, 2, 1), t1),
            Err(Refusal::ReasonNotOk(Reason::Blank))
        );
        assert_eq!(accept(&report(TIER_T1, 0, 1), t1), Ok(()));
        assert_eq!(accept(&report(TIER_T2, 0, 2), build), Err(Refusal::HwVersionMismatch));
        // Malformed reports: length, magic, checksum, unknown reason.
        let good = report(TIER_T2, 0, 3);
        assert_eq!(accept(&good[..6], build), Err(Refusal::ReportMalformed));
        let mut bad = good;
        bad[0] = 0;
        assert_eq!(accept(&bad, build), Err(Refusal::ReportMalformed));
        let mut bad = good;
        bad[5] ^= 1;
        assert_eq!(accept(&bad, build), Err(Refusal::ReportMalformed));
        assert_eq!(accept(&report(TIER_T2, 9, 3), build), Err(Refusal::ReportMalformed));
        assert_eq!(accept(&[0u8; 7], build), Err(Refusal::ReportMalformed));
    }

    // @verifies REQ-UPG-04
    #[test]
    fn every_stage_in_order_on_a_host_double_of_the_unit() {
        // The whole step, with a signer that stands in for the undecided service and a unit double whose
        // report comes from `verify_record`, which is the firmware's verdict.
        for (tier, hw) in [(TIER_T1, 1u8), (TIER_T2, 1), (TIER_T2, 15)] {
            let build = BuildRecord { tier, hw_version: hw };
            let window = [0xFFu8; RECORD_LEN];
            check_blank(&window).unwrap();
            check_build_record(build, &[1, 15], hw).unwrap();
            let body = record_body(tier, hw).unwrap();
            let sig = test_key().sign(&signing_message(&body, &UID));
            let rec = assemble(&body, &sig.to_bytes()).unwrap();
            let programmed: Vec<u8> = double_words(&rec).concat();
            check_read_back(&rec, &programmed).unwrap();
            let v = verify_record(Some(&pk()), &programmed, &UID);
            accept(&report(v.tier, v.reason.code(), v.hw_version), build).unwrap();
        }
        // Skipping the programming leaves a blank unit, and acceptance refuses it.
        let v = verify_record(Some(&pk()), &[0xFF; RECORD_LEN], &UID);
        let build = BuildRecord { tier: TIER_T1, hw_version: 1 };
        assert!(accept(&report(v.tier, v.reason.code(), v.hw_version), build).is_err());
    }
}
