//! The desktop central names a failure by its kind and never prints the Bluetooth library's message (`OI-PRIV-16`).
//! The same checks sit beside the code in `src/central.rs`; these are here because the requirement trace
//! (`scripts/req-trace.ts`) reads Rust tests under `tests/` only.

use neurone_btle_jni::central::{error_kind, failure_line};

// @verifies REQ-SW03-31#c
#[test]
fn a_failure_line_holds_the_error_kind_and_none_of_the_librarys_text() {
    // The text a D-Bus error can carry: the device's object path, which holds its address.
    let path = "/org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF";
    for e in [btleplug::Error::Other(path.into()), btleplug::Error::RuntimeError(path.into()), btleplug::Error::NotSupported(path.into())] {
        assert!(e.to_string().contains(path), "the library's message does carry the path");
        let line = failure_line("write", &e);
        assert_eq!(line, format!("neurone-btle: write failed: {}", error_kind(&e)));
        assert!(!line.contains("dev_") && !line.contains("/org"), "{line:?}");
    }
}

// @verifies REQ-SW03-31#c
#[test]
fn no_print_in_the_library_formats_an_error_itself() {
    let code = include_str!("../src/central.rs").split("#[cfg(test)]").next().unwrap();
    let prints: Vec<&str> = code.lines().filter(|l| l.contains("println!")).collect();
    assert!(!prints.is_empty());
    for line in prints {
        assert!(line.contains("failure_line("), "prints without failure_line: {line}");
    }
    // The JNI surface prints nothing at all.
    assert!(!include_str!("../src/lib.rs").contains("println!"));
}
