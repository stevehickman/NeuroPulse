//! Tests of the RPMB key programming step's host model (`src/rpmb_provision.rs`; `NP-REG-UPG-001` §7.10;
//! `OI-PRIV-32` (NP-PRIV-ANALYSIS-004)). Every part here is simulated; no key in this file is a production key.

use neurone_npps_core::rpmb_provision::*;


/// A test key. Not a production key, not a default: nothing outside these tests can reach it.
const K: [u8; KEY_LEN] = [
    0x3a, 0x91, 0x0c, 0x5e, 0x77, 0x02, 0xd4, 0xb8, 0x16, 0xe9, 0x4f, 0x20, 0xa3, 0x6b, 0xc1, 0x08,
    0x5d, 0xf2, 0x9e, 0x31, 0x84, 0x47, 0x0b, 0xca, 0x6e, 0x13, 0xb5, 0x2c, 0xd9, 0x70, 0x08, 0xe4,
];
const OTHER: [u8; KEY_LEN] = [0x5a; KEY_LEN];

fn nonces() -> impl FnMut() -> [u8; NONCE_LEN] {
    let mut i = 0u8;
    move || {
        i += 1;
        [i; NONCE_LEN]
    }
}

// @verifies REQ-RPMBK-02#a REQ-RPMBK-03
#[test]
fn blank_part_is_programmed_once_and_proved_by_an_authenticated_read() {
    let mut part = SimRpmb::blank();
    let got = provision(&mut part, &K, &mut nonces());
    assert_eq!(got, Ok(Accepted::Programmed { write_counter: 0 }));
    assert_eq!(part.program_requests, 1);
    assert_eq!(part.key, Some(K));
    assert_eq!(part.exchanges, 3, "read, program, proving read");
}

// @verifies REQ-RPMBK-02#b REQ-RPMBK-04#b
#[test]
fn a_rerun_on_the_same_part_never_programs_again() {
    let mut part = SimRpmb::blank();
    provision(&mut part, &K, &mut nonces()).unwrap();
    part.write_counter = 7;
    let again = provision(&mut part, &K, &mut nonces());
    assert_eq!(again, Ok(Accepted::AlreadyThisKey { write_counter: 7 }));
    assert_eq!(part.program_requests, 1, "the second run sent no key");
}

// @verifies REQ-RPMBK-02#b REQ-RPMBK-04#a
#[test]
fn a_part_holding_another_key_is_refused_and_never_sent_a_key() {
    let mut part = SimRpmb::keyed(OTHER);
    assert_eq!(provision(&mut part, &K, &mut nonces()), Err(Refusal::OtherKey));
    assert_eq!(part.program_requests, 0);
    assert_eq!(part.key, Some(OTHER));
}

// @verifies REQ-RPMBK-01#c
#[test]
fn an_all_zero_or_all_one_key_is_refused_before_anything_is_sent() {
    for k in [[0x00u8; KEY_LEN], [0xFFu8; KEY_LEN]] {
        let mut part = SimRpmb::blank();
        assert_eq!(provision(&mut part, &k, &mut nonces()), Err(Refusal::DegenerateKey));
        assert_eq!(part.exchanges, 0);
        assert_eq!(part.key, None);
    }
    let mut one_bit = [0u8; KEY_LEN];
    one_bit[31] = 1;
    assert!(!is_degenerate(&one_bit));
}

// @verifies REQ-RPMBK-03
#[test]
fn a_part_that_acknowledges_but_keeps_no_key_is_refused() {
    let mut part = SimRpmb { drop_programming: true, ..SimRpmb::blank() };
    assert_eq!(provision(&mut part, &K, &mut nonces()), Err(Refusal::ProgramNotTaken));
    assert_eq!(part.program_requests, 1);
}

// @verifies REQ-RPMBK-03
#[test]
fn a_replayed_response_is_refused_on_its_nonce() {
    // A keyed part whose answer carries an old nonce, MAC'd correctly for that old frame.
    let mut part = SimRpmb { replay_nonce: Some([0xEE; NONCE_LEN]), ..SimRpmb::keyed(K) };
    assert_eq!(provision(&mut part, &K, &mut nonces()), Err(Refusal::NonceMismatch));
    // And after programming a blank one: the proving read is the one replayed.
    let mut blank = SimRpmb { replay_nonce: Some([0xEE; NONCE_LEN]), ..SimRpmb::blank() };
    assert_eq!(provision(&mut blank, &K, &mut nonces()), Err(Refusal::NonceMismatch));
    assert_eq!(blank.program_requests, 1);
}

// @verifies REQ-RPMBK-03
#[test]
fn a_tampered_mac_reads_as_another_key() {
    struct Tamper(SimRpmb);
    impl RpmbPart for Tamper {
        fn exchange(&mut self, r: &Frame) -> Result<Frame, TransportError> {
            let mut f = self.0.exchange(r)?;
            f[OFF_WRITE_COUNTER + 3] ^= 1; // the counter changed in transit; the MAC no longer covers it
            Ok(f)
        }
    }
    let mut part = Tamper(SimRpmb::keyed(K));
    assert_eq!(provision(&mut part, &K, &mut nonces()), Err(Refusal::OtherKey));
}

// @verifies REQ-RPMBK-03
#[test]
fn wrong_response_type_expired_counter_and_bus_failure_are_refused() {
    let mut p = SimRpmb { wrong_type: Some(0x0400), ..SimRpmb::blank() };
    assert_eq!(provision(&mut p, &K, &mut nonces()), Err(Refusal::WrongResponseType { got: 0x0400 }));
    assert_eq!(p.program_requests, 0);

    let mut p = SimRpmb { counter_expired: true, ..SimRpmb::keyed(K) };
    assert_eq!(provision(&mut p, &K, &mut nonces()), Err(Refusal::CounterExpired));

    for n in 1..=3 {
        let mut p = SimRpmb { fail_exchange: Some(n), ..SimRpmb::blank() };
        assert_eq!(provision(&mut p, &K, &mut nonces()), Err(Refusal::Transport), "exchange {n}");
    }
}

// @verifies REQ-RPMBK-03
#[test]
fn a_repeated_or_zero_nonce_stops_the_run() {
    let mut part = SimRpmb::blank();
    let mut same = || [9u8; NONCE_LEN];
    assert_eq!(provision(&mut part, &K, &mut same), Err(Refusal::BadNonce));
    assert_eq!(part.key, Some(K), "programmed, then the proving read refused to reuse the nonce");

    let mut part = SimRpmb::blank();
    let mut zero = || [0u8; NONCE_LEN];
    assert_eq!(provision(&mut part, &K, &mut zero), Err(Refusal::BadNonce));
    assert_eq!(part.exchanges, 0);
}

// @verifies-none harness self-check: the MAC covers bytes 228..512 and nothing before them, as OP-TEE's frame does
#[test]
fn mac_covers_exactly_the_data_field_onward() {
    let mut f = [0u8; FRAME_LEN];
    f[OFF_TYPE + 1] = 2;
    let m = frame_mac(&K, &f);
    let mut g = f;
    g[0] = 0xAA;
    g[OFF_KEY_MAC] = 0xAA;
    assert_eq!(frame_mac(&K, &g), m, "stuff bytes and the MAC field are outside the MAC");
    g[OFF_DATA] = 1;
    assert_ne!(frame_mac(&K, &g), m);
}
