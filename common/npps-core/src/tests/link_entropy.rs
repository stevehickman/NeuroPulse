//! The randomness a pairing is given (`SWR-MG-28` (d), `OI-PRIV-12` stage 1). A child module of `link`, so it reaches the
//! private source; it is in this directory, not inside `link.rs`, so that the requirements trace reads its tag.

use super::*;

const CODE: &str = "12345678";

// @verifies SWR-MG-28#d
#[test]
fn the_supplied_randomness_is_handed_out_once_and_asking_for_more_is_refused() {
    let mut given = Given(Zeroizing::new(vec![1, 2, 3, 4, 5]), 0);
    let mut first = [0u8; 3];
    given.try_fill_bytes(&mut first).unwrap();
    assert_eq!(first, [1, 2, 3]);
    let mut too_many = [0xeeu8; 3];
    assert!(given.try_fill_bytes(&mut too_many).is_err(), "two bytes are left and three were asked for");
    assert_eq!(too_many, [0xee; 3], "a refusal writes nothing, least of all the first bytes again");
    let mut rest = [0u8; 2];
    given.try_fill_bytes(&mut rest).unwrap();
    assert_eq!(rest, [4, 5]);
    assert!(given.try_fill_bytes(&mut [0u8; 1]).is_err());
    // The infallible form cannot say no, so it stops rather than repeat a byte.
    let stopped = std::panic::catch_unwind(|| Given(Zeroizing::new(vec![9; 4]), 0).fill_bytes(&mut [0u8; 5]));
    assert!(stopped.is_err());
    let stopped = std::panic::catch_unwind(|| Given(Zeroizing::new(vec![]), 0).next_u32());
    assert!(stopped.is_err(), "no bytes at all is too few");
    // One SPAKE2 start takes less than the 96 bytes a driver is asked for, so a pairing never reaches the refusal.
    let seed = [7u8; PAIR_RANDOM as usize];
    let _ = spake(true, CODE, &seed);
    let _ = spake(false, CODE, &seed);
}
