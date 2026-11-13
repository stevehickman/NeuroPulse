//! The version of the wavelength rules (`SWR-HV-03`, `OI-WRV-01`, `NP-NPPS-REF-001` §7a.1): the canonical form and
//! hash, the stamp, the register, and the major/minor floor, on the six hand-checkable vectors of
//! `docs/human-input/oi-ui-kmp-10-wavelength-rules-versioning.md` §5.3. Each vector's hash was also produced with
//! `printf 'NeurOne.WavelengthRules.1\n<json>' | sha256sum`.

use neurone_npps_core::api::rules as rules_from_json;
use neurone_npps_core::parse_file;
use neurone_npps_core::wavelength::{Channel, ChannelRule, WavelengthRules};
use neurone_npps_core::wavelength_version::*;

fn rule(el: Channel, nominal: f64, min: f64, max: f64) -> ChannelRule {
    ChannelRule { element: el, nominal_nm: nominal, min_nm: min, max_nm: max }
}

fn v1() -> WavelengthRules {
    WavelengthRules::default_rules()
}

fn with(f: impl FnOnce(&mut Vec<ChannelRule>)) -> ResolvedSet {
    let mut r = v1();
    f(&mut r.channels);
    ResolvedSet::from_rules(&r).unwrap()
}

fn set(r: &WavelengthRules) -> ResolvedSet {
    ResolvedSet::from_rules(r).unwrap()
}

const V1: &str = "316654aa4924a962a5e3501f80aeff3d0289498e513ac4a8572581c0f8828aa1";
const V2: &str = "eadab368297cb7a38b3e388b046ebdce52c47ca2a9032d471ed4e19d1ef54181";
const V3: &str = "76e7758b90e3bcecf629095f1ab09d904a73f526485dcc6d4e51ffdcc30ba409";
const V4: &str = "4811556ec25483355af53ea41d7f613f2d1e3a5c80245a769ad4978092e74e9b";
const V5: &str = "738b238a651d050a5d11e925a59f9f05d02c32c118caa0011ee781b0a15b337f";

fn v2() -> ResolvedSet { with(|c| c[1].max_nm = 860.0) }
fn v3() -> ResolvedSet { with(|c| c.retain(|r| r.element != Channel::Led1064)) }
fn v4() -> ResolvedSet { with(|c| c[0].min_nm = 632.8) }
fn v5() -> ResolvedSet { with(|c| c[1].nominal_nm = 810.0) }

// @verifies SWR-HV-03#a
#[test]
fn the_six_vectors_hash_as_written_in_the_design_record() {
    assert_eq!(set(&v1()).hash(), V1);
    assert_eq!(v2().hash(), V2);
    assert_eq!(v3().hash(), V3);
    assert_eq!(v4().hash(), V4);
    assert_eq!(v5().hash(), V5);
}

// @verifies SWR-HV-03#a
#[test]
fn the_canonical_form_is_as_specified() {
    assert_eq!(set(&v1()).canonical().len(), 285);
    assert_eq!(
        v3().canonical(),
        "{\"channels\":[{\"element\":\"led_660\",\"max_pm_x10\":6800000,\"min_pm_x10\":6500000,\"nominal_pm_x10\":6600000},{\"element\":\"led_808\",\"max_pm_x10\":8400000,\"min_pm_x10\":7980000,\"nominal_pm_x10\":8080000},{\"element\":\"led_1064\",\"max_pm_x10\":null,\"min_pm_x10\":null,\"nominal_pm_x10\":null}]}"
    );
}

// @verifies SWR-HV-03#a
#[test]
fn name_level_description_and_the_order_written_do_not_move_the_identity() {
    // V6: V1 renamed, its channels written in the order 1064, 660, 808. The hash is V1's.
    let mut r = v1();
    r.name = "Lab A (renamed)".into();
    r.channels.reverse();
    assert_eq!(set(&r).hash(), V1);
}

// @verifies SWR-HV-03#a
#[test]
fn a_value_off_the_grid_is_refused_with_the_value_named_and_never_rounded() {
    let mut r = v1();
    r.channels[0].min_nm = 632.80001;
    let e = ResolvedSet::from_rules(&r).unwrap_err();
    assert!(e.contains("632.80001") && e.contains("led_660") && e.contains("not rounded"), "{e}");
    // A value on the grid, written to 0.0001 nm, is accepted, and one finer than the old 1 pm grid is distinct.
    let a = with(|c| c[0].min_nm = 632.8001);
    let b = with(|c| c[0].min_nm = 632.8);
    assert_ne!(a.hash(), b.hash());
    assert!(nm_to_units("x", f64::NAN).is_err() && nm_to_units("x", 0.0).is_err() && nm_to_units("x", -1.0).is_err());
}

// @verifies SWR-HV-03#a
#[test]
fn the_stamp_is_39_bytes_whatever_the_rules_and_round_trips() {
    for s in [set(&v1()), v2(), v3(), v4(), v5()] {
        let st = stamp_for(&s).unwrap();
        let b = st.encode();
        assert_eq!(b.len(), STAMP_LEN);
        assert_eq!(Stamp::decode(&b).unwrap(), st);
        assert_eq!(ResolvedSet::from_windows_bytes(&b[3..]).unwrap(), s);
    }
    assert_eq!(Stamp::NOT_STATED.encode(), [0u8; STAMP_LEN]);
    // V1's windows, as the design record writes them.
    let hex: String = set(&v1()).windows_bytes().iter().map(|b| format!("{b:02x}")).collect();
    assert_eq!(hex, "40b56400a02e630080c26700804a7b00e0c37900802c8000805aa200e0d3a00020e1a300");
}

// @verifies SWR-HV-03#a
#[test]
fn a_malformed_stamp_is_refused_not_guessed() {
    let good = stamp_for(&set(&v1())).unwrap().encode();
    assert!(Stamp::decode(&good[..38]).is_err());
    let mut b = good;
    b[0] = 3;
    assert!(Stamp::decode(&b).is_err(), "unknown origin");
    let mut b = [0u8; STAMP_LEN];
    b[1] = 1;
    assert!(Stamp::decode(&b).is_err(), "a stamp that states nothing carries zeros");
    let mut w = [0u8; WINDOWS_LEN];
    w[0] = 1;
    assert!(ResolvedSet::from_windows_bytes(&w).is_err(), "a window has all three values or none");
}

// @verifies SWR-HV-03#a
#[test]
fn a_released_set_is_its_release_and_an_edited_one_is_an_owner_set_over_the_base() {
    assert_eq!(release_of(&set(&v1())).unwrap(), Some((1, 0)));
    let s = stamp_for(&set(&v1())).unwrap();
    assert_eq!((s.origin, s.major, s.minor), (Origin::Release, 1, 0));
    // An owner restores the defaults: back on the release, whatever level the block says.
    let s = stamp_for(&v2()).unwrap();
    assert_eq!((s.origin, s.major, s.minor), (Origin::Owner, 1, 0));
    assert_eq!(read_stamp(&s).unwrap(), Reading::OwnerSet { base_major: 1, base_minor: 0 });
}

// @verifies SWR-HV-03#a
#[test]
fn a_reader_trusts_the_windows_and_never_invents_a_version() {
    assert_eq!(read_stamp(&Stamp::NOT_STATED).unwrap(), Reading::NotStated);
    let rel = stamp_for(&set(&v1())).unwrap();
    assert_eq!(read_stamp(&rel).unwrap(), Reading::Release { major: 1, minor: 0 });
    // A release this build does not know (a record from a newer app).
    let mut newer = rel;
    newer.major = 3;
    newer.minor = 2;
    newer.windows = v2().windows_bytes();
    assert_eq!(read_stamp(&newer).unwrap(), Reading::UnknownRelease { major: 3, minor: 2 });
    // It claims release 1.0 and the windows are not 1.0: the windows win, and the mismatch is noted.
    let mut lie = rel;
    lie.windows = v2().windows_bytes();
    assert_eq!(read_stamp(&lie).unwrap(), Reading::Mismatch { claimed_major: 1, claimed_minor: 0 });
    // It claims an owner set whose windows are in fact release 1.0.
    let mut o = rel;
    o.origin = Origin::Owner;
    assert_eq!(read_stamp(&o).unwrap(), Reading::Release { major: 1, minor: 0 });
}

// @verifies SWR-HV-03#a
#[test]
fn major_and_minor_on_the_design_records_table() {
    let b = set(&v1());
    assert_eq!(delivery_change(&b, &v2()), Change::Minor, "V1 to V2: 840 to 860 newly accepted");
    assert_eq!(delivery_change(&b, &v3()), Change::Major, "V1 to V3: 1054 to 1074 now refused");
    assert_eq!(delivery_change(&b, &v4()), Change::Minor, "V1 to V4: 632.8 to 650 newly accepted");
    assert_eq!(delivery_change(&b, &v5()), Change::Minor, "V1 to V5: the nominal decides nothing here");
    assert_eq!(delivery_change(&b, &b), Change::None, "V1 to V6: the same set");
    assert_eq!(delivery_change(&v2(), &b), Change::Major, "V2 to V1: 840 to 860 now refused");
}

// @verifies SWR-HV-03#a
#[test]
fn a_wavelength_that_moves_channel_is_major_even_when_nothing_is_refused() {
    // The 660 channel is widened to 650 to 840 nm, so its window overlaps the 808 channel's (798 to 840 nm).
    // Where they overlap the nearer nominal wins, so the nominal of the 660 channel decides who delivers 830 nm.
    let wide = |nom: f64| {
        let mut r = v1();
        r.channels = vec![rule(Channel::Led660, nom, 650.0, 840.0), rule(Channel::Led808, 808.0, 798.0, 840.0), rule(Channel::Led1064, 1064.0, 1054.0, 1074.0)];
        set(&r)
    };
    // Nominal 660: the tie point is 734 nm, below the overlap, so the 808 channel delivers all of it.
    // Nominal 820: the tie point is 814 nm, inside it, so 830 nm moves to the 660 channel. Nothing is refused.
    assert_eq!(delivery_change(&wide(660.0), &wide(820.0)), Change::Major);
    // Nominal 760: the tie point is 784 nm, still below the overlap, so nothing moves.
    assert_eq!(delivery_change(&wide(660.0), &wide(760.0)), Change::Minor);
}

// @verifies SWR-HV-03#a
#[test]
fn a_tie_goes_to_the_earlier_channel_and_a_sliver_that_moves_is_still_major() {
    // Nominals 700 and 900 tie at 800 nm, which goes to the earlier channel (the 660 channel).
    let tie = |b: f64| {
        let mut r = v1();
        r.channels = vec![rule(Channel::Led660, 700.0, 650.0, 900.0), rule(Channel::Led808, b, 700.0, 900.0)];
        set(&r)
    };
    assert_eq!(delivery_change(&tie(900.0), &tie(900.0)), Change::None);
    // Raising the second nominal by 0.0001 nm moves the tie point to 800.00005 nm, so the wavelengths between 800
    // and that point change from the 808 channel to the 660 channel. The floor is exact: that is major.
    assert_eq!(delivery_change(&tie(900.0), &tie(900.0001)), Change::Major);
}

// @verifies SWR-HV-03#a
#[test]
fn the_register_is_well_formed_and_its_last_row_is_the_shipped_file() {
    assert_eq!(register_problems(), Vec::<String>::new());
    let rows = releases().unwrap();
    assert_eq!((rows[0].major, rows[0].minor, rows[0].hash.as_str()), (1, 0, V1), "the first release is the defaults of 2026-11-13");
    let src = std::fs::read_to_string(concat!(env!("CARGO_MANIFEST_DIR"), "/../../protocols/predefined/00-wavelength-rules.npps")).unwrap();
    let parsed = parse_file(&src).unwrap();
    assert_eq!(parsed.wavelength_rules.len(), 1);
    let shipped = rules_from_json(&parsed.wavelength_rules[0]).unwrap().unwrap();
    assert_eq!(set(&shipped).hash(), rows.last().unwrap().hash, "the shipped file is not the last row of the register: add a release row, or revert the edit");
    // The Rust copy the compiler falls back to is the shipped file (the drift no test covered before).
    assert_eq!(set(&WavelengthRules::default_rules()).hash(), set(&shipped).hash(), "default_rules() has drifted from the shipped file");
}
