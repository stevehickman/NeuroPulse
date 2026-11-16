//! The desktop central names a failure by its kind and never prints the Bluetooth library's message (`OI-PRIV-16`).

use neurone_btle_jni::central::error_kind;

// @verifies REQ-SW03-31#d
#[test]
fn a_failure_is_named_by_its_kind_and_the_librarys_text_is_never_printed() {
    // The text a D-Bus error can carry: the device's object path, which holds its address.
    let path = "/org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF";
    for e in [btleplug::Error::Other(path.into()), btleplug::Error::RuntimeError(path.into()), btleplug::Error::NotSupported(path.into())] {
        assert!(e.to_string().contains(path), "the library's message does carry the path");
        assert!(!error_kind(&e).contains("dev_"), "the kind does not");
    }
    assert_eq!(error_kind(&btleplug::Error::Other(path.into())), "other");
    assert_eq!(error_kind(&btleplug::Error::NotConnected), "not-connected");

    // No print in the central's code formats an error value: each names `error_kind`.
    let code = include_str!("../src/central.rs").split("#[cfg(test)]").next().unwrap();
    for line in code.lines().filter(|l| l.contains("println!")) {
        assert!(!line.contains("{e") && !line.contains("{:?}") && !line.contains("{err"), "prints an error value: {line}");
    }
    assert!(code.matches("eprintln!").count() > 0);
    assert_eq!(code.matches("eprintln!").count(), code.matches("error_kind(&e));").count(), "every print names the kind only");
    // The JNI surface prints nothing at all.
    assert!(!include_str!("../src/lib.rs").contains("println!"));
}
