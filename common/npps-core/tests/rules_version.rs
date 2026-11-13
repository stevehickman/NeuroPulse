//! `SWR-HV-03`: the version of the wavelength rules (NP-NPPS-REF-001 §7a.1). Vectors V1 to V6 are
//! those of docs/human-input/oi-ui-kmp-10-wavelength-rules-versioning.md §5.3.

use neurone_npps_core::wavelength::*;

fn rules(ch: &[(Channel, f64, f64, f64)]) -> WavelengthRules {
    WavelengthRules {
        name: "t".into(),
        channels: ch
            .iter()
            .map(|&(element, nominal_nm, min_nm, max_nm)| ChannelRule { element, nominal_nm, min_nm, max_nm })
            .collect(),
    }
}

fn v1() -> Vec<(Channel, f64, f64, f64)> {
    vec![
        (Channel::Led660, 660.0, 650.0, 680.0),
        (Channel::Led808, 808.0, 798.0, 840.0),
        (Channel::Led1064, 1064.0, 1054.0, 1074.0),
    ]
}

fn w(c: Vec<(Channel, f64, f64, f64)>) -> [Window; 3] {
    windows_of(&rules(&c)).unwrap()
}

// @verifies SWR-HV-03
#[test]
fn shipped_defaults_hash_to_vector_v1() {
    assert_eq!(WavelengthRules::default_rules().channels.len(), 3);
    assert_eq!(
        rules_hash(&WavelengthRules::default_rules()).unwrap(),
        "316654aa4924a962a5e3501f80aeff3d0289498e513ac4a8572581c0f8828aa1"
    );
    let hex: String = windows_bytes(&w(v1())).iter().map(|b| format!("{b:02x}")).collect();
    assert_eq!(
        hex,
        "40b56400a02e630080c26700804a7b00e0c37900802c8000805aa200e0d3a00020e1a300"
    );
}

// @verifies SWR-HV-03
#[test]
fn vectors_v2_to_v5() {
    let mut v2 = v1();
    v2[1].3 = 860.0;
    assert_eq!(rules_hash_hex(&w(v2)), "eadab368297cb7a38b3e388b046ebdce52c47ca2a9032d471ed4e19d1ef54181");
    let v3 = vec![v1()[0], v1()[1]];
    assert_eq!(rules_hash_hex(&w(v3)), "76e7758b90e3bcecf629095f1ab09d904a73f526485dcc6d4e51ffdcc30ba409");
    let mut v4 = v1();
    v4[0].2 = 632.8;
    assert_eq!(rules_hash_hex(&w(v4)), "4811556ec25483355af53ea41d7f613f2d1e3a5c80245a769ad4978092e74e9b");
    let mut v5 = v1();
    v5[1].1 = 810.0;
    assert_eq!(rules_hash_hex(&w(v5)), "738b238a651d050a5d11e925a59f9f05d02c32c118caa0011ee781b0a15b337f");
}

// @verifies SWR-HV-03
#[test]
fn name_and_channel_order_do_not_change_identity_v6() {
    let mut r = rules(&v1());
    r.name = "renamed".into();
    r.channels.reverse();
    assert_eq!(
        rules_hash(&r).unwrap(),
        "316654aa4924a962a5e3501f80aeff3d0289498e513ac4a8572581c0f8828aa1"
    );
}

// @verifies SWR-HV-03
#[test]
fn a_value_off_the_grid_is_refused_not_rounded() {
    let mut c = v1();
    c[0].2 = 650.00004;
    let e = rules_hash(&rules(&c)).unwrap_err();
    assert!(e.contains("0.1 pm grid"), "{e}");
    assert!(RulesStamp::for_rules(ORIGIN_RELEASE, 1, 0, &rules(&c)).is_err());
    // 632.8 nm is on the grid.
    c[0].2 = 632.8;
    assert!(rules_hash(&rules(&c)).is_ok());
}

// @verifies SWR-HV-03
#[test]
fn stamp_round_trips_and_a_reader_hashes_the_carried_windows() {
    let s = RulesStamp::for_rules(ORIGIN_RELEASE, 1, 0, &WavelengthRules::default_rules()).unwrap();
    let b = s.to_bytes();
    assert_eq!(b.len(), 39);
    assert_eq!(&b[..3], &[1, 1, 0]);
    let back = RulesStamp::from_bytes(&b);
    assert_eq!(back, s);
    assert_eq!(
        back.hash_hex().unwrap(),
        "316654aa4924a962a5e3501f80aeff3d0289498e513ac4a8572581c0f8828aa1"
    );
    // A channel with no rule is twelve zero bytes and reads back as no rule (V3).
    let s3 = RulesStamp::for_rules(ORIGIN_OWNER_SET, 1, 0, &rules(&v1()[..2])).unwrap();
    assert_eq!(&s3.to_bytes()[3 + 24..], &[0u8; 12]);
    assert_eq!(
        s3.hash_hex().unwrap(),
        "76e7758b90e3bcecf629095f1ab09d904a73f526485dcc6d4e51ffdcc30ba409"
    );
}

// @verifies SWR-HV-03
#[test]
fn no_stamp_is_recorded_as_not_stated_never_an_invented_version() {
    let s = RulesStamp::not_stated();
    assert_eq!(s.to_bytes(), [0u8; 39]);
    assert!(s.hash_hex().is_none());
    assert!(RulesStamp::for_rules(ORIGIN_NOT_STATED, 0, 0, &WavelengthRules::default_rules()).is_err());
}

// @verifies SWR-HV-03
#[test]
fn major_and_minor_follow_the_table_of_section_5_4() {
    let base = w(v1());
    let mut v2 = v1();
    v2[1].3 = 860.0;
    let mut v4 = v1();
    v4[0].2 = 632.8;
    let mut v5 = v1();
    v5[1].1 = 810.0;
    let v3 = vec![v1()[0], v1()[1]];
    assert_eq!(classify_change(&base, &w(v2.clone())), RulesChange::Minor);
    assert_eq!(classify_change(&base, &w(v3)), RulesChange::Major);
    assert_eq!(classify_change(&base, &w(v4)), RulesChange::Minor);
    assert_eq!(classify_change(&base, &w(v5)), RulesChange::Minor);
    assert_eq!(classify_change(&base, &w(v1())), RulesChange::None);
    assert_eq!(classify_change(&w(v2), &base), RulesChange::Major);
    // A wavelength that moves channel is major: the 808 window now reaches below 660's nominal.
    let mut mv = v1();
    mv[1].1 = 650.0;
    mv[1].2 = 640.0;
    mv[1].3 = 700.0;
    assert_eq!(classify_change(&base, &w(mv)), RulesChange::Major);
}
