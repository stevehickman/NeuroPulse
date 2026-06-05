import Foundation

// Region detection helper used to gate the BIPA written-release disclosure.
//
// Illinois BIPA (740 ILCS 14) requires a written release before collecting
// biometric information (EEG brainwave data). This helper performs a best-effort
// locale-based check. It is intentionally conservative in scope: locale alone is
// not authoritative, so the BIPA flow also presents an explicit Illinois
// declaration. See ISA decision 2026-06-04 and open item OI-PA-03 (legal counsel
// review of the detection mechanism and the 16-year threshold).
enum RegionHelper {

    /// Returns true if the device locale strongly suggests Illinois — used to gate BIPA disclosure.
    /// Note: locale-based detection is best-effort. OI-PA-03 (legal counsel review) is still required.
    static var isLikelyIllinois: Bool {
        Locale.current.region?.identifier == "US-IL"
    }
}
